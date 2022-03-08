/* format of directory entries */

#pragma once
#include <sys/types.h>

#define DT_UNKNOWN 0  /* = WASI FILETYPE_UNKNOWN */
#define DT_BLK     1  /* = WASI FILETYPE_BLOCK_DEVICE */
#define DT_CHR     2  /* = WASI FILETYPE_CHARACTER_DEVICE */
#define DT_DIR     3  /* = WASI FILETYPE_DIRECTORY */
#define DT_REG     4  /* = WASI FILETYPE_REGULAR_FILE */
#define DT_FIFO    6  /* = WASI FILETYPE_SOCKET_STREAM */
#define DT_LNK     7  /* = WASI FILETYPE_SYMBOLIC_LINK */

struct dirent {
  ino_t d_ino;
  unsigned char d_type;
  char d_name[1];
};

typedef struct _DIR DIR;

extern DIR *fdopendir(int fd);
extern int fdclosedir(DIR *dirp);
extern DIR *opendirat(int dirfd, const char *dirname);
extern DIR *opendir(const char *dirname);
extern int dirfd(DIR *dirp);
extern int closedir(DIR *dirp);
extern struct dirent *readdir(DIR *dirp);
extern void rewinddir(DIR *dirp);
extern void seekdir(DIR *dirp, long loc);
extern long telldir(DIR *dirp);
extern int scandirat(int dirfd, const char *dir, struct dirent ***namelist,
  int (*filter)(const struct dirent *),
  int (*compar)(const struct dirent **, const struct dirent **)); 
extern int scandir(const char *dir, struct dirent ***namelist,
  int (*filter)(const struct dirent *),
  int (*compar)(const struct dirent **, const struct dirent **));
extern int alphasort (const struct dirent **a, const struct dirent **b);

/* NYI:
int readdir_r(DIR *restrict, struct dirent *restrict, struct dirent **restrict);
*/
