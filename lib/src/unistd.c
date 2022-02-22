#include <wasi/api.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

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

static_assert(SEEK_SET == WHENCE_SET);
static_assert(SEEK_CUR == WHENCE_CUR);
static_assert(SEEK_END == WHENCE_END);
static_assert(sizeof(off_t) == sizeof(filesize_t));
static_assert(sizeof(off_t) == sizeof(filedelta_t));

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

