#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>
#include <wasi/api.h>
#include <sys/crt.h>
#include <sys/intrs.h>

static void panic(const char *s, size_t n)
{
  ciovec_t iov; size_t ret;
  fd_t fd = 2; /* stderr */
  iov.buf = (uint8_t*)s; iov.buf_len = n;
  fd_write(fd, &iov, 1, &ret);
}

void _panicf(const char *fmt, ...)
{
  va_arg_t *ap = va_etc();
  while (*fmt) {
    if (*fmt != '%' || *++fmt == '%') {
      panic(fmt++, 1);
    } else if (fmt[0] == 's') {
      char *s = va_arg(ap, char*);
      if (s) panic(s, strlen(s)); 
      else panic("(NULL)", 6);
      fmt += 1;
    } else if (fmt[0] == 'd') {
      int val = va_arg(ap, int);
      char buf[39+1]; /* enough up to 128 bits (w/sign) */
      char *e = &buf[40], *p = e;
      if (val) {
        unsigned m; 
        if (val == (-1-0x7fffffff)) m = (0x7fffffff) + (unsigned)1;
        else if (val < 0) m = -val;
        else m = val;
        do *--p = (int)(m%10) + '0';
          while ((m /= 10) > 0);
        if (val < 0) *--p = '-';
      } else *--p = '0';
      panic(p, e-p);
      fmt += 1;
    } else if (fmt[0] == 'u') {
      unsigned val = va_arg(ap, unsigned);
      char buf[39+1]; /* enough up to 128 bits */
      char *e = &buf[40], *p = e;
      if (val) {
        unsigned m = val; 
        do *--p = (int)(m%10) + '0';
          while ((m /= 10) > 0);
      } else *--p = '0';
      panic(p, e-p);
      fmt += 1;
    } else if (fmt[0] == 'o') {
      unsigned val = va_arg(ap, unsigned);
      char buf[39+1]; /* enough up to 128 bits */
      char *e = &buf[40], *p = e;
      if (val) {
        unsigned m = val; 
        do *--p = (int)(m%8) + '0';
          while ((m /= 8) > 0);
      } else *--p = '0';
      panic(p, e-p);
      fmt += 1;
    } else if (fmt[0] == 'p' || fmt[0] == 'x') {
      unsigned val = va_arg(ap, unsigned);
      char buf[39+1]; /* enough up to 128 bits */
      char *e = &buf[40], *p = e;
      if (val) {
        unsigned m = val, d; 
        do *--p = (int)(d = (m%16), d < 10 ? d + '0' : d-10 + 'a');
          while ((m /= 16) > 0);
      } else *--p = '0';
      panic(p, e-p);
      fmt += 1;
    }
  }
}

double atof(const char *s)
{
  return strtod(s, NULL);
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

/* poor man's strtod: rounding errors are possible in decimal, hex is exact */

#define MAX_BASE10_EXPONENT 511 /* largest possible base 10 exponent */
#define MAX_BASE10_DIGS 18 /* more than this won't affect the result */
static double powers_of_10[] = { 10., 100., 1.0e4, 1.0e8, 1.0e16, 1.0e32, 1.0e64, 1.0e128, 1.0e256 };

static double strtod_dec(const char *s, const char *pc, bool neg, char **endp)
{
  bool negexp = false;
  double fraction, exponent, *pd;
  int c, exp = 0, fracexp = 0;
  int mantsz = 0, dotpos = -1, expsz = -1;
  const char *pexp;

  while (true) {
    if (!isdigit(c = *pc)) {
      if ((c != '.') || (dotpos >= 0)) break;
      dotpos = mantsz;
    }
    ++pc; ++mantsz;
  }
  pexp = pc; pc -= mantsz;
  if (dotpos < 0) dotpos = mantsz; 
  else --mantsz;
  if (mantsz > MAX_BASE10_DIGS) mantsz = MAX_BASE10_DIGS;
  fracexp = dotpos - mantsz;
  if (mantsz == 0) {
    if (endp != NULL) *endp = (char*)s;
    return 0.0;
  } else {
    long long frac = 0LL;
    while (mantsz-- > 0) {
      if ((c = *pc++) == '.') c = *pc++;
      frac = 10LL * frac + (c - '0');
    }
    fraction = (double)frac;
  }
  pc = pexp;
  if ((*pc == 'E') || (*pc == 'e')) {
    expsz = 0;
    if (*++pc == '-') { ++pc; negexp = true; }
    else if (*pc == '+') ++pc;
    while (isdigit(*pc)) {
      exp = exp * 10 + (*pc++ - '0');
      ++expsz;
    }
  }
  if (expsz == 0) {
    if (endp != NULL) *endp = (char*)s;
    return 0.0;
  }
  if (negexp) exp = fracexp - exp;
  else exp = fracexp + exp;
  if (exp < 0) { negexp = true; exp = -exp; } 
  else negexp = false;
  if (exp > MAX_BASE10_EXPONENT) {
    if (endp != NULL) *endp = (char *)pc;
    errno = ERANGE;
    return neg ? -HUGE_VAL : HUGE_VAL;
  }
  exponent = 1.0;
  for (pd = &powers_of_10[0]; exp != 0; exp >>= 1, ++pd) {
    if (exp & 1) exponent *= *pd;
  }
  if (negexp) fraction /= exponent;
  else fraction *= exponent;

  if (endp != NULL) *endp = (char *)pc;
  return neg ? -fraction : fraction;
}

#define MAX_BASE2_EXPONENT 1024 /* largest possible base 2 exponent */
#define MAX_BASE16_DIGS 15 /* more than this won't affect the result */

static double strtod_hex(const char *s, const char *pc, bool neg, char **endp)
{
  bool negexp = false;
  double fraction; // , exponent, *pd;
  int c, exp = 0, fracexp = 0, val;
  int mantsz = 0, dotpos = -1, expsz = -1;
  const char *pexp;

  while (true) {
    if (!isxdigit(c = *pc)) {
      if ((c != '.') || (dotpos >= 0)) break;
      dotpos = mantsz;
    }
    ++pc; ++mantsz;
  }
  pexp = pc; pc -= mantsz;
  if (dotpos < 0) dotpos = mantsz;
  else --mantsz;
  if (mantsz > MAX_BASE16_DIGS) mantsz = MAX_BASE16_DIGS;
  fracexp = (dotpos - mantsz)*4;
  if (mantsz == 0) {
    if (endp != NULL) *endp = (char*)s;
    return 0.0;
  } else {
    long long frac = 0LL;
    while (mantsz-- > 0) {
      if ((c = *pc++) == '.') c = *pc++;
      if ('0' <= c && c <= '9') val = c - '0';
      else if ('a' <= c && c <= 'f') val = c - 'a' + 10;
      else val = c - 'A' + 10;
      frac = 16LL * frac + val;
    }
    fraction = (double)frac;
  }
  pc = pexp;
  if ((*pc == 'P') || (*pc == 'p')) {
    expsz = 0;
    if (*++pc == '-') { ++pc; negexp = true; }
    else if (*pc == '+') ++pc;
    while (isdigit(*pc)) {
      exp = exp * 10 + (*pc++ - '0');
      ++expsz;
    }
  }
  if (expsz == 0) {
    if (endp != NULL) *endp = (char*)s;
    return 0.0;
  }
  if (negexp) exp = fracexp - exp;
  else exp = fracexp + exp;
  if (exp < 0) { negexp = true; exp = -exp; }
  else negexp = false;
  if (exp > MAX_BASE2_EXPONENT) {
    if (endp != NULL) *endp = (char *)pc;
    errno = ERANGE;
    return neg ? -HUGE_VAL : HUGE_VAL;
  }
  fraction = ldexp(fraction, negexp ? -exp : exp);

  if (endp != NULL) *endp = (char *)pc;
  return neg ? -fraction : fraction;
}

/* internal scanner for special values */
static int strtod_s(const char *pc, const char *s)
{
  int n = 0; bool full = true;
  while (*s) {
    int c = *pc;
    if (c == 0) break;
    if (*s == '*') {
      full = false;
      if (c == ')') { ++s; full = true; }
      else ++pc;
    } else if (tolower(*s) == tolower(c)) {
      ++s; ++pc;
    } else break;
    ++n;
  }
  return full ? n : -1;
}

double strtod(const char *s, char **endp)
{
  int neg = false;
  const char *pc = s;
  while (isspace(*pc)) ++pc;
  if (*pc == '-') { ++pc; neg = true; }
  else if (*pc == '+') ++pc;
  if (pc[0] == '0' && (pc[1] == 'x' || pc[1] == 'X')) {
    return strtod_hex(s, pc + 2, neg, endp);
  } else if (pc[0] == 'I' || pc[0] == 'i') {
    int n = strtod_s(pc, "INFINITY");
    if (n == 3 || n == 8) {
      if (endp) *endp = (char *)pc + n;
      return neg ? -HUGE_VAL : HUGE_VAL;
    } else {
      if (endp) *endp = (char *)s;
      return 0.0;
    }
  } else if (pc[0] == 'N' || pc[0] == 'n') {
    int n = strtod_s(pc, "NAN(*)");
    if (n >= 3) {
      if (endp) *endp = (char *)pc + n;
      return -HUGE_VAL + HUGE_VAL; /* fixme */
    } else {
      if (endp) *endp = (char *)s;
      return 0.0;
    }
  }
  return strtod_dec(s, pc, neg, endp);
}

static int digitval(int c)
{
  if ('0' <= c && c <= '9') return c - '0';
  if ('a' <= c && c <= 'z') return 10 + c - 'a';
  if ('A' <= c && c <= 'Z') return 10 + c - 'A';
  return -1;
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
static long jrand48(unsigned short *xsubi) /* unsigned short xsubi[3] */
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


/* system */

void abort(void)
{
  proc_exit((exitcode_t)EXIT_FAILURE);
}

int atexit(onterm_func_t fn)
{
  if (onterm_count < ONTERM_MAX) {
    onterm_funcs[onterm_count++] = fn;
    return 0;
  } 
  return -1;
}

void exit(int status)
{
  terminate(status);
}

char **_environ = NULL;
static char *empty_environ[] = { NULL };

void initialize_environ(void) 
{
  if (_environ != NULL) {
    return;
  } else {
    size_t environ_count, environ_buf_size;
    errno_t error = environ_sizes_get(&environ_count, &environ_buf_size);
    if (error != ERRNO_SUCCESS) goto err;
    size_t num_ptrs = environ_count + 1;
    if (num_ptrs == 0) goto err;
    char *environ_buf = malloc(environ_buf_size);
    if (environ_buf == NULL) goto err;
    char **environ_ptrs = calloc(num_ptrs, sizeof(char *));
    if (environ_ptrs == NULL) { free(environ_buf); goto err; }
    error = environ_get((uint8_t **)environ_ptrs, (uint8_t *)environ_buf);
    if (error != ERRNO_SUCCESS) {
      free(environ_buf);
      free(environ_ptrs);
      goto err;
    }
    _environ = environ_ptrs;
    return;
  err:;
  }
  ciovec_t iov; size_t ret;
  iov.buf = (uint8_t*)"internal error: unable to retrieve environment strings\n"; 
  iov.buf_len = 55;
  fd_write(2, &iov, 1, &ret);
  _environ = &empty_environ[0];
}

char *getenv(const char *name)
{
  if (name != NULL) {
    size_t len = strlen(name);
    initialize_environ();
    char **env;
    for (env = _environ; *env; ++env) {
      if (strncmp(name, *env, len) && (*env)[len] == '=')
        return *env + len + 1;
    }
  }
  return NULL;
}


/* system() is absent */

void *bsearch(const void *key, const void *base, size_t nmemb,
  size_t size, int (*cmp)(const void *, const void *))
{
  while (nmemb) {
    size_t mididx = nmemb / 2;
    const void *midobj = (const char *)base + mididx * size;
    int diff = (*cmp)(key, midobj);
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

void qsort(void *base, size_t nmemb, size_t size, int (*cmp)(const void *, const void *))
{ /* combsort, O(n log n) */
  size_t gap = nmemb, i, j;
  char *p1, *p2; int swapped;
  if (!nmemb) return;
  do {
    gap = newgap(gap);
    swapped = 0;
    for (i = 0, p1 = base; i < nmemb - gap; i++, p1 += size) {
      j = i + gap;
      if ((*cmp)(p1, p2 = (char *)base + j * size) > 0) {
        memswap(p1, p2, size);
        swapped = 1;
      }
    }
  } while (gap > 1 || swapped);
}


/* 'binary buddies' memory allocator */

#define WASMPAGESIZE   65536
#define BUCKET0P2      5U            /* smallest block is 2^5 = 32 bytes */
#define NBUCKETS       27            /* 2^5 .. 2^31 */
#define SBRKBUCKET     11            /* BI2BLKSZ(11) = WASMPAGESIZE */
#define MAXBLOCKSZ     0x80000000UL  /* 2^31 */
#define MAXPAYLOAD     0x7FFFFFF0UL  /* 2^31-sizeof(header) */
#define HDRPTRMASK     0x0000000FUL  /* lower 4 bits must be 0 */
#define BI2BLKSZ(bi)   (1U << ((bi) + BUCKET0P2))
#define CHECKHDRPTR(p) (assert(!((uintptr_t)(p) & HDRPTRMASK)))

/* in WASM 'ff' overlaps w/ ls quad of 'next' */
typedef union header {
  struct { union header *next, *prev; } free;
  struct { uint8_t ff, bi; size_t plsz; } used;
  char align16[16]; /* for 16-byte alignment */
} header_t;
static_assert(sizeof(header_t) == 16);

static header_t* buckets[NBUCKETS];

static header_t *sbrkblock(size_t bi)
{
  assert(bi >= SBRKBUCKET);
  void *p = sbrk((intptr_t)BI2BLKSZ(bi));
  if (p == (void *)-1) return NULL;
  assert(!((uintptr_t)(p) & (uintptr_t)(WASMPAGESIZE-1)));
  return p;
}

static void pushblock(size_t bi, void *pb);
static header_t *pullblock(size_t bi)
{
  header_t *pbi = buckets[bi];
  if (pbi != NULL) {
    if (pbi->free.next != NULL) pbi->free.next->free.prev = NULL;
    buckets[bi] = pbi->free.next;
    return pbi;
  }
  if (bi < SBRKBUCKET) {
    header_t *pb = pullblock(bi + 1);
    if (pb != NULL) {
      header_t *pb2 = (header_t *)((char*)pb + BI2BLKSZ(bi));
      if ((pbi = buckets[bi]) != NULL) pbi->free.prev = pb2;
      pb2->free.next = pbi; pb2->free.prev = NULL;
      buckets[bi] = pb2;
    }
    return pb;
  }
  return sbrkblock(bi);
}

static void unlinkblock(size_t bi, header_t *pb)
{
  if (pb->free.prev == NULL) buckets[bi] = pb->free.next;
  else pb->free.prev->free.next = pb->free.next;
  if (pb->free.next == NULL) /* do nothing */ ;
  else pb->free.next->free.prev = pb->free.prev;
}

static void pushblock(size_t bi, void *p)
{
  header_t *pbi, *pb;
  { retry:
    pbi = buckets[bi], pb = p;
    if (bi < SBRKBUCKET) {
      size_t bsz = BI2BLKSZ(bi); /* odd/even sibling bit */
      if ((uintptr_t)pb & bsz) {
        header_t *pbi_prec = (header_t*)((char*)pb - bsz);
        /* check if prec block is free and belongs to the same bucket */
        if (pbi_prec->used.ff != 0xff && (pbi_prec+1)->used.bi == bi) { 
          unlinkblock(bi, pbi_prec); 
          bi += 1; p = pbi_prec; 
          goto retry; 
        }
      } else {
        header_t *pbi_succ = (header_t*)((char*)pb + bsz);
        /* check if next block is free and belongs to the same bucket */
        if (pbi_succ->used.ff != 0xff && (pbi_succ+1)->used.bi == bi) { 
          unlinkblock(bi, pbi_succ); 
          bi += 1; p = pb; 
          goto retry; 
        }
      }
    }
  }
  if (pbi) pbi->free.prev = pb;
  pb->free.next = pbi; pb->free.prev = NULL;
  (pb+1)->used.bi = (uint8_t)bi; /* bi mark */
  buckets[bi] = pb;
}

static size_t findbktidx(size_t payload)
{
  if (payload <= MAXPAYLOAD) {
    size_t blkmin = payload + sizeof(header_t) - 1;
    size_t p2 = 32U - _clz((unsigned)blkmin);
    return p2 < BUCKET0P2 ? 0 : p2 - BUCKET0P2;
  }
  return NBUCKETS;
}

void *realloc(void *p, size_t n)
{
  if (p != NULL) { /* realloc or free */
    header_t *pb = (header_t*)p - 1;
    assert(pb->used.ff == (uint8_t)0xff);
    size_t bi = pb->used.bi;
    assert(bi < NBUCKETS);
    assert(bi > SBRKBUCKET || !((uintptr_t)pb & (uintptr_t)(BI2BLKSZ(bi)-1UL)));
    if (n > 0) { /* realloc */
      if (n + sizeof(header_t) <= BI2BLKSZ(bi)) {
        pb->used.plsz = n;
        return p;
      } else {
        void *np = realloc(NULL, n); /* malloc */
        size_t orgn = pb->used.plsz;
        if (np != NULL) memcpy(np, p, n < orgn ? n : orgn);
        pushblock(bi, pb);
        return np;
      }
    } else { /* free */
      pushblock(bi, pb);
    }
  } else { /* malloc */
    size_t bi = findbktidx(n);
    if (bi < NBUCKETS) {
      header_t *pb = pullblock(bi);
      if (pb != NULL) {
        pb->used.plsz = n;
        pb->used.ff = (uint8_t)0xff;
        pb->used.bi = (uint8_t)bi;
        return pb+1;
      }
    }
  }
  return NULL;  
}

void *malloc(size_t n)
{
  return realloc(NULL, n);
}

void *calloc(size_t n, size_t sz)
{
  void *p = NULL; 
  assert(sz);
  if (n <= MAXPAYLOAD/sz) {
    n *= sz;
    p = realloc(NULL, n);
    if (p != NULL) memset(p, 0, n);
  }
  return p;
}

void free(void *p)
{
  if (p != NULL) realloc(p, 0);
}


/* misc */

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
  div_t d; d.quot = num/den; d.rem = num%den; return d;
}

ldiv_t ldiv(long num, long den)
{
  ldiv_t d; d.quot = num/den; d.rem = num%den; return d;
}

lldiv_t lldiv(long long num, long long den)
{
  lldiv_t d; d.quot = num/den; d.rem = num%den; return d;
}

int mblen(const char *s, size_t n)
{
  return mbtowc(NULL, s, n);
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
  {0, 0, 0, 0, 0 /* end of table */}
};

int mbtowc(wchar_t *p, const char *s, size_t n) 
{
  const unsigned char *ip = (const unsigned char *)s;
  int c0, c, nc; long l;
  struct utf8_table *t;
  { c0 = *ip; l = c0; nc = 0;
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
  ilseq:;
  }
  errno = EILSEQ;  
  return -1;
}

static int wctomb_chk(unsigned char *s, wchar_t wc, size_t maxlen) 
{
  long l = wc; int c, nc = 0;
  struct utf8_table *t;
  if (s == NULL) return 0;
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

size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n) 
{
  wchar_t *op = pwcs;
  const unsigned char *ip = (const unsigned char *)s;
  while (*ip && n > 0) {
    if (*ip & 0x80) {
      int size = mbtowc(op, (const char *)ip, n);
      if (size == -1) { /* ignore character and move on */
        ip++; n--;
      } else {
        op++; ip += size; n -= size;
      }
    } else {
      *op++ = *ip++; n--;
    }
  }
  return op - pwcs;
}

size_t wcstombs(char *s, const wchar_t *pwcs, size_t maxlen) 
{
  const wchar_t *ip = pwcs; 
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
  return op - (unsigned char *)s;
}
