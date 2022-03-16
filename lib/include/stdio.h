/* Input/output */

#pragma once
#include <stdarg.h>
#include <sys/defs.h>

#define EOF (-1)
#define BUFSIZ 1024
#define SBFSIZ 8
#define FOPEN_MAX 20 /* max #files open at once */
#define FILENAME_MAX 260 /* longest file name */

typedef long long fpos_t;

typedef struct _iobuf {
  ssize_t cnt;         /* characters left */
  unsigned char *base; /* location of buffer */
  unsigned char *ptr;  /* next character position */
  unsigned char *end;  /* end of buffer */
  int flags;           /* set of _IOXXX flags */
  int fd;              /* file descriptor (= WASI fd_t) */
  char sbuf[SBFSIZ];   /* 1-mbchar buffer */
} FILE;

#define _IOFBF   0000
#define _IOREAD  0001
#define _IOWRT   0002
#define _IONBF   0004
#define _IOEOF   0020
#define _IOERR   0040
#define _IOLBF   0100
#define _IORW    0200
#define _IOMYBUF 0400

extern FILE _iob[FOPEN_MAX];
#define stdin  (&_iob[0])
#define stdout (&_iob[1])
#define stderr (&_iob[2])

extern FILE *fopen(const char *filename, const char *mode);
extern FILE *fdopen(int fd, const char *mode);
extern FILE *freopen(const char *filename, const char *mode, FILE *stream);
extern int int fclose(FILE *stream);
#define getc(p) (--(p)->cnt >= 0 ? (unsigned char)*(p)->ptr++ : _fillbuf(p))
#define putc(x, p) (--(p)->cnt >= 0 ? *(p)->ptr++ = (x) : _flushbuf((x), p))
#define getchar() (getc(stdin))
#define putchar(x) (putc((x), stdout))
#define clearerr(p) ((void)((p)->flags &= ~(_IOERR|_IOEOF)))
#define feof(p) ((p)->flags & _IOEOF)
#define ferror(p) ((p)->flags & _IOERR)
#define fileno(p) ((p)->fd)
extern int _fillbuf(FILE *stream);
extern int _flushbuf(int c, FILE *stream);
extern int fgetc(FILE *stream);
extern char *fgets(char *s, int n, FILE *stream);
extern int fputc(int c, FILE *stream);
extern int fputs(const char *s, FILE *stream);
extern int ungetc(int c, FILE *stream);
/* char *gets(char *s) -- obsolete */
extern size_t fread(void *ptr, size_t size, size_t nobj, FILE *stream);
extern size_t fwrite(const void *ptr, size_t size, size_t nobj, FILE *stream);
extern int fseek(FILE *stream, long offset, int origin);
extern long ftell(FILE *stream);
extern void rewind(FILE *stream);
extern int fgetpos(FILE *stream, fpos_t *ptr);
extern int fsetpos(FILE *stream, const fpos_t *ptr);
extern int fflush(FILE *stream);
extern int setvbuf(FILE *stream, char *buf, int mode, size_t size);
extern void setbuf(FILE *stream, char *buf);
extern int fprintf(FILE *stream, const char *format, ...);
extern int printf(const char *format, ...);
extern int sprintf(char *s, const char *format, ...);
extern int snprintf(char *s, size_t count, const char *format, ...);
extern int vprintf(const char *format, va_list arg);
extern int vfprintf(FILE *stream, const char *format, va_list arg);
extern int vsprintf(char *s, const char *format, va_list arg);
extern int vsnprintf(char *s, size_t count, const char* format, va_list arg);
extern int fscanf(FILE *stream, const char *format, ...);
extern int scanf(const char *format, ...);
extern int sscanf(const char *s, const char *format, ...);
extern int snscanf(const char *s, size_t count, const char *format, ...);
extern int vfscanf(FILE *stream, const char *format, va_list arg);
extern int vscanf(const char *format, va_list arg);
extern int vsscanf(const char *s, const char *format, va_list arg);
extern int vsnscanf(const char *s, size_t count, const char *format, va_list arg);
/* extern FILE *tmpfile(void); -- not in WASI? */
/* extern char *tmpnam(char s[L_tmpnam]); -- not in WASI? */
extern int int remove(const char *filename);
extern int rename(const char *oldname, const char *newname);
extern void perror(const char *s);

/* *at variants */
extern int renameat(int oldfd, const char *oldpath, int newfd, const char *newpath);
