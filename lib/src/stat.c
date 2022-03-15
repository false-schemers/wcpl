#include <wasi/api.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

static_assert(S_ISBLK(S_IFBLK));
static_assert(S_ISCHR(S_IFCHR));
static_assert(S_ISDIR(S_IFDIR));
static_assert(S_ISFIFO(S_IFIFO));
static_assert(S_ISLNK(S_IFLNK));
static_assert(S_ISREG(S_IFREG));
static_assert(S_ISSOCK(S_IFSOCK));

static void timestamp_to_timespec(timestamp_t timestamp, time_t *pt, long *pns) 
{
  /* timestamp is in nanoseconds */
  *pt = (time_t)(timestamp / 1000000000);
  *pns = (long)(timestamp % 1000000000);
}

static void to_public_stat(const filestat_t *in, struct stat *out) 
{
  out->st_dev = in->dev;
  out->st_ino = in->ino;
  out->st_nlink = in->nlink;
  static_assert(sizeof(off_t) == sizeof(filesize_t));
  out->st_size = (off_t)in->size;
  timestamp_to_timespec(in->atim, &out->st_atime, &out->st_atime_ns);
  timestamp_to_timespec(in->mtim, &out->st_mtime, &out->st_mtime_ns);
  timestamp_to_timespec(in->ctim, &out->st_ctime, &out->st_ctime_ns);
  switch (in->filetype) {
    case FILETYPE_BLOCK_DEVICE:
      out->st_mode |= S_IFBLK;
      break;
    case FILETYPE_CHARACTER_DEVICE:
      out->st_mode |= S_IFCHR;
      break;
    case FILETYPE_DIRECTORY:
      out->st_mode |= S_IFDIR;
      break;
    case FILETYPE_REGULAR_FILE:
      out->st_mode |= S_IFREG;
      break;
    case FILETYPE_SOCKET_DGRAM:
    case FILETYPE_SOCKET_STREAM:
      out->st_mode |= S_IFSOCK;
      break;
    case FILETYPE_SYMBOLIC_LINK:
      out->st_mode |= S_IFLNK;
      break;
  }
}

int fstat(int fd, struct stat *ps) 
{
  filestat_t fs;
  errno_t error = fd_filestat_get(fd, &fs);
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  to_public_stat(&fs, ps);
  return 0;
}

int fstatat(int fd, const char *path, struct stat *ps, int atflag) 
{
  lookupflags_t lflags = 0;
  if ((atflag & AT_SYMLINK_NOFOLLOW) == 0)
    lflags |= LOOKUPFLAGS_SYMLINK_FOLLOW;
  filestat_t fs;
  errno_t error = path_filestat_get(fd, lflags, path, strlen(path), &fs);
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  to_public_stat(&fs, ps);
  return 0;
}

int stat(const char *path, struct stat *ps) 
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return fstatat(dirfd, relpath, ps, 0);
}

int lstat(const char *path, struct stat *ps) 
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return fstatat(dirfd, relpath, ps, AT_SYMLINK_NOFOLLOW);
}

int mkdirat(int fd, const char *path, mode_t mode) 
{
  errno_t error = path_create_directory(fd, path, strlen(path));
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  return 0;
}

int mkdir(const char *path, mode_t mode) 
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return mkdirat(dirfd, relpath, mode);
}
