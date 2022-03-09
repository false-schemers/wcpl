#include <wasi/api.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
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
  return fp;
}

FILE *fopen(char *name, char *mode)
{
  return freopen(name, mode, NULL);
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
  if (fp->base == NULL) addbuf(fp);
  if (fp->ptr == fp->base && !(fp->flags & (_IONBF | _IOLBF)) )  {
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
      if (fp->ptr >= fp->end) break; /* flush buf add be done */
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
