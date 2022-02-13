#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
//#include <wasi.h>

double atof(const char *s)
{
	return strtod(s, 0);
}

int atoi(const char *s)
{
  int n = 0, neg = 0;
  while (isspace(*s)) s++;
  switch (*s) {
    case '-': neg = 1; 
    /* fall thru */
    case '+': s++;
  }
  /* Compute n as a negative number to avoid overflow on INT_MIN */
  while (isdigit(*s)) n = 10*n - (*s++ - '0');
  return neg ? n : -n;
}

long atol(const char *s)
{
  long n = 0; int neg = 0;
  while (isspace(*s)) s++;
  switch (*s) {
    case '-': neg = 1; 
    /* fall thru */
    case '+': s++;
  }
  /* Compute n as a negative number to avoid overflow on LONG_MIN */
  while (isdigit(*s)) n = 10*n - (*s++ - '0');
  return neg ? n : -n;
}

long long atoll(const char *s)
{
  long long n = 0; int neg = 0;
  while (isspace(*s)) s++;
  switch (*s) {
    case '-': neg = 1; 
    /* fall thru */
    case '+': s++;
  }
  /* Compute n as a negative number to avoid overflow on LLONG_MIN */
  while (isdigit(*s)) n = 10*n - (*s++ - '0');
  return neg ? n : -n;
}

/* mattn/strtod (Yasuhiro Matsumoto); simplistic */
double strtod(const char *str, char **end)
{
  double d = 0.0; int sign, n = 0;
  const char *p = str, *a = str;
  while (isspace(*p)) p++;
  /* decimal part */
  sign = 1;
  if (*p == '-') {
    sign = -1; ++p;
  } else if (*p == '+') {
    ++p;
  }
  if (isdigit(*p)) {
    d = (double)(*p++ - '0');
    while (*p && vim_isdigit(*p)) {
      d = d * 10.0 + (double)(*p - '0');
      ++p; ++n;
    }
    a = p;
  } else if (*p != '.') {
    goto done;
  }
  d *= sign;
  /* fraction part */
  if (*p == '.') {
    double f = 0.0, base = 0.1;
    ++p;
    if (isdigit(*p)) {
      while (*p && vim_isdigit(*p)) {
        f += base * (*p - '0');
        base /= 10.0;
        ++p;
        ++n;
      }
    }
    d += f * sign;
    a = p;
  }
  /* exponential part */
  if ((*p == 'E') || (*p == 'e')) {
    int e = 0;
    ++p; sign = 1;
    if (*p == '-') {
      sign = -1; ++p;
    } else if (*p == '+') {
      ++p;
    }
    if (isdigit(*p)) {
      while (*p == '0') ++p;
      if (*p == '\0') --p;
      e = (int)(*p++ - '0');
      while (*p && isdigit(*p)) {
        e = e * 10 + (int)(*p - '0');
        ++p;
      }
      e *= sign;
    } else if (!isdigit(*(a - 1))) {
      a = str;
      goto done;
    } else if (*p == 0) {
      goto done;
    }
    if (d == 2.2250738585072011 && e == -308) {
      d = 0.0; a = p; errno = ERANGE;
      goto done;
    }
    if (d == 2.2250738585072012 && e <= -308) {
      d *= 1.0e-308; a = p;
      goto done;
    }
    d *= pow(10.0, (double)e); a = p;
  } else if (p > str && !isdigit(*(p - 1))) {
    a = str;
    goto done;
  }
done:
  if (end) *end = (char *)a;
  return d;
}

static uintmax_t strntoumax(const char *nptr, char **endptr, int base, size_t n)
{
  int minus = 0; uintmax_t v = 0; int d;
  while (n && isspace((unsigned char)*nptr)) {  nptr++;  n--; }
  if (n) {
    char c = *nptr;
    if (c == '-' || c == '+') {
      minus = (c == '-');
      nptr++;
      n--;
    }
  }
  if (base == 0) {
    if (n >= 2 && nptr[0] == '0' &&
        (nptr[1] == 'x' || nptr[1] == 'X')) {
      n -= 2; nptr += 2; base = 16;
    } else if (n >= 1 && nptr[0] == '0') {
      n--; nptr++; base = 8;
    } else {
      base = 10;
    }
  } else if (base == 16) {
    if (n >= 2 && nptr[0] == '0' &&
        (nptr[1] == 'x' || nptr[1] == 'X')) {
      n -= 2; nptr += 2;
    }
  }
  while (n && (d = digitval(*nptr)) >= 0 && d < base) {
    v = v * base + d;
    n--; nptr++;
  }
  if (endptr) *endptr = (char *)nptr;
  return minus ? -v : v;
}

intmax_t strntoimax(const char *nptr, char **endptr, int base, size_t n)
{
  return (intmax_t)strntoumax(nptr, endptr, base, n);
}

intmax_t strtoimax(const char *nptr, char **endptr, int base)
{
	return (intmax_t)strntoumax(nptr, endptr, base, ~(size_t)0);
}

long strtol(const char *nptr, char **endptr, int base)
{
	return (long)strntoumax(nptr, endptr, base, ~(size_t)0);
}

long long strtoll(const char *nptr, char **endptr, int base)
{
	return (long long)strntoumax(nptr, endptr, base, ~(size_t)0);
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
	return (unsigned long)strntoumax(nptr, endptr, base, ~(size_t)0);
}

unsigned long long strtoull(const char *nptr, char **endptr, int base)
{
	return (unsigned long long) strntoumax(nptr, endptr, base, ~(size_t)0);
}

uintmax_t strtoumax(const char *nptr, char **endptr, int base)
{
	return (uintmax_t)strntoumax(nptr, endptr, base, ~(size_t)0);
}

static unsigned short rand48_seed[3]; 
static long jrand48(unsigned short xsubi[3])
{
  uint64_t x;
  /* The xsubi[] array is littleendian by spec */
  x = (uint64_t)(uint16_t)xsubi[0] +
      ((uint64_t)(uint16_t)xsubi[1] << 16) +
      ((uint64_t)(uint16_t)xsubi[2] << 32);
  x = (0x5DEECE66DULL * x) + 0xb;
  xsubi[0] = (unsigned short)(uint16_t)x;
  xsubi[1] = (unsigned short)(uint16_t)(x >> 16);
  xsubi[2] = (unsigned short)(uint16_t)(x >> 32);
  return (long)(int32_t)(x >> 16);
}

int rand(void)
{
  return (int)jrand48(rand48_seed) >> 1;
}

void srand(unsigned seedval)
{
  rand48_seed[0] = 0x330e;
  rand48_seed[1] = (unsigned short)seedval;
  rand48_seed[2] = (unsigned short)((uint32_t)seedval >> 16);
}


/* malloc & friends is missing */

/* system */

void abort(void)
{
  proc_exit((exitcode_t)EXIT_FAILURE);
}

int atexit(void (*func)(void))
{
  /* fake */
  return -1;
}

void exit(int status)
{
  proc_exit((exitcode_t)status);
}

char *getenv(const char *name)
{
  /* fake; TODO: use WASI environ_get */
}

/* system() is absent */

void *bsearch(const void *key, const void *base, size_t nmemb,
  size_t size, int (*cmp)(const void *, const void *))
{
  while (nmemb) {
    size_t mididx = nmemb / 2;
    const void *midobj = (const char *)base + mididx * size;
    int diff = cmp(key, midobj);
    if (diff == 0) return (void *)midobj;
    if (diff > 0) {
      base = (const char *)midobj + size;
      nmemb -= mididx + 1;
    } else
      nmemb = mididx;
  }
  return NULL;
}

/* qsort -- This is actually combsort.  It's an O(n log n) algorithm with
 * simplicity/small code size being its main virtue. */
static size_t newgap(size_t gap)
{
  gap = (gap * 10) / 13;
  if (gap == 9 || gap == 10) gap = 11;
  if (gap < 1) gap = 1;
  return gap;
}

void qsort(void *base, size_t nmemb, size_t size,
  int (*compar)(const void *, const void *))
{ /* combsort, O(n log n) */
  size_t gap = nmemb, i, j;
  char *p1, *p2; int swapped;
  if (!nmemb) return;
  do {
    gap = newgap(gap);
    swapped = 0;
    for (i = 0, p1 = base; i < nmemb - gap; i++, p1 += size) {
      j = i + gap;
      if (compar(p1, p2 = (char *)base + j * size) > 0) {
        memswap(p1, p2, size);
        swapped = 1;
      }
    }
  } while (gap > 1 || swapped);
}

int abs(int n)
{
	return (n < 0) ? -n : n;
}

long labs(long n)
{
	return (n < 0L) ? -n : n;
}

long long llabs(long long n)
{
	return (n < 0LL) ? -n : n;
}

div_t div(int num, int den)
{
  return (div_t){ num/den, num%den };
}

ldiv_t ldiv(long num, long den)
{
  return (ldiv_t){ num/den, num%den };
}

lldiv_t lldiv(long long num, long long den)
{
  return (lldiv_t){ num/den, num%den };
}

int mblen(const char *s, size_t n)
{
  return mbtowc(0, s, n);
}


/* see http://www.stonehand.com/unicode/standard/fss-utf.html */
struct utf8_table {
  int cmask;
  int cval;
  int shift;
  long lmask;
  long lval;
};

static struct utf8_table utf8_table[] = {
  {0x80, 0x00, 0 * 6, 0x7F, 0, /* 1 byte sequence */},
  {0xE0, 0xC0, 1 * 6, 0x7FF, 0x80, /* 2 byte sequence */},
  {0xF0, 0xE0, 2 * 6, 0xFFFF, 0x800, /* 3 byte sequence */},
  {0xF8, 0xF0, 3 * 6, 0x1FFFFF, 0x10000, /* 4 byte sequence */},
  {0xFC, 0xF8, 4 * 6, 0x3FFFFFF, 0x200000, /* 5 byte sequence */},
  {0xFE, 0xFC, 5 * 6, 0x7FFFFFFF, 0x4000000, /* 6 byte sequence */},
  {0, /* end of table    */}
};

int mbtowc(wchar_t *p, const char *s, int n) 
{
  const unsigned char *ip = (const unsigned char *)s;
  int c0, c, nc; long l;
  struct utf8_table *t;
  c0 = *ip; l = c0; nc = 0;
  if (!s) return 0;
  if (!n) goto ilseq;
  for (t = utf8_table; t->cmask; t++) {
    nc++;
    if ((c0 & t->cmask) == t->cval) {
      l &= t->lmask;
      if (l < t->lval) goto ilseq;
      *p = (wchar_t)l;
      return nc;
    }
    if (n <= nc) goto ilseq;
    ip++;
    c = (*ip ^ 0x80) & 0xFF;
    if (c & 0xC0) goto ilseq; 
    l = (l << 6) | c;
  }
ilseq:
  /* TODO: errno = EILSEQ; */  
  return -1;
}

static int wctomb_chk(unsigned char *s, wchar_t wc, int maxlen) 
{
  long l = wc; int c, nc = 0;
  struct utf8_table *t;
  if (s == 0) return 0;
  for (t = utf8_table; t->cmask && maxlen; t++, maxlen--) {
    nc++;
    if (l <= t->lmask) {
      c = t->shift;
      *s = (unsigned char)(t->cval | (l >> c));
      while (c > 0) {
        c -= 6;
        s++;
        *s = 0x80 | (unsigned char)((l >> c) & 0x3F);
      }
      return nc;
    }
  }
  return -1;
}

int wctomb(char *s, wchar_t wc)
{
  /* wctomb requires at most MB_CUR_MAX available chars in s */
  return wctomb_chk((unsigned char *)s, wc, MB_CUR_MAX);
}

int mbstowcs(wchar_t *pwcs, const char *s, int n) 
{
  unsigned short *op = (unsigned short *)pwcs;
  const unsigned char *ip = (const unsigned char *)s;
  while (*ip && n > 0) {
    if (*ip & 0x80) {
      int size = mbtowc(op, ip, n);
      if (size == -1) { /* ignore character and move on */
        ip++; n--;
      } else {
        op++; ip += size; n -= size;
      }
    } else {
      *op++ = *ip++; n--;
    }
  }
  return (int)(op - pwcs);
}

int wcstombs(char *s, const wchar_t *pwcs, int maxlen) 
{
  const unsigned short *ip = (const unsigned short *)pwcs; 
  unsigned char *op = (unsigned char *)s;
  while (*ip && maxlen > 0) {
    if (*ip > 0x7f) {
      int size = wctomb_chk(op, *ip, maxlen);
      if (size == -1) { /* ignore character and move on */
        maxlen--;
      } else {
        op += size; maxlen -= size;
      }
    } else {
      *op++ = (unsigned char)*ip;
    }
    ip++;
  }
  return (int)(op - s);
}
