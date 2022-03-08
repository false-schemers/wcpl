/* data returned by the stat function (abridged) */

#pragma module "stat"
#pragma once
#include <sys/types.h>

#define S_IFBLK  (0x6000)
#define S_IFCHR  (0x2000)
#define S_IFDIR  (0x4000)
#define S_IFLNK  (0xa000)
#define S_IFREG  (0x8000)
#define S_IFSOCK (0xc000)
#define S_IFIFO  (0xc000)
#define S_IFMT   (S_IFBLK|S_IFCHR|S_IFDIR|S_IFIFO|S_IFLNK|S_IFREG|S_IFSOCK)

#define S_IXOTH (0x1)
#define S_IWOTH (0x2)
#define S_IROTH (0x4)
#define S_IRWXO (S_IXOTH|S_IWOTH|S_IROTH)
#define S_IXGRP (0x8)
#define S_IWGRP (0x10)
#define S_IRGRP (0x20)
#define S_IRWXG (S_IXGRP|S_IWGRP|S_IRGRP)
#define S_IXUSR (0x40)
#define S_IWUSR (0x80)
#define S_IRUSR (0x100)
#define S_IRWXU (S_IXUSR|S_IWUSR|S_IRUSR)
#define S_ISVTX (0x200)
#define S_ISGID (0x400)
#define S_ISUID (0x800)

#define S_ISBLK(m)  (((m)&S_IFMT) == S_IFBLK)
#define S_ISCHR(m)  (((m)&S_IFMT) == S_IFCHR)
#define S_ISDIR(m)  (((m)&S_IFMT) == S_IFDIR)
#define S_ISFIFO(m) (((m)&S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m)&S_IFMT) == S_IFLNK)
#define S_ISREG(m)  (((m)&S_IFMT) == S_IFREG)
#define S_ISSOCK(m) (((m)&S_IFMT) == S_IFSOCK)

struct stat {
  dev_t st_dev;
  ino_t st_ino;
  nlink_t st_nlink;
  mode_t st_mode;
  /* uid_t st_uid; */
  /* gid_t st_gid; */
  /* dev_t st_rdev; */
  off_t st_size;
  /* blksize_t st_blksize; */
  /* blkcnt_t st_blocks; */
  time_t st_atime; 
  time_t st_mtime; 
  time_t st_ctime; 
  long st_atime_ns;
  long st_mtime_ns;
  long st_ctime_ns;
};

extern int fstat(int fd, struct stat *ps);
extern int fstatat(int fd, const char *path, struct stat *ps, int atflag);
extern int stat(const char *path, struct stat *ps);
extern int lstat(const char *path, struct stat *ps);
extern int mkdirat(int fd, const char *path, mode_t /* ignored, use S_IRWXU */);
extern int mkdir(const char *path, mode_t /* ignored, use S_IRWXU */);

/* NYI:
int    chmod(const char *, mode_t);
int    fchmod(int, mode_t);
int    mkfifo(const char *, mode_t);
int    mknod(const char *, mode_t, dev_t);
mode_t umask(mode_t);
*/
