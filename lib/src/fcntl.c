#include <wasi/api.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

static_assert(O_APPEND == FDFLAGS_APPEND);
static_assert(O_DSYNC == FDFLAGS_DSYNC);
static_assert(O_NONBLOCK == FDFLAGS_NONBLOCK);
static_assert(O_RSYNC == FDFLAGS_RSYNC);
static_assert(O_SYNC == FDFLAGS_SYNC);
static_assert(O_CREAT == OFLAGS_CREAT << 12);
static_assert(O_DIRECTORY == OFLAGS_DIRECTORY << 12);
static_assert(O_EXCL == OFLAGS_EXCL << 12);
static_assert(O_TRUNC == OFLAGS_TRUNC << 12);

static_assert(POSIX_FADV_DONTNEED == ADVICE_DONTNEED);
static_assert(POSIX_FADV_NOREUSE == ADVICE_NOREUSE);
static_assert(POSIX_FADV_NORMAL == ADVICE_NORMAL);
static_assert(POSIX_FADV_RANDOM == ADVICE_RANDOM);
static_assert(POSIX_FADV_SEQUENTIAL == ADVICE_SEQUENTIAL);
static_assert(POSIX_FADV_WILLNEED == ADVICE_WILLNEED);


typedef struct preopen {
  const char *prefix;
  fd_t fd;
} preopen_t;

static preopen_t *preopens;
static size_t num_preopens;
static size_t preopen_capacity;
static int preopens_populated;
static int preopen_sep = '/';

static int resize_preopens(void) 
{
  size_t start_capacity = 4;
  size_t old_capacity = preopen_capacity;
  size_t new_capacity = old_capacity == 0 ? start_capacity : old_capacity * 2;

  preopen_t *old_preopens = preopens;
  preopen_t *new_preopens = calloc(sizeof(preopen_t), new_capacity);
  if (new_preopens == NULL) return -1;

  memcpy(new_preopens, old_preopens, num_preopens * sizeof(preopen_t));
  preopens = new_preopens;
  preopen_capacity = new_capacity;
  free(old_preopens);

  return 0;
}

static const char *strip_prefixes(const char *path) 
{
  while (1) {
    if (path[0] == preopen_sep) {
      path++;
    } else if (path[0] == '.' && path[1] == preopen_sep) {
      path += 2;
    } else if (path[0] == '.' && path[1] == 0) {
      path++;
    } else {
      break;
    }
  }
  return path;
}

static int register_preopened_fd(fd_t fd, const char *relprefix) 
{
  if (num_preopens == preopen_capacity && resize_preopens() != 0) return -1;
  char *prefix = strdup(strip_prefixes(relprefix));
  if (prefix == NULL) return -1;
  preopen_t *pp = &preopens[num_preopens++];
  pp->prefix = prefix;
  pp->fd = fd;
  return 0;
}

static int populate_preopens(void)
{
  fd_t fd;
  if (preopens_populated) return 0;
  preopens_populated = true;
  for (fd = 3; fd != 0; ++fd) {
    prestat_t prestat;
    errno_t ret = fd_prestat_get(fd, &prestat);
    if (ret == ERRNO_BADF) break;
    if (ret != ERRNO_SUCCESS) return 1;
    if (prestat.tag == PREOPENTYPE_DIR) {
      char *prefix = malloc(prestat.u.dir.pr_name_len + 1);
      if (!prefix) return 2;
      ret = fd_prestat_dir_name(fd, (uint8_t *)prefix, prestat.u.dir.pr_name_len);
      if (ret != ERRNO_SUCCESS) return 3;
      prefix[prestat.u.dir.pr_name_len] = '\0';
      if (prestat.u.dir.pr_name_len > 0 && prefix[prestat.u.dir.pr_name_len-1] == '\\') {
        preopen_sep = '\\'; /* windows path conventions */
      }
      register_preopened_fd(fd, prefix);
      free(prefix);                                       
    }
  }
  return 0;
}

static bool prefix_matches(const char *prefix, size_t prefix_len, const char *path) 
{
  if (path[0] != preopen_sep && prefix_len == 0) return true;
  if (memcmp(path, prefix, prefix_len) != 0) return false;
  size_t i = prefix_len;
  while (i > 0 && prefix[i-1] == preopen_sep) --i;
  char last = path[i];
  return last == preopen_sep || last == '\0';
}

static int find_abspath(const char *path, const char **abs_prefix, const char **relative_path) 
{
  while (*path == preopen_sep) ++path;
  size_t match_len = 0, i;
  int fd = -1;
  for (i = num_preopens; i > 0; --i) {
    const preopen_t *pre = &preopens[i-1];
    const char *prefix = pre->prefix;
    size_t len = strlen(prefix);
    if ((fd == -1 || len > match_len) && prefix_matches(prefix, len, path)) {
      fd = pre->fd;
      match_len = len;
      *abs_prefix = prefix;
    }
  }
  if (fd == -1) {
    errno = ENOENT;
    return -1;
  }
  const char *computed = path + match_len;
  while (*computed == preopen_sep) ++computed;
  if (*computed == '\0') computed = ".";
  *relative_path = computed;
  return fd;
}

int find_relpath(const char *path, char **relative_path) 
{
  const char *abs_prefix; 
  if (!preopens_populated) populate_preopens(); // todo: check ret value!
  return find_abspath(path, &abs_prefix, (const char**)relative_path);
}

int openat(int fd, const char *path, int oflags) 
{
  rights_t max =
      ~(RIGHTS_FD_DATASYNC | RIGHTS_FD_READ |
        RIGHTS_FD_WRITE | RIGHTS_FD_ALLOCATE |
        RIGHTS_FD_READDIR | RIGHTS_FD_FILESTAT_SET_SIZE);
  switch (oflags & O_ACCMODE) {
    case O_RDONLY:
    case O_RDWR:
    case O_WRONLY:
      if ((oflags & O_RDONLY) != 0) {
        max |= RIGHTS_FD_READ | RIGHTS_FD_READDIR;
      }
      if ((oflags & O_WRONLY) != 0) {
        max |= RIGHTS_FD_DATASYNC | RIGHTS_FD_WRITE |
               RIGHTS_FD_ALLOCATE |
               RIGHTS_FD_FILESTAT_SET_SIZE;
      }
      break;
    case O_EXEC:
      break;
    case O_SEARCH:
      break;
    default:
      errno = EINVAL;
      return -1;
  }

  fdstat_t fsb_cur;
  errno_t error = fd_fdstat_get(fd, &fsb_cur);
  if (error != 0) {
    errno = (int)error;
    return -1;
  }

  lookupflags_t lookup_flags = 0;
  if ((oflags & O_NOFOLLOW) == 0)
    lookup_flags |= LOOKUPFLAGS_SYMLINK_FOLLOW;

  fdflags_t fs_flags = oflags & 0xfff;
  rights_t fs_rights_base = max & fsb_cur.fs_rights_inheriting;
  rights_t fs_rights_inheriting = fsb_cur.fs_rights_inheriting;
  fd_t newfd;
  error = path_open(fd, lookup_flags, path, strlen(path),
                    (oflags_t)((oflags >> 12) & 0xfff),
                    fs_rights_base, fs_rights_inheriting, fs_flags,
                    &newfd);
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  return newfd;
}

int open(const char *path, int oflags) 
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return openat(dirfd, relpath, oflags);
}

int fcntl(int fd, int cmd, ...) 
{
  switch (cmd) {
    case F_GETFD:
      return FD_CLOEXEC;
    case F_SETFD:
      return 0;
    case F_GETFL: {
      fdstat_t fds;
      errno_t error = fd_fdstat_get(fd, &fds);
      if (error != 0) {
        errno = error;
        return -1;
      }
      int oflags = fds.fs_flags;
      if ((fds.fs_rights_base & (RIGHTS_FD_READ | RIGHTS_FD_READDIR)) != 0) {
        if ((fds.fs_rights_base & RIGHTS_FD_WRITE) != 0) {
          oflags |= O_RDWR;
        } else {
          oflags |= O_RDONLY;
        }
      } else if ((fds.fs_rights_base & RIGHTS_FD_WRITE) != 0) {
        oflags |= O_WRONLY;
      } else {
        oflags |= O_SEARCH;
      }
      return oflags;
    }
    case F_SETFL: {
      va_list ap;
      va_start(ap, cmd);
      int flags = va_arg(ap, int);
      va_end(ap);
      fdflags_t fs_flags = flags & 0xfff;
      errno_t error = fd_fdstat_set_flags(fd, fs_flags);
      if (error != 0) {
        errno = error;
        return -1;
      }
      return 0;
    }
    default: {
      errno = EINVAL;
      return -1;
    }
  }
}


int posix_fadvise(int fd, off_t offset, off_t len, int advice) 
{
  if (offset < 0 || len < 0)
    return EINVAL;
  return fd_advise(fd, offset, len, (advice_t)advice);
}

int posix_fallocate(int fd, off_t offset, off_t len) 
{
  if (offset < 0 || len < 0)
    return EINVAL;
  return fd_allocate(fd, offset, len);
}

