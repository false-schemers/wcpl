/* standard symbolic constants and types (abridged) */

#pragma once
#include <sys/types.h>

#define STDIN_FILENO  (0)
#define STDOUT_FILENO (1)
#define STDERR_FILENO (2)
#define SEEK_SET (0) /* = WASI WHENCE_SET */
#define SEEK_CUR (1) /* = WASI WHENCE_CUR */
#define SEEK_END (2) /* = WASI WHENCE_END */
#define F_OK (0)
#define X_OK (1)
#define W_OK (2)
#define R_OK (4)

extern ssize_t read(int fd, void *buf, size_t n);
extern ssize_t write(int fd, const void *buf, size_t n);
extern ssize_t pread(int fd, void *buf, size_t nbyte, off_t offset);
extern ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset);
extern off_t lseek(int fd, off_t offset, int whence);
extern int ftruncate(int fd, off_t length);
extern int close(int fd);
extern int isatty(int fd);
extern void *sbrk(intptr_t inc); /* inc should be a multiple of 64K */
extern int access(const char *path, int amode);
extern int unlinkat(int fd, const char *path, int atflag);
extern int unlink(const char *path);
extern int linkat(int oldfd, const char *oldpath, int newfd, const char *newpath, int atflag);
extern int link(const char *oldpath, const char *newpath);
extern int symlinkat(const char *tgtpath, int fd, const char *lnkpath);
extern int symlink(const char *tgtpath, const char *lnkpath);
extern ssize_t readlinkat(int fd, const char *path, char *buf, size_t bufsize);
extern ssize_t readlink(const char *path, char *buf, size_t bufsize);
extern int fdatasync(int fd);
extern int fsync(int fd);
extern void _exit(int status);
