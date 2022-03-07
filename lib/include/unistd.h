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

/* NYI
extern int fdatasync(int);
extern int fsync(int);
extern void _exit(int);
extern int link(const char *, const char *);
extern int linkat(int, const char *, int, const char *, int);
extern ssize_t readlink(const char *restrict, char *restrict, size_t);
extern ssize_t readlinkat(int, const char *restrict, char *restrict, size_t);
extern int unlink(const char *);
extern int unlinkat(int, const char *, int);
extern int symlink(const char *, const char *);
extern int symlinkat(const char *, int, const char *);
extern int access(const char *, int);
*/
