#include <wasi/api.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

static_assert(SEEK_SET == WHENCE_SET);
static_assert(SEEK_CUR == WHENCE_CUR);
static_assert(SEEK_END == WHENCE_END);
static_assert(sizeof(off_t) == sizeof(filesize_t));
static_assert(sizeof(off_t) == sizeof(filedelta_t));

ssize_t read(int fd, void *buf, size_t nbyte) 
{
  iovec_t iov; 
  errno_t err; 
  size_t bytes_read;
  iov.buf = buf; 
  iov.buf_len = nbyte;
  err = fd_read(fd, &iov, 1, &bytes_read);
  if (err != 0) {
    errno = (err == ERRNO_NOTCAPABLE) ? EBADF : (int)err;
    return -1;
  }
  return (ssize_t)bytes_read;
}

ssize_t write(int fd, const void *buf, size_t nbyte) 
{
  ciovec_t iov; errno_t err; size_t bytes_written;
  iov.buf = buf; iov.buf_len = nbyte;
  err = fd_write(fd, &iov, 1, &bytes_written);
  if (err != 0) {
    errno = (err == ERRNO_NOTCAPABLE) ? EBADF : (int)err;
    return -1;
  }
  return (ssize_t)bytes_written;
}

ssize_t pread(int fd, void *buf, size_t nbyte, off_t offset) 
{
  iovec_t iov; 
  size_t bytes_read; 
  errno_t err;
  if (offset < 0) {
    errno = EINVAL;
    return -1;
  }
  iov.buf = buf;
  iov.buf_len = nbyte;
  err = fd_pread(fd, &iov, 1, offset, &bytes_read);
  if (err != 0) {
    fdstat_t fds;
    if (err == ERRNO_NOTCAPABLE && fd_fdstat_get(fd, &fds) == 0) {
      if ((fds.fs_rights_base & RIGHTS_FD_READ) == 0) err = EBADF;
      else err = ESPIPE;
    }
    errno = (int)err;
    return -1;
  }
  return (ssize_t)bytes_read;
}

ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset) 
{
  ciovec_t iov;
  size_t bytes_written;
  errno_t err;
  if (offset < 0) {
    errno = EINVAL;
    return -1;
  }
  iov.buf = buf; iov.buf_len = nbyte;
  err = fd_pwrite(fd, &iov, 1, offset, &bytes_written);
  if (err != 0) {
    fdstat_t fds;
    if (err == ERRNO_NOTCAPABLE && fd_fdstat_get(fd, &fds) == 0) {
      if ((fds.fs_rights_base & RIGHTS_FD_WRITE) == 0) err = EBADF;
      else err = ESPIPE;
    }
    errno = (int)err;
    return -1;
  }
  return (ssize_t)bytes_written;
}

off_t lseek(int fd, off_t offset, int whence) 
{
  filesize_t new_offset;
  errno_t err = fd_seek(fd, (filedelta_t)offset, (whence_t)whence, &new_offset);
  if (err != 0) {
    errno = (err == ERRNO_NOTCAPABLE) ? ESPIPE : (int)err;
    return -1;
  }
  return (off_t)new_offset;
}

int ftruncate(int fd, off_t length) 
{
  filesize_t sz = (filesize_t)length;
  if (length < 0) {
    errno = EINVAL;
    return -1;
  }
  errno_t err = fd_filestat_set_size(fd, sz);
  if (err != 0) {
    errno = (int)err;
    return -1;
  }
  return 0;
}

int close(int fd) 
{
  errno_t err = fd_close(fd);
  if (err != 0) {
    errno = (int)err;
    return -1;
  }
  return 0;
}

int isatty(int fd)
{
  fdstat_t statbuf;
  int r = fd_fdstat_get(fd, &statbuf);
  if (r != 0) {
    errno = r;
    return 0;
  }
  if (statbuf.fs_filetype != FILETYPE_CHARACTER_DEVICE ||
     (statbuf.fs_rights_base & (RIGHTS_FD_SEEK | RIGHTS_FD_TELL)) != 0) {
    errno = (int)ERRNO_NOTTY;
    return 0;
  }
  return 1;
}

void *sbrk(intptr_t inc) /* inc should be a multiple of 64K */
{
  assert(inc >= 0 && (inc & 0xFFFF) == 0);
  if (!inc) return (void *)asm(memory.size, i32.const 16, i32.shl);
  size_t new = (size_t)inc >> 16;
  size_t old = (size_t)asm(i32.const 0, local.get new, memory.grow);
  if (old == SIZE_MAX) { errno = ENOMEM; return (void *)-1; }
  return (void *)(old << 16);
}

int faccessat(int fd, const char *path, int amode, int atflag) 
{
  if ((amode & ~(F_OK | R_OK | W_OK | X_OK)) != 0 ||
      (atflag & ~AT_EACCESS) != 0) {
    errno = EINVAL;
    return -1;
  }
  lookupflags_t lookup_flags = LOOKUPFLAGS_SYMLINK_FOLLOW;
  filestat_t file;
  errno_t error = path_filestat_get(fd, lookup_flags, path, strlen(path), &file);
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  if (amode != 0) {
    fdstat_t directory;
    error = fd_fdstat_get(fd, &directory);
    if (error != 0) {
      errno = (int)error;
      return -1;
    }
    rights_t min = 0;
    if ((amode & R_OK) != 0)
      min |= file.filetype == FILETYPE_DIRECTORY ? RIGHTS_FD_READDIR : RIGHTS_FD_READ;
    if ((amode & W_OK) != 0)
      min |= RIGHTS_FD_WRITE;
    if ((min & directory.fs_rights_inheriting) != min) {
      errno = EACCES;
      return -1;
    }
  }
  return 0;
}

int access(const char *path, int amode)
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return faccessat(dirfd, relpath, amode, 0);
}

int unlinkat(int fd, const char *path, int atflag) 
{
  errno_t error;
  if ((atflag & AT_REMOVEDIR) != 0)
    error = path_remove_directory(fd, path, strlen(path));
  else 
    error = path_unlink_file(fd, path, strlen(path));
  if (error != 0) {
      errno = (int)error;
      return -1;
  }
  return 0;
}

int unlink(const char *path) 
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return unlinkat(dirfd, relpath, 0);
}

int linkat(int oldfd, const char *oldpath, int newfd, const char *newpath, int atflag) 
{
  lookupflags_t lookup_flags = 0;
  if ((atflag & AT_SYMLINK_FOLLOW) != 0)
    lookup_flags |= LOOKUPFLAGS_SYMLINK_FOLLOW;
  errno_t error = path_link(oldfd, lookup_flags, 
    oldpath, strlen(oldpath), newfd, newpath, strlen(newpath));
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  return 0;
}

int link(const char *oldpath, const char *newpath) 
{
  char *oldrelpath;
  int olddirfd = find_relpath(oldpath, &oldrelpath);
  if (olddirfd != -1) {
    char *newrelpath;
    int newdirfd = find_relpath(newpath, &newrelpath);
    if (newdirfd != -1)
      return linkat(olddirfd, oldrelpath, newdirfd, newrelpath, 0);
  }
  errno = ENOTCAPABLE;
  return -1;
}

int symlinkat(const char *tgtpath, int fd, const char *lnkpath) 
{
  errno_t error = path_symlink(tgtpath, strlen(tgtpath), fd, lnkpath, strlen(lnkpath));
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  return 0;
}

int symlink(const char *tgtpath, const char *lnkpath) 
{
  char *relpath;
  int dirfd = find_relpath(lnkpath, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return symlinkat(tgtpath, dirfd, relpath);
}

ssize_t readlinkat(int fd, const char *path, char *buf, size_t bufsize) 
{
  size_t bufused;
  errno_t error = path_readlink(fd, path, strlen(path), (uint8_t*)buf, bufsize, &bufused);
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  return (ssize_t)bufused;
}

ssize_t readlink(const char *path, char *buf, size_t bufsize)
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return readlinkat(dirfd, relpath, buf, bufsize);
}

int rmdirat(int fd, const char *path) 
{
  errno_t error = path_remove_directory(fd, path, strlen(path));
  if (error != 0) {
   errno = (int)error;
   return -1;
  }
  return 0;
}

int rmdir(const char *path) 
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return rmdirat(dirfd, relpath);
}

int fdatasync(int fd)
{
  errno_t error = fd_datasync(fd);
  if (error != 0) {
    errno = (error == ERRNO_NOTCAPABLE) ? EBADF : (int)error;
    return -1;
  }
  return 0;
}

int fsync(int fd) 
{
  errno_t error = fd_sync(fd);
  if (error != 0) {
    errno = (error == ERRNO_NOTCAPABLE) ? EINVAL : (int)error;
    return -1;
  }
  return 0;
}

void _exit(int status) 
{
  proc_exit(status);
}
