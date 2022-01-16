/* l.c (wcpl library) -- esl */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <wchar.h>
#include "l.h"

/* globals */

int g_verbosity = 0;

/* common utility functions */

void exprintf(const char *fmt, ...)
{
  va_list args;

  fflush(stdout);
  //if (progname() != NULL)
  //  fprintf(stderr, "%s: ", progname());

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':')
    fprintf(stderr, " %s", strerror(errno));
  fprintf(stderr, "\naborting execution...\n");

  exit(2); /* conventional value for failed execution */
}

void *exmalloc(size_t n)
{
  void *p = malloc(n);
  if (p == NULL) exprintf("malloc() failed:");
  return p;
}

void *excalloc(size_t n, size_t s)
{
  void *p = calloc(n, s);
  if (p == NULL) exprintf("calloc() failed:");
  return p;
}

void *exrealloc(void *m, size_t n)
{
  void *p = realloc(m, n);
  if (p == NULL) exprintf("realloc() failed:");
  return p;
}

char *exstrdup(const char *s)
{
  char *t = (char *)exmalloc(strlen(s)+1);
  strcpy(t, s);
  return t;
}

char *strprf(const char *str, const char *prefix)
{
  assert(str); assert(prefix);
  while (*str && *str == *prefix) ++str, ++prefix;
  if (*prefix) return NULL;
  else return (char *)str; 
}

char *strsuf(const char *str, const char *suffix)
{
  size_t l, sl;
  assert(str); assert(suffix);
  l = strlen(str), sl = strlen(suffix);
  if (l >= sl) { 
    str += (l - sl);
    if (strcmp(str, suffix) == 0) return (char *)str;
  }
  return NULL;
}

bool streql(const char *s1, const char *s2)
{
  if (s1 == NULL && s2 == NULL) return true;
  if (s1 == NULL || s2 == NULL) return false;
  return strcmp(s1, s2) == 0;
}

bool strieql(const char *s1, const char *s2)
{
  if (s1 == NULL && s2 == NULL) return true;
  if (s1 == NULL || s2 == NULL) return false;
  while (*s1 != 0 && *s2 != 0) {
    int c1 = *s1++ & 0xff, c2 = *s2++ & 0xff;
    if (c1 == c2) continue;
    if (c1 < 0xff && c2 < 0xff && tolower(c1) == tolower(c2)) continue;
    break; 
  }
  return *s1 == 0 && *s2 == 0;
}

void memswap(void *mem1, void *mem2, size_t sz)
{
  char c, *p1, *p2;
  assert(mem1); assert(mem2);
  p1 = (char*)mem1; p2 = (char*)mem2;
  while (sz--) c = *p1, *p1++ = *p2, *p2++ = c;
}

/* encodes legal unicode as well as other code points up to 21 bits long (0 to U+001FFFFF) */
unsigned char *utf8(unsigned long c, unsigned char *s) 
{ /* if c has more than 21 bits (> U+1FFFFF), *s is set to 0 and s does not advance; accepts any other c w/o error */
  int t = (int)!!(c & ~0x7fL) + (int)!!(c & ~0x7ffL) + (int)!!(c & ~0xffffL) + (int)!!(c & ~0x1fffffL);
  *s = ((c >> t["\0\0\0\x12\0"]) & t["\0\0\0\x07\0"]) | t["\0\0\0\xf0\0"];
  s += t["\0\0\0\1\0"];
  *s = ((c >> t["\0\0\x0c\x0c\0"]) & t["\0\0\x0f\x3f\0"]) | t["\0\0\xe0\x80\0"];
  s += t["\0\0\1\1\0"];
  *s = ((c >> t["\0\x06\x06\x06\0"]) & t["\0\x1f\x3f\x3f\0"]) | t["\0\xc0\x80\x80\0"];
  s += t["\0\1\1\1\0"];
  *s = ((c & t["\x7f\x3f\x3f\x3f\0"])) | t["\0\x80\x80\x80\0"];
  s += t["\1\1\1\1\0"];
  return s; 
}

/* decodes legal utf-8 as well as any other encoded sequences for code points up to 21 bits (0 to U+001FFFFF) */
/* CAVEAT: in broken encoding can step over \0 byte if it is a (bad) payload byte and thus run past end of line!! */
unsigned long unutf8(unsigned char **ps) 
{ /* NB: unutf8 never advances less than 1 and more than 4 bytes; gobbles up arbitrary bytes w/o error */
  unsigned char *s = *ps; unsigned long c; unsigned t = *s >> 4; 
  c =  (unsigned long)(*s & t["\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f\0\0\0\0\x1f\x1f\x0f\x07"]) << t["\0\0\0\0\0\0\0\0\0\0\0\0\x06\x06\x0c\x12"];
  s += t["\1\1\1\1\1\1\1\1\0\0\0\0\1\1\1\1"];
  c |= (unsigned long)(*s & t["\0\0\0\0\0\0\0\0\0\0\0\0\x3f\x3f\x3f\x3f"]) << t["\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x06\x0c"];
  s += t["\0\0\0\0\0\0\0\0\0\0\0\0\1\1\1\1"];
  c |= (unsigned long)(*s & t["\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x3f\x3f"]) << t["\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x06"];
  s += t["\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1\1"];
  c |= (unsigned long)(*s & t["\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x3f"]);
  s += t["\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1"];
  *ps = s; return c;
}

unsigned long strtou8c(const char *s, char **ep)
{
  unsigned long c = unutf8((unsigned char **)&s);
  if (ep) *ep = (char *)s; return c;
}

/* size of buffer large enough to hold char escapes */
#define SBSIZE 32
/* convert a single C99-like char/escape sequence (-1 on error) */
unsigned long strtocc32(const char *s, char** ep)
{
  char buf[SBSIZE+1]; 
  int c;
  assert(s);
  if (!*s) goto err;
  c = *s++;
  if (c == '\\') {
    switch (c = *s++) {
      case 'a':   c = '\a'; break;
      case 'b':   c = '\b'; break;
      case 'f':   c = '\f'; break;
      case 'n':   c = '\n'; break;
      case 'r':   c = '\r'; break;
      case 't':   c = '\t'; break;
      case 'v':   c = '\v'; break;
      case '\\':  break;
      case '\'':  break;
      case '\"':  break;
      case '\?':  break;
      case 'x': {
        int i = 0; long l;
        while (i < SBSIZE && (c = *s++) && isxdigit(c))
          buf[i++] = c;
        if (i == SBSIZE) goto err;
        --s;
        buf[i] = 0; 
        l = strtol(buf, NULL, 16);
        c = (int)l & 0xff;
        if ((long)c != l) goto err; 
      } break;
      case 'u': {
        int i = 0; long l;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        buf[i] = 0;
        l = strtol(buf, NULL, 16);
        c = l & 0xffff;
        if ((long)c != l) goto err; 
      } break;
      case 'U': {
        int i = 0; long l;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        c = *s++; if (isxdigit(c)) buf[i++] = c; else goto err;
        buf[i] = 0;
        l = strtol(buf, NULL, 16);
        c = l & 0x7fffffff;
        if ((long)c != l) goto err; 
      } break;
      case '0': case '1': case '2': case '3':
      case '4': case '5': case '6': case '7': {
        int i = 0;
        buf[i++] = c; c = *s++;
        if (c >= '0' && c <= '7') {
          buf[i++] = c; c = *s++;
          if (c >= '0' && c <= '7')
            buf[i++] = c;
          else --s;
        } else --s;
        buf[i] = 0; 
        c = (int)strtol(buf, NULL, 8);
      } break;
      case 0:
        goto err;
      default:
        /* warn? return as is */
        break;
    }
  }
  if (ep) *ep = (char*)s; 
  return (unsigned long)c;
err:
  if (ep) *ep = (char*)s;
  errno = EINVAL;
  return (unsigned long)(-1);
}


bool fget8bom(FILE *fp)
{
  int c = fgetc(fp);
  if (c == EOF) return false;
  /* not comitted yet - just looking ahead... */
  if ((c & 0xFF) != 0xEF) { ungetc(c, fp); return false; }
  /* now we are comitted to read all 3 bytes - partial reads can happen */
  if ((c = fgetc(fp)) == EOF || (c & 0xFF) != 0xBB) return false;
  if ((c = fgetc(fp)) == EOF || (c & 0xFF) != 0xBF) return false;
  /* all three bytes are there */
  return true;
}

int int_cmp(const void *pi1, const void *pi2)
{
  assert(pi1); assert(pi2);
  return *(int*)pi1 - *(int*)pi2;
}


/* dynamic (heap-allocated) 0-terminated strings */

void dsicpy(dstr_t* mem, const dstr_t* pds)
{
  dstr_t ds = NULL;
  assert(mem); assert(pds);
  if (*pds) strcpy((ds = exmalloc(strlen(*pds)+1)), *pds);
  *mem = ds;
}

void (dsfini)(dstr_t* pds)
{
  if (pds) free(*pds);
}

void dscpy(dstr_t* pds, const dstr_t* pdss)
{ 
  dstr_t ds = NULL;
  assert(pds); assert(pdss);
  if (*pdss) strcpy((ds = exmalloc(strlen(*pdss)+1)), *pdss);
  free(*pds); *pds = ds;
}

void dssets(dstr_t* pds, const char *s)
{
  dstr_t ds = NULL;
  assert(pds);
  if (s) strcpy((ds = exmalloc(strlen(s)+1)), s);
  free(*pds); *pds = ds;
}

int dstr_cmp(const void *pds1, const void *pds2)
{
  assert(pds1); assert(pds2);
  return strcmp(*(dstr_t*)pds1, *(dstr_t*)pds2);
}


/* simple dynamic memory buffers */

buf_t* bufinit(buf_t* pb, size_t esz)
{
  assert(pb); assert(esz);
  pb->esz = esz;
  pb->end = 10; /* buf is never NULL */
  pb->buf = excalloc(pb->end, pb->esz);
  pb->fill = 0;
  return pb;
}

buf_t* buficpy(buf_t* mem, const buf_t* pb) /* over non-inited */
{
  assert(mem); assert(pb);
  assert(mem != pb);
  mem->esz = pb->esz;
  mem->end = pb->end; 
  mem->fill = pb->fill;
  mem->buf = excalloc(pb->end, pb->esz);
  memcpy(mem->buf, pb->buf, pb->esz*pb->fill);
  return mem;
}

void *buffini(buf_t* pb)
{
  assert(pb);
  free(pb->buf);
#ifndef NDEBUG
  pb->buf = NULL;
#endif
  return pb;
}

buf_t mkbuf(size_t esz)
{
  buf_t b;
  assert(esz);
  bufinit(&b, esz);
  return b;
}

buf_t* newbuf(size_t esz)
{
  buf_t* pb = (buf_t*)exmalloc(sizeof(buf_t));
  bufinit(pb, esz);
  return pb;
}

void freebuf(buf_t* pb)
{
  if (pb) {
    buffini(pb);
    free(pb);
  }
}

size_t buflen(const buf_t* pb)
{
  assert(pb);
  return pb->fill;
}

void* bufdata(buf_t* pb)
{
  assert(pb);
  return pb->buf;
}

bool bufempty(const buf_t* pb)
{
  assert(pb);
  return pb->fill == 0;
}

void bufclear(buf_t* pb)
{
  assert(pb);
  pb->fill = 0;
}

void bufgrow(buf_t* pb, size_t n)
{
  assert(pb);
  { 
    size_t oldsz = pb->end;
    size_t newsz = oldsz*2;
    if (oldsz + n > newsz) newsz += n;
    pb->buf = exrealloc(pb->buf, newsz*pb->esz);
    pb->end = newsz;
  }
}

void bufresize(buf_t* pb, size_t n)
{
  assert(pb);
  if (n > pb->end) bufgrow(pb, n - pb->end);
  if (n > pb->fill) memset((char*)pb->buf + pb->fill*pb->esz, 0, (n - pb->fill)*pb->esz);
  pb->fill = n;
}

void* bufref(buf_t* pb, size_t i)
{
  assert(pb);
  assert(i < pb->fill);
  return (char*)pb->buf + i*pb->esz;
}

void bufrem(buf_t* pb, size_t i)
{
  size_t esz;
  assert(pb); assert(i < pb->fill);
  esz = pb->esz;
  if (i < pb->fill - 1)
    memmove((char*)pb->buf + i*esz,
            (char*)pb->buf + (i+1)*esz,
            (pb->fill - 1 - i)*esz);
  --pb->fill;
}

void bufnrem(buf_t* pb, size_t i, size_t cnt)
{
  if (cnt > 0) {
    size_t esz;
    assert(pb); assert(i + cnt <= pb->fill);
    esz = pb->esz;
    if (i < pb->fill - cnt)
      memmove((char*)pb->buf + i*esz,
              (char*)pb->buf + (i+cnt)*esz,
              (pb->fill - cnt - i)*esz);
    pb->fill -= cnt;
  }
}

void* bufins(buf_t* pb, size_t i)
{
  size_t esz; char *pnewe;
  assert(pb); assert(i <= pb->fill);
  esz = pb->esz;
  if (pb->fill == pb->end) bufgrow(pb, 1);
  pnewe = (char*)pb->buf + i*esz;
  if (i < pb->fill)
    memmove(pnewe + esz, pnewe,
            (pb->fill - i)*esz);
  memset(pnewe, 0, esz);
  ++pb->fill;
  return pnewe;
}

void *bufbk(buf_t* pb)
{
  char* pbk;
  assert(pb); assert(pb->fill > 0);
  pbk = (char*)pb->buf + (pb->fill-1)*pb->esz;
  return pbk;
}

void *bufnewbk(buf_t* pb)
{
  char* pbk; size_t esz;
  assert(pb);
  if (pb->fill == pb->end) bufgrow(pb, 1);
  esz = pb->esz;
  pbk = (char*)pb->buf + pb->fill*esz;
  memset(pbk, 0, esz);
  ++pb->fill;
  return pbk;
}

void *bufpopbk(buf_t* pb)
{
  char* pbk;
  assert(pb); assert(pb->fill > 0);
  pbk = (char*)pb->buf + (pb->fill-1)*pb->esz;
  pb->fill -= 1;
  return pbk; /* outside but untouched */
}

void *bufnewfr(buf_t* pb)
{
  return bufins(pb, 0);
}

void *bufalloc(buf_t* pb, size_t n)
{
  size_t esz; char* pbk;
  assert(pb);
  if (pb->fill + n > pb->end) bufgrow(pb, pb->fill + n - pb->end);
  esz = pb->esz;
  pbk = (char*)pb->buf + pb->fill*esz;
  memset(pbk, 0, esz*n);
  pb->fill += n;
  return pbk;
}

/* unstable sort */
void bufqsort(buf_t* pb, int (*cmp)(const void *, const void *))
{
  assert(pb); assert(cmp);
  qsort(pb->buf, pb->fill, pb->esz, cmp);
}

/* removes adjacent */
void bufremdups(buf_t* pb, int (*cmp)(const void *, const void *), void (*fini)(void *))
{
  char *pdata; size_t i, j, len, esz;
  assert(pb); assert(cmp);
  len = pb->fill;
  esz = pb->esz;
  if (len < 2) return;
  i = 0; j = 1;  
  pdata = (char*)pb->buf;
  while (j < len) {
    /* invariant: elements [0..i] are unique, [i+1,j[ is a gap */
    assert(i < j);
    if (0 == (cmp)(pdata+i*esz, pdata+j*esz)) {
      /* duplicate: drop it */
      if (fini) (fini)(pdata+j*esz);
    } else {
      /* unique: move it */
      ++i; if (i < j) memcpy(pdata+i*esz, pdata+j*esz, esz);
    }  
    ++j;
  }
  pb->fill = i+1;
}

/* linear search */
void* bufsearch(const buf_t* pb, const void *pe, int (*cmp)(const void *, const void *))
{
  size_t esz; char *pdata, *pend; 
  assert(pb); assert(pe); assert(cmp);
  esz = pb->esz;
  pdata = (char*)pb->buf;
  pend = pdata + esz*pb->fill;
  while (pdata < pend) {
    if (0 == (cmp)(pdata, pe))
      return pdata;
    pdata += esz;
  }
  return NULL;
}

/* binary search */
void* bufbsearch(const buf_t* pb, const void *pe, int (*cmp)(const void *, const void *))
{
  assert(pb); assert(pe); assert(cmp);
  return bsearch(pe, pb->buf, pb->fill, pb->esz, cmp);
}

size_t bufoff(const buf_t* pb, const void *pe)
{
  char *p0, *p; size_t off;
  assert(pb); assert(pe);
  p0 = (char*)pb->buf, p = (char*)pe;
  assert(p >= p0);
  off = (p - p0) / pb->esz;
  assert(off <= pb->fill); /* internal pointers and pointer past last elt are legal */
  return off;
}


/* unicode charsets */

bool ucsin(unsigned c, const ucset_t *ps)
{
  unsigned e = c;
  const unsigned *pbase = ps->buf; /* int partition base */
  size_t psize = ps->fill; /* partition size in int pairs */
  while (psize) { /* still have int pairs in partition */
    /* pick central pair, rounding offset to a pair */
    const unsigned *ci = pbase + (psize & ~1);
    if (e < ci[0]) { /* FIRST */
      /* search from pbase to pair before ci */
      psize >>= 1;
    } else if (e > ci[1]) { /* LAST */ 
      /* search from pair after ci */
      pbase = ci + 2;
      psize = (psize-1) >> 1;
    } else {
      return true;
    }
  }
  return false;
}

void ucspushi(ucset_t *ps, unsigned fc, unsigned lc)
{
  unsigned *nl = bufnewbk(ps);
  /* ps spacing/ordering preconds must hold! */
  assert(fc <= lc);
  assert(nl == bufdata(ps) || fc > nl[-1]);
  nl[0] = fc, nl[1] = lc; 
}



/* dstr_t buffers */

void dsbicpy(dsbuf_t* mem, const dsbuf_t* pb)
{
  dstr_t *pdsm, *pds; size_t i;
  assert(mem); assert(pb); assert(pb->esz == sizeof(dstr_t));
  buficpy(mem, pb);
  pdsm = (dstr_t*)(mem->buf), pds = (dstr_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) dsicpy(pdsm+i, pds+i);
}

void dsbfini(dsbuf_t* pb)
{
  dstr_t *pds; size_t i;
  assert(pb); assert(pb->esz = sizeof(dstr_t));
  pds = (dstr_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) dsfini(pds+i);
  buffini(pb);
}


/* char buffers */

void chbput(const char *s, size_t n, chbuf_t* pb)
{
  size_t l = buflen(pb);
  bufresize(pb, l+n);
  memcpy(l+(char*)pb->buf, s, n);
}

void chbputs(const char *s, chbuf_t* pb)
{
  size_t n = strlen(s);
  chbput(s, n, pb);
}

void chbputlc(unsigned long uc, chbuf_t* pb)
{
  if (uc < 128) chbputc((unsigned char)uc, pb);
  else {
    char buf[5], *pc = (char *)utf8(uc, (unsigned char *)buf);
    chbput(buf, pc-buf, pb);
  }
}

void chbputwc(wchar_t wc, chbuf_t* pb)
{
  assert(WCHAR_MIN <= wc && wc <= WCHAR_MAX);
  if (wc < 128) chbputc((unsigned char)wc, pb);
  else {
    char buf[5], *pc = (char *)utf8(wc, (unsigned char *)buf);
    chbput(buf, pc-buf, pb);
  }
}

void chbputd(int val, chbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits (w/sign) */
  char *e = buf + sizeof(buf), *p = e;
  if (val) {
    unsigned m; 
    if (val == INT_MIN) m = INT_MAX + (unsigned)1;
    else if (val < 0) m = -val;
    else m = val;
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
    if (val < 0) *--p = '-';
  } else *--p = '0';
  chbput(p, e-p, pb);
}

void chbputld(long val, chbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits (w/sign) */
  char *e = buf + sizeof(buf), *p = e;
  if (val) {
    unsigned long m; 
    if (val == LONG_MIN) m = LONG_MAX + (unsigned long)1;
    else if (val < 0) m = -val;
    else m = val;
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
    if (val < 0) *--p = '-';
  } else *--p = '0';
  chbput(p, e-p, pb);
}

void chbputt(ptrdiff_t val, chbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits (w/sign) */
  char *e = buf + sizeof(buf), *p = e;
  if (val) {
    size_t m; assert(sizeof(ptrdiff_t) == sizeof(size_t));
    if (val < 0 && val-1 > 0) m = (val-1) + (size_t)1;
    else if (val < 0) m = -val;
    else m = val;
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
    if (val < 0) *--p = '-';
  } else *--p = '0';
  chbput(p, e-p, pb);
}

void chbputu(unsigned val, chbuf_t* pb)
{
  char buf[39]; /* enough up to 128 bits */
  char *e = buf + sizeof(buf), *p = e;
  if (val) {
    unsigned m = val; 
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
  } else *--p = '0';
  chbput(p, e-p, pb);
}

void chbputlu(unsigned long val, chbuf_t* pb)
{
  char buf[39]; /* enough up to 128 bits */
  char *e = buf + sizeof(buf), *p = e;
  if (val) {
    unsigned long m = val; 
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
  } else *--p = '0';
  chbput(p, e-p, pb);
}

void chbputllu(unsigned long long val, chbuf_t* pb)
{
  char buf[39]; /* enough up to 128 bits */
  char *e = buf + sizeof(buf), *p = e;
  if (val) {
    unsigned long long m = val; 
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
  } else *--p = '0';
  chbput(p, e-p, pb);
}

void chbputz(size_t val, chbuf_t* pb)
{
  char buf[39]; /* enough up to 128 bits */
  char *e = buf + sizeof(buf), *p = e;
  if (val) {
    size_t m = val; 
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
  } else *--p = '0';
  chbput(p, e-p, pb);
}

void chbputg(double v, chbuf_t* pb)
{
  char buf[100];
  sprintf(buf, "%.17g", v); /* see WCSSKAFPA paper */
  chbputs(buf, pb);
}

/* minimalistic printf to char bufer */
void chbputvf(chbuf_t* pb, const char *fmt, va_list ap)
{
  assert(pb); assert(fmt);
  while (*fmt) {
    if (*fmt != '%' || *++fmt == '%') {
      chbputc(*fmt++, pb);
    } else if (fmt[0] == 's') {
      char *s = va_arg(ap, char*);
      chbputs(s, pb); fmt += 1;
    } else if (fmt[0] == 'c') {
      int c = va_arg(ap, int);
      chbputc(c, pb); fmt += 1;
    } else if (fmt[0] == 'l' && fmt[1] == 'c') {
      unsigned c = va_arg(ap, unsigned);
      chbputlc(c, pb); fmt += 2;
    } else if (fmt[0] == 'w' && fmt[1] == 'c') {
      wchar_t c = va_arg(ap, wchar_t);
      chbputwc(c, pb); fmt += 2;
    } else if (fmt[0] == 'd') {
      int d = va_arg(ap, int);
      chbputd(d, pb); fmt += 1;
    } else if (fmt[0] == 'l' && fmt[1] == 'd') {
      long ld = va_arg(ap, long);
      chbputld(ld, pb); fmt += 2;
    } else if (fmt[0] == 't') {
      ptrdiff_t t = va_arg(ap, ptrdiff_t);
      chbputt(t, pb); fmt += 1;
    } else if (fmt[0] == 'u') {
      unsigned u = va_arg(ap, unsigned);
      chbputu(u, pb); fmt += 1;
    } else if (fmt[0] == 'l' && fmt[1] == 'u') {
      unsigned long lu = va_arg(ap, unsigned long);
      chbputlu(lu, pb); fmt += 2;
    } else if (fmt[0] == 'z') {
      size_t z = va_arg(ap, size_t);
      chbputz(z, pb); fmt += 1;
    } else if (fmt[0] == 'g') {
      double g = va_arg(ap, double);
      chbputg(g, pb); fmt += 1;
    } else {
      assert(0 && "unsupported chbputvf format directive");
      break;
    } 
  }
}

void chbputf(chbuf_t* pb, const char *fmt, ...)
{
  va_list args;
  assert(pb); assert(fmt);
  va_start(args, fmt);
  chbputvf(pb, fmt, args);
  va_end(args);
}

char* chbset(chbuf_t* pb, const char *s, size_t n)
{
  bufclear(pb);
  chbput(s, n, pb);
  return chbdata(pb);
}

char* chbsets(chbuf_t* pb, const char *s)
{
  bufclear(pb);
  chbputs(s, pb);
  return chbdata(pb);
}

char *chbsetf(chbuf_t* pb, const char *fmt, ...)
{
  va_list args;
  assert(pb); assert(fmt);
  bufclear(pb);
  va_start(args, fmt);
  chbputvf(pb, fmt, args);
  va_end(args);
  return chbdata(pb);
}

char* chbdata(chbuf_t* pb)
{
  *(char*)bufnewbk(pb) = 0;
  pb->fill -= 1;
  return pb->buf; 
}

dstr_t chbclose(chbuf_t* pb)
{
  dstr_t s;
  *(char*)bufnewbk(pb) = 0;
  s = pb->buf; pb->buf = NULL; /* pb is finalized */
  return s; 
}

char *fgetlb(chbuf_t *pcb, FILE *fp)
{
  char buf[256], *line; size_t len;
  assert(fp); assert(pcb);
  chbclear(pcb);
  line = fgets(buf, sizeof(buf), fp);
  if (!line) return NULL;
  len = strlen(line);
  if (len > 0 && line[len-1] == '\n') {
    line[len-1] = 0;
    if (len > 1 && line[len-2] == '\r') line[len-2] = 0;
    return chbsets(pcb, line);
  } else for (;;) {
    if (len > 0 && line[len-1] == '\r') line[len-1] = 0;
    chbputs(line, pcb);
    line = fgets(buf, sizeof(buf), fp);
    if (!line) break;
    len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
      line[len-1] = 0;
      if (len > 1 && line[len-2] == '\r') line[len-2] = 0;
      chbputs(line, pcb);
      break;
    }
  } 
  return chbdata(pcb);
}

/* convert wchar_t string to utf-8 string
 * if rc = 0, return NULL on errors, else subst rc */
char *wcsto8cb(const wchar_t *wstr, int rc, chbuf_t *pcb)
{
  bool convok = true;
  chbclear(pcb);
  while (*wstr) {
    unsigned c = (unsigned)*wstr++;
    if (c > 0x1FFFFF) {
      if (rc) chbputc(rc, pcb);
      else { convok = false; break; }
    }
    chbputlc(c, pcb);
  }
  if (!convok) return NULL;
  return chbdata(pcb);
}


/* wide char buffers */

/* convert utf-8 string to wide chars in wchar_t buffer */
wchar_t *s8ctowcb(const char *str, wchar_t rc, buf_t *pb)
{
  char *s8c = (char *)str;
  bool convok = true;
  wchar_t wc;
  assert(s8c); assert(pb); 
  assert(pb->esz == sizeof(wchar_t));
  bufclear(pb);
  while (*s8c) {
    unsigned long c = unutf8((unsigned char **)&s8c);
    if (c == (unsigned long)-1 || c < WCHAR_MIN || c > WCHAR_MAX) {
      if (rc) wc = rc;
      else { convok = false; break; }
    } else {
      wc = (wchar_t)c;
    }
    *(wchar_t*)bufnewbk(pb) = wc; 
  }
  *(wchar_t*)bufnewbk(pb) = 0; 
  if (!convok) return NULL;
  return (wchar_t *)bufdata(pb);
}

/* unicode char (unsigned long) buffers */

/* convert utf-8 string to unicode chars in unsigned long buffer */
unsigned long *s8ctoucb(const char *str, unsigned long rc, buf_t *pb)
{
  char *s8c = (char *)str;
  bool convok = true;
  unsigned long uc;
  assert(s8c); assert(pb); 
  assert(pb->esz == sizeof(unsigned long));
  bufclear(pb);
  while (*s8c) {
    unsigned long c = unutf8((unsigned char **)&s8c);
    if (c == (unsigned)-1) {
      if (rc) uc = rc;
      else { convok = false; break; }
    } else {
      uc = c;
    }
    *(unsigned long*)bufnewbk(pb) = uc; 
  }
  *(unsigned long*)bufnewbk(pb) = 0; 
  if (!convok) return NULL;
  return (unsigned long *)bufdata(pb);
}


/* symbols */

static struct { char **a; char ***v; size_t sz; size_t u; size_t maxu; } g_symt;

static unsigned long hashs(const char *s) {
  unsigned long i = 0, l = (unsigned long)strlen(s), h = l;
  while (i < l) h = (h << 4) ^ (h >> 28) ^ s[i++];
  return h ^ (h  >> 10) ^ (h >> 20);
}

const char *symname(sym_t s) 
{
  int sym = (int)s; const char *name = NULL;
  assert(sym >= 0); assert(sym-1 < (int)g_symt.u);
  if (sym > 0) { name = g_symt.a[sym-1]; assert(name); }
  return name;
}

sym_t intern(const char *name) 
{
  size_t i, j; int sym;
  if (name == NULL) return (sym_t)0;
  if (g_symt.sz == 0) { /* init */
    g_symt.a = excalloc(64, sizeof(char*));
    g_symt.v = excalloc(64, sizeof(char**));
    g_symt.sz = 64, g_symt.maxu = 64 / 2;
    i = hashs(name) & (g_symt.sz-1);
  } else {
    unsigned long h = hashs(name);
    for (i = h & (g_symt.sz-1); g_symt.v[i]; i = (i-1) & (g_symt.sz-1))
      if (strcmp(name, *g_symt.v[i]) == 0) return (sym_t)(g_symt.v[i]-g_symt.a+1);
    if (g_symt.u == g_symt.maxu) { /* rehash */
      size_t nsz = g_symt.sz * 2;
      char **na = excalloc(nsz, sizeof(char*));
      char ***nv = excalloc(nsz, sizeof(char**));
      for (i = 0; i < g_symt.sz; i++)
        if (g_symt.v[i]) {
          for (j = hashs(*g_symt.v[i]) & (nsz-1); nv[j]; j = (j-1) & (nsz-1)) ;
          nv[j] = g_symt.v[i] - g_symt.a + na;
        }
      free(g_symt.v); g_symt.v = nv; g_symt.sz = nsz; g_symt.maxu = nsz / 2;
      memcpy(na, g_symt.a, g_symt.u * sizeof(char*)); free(g_symt.a); g_symt.a = na; 
      for (i = h & (g_symt.sz-1); g_symt.v[i]; i = (i-1) & (g_symt.sz-1)) ;
    }
  }
  *(g_symt.v[i] = g_symt.a + g_symt.u) = 
    strcpy(exmalloc(strlen(name)+1), name);
  sym = (int)((g_symt.u)++);
  return (sym_t)(sym+1);
}

sym_t internf(const char *fmt, ...)
{
  va_list args; sym_t s; chbuf_t cb; 
  assert(fmt); chbinit(&cb);
  va_start(args, fmt);
  chbputvf(&cb, fmt, args);
  va_end(args);
  s = intern(chbdata(&cb));
  chbfini(&cb);
  return s;
}

void clearsyms(void)
{
  if (g_symt.sz != 0) {
    size_t i;
    for (i = 0; i < g_symt.sz; ++i) {
      char **p = g_symt.v[i];
      if (p) free(*p);
    }
    free(g_symt.v); g_symt.v = NULL;
    free(g_symt.a); g_symt.a = NULL;
    g_symt.sz = g_symt.u = g_symt.maxu = 0;
  }
}