#include <wasi/api.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

static_assert(DT_BLK == FILETYPE_BLOCK_DEVICE);
static_assert(DT_CHR == FILETYPE_CHARACTER_DEVICE);
static_assert(DT_DIR == FILETYPE_DIRECTORY);
static_assert(DT_FIFO == FILETYPE_SOCKET_STREAM);
static_assert(DT_LNK == FILETYPE_SYMBOLIC_LINK);
static_assert(DT_REG == FILETYPE_REGULAR_FILE);
static_assert(DT_UNKNOWN == FILETYPE_UNKNOWN);

#define DIRENT_DEFAULT_BUFFER_SIZE 4096

struct _DIR {
  int fd;
  dircookie_t cookie;
  char *buffer;
  size_t buffer_processed;
  size_t buffer_size;
  size_t buffer_used;
  struct dirent *dirent;
  size_t dirent_size;
};

DIR *fdopendir(int fd) 
{
  DIR *dirp = malloc(sizeof(DIR));
  if (dirp == NULL) return NULL;
  dirp->buffer = malloc(DIRENT_DEFAULT_BUFFER_SIZE);
  if (dirp->buffer == NULL) {
    free(dirp);
    return NULL;
  }
  errno_t error = fd_readdir(fd, 
    (uint8_t*)dirp->buffer, DIRENT_DEFAULT_BUFFER_SIZE,
    DIRCOOKIE_START, &dirp->buffer_used);
  if (error != 0) {
    free(dirp->buffer);
    free(dirp);
    errno = (int)error;
    return NULL;
  }
  dirp->fd = fd;
  dirp->cookie = DIRCOOKIE_START;
  dirp->buffer_processed = 0;
  dirp->buffer_size = DIRENT_DEFAULT_BUFFER_SIZE;
  dirp->dirent = NULL;
  dirp->dirent_size = 1;
  return dirp;
}

int fdclosedir(DIR *dirp) 
{
  int fd = dirp->fd;
  free(dirp->buffer);
  free(dirp->dirent);
  free(dirp);
  return fd;
}

DIR *opendirat(int dirfd, const char *dirname) 
{
  int fd = openat(dirfd, dirname, O_RDONLY|O_NONBLOCK|O_DIRECTORY);
  if (fd == -1) return NULL;
  DIR *result = fdopendir(fd);
  if (result == NULL) close(fd);
  return result;
}

DIR *opendir(const char *dirname) 
{
  char *relpath;
  int dirfd = find_relpath(dirname, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return NULL;
  }
  return opendirat(dirfd, relpath);
}

int dirfd(DIR *dirp) 
{
  return dirp->fd;
}

int closedir(DIR *dirp) 
{
  return close(fdclosedir(dirp));
}

#define GROW(buffer, buffer_size, target_size)      \
  do {                                              \
    if ((buffer_size) < (target_size)) {            \
      size_t new_size = (buffer_size);              \
      while (new_size < (target_size))              \
        new_size *= 2;                              \
      void *new_buffer = realloc(buffer, new_size); \
      if (new_buffer == NULL)                       \
        return NULL;                                \
      (buffer) = new_buffer;                        \
      (buffer_size) = new_size;                     \
    }                                               \
  } while (0)

struct dirent *readdir(DIR *dirp) 
{
  for (;;) {
    { size_t buffer_left = dirp->buffer_used - dirp->buffer_processed;
      if (buffer_left < sizeof(dirent_t)) {
        if (dirp->buffer_used < dirp->buffer_size) return NULL;
        goto read_entries;
      }
      dirent_t entry;
      memcpy(&entry, dirp->buffer + dirp->buffer_processed, sizeof(dirent_t));

      size_t entry_size = sizeof(dirent_t) + entry.d_namlen;
      if (entry.d_namlen == 0) {
        dirp->buffer_processed += entry_size;
        continue;
      }

      if (buffer_left < entry_size) {
        GROW(dirp->buffer, dirp->buffer_size, entry_size);
        goto read_entries;
      }

      const char *name = dirp->buffer + dirp->buffer_processed + sizeof(dirent_t);
      if (memchr(name, '\0', entry.d_namlen) != NULL) {
        dirp->buffer_processed += entry_size;
        continue;
      }

      GROW(dirp->dirent, dirp->dirent_size, offsetof(struct dirent, d_name) + entry.d_namlen + 1);
      struct dirent *dirent = dirp->dirent;
      dirent->d_ino = entry.d_ino;
      dirent->d_type = entry.d_type;
      memcpy(&dirent->d_name[0], name, entry.d_namlen);
      dirent->d_name[entry.d_namlen] = '\0';
      dirp->cookie = entry.d_next;
      dirp->buffer_processed += entry_size;
      return dirent;

      read_entries:;
    }
    dirp->buffer_used = dirp->buffer_processed = dirp->buffer_size;
    errno_t error = fd_readdir(dirp->fd, (uint8_t *)dirp->buffer, 
      dirp->buffer_size, dirp->cookie, &dirp->buffer_used);
    if (error != 0) {
      errno = (int)error;
      break;
    }
    dirp->buffer_processed = 0;
  }
  return NULL;
}

void rewinddir(DIR *dirp) 
{
  dirp->cookie = DIRCOOKIE_START;
  dirp->buffer_used = dirp->buffer_processed = dirp->buffer_size;
}

void seekdir(DIR *dirp, long loc) 
{
  dirp->cookie = (unsigned long)loc;
  dirp->buffer_used = dirp->buffer_processed = dirp->buffer_size;
}

long telldir(DIR *dirp) 
{
  return (long)dirp->cookie;
}


static int sel_true(const struct dirent *de) 
{
  return 1;
}

int scandirat(int dirfd, const char *dir, struct dirent ***namelist,
  int (*sel)(const struct dirent *),
  int (*compar)(const struct dirent **, const struct dirent **)) 
{
  if (sel == NULL) sel = &sel_true;
  int fd = openat(dirfd, dir, O_RDONLY|O_NONBLOCK|O_DIRECTORY);
  if (fd == -1) return -1;

  size_t buffer_size = DIRENT_DEFAULT_BUFFER_SIZE;
  char *buffer = malloc(buffer_size);
  if (buffer == NULL) {
    close(fd);
    return -1;
  }
  
  { size_t buffer_processed = buffer_size;
    size_t buffer_used = buffer_size;
    struct dirent **dirents = NULL;
    size_t dirents_size = 0;
    size_t dirents_used = 0;
    dircookie_t cookie = DIRCOOKIE_START;
    
    for (;;) {
      { size_t buffer_left = buffer_used - buffer_processed;
        if (buffer_left < sizeof(dirent_t)) {
          if (buffer_used < buffer_size) break;
          goto read_entries;
        }

        dirent_t entry;
        memcpy(&entry, buffer + buffer_processed, sizeof(dirent_t));
        size_t entry_size = sizeof(dirent_t) + entry.d_namlen;
        if (entry.d_namlen == 0) {
          buffer_processed += entry_size;
          continue;
        }

        if (buffer_left < entry_size) {
          while (buffer_size < entry_size) buffer_size *= 2;
          char *new_buffer = realloc(buffer, buffer_size);
          if (new_buffer == NULL) goto bad;
          buffer = new_buffer;
          goto read_entries;
        }

        const char *name = buffer + buffer_processed + sizeof(dirent_t);
        buffer_processed += entry_size;
        if (memchr(name, '\0', entry.d_namlen) != NULL) continue;

        struct dirent *dirent = malloc(offsetof(struct dirent, d_name) + entry.d_namlen + 1);
        if (dirent == NULL) goto bad;
        dirent->d_ino = entry.d_ino;
        dirent->d_type = entry.d_type;
        memcpy(&dirent->d_name[0], name, entry.d_namlen);
        dirent->d_name[entry.d_namlen] = '\0';
        cookie = entry.d_next;

        if ((*sel)(dirent)) {
          if (dirents_used == dirents_size) {
            dirents_size = dirents_size < 8 ? 8 : dirents_size * 2;
            struct dirent **new_dirents = realloc(dirents, dirents_size * sizeof(struct dirent *));
            if (new_dirents == NULL) {
              free(dirent);
              goto bad;
            }
            dirents = new_dirents;
          }
          dirents[dirents_used++] = dirent;
        } else {
          free(dirent);
        }
        continue;
        read_entries:;
      }
      errno_t error = fd_readdir(fd, (uint8_t*)buffer, buffer_size, cookie, &buffer_used);
      if (error != 0) {
        errno = (int)error;
        goto bad;
      }
      buffer_processed = 0;
    }

    free(buffer);
    close(fd);
    qsort(dirents, dirents_used, sizeof(struct dirent *), (int (*)(const void *, const void *))compar);
    *namelist = dirents;
    return (int)dirents_used;

    bad:; 
  }

  size_t i;
  for (i = 0; i < dirents_used; ++i) free(dirents[i]);
  free(dirents);
  free(buffer);
  close(fd);
  return -1;
}

int scandir(const char *dir, struct dirent ***namelist,
  int (*filter)(const struct dirent *),
  int (*compar)(const struct dirent **, const struct dirent **)) 
{
  char *relpath;
  int dirfd = find_relpath(dir, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  return scandirat(dirfd, relpath, namelist, filter, compar);
}

int alphasort (const struct dirent **a, const struct dirent **b)
{
  return strcoll(&(*a)->d_name[0], &(*b)->d_name[0]);
}
