#include <wasi/api.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int renameat(int oldfd, const char *oldpath, int newfd, const char *newpath)
{
  errno_t error = path_rename(oldfd, oldpath, strlen(oldpath), newfd, newpath, strlen(newpath));
  if (error != 0) {
    errno = (int)error;
    return -1;
  }
  return 0;
}

int rename(const char *oldpath, const char *newpath) 
{
  char *oldrelpath;
  int olddirfd = find_relpath(oldpath, &oldrelpath);
  if (olddirfd != -1) {
    char *newrelpath;
    int newdirfd = find_relpath(newpath, &newrelpath);
    if (newdirfd != -1)
      return renameat(olddirfd, oldrelpath, newdirfd, newrelpath);
  }
  errno = ENOTCAPABLE;
  return -1;
}

int remove(const char *path) 
{
  char *relpath;
  int dirfd = find_relpath(path, &relpath);
  if (dirfd == -1) {
    errno = ENOTCAPABLE;
    return -1;
  }
  int r = unlinkat(dirfd, relpath, 0);
  if (r != 0 && (errno == EISDIR || errno == ENOTCAPABLE)) {
    r = rmdirat(dirfd, relpath);
    if (errno == ENOTDIR) errno = ENOTCAPABLE;
  }
  return r;
}
