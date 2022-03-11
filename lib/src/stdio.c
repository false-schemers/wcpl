#include <wasi/api.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <float.h>
#include <stdio.h>

/* stream i/o after K&R TCPL */

FILE _iob[FOPEN_MAX] = {
  { 0, NULL, NULL, NULL, _IOREAD,       0, {} }, /* stdin */
  { 0, NULL, NULL, NULL, _IOWRT,        1, {} }, /* stdout */
  { 0, NULL, NULL, NULL, _IOWRT|_IONBF, 2, {} }  /* stderr */
};

static FILE *findfp(void)
{
  FILE *fp;
  for (fp = &_iob[0]; fp < &_iob[FOPEN_MAX]; ++fp)
    if ((fp->flags & (_IOREAD|_IOWRT|_IORW)) == 0)
      return fp; /* found free slot */
  return NULL;
}

/* atexit() handler */
static bool closeall_set = false;
static void closeall(void)
{
  FILE *fpi;
  for (fpi = &_iob[0]; fpi < &_iob[FOPEN_MAX]; ++fpi) {
    if (fpi->flags & (_IOREAD|_IOWRT|_IORW)) 
      fclose(fpi);
  }
}

FILE *freopen(const char *filename, const char *mode, FILE *fp)
{
  int plus, oflags, fd;
  if (fp != NULL) fclose(fp); else fp = findfp();
  if (fp == NULL || filename == NULL || mode == NULL) return NULL;
  switch (mode[0]) {
    case 'w': oflags = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_TRUNC | O_CREAT; break;
    case 'a': oflags = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_APPEND | O_CREAT; break;
    case 'r': oflags = mode[1] == '+' ? O_RDWR : O_RDONLY; break;
    default: return NULL;
  }
  if ((fd = open(filename, oflags)) < 0) return NULL;
  fp->fd = fd; fp->cnt = 0;
  fp->base = fp->ptr = fp->end = NULL;
  fp->flags = (oflags & O_RDWR) ? _IORW : (oflags & O_RDONLY) ? _IOREAD : _IOWRT;
  if ((oflags & O_APPEND) && !(oflags & O_RDWR) && lseek(fd, 0L, SEEK_END) < 0) {
    fclose(fp); fp = NULL;
  }
  if (!closeall_set) { atexit(&closeall); closeall_set = true; }
  return fp;
}

FILE *fopen(const char *name, const char *mode)
{
  return freopen(name, mode, NULL);
}

FILE *fdopen(int fd, const char *mode)
{
	FILE *fp = findfp();
	if (fp == NULL) return NULL;
	fp->fd = fd;
	fp->cnt = 0;fp->flags = 0;
  fp->base = fp->ptr = fp->end = NULL;
	switch (*mode) {
		case 'r': fp->flags |= _IOREAD; break;
		case 'a': lseek(fd, 0L, 2); /* fall thru */
		case 'w':	fp->flags |= _IOWRT; break;
		default: return NULL;
	}
	if (mode[1] == '+') {
		fp->flags &= ~(_IOREAD|_IOWRT);
		fp->flags |= _IORW;
	}
  if (!closeall_set) { atexit(&closeall); closeall_set = true; }
	return fp;
}

int fclose(FILE *fp)
{
  int res = -1;
  if (fp == NULL) return res;
  if (fp->flags & (_IOREAD|_IOWRT|_IORW)) {
    res = (fp->flags & _IONBF)? 0 : fflush(fp);
    if (close(fp->fd) < 0) {
      errno = ENOENT;
      res = -1; 
    }
  }
  if (fp->base && fp->base != &fp->sbuf[0]) {
    if (fp->flags & _IOMYBUF) free(fp->base);
  }
  bzero(fp, sizeof(FILE));
  return res;
}

static void addbuf(FILE *fp)
{
  if (fp->flags & _IONBF) {
    fp->base = (unsigned char *)&fp->sbuf[0];
    fp->end = fp->base + SBFSIZ;
  } else {
    fp->base = (unsigned char *)malloc(BUFSIZ + SBFSIZ);
    if (fp->base != NULL) {
      fp->end = fp->base + BUFSIZ + SBFSIZ;
      fp->flags |= _IOMYBUF;
    } else { 
      fp->base = (unsigned char *)&fp->sbuf[0]; 
      fp->end = fp->base + SBFSIZ; 
    }
  }
  fp->ptr = fp->base;
}

/* getc helper (returns next char); precondition: f->cnt is <= 0 */
int _fillbuf(FILE *fp)
{
  FILE *fpi; size_t readc;
  if (fp->base == NULL) addbuf(fp);
  if (!(fp->flags & _IOREAD)) {
    if (fp->flags & _IORW) fp->flags |= _IOREAD;
    else return EOF;
  }
  /* if this device is a terminal (line-buffered) or unbuffered, then
   * flush buffers of all line-buffered devices currently writing */
  if (fp->flags & (_IOLBF|_IONBF))
    for (fpi = &_iob[0]; fpi < &_iob[FOPEN_MAX]; ++fpi)
      if (fpi->flags & (_IOREAD|_IOLBF) == _IOLBF) 
        fflush(fpi);
  fp->ptr = fp->base;
  readc = (fp->flags & _IONBF) ? 1 : fp->end - fp->base;
  fp->cnt = read(fp->fd, (char *)fp->base, readc);
  if (--fp->cnt >= 0) return (*fp->ptr++);
  else if (fp->cnt != -1) fp->flags |= _IOERR;
  else { /* EOF */
    fp->flags |= _IOEOF;
    if (fp->flags & _IORW) fp->flags &= ~_IOREAD;
  }
  fp->cnt = 0;
  return EOF;
}

size_t fread(void *buf, size_t size, size_t count, FILE *fp)
{
  char *ptr = buf; size_t nleft; ssize_t n;
  if (size == 0 || count == 0) return 0;
  nleft = count * size;
  for (;;) {
    if (fp->cnt <= 0) {
      if (_fillbuf(fp) == EOF)
        return (count - (nleft+size-1)/size);
      fp->ptr--; fp->cnt++; /* ungetc */
    }
    n = fp->cnt; if (nleft < (size_t)n) n = (ssize_t)nleft;
    memcpy(ptr, (char *)fp->ptr, n);
    ptr += n; fp->ptr += n; fp->cnt -= n; 
    if (nleft <= (size_t)n) return count;
    nleft -= (size_t)n;
  }
  return 0; /* unreacheable */
}

static int writebuf(FILE *fp)
{
  unsigned char *base = fp->base;
  ssize_t n = fp->ptr - base;
  fp->ptr = base;
  fp->cnt = (fp->flags & (_IONBF|_IOLBF)) ? 0 : fp->end - base;
  if (n > 0 && n != write(fp->fd, (char*)base, (size_t)n)) {
    fp->flags |= _IOERR;
    return EOF;
  }
  return 0;
}

static int wrtchk(FILE *fp)
{
  if ((fp->flags & (_IOWRT|_IOEOF)) != _IOWRT) {
    if (!(fp->flags & (_IOWRT|_IORW))) return EOF; /* read-only */
    fp->flags &= ~_IOEOF|_IOWRT; /* clear flags */
  }
  if (fp->base == NULL) {
    /* first attempt to bufferize output */
    addbuf(fp);
    /* we will have to flush even if all we use is stdout/stderr */
    if (!closeall_set) { atexit(&closeall); closeall_set = true; }
  }
  if (fp->ptr == fp->base && !(fp->flags & (_IONBF|_IOLBF)))  {
    fp->cnt = fp->end - fp->base; /* first write since seek--set cnt */
  }
  return 0;
}

/* putc helper; precondition: f->cnt <= 0 */
int _flushbuf(int c, FILE *fp)
{
  unsigned char c1;
  do {
    /* check for linebuffered with write perm, but no EOF */
    if ((fp->flags & (_IOLBF|_IOWRT|_IOEOF)) == (_IOLBF|_IOWRT)) {
      if (fp->ptr >= fp->end) break; /* flush buf and be done */
      if ((*fp->ptr++ = c) != '\n') return c;
      return writebuf(fp) == EOF ? EOF : c;
    }
    /* write out an unbuffered file, if have write perm, but no EOF */
    if ((fp->flags & (_IONBF|_IOWRT|_IOEOF)) == (_IONBF|_IOWRT)) {
      c1 = c; fp->cnt = 0;
      if (write(fp->fd, (char *)&c1, 1) == 1) return c;
      fp->flags |= _IOERR;
      return EOF;
    }
    /* The _wrtchk call is here rather than at the top of _flushbuf to 
     * reduce overhead for line-buffered I/O under normal circumstances. */
    if (wrtchk(fp) != 0) return EOF;
  } while (fp->flags & (_IONBF|_IOLBF));
  writebuf(fp); /* full buffer: flush it */
  putc((char)c, fp); /* then put "c" in newly emptied buf */
  return ferror(fp) ? EOF : c;
}

size_t fwrite(const void *buf, size_t size, size_t count, FILE *fp)
{
  char *ptr = buf; size_t nleft; ssize_t n;
  unsigned char *cptr;
  if (size == 0 || count == 0 || wrtchk(fp)) return 0;
  nleft = count*size;
  /* if the file is unbuffered, or if the fp->ptr == fp->base, and there
   * is > BUFSIZ chars to write, we can do a direct write */
  if (fp->base >= fp->ptr) { /* this covers the unbuffered case, too */
    if ((fp->flags & _IONBF) != 0 || nleft >= BUFSIZ) {
      n = write(fp->fd, ptr, nleft);
      if (n != nleft) { fp->flags |= _IOERR; n = (n >= 0) ? n : 0; }
      return n/size;
    }
  }
  /* put characters in the buffer */
  for (;; ptr += n) {
    while ((n = fp->end - (cptr = fp->ptr)) <= 0) { /* full buf */
      if (writebuf(fp) == EOF) return count - (nleft + size - 1)/size;
    }
    if (nleft < n) n = (ssize_t)nleft;
    memcpy(cptr, ptr, n);
    fp->cnt -= n; fp->ptr += n;
    /* done; flush if linebuffered with a newline */
    if ((nleft -= n) == 0) { 
      if (fp->flags & (_IOLBF|_IONBF)) {
        if ((fp->flags & _IONBF) || (memchr(fp->base, '\n', count*size) != NULL)) 
          writebuf(fp);
      }
      return (count);
    }
  }
  return 0; /* unreacheable */
}

int fputs(const char *s, FILE *fp)
{
  ssize_t ndone = 0, n;
  unsigned char *cptr; char *p;
  if (wrtchk(fp)) return 0;
  if ((fp->flags & _IONBF) == 0) {
    for (;; s += n) {
      while ((n = fp->end - (cptr = fp->ptr)) <= 0) { /* full buf */
        if (writebuf(fp) == EOF) return EOF;
      }
      if ((p = memccpy((char *)cptr, s, '\0', n)) != NULL) n = (p - (char *)cptr) - 1;
      fp->cnt -= n; fp->ptr += n;
      ndone += n;
      if (p != NULL)  { 
        /* done; flush buffer if line-buffered */
        if (fp->flags & _IOLBF) if (writebuf(fp) == EOF) return EOF;
        return (int)ndone;
      }
    }
  } else {
    /* write out to an unbuffered file */
    size_t cnt = strlen(s);
    write(fp->fd, s, cnt);
    return (int)cnt;
  }
  return 0; /* unreacheable */
}

/* macros in stdio.h
int feof(FILE *fp)
{
  return (fp->flags & _IOEOF) != 0;
}

int ferror(FILE *fp)
{
  return (fp->flags & _IOERR) != 0);
}
*/

int fgetc(FILE *fp)
{
  return getc(fp);
}

int fputc(int c, FILE *fp)
{
  return putc(c, fp);
}

char *fgets(char *s, int size, FILE *fp)
{
  char *p, *s0 = s; ssize_t n;
  for (size--; size > 0; size -= (int)n) {
    if (fp->cnt <= 0) { /* empty buffer */
      if (_fillbuf(fp) == EOF) {
        if (s0 == s) return NULL;
        break; /* no more data */
      }
      fp->ptr--; fp->cnt++;
    }
    n = (size < fp->cnt) ? size : fp->cnt;
    if ((p = memccpy(s, fp->ptr, '\n', n)) != NULL) n = p - s;
    s += n;
    fp->cnt -= n; fp->ptr += n;
    if (p != NULL) break; /* found '\n' in buffer */
  }
  *s = '\0';
  return s0;
}

int ungetc(int c, FILE *fp)
{
  if (c == EOF) return -1;
  if ((fp->flags & _IOREAD) == 0 || fp->ptr <= fp->base) {
    if (fp->ptr == fp->base && fp->cnt == 0) ++fp->ptr;
    else return -1;
  }
  *--fp->ptr = c; ++fp->cnt;
  return c;
}

int fseek(FILE *fp, long offset, int origin)
{
  ssize_t c; long p;
  fp->flags &= ~_IOEOF;
  if (fp->flags & _IOREAD) {
    if (origin < 2 && fp->base != NULL && !(fp->flags & _IONBF)) {
      c = fp->cnt; p = offset;
      if (origin == 0) p += (long)c - (long)lseek(fp->fd, 0, SEEK_CUR);
      else offset -= (long)c;
      if (!(fp->flags & _IORW) && c > 0 && p <= c && p >= fp->base - fp->ptr) {
        fp->ptr += (int)p; fp->cnt -= (int)p;
        return 0;
      }
    }
    if (fp->flags & _IORW) {
      fp->ptr = fp->base;
      fp->flags &= ~_IOREAD;
    }
    p = (long)lseek(fp->fd, offset, origin);
    fp->cnt = 0;
  } else if (fp->flags & (_IOWRT|_IORW)) {
    fflush(fp);
    fp->cnt = 0;
    if (fp->flags & _IORW) {
      fp->flags &= ~_IOWRT;
      fp->ptr = fp->base;
    }
    p = (long)lseek(fp->fd, offset, origin);
  }
  return (p == -1) ? -1 : 0;
}

long ftell(FILE *fp)
{
  long offset; ssize_t adjust;
  if (fp->cnt < 0) fp->cnt = 0;
  if (fp->flags & _IOREAD) {
    adjust = - fp->cnt;
  } else if (fp->flags & (_IOWRT|_IORW)) {
    adjust = 0;
    if (fp->flags & _IOWRT && fp->base != NULL && (fp->flags & _IONBF) == 0)
      adjust = fp->ptr - fp->base;
  } else return -1;
  offset = (long)lseek(fp->fd, 0, SEEK_CUR);
  if (offset >= 0) offset += (long)adjust;
  return offset;
}

void rewind(FILE *fp)
{
  fflush(fp);
  lseek(fp->fd, 0, SEEK_SET);
  fp->cnt = 0; fp->ptr = fp->base;
  fp->flags &= ~(_IOERR|_IOEOF);
  if (fp->flags & _IORW) fp->flags &= ~(_IOREAD|_IOWRT);
}

int fgetpos(FILE *fp, fpos_t *ptr)
{
  static_assert(sizeof(fpos_t) == sizeof(off_t));
  off_t offset; ssize_t adjust;
  if (fp->cnt < 0) fp->cnt = 0;
  if (fp->flags & _IOREAD) {
    adjust = - fp->cnt;
  } else if (fp->flags & (_IOWRT|_IORW)) {
    adjust = 0;
    if (fp->flags & _IOWRT && fp->base != NULL && (fp->flags & _IONBF) == 0)
      adjust = fp->ptr - fp->base;
  } else return -1;
  offset = lseek(fp->fd, 0, SEEK_CUR);
  if (offset >= 0) offset += adjust;
  *ptr = offset;
  return 0;
}

int fsetpos(FILE *fp, const fpos_t *ptr)
{
  static_assert(sizeof(fpos_t) == sizeof(off_t));
  ssize_t c; off_t offset = *ptr, p;
  fp->flags &= ~_IOEOF;
  if (fp->flags & _IOREAD) {
    if (fp->base != NULL && !(fp->flags & _IONBF)) {
      c = fp->cnt; p = offset;
      p += (off_t)c - lseek(fp->fd, 0, SEEK_CUR);
      if (!(fp->flags & _IORW) && c > 0 && p <= c && p >= fp->base - fp->ptr) {
        fp->ptr += (int)p; fp->cnt -= (int)p;
        return 0;
      }
    }
    if (fp->flags & _IORW) {
      fp->ptr = fp->base;
      fp->flags &= ~_IOREAD;
    }
    p = lseek(fp->fd, offset, SEEK_SET);
    fp->cnt = 0;
  } else if (fp->flags & (_IOWRT|_IORW)) {
    fflush(fp);
    fp->cnt = 0;
    if (fp->flags & _IORW) {
      fp->flags &= ~_IOWRT;
      fp->ptr = fp->base;
    }
    p = lseek(fp->fd, offset, SEEK_SET);
  }
  return (p == -1) ? -1 : 0;
}

int fflush(FILE *fp)
{
  if (!(fp->flags & _IOWRT)) {
    fp->cnt = 0;
    return 0;
  }
  while (!(fp->flags & _IONBF) && (fp->flags & _IOWRT) &&
         (fp->base != NULL) && (fp->ptr > fp->base)) {
    writebuf(fp);
  }
  return ferror(fp) ? EOF : 0;
}

int setvbuf(FILE *fp, char *buf, int mode, size_t size)
{
  if (fp->base != NULL && fp->base != &fp->sbuf[0]) {
    if (fp->flags & _IOMYBUF) free(fp->base);
    fp->base = fp->end = NULL;
  }
  fp->flags &= ~(_IONBF|_IOLBF|_IOFBF|_IOMYBUF);
  switch (mode) {
    case _IONBF: /* file is unbuffered */
      fp->flags |= _IONBF;
      fp->base = fp->end = NULL;
      break;
    case _IOLBF:
    case _IOFBF:
      fp->flags |= mode;
      size = (size == 0) ? BUFSIZ : size;
      if (buf != NULL && size > SBFSIZ) {
        fp->base = (unsigned char *)buf;
        fp->end = fp->base + size - SBFSIZ;
      } else if (size > SBFSIZ && (fp->base = malloc(size + SBFSIZ)) != NULL) {
        fp->end = fp->base + size + SBFSIZ;
        fp->flags |= _IOMYBUF;
      } else {
        fp->base = (unsigned char *)&fp->sbuf[0]; 
        fp->end = fp->base + SBFSIZ;
      }
      break;
    default:
      return -1;
  }
  fp->ptr = fp->base;
  fp->cnt = 0;
  return 0;
}

void setbuf(FILE *fp, char *buf)
{
  setvbuf(fp, buf, buf == NULL ? _IONBF : _IOFBF, BUFSIZ);
}


/* formatted output */
/* derived from: https://github.com/mpaland/printf (c) Marco Paland (info@paland.com) */

/* 'ntoa' conversion buffer size, this must be big enough to hold one converted
 * numeric number including padded zeros (dynamically created on stack) */
#define PRINTF_NTOA_BUFFER_SIZE    32U

/* 'ftoa' conversion buffer size, this must be big enough to hold one converted
 * float number including padded zeros (dynamically created on stack) */
#define PRINTF_FTOA_BUFFER_SIZE    32U

/* default floating point precision */
#define PRINTF_DEFAULT_FLOAT_PRECISION  6U

/* largest float suitable to print with %f */
#define PRINTF_MAX_FLOAT  1e9

// internal flag definitions
#define FLAGS_ZEROPAD   (1U <<  0U)
#define FLAGS_LEFT      (1U <<  1U)
#define FLAGS_PLUS      (1U <<  2U)
#define FLAGS_SPACE     (1U <<  3U)
#define FLAGS_HASH      (1U <<  4U)
#define FLAGS_UPPERCASE (1U <<  5U)
#define FLAGS_CHAR      (1U <<  6U)
#define FLAGS_SHORT     (1U <<  7U)
#define FLAGS_LONG      (1U <<  8U)
#define FLAGS_LONG_LONG (1U <<  9U)
#define FLAGS_PRECISION (1U << 10U)
#define FLAGS_ADAPT_EXP (1U << 11U)

/* output function type */
typedef void (*out_fct_t)(char c, void* buffer, size_t idx, size_t maxlen);

/* wrapper (used as buffer) for output function type */
typedef struct out_fct_wrap {
  void  (*fct)(char c, void* arg);
  void* arg;
} out_fct_wrap_t;

/* internal buffer output */
static void _out_buffer(char c, void* buffer, size_t idx, size_t maxlen)
{
  if (idx < maxlen) ((char*)buffer)[idx] = c;
}

/* internal null output */
static void _out_null(char c, void* buffer, size_t idx, size_t maxlen)
{
}

/* internal putc wrapper */
static void _out_char(char c, void* buffer, size_t idx, size_t maxlen)
{
  FILE *fp = (FILE *)buffer;
  if (c) putc(c, fp);
} 

/* internal output function wrapper */
static void _out_fct(char c, void* buffer, size_t idx, size_t maxlen)
{
  out_fct_wrap_t *pw = (out_fct_wrap_t*)buffer;
  if (c) (*pw->fct)(c, pw->arg);
}

/* length of the string (excluding the terminating 0) limited by 'maxsize' */
static unsigned int _strnlen_s(const char* str, size_t maxsize)
{
  const char* s;
  for (s = str; *s && maxsize--; ++s);
  return (unsigned int)(s - str);
}

/* todo: use macro? */
static bool _is_digit(char ch)
{
  return (ch >= '0') && (ch <= '9');
}

static unsigned int _atoi(const char** str)
{
  unsigned i = 0U;
  while (_is_digit(**str)) {
    i = i * 10U + (unsigned)(*((*str)++) - '0');
  }
  return i;
}

/* output the specified string in reverse, taking care of any zero-padding */
static size_t _out_rev(out_fct_t out, char *buffer, size_t idx, size_t maxlen, const char* buf, size_t len, unsigned int width, unsigned int flags)
{
  const size_t start_idx = idx, i;
  if (!(flags & FLAGS_LEFT) && !(flags & FLAGS_ZEROPAD)) {
    for (i = len; i < width; i++) (*out)(' ', buffer, idx++, maxlen);
  }
  while (len) (*out)(buf[--len], buffer, idx++, maxlen);
  if (flags & FLAGS_LEFT) {
    while (idx - start_idx < width) (*out)(' ', buffer, idx++, maxlen);
  }
  return idx;
}

/* internal itoa format */
static size_t _ntoa_format(out_fct_t out, char *buffer, size_t idx, size_t maxlen, char* buf, size_t len, bool negative, unsigned int base, unsigned int prec, unsigned int width, unsigned int flags)
{
  if (!(flags & FLAGS_LEFT)) {
    if (width && (flags & FLAGS_ZEROPAD) && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) --width;
    while ((len < prec) && (len < PRINTF_NTOA_BUFFER_SIZE)) buf[len++] = '0';
    while ((flags & FLAGS_ZEROPAD) && (len < width) && (len < PRINTF_NTOA_BUFFER_SIZE)) buf[len++] = '0';
  }
  if (flags & FLAGS_HASH) {
    if (!(flags & FLAGS_PRECISION) && len && ((len == prec) || (len == width))) {
      --len; if (len && (base == 16U)) --len;
    }
    if ((base == 16U) && !(flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) buf[len++] = 'x';
    else if ((base == 16U) && (flags & FLAGS_UPPERCASE) && (len < PRINTF_NTOA_BUFFER_SIZE)) buf[len++] = 'X';
    else if ((base == 2U) && (len < PRINTF_NTOA_BUFFER_SIZE)) buf[len++] = 'b';
    if (len < PRINTF_NTOA_BUFFER_SIZE) buf[len++] = '0';
  }
  if (len < PRINTF_NTOA_BUFFER_SIZE) {
    if (negative) buf[len++] = '-';
    else if (flags & FLAGS_PLUS) buf[len++] = '+'; 
    else if (flags & FLAGS_SPACE) buf[len++] = ' ';
  }
  return _out_rev(out, buffer, idx, maxlen, buf, len, width, flags);
}

/* internal itoa for 'long' type */
static size_t _ntoa_long(out_fct_t out, char *buffer, size_t idx, size_t maxlen, unsigned long value, bool negative, unsigned long base, unsigned int prec, unsigned int width, unsigned int flags)
{
  char buf[PRINTF_NTOA_BUFFER_SIZE];
  size_t len = 0U;
  if (!value) flags &= ~FLAGS_HASH;
  if (!(flags & FLAGS_PRECISION) || value) {
    do {
      const char digit = (char)(value % base);
      buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
      value /= base;
    } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
  }
  return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, (unsigned int)base, prec, width, flags);
}

/* internal itoa for 'long long' type */
static size_t _ntoa_long_long(out_fct_t out, char *buffer, size_t idx, size_t maxlen, unsigned long long value, bool negative, unsigned long long base, unsigned int prec, unsigned int width, unsigned int flags)
{
  char buf[PRINTF_NTOA_BUFFER_SIZE];
  size_t len = 0U;
  if (!value) flags &= ~FLAGS_HASH;
  if (!(flags & FLAGS_PRECISION) || value) {
    do {
      const char digit = (char)(value % base);
      buf[len++] = digit < 10 ? '0' + digit : (flags & FLAGS_UPPERCASE ? 'A' : 'a') + digit - 10;
      value /= base;
    } while (value && (len < PRINTF_NTOA_BUFFER_SIZE));
  }
  return _ntoa_format(out, buffer, idx, maxlen, buf, len, negative, (unsigned int)base, prec, width, flags);
}

/* powers of 10 for _ftoa */
static const double pow10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000 };

/* forward declaration so that _ftoa can switch to exp notation for values > PRINTF_MAX_FLOAT */
static size_t _etoa(out_fct_t out, char *buffer, size_t idx, size_t maxlen, double value, unsigned int prec, unsigned int width, unsigned int flags);

/* internal ftoa for fixed decimal floating point */
static size_t _ftoa(out_fct_t out, char *buffer, size_t idx, size_t maxlen, double value, unsigned int prec, unsigned int width, unsigned int flags)
{
  char buf[PRINTF_FTOA_BUFFER_SIZE];
  size_t len  = 0U; double diff = 0.0;
  
  if (value != value)
    return _out_rev(out, buffer, idx, maxlen, "nan", 3, width, flags);
  if (value < -DBL_MAX)
    return _out_rev(out, buffer, idx, maxlen, "fni-", 4, width, flags);
  if (value > DBL_MAX)
    return _out_rev(out, buffer, idx, maxlen, (flags & FLAGS_PLUS) ? "fni+" : "fni", (flags & FLAGS_PLUS) ? 4U : 3U, width, flags);
  /* fall back to etoa for very large values */
  if ((value > PRINTF_MAX_FLOAT) || (value < -PRINTF_MAX_FLOAT)) {
    return _etoa(out, buffer, idx, maxlen, value, prec, width, flags);
  }
  bool negative = false;
  if (value < 0) { negative = true; value = -value; }
  if (!(flags & FLAGS_PRECISION)) {
    prec = PRINTF_DEFAULT_FLOAT_PRECISION;
  }
  /* limit precision to 9, cause a prec >= 10 can lead to overflow errors */
  while ((len < PRINTF_FTOA_BUFFER_SIZE) && (prec > 9U)) {
    buf[len++] = '0';
    prec--;
  }
  
  int whole = (int)value;
  double tmp = (value - whole) * pow10[prec];
  unsigned long frac = (unsigned long)tmp;
  diff = tmp - frac;
  if (diff > 0.5) {
    ++frac;
    /* handle rollover, e.g. case 0.99 with prec 1 is 1.0 */
    if (frac >= pow10[prec]) {
      frac = 0;
      ++whole;
    }
  } else if (diff < 0.5) {
  } else if ((frac == 0U) || (frac & 1U)) {
    /* if halfway, round up if odd OR if last digit is 0 */
    ++frac;
  }
  if (prec == 0U) {
    diff = value - (double)whole;
    if ((!(diff < 0.5) || (diff > 0.5)) && (whole & 1)) {
      /* exactly 0.5 and ODD, then round up:  1.5 -> 2, but 2.5 -> 2 */
      ++whole;
    }
  } else {
    unsigned int count = prec;
    /* now do fractional part, as an unsigned number */
    while (len < PRINTF_FTOA_BUFFER_SIZE) {
      --count;
      buf[len++] = (char)(48U + (frac % 10U));
      if (!(frac /= 10U)) break;
    }
    /* add extra 0s */
    while ((len < PRINTF_FTOA_BUFFER_SIZE) && (count-- > 0U)) {
      buf[len++] = '0';
    }
    if (len < PRINTF_FTOA_BUFFER_SIZE) {
      /* add decimal dot */
      buf[len++] = '.';
    }
  }

  /* do whole part, number is reversed */
  while (len < PRINTF_FTOA_BUFFER_SIZE) {
    buf[len++] = (char)(48 + (whole % 10));
    if (!(whole /= 10)) break;
  }
  if (!(flags & FLAGS_LEFT) && (flags & FLAGS_ZEROPAD)) {
    if (width && (negative || (flags & (FLAGS_PLUS | FLAGS_SPACE)))) --width;
    while ((len < width) && (len < PRINTF_FTOA_BUFFER_SIZE)) buf[len++] = '0';
  }
  if (len < PRINTF_FTOA_BUFFER_SIZE) {
    if (negative) buf[len++] = '-';
    else if (flags & FLAGS_PLUS) buf[len++] = '+';
    else if (flags & FLAGS_SPACE) buf[len++] = ' ';
  }

  return _out_rev(out, buffer, idx, maxlen, buf, len, width, flags);
}

/* internal ftoa variant for exponential floating-point type, contributed by Martijn Jasperse <m.jasperse@gmail.com> */
static size_t _etoa(out_fct_t out, char *buffer, size_t idx, size_t maxlen, double value, unsigned int prec, unsigned int width, unsigned int flags)
{
  if ((value != value) || (value > DBL_MAX) || (value < -DBL_MAX)) {
    return _ftoa(out, buffer, idx, maxlen, value, prec, width, flags);
  }
  const bool negative = value < 0; if (negative) value = -value;
  if (!(flags & FLAGS_PRECISION)) prec = PRINTF_DEFAULT_FLOAT_PRECISION;

  /* determine the decimal exponent
   * based on the algorithm by David Gay (https://www.ampl.com/netlib/fp/dtoa.c) */
  union { uint64_t U; double F; } conv;
  conv.F = value;
  int exp2 = (int)((conv.U >> 52U) & 0x07FFU) - 1023;  /* effectively log2 */
  conv.U = (conv.U & ((1ULL << 52U) - 1U)) | (1023ULL << 52U);  /* drop the exponent so conv.F is now in [1,2) */
  /* now approximate log10 from the log2 integer part and an expansion of ln around 1.5 */
  int expval = (int)(0.1760912590558 + exp2 * 0.301029995663981 + (conv.F - 1.5) * 0.289529654602168);
  /* now we want to compute 10^expval but we want to be sure it won't overflow */
  exp2 = (int)(expval * 3.321928094887362 + 0.5);
  const double z  = expval * 2.302585092994046 - exp2 * 0.6931471805599453;
  const double z2 = z * z;
  conv.U = (uint64_t)(exp2 + 1023) << 52U;
  /* compute exp(z) using continued fractions, see 
   * https://en.wikipedia.org/wiki/Exponential_function#Continued_fractions_for_ex */
  conv.F *= 1 + 2 * z / (2 - z + (z2 / (6 + (z2 / (10 + z2 / 14)))));
  /* correct for rounding errors */
  if (value < conv.F) { --expval; conv.F /= 10; }

  /* the exponent format is "%+03d" and largest value is "307", so set aside 4-5 characters */
  unsigned int minwidth = ((expval < 100) && (expval > -100)) ? 4U : 5U;

  /* in "%g" mode, "prec" is the number of *significant figures* not decimals */
  if (flags & FLAGS_ADAPT_EXP) {
    /* do we want to fall-back to "%f" mode? */
    if ((value >= 1e-4) && (value < 1e6)) {
      if ((int)prec > expval) prec = (unsigned)((int)prec - expval - 1);
      else prec = 0;
      flags |= FLAGS_PRECISION; /* make sure _ftoa respects precision */
      /* no characters in exponent */
      minwidth = 0U; expval = 0;
    } else {
      /* we use one sigfig for the whole part */
      if ((prec > 0) && (flags & FLAGS_PRECISION)) --prec;
    }
  }

  unsigned int fwidth = width;
  if (width > minwidth) { /* we didn't fall-back so subtract the characters required for the exponent */
    fwidth -= minwidth;
  } else { /* not enough characters, so go back to default sizing */
    fwidth = 0U;
  }
  if ((flags & FLAGS_LEFT) && minwidth) { /* if we're padding on the right, DON'T pad the floating part */
    fwidth = 0U;
  }

  /* rescale the float value */
  if (expval) value /= conv.F;

  /* output the floating part */
  const size_t start_idx = idx;
  idx = _ftoa(out, buffer, idx, maxlen, negative ? -value : value, prec, fwidth, flags & ~FLAGS_ADAPT_EXP);
  /* output the exponent part */
  if (minwidth) {
    (*out)((flags & FLAGS_UPPERCASE) ? 'E' : 'e', buffer, idx++, maxlen);
    idx = _ntoa_long(out, buffer, idx, maxlen, (expval < 0) ? -expval : expval, expval < 0, 10, 0, minwidth-1, FLAGS_ZEROPAD | FLAGS_PLUS);
    if (flags & FLAGS_LEFT) { /* right-pad spaces */ 
      while (idx - start_idx < width) (*out)(' ', buffer, idx++, maxlen);
    }
  }

  return idx;
}

/* internal vsnprintf */
static int _vsnprintf(out_fct_t out, char *buffer, const size_t maxlen, const char *format, va_list va)
{
  unsigned int flags, width, precision, n;
  size_t idx = 0U;
  if (!buffer) out = &_out_null; /* use null output function */

  while (*format) {
    /* format specifier?  %[flags][width][.precision][length] */
    if (*format != '%') { /* no */
      (*out)(*format, buffer, idx++, maxlen);
      ++format;
      continue;
    } else { /* yes */
      ++format;
    }

    flags = 0U;
    do {
      switch (*format) {
        case '0': flags |= FLAGS_ZEROPAD; format++; n = 1U; break;
        case '-': flags |= FLAGS_LEFT;    format++; n = 1U; break;
        case '+': flags |= FLAGS_PLUS;    format++; n = 1U; break;
        case ' ': flags |= FLAGS_SPACE;   format++; n = 1U; break;
        case '#': flags |= FLAGS_HASH;    format++; n = 1U; break;
        default :                                   n = 0U; break;
      }
    } while (n);

    width = 0U;
    if (_is_digit(*format)) {
      width = _atoi(&format);
    } else if (*format == '*') {
      const int w = va_arg(va, int);
      if (w < 0) {
        flags |= FLAGS_LEFT; /* reverse padding */
        width = (unsigned int)-w;
      } else {
        width = (unsigned int)w;
      }
      format++;
    }

    precision = 0U;
    if (*format == '.') {
      flags |= FLAGS_PRECISION;
      format++;
      if (_is_digit(*format)) {
        precision = _atoi(&format);
      } else if (*format == '*') {
        const int prec = (int)va_arg(va, int);
        precision = prec > 0 ? (unsigned int)prec : 0U;
        ++format;
      }
    }

    switch (*format) {
      case 'l':
        flags |= FLAGS_LONG; ++format;
        if (*format == 'l') { flags |= FLAGS_LONG_LONG; ++format; }
        break;
      case 'h':
        flags |= FLAGS_SHORT; ++format;
        if (*format == 'h') { flags |= FLAGS_CHAR; ++format; }
        break;
      case 't':
        flags |= (sizeof(ptrdiff_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        ++format;
        break;
      case 'j':
        flags |= (sizeof(intmax_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        format++;
        break;
      case 'z':
        flags |= (sizeof(size_t) == sizeof(long) ? FLAGS_LONG : FLAGS_LONG_LONG);
        format++;
        break;
      default :
        break;
    }

    switch (*format) {
      case 'd': case 'i': case 'u':  case 'x':
      case 'X': case 'o': case 'b': {
        unsigned int base;
        if (*format == 'x' || *format == 'X') base = 16U;
        else if (*format == 'o') base =  8U;
        else if (*format == 'b') base =  2U;
        else { base = 10U; flags &= ~FLAGS_HASH; }  /* no hash for dec format */
        if (*format == 'X') flags |= FLAGS_UPPERCASE;
        /* no plus or space flag for u, x, X, o, b */
        if ((*format != 'i') && (*format != 'd')) flags &= ~(FLAGS_PLUS | FLAGS_SPACE);
        /* ignore '0' flag when precision is given */
        if (flags & FLAGS_PRECISION) flags &= ~FLAGS_ZEROPAD;
        if (*format == 'i' || *format == 'd') { /* signed */
          if (flags & FLAGS_LONG_LONG) {
            const long long value = va_arg(va, long long);
            idx = _ntoa_long_long(out, buffer, idx, maxlen, (unsigned long long)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
          } else if (flags & FLAGS_LONG) {
            const long value = va_arg(va, long);
            idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned long)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
          } else {
            const int value = (flags & FLAGS_CHAR) ? (char)va_arg(va, int) : (flags & FLAGS_SHORT) ? (short int)va_arg(va, int) : va_arg(va, int);
            idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned int)(value > 0 ? value : 0 - value), value < 0, base, precision, width, flags);
          }
        } else { /* unsigned */
          if (flags & FLAGS_LONG_LONG) {
            idx = _ntoa_long_long(out, buffer, idx, maxlen, va_arg(va, unsigned long long), false, base, precision, width, flags);
          } else if (flags & FLAGS_LONG) {
            idx = _ntoa_long(out, buffer, idx, maxlen, va_arg(va, unsigned long), false, base, precision, width, flags);
          } else {
            const unsigned int value = (flags & FLAGS_CHAR) ? (unsigned char)va_arg(va, unsigned int) : (flags & FLAGS_SHORT) ? (unsigned short int)va_arg(va, unsigned int) : va_arg(va, unsigned int);
            idx = _ntoa_long(out, buffer, idx, maxlen, value, false, base, precision, width, flags);
          }
        }
        ++format;
      } break;
      case 'f': case 'F': {
        if (*format == 'F') flags |= FLAGS_UPPERCASE;
        idx = _ftoa(out, buffer, idx, maxlen, va_arg(va, double), precision, width, flags);
        format++;
      } break;
      case 'e': case 'E': case 'g': case 'G': {
        if (*format == 'g' || *format == 'G') flags |= FLAGS_ADAPT_EXP;
        if (*format == 'E' || *format == 'G') flags |= FLAGS_UPPERCASE;
        idx = _etoa(out, buffer, idx, maxlen, va_arg(va, double), precision, width, flags);
        ++format;
      } break;
      case 'c': {
        unsigned int l = 1U;
        if (!(flags & FLAGS_LEFT)) {
          while (l++ < width) (*out)(' ', buffer, idx++, maxlen);
        }
        (*out)((char)va_arg(va, int), buffer, idx++, maxlen);
        if (flags & FLAGS_LEFT) {
          while (l++ < width) (*out)(' ', buffer, idx++, maxlen);
        }
        ++format;
      } break;
      case 's': {
        const char* p = va_arg(va, char*);
        unsigned int l = _strnlen_s(p, precision ? precision : (size_t)-1);
        if (flags & FLAGS_PRECISION) l = (l < precision ? l : precision);
        if (!(flags & FLAGS_LEFT)) {
          while (l++ < width) (*out)(' ', buffer, idx++, maxlen);
        }
        while (*p != 0 && (!(flags & FLAGS_PRECISION) || precision--)) {
          (*out)(*(p++), buffer, idx++, maxlen);
        }
        if (flags & FLAGS_LEFT) {
          while (l++ < width) (*out)(' ', buffer, idx++, maxlen);
        }
        ++format;
      } break;
      case 'p': {
        width = sizeof(void*) * 2U;
        flags |= FLAGS_ZEROPAD | FLAGS_UPPERCASE;
        const bool is_ll = sizeof(uintptr_t) == sizeof(long long);
        if (is_ll) {
          idx = _ntoa_long_long(out, buffer, idx, maxlen, (uintptr_t)va_arg(va, void*), false, 16U, precision, width, flags);
        } else {
          idx = _ntoa_long(out, buffer, idx, maxlen, (unsigned long)((uintptr_t)va_arg(va, void*)), false, 16U, precision, width, flags);
        }
        ++format;
      } break;
      case '%': {
        (*out)('%', buffer, idx++, maxlen);
        format++;
      } break;
      default: {
        (*out)(*format, buffer, idx++, maxlen);
        format++;
      } break;
    }
  }

  (*out)((char)0, buffer, idx < maxlen ? idx : maxlen - 1U, maxlen); /* \0 */
  return (int)idx; /* # of written chars sans \0 */
}

int fprintf(FILE *stream, const char *format, ...)
{
  va_list va;
  va_start(va, format);
  const int ret = _vsnprintf(&_out_char, (char*)stream, (size_t)-1, format, va);
  va_end(va);
  return ret;
}

int printf(const char* format, ...)
{
  va_list va;
  va_start(va, format);
  const int ret = _vsnprintf(&_out_char, (char*)stdout, (size_t)-1, format, va);
  va_end(va);
  return ret;
}

int sprintf(char* buffer, const char* format, ...)
{
  va_list va;
  va_start(va, format);
  const int ret = _vsnprintf(&_out_buffer, buffer, (size_t)-1, format, va);
  va_end(va);
  return ret;
}

int snprintf(char* buffer, size_t count, const char* format, ...)
{
  va_list va;
  va_start(va, format);
  const int ret = _vsnprintf(&_out_buffer, buffer, count, format, va);
  va_end(va);
  return ret;
}

int vprintf(const char* format, va_list va)
{
  return _vsnprintf(&_out_char, (char*)stdout, (size_t)-1, format, va);
}

int vfprintf(FILE *stream, const char *format, va_list va)
{
  return _vsnprintf(&_out_char, (char*)stream, (size_t)-1, format, va);
}

int vsprintf(char* buffer, const char* format, va_list va)
{
  return _vsnprintf(&_out_buffer, buffer, (size_t)-1, format, va);
}

int vsnprintf(char* buffer, size_t count, const char* format, va_list va)
{
  return _vsnprintf(&_out_buffer, buffer, count, format, va);
}



/* misc filesystem functions */

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
