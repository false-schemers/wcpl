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


/* memory allocator */

#define WASMPAGESIZE 65536

#if 1 /* 'sequential next fit' allocator (after K&R) */

typedef union header {
  struct {
    union header *next;
    size_t size;
  } s;
  char a[16];
} header_t;

static_assert(sizeof(header_t) == 16, "header is not 16 bytes long");
#define UNITSIZE (sizeof(header_t)) 
#define NALLOC (WASMPAGESIZE/UNITSIZE) 

static header_t malloc_base;
static header_t *malloc_freep;

static header_t *morecore(size_t nunits)
{
  assert(nunits > 0);
  nunits = ((nunits + NALLOC - 1) / NALLOC) * NALLOC;

  void *freemem = sbrk((intptr_t)(nunits * UNITSIZE));
  if (freemem == (void *)-1) return NULL;

  header_t *insertp = (header_t *)freemem;
  insertp->s.size = nunits;

  free((void *)(insertp + 1));

  return malloc_freep;
}

void *malloc(size_t nbytes)
{
  header_t *currp, *prevp;
  /* each unit is a chunk of sizeof(header_t) bytes */
  size_t nunits = ((nbytes + sizeof(header_t) - 1) / sizeof(header_t)) + 1; 

  if (malloc_freep == NULL) {
    malloc_base.s.next = &malloc_base;
    malloc_base.s.size = 0;
    malloc_freep = &malloc_base;
  }

  prevp = malloc_freep;
  currp = prevp->s.next;

  for (;; prevp = currp, currp = currp->s.next) {
    if (currp->s.size >= nunits) {
      if (currp->s.size == nunits) {
        prevp->s.next = currp->s.next;
      } else {
        currp->s.size -= nunits;
        currp += currp->s.size;
        currp->s.size = nunits;
      }
      malloc_freep = prevp;
      return (void *)(currp + 1);
    }

    if (currp == malloc_freep) {
      if ((currp = morecore(nunits)) == NULL) {
        errno = ENOMEM;
        return NULL;
      }
    } 
  }
  
  return NULL;
}

void free(void *ptr)
{
  if (!ptr) return;

  header_t *currp, *insertp = ((header_t *)ptr) - 1;

  for (currp = malloc_freep; !((currp < insertp) && (insertp < currp->s.next)); currp = currp->s.next) {
    if ((currp >= currp->s.next) && ((currp < insertp) || (insertp < currp->s.next))) break;
  }

  if ((insertp + insertp->s.size) == currp->s.next) {
    insertp->s.size += currp->s.next->s.size;
    insertp->s.next = currp->s.next->s.next;
  } else {
    insertp->s.next = currp->s.next;
  }

  if ((currp + currp->s.size) == insertp) {
    currp->s.size += insertp->s.size;
    currp->s.next = insertp->s.next;
  } else {
    currp->s.next = insertp;
  }

  malloc_freep = currp;
}

void *calloc(size_t nels, size_t esz)
{
  size_t nbytes = nels * esz;
  void *ptr = malloc(nbytes);
  if (ptr) memset(ptr, 0, nbytes);
  return ptr;
}

void *realloc(void *ptr, size_t nbytes)
{
  if (!ptr) return malloc(nbytes);
  if (!nbytes) { free(ptr); return NULL; }

  header_t *currp = ((header_t *)ptr) - 1;
  size_t nunits = ((nbytes + sizeof(header_t) - 1) / sizeof(header_t)) + 1;
  if (currp->s.size >= nunits) return ptr;
  size_t currnb = (currp->s.size - 1) * sizeof(header_t);

  /* todo: try to merge with next free block if possible */
  void *newptr = malloc(nbytes);
  if (!newptr) { free(ptr); return NULL; }

  memcpy(newptr, ptr, currnb < nbytes ? currnb : nbytes);
  free(ptr);
  return newptr;
}

#else /* 'binary buddies' allocator (no recombining) */

#define NBUCKETS       28            /* 2^4 .. 2^31 */
#define SBRKBUCKET     12            /* 2^12 is WASMPAGESIZE */
#define MAXBLOCKSZ     0x80000000UL  /* 2^31 */
#define MAXPAYLOAD     0x7FFFFFF8UL  /* 2^31-sizeof(header) */
#define HDRPTRMASK     0x00000007UL  /* lower 3 bits */
#define BI2BLKSZ(bi)   (1UL << ((bi) + 4))
#define CHECKHDRPTR(p) (assert(!((uintptr_t)(p) & HDRPTRMASK)))

typedef union header {
  struct { union header *next, *prev; } free;
  struct { size_t bi; size_t plsz; } used;
} header_t;
static_assert(sizeof(header_t) == 8);

static header_t* buckets[NBUCKETS];
static size_t bucketlens[NBUCKETS];

static header_t *sbrkblock(size_t bi)
{
  assert(bi >= SBRKBUCKET);
  void *freemem = sbrk((intptr_t)BI2BLKSZ(bi));
  if (freemem == (void *)-1) return NULL;
  return freemem;
}

static void pushblock(size_t bi, void *pb);
static header_t *pullblock(size_t bi)
{
  header_t *pbi = buckets[bi];
  if (pbi != NULL) {
    CHECKHDRPTR(pbi->free.prev);
    CHECKHDRPTR(pbi->free.next);
    if (pbi->free.next != NULL) pbi->free.next->free.prev = NULL;
    buckets[bi] = pbi->free.next;
    bucketlens[bi] -= 1;
    return pbi;
  }
  if (bi < SBRKBUCKET) {
    pbi = pullblock(bi + 1);
    if (!pbi) return NULL;
    pushblock(bi, (char*)pbi + BI2BLKSZ(bi));
    return pbi;
  }
  return sbrkblock(bi);
}

static void pushblock(size_t bi, void *p)
{
  header_t *pbi = buckets[bi], *pb = p;
  if (pbi) pb->free.prev = pb;
  pb->free.next = pbi; pb->free.prev = NULL;
  buckets[bi] = pb; bucketlens[bi] += 1;
}

static size_t findbktidx(size_t payload)
{
  if (payload <= MAXPAYLOAD) {
    size_t blkmin = payload + sizeof(header_t) - 1;
    size_t p2 = 32U - (uint32_t)asm(local.get blkmin, i32.clz);
    return p2 < 4U ? 0 : p2 - 4U;
  }
  return NBUCKETS;
}

void *realloc(void *p, size_t n)
{
  if (p != NULL) { /* realloc or free */
    CHECKHDRPTR(p);
    header_t *pb = (header_t*)p - 1;
    size_t bi = pb->used.bi; 
    assert(bi < NBUCKETS);
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
        pb->used.bi = bi;
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

#endif


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
