/* file control options (abridged) */

#pragma once
#include <sys/types.h>

#define O_APPEND    (0x00000001)  /* = WASI FDFLAGS_APPEND */
#define O_DSYNC     (0x00000002)  /* = WASI FDFLAGS_DSYNC */
#define O_NONBLOCK  (0x00000004)  /* = WASI FDFLAGS_NONBLOCK */
#define O_RSYNC     (0x00000008)  /* = WASI FDFLAGS_RSYNC */
#define O_SYNC      (0x00000010)  /* = WASI FDFLAGS_SYNC */
#define O_CREAT     (0x00001000)  /* = WASI OFLAGS_CREAT<<12 */ 
#define O_DIRECTORY (0x00002000)  /* = WASI OFLAGS_DIRECTORY<<12 */
#define O_EXCL      (0x00004000)  /* = WASI OFLAGS_EXCL<<12 */
#define O_TRUNC     (0x00008000)  /* = WASI OFLAGS_TRUNC<<12 */
#define O_NOFOLLOW  (0x01000000)
#define O_EXEC      (0x02000000)
#define O_RDONLY    (0x04000000)
#define O_SEARCH    (0x08000000)
#define O_WRONLY    (0x10000000)
#define O_RDWR      (O_RDONLY|O_WRONLY)
#define O_ACCMODE   (O_EXEC|O_RDWR|O_SEARCH)

#define POSIX_FADV_NORMAL     (0)  /* = WASI ADVICE_NORMAL */
#define POSIX_FADV_SEQUENTIAL (1)  /* = WASI ADVICE_SEQUENTIAL */
#define POSIX_FADV_RANDOM     (2)  /* = WASI ADVICE_RANDOM */
#define POSIX_FADV_WILLNEED   (3)  /* = WASI ADVICE_WILLNEED */
#define POSIX_FADV_DONTNEED   (4)  /* = WASI ADVICE_DONTNEED */
#define POSIX_FADV_NOREUSE    (5)  /* = WASI ADVICE_NOREUSE */

#define F_GETFD (1)
#define F_SETFD (2)
#define F_GETFL (3)
#define F_SETFL (4)

#define FD_CLOEXEC (1)

#define AT_EACCESS          (0x0)
#define AT_SYMLINK_NOFOLLOW (0x1)
#define AT_SYMLINK_FOLLOW   (0x2)
#define AT_REMOVEDIR        (0x4)

#define AT_FDCWD (-2)

extern int open(const char *path, int oflags);
extern int openat(int fd, const char *path, int oflags);
extern int fcntl(int fd, int cmd, ...);
extern int posix_fadvise(int fd, off_t offset, off_t len, int advice);
extern int posix_fallocate(int fd, off_t offset, off_t len);

/* NYI
extern int open(const char *path, int oflags, mode_t);
extern int openat(int fd, const char *path, int oflags, mode_t);
extern int creat(const char *path, mode_t);
*/

/* helper function for abspath calls; returns fd or -1 */
extern int find_relpath(const char *path, char **relpath);
