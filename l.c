/* l.c (wcpl library) -- esl */

#include <stdbool.h>
#include <stdint.h>
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
#include <time.h>
#include <math.h>
#include "l.h"

/* globals */
static const char *g_progname = NULL;  /* program name for messages */
static const char *g_usage = NULL;  /* usage string for messages */
int g_wlevel = 0; /* warnings below this level are ignored */
int g_verbosity = 0; /* how verbose are we? */
int g_quietness = 0; /* how quiet are we? */
/* AT&T-like option parser */
int eoptind = 1; /* start*/
int eopterr = 1; /* throw errors by default */
int eoptopt = 0;
char* eoptarg = NULL;
int eoptich = 0;


/* common utility functions */

void eprintf(const char *fmt, ...)
{
  va_list args;

  fflush(stdout);
  if (progname() != NULL)
    fprintf(stderr, "%s: ", progname());

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':')
    fprintf(stderr, " %s", strerror(errno));
  fprintf(stderr, "\naborting execution...\n");

  exit(2); /* conventional value for failed execution */
}

void *emalloc(size_t n)
{
  void *p = malloc(n);
  if (p == NULL) eprintf("malloc() failed:");
  return p;
}

void *ecalloc(size_t n, size_t s)
{
  void *p = calloc(n, s);
  if (p == NULL) eprintf("calloc() failed:");
  return p;
}

void *erealloc(void *m, size_t n)
{
  void *p = realloc(m, n);
  if (p == NULL) eprintf("realloc() failed:");
  return p;
}

char *estrdup(const char *s)
{
  char *t = (char *)emalloc(strlen(s)+1);
  strcpy(t, s);
  return t;
}

char *estrndup(const char* s, size_t n)
{
  char *t = (char *)emalloc(n+1);
  strncpy(t, s, n); t[n] = '\0';
  return t;
}

char *strtrc(char* str, int c, int toc)
{
  char *s;
  for (s = str; *s; ++s) if (*s == c) *s = toc;
  return str;
}

char *strprf(const char *str, const char *prefix)
{
  assert(str); assert(prefix);
  while (*str && *str == *prefix) ++str, ++prefix;
  if (*prefix) return NULL;
  return (char *)str; 
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
  *s = (unsigned char)(((c >> t["\0\0\0\x12\0"]) & t["\0\0\0\x07\0"]) | t["\0\0\0\xf0\0"]);
  s += (unsigned char)(t["\0\0\0\1\0"]);
  *s = (unsigned char)(((c >> t["\0\0\x0c\x0c\0"]) & t["\0\0\x0f\x3f\0"]) | t["\0\0\xe0\x80\0"]);
  s += (unsigned char)(t["\0\0\1\1\0"]);
  *s = (unsigned char)(((c >> t["\0\x06\x06\x06\0"]) & t["\0\x1f\x3f\x3f\0"]) | t["\0\xc0\x80\x80\0"]);
  s += (unsigned char)(t["\0\1\1\1\0"]);
  *s = (unsigned char)(((c & t["\x7f\x3f\x3f\x3f\0"])) | t["\0\x80\x80\x80\0"]);
  s += (unsigned char)(t["\1\1\1\1\0"]);
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
unsigned long strtocc32(const char *s, char** ep, bool *rp)
{
  char buf[SBSIZE+1]; 
  int c;
  assert(s);
  if (rp) *rp = true;
  if (*s) {
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
          c = (int)l & 0xffff;
          if ((long)c != l) goto err;
          if (rp) *rp = false; 
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
          c = (int)l & 0x7fffffff;
          if ((long)c != l) goto err; 
          if (rp) *rp = false; 
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
  err:;
  }
  if (ep) *ep = (char*)s;
  errno = EINVAL;
  return (unsigned long)(-1);
}

unsigned long strtou8cc32(const char *s, char **ep, bool *rp)
{
  if (is8chead(*s)) { if (rp) *rp = false;  return strtou8c(s, ep); }  
  return strtocc32(s, ep, rp);
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


/* floating-point reinterpret casts and exact hex i/o */

unsigned long long as_uint64(double f) /* NB: asuint64 is WCPL intrinsic */
{
  union { unsigned long long u; double f; } uf;
  uf.f = f;
  return uf.u;
}

double as_double(unsigned long long u) /* NB: asdouble is WCPL intrinsic */
{
  union { unsigned long long u; double f; } uf;
  uf.u = u;
  return uf.f;
}

unsigned as_uint32(float f) /* NB: asuint32 is WCPL intrinsic */
{
  union { unsigned u; float f; } uf;
  uf.f = f;
  return uf.u;
}

float as_float(unsigned u) /* NB: asfloat is WCPL intrinsic */
{
  union { unsigned u; float f; } uf;
  uf.u = u;
  return uf.f;
}

/* print uint64 representation of double exactly in WASM-compatible hex format */
char *udtohex(unsigned long long uval, char *buf) /* needs 32 chars (25 chars, rounded up) */
{
  bool dneg = (bool)(uval >> 63ULL);
  int exp2 = (int)((uval >> 52ULL) & 0x07FFULL);
  unsigned long long mant = (uval & 0xFFFFFFFFFFFFFULL);
  if (exp2 == 0x7FF) {
    /* print as canonical NaN; todo: WASM allows :0xHHHHHHHHHH suffix! */
    if (mant != 0ULL) strcpy(buf, dneg ? "-nan" : "+nan");
    else strcpy(buf, dneg ? "-inf" : "+inf");
  } else {
    size_t i = 0, j; int val, vneg = false, dig, mdigs;
    if (exp2 == 0) val = mant == 0 ? 0 : -1022; else val = exp2 - 1023;
    if (val < 0) val = -val, vneg = true;
    do { dig = val % 10; buf[i++] = '0' + dig; val /= 10; } while (val);
    buf[i++] = vneg ? '-' : '+'; buf[i++] = 'p';
    for (mdigs = 13; mdigs > 0; --mdigs, mant >>= 4ULL) {
      dig = (int)(mant % 16); buf[i++] = dig < 10 ? '0' + dig : 'a' + dig - 10; 
    }
    buf[i++] = '.'; buf[i++] = '0' + ((exp2 == 0) ? 0 : 1);
    buf[i++] = 'x'; buf[i++] = '0';
    if (dneg) buf[i++] = '-'; buf[i] = 0;
    for (j = 0, i -= 1; j < i; ++j, --i) { 
      dig = buf[j]; buf[j] = buf[i]; buf[i] = dig;
    }
  }
  return buf;
}

/* read back uint64 representation of double as printed via udtohex; -1 on error */
unsigned long long hextoud(const char *buf)
{
  bool dneg = false;
  if (*buf == '-') ++buf, dneg = true;
  else if (*buf == '+') ++buf;
  if (strcmp(buf, "inf") == 0) {
    return dneg ? 0xFFF0000000000000ULL : 0x7FF0000000000000ULL;
  } else if (strcmp(buf, "nan") == 0) {
    /* read canonical NaN; todo: WASM allows :0xHHHHHHHHHH suffix! */
    return dneg ? 0xFFF8000000000000ULL : 0x7FF8000000000000ULL;
  } else {
    int exp2 = 0; unsigned long long mant = 0ULL;
    int ldig, val, vneg, dig, mdigs;
    if (*buf++ != '0') goto err;
    if (*buf++ != 'x') goto err;
    if (*buf != '0' && *buf != '1') goto err;
    ldig = *buf++ - '0';
    if (*buf++ != '.') goto err;
    for (mdigs = 13; mdigs > 0; --mdigs) {
      if (!*buf || !isxdigit(*buf)) goto err; 
      val = *buf++;
      dig = val <= '9' ? val - '0'
          : val <= 'F' ? val - 'A' + 10
          : val - 'a' + 10;
      mant = (mant << 4ULL) | dig;
    }
    if (*buf++ != 'p') goto err;
    if (*buf != '+' && *buf != '-') goto err;
    vneg = *buf++ == '-';
    for (val = 0; *buf; ++buf) {
      if (!isdigit(*buf)) goto err;
      val = val * 10 + *buf - '0';
    }
    if (vneg) val = -val;
    exp2 = ldig == 1 ? val + 1023 : 0;
    if (0 <= exp2 && exp2 <= 2046)
      return (unsigned long long)dneg << 63ULL | (unsigned long long)exp2 << 52ULL | mant;
 err:;
  }
  return 0xFFFFFFFFFFFFFFFFULL; 
}

/* print uint32 representation of double exactly in WASM-compatible hex format */
char *uftohex(unsigned uval, char *buf) /* needs 32 chars (17 chars, rounded up) */
{
  bool dneg = (bool)(uval >> 31U);
  int exp2 = (int)((uval >> 23U) & 0x0FFU);
  unsigned mant = (uval & 0x7FFFFFU);
  if (exp2 == 0xFF) {
    /* print as canonical NaN; todo: WASM allows :0xHHHHHH suffix! */
    if (mant != 0UL) strcpy(buf, dneg ? "-nan" : "+nan");
    else strcpy(buf, dneg ? "-inf" : "+inf");
  } else {
    size_t i = 0, j; int val, vneg = false, dig, mdigs;
    if (exp2 == 0) val = mant == 0 ? 0 : -126; else val = exp2 - 127;
    if (val < 0) val = -val, vneg = true;
    do { dig = val % 10; buf[i++] = '0' + dig; val /= 10; } while (val);
    buf[i++] = vneg ? '-' : '+'; buf[i++] = 'p';
    for (mdigs = 6, mant <<= 1; mdigs > 0; --mdigs, mant >>= 4) {
      dig = (int)(mant % 16); buf[i++] = dig < 10 ? '0' + dig : 'a' + dig - 10;
    }
    buf[i++] = '.'; buf[i++] = '0' + ((exp2 == 0) ? 0 : 1);
    buf[i++] = 'x'; buf[i++] = '0';
    if (dneg) buf[i++] = '-'; buf[i] = 0;
    for (j = 0, i -= 1; j < i; ++j, --i) {
      dig = buf[j]; buf[j] = buf[i]; buf[i] = dig;
    }
  }
  return buf;
}

/* read back uint32 representation of float as printed via uftohex; -1 on error */
unsigned hextouf(const char *buf)
{
  bool dneg = false;
  if (*buf == '-') ++buf, dneg = true;
  else if (*buf == '+') ++buf;
  if (strcmp(buf, "inf") == 0) {
    /* read canonical NaN; todo: WASM allows :0xHHHHHH suffix! */
    return dneg ? 0xFF800000U : 0x7F800000U;
  } else if (strcmp(buf, "nan") == 0) {
    return dneg ? 0xFFC00000U : 0x7FC00000U;
  } else {
    int exp2 = 0; unsigned mant = 0UL;
    int ldig, val, vneg, dig, mdigs;
    if (*buf++ != '0') goto err;
    if (*buf++ != 'x') goto err;
    if (*buf != '0' && *buf != '1') goto err;
    ldig = *buf++ - '0';
    if (*buf++ != '.') goto err;
    for (mdigs = 6; mdigs > 0; --mdigs) {
      if (!*buf || !isxdigit(*buf)) goto err;
      val = *buf++;
      dig = val <= '9' ? val - '0'
          : val <= 'F' ? val - 'A' + 10
          : val - 'a' + 10;
      mant = (mant << 4) | dig;
    }
    if (*buf++ != 'p') goto err;
    if (*buf != '+' && *buf != '-') goto err;
    vneg = *buf++ == '-';
    for (val = 0; *buf; ++buf) {
      if (!isdigit(*buf)) goto err;
      val = val * 10 + *buf - '0';
    }
    if (vneg) val = -val;
    exp2 = ldig == 1 ? val + 127 : 0;
    if (0 <= exp2 && exp2 <= 254)
      return (unsigned)dneg << 31U | (unsigned)exp2 << 23U | mant >> 1;
  err:;
  }
  return 0xFFFFFFFFU;
}


/* dynamic (heap-allocated) 0-terminated strings */

void dsicpy(dstr_t* mem, const dstr_t* pds)
{
  dstr_t ds = NULL;
  assert(mem); assert(pds);
  if (*pds) strcpy((ds = emalloc(strlen(*pds)+1)), *pds);
  *mem = ds;
}

void dsfini(dstr_t* pds)
{
  if (pds) free(*pds);
}

void dscpy(dstr_t* pds, const dstr_t* pdss)
{ 
  dstr_t ds = NULL;
  assert(pds); assert(pdss);
  if (*pdss) strcpy((ds = emalloc(strlen(*pdss)+1)), *pdss);
  free(*pds); *pds = ds;
}

void dssets(dstr_t* pds, const char *s)
{
  dstr_t ds = NULL;
  assert(pds);
  if (s) strcpy((ds = emalloc(strlen(s)+1)), s);
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
  pb->end = 0;
  pb->buf = NULL;
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
  mem->buf = NULL;
  if (pb->end > 0) {
    mem->buf = ecalloc(pb->end, pb->esz);
    memcpy(mem->buf, pb->buf, pb->esz*pb->fill);
  }
  return mem;
}

void *buffini(buf_t* pb)
{
  assert(pb);
  if (pb->buf) free(pb->buf);
  pb->buf = NULL;
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
  buf_t* pb = (buf_t*)emalloc(sizeof(buf_t));
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
  if (n > 0) { 
    size_t oldsz = pb->end;
    size_t newsz = oldsz*2;
    if (oldsz + n > newsz) newsz += n;
    pb->buf = erealloc(pb->buf, newsz*pb->esz);
    pb->end = newsz;
  }
}

void bufresize(buf_t* pb, size_t n)
{
  assert(pb);
  if (n > pb->end) bufgrow(pb, n - pb->end);
  if (n > pb->fill) {
    assert(pb->buf != NULL);
    memset((char*)pb->buf + pb->fill*pb->esz, 0, (n - pb->fill)*pb->esz);
  }
  pb->fill = n;
}

void* bufref(buf_t* pb, size_t i)
{
  assert(pb);
  assert(i < pb->fill);
  assert(pb->buf != NULL);
  return (char*)pb->buf + i*pb->esz;
}

void bufrem(buf_t* pb, size_t i)
{
  size_t esz;
  assert(pb); assert(i < pb->fill);
  assert(pb->buf != NULL);
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
    assert(pb->buf != NULL);
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
  assert(pb->buf != NULL);
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
  assert(pb->buf != NULL);
  pbk = (char*)pb->buf + (pb->fill-1)*pb->esz;
  return pbk;
}

void *bufnewbk(buf_t* pb)
{
  char* pbk; size_t esz;
  assert(pb);
  if (pb->fill == pb->end) bufgrow(pb, 1);
  assert(pb->buf != NULL);
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
  assert(pb->buf != NULL);
  pbk = (char*)pb->buf + (pb->fill-1)*pb->esz;
  pb->fill -= 1;
  return pbk; /* outside but untouched */
}

void *bufnewfr(buf_t* pb)
{
  return bufins(pb, 0);
}

void bufpopfr(buf_t* pb)
{
  bufrem(pb, 0);
}

void *bufalloc(buf_t* pb, size_t n)
{
  size_t esz; char* pbk;
  assert(pb);
  if (pb->fill + n > pb->end) bufgrow(pb, pb->fill + n - pb->end);
  else if (!pb->buf && n == 0) bufgrow(pb, 1); /* ensure buf != NULL */ 
  esz = pb->esz;
  assert(pb->buf != NULL);
  pbk = (char*)pb->buf + pb->fill*esz;
  memset(pbk, 0, esz*n);
  pb->fill += n;
  return pbk;
}

void bufrev(buf_t* pb)
{
  char *pdata; 
  size_t i, j, len, esz;
  assert(pb);
  len = pb->fill;
  if (len < 2) return;
  esz = pb->esz;
  i = 0; j = len-1;  
  pdata = (char*)pb->buf;
  assert(pb->buf != NULL);
  while (i < j) {
    memswap(pdata+i*esz, pdata+j*esz, esz);
    ++i, --j;
  }
}

void bufcpy(buf_t* pb, const buf_t* pab)
{
  size_t an = buflen(pab);
  assert(pb->esz == pab->esz);
  bufclear(pb);
  if (pab->buf) memcpy(bufalloc(pb, an), pab->buf, an*pab->esz);
}

void bufcat(buf_t* pb, const buf_t* pab)
{
  size_t an = buflen(pab);
  assert(pb->esz == pab->esz);
  if (pab->buf) memcpy(bufalloc(pb, an), pab->buf, an*pab->esz);
}

/* unstable sort */
void bufqsort(buf_t* pb, int (*cmp)(const void *, const void *))
{
  assert(pb); assert(cmp);
  if (pb->buf) qsort(pb->buf, pb->fill, pb->esz, cmp);
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
  assert(pb->buf != NULL);
  while (j < len) {
    /* invariant: elements [0..i] are unique, [i+1,j[ is a gap */
    assert(i < j);
    if (0 == (*cmp)(pdata+i*esz, pdata+j*esz)) {
      /* duplicate: drop it */
      if (fini) (*fini)(pdata+j*esz);
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
  if (pb->fill == 0) return NULL;
  esz = pb->esz;
  pdata = (char*)pb->buf;
  assert(pb->buf != NULL);
  pend = pdata + esz*pb->fill;
  while (pdata < pend) {
    if (0 == (*cmp)(pdata, pe))
      return pdata;
    pdata += esz;
  }
  return NULL;
}

/* binary search */
void* bufbsearch(const buf_t* pb, const void *pe, int (*cmp)(const void *, const void *))
{
  assert(pb); assert(pe); assert(cmp);
  return pb->buf ? bsearch(pe, pb->buf, pb->fill, pb->esz, cmp) : NULL;
}

size_t bufoff(const buf_t* pb, const void *pe)
{
  char *p0, *p; size_t off;
  assert(pb); assert(pe);
  assert(pb->buf != NULL);
  p0 = (char*)pb->buf, p = (char*)pe;
  assert(p >= p0);
  off = (p - p0) / pb->esz;
  assert(off <= pb->fill); /* internal pointers and pointer past last elt are legal */
  return off;
}

void bufswap(buf_t* pb1, buf_t* pb2)
{
  assert(pb1); assert(pb2);
  assert(pb1->esz == pb2->esz);
  memswap(pb1, pb2, sizeof(buf_t));
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
  assert(pb); assert(pb->esz == sizeof(dstr_t));
  pds = (dstr_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) dsfini(pds+i);
  buffini(pb);
}


/* char buffers */

void cbput(const char *s, size_t n, cbuf_t* pb)
{
  size_t l = buflen(pb);
  bufresize(pb, l+n);
  memcpy(l+(char*)pb->buf, s, n);
}

void cbputs(const char *s, cbuf_t* pb)
{
  size_t n = strlen(s);
  cbput(s, n, pb);
}

void cbputlc(unsigned long uc, cbuf_t* pb)
{
  if (uc < 128) cbputc((unsigned char)uc, pb);
  else {
    char buf[5], *pc = (char *)utf8(uc, (unsigned char *)&buf[0]);
    cbput(&buf[0], pc-&buf[0], pb);
  }
}

void cbputwc(wchar_t wc, cbuf_t* pb)
{
  assert(WCHAR_MIN <= wc && wc <= WCHAR_MAX);
  if (wc < 128) cbputc((unsigned char)wc, pb);
  else {
    char buf[5], *pc = (char *)utf8(wc, (unsigned char *)&buf[0]);
    cbput(&buf[0], pc-&buf[0], pb);
  }
}

void cbputd(int val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits (w/sign) */
  char *e = &buf[40], *p = e;
  if (val) {
    unsigned m; 
    if (val == INT_MIN) m = INT_MAX + (unsigned)1;
    else if (val < 0) m = -val;
    else m = val;
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
    if (val < 0) *--p = '-';
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputld(long val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits (w/sign) */
  char *e = &buf[40], *p = e;
  if (val) {
    unsigned long m; 
    if (val == LONG_MIN) m = LONG_MAX + (unsigned long)1;
    else if (val < 0) m = -val;
    else m = val;
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
    if (val < 0) *--p = '-';
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputt(ptrdiff_t val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits (w/sign) */
  char *e = &buf[40], *p = e;
  if (val) {
    size_t m; assert(sizeof(ptrdiff_t) == sizeof(size_t));
    if (val < 0 && val-1 > 0) m = (val-1) + (size_t)1;
    else if (val < 0) m = -val;
    else m = val;
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
    if (val < 0) *--p = '-';
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputu(unsigned val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits */
  char *e = &buf[40], *p = e;
  if (val) {
    unsigned m = val; 
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputx(unsigned val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits */
  char *e = &buf[40], *p = e;
  if (val) {
    unsigned m = val, d; 
    do *--p = (int)(d = (m%16), d < 10 ? d + '0' : d-10 + 'a');
      while ((m /= 16) > 0);
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputlu(unsigned long val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits */
  char *e = &buf[40], *p = e;
  if (val) {
    unsigned long m = val; 
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputllu(unsigned long long val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits */
  char *e = &buf[40], *p = e;
  if (val) {
    unsigned long long m = val; 
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputz(size_t val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits */
  char *e = &buf[40], *p = e;
  if (val) {
    size_t m = val; 
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputll(long long val, cbuf_t* pb)
{
  char buf[39+1]; /* enough up to 128 bits (w/sign) */
  char *e = &buf[40], *p = e;
  if (val) {
    unsigned long long m;
    if (val == LLONG_MIN) m = LLONG_MAX + (unsigned long long)1;
    else if (val < 0) m = -val;
    else m = val;
    do *--p = (int)(m%10) + '0';
      while ((m /= 10) > 0);
    if (val < 0) *--p = '-';
  } else *--p = '0';
  cbput(p, e-p, pb);
}

void cbputg(double v, cbuf_t* pb)
{
  char buf[100];
  if (v != v) strcpy(buf, "+nan"); 
  else if (v <= -HUGE_VAL) strcpy(buf, "-inf");
  else if (v >= HUGE_VAL)  strcpy(buf, "+inf");
  else sprintf(buf, "%.17g", v); /* see WCSSKAFPA paper */
  cbputs(buf, pb);
}

/* minimalistic printf to char bufer */
void cbputvf(cbuf_t* pb, const char *fmt, va_list ap)
{
  assert(pb); assert(fmt);
  while (*fmt) {
    if (*fmt != '%' || *++fmt == '%') {
      cbputc(*fmt++, pb);
    } else if (fmt[0] == 's') {
      char *s = va_arg(ap, char*);
      cbputs(s, pb); fmt += 1;
    } else if (fmt[0] == 'c') {
      int c = va_arg(ap, int);
      cbputc(c, pb); fmt += 1;
    } else if (fmt[0] == 'l' && fmt[1] == 'c') {
      unsigned c = va_arg(ap, unsigned);
      cbputlc(c, pb); fmt += 2;
    } else if (fmt[0] == 'w' && fmt[1] == 'c') {
      wchar_t c = va_arg(ap, int); /* wchar_t promoted to int */
      cbputwc(c, pb); fmt += 2;
    } else if (fmt[0] == 'd') {
      int d = va_arg(ap, int);
      cbputd(d, pb); fmt += 1;
    } else if (fmt[0] == 'x') {
      unsigned d = va_arg(ap, unsigned);
      cbputx(d, pb); fmt += 1;
    } else if (fmt[0] == 'l' && fmt[1] == 'd') {
      long ld = va_arg(ap, long);
      cbputld(ld, pb); fmt += 2;
    } else if (fmt[0] == 'l' && fmt[1] == 'l' && fmt[2] == 'd') {
      long long lld = va_arg(ap, long long);
      cbputll(lld, pb); fmt += 3;
    } else if (fmt[0] == 't') {
      ptrdiff_t t = va_arg(ap, ptrdiff_t);
      cbputt(t, pb); fmt += 1;
    } else if (fmt[0] == 'u') {
      unsigned u = va_arg(ap, unsigned);
      cbputu(u, pb); fmt += 1;
    } else if (fmt[0] == 'l' && fmt[1] == 'u') {
      unsigned long lu = va_arg(ap, unsigned long);
      cbputlu(lu, pb); fmt += 2;
    } else if (fmt[0] == 'l' && fmt[1] == 'l' && fmt[2] == 'u') {
      unsigned long long llu = va_arg(ap, unsigned long long);
      cbputllu(llu, pb); fmt += 3;
    } else if (fmt[0] == 'z') {
      size_t z = va_arg(ap, size_t);
      cbputz(z, pb); fmt += 1;
    } else if (fmt[0] == 'g') {
      double g = va_arg(ap, double);
      cbputg(g, pb); fmt += 1;
    } else {
      assert(0 && !!"unsupported cbputvf format directive");
      break;
    } 
  }
}

void cbputf(cbuf_t* pb, const char *fmt, ...)
{
  va_list args;
  assert(pb); assert(fmt);
  va_start(args, fmt);
  cbputvf(pb, fmt, args);
  va_end(args);
}

void cbput4le(unsigned v, cbuf_t* pb)
{
  cbputc(v & 0xFF, pb); v >>= 8;
  cbputc(v & 0xFF, pb); v >>= 8;
  cbputc(v & 0xFF, pb); v >>= 8;
  cbputc(v & 0xFF, pb);
}

void cbputtime(const char *fmt, const struct tm *tp, cbuf_t* pcb)
{
  char buf[201]; /* always enough? */
  assert(fmt); assert(pcb);
  if (tp != NULL && strftime(buf, 200, fmt, tp)) cbputs(buf, pcb);
}

void cbinsc(cbuf_t* pcb, size_t n, int c)
{
  char *pc = bufins(pcb, n); *pc = c;
}

void cbinss(cbuf_t* pcb, size_t n, const char *s)
{
  while (*s) {
    char *pc = bufins(pcb, n); *pc = *s;
    ++n; ++s;  
  }
}

char* cbset(cbuf_t* pb, const char *s, size_t n)
{
  bufclear(pb);
  cbput(s, n, pb);
  return cbdata(pb);
}

char* cbsets(cbuf_t* pb, const char *s)
{
  bufclear(pb);
  cbputs(s, pb);
  return cbdata(pb);
}

char *cbsetf(cbuf_t* pb, const char *fmt, ...)
{
  va_list args;
  assert(pb); assert(fmt);
  bufclear(pb);
  va_start(args, fmt);
  cbputvf(pb, fmt, args);
  va_end(args);
  return cbdata(pb);
}

char* cbdata(cbuf_t* pb)
{
  *(char*)bufnewbk(pb) = 0;
  pb->fill -= 1;
  return pb->buf; 
}

void cbcpy(cbuf_t* pb, const cbuf_t* pcb)
{
  size_t n = cblen(pcb);
  bufresize(pb, n);
  memcpy((char*)pb->buf, (char*)pcb->buf, n);
}

void cbcat(cbuf_t* pb, const cbuf_t* pcb)
{
  size_t i = cblen(pb), n = cblen(pcb);
  bufresize(pb, i+n);
  memcpy((char*)pb->buf+i, (char*)pcb->buf, n);
}

dstr_t cbclose(cbuf_t* pb)
{
  dstr_t s;
  *(char*)bufnewbk(pb) = 0;
  s = pb->buf; pb->buf = NULL; /* pb is finalized */
  return s; 
}

int cbuf_cmp(const void *p1, const void *p2)
{
  cbuf_t *pcb1 = (cbuf_t *)p1, *pcb2 = (cbuf_t *)p2; 
  const unsigned char *pc1, *pc2;
  size_t n1, n2, i = 0;
  assert(pcb1); assert(pcb2);
  n1 = cblen(pcb1), n2 = cblen(pcb2);
  pc1 = pcb1->buf, pc2 = pcb2->buf;
  while (i < n1 && i < n2) {
    int d = (int)*pc1++ - (int)*pc2++;
    if (d < 0) return -1; if (d > 0) return 1;
    ++i; 
  }
  if (n1 < n2) return -1; if (n1 > n2) return 1;
  return 0;
}

char *fgetlb(cbuf_t *pcb, FILE *fp)
{
  char buf[256], *line; size_t len;
  assert(fp); assert(pcb);
  cbclear(pcb);
  line = fgets(buf, 256, fp); /* sizeof(buf) */
  if (!line) return NULL;
  len = strlen(line);
  if (len > 0 && line[len-1] == '\n') {
    line[len-1] = 0;
    if (len > 1 && line[len-2] == '\r') line[len-2] = 0;
    return cbsets(pcb, line);
  } else for (;;) {
    if (len > 0 && line[len-1] == '\r') line[len-1] = 0;
    cbputs(line, pcb);
    line = fgets(buf, 256, fp); /* sizeof(buf) */
    if (!line) break;
    len = strlen(line);
    if (len > 0 && line[len-1] == '\n') {
      line[len-1] = 0;
      if (len > 1 && line[len-2] == '\r') line[len-2] = 0;
      cbputs(line, pcb);
      break;
    }
  } 
  return cbdata(pcb);
}

/* convert wchar_t string to utf-8 string
 * if rc = 0, return NULL on errors, else subst rc */
char *wcsto8cb(const wchar_t *wstr, int rc, cbuf_t *pcb)
{
  bool convok = true;
  cbclear(pcb);
  while (*wstr) {
    unsigned c = (unsigned)*wstr++;
    if (c > 0x1FFFFF) {
      if (rc) cbputc(rc, pcb);
      else { convok = false; break; }
    }
    cbputlc(c, pcb);
  }
  if (!convok) return NULL;
  return cbdata(pcb);
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

/* grow pcb to get to the required alignment */
void binalign(cbuf_t* pcb, size_t align)
{
  size_t addr = buflen(pcb), n;
  assert(align == 1 || align == 2 || align == 4 || align == 8 || align == 16);
  n = addr % align;
  if (n > 0) bufresize(pcb, addr+(align-n));
}

/* lay out numbers as little-endian binary into cbuf */
void binchar(int c, cbuf_t* pcb) /* align=1 */
{
  cbputc(c, pcb);
}

void binshort(int s, cbuf_t* pcb) /* align=2 */
{
  binalign(pcb, 2);
  cbputc((s >> 0) & 0xFF, pcb);
  cbputc((s >> 8) & 0xFF, pcb);
}

void binint(int i, cbuf_t* pcb) /* align=4 */
{
  binalign(pcb, 4);
  cbputc((i >> 0)  & 0xFF, pcb);
  cbputc((i >> 8)  & 0xFF, pcb);
  cbputc((i >> 16) & 0xFF, pcb);
  cbputc((i >> 24) & 0xFF, pcb);
}

void binllong(long long ll, cbuf_t* pcb) /* align=8 */
{
  binalign(pcb, 8);
  cbputc((ll >> 0)  & 0xFF, pcb);
  cbputc((ll >> 8)  & 0xFF, pcb);
  cbputc((ll >> 16) & 0xFF, pcb);
  cbputc((ll >> 24) & 0xFF, pcb);
  cbputc((ll >> 32) & 0xFF, pcb);
  cbputc((ll >> 40) & 0xFF, pcb);
  cbputc((ll >> 48) & 0xFF, pcb);
  cbputc((ll >> 56) & 0xFF, pcb);
}

void binuchar(unsigned uc, cbuf_t* pcb) /* align=1 */
{
  cbputc((int)uc, pcb);
}

void binushort(unsigned us, cbuf_t* pcb) /* align=2 */
{
  binshort((int)us, pcb);
}

void binuint(unsigned ui, cbuf_t* pcb) /* align=4 */
{
  binint((int)ui, pcb);
}

void binullong(unsigned long long ull, cbuf_t* pcb) /* align=8 */
{
  binllong((long long)ull, pcb);
}

void binfloat(float f, cbuf_t* pcb) /* align=4 */
{
  union { int i; float f; } inf; inf.f = f;
  binint(inf.i, pcb); 
}

void bindouble(double d, cbuf_t* pcb) /* align=8 */
{
  union { long long ll; double d; } ind; ind.d = d;
  binllong(ind.ll, pcb); 
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
    g_symt.a = ecalloc(64, sizeof(char*));
    g_symt.v = ecalloc(64, sizeof(char**));
    g_symt.sz = 64, g_symt.maxu = 64 / 2;
    i = hashs(name) & (g_symt.sz-1);
  } else {
    unsigned long h = hashs(name);
    for (i = h & (g_symt.sz-1); g_symt.v[i]; i = (i-1) & (g_symt.sz-1))
      if (strcmp(name, *g_symt.v[i]) == 0) return (sym_t)(g_symt.v[i]-g_symt.a+1);
    if (g_symt.u == g_symt.maxu) { /* rehash */
      size_t nsz = g_symt.sz * 2;
      char **na = ecalloc(nsz, sizeof(char*));
      char ***nv = ecalloc(nsz, sizeof(char**));
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
    strcpy(emalloc(strlen(name)+1), name);
  sym = (int)((g_symt.u)++);
  return (sym_t)(sym+1);
}

sym_t internf(const char *fmt, ...)
{
  va_list args; sym_t s; cbuf_t cb; 
  assert(fmt); cbinit(&cb);
  va_start(args, fmt);
  cbputvf(&cb, fmt, args);
  va_end(args);
  s = intern(cbdata(&cb));
  cbfini(&cb);
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


/* path name components */

/* returns trailing file name */
char *getfname(const char *path)
{
  char *s1, *s2, *s3, *s = (char*)path;
  s1 = strrchr(path, '\\'), s2 = strrchr(path, '/'), s3 = strrchr(path, ':');
  if (s1 && s < s1+1) s = s1+1; 
  if (s2 && s < s2+1) s = s2+1; 
  if (s3 && s < s3+1) s = s3+1;
  return s;
}

/* returns file base (up to, but not including last .) */
size_t spanfbase(const char* path)
{
  char* fn; char* pc;
  assert(path);
  fn = getfname(path); 
  if ((pc = strrchr(path, '.')) != NULL && pc >= fn)
    return pc-path;
  return strlen(path);
}

/* returns trailing file extension ("" or ".foo") */
char* getfext(const char* path)
{
  char* fn; char* pc;
  assert(path);
  fn = getfname(path); 
  if ((pc = strrchr(path, '.')) != NULL && pc >= fn)
    return (char*)pc;
  return (char*)(path+strlen(path));
}



/* warnings and -w */

void setwlevel(int level)
{
  g_wlevel = level;
}

int getwlevel(void)
{
  int n;
  n = g_wlevel;
  return n;
}

/* verbose output and -v */
void setverbosity(int v)
{
  g_verbosity = v;
}

int getverbosity(void)
{
  int v;
  v = g_verbosity;
  return v;
}

void incverbosity(void)
{
  ++g_verbosity;
}

static void verbosenfa(int n, const char *fmt, va_list args)
{
  int v;
  v = g_verbosity;
  if (n > v) return;
  fflush(stdout);
  vfprintf(stderr, fmt, args);
}

void verbosenf(int n, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  verbosenfa(n, fmt, args);
  va_end(args);
}

void verbosef(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  verbosenfa(1, fmt, args);
  va_end(args);
}

void vverbosef(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  verbosenfa(2, fmt, args);
  va_end(args);
}

void vvverbosef(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  verbosenfa(3, fmt, args);
  va_end(args);
}


/* logger and -q */

void setquietness(int q)
{
  g_quietness = q;
}

int getquietness(void)
{
  int q;
  q = g_quietness;
  return q;
}

void incquietness(void)
{
  ++g_quietness;
}

static void lognfa(int n, const char *fmt, va_list args)
{
  if (n <= g_quietness) return;
  fflush(stdout);
  vfprintf(stderr, fmt, args);
  fflush(stderr);
}

void logenf(int n, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  lognfa(n, fmt, args);
  va_end(args);
}

/* ordinary stderr log: shut up by -q */
void logef(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  lognfa(1, fmt, args);
  va_end(args);
}

/* loud stderr log: shut up by -qq */
void llogef(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  lognfa(2, fmt, args);
  va_end(args);
}

/* very loud stderr log: shut up by -qqq */
void lllogef(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  lognfa(3, fmt, args);
  va_end(args);
}

/* progname: return stored name of program */
const char *progname(void)
{
  const char *str;
  str = g_progname;
  return str;
}

/* setprogname: set stored name of program */
void setprogname(const char *str)
{
  char *pname;
  assert(str);
  str = getfname(str);
  pname = estrndup(str, spanfbase(str));
  g_progname = pname;
}

/* usage: return stored usage string */
const char *usage(void)
{
  const char *s;
  s = g_usage;
  return s;
}

/* setusage: set stored usage string */
void setusage(const char *str)
{
  g_usage = estrdup(str);
}

/* eusage: report wrong usage */
void eusage(const char *fmt, ...)
{
  va_list args;

  fflush(stdout);
  if (progname() != NULL)
    fprintf(stderr, "%s: ", progname());

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  if (fmt[0] != '\0' && fmt[strlen(fmt)-1] == ':')
    fprintf(stderr, " %s", strerror(errno));
  fprintf(stderr, "\n");

  if (progname() != NULL && usage() != NULL)
    fprintf(stderr, "usage: %s %s\n", progname(), usage());

  exit(1);
}

void eoptreset(void)
{
  eoptind = 1;
  eopterr = 1;
  eoptopt = 0;
  eoptarg = NULL;
  eoptich = 0;
}

int egetopt(int argc, char* argv[], const char* opts)
{
  int c; char *popt;

  /* set the name of the program if it isn't done already */
  if (progname() == NULL) setprogname(argv[0]);

  /* check if it's time to stop */
  if (eoptich == 0) { 
    if (eoptind >= argc) 
      return EOF; 
    if (argv[eoptind][0] != '-' ||
        argv[eoptind][1] == '\0')
      return EOF;
    if (strcmp(argv[eoptind], "--") == 0) {
      ++eoptind;
      return EOF;
    }
  }

  /* get next option char */
  c = argv[eoptind][++eoptich];

  /* check if it's legal */
  if (c == ':' || (popt = strchr(opts, c)) == NULL) {
    if (eopterr) {
      eusage("illegal option: -%c", c);
    }
    if (argv[eoptind][++eoptich] == '\0') {
      ++eoptind; 
      eoptich = 0;
    }
    eoptopt = c;
    return '?';
  }

  /* check if it should be accompanied by arg */
  if (popt[1] == ':') {
    if (argv[eoptind][++eoptich] != '\0') {
      eoptarg  = argv[eoptind++] + eoptich;
    } else if (++eoptind >= argc) {
      if (eopterr) {
        eusage("option -%c requires an argument", c);
      }
      eoptich = 0;
      eoptopt = c;
      return '?';
    } else {
      eoptarg = argv[eoptind++];
    }
    eoptich = 0;
  } else {
    if (argv[eoptind][eoptich + 1] == '\0') {
      ++eoptind;
      eoptich = 0;
    }
    eoptarg  = NULL;
  }

  /* eoptopt, eoptind and eoptarg are updated */
  return c;
}


/* NB: the section below is generated with the help of baz archiver (see https://github.com/false-schemers/baz); the command is:
*  baz cvfz l.c --exclude="src" --exclude="lib/include/NDEBUG" --exclude="README.md" --zopfli=1000 wcpl/lib */

struct memdir {
  const char *path;
  size_t size;
  int compression;
  size_t org_size;
  const unsigned char *data;
};

/* start of in-memory archive */

/* lib/crt.args.wo (DEFLATEd, org. size 4765) */
static unsigned char file_l_0[881] =
  "\305\130\333\156\243\60\20\175\137\151\377\301\102\171\10\17\33\5\162\121\124\251\177\122\11\31\30\122\253\140\263\266\111"
  "\333\375\372\5\14\31\300\64\70\152\126\53\125\25\36\316\234\271\333\46\353\102\244\125\16\144\225\110\375\363\7\41"
  "\153\126\224\102\152\342\275\123\305\42\305\151\251\136\205\216\112\11\27\6\357\201\107\274\54\215\336\45\323\340\221\165"
  "\126\361\204\254\346\221\117\75\216\254\113\52\151\101\330\56\364\335\236\45\250\52\327\355\302\367\335\234\242\362\254\242"
  "\63\350\45\247\172\334\343\215\53\366\7\234\135\100\364\103\35\51\245\110\42\370\140\213\76\364\100\64\217\126\12\50"
  "\204\374\154\33\342\251\173\136\303\207\261\154\326\236\117\102\203\75\347\42\246\271\301\106\165\140\27\204\266\313\32\271"
  "\56\52\335\105\124\377\337\44\202\53\115\266\376\274\176\62\326\117\356\321\27\134\203\54\242\104\124\134\43\315\120\172"
  "\17\233\52\127\110\122\57\276\322\335\207\373\160\116\135\323\344\55\212\251\2\144\101\231\347\67\74\16\64\257\100\313"
  "\11\213\21\55\221\244\124\123\103\1\105\251\77\115\155\56\124\22\232\263\63\177\336\223\246\1\237\117\15\266\106\113"
  "\310\66\250\221\252\303\312\274\40\215\160\123\126\72\252\21\104\144\231\2\375\334\147\153\254\100\214\13\206\76\60\364"
  "\201\15\74\116\201\36\153\12\304\353\200\101\112\41\237\110\305\151\234\3\321\202\110\320\222\301\5\152\215\242\240\74"
  "\375\225\63\16\244\216\244\52\200\153\365\302\137\266\133\17\115\214\272\240\351\176\205\111\33\112\75\337\116\104\20\236"
  "\14\121\203\60\104\214\63\315\150\136\277\105\32\224\31\273\265\102\56\222\246\130\255\363\246\71\172\21\257\212\250\324"
  "\122\215\245\355\364\307\125\66\226\66\70\133\312\147\100\300\323\31\102\333\116\134\256\306\202\154\52\140\301\124\20\116"
  "\5\273\116\140\142\65\215\271\71\203\276\216\210\171\321\302\67\12\164\153\166\1\214\355\272\13\121\242\252\170\244\247"
  "\54\75\27\353\331\14\30\367\246\251\375\55\12\70\164\317\331\220\363\214\21\335\362\114\202\256\44\67\317\220\253\216"
  "\52\256\71\336\332\306\30\123\242\233\363\262\301\70\243\200\246\251\131\324\370\334\361\144\31\232\320\0\135\217\116\215"
  "\34\17\207\335\141\140\210\247\16\151\212\145\304\262\57\143\103\174\56\150\172\335\66\372\211\273\301\17\277\377\1\177"
  "\140\247\21\263\202\143\172\267\133\166\227\341\6\77\261\202\143\357\36\317\336\216\307\166\37\167\16\53\352\43\112\44"
  "\24\121\145\7\176\43\342\263\356\25\254\211\130\60\71\206\162\153\302\147\43\121\66\55\360\164\301\60\276\260\153\150"
  "\117\120\121\345\16\211\64\126\355\322\342\101\154\347\310\356\213\5\227\261\41\320\147\124\301\126\161\34\370\377\65\352"
  "\30\302\3\206\324\275\122\355\301\165\303\161\245\205\4\313\336\202\363\326\346\216\7\306\375\161\315\263\45\337\75\131"
  "\170\212\145\150\126\56\107\310\151\41\223\341\325\120\266\31\137\323\334\23\352\152\335\52\371\202\157\73\213\357\160\162"
  "\363\12\65\302\357\273\31\270\120\4\307\373\316\152\374\72\66\320\124\212\22\53\61\167\171\137\154\123\313\343\307\67"
  "\341\364\212\254\101\26\214\123\75\270\41\367\42\274\40\233\357\313\225\322\124\127\152\160\235\304\213\51\277\112\361\332"
  "\24\134\157\110\242\44\253\320\272\326\331\237\174\167\234\146\230\145\373\233\341\176\103\301\362\55\326\326\137\266\340\266"
  "\71\56\154\107\326\124\145\34\173\63\142\74\145\22\22\335\25\351\372\233\203\337\157\375\230\171\340\351\365\141\44\305"
  "\122\141\123\165\305\106\113\313\277\103\314\264\132\375\367\27";

/* lib/crt.argv.wo (DEFLATEd, org. size 4717) */
static unsigned char file_l_1[876] =
  "\304\127\321\156\243\72\20\175\277\322\375\7\13\361\20\36\66\12\44\251\242\110\375\223\112\310\300\100\255\32\233\265\115"
  "\332\356\327\57\340\20\47\23\32\23\245\325\276\104\160\162\346\314\301\36\17\303\242\226\105\313\201\204\271\62\377\377"
  "\107\310\202\325\215\124\206\4\357\124\263\124\13\332\350\127\151\322\106\301\201\301\173\34\220\240\54\322\167\305\14\4"
  "\144\121\266\42\47\341\64\163\77\362\310\242\241\212\326\204\255\223\150\336\265\2\335\162\63\334\104\321\74\123\124\125"
  "\72\255\300\370\114\215\274\357\117\256\331\37\230\145\301\261\277\335\110\243\144\236\302\7\363\172\30\211\27\351\121\26"
  "\155\12\316\262\116\65\247\234\313\374\44\151\361\275\105\357\262\217\205\113\5\200\145\173\154\216\251\172\322\224\105\175"
  "\106\152\250\245\372\34\112\176\177\274\136\300\207\115\140\357\203\210\44\226\133\161\231\121\156\271\51\125\325\301\121\207"
  "\333\216\271\250\133\163\314\325\375\56\163\51\264\41\253\150\72\76\277\214\317\357\211\227\302\200\252\323\134\266\302\70"
  "\231\163\364\36\65\335\204\116\244\273\371\52\166\223\154\222\251\160\103\363\267\64\243\32\234\212\303\202\250\327\231\41"
  "\363\12\264\101\52\26\362\211\24\324\120\53\1\165\143\76\355\336\34\250\42\224\263\112\74\157\110\177\304\236\167\75"
  "\267\143\53\50\227\56\242\320\353\320\376\101\172\160\331\264\46\355\30\104\226\245\6\363\74\256\326\145\0\261\26\254"
  "\174\154\345\343\153\342\6\23\3\326\157\220\240\234\200\122\122\355\111\53\150\306\201\30\111\24\30\305\340\0\135\104"
  "\135\123\121\374\342\114\0\241\252\152\153\20\106\277\210\227\325\52\100\51\306\52\350\353\136\343\52\260\150\20\135\57"
  "\104\234\354\254\120\317\260\102\114\60\303\50\357\376\165\62\16\263\171\273\0\56\363\176\263\6\363\266\70\106\110\264"
  "\165\332\30\245\57\321\241\277\145\155\171\215\116\220\263\46\274\4\112\14\260\30\3\11\6\326\107\300\32\266\325\265"
  "\254\300\234\352\334\376\61\320\227\32\314\220\326\103\166\65\267\116\34\242\333\354\42\116\343\270\131\331\313\11\262\153"
  "\60\70\377\312\1\2\216\327\345\271\146\205\237\150\332\231\2\323\52\141\257\201\353\243\124\326\151\274\15\273\213\44"
  "\235\115\214\41\177\33\7\320\242\260\67\35\237\317\174\1\236\247\60\0\307\102\303\111\236\266\333\365\326\201\124\24"
  "\63\226\51\123\51\53\75\317\326\363\271\244\305\351\354\217\307\346\206\76\374\376\1\375\30\55\43\132\225\361\254\375"
  "\220\255\315\150\313\355\37\172\251\42\107\356\234\77\346\350\253\347\232\62\222\337\60\342\137\233\351\223\343\236\143\42"
  "\141\251\340\124\112\147\366\101\24\130\305\231\360\350\373\116\306\77\74\23\356\21\36\257\346\63\240\156\371\355\322\146"
  "\161\170\303\270\66\122\301\165\76\217\171\327\5\161\147\275\377\271\246\325\362\107\133\260\50\160\111\371\173\355\316\263"
  "\222\311\230\10\215\73\233\320\273\240\367\147\307\133\356\361\266\276\322\333\356\274\256\120\104\362\270\315\170\216\104\374"
  "\164\337\113\315\175\355\132\152\241\144\203\166\2\215\252\236\62\105\216\177\246\10\361\100\150\100\325\114\120\163\66\17"
  "\216\220\33\7\355\247\125\250\15\65\255\76\233\273\334\4\47\34\172\232\57\342\160\164\52\33\22\46\123\363\17\372"
  "\300\271\321\22\52\223\266\256\235\243\125\106\23\362\375\211\142\357\270\207\342\347\147\360\67\107\137\73\302\247\252\24"
  "\256\66\123\46\12\246\40\37\77\352\117\337\276\221\173\207\45\241\353\67\343\305\5\152\267\12\25\225\335\354\31\247"
  "\340\357\340\343\12\130\222\32\20\1\0";

/* lib/crt.void.wo (DEFLATEd, org. size 1416) */
static unsigned char file_l_2[450] =
  "\224\122\327\222\244\60\14\174\276\364\17\52\152\3\274\120\7\227\323\267\120\36\143\130\327\72\225\55\157\372\372\63\30"
  "\106\114\272\60\21\265\273\133\126\50\265\355\243\22\160\305\75\276\173\15\120\112\355\254\107\50\36\131\220\135\60\314"
  "\205\73\213\235\363\342\101\212\307\246\200\302\171\313\73\361\44\261\200\162\210\206\303\325\171\352\367\75\21\112\307\74"
  "\323\40\77\264\125\125\315\131\264\320\326\77\317\131\277\57\317\245\170\312\231\163\134\124\320\146\356\250\354\216\251\314"
  "\355\230\37\71\121\347\60\61\113\35\161\266\207\62\375\326\334\232\200\320\124\247\172\153\120\170\335\161\33\15\222\315"
  "\26\275\344\366\376\214\133\160\127\144\222\202\113\332\217\355\307\366\234\34\31\277\357\166\54\10\162\41\54\231\45\207"
  "\177\260\271\23\314\35\271\144\350\157\46\75\103\106\155\175\40\371\34\46\351\3\363\300\224\34\315\257\217\20\344\213"
  "\370\365\165\322\45\245\27\103\115\352\76\64\127\371\0\46\260\166\21\273\304\0\73\14\101\340\257\265\163\207\2\310"
  "\327\311\366\115\266\157\16\210\64\256\151\315\2\135\157\213\236\273\145\323\176\315\106\23\43\33\111\43\121\62\225\116"
  "\311\206\260\142\246\3\170\201\321\233\351\371\130\77\45\224\206\341\106\276\102\253\172\335\362\253\200\14\143\310\113\260"
  "\234\50\313\247\151\15\146\213\356\22\172\17\127\153\357\224\265\16\256\332\45\312\3\256\107\201\247\213\233\31\233\325"
  "\44\140\304\56\56\321\220\377\151\124\307\15\375\377\104\15\1\41\356\16\364\341\234\376\377\63\174\44\100\107\105\1"
  "\353\173\12\224\145\375\176\271\326\321\347\343\271\321\65\12\61\65\73\103\11\120\235\64\275\364\202\343\62\244\152\132"
  "\341\20\25\126\313\50\74\165\136\230\176\377\160\200\156\107\305\327\202\362\260\51\323\357\21\56\15\261\44\65\40\2"
  "\0";

/* lib/crt.wo (DEFLATEd, org. size 264) */
static unsigned char file_l_3[121] =
  "\215\214\103\2\2\121\30\200\327\351\16\331\256\125\207\31\173\346\331\247\317\366\157\176\175\200\2\121\204\365\266\117\171"
  "\255\134\257\367\101\10\20\65\307\176\163\256\373\241\306\210\362\172\363\324\67\7\365\345\340\170\33\27\310\163\213\323"
  "\55\303\355\333\341\276\151\16\366\54\301\353\351\152\271\257\366\161\352\43\310\170\175\275\134\57\7\157\336\271\353\347"
  "\266\347\262\360\106\271\315\232\203\3\347\17\114\22\272\370\211\162\35\175\206\354\155\7";

/* lib/ctype.wo (DEFLATEd, org. size 4203) */
static unsigned char file_l_4[489] =
  "\314\127\127\162\34\41\20\375\166\272\3\316\312\141\363\356\145\124\23\320\210\62\111\4\247\323\33\57\135\325\26\116\150"
  "\252\107\265\177\314\243\323\343\65\141\216\224\351\243\344\354\135\27\276\131\376\352\71\143\107\102\131\343\2\173\323\271"
  "\360\206\275\361\366\335\33\166\64\110\323\66\62\131\271\260\113\10\73\122\61\60\61\237\35\37\37\377\301\107\161\145"
  "\334\267\344\226\7\331\15\306\127\340\161\33\165\7\131\167\302\67\122\107\305\216\370\327\34\6\200\67\173\323\144\154"
  "\33\327\250\144\275\117\311\216\34\367\121\206\374\221\55\244\351\32\171\61\360\220\214\62\222\276\345\257\361\355\135\223"
  "\47\304\355\203\0\31\114\243\213\316\150\37\330\165\6\270\364\274\56\164\57\6\21\300\111\367\171\340\170\210\116\377"
  "\34\43\331\242\30\44\13\0\35\131\151\276\160\67\15\331\150\55\167\217\42\333\351\340\44\222\5\200\216\254\165\102"
  "\7\344\305\357\277\127\124\225\105\303\252\0\30\127\125\271\250\213\15\42\76\266\345\364\26\1\311\157\142\115\265\203"
  "\153\354\35\126\13\300\44\153\130\64\114\235\277\267\115\307\113\15\240\257\112\372\127\265\335\203\255\214\314\1\240\321"
  "\151\273\106\144\340\67\276\172\1\212\75\64\233\75\120\324\23\160\107\125\220\73\0\64\334\347\63\42\356\253\211\270"
  "\247\257\200\334\1\70\210\216\307\373\152\104\307\37\326\26\303\310\270\324\0\320\267\31\277\247\270\222\12\343\253\42"
  "\76\322\246\115\264\175\242\74\327\363\62\121\215\216\170\65\243\216\0\320\350\270\132\222\34\27\333\253\151\116\213\257"
  "\305\165\16\10\341\171\261\17\107\45\67\375\25\164\65\342\12\242\356\142\372\166\131\217\154\227\372\207\270\357\204\300"
  "\266\1\200\146\313\234\137\317\66\210\65\272\177\364\33\265\225\215\376\204\325\1\100\364\366\231\366\134\236\317\106\36"
  "\143\301\24\307\30\0\117\361\163\124\241\351\174\136\110\372\317\45\251\47\135\74\163\1\240\43\35\255\35\105\272\324"
  "\322\270\261\224\177\14\210\0";

/* lib/dirent.wo (DEFLATEd, org. size 21870) */
static unsigned char file_l_5[2161] =
  "\244\224\335\156\332\60\24\307\357\47\355\35\254\50\27\160\123\101\313\230\26\251\317\202\334\370\30\54\71\266\173\354\254"
  "\353\333\317\362\241\46\73\115\24\165\334\300\341\227\363\377\210\43\262\31\274\32\55\210\126\31\4\227\276\177\23\142"
  "\143\206\340\61\211\246\307\324\210\46\206\266\21\233\263\365\57\322\212\66\263\56\23\261\31\306\44\314\323\343\166\273"
  "\235\321\14\60\170\174\317\62\32\110\166\235\167\125\261\236\222\3\330\366\233\214\346\24\235\14\361\342\323\51\40\374"
  "\66\360\266\317\6\132\235\20\244\122\6\263\217\36\135\57\332\371\335\356\266\51\66\101\242\34\112\314\352\174\74\60"
  "\216\20\107\73\173\10\61\241\161\147\72\207\76\274\327\102\304\73\242\313\201\137\14\311\103\357\255\345\51\127\174\207"
  "\65\365\277\40\167\46\172\157\177\100\164\276\251\337\365\331\227\337\135\371\44\31\253\246\254\171\311\62\215\0\265\30"
  "\321\56\63\230\306\57\151\7\151\255\357\271\232\350\152\175\156\206\60\353\106\230\331\175\321\372\65\172\114\334\270\100"
  "\146\273\72\263\4\335\273\144\163\200\17\340\344\55\241\340\216\340\235\217\267\106\150\343\324\11\301\6\231\56\54\150"
  "\172\351\177\42\106\147\142\122\71\243\267\76\102\65\47\334\25\270\146\105\12\172\367\165\132\371\0\256\274\27\340\17"
  "\105\124\324\224\175\361\341\327\152\365\331\123\134\127\254\357\245\55\256\241\360\112\0\321\343\277\310\354\133\6\36\71"
  "\170\342\340\300\301\17\16\216\34\374\154\247\15\363\370\320\173\27\223\70\354\210\344\55\313\376\10\164\241\30\74\44"
  "\0\272\37\256\337\335\0\274\136\147\275\260\204\220\106\164\64\203\123\123\377\63\244\171\377\375\361\106\244\122\254\123"
  "\71\276\317\267\364\353\270\174\123\165\71\46\217\40\274\326\21\322\363\116\110\153\316\356\371\260\122\312\172\251\76\64"
  "\373\343\124\264\176\50\313\316\177\333\271\332\335\106\221\40\370\52\53\205\277\27\305\100\200\74\215\5\11\326\242\70"
  "\261\205\155\345\156\237\376\160\270\243\324\56\146\313\223\61\34\253\133\51\77\342\206\231\256\351\351\257\231\156\373\214"
  "\323\370\260\57\12\160\363\22\4\237\5\310\113\312\322\373\372\317\143\307\174\335\75\133\237\256\332\303\270\260\173\210"
  "\65\353\10\115\33\376\151\102\227\34\262\307\307\344\21\304\262\227\215\143\113\336\153\271\45\132\132\274\145\356\271\364"
  "\273\136\213\352\343\344\175\207\312\304\312\20\243\43\253\212\243\21\345\12\260\35\300\52\204\75\47\321\225\252\7\352"
  "\50\232\342\72\325\174\20\160\122\206\163\23\61\304\251\140\374\350\366\153\241\274\223\130\360\316\46\132\164\242\234\171"
  "\116\214\127\1\214\241\372\243\261\376\63\77\60\301\36\64\33\355\373\111\105\274\107\112\340\341\126\106\227\161\200\271"
  "\115\356\235\170\256\44\236\312\323\141\131\142\157\376\111\271\312\43\266\146\40\215\354\314\220\212\201\364\136\276\325\327"
  "\356\327\360\231\337\264\213\244\55\31\130\221\353\316\127\253\374\41\173\200\124\154\156\115\232\277\241\200\365\307\52\44"
  "\263\142\251\3\11\145\273\204\246\27\204\157\126\303\254\154\52\336\323\137\332\335\336\5\24\234\257\125\21\122\20\250"
  "\207\237\56\100\215\6\122\265\217\54\141\363\57\1\161\270\307\75\334\127\220\25\167\163\210\227\307\123\335\303\251\262"
  "\301\236\306\151\356\237\200\135\32\153\350\170\27\332\112\7\264\261\343\300\127\325\66\317\274\123\31\240\45\271\222\174"
  "\264\111\0\75\113\101\70\152\10\211\334\225\37\114\255\344\237\30\241\342\237\37\145\144\12\213\101\2\21\307\114\357"
  "\210\51\202\7\205\147\267\73\221\150\317\147\10\3\266\47\174\55\272\127\247\315\246\156\327\333\172\163\264\156\241\343"
  "\324\376\265\76\64\77\152\113\177\257\77\34\324\176\52\113\37\274\24\215\277\313\134\63\334\345\366\111\277\154\171\345"
  "\340\166\154\363\137\112\130\102\161\111\170\42\140\17\104\131\21\45\41\112\114\224\164\122\137\236\304\223\371\362\252\43"
  "\276\176\213\160\102\333\355\277\105\53\127\306\136\265\353\146\323\275\155\7\47\366\343\331\54\326\147\65\156\352\203\207"
  "\343\210\13\316\136\365\40\270\33\222\20\242\213\61\267\237\235\235\266\307\341\44\270\231\34\172\112\320\211\377\44\141"
  "\253\152\151\217\360\230\343\227\377\261\41\140\3\161\230\343\115\202\343\66\205\17\223\10\362\40\136\221\6\17\34\144"
  "\70\160\317\1\163\353\44\70\374\252\141\105\133\7\232\72\67\142\10\226\113\354\364\61\272\152\341\33\130\275\310\50"
  "\45\143\155\235\302\320\146\344\2\235\101\350\65\136\62\65\36\367\221\167\13\243\302\100\143\46\322\41\20\336\116\133"
  "\5\273\5\312\176\63\373\177\14\25\313\12\365\30\140\16\263\267\245\60\122\171\144\62\332\314\202\375\353\55\153\20"
  "\372\266\24\113\233\347\272\60\21\352\50\1\350\140\263\224\300\42\104\201\323\55\153\212\277\353\347\370\365\275\325\265"
  "\205\233\305\4\62\116\320\26\20\23\324\115\242\63\212\76\5\207\171\262\121\172\52\334\254\202\56\143\303\135\146\242"
  "\103\146\242\103\316\373\206\161\113\222\206\6\250\303\16\6\102\143\362\221\300\143\250\220\230\347\255\264\300\355\35\172"
  "\356\362\105\7\37\131\254\241\104\204\26\67\103\341\46\23\372\356\21\200\174\365\201\300\240\221\215\212\113\156\73\313"
  "\122\63\175\201\52\242\157\225\21\334\75\252\236\105\244\175\100\261\76\121\250\274\54\217\25\6\40\136\220\0\237\30"
  "\40\6\41\250\6\307\120\76\3\6\0\364\303\42\266\340\111\26\75\131\270\67\251\170\257\36\74\24\23\212\67\113"
  "\371\173\65\172\50\235\371\124\312\272\21\354\172\71\305\325\262\222\373\230\104\67\22\172\34\5\34\143\75\244\11\116"
  "\142\47\203\373\245\302\173\254\2\44\102\203\204\307\377\305\133\232\156\337\132\204\264\56\346\203\100\240\256\177\245\301"
  "\6\126\234\104\300\266\2\66\363\1\250\303\262\67\121\152\372\150\336\155\261\151\40\271\313\115\75\335\135\165\111\134"
  "\145\230\333\4\236\171\133\255\24\234\133\271\317\331\274\147\270\363\324\172\165\250\353\127\243\125\75\301\255\123\3\245"
  "\103\272\44\45\303\64\147\144\13\323\263\337\172\166\254\267\133\243\147\75\41\240\254\57\3\55\120\177\264\345\176\335"
  "\144\327\331\303\166\175\154\117\365\5\252\332\201\211\56\105\324\364\317\345\145\347\131\107\362\351\74\263\204\363\271\151"
  "\333\34\216\226\332\55\302\22\236\167\157\335\177\176\15\153\150\111\260\115\6\170\60\372\362\276\335\75\327\207\103\375"
  "\62\326\72\160\30\45\202\1\75\71\321\104\317\273\335\153\123\367\337\226\12\152\235\320\115\22\0\302\243\261\36\361"
  "\365\213\145\265\106\374\162\335\10\60\371\263\126\173\334\244\265\365\346\336\145\331\340\205\171\105\33\27\121\147\157\311"
  "\304\133\341\307\123\147\353\67\0\32\353\27\137\167\241\101\23\267\166\336\136\32\270\147\257\112\332\156\222\4\264\207"
  "\334\236\154\231\100\326\242\230\210\170\113\342\5\252\301\177\136\375\42\330\212\267\341\226\75\162\173\63\107\357\300\347"
  "\354\65\362\157\277\304\20\336\337\311\272\212\302\101\162\7\0\13\316\277\335\107\346\343\214\104\113\361\77\155\355\211"
  "\323\351\132\173\170\156\355\4\265\244\364\125\247\166\114\363\164\340\350\156\26\326\131\177\116\142\62\217\342\42\106\6"
  "\265\265\150\335\237\254\167\5\6\316\261\213\347\241\245\13\237\60\215\205\13\303\103\71\150\76\313\300\104\241\55\36"
  "\161\32\334\342\201\75\115\42\135\316\327\110\374\313\371\52\331\103\45\64\104\77\335\325\324\330\243\150\265\312\146\56"
  "\247\46\236\345\324\142\321\345\324\70\135\116\71\25\130\202\277\70\253\204\353\121\237\247\364\125\252\61\316\254\220\351"
  "\272\171\77\277\366\174\164\376\150\204\63\276\161\342\315\17\341\371\50\307\20\3\130\365\51\342\62\114\36\123\157\17"
  "\265\340\45\102\63\102\216\70\244\60\3\57\306\51\61\366\210\312\256\263\225\207\350\177\372\155\326\252\205\247\34\213"
  "\300\14\202\105\205\301\102\134\102\303\370\241\10\36\352\310\50\266\101\367\41\61\54\175\132\206\162\172\356\6\144\110"
  "\231\263\72\151\352\304\111\346\301\160\66\354\266\376\67\25\141\262\4\306\43\123\73\377\122\56\157\220\103\111\2\56"
  "\261\244\121\12\343\241\267\373\133\176\13\24\277\350\164\361\72\152\7\272\333\16\110\264\255\311\25\204\265\325\124\245"
  "\275\256\313\43\255\24\15\116\242\243\135\241\215\102\155\342\41\234\374\17\223\334\346\221\1\126\10\31\32\376\36\176"
  "\121\334\14\11\127\66\212\253\361\164\351\15\14\51\203\355\134\174\310\305\372\31\143\270\335\315\167\171\54\352\173\124"
  "\335\103\155\317\253\220\267\151\266\307\272\25\265\274\337\77\70\260\374\37\33\340\111\27\362\153\3\42\50\340\101\257"
  "\212\72\346\120\245\373\326\46\126\156\367\337\313\163\100\203\221\15\244\113\63\53\255\351\124\327\164\26\224\102\144\236"
  "\47\335\52\160\72\10\326\376\354\346\210\244\272\277\277\1";

/* lib/errno.wo (DEFLATEd, org. size 9609) */
static unsigned char file_l_6[1474] =
  "\154\315\201\6\303\60\20\6\140\0\306\336\341\104\220\100\125\73\114\351\263\124\266\136\52\44\115\135\322\331\366\364\113"
  "\233\126\307\6\221\373\317\367\237\160\276\237\55\2\107\242\321\237\117\0\302\270\311\123\4\166\247\310\200\205\211\63"
  "\20\203\365\67\145\201\247\254\111\11\10\67\107\60\165\45\245\374\143\34\72\117\257\304\362\47\263\355\137\146\261\67"
  "\346\273\315\372\202\300\147\256\131\107\46\217\63\331\364\52\252\135\164\350\302\160\210\165\114\342\241\10\224\65\303\330"
  "\136\40\230\67\266\165\171\135\154\322\204\272\370\156\350\103\305\363\12\226\270\230\346\330\21\152\360\132\7\214\155\271"
  "\355\176\331\207\17\72\50\0\30\204\201\0\146\11\240\24\346\337\330\24\134\4\344\223\55\126\221\225\330\213\354\210"
  "\315\25\135\323\165\164\127\156\345\225\107\227\133\76\272\374\62\207\340\136\31\116\302\316\160\11\326\310\160\23\126\206"
  "\105\210\234\43\170\220\323\204\310\271\202\215\234\107\210\234\217\60\347\254\41\170\127\206\223\260\63\134\202\157\144\270"
  "\11\53\303\42\104\316\21\374\220\323\204\310\271\202\163\140\347\131\242\347\263\314\77\173\120\316\225\345\264\354\54\27"
  "\345\32\131\156\313\312\262\54\61\164\50\67\206\332\22\103\227\262\60\364\54\61\364\131\346\241\32\224\147\145\71\55"
  "\73\313\105\331\43\313\155\131\131\226\45\206\16\345\305\120\133\142\350\122\76\14\75\113\14\175\226\171\350\14\312\157"
  "\145\71\55\73\313\45\271\306\310\162\133\126\226\145\211\241\103\71\61\324\226\30\272\224\13\103\317\22\103\237\145\36"
  "\352\101\271\127\226\323\262\263\134\224\65\262\334\226\225\145\131\142\350\120\36\14\265\45\206\56\145\143\350\131\142\350"
  "\263\314\103\167\120\336\225\345\264\354\54\27\345\33\131\156\313\312\262\54\61\164\50\77\14\265\45\206\256\344\36\30"
  "\172\226\365\127\142\36\313\256\244\60\30\176\25\325\335\116\162\16\263\232\234\163\332\114\304\240\266\51\323\250\257\20"
  "\76\307\117\177\203\373\304\2\165\173\252\274\375\176\324\342\127\300\305\177\61\375\77\51\140\51\46\271\373\357\145\12"
  "\57\176\40\100\146\142\40\153\63\63\272\277\46\223\27\65\164\136\240\37\363\76\267\30\5\202\117\2\102\4\201\342"
  "\136\343\27\5\377\23\162\353\123\362\24\301\141\364\372\321\313\362\150\347\30\123\2\37\41\47\324\270\225\312\105\22"
  "\60\47\343\203\331\5\25\137\253\170\143\132\37\316\27\225\224\273\216\130\364\340\67\205\312\57\230\50\263\105\310\361"
  "\41\206\167\201\30\250\103\66\342\51\302\15\345\340\140\27\310\36\65\335\155\241\373\51\305\210\366\302\233\300\150\334"
  "\31\174\204\216\151\317\230\222\246\63\235\24\102\237\30\7\215\17\10\16\223\145\337\11\261\112\117\253\164\213\51\231"
  "\75\252\124\351\303\317\360\344\55\2\61\360\175\172\166\71\235\125\205\322\216\77\76\144\317\232\150\61\240\123\341\105"
  "\255\14\354\301\7\7\35\223\305\224\60\251\360\162\60\361\273\101\63\114\127\103\60\143\223\323\0\274\36\206\23\212"
  "\212\16\270\320\241\161\157\215\326\233\256\357\3\252\316\266\162\161\111\174\354\23\157\356\352\203\361\145\366\3\275\144"
  "\126\172\356\173\43\7\154\215\170\233\300\334\67\26\312\2\324\200\243\326\370\10\324\100\223\343\345\153\125\341\151\355"
  "\73\221\117\3\261\224\136\374\302\7\4\274\365\111\222\112\315\253\276\357\23\240\123\213\352\131\175\357\64\274\107\25"
  "\54\115\367\25\45\1\237\40\107\106\143\17\103\215\154\126\272\356\153\207\121\174\343\221\201\261\245\241\374\254\113\70"
  "\4\334\233\0\273\263\40\44\174\231\61\132\375\360\215\136\251\327\364\247\331\266\74\77\12\62\347\116\320\215\72\142"
  "\76\251\340\47\23\274\173\260\231\212\116\113\364\203\37\373\161\251\62\245\233\176\45\173\304\313\125\331\276\132\7\146"
  "\354\274\74\61\201\1\347\31\255\20\237\125\260\64\326\157\104\320\232\170\206\200\47\14\11\250\201\164\156\167\24\274"
  "\205\340\343\61\251\132\313\272\111\37\207\1\234\114\310\127\270\166\276\322\243\32\16\141\135\366\207\176\242\134\161\350"
  "\106\155\1\52\262\255\176\161\64\55\216\257\65\245\277\176\100\271\41\76\202\117\340\350\46\252\344\364\212\101\2\273"
  "\63\304\136\117\325\231\151\21\134\323\33\26\163\375\216\32\37\60\1\165\170\51\324\164\116\202\255\252\123\235\251\273"
  "\334\64\310\220\72\143\161\174\341\132\54\153\32\51\333\3\70\74\171\253\203\53\15\354\367\30\342\361\22\132\224\236"
  "\373\374\26\155\226\267\21\367\62\15\161\153\144\270\1\54\66\265\120\336\316\327\164\105\2\266\377\333\273\313\212\377"
  "\110\0\43\345\375\241\117\274\212\116\53\350\375\352\6\324\200\34\56\125\357\31\35\310\271\323\205\112\7\376\304\44"
  "\144\51\134\267\151\57\347\265\110\172\327\4\154\4\50\216\70\140\131\231\254\167\223\341\272\65\175\271\54\353\340\200"
  "\220\36\232\167\44\31\157\340\313\232\21\345\151\7\7\142\60\317\73\61\10\135\323\343\227\245\101\77\273\147\56\321"
  "\141\333\211\116\227\276\374\125\214\340\205\144\264\164\102\36\274\242\255\362\145\175\206\64\154\125\265\347\343\165\74\276"
  "\173\312\253\2\212\143\352\225\11\35\115\327\61\165\354\215\40\274\235\327\226\242\60\205\307\123\124\265\331\110\373\1"
  "\342\261\15\157\125\72\371\217\347\303\22\204\140\207\220\204\30\35\370\10\27\201\241\362\132\55\52\345\205\47\117\71"
  "\1\335\104\144\160\36\235\112\57\325\375\253\317\167\207\334\172\221\1\205\125\271\353\62\35\61\102\347\7\242\136\353"
  "\115\141\260\203\256\66\72\170\125\51\257\266\272\300\15\123\334\137\262\15\15\361\210\275\326\223\132\73\316\101\306\267"
  "\220\365\264\202\32\367\36\305\320\317\325\221\161\272\236\251\153\153\102\74\252\330\134\263\360\375\303\126\45\27\352\350"
  "\121\221\301\227\260\370\26\35\120\26\25\257\154\207\170\53\175\166\206\336\376\353\312\73\230\51\245\367\372\52\275\364"
  "\124\225\335\124\246\274\140\114\236\342\207\360\251\351\314\316\7\57\36\23\370\230\162\323\170\353\37\237\10\157\176\257"
  "\1";

/* lib/fcntl.wo (DEFLATEd, org. size 20902) */
static unsigned char file_l_7[2337] =
  "\324\125\301\156\333\60\14\275\17\330\77\10\206\17\351\141\101\234\25\135\21\240\177\62\300\120\54\312\65\46\113\232\44"
  "\267\353\276\176\116\350\104\65\147\55\102\233\36\166\111\331\247\107\362\361\311\246\127\275\21\203\2\126\312\106\7\365"
  "\371\23\143\253\256\267\306\5\126\64\56\24\254\360\266\54\330\252\125\146\317\25\53\107\154\67\42\154\325\17\201\165"
  "\137\267\67\67\67\13\71\75\364\306\275\214\151\30\140\332\24\157\60\43\253\313\330\200\260\237\271\357\152\257\271\365"
  "\217\46\324\326\301\123\7\317\325\130\100\212\232\213\247\316\303\130\106\16\272\141\345\62\165\167\46\262\225\345\216\367"
  "\307\46\347\370\356\166\61\106\216\3\77\50\62\166\206\54\245\114\303\103\226\60\244\346\110\173\263\34\51\174\340\241"
  "\156\41\144\10\212\144\42\51\306\357\223\341\41\324\122\361\326\347\210\41\51\327\226\144\35\144\133\63\143\177\224\20"
  "\321\271\132\363\36\162\324\220\24\52\351\152\362\54\17\217\265\261\240\57\211\72\23\163\244\320\70\377\275\314\37\311"
  "\7\241\272\375\70\102\163\174\315\316\372\21\337\41\372\256\302\322\1\320\262\7\54\26\112\347\366\213\242\20\315\27"
  "\342\72\335\342\376\155\354\313\271\30\342\73\104\63\154\317\157\322\133\332\4\321\253\65\31\3\5\232\66\101\64\267"
  "\30\70\247\115\161\372\33\277\62\307\377\167\307\137\114\113\112\20\203\245\22\20\275\44\141\152\205\37\327\235\165\140"
  "\54\150\37\277\236\113\54\75\364\165\36\163\142\325\15\267\274\351\302\113\26\333\327\326\330\101\361\0\42\257\272\7"
  "\33\211\154\65\376\256\33\243\175\140\267\337\246\74\301\3\77\145\11\137\155\112\206\4\256\272\126\77\124\254\130\177"
  "\337\154\12\344\242\211\310\165\340\273\337\160\36\366\160\316\146\56\262\11\122\246\71\350\362\201\273\20\247\105\75\323"
  "\231\121\42\161\242\341\71\235\23\235\246\71\344\4\225\274\32\36\201\43\177\355\41\120\165\170\214\216\256\133\10\251"
  "\73\173\135\46\0\314\7\241\115\67\21\200\237\123\54\147\216\275\56\327\246\124\201\362\100\211\377\354\273\215\100\77"
  "\250\251\210\26\324\2\264\72\317\0\117\222\347\267\101\5\334\23\271\113\255\306\43\105\266\71\61\167\176\257\131\346"
  "\122\322\227\12\21\7\141\160\232\70\101\344\55\115\332\322\111\251\115\144\13\44\334\240\327\201\323\223\115\217\7\302"
  "\31\233\226\107\25\370\364\105\245\255\117\47\23\136\332\3\172\177\322\1\44\56\50\132\377\367\106\71\314\157\17\165"
  "\145\367\13\260\356\151\105\227\226\207\307\270\250\311\212\331\217\322\176\260\262\52\117\102\215\145\345\266\244\22\252\11"
  "\220\144\36\254\36\331\312\160\161\137\173\146\244\364\20\36\66\247\125\170\151\53\170\260\351\247\220\166\113\202\124\60"
  "\2\134\320\27\26\123\350\143\202\333\341\155\343\321\45\171\227\265\260\362\73\125\357\62\22\224\117\75\124\240\105\206"
  "\331\164\43\146\370\372\177\270\111\115\271\252\167\37\377\240\356\35\276\272\121\15\11\42\155\273\100\233\241\261\22\125"
  "\174\161\373\70\150\73\37\300\235\26\33\210\132\212\371\22\222\177\332\271\16\55\305\161\40\370\57\227\363\71\12\317"
  "\327\370\221\361\46\170\206\215\137\177\142\204\137\141\152\364\112\326\211\315\167\233\334\26\255\126\253\143\111\303\152\124"
  "\7\377\324\257\137\270\150\165\41\373\213\237\253\141\240\35\306\317\135\376\323\15\241\270\20\302\323\214\56\131\244\25"
  "\42\236\123\245\47\123\357\253\165\270\275\105\247\147\250\235\104\245\24\202\367\243\312\237\112\13\360\113\133\124\250\12"
  "\112\355\251\36\243\135\21\31\76\266\100\1\73\322\33\253\262\143\17\44\365\36\117\373\176\175\33\62\53\376\330\301"
  "\123\270\373\304\351\12\236\173\263\122\363\106\224\52\103\27\26\332\376\40\166\14\317\226\73\10\276\20\261\270\300\250"
  "\340\342\10\34\64\70\106\14\70\54\105\143\313\324\73\230\254\311\200\162\174\275\40\163\302\347\102\147\337\360\140\177"
  "\217\353\363\271\214\166\31\213\142\341\274\376\111\176\203\217\13\251\120\164\162\231\131\362\232\127\50\114\71\103\141\200"
  "\77\250\42\14\175\30\262\141\333\155\56\263\103\230\322\317\32\44\267\5\210\214\101\50\51\271\132\77\320\40\263\251"
  "\353\262\6\161\316\112\156\50\240\142\41\44\146\370\14\102\157\144\76\171\22\363\361\50\25\245\332\153\52\12\203\22"
  "\214\146\75\360\255\20\301\144\22\100\207\6\150\122\245\101\230\33\353\260\270\207\16\211\4\211\42\124\102\26\256\221"
  "\167\322\310\375\214\260\114\250\300\4\332\42\173\341\224\316\113\104\122\155\74\266\36\51\5\115\262\75\265\257\165\323"
  "\4\35\244\163\42\244\76\150\150\172\373\367\120\304\166\145\314\211\214\204\333\347\10\37\203\303\310\216\210\161\51\346"
  "\303\170\20\67\156\276\164\45\142\31\347\126\356\42\361\200\26\61\141\11\301\25\241\133\174\373\162\176\132\356\10\274"
  "\102\151\167\103\153\355\111\10\350\12\350\102\325\67\56\2\137\314\217\30\166\127\144\13\41\115\73\42\126\230\30\241"
  "\300\10\321\60\262\22\110\74\45\65\103\263\57\17\121\341\76\23\242\322\364\144\351\235\2\73\307\234\272\173\106\323"
  "\56\115\260\374\4\170\240\224\234\265\236\0\175\42\51\242\323\311\372\335\311\316\144\307\120\226\76\107\1\317\270\211"
  "\312\45\263\40\235\171\341\121\41\206\67\6\100\257\42\316\156\272\127\253\166\276\70\102\255\34\71\7\212\35\166\301"
  "\202\10\267\233\237\272\67\353\66\50\330\76\206\164\4\152\117\14\336\254\250\235\327\375\75\61\135\356\137\36\136\333"
  "\26\63\24\31\24\41\341\223\37\163\104\0\305\61\376\306\166\5\246\330\77\17\162\107\325\4\64\132\375\24\6\301"
  "\161\314\300\76\224\321\241\231\373\175\47\20\344\253\45\226\22\37\271\43\300\300\176\115\45\66\43\155\236\206\223\161"
  "\332\247\162\40\150\124\62\322\306\246\213\150\74\51\33\25\166\16\6\33\233\135\171\102\270\6\112\164\256\72\343\22"
  "\43\144\321\15\22\173\213\137\164\166\100\32\215\0\235\24\106\346\240\121\377\224\262\20\50\21\202\106\17\360\317\10"
  "\363\44\250\273\242\156\344\372\322\23\63\215\255\205\151\207\74\341\31\331\151\24\203\314\50\353\314\110\1\370\324\375"
  "\63\17\317\110\73\34\276\274\336\255\206\215\311\122\361\140\142\26\34\201\7\132\323\377\207\256\152\211\45\360\376\337"
  "\324\76\362\0\205\170\112\277\143\313\327\345\234\225\351\261\22\373\155\375\316\135\337\273\246\376\360\73\227\171\21\345"
  "\234\70\123\371\252\217\120\30\314\345\104\102\7\132\300\171\302\143\11\244\223\106\207\311\271\246\117\10\313\234\327\62"
  "\77\301\262\334\63\154\212\16\366\75\66\266\167\227\326\165\257\360\316\135\161\36\10\66\126\357\373\261\275\331\100\371"
  "\374\365\241\5\103\130\342\321\103\355\273\355\356\164\154\27\363\343\332\261\347\167\335\253\335\272\357\116\266\166\302\210"
  "\317\342\10\261\54\356\146\377\166\241\103\366\253\212\74\317\362\202\306\332\15\341\36\11\217\263\361\243\241\362\32\217"
  "\124\174\227\343\107\216\234\316\150\250\363\255\263\262\314\115\325\144\243\63\4\256\67\162\126\145\131\327\125\125\362\321"
  "\13\306\362\311\132\115\154\314\54\317\232\306\124\223\330\24\34\330\112\253\364\331\254\150\46\361\61\304\247\60\115\125"
  "\326\125\155\46\361\251\274\332\311\46\361\51\177\342\164\317\175\46\27\171\172\247\241\350\330\303\42\114\0\73\206\315"
  "\347\246\154\14\210\373\336\153\374\10\336\102\144\332\214\324\62\127\371\103\145\212\172\212\324\214\355\303\246\351\205\361"
  "\275\230\261\331\65\121\365\165\144\146\2\136\221\362\200\176\263\362\235\317\273\304\223\364\350\22\2\206\63\377\310\12"
  "\126\140\320\70\371\6\272\104\156\146\66\312\345\312\43\164\143\302\263\173\232\223\175\57\344\236\346\323\125\366\100\373"
  "\22\262\163\230\37\245\211\307\311\101\243\4\175\15\45\344\346\322\54\64\170\317\263\335\226\74\161\374\231\45\125\112"
  "\332\27\131\361\301\27\144\5\242\45\154\356\272\150\332\365\155\352\75\335\152\55\153\215\221\135\114\234\376\300\61\271"
  "\362\134\134\321\77\250\10\366\337\3\140\310\265\207\2\260\136\372\216\153\334\157\121\7\37\333\135\255\272\36\307\54"
  "\137\127\67\317\41\104\125\5\214\237\220\13\130\175\305\242\217\63\363\351\155\27\342\53\213\26\100\24\142\54\224\7"
  "\160\40\261\375\77\376\11\7\170\174\124\170\303\362\345\15\141\176\370\111\172\3\103\13\360\43\320\54\253\361\163\10"
  "\356\0\376\77\32\355\157\137\50\172\0\104\40\145\317\157\267\122\36\54\270\356\363\64\137\274\130\237\171\234\331\236"
  "\147\72\117\56\357\55\45\151\103\312\244\327\241\320\30\177\332\356\346\173\162\317\15\56\35\27\227\160\310\230\30\302"
  "\141\130\125\335\14\105\265\100\40\174\12\167\157\335\115\220\326\276\153\217\240\142\33\22\310\143\252\273\10\3\235\11"
  "\310\211\172\67\326\67\37\60\153\364\110\363\206\211\140\222\57\120\235\0\237\322\253\23\160\145\204\76\105\253\35\357"
  "\324\236\263\151\233\376\51\344\315\17\117\14\363\227\247\174\304\211\117\350\332\11\237\170\122\227\31\63\146\351\356\216"
  "\105\210\144\23\321\237\122\116\222\137\143\304\231\311\125\115\167\315\115\140\377\245\140\157\321\170\351\47\200\155\325\117"
  "\376\35\273\167\355\306\175\13\31\312\374\21\131\225\373\316\351\316\61\22\64\213\15\215\11\147\126\313\265\257\13\300"
  "\202\6\166\223\303\363\13\153\306\251\356\106\105\114\54\256\166\27\115\364\317\14\101\37\362\126\225\123\62\315\135\307"
  "\204\6\267\367\301\26\204\257\213\33\331\20\136\374\77\53\372\156\61\11\55\106\156\275\333\262\247\66\337\376\372\17"
  "";

/* lib/fenv.wo (DEFLATEd, org. size 1286) */
static unsigned char file_l_8[260] =
  "\265\221\105\162\304\100\14\105\327\241\73\50\314\274\233\333\164\154\331\201\246\222\324\201\333\307\130\32\346\61\252\236\340"
  "\367\57\135\271\220\47\213\160\126\240\377\76\332\7\270\372\160\61\220\300\111\106\162\2\47\34\317\116\340\252\264\341"
  "\315\130\70\253\330\240\42\160\345\222\300\307\353\313\365\365\365\224\36\207\56\320\137\325\326\6\155\133\27\77\165\35"
  "\105\362\131\53\72\50\60\263\150\10\177\63\214\2\127\370\333\316\32\301\47\115\127\325\27\15\31\7\147\316\360\127"
  "\163\0\150\71\241\44\362\165\74\145\174\211\322\116\51\254\51\125\140\54\61\56\121\263\330\152\364\250\55\346\245\205"
  "\311\174\60\216\373\122\254\276\326\126\340\131\326\170\307\326\4\131\306\235\51\135\140\354\212\220\223\225\41\261\72\174"
  "\310\202\147\201\247\345\66\112\41\371\134\265\225\365\312\33\213\360\204\210\262\161\173\15\334\236\271\52\120\325\236\214"
  "\153\126\50\56\275\257\367\140\363\311\175\51\235\71\174\163\77\74\341\207\67\367\223\142\156\4\107\347\52\134\151\164"
  "\365\374\3";

/* lib/include/assert.h (DEFLATEd, org. size 182) */
static unsigned char file_l_9[137] =
  "\105\214\301\12\302\60\14\206\357\203\275\103\300\113\73\220\355\354\105\220\211\67\157\236\45\333\342\32\350\132\111\132\237"
  "\337\26\25\41\204\44\371\277\257\357\140\144\134\103\324\304\263\102\327\267\115\333\354\236\202\353\206\20\303\114\165\57"
  "\241\344\130\241\24\202\320\232\75\12\70\302\205\244\22\365\257\11\213\340\216\252\44\251\6\247\314\76\355\71\374\224"
  "\13\75\70\20\134\307\363\351\166\201\341\177\371\40\206\54\230\332\216\140\136\221\27\73\300\341\73\241\156\46\7\41"
  "\234\35\116\236\254\155\233\67";

/* lib/include/ctype.h (DEFLATEd, org. size 495) */
static unsigned char file_l_10[154] =
  "\164\320\101\152\5\61\10\6\340\375\203\271\203\320\115\233\115\16\320\145\117\342\230\220\110\23\23\214\241\163\374\62\333"
  "\171\270\123\76\104\177\143\200\237\212\212\144\131\241\242\244\306\122\40\304\343\165\274\76\246\142\351\10\103\50\337\175"
  "\276\54\253\0\213\1\57\154\262\373\347\135\323\327\367\233\315\212\216\221\230\66\307\22\27\66\307\212\342\254\216\265"
  "\361\227\325\261\251\54\346\331\26\362\154\115\244\354\330\236\323\335\167\371\41\154\370\203\66\36\41\142\0\31\262\14"
  "\45\241\46\70\267\1\215\336\207\100\210\317\167\57\142\166\316\71\33\312\357\377\20\162\0";

/* lib/include/dirent.h (DEFLATEd, org. size 1506) */
static unsigned char file_l_11[504] =
  "\322\327\2\64\123\225\135\157\302\120\370\173\345\77\334\171\141\356\256\324\250\353\174\343\144\20\332\234\321\204\223\244\363"
  "\375\367\335\244\274\306\201\356\353\133\357\43\127\11\20\13\271\41\32\104\14\21\223\64\324\102\376\4\312\265\144\124"
  "\201\173\275\136\251\127\316\245\222\254\66\4\4\17\51\376\143\74\114\266\21\205\47\352\247\272\256\177\246\124\135\133"
  "\77\263\272\210\306\214\123\150\56\202\345\250\77\32\277\31\301\15\0\114\361\24\336\274\234\373\320\366\7\255\305\273"
  "\111\353\220\305\360\307\115\257\6\175\60\217\233\105\46\144\307\136\77\150\266\136\373\136\53\357\364\272\63\353\274\125"
  "\344\104\366\345\354\245\267\150\315\112\334\115\177\347\276\135\350\106\266\345\55\306\263\167\171\333\254\325\261\266\73\205"
  "\66\144\227\203\227\263\0\201\174\102\304\332\143\343\274\127\350\234\143\237\255\105\60\137\314\132\57\207\171\353\140\264"
  "\233\321\375\142\353\273\341\253\361\300\367\202\201\217\272\335\366\224\226\333\120\333\345\162\15\277\353\25\0\306\105\200"
  "\110\200\337\217\315\377\55\127\154\305\151\4\341\232\110\304\315\122\55\221\375\347\144\103\77\334\374\204\320\137\174\327"
  "\53\206\307\222\40\13\155\7\210\157\313\321\37\232\112\16\6\162\343\110\244\224\143\346\6\303\324\161\344\240\42\343"
  "\167\100\230\10\105\15\157\345\370\43\75\46\261\130\26\200\150\33\2\177\305\321\25\10\5\127\332\26\147\115\246\274"
  "\22\137\143\277\364\60\146\121\1\226\335\133\341\311\321\272\222\222\250\114\372\115\260\10\44\375\316\370\176\211\242\364"
  "\353\11\301\25\110\4\137\341\107\170\114\152\41\115\223\4\25\245\245\253\220\374\167\164\127\362\75\270\256\31\121\302"
  "\224\276\142\257\24\304\32\156\314\22\214\352\144\303\314\71\234\143\302\120\154\122\122\46\164\257\100\61\356\70\217\241"
  "\250\362\306\51\55\366\104\255\44\111\327\104\11\251\241\44\20\51\213\364\305\261\7\6\217\361\350\235\377\250\136\141"
  "\210\146\227\120\220\155\125\122\64\261\120\347\73\57\47\16\31\23\334\334\0\376\1";

/* lib/include/errno.h (DEFLATEd, org. size 1694) */
static unsigned char file_l_12[599] =
  "\322\327\122\160\55\52\312\57\52\126\320\322\347\345\342\345\122\56\50\112\114\317\115\124\310\317\113\116\5\363\123\122\323"
  "\62\363\122\1\144\324\111\272\243\40\24\206\341\255\374\113\10\255\336\41\321\243\341\271\10\6\60\115\355\177\41\25"
  "\122\335\41\65\175\155\77\72\120\71\246\211\112\301\211\231\74\373\25\202\201\153\367\100\162\231\347\354\343\121\10\352"
  "\103\143\252\356\346\174\200\346\27\226\230\312\261\357\51\127\30\356\253\363\21\226\113\310\344\346\47\6\146\147\67\57"
  "\30\173\330\312\212\57\116\107\171\102\360\212\311\305\211\2\315\20\202\353\305\207\27\111\116\51\106\167\116\271\266\173"
  "\325\307\205\114\313\121\332\5\375\337\205\102\25\202\327\314\344\346\360\15\141\73\53\265\15\113\246\53\4\217\232\323"
  "\6\61\162\270\36\251\102\360\50\172\370\122\41\171\325\342\216\360\42\301\251\115\227\344\105\227\124\352\21\63\271\351"
  "\2\311\213\374\234\67\110\315\45\24\272\102\32\116\161\317\151\315\124\12\244\355\274\146\310\241\223\233\13\220\43\247"
  "\4\311\23\174\151\203\5\165\352\154\366\31\212\67\204\224\166\50\336\260\55\76\20\224\342\24\174\374\206\322\234\312"
  "\132\374\17\202\62\34\217\120\375\245\275\320\62\215\156\243\232\122\110\161\205\342\25\221\352\234\356\21\152\354\361\327"
  "\14\253\257\136\377\214\253\76\161\177\377\254\26\234\322\371\130\12\264\354\154\246\33\264\352\210\142\205\326\75\75\150"
  "\202\66\235\205\351\33\332\366\344\343\313\206\316\66\332\240\307\236\312\12\375\325\321\236\123\115\151\257\60\247\316\313"
  "\76\301\210\236\236\5\246\217\250\357\51\65\252\307\66\247\106\367\106\333\136\237\60\246\327\114\123\272\121\166\347\100"
  "\60\175\122\55\351\325\151\206\17\74\166\230\261\267\366\336\56\352\341\23\54\317\151\337\130\102\272\303\12\256\367\110"
  "\271\355\124\130\136\265\123\336\140\25\27\277\23\254\346\322\106\15\326\174\322\277\263\315\332\317\153\365\331\336\302\173"
  "\262\213\353\213\106\116\151\51\260\274\246\274\277\76\234\70\345\351\202\101\160\251\56\20\6\236\121\375\106\163\72\52"
  "\6\305\365\121\317\345\211\201\307\74\332\122\34\314\307\304\272\375\75\53\303\253\343\347\170\271\122\53\112\122\213\362"
  "\24\62\363\112\24\122\213\212\362\362\255\121\152\216\40\140\121\37\357\353\30\1\144\0\325\303\125\47\347\347\25\227"
  "\50\44\147\44\26\51\150\305\247\346\26\247\107\43\253\324\66\214\5\232\2\0";

/* lib/include/fcntl.h (DEFLATEd, org. size 2242) */
static unsigned char file_l_13[715] =
  "\235\223\327\222\233\74\30\206\217\377\166\17\337\337\155\334\235\336\103\100\336\60\213\141\3\256\107\14\13\302\146\6\13"
  "\17\150\323\57\76\122\144\215\221\43\247\161\102\173\236\127\37\57\60\60\40\313\13\14\111\111\150\125\26\120\356\151"
  "\136\222\32\132\361\165\225\247\33\234\266\301\30\374\361\353\37\277\376\275\257\342\315\56\206\222\44\230\235\345\44\51"
  "\156\122\14\217\353\167\365\200\276\333\343\272\277\175\172\172\75\111\161\46\256\263\73\354\70\47\30\374\310\274\272\102"
  "\236\15\154\153\15\337\16\305\66\152\3\260\121\236\300\322\14\35\230\330\23\327\274\10\45\151\14\232\272\35\256\75"
  "\13\124\175\254\325\5\251\332\236\357\275\160\175\353\262\151\337\326\332\222\74\11\10\64\313\337\327\6\4\232\345\205"
  "\254\330\243\241\326\326\310\126\200\314\231\224\205\72\124\145\137\270\237\301\307\217\107\143\26\0\112\167\116\200\254\231"
  "\37\254\145\302\130\237\160\4\105\212\62\6\132\131\256\362\14\267\317\204\160\120\343\317\202\271\332\340\175\275\57\100"
  "\115\200\347\117\174\327\365\227\42\200\227\300\3\324\21\121\243\346\261\206\10\154\337\163\327\222\270\255\41\102\144\6"
  "\326\113\111\334\327\20\313\240\221\61\32\152\127\131\6\207\71\344\222\37\245\247\200\246\145\115\175\33\161\120\114\377"
  "\121\310\37\345\34\214\76\362\127\176\350\254\242\211\151\57\130\27\301\324\164\305\22\152\211\354\256\143\41\11\30\3"
  "\255\36\242\127\163\344\315\34\106\264\106\72\135\2\347\43\2\323\263\375\251\230\140\254\213\20\300\71\175\351\270\256"
  "\207\220\315\365\133\72\135\2\347\2\130\253\63\31\160\133\23\40\1\21\240\257\20\315\103\4\74\340\216\56\100\2"
  "\334\77\46\114\242\13\64\233\330\274\267\346\305\120\134\34\267\117\111\227\77\337\51\351\362\231\225\124\73\262\134\237"
  "\177\1\54\130\271\143\316\42\304\76\23\24\206\160\330\304\227\331\126\220\160\75\165\35\357\362\370\217\60\144\244\107"
  "\4\40\122\306\52\22\240\251\277\100\266\23\64\26\22\163\262\162\32\334\304\266\226\354\141\173\374\275\367\172\120\341"
  "\42\246\70\5\43\246\220\335\220\204\346\45\211\213\234\276\203\232\346\105\1\273\274\256\163\262\71\24\211\337\122\134"
  "\21\310\11\205\162\217\111\53\51\111\115\41\331\306\25\30\373\230\156\273\342\126\126\304\233\272\375\350\13\76\246\55"
  "\176\230\245\135\370\1\63\113\10\55\16\242\300\222\35\73\350\367\373\47\340\276\254\363\267\121\26\247\257\363\32\113"
  "\201\145\146\21\117\316\152\114\345\131\201\211\110\342\154\202\317\344\24\105\231\304\364\233\111\314\26\55\173\153\347\307"
  "\52\352\302\256\114\161\104\177\276\252\63\11\111\205\131\300\251\330\204\215\301\141\350\55\56\366\270\2\371\356\41\53"
  "\53\210\257\153\316\103\302\52\250\37\101\205\351\115\105\152\66\14\260\233\275\21\60\131\171\101\71\111\243\12\27\334"
  "\321\54\52\216\215\3\300\27\377\4";

/* lib/include/fenv.h (DEFLATEd, org. size 780) */
static unsigned char file_l_14[317] =
  "\215\120\305\162\303\60\20\275\6\377\141\313\216\313\327\346\122\162\56\145\356\315\243\261\327\216\2\222\106\222\135\376\367"
  "\256\303\114\346\267\373\350\320\205\132\113\62\313\105\274\257\44\27\26\120\244\134\113\321\106\172\166\17\313\205\162\141"
  "\103\151\26\267\31\110\21\140\366\116\73\267\347\47\360\166\366\164\3\241\104\3\102\132\60\211\122\122\133\32\302\75"
  "\60\22\154\235\33\240\203\101\304\232\330\147\12\61\342\2\241\346\371\147\327\327\276\367\176\341\335\77\303\321\30\360"
  "\174\167\353\235\75\172\117\317\100\0\355\330\57\205\204\102\42\14\217\5\206\320\222\42\206\10\77\3\124\326\267\325"
  "\321\31\143\165\22\130\370\51\27\362\343\343\276\37\174\320\344\37\355\211\224\226\46\314\134\326\310\314\355\53\70\116"
  "\40\205\261\275\51\160\53\373\307\225\154\24\77\55\152\1\251\344\41\141\101\13\231\356\352\73\131\143\335\107\123\251"
  "\116\16\306\150\273\130\324\142\261\63\260\354\102\366\256\366\140\341\262\146\334\340\52\52\146\114\245\37\140\65\255\354"
  "\53\104\104\140\354\42\251\376\34\5\322\62\21\241\223\111\117\303\246\17\147\357\235\247\331\235\210\324\351\66\354\2"
  "\335\324\64\121\135\266\302\236\237\71\203\303\354\4\364\103\57\34\115\124\310\54\316\237\56\27\376\1";

/* lib/include/float.h (DEFLATEd, org. size 665) */
static unsigned char file_l_15[278] =
  "\322\327\122\360\314\55\310\111\315\115\315\53\111\54\311\314\317\123\310\311\314\315\54\51\126\320\110\53\320\124\320\322\347"
  "\345\342\345\122\56\50\112\114\317\115\124\310\317\113\116\5\363\123\122\323\62\363\0\145\220\5\212\105\61\14\105\267"
  "\122\164\264\217\150\223\240\343\356\372\321\207\165\377\113\370\256\55\316\65\16\267\246\233\253\227\361\346\361\76\241\36"
  "\152\257\227\157\77\113\103\271\65\46\43\302\170\73\371\110\14\336\171\113\3\201\244\161\36\337\66\255\343\314\140\47"
  "\275\275\362\20\10\117\366\1\357\136\126\34\345\100\332\361\221\264\306\216\317\73\153\205\107\255\161\100\147\47\235\273"
  "\201\43\355\330\276\56\157\36\47\211\346\162\173\105\302\301\302\112\60\262\170\41\106\65\160\324\172\306\320\203\45\36"
  "\4\310\211\245\24\166\45\167\15\107\232\207\375\356\160\371\366\343\373\361\345\375\55\321\100\4\42\5\44\110\201\221"
  "\301\301\265\146\54\355\267\253\254\202\261\253\53\30\1\262\63\104\315\35\311\146\34\7\14\202\40\17\125\60\107\322"
  "\232\301\356\332\143\226\101\123\11\141\5\47\42\67\5\13\253\171\211\75\3";

/* lib/include/inttypes.h (DEFLATEd, org. size 3738) */
static unsigned char file_l_16[557] =
  "\322\327\122\160\313\57\312\115\54\121\310\115\114\56\312\127\110\316\317\53\56\111\314\53\51\126\320\322\347\345\342\345\122"
  "\56\50\112\114\317\115\124\310\317\113\116\5\363\123\122\323\62\363\122\25\2\202\74\123\54\24\224\122\224\120\205\14"
  "\315\60\305\214\215\60\305\314\114\24\224\162\162\0\165\225\1\206\36\141\20\5\357\62\67\131\221\17\44\21\331\305"
  "\34\341\37\60\140\230\343\247\377\236\142\75\245\1\352\265\256\326\360\336\64\371\257\237\37\237\137\354\25\367\162\2"
  "\33\10\46\315\262\5\214\304\34\207\370\244\370\375\261\23\4\376\373\365\257\260\306\217\272\346\330\22\325\45\142\165"
  "\205\330\163\301\241\225\374\122\363\374\162\10\60\70\230\64\113\26\60\22\163\34\346\223\242\176\111\20\270\177\351\361"
  "\263\256\71\267\104\165\211\130\135\41\366\134\160\152\45\277\324\74\277\34\2\14\16\46\315\222\5\214\304\34\207\371"
  "\244\250\137\22\4\356\137\172\374\252\153\256\55\121\135\42\126\127\210\75\27\134\132\311\57\65\317\57\207\300\6\202"
  "\111\263\144\1\113\2\227\3\76\53\370\145\5\201\373\227\36\277\353\232\173\113\124\227\210\325\25\142\317\5\267\126"
  "\362\113\315\363\313\41\260\201\140\322\54\131\300\222\300\345\200\317\12\176\131\101\340\376\245\307\367\272\146\337\22\325"
  "\45\142\165\205\330\163\301\256\225\374\122\363\374\162\10\154\40\230\64\113\26\260\44\160\71\340\263\202\137\126\20\370"
  "\371\145\216\177\376\370\363\356\361\327\253\52\51\340\373\26\101\112\60\230\12\20\316\73\265\232\44\366\107\142\11\301"
  "\144\132\223\150\341\61\307\142\76\111\324\347\140\367\171\363\243\357\71\162\270\33\135\220\62\14\246\42\204\363\123\255"
  "\46\211\375\221\130\102\60\231\326\44\132\170\314\261\230\117\22\365\72\330\275\336\374\354\173\316\34\356\146\27\244\24"
  "\203\251\20\341\374\124\253\111\142\177\44\226\20\114\246\65\211\26\36\163\54\346\223\104\375\16\166\277\67\277\372\236"
  "\53\207\273\341\5\51\307\140\52\106\70\77\325\152\222\330\37\211\45\4\223\151\115\242\205\307\34\213\371\44\121\317"
  "\203\335\363\315\357\276\347\316\341\156\172\101\112\62\230\12\22\316\117\265\232\44\366\107\142\11\301\144\132\223\150\341"
  "\61\307\142\76\111\324\367\340\357\276\377\17";

/* lib/include/limits.h (DEFLATEd, org. size 635) */
static unsigned char file_l_17[230] =
  "\164\120\63\202\103\1\20\255\127\167\210\155\227\153\306\256\222\174\333\274\375\142\366\33\123\215\37\332\265\314\207\40\363"
  "\204\100\210\72\242\63\222\230\341\31\201\321\265\114\255\175\167\175\167\235\227\125\204\22\220\214\44\142\4\324\70\101"
  "\62\42\221\171\172\177\130\237\37\77\266\231\111\244\67\173\70\146\272\275\161\264\373\61\317\124\232\335\336\244\352\17"
  "\146\217\347\351\313\34\16\6\231\137\32\206\116\66\47\0\353\156\174\314\267\60\316\164\354\61\371\37\241\41\274\375"
  "\373\333\364\27\2\0\323\305\374\15\356\335\77\277\40\26\242\11\375\236\213\342\255\301\47\367\143\312\32\354\171\164"
  "\302\61\235\306\26\101\261\167\324\354\116\247\1\152\233\144\257\66\251\146\155\336\327\41\57\102\223\230\21\201\303\235"
  "\207\324\33\16\3\155\357\141\307\16\373\272\3\33\334\1\304\56\60\365\134\330\375\14\177\250\206\242\204\127\32\32"
  "\10\5\5\30\0";

/* lib/include/locale.h (DEFLATEd, org. size 215) */
static unsigned char file_l_18[154] =
  "\115\216\275\112\300\60\24\106\367\100\336\341\243\56\115\350\337\350\132\134\343\344\340\50\151\162\133\3\361\106\154\122\301"
  "\247\267\241\124\34\317\271\334\303\67\152\230\344\154\14\77\66\207\304\320\243\24\122\74\174\176\331\355\303\42\261\243"
  "\312\243\306\236\313\62\300\131\306\102\50\73\171\4\206\57\66\366\47\300\45\117\310\11\147\150\143\160\342\376\165\176"
  "\171\226\2\32\261\346\11\337\41\277\243\312\133\34\301\142\247\174\121\153\236\336\146\143\72\64\103\311\353\143\243\356"
  "\41\236\326\300\204\353\214\351\317\374\173\215\256\253\115\205\266\75\122\360\152\122\122\374\2";

/* lib/include/math.h (DEFLATEd, org. size 1519) */
static unsigned char file_l_19[417] =
  "\215\223\345\262\334\60\14\106\177\227\336\101\145\146\146\146\146\6\45\226\167\65\343\310\251\341\362\175\367\146\231\325\345"
  "\44\347\130\363\31\164\372\30\274\304\324\246\12\23\227\21\216\235\336\263\163\317\316\375\165\300\126\205\340\245\244\346"
  "\216\245\164\331\20\334\210\353\361\64\113\12\361\124\373\126\307\153\6\77\371\370\370\341\357\117\167\137\0\107\50\62"
  "\273\164\222\145\120\305\220\145\41\170\165\367\25\34\371\235\61\332\43\147\326\56\77\272\177\246\363\372\170\364\350\310"
  "\170\372\352\321\323\127\117\77\174\35\327\256\14\264\116\51\132\113\24\4\214\317\205\43\300\322\307\43\375\353\265\243"
  "\327\147\160\144\321\160\302\377\341\163\103\176\142\360\170\175\326\324\123\350\41\324\14\235\312\155\275\264\206\23\252\230"
  "\326\352\61\12\323\330\206\61\141\5\135\246\23\300\222\340\130\363\170\266\230\63\343\345\172\346\174\321\267\224\114\15"
  "\75\173\106\341\225\67\166\62\323\200\34\343\72\205\331\1\265\137\135\152\17\343\337\220\264\215\40\166\12\266\130\104"
  "\15\73\357\203\306\53\157\26\244\354\366\126\44\107\145\42\3\367\257\136\5\64\206\23\173\211\140\175\350\273\275\176"
  "\45\311\25\154\302\243\67\277\233\116\73\321\371\357\367\323\303\356\315\267\207\357\136\167\57\336\177\274\367\352\365\273"
  "\227\167\137\164\357\172\227\260\75\112\325\331\73\133\227\16\143\144\273\76\231\173\144\160\264\54\234\150\61\147\261\213"
  "\241\240\50\320\207\12\335\42\36\271\45\5\253\333\345\353\365\216\265\334\326\227\350\12\231\72\273\62\353\5\237\305"
  "\150\355\26\262\224\12\27\302\120\254\263\44\365\44\340\232\32\172\44\262\350\342\250\313\253\263\223\175\336\171\377\3"
  "";

/* lib/include/stdarg.h (DEFLATEd, org. size 583) */
static unsigned char file_l_20[267] =
  "\165\120\265\162\300\60\14\235\113\377\240\262\235\162\227\362\324\261\314\143\56\174\56\310\71\103\312\377\136\311\201\362\244"
  "\207\6\255\104\160\235\30\225\244\367\5\44\246\362\17\5\72\13\321\312\304\350\304\350\164\155\222\352\41\1\215\131"
  "\301\234\262\115\22\123\12\224\205\324\253\173\267\244\60\144\7\47\166\337\274\304\116\214\102\4\36\225\306\276\373\12"
  "\101\3\120\350\130\243\261\323\113\36\255\252\260\310\173\317\263\331\171\367\32\53\326\170\376\56\364\256\147\360\255\62"
  "\364\376\57\176\151\177\255\133\365\122\304\216\165\106\103\65\327\236\266\305\162\213\372\170\263\266\276\331\306\31\15\252"
  "\126\71\104\44\62\250\235\151\365\167\340\21\266\354\236\353\42\57\312\141\201\121\170\254\262\374\361\151\62\24\206\273"
  "\62\355\321\11\11\42\0\135\212\345\345\145\51\201\66\177\263\177\172\110\147\175\113\133\227\30\47\222\172\21\32\252"
  "\20\220\260\7\334\370\226\52\60\47\213\3\374\70\111\230\122\307\127\207\207\122\376\270\273\176\26\271\165\213\140\115"
  "\306\161\302\174\240\140\52\371\23\37";

/* lib/include/stdbool.h (DEFLATEd, org. size 122) */
static unsigned char file_l_21[81] =
  "\323\327\122\160\312\317\317\111\115\314\123\50\251\54\110\125\110\314\113\121\50\113\314\51\115\55\126\320\322\347\345\342\345"
  "\122\56\50\112\114\317\115\124\310\317\113\116\5\361\201\32\222\200\32\24\62\213\25\222\112\63\163\112\164\63\363\200"
  "\52\301\342\45\105\245\251\330\304\323\22\163\212\61\45\0";

/* lib/include/stddef.h (DEFLATEd, org. size 178) */
static unsigned char file_l_22[100] =
  "\323\327\122\160\316\317\315\315\317\123\110\111\115\313\314\313\54\311\314\317\53\126\320\322\347\345\342\345\122\56\50\112\114"
  "\317\115\124\310\317\113\116\5\361\201\152\375\102\175\174\24\62\213\25\222\112\63\163\112\164\63\363\200\52\301\342\371"
  "\151\151\305\251\45\371\151\330\344\12\112\212\122\62\323\322\342\113\260\111\26\147\126\245\142\227\51\117\316\110\54\302"
  "\42\5\0";

/* lib/include/stdint.h (DEFLATEd, org. size 2186) */
static unsigned char file_l_23[490] =
  "\175\222\325\232\34\41\24\204\257\143\357\100\334\335\335\335\205\170\62\332\302\12\73\322\254\274\175\252\371\226\324\64\207"
  "\141\174\372\57\212\52\372\234\77\245\236\231\355\142\254\266\314\270\251\225\261\115\121\25\63\325\354\114\212\271\72\165"
  "\376\300\336\3\173\17\117\146\203\152\175\240\66\354\250\300\77\143\107\153\156\134\250\73\133\243\172\60\73\127\337\153"
  "\65\260\301\322\111\63\353\65\312\314\325\320\231\265\346\254\261\336\1\314\345\340\313\167\137\76\174\371\324\173\373\362"
  "\135\216\76\374\236\242\72\203\221\165\134\224\306\26\12\212\47\57\237\75\363\133\160\267\4\207\13\15\311\77\277\374"
  "\371\324\103\275\100\341\337\236\22\44\155\365\33\350\206\257\136\71\230\267\277\157\167\340\265\53\244\370\3\234\130\355"
  "\5\153\105\152\375\305\153\340\1\343\137\314\57\137\132\344\370\47\367\47\227\1\334\156\2\227\56\340\274\3\171\306"
  "\1\77\322\45\134\150\341\226\325\160\241\207\23\105\144\16\131\105\226\135\37\154\57\133\116\310\173\214\73\173\303\17"
  "\210\122\47\316\136\74\173\141\373\172\171\262\103\221\27\230\264\214\70\322\106\274\24\32\354\57\64\174\274\171\163\122"
  "\46\302\324\101\237\216\3\6\224\316\102\230\16\102\101\76\205\146\14\210\73\76\232\61\74\22\20\61\10\375\103\167"
  "\24\41\7\25\174\150\171\32\120\266\307\307\203\24\24\157\266\353\266\240\200\373\106\376\275\147\17\77\377\37\202\60"
  "\17\102\21\156\42\143\304\66\157\236\146\174\50\11\23\305\341\222\232\60\125\34\60\251\311\5\142\352\60\112\341\166"
  "\246\173\101\302\3\114\365\312\373\260\27\64\234\322\164\57\200\320\53\255\311\6\322\121\61\55\3\351\250\231\116\33"
  "\261\132\306\51\352\246\23\335\164\134\116\313\162\24\345\103\371\10\217\117\214\116\52\274\27\126\363\172\27\370\70\351"
  "\5\151\340\243\355\136\77\141\154\163\22\277\242\165\344\316\316\115\145\213\161\44\362\311\203\146\155\303\126\252\375\20"
  "\116\124\321\111\105\162\124\377\7";

/* lib/include/stdio.h (DEFLATEd, org. size 3869) */
static unsigned char file_l_24[1054] =
  "\235\126\345\272\244\70\20\375\275\366\16\265\16\355\335\353\173\155\334\335\275\277\64\204\276\354\102\302\227\204\361\231\147"
  "\237\242\302\275\35\322\356\122\71\347\224\102\322\157\301\125\121\224\246\57\113\203\137\320\352\177\367\365\167\137\377\130"
  "\50\66\315\31\110\21\161\374\227\212\50\53\143\16\373\332\304\114\115\173\307\207\15\343\73\335\217\142\236\150\264\23"
  "\31\177\247\202\303\305\333\227\40\350\16\303\231\345\334\303\113\367\257\76\203\341\140\364\373\314\170\377\34\31\377\236"
  "\131\56\335\276\163\361\326\370\346\331\47\60\32\0\6\230\263\267\360\143\222\146\134\203\54\270\0\146\50\60\214\325"
  "\341\134\275\161\361\326\331\233\27\55\355\117\342\145\122\114\271\66\120\121\101\260\234\327\331\231\167\5\107\32\255\333"
  "\217\244\220\172\154\366\334\105\155\124\31\31\30\247\162\122\46\360\341\273\257\1\264\116\337\363\261\201\110\230\75\250"
  "\37\225\237\350\230\51\26\31\256\64\144\74\261\65\4\50\205\116\247\202\307\264\14\255\11\323\174\317\106\25\61\223"
  "\112\1\62\1\224\116\270\132\114\50\214\332\43\171\301\337\232\231\17\300\120\123\342\57\144\161\21\133\26\376\230\363"
  "\220\12\3\111\306\246\32\41\156\2\232\233\12\73\276\172\373\311\223\47\26\341\62\142\27\116\14\133\322\230\353\110"
  "\245\205\221\12\202\3\170\174\366\376\125\304\216\115\150\271\66\40\215\1\74\267\55\176\271\147\271\303\156\76\241\65"
  "\47\266\117\324\300\75\167\174\60\232\113\347\56\41\145\200\217\206\371\336\305\263\27\310\74\154\230\37\337\173\140\321"
  "\243\206\371\326\211\310\357\15\63\116\47\231\107\203\246\371\336\75\62\377\336\64\337\260\42\103\77\222\307\120\231\107"
  "\236\371\346\123\234\164\100\215\101\225\20\266\217\53\101\11\322\70\75\77\235\357\227\173\63\232\66\161\52\0\202\137"
  "\10\62\170\31\66\226\144\151\116\226\206\336\22\127\352\144\151\204\113\236\303\126\122\135\62\101\44\205\66\365\210\124"
  "\275\253\256\206\16\270\326\134\306\74\334\363\311\61\261\355\24\154\202\127\174\13\167\235\232\245\215\342\54\167\304\310"
  "\137\224\111\315\203\6\302\51\327\224\233\50\50\102\10\272\135\374\352\36\106\110\71\74\200\1\34\101\320\270\44\302"
  "\26\255\27\106\265\333\360\57\214\61\234\14\7\17\215\116\31\213\22\325\336\166\140\261\240\253\160\0\301\333\220\164"
  "\262\122\37\127\102\370\277\42\206\315\330\320\163\200\142\24\46\265\326\163\107\0\124\12\310\65\151\330\66\273\270\50"
  "\343\114\161\245\50\321\340\265\114\343\60\240\130\354\45\372\313\1\174\16\354\314\176\264\23\35\272\354\204\313\204\230"
  "\56\5\54\260\1\123\112\252\171\240\25\166\200\324\110\351\0\343\320\355\330\254\264\313\273\352\224\255\372\27\255\236"
  "\0\52\336\22\100\75\133\10\321\201\375\255\73\304\22\253\65\251\334\33\370\106\234\156\214\261\136\211\237\207\173\200"
  "\122\114\371\162\317\375\126\115\163\363\11\241\333\5\71\321\62\343\306\156\135\265\136\275\15\45\310\216\151\52\150\263"
  "\350\324\166\372\72\375\43\344\344\277\171\177\236\320\33\225\32\136\207\277\223\236\133\71\315\371\377\215\256\165\354\36"
  "\53\23\134\62\266\111\122\245\323\124\70\114\102\44\206\147\331\262\206\123\134\212\277\111\105\34\254\31\232\102\152\57"
  "\0\273\277\123\132\163\341\56\302\123\51\126\261\150\214\127\305\201\262\257\375\153\241\123\67\31\355\266\14\164\23\164"
  "\313\354\347\213\52\53\104\346\146\126\341\227\217\156\334\212\245\312\31\266\240\327\353\171\334\232\272\31\130\133\364\354"
  "\272\333\220\46\174\236\315\34\351\245\60\233\252\274\136\21\352\153\66\316\122\264\62\65\365\131\233\27\147\205\210\227"
  "\370\366\374\215\53\320\202\365\162\211\216\230\330\255\335\226\271\41\326\5\157\335\162\237\274\173\337\335\164\167\156\40"
  "\111\154\113\162\131\273\267\176\247\112\170\232\264\123\64\116\134\46\57\252\175\331\36\16\366\252\35\103\110\203\56\351"
  "\70\216\107\227\276\103\41\145\242\10\226\333\31\324\317\157\214\355\377\227\13\331\156\22\364\126\74\227\257\371\302\3"
  "\236\227\262\42\143\3\51\263\170\376\44\50\370\33\142\173\267\276\202\216\45\363\173\52\345\323\142\6\13\243\122\46"
  "\214\266\121\372\176\231\241\275\26\75\372\7\127\64\25\314\34\167\10\214\316\375\165\64\125\353\225\257\57";

/* lib/include/stdlib.h (DEFLATEd, org. size 1993) */
static unsigned char file_l_25[597] =
  "\265\225\305\222\343\60\20\206\317\103\357\320\313\216\53\260\214\227\141\146\276\245\144\273\223\121\255\54\145\244\366\140\355"
  "\273\157\144\205\144\55\103\320\356\376\376\277\305\156\305\260\206\22\65\23\120\20\27\234\70\32\210\133\163\323\163\323"
  "\217\62\354\160\211\260\162\276\161\334\136\135\330\330\76\71\134\201\27\225\370\321\311\322\322\312\321\21\74\37\307\167"
  "\26\333\113\47\207\355\235\205\163\170\15\175\377\202\72\215\367\245\147\53\206\335\223\355\155\340\6\222\202\13\152\160"
  "\151\343\43\345\341\302\356\162\251\173\176\363\256\343\136\163\323\164\333\303\176\36\14\351\42\45\270\7\56\11\56\13"
  "\105\165\320\230\177\202\57\220\361\253\66\175\372\6\51\224\354\372\250\370\41\373\55\301\110\321\212\301\360\73\154\123"
  "\265\371\66\163\235\136\60\35\244\354\50\342\15\241\226\220\251\42\21\10\214\124\47\112\225\64\4\126\1\261\251\175"
  "\32\61\134\222\5\370\167\1\333\74\113\210\37\23\43\54\344\52\315\61\244\111\145\76\125\37\134\304\50\263\212\263"
  "\343\305\367\371\172\331\207\204\31\254\52\47\344\277\253\57\244\341\135\211\331\204\107\361\127\36\236\221\357\64\62\372"
  "\246\213\15\151\46\263\350\112\361\311\261\261\267\140\312\314\250\220\101\14\220\70\145\102\250\64\32\254\43\131\207\301"
  "\225\271\253\242\35\215\30\71\121\217\164\140\224\173\106\241\74\326\350\210\221\305\17\152\261\104\151\52\311\140\61\342"
  "\15\167\31\210\342\116\41\323\232\303\252\16\45\146\25\206\30\25\146\42\355\206\263\213\204\362\312\33\151\311\162\254"
  "\271\155\125\12\157\15\141\356\21\151\236\325\240\321\0\251\250\217\300\331\302\321\6\304\255\112\77\23\203\114\247\27"
  "\3\241\213\175\306\333\72\114\6\354\64\326\141\156\32\276\61\360\156\242\243\270\6\236\207\157\20\164\370\322\330\41"
  "\373\137\65\334\340\47\246\34\122\31\154\45\233\261\127\141\312\375\214\201\200\52\17\63\373\353\254\135\303\274\155\356"
  "\10\373\67\250\121\167\56\36\64\244\306\230\307\6\2\133\45\117\4\312\312\266\35\15\126\300\222\272\116\243\341\251"
  "\32\367\256\323\72\374\232\364\72\45\225\47\321\10\163\36\66\76\6\207\332\74\61\266\216\361\12\231\37\127\362\14"
  "\372\264\255\146\306\345\234\324\367\363\14\6\53\336\272\60\1\202\47\232\351\133\110\30\27\252\350\13\133\71\32\303"
  "\272\30\254\364\166\217\111\236\372\17\217\116\116\165\150\66\233\326\367\53";

/* lib/include/string.h (DEFLATEd, org. size 1854) */
static unsigned char file_l_26[414] =
  "\235\224\105\227\343\60\14\200\317\113\377\101\313\145\74\356\161\57\313\14\163\353\13\170\332\274\306\162\236\354\342\257\237"
  "\250\24\122\231\145\353\23\127\335\6\374\166\24\341\30\46\36\206\61\377\150\164\237\75\176\366\370\145\102\336\130\173"
  "\140\60\120\54\247\212\337\376\176\371\2\221\5\177\26\305\355\10\131\221\217\155\264\126\43\267\277\160\333\33\106\324"
  "\322\51\102\230\233\50\204\206\126\72\110\126\265\255\20\132\327\202\300\240\165\273\133\113\101\153\157\10\353\357\252\254"
  "\66\163\265\203\213\344\36\313\101\301\304\243\324\244\43\166\310\102\301\341\356\226\2\211\300\14\51\352\237\164\343\71"
  "\221\221\35\110\312\262\203\10\35\160\331\164\122\53\324\252\137\252\335\100\54\35\323\326\121\106\113\136\5\175\23\307"
  "\5\200\335\25\344\101\25\302\314\213\14\311\61\356\216\330\302\362\236\264\334\253\13\206\43\230\120\261\102\55\340\300"
  "\104\52\153\105\106\361\121\106\211\361\5\66\301\262\166\136\14\254\344\42\361\151\172\242\372\22\162\135\130\67\105\225"
  "\276\116\101\126\204\234\231\312\103\56\353\216\110\236\362\335\167\103\152\243\125\56\333\17\102\13\205\374\143\125\312\77"
  "\247\125\70\167\244\210\14\325\330\250\42\102\303\212\335\6\40\53\171\30\172\24\202\77\163\51\244\265\341\5\126\315"
  "\52\234\45\145\137\320\355\302\217\357\277\77\336\265\340\375\140\130\105\260\302\144\371\310\164\141\254\163\13\263\5\362"
  "\200\137\156\55\175\325\244\305\51\257\123\261\332\310\345\226\323\321\336\62\126\225\77\47\67\165\341\45\265\154\161\211"
  "\53\253\300\370\153\105\146\117\234\334\154\236\125\347\367\116\256\60\325\255\165\221\5\241\310\154\213\237\17";

/* lib/include/sys.cdefs.h (DEFLATEd, org. size 191) */
static unsigned char file_l_27[113] =
  "\323\327\122\110\316\317\315\315\317\123\50\256\54\56\111\315\125\110\111\115\313\314\313\54\311\314\317\53\126\320\322\347\345"
  "\342\345\122\56\50\112\114\317\115\124\310\317\113\116\5\363\301\112\122\25\202\135\135\275\343\203\135\103\24\64\14\64"
  "\25\200\346\330\52\204\73\6\173\52\204\173\270\372\71\273\202\44\200\372\321\124\73\207\6\51\150\30\142\252\6\111"
  "\140\252\166\365\163\121\320\60\302\124\15\222\200\272\15\0";

/* lib/include/sys.crt.h (DEFLATEd, org. size 243) */
static unsigned char file_l_28[154] =
  "\125\215\1\7\303\60\20\205\1\10\371\17\247\203\266\120\66\300\300\60\100\67\146\30\63\25\351\165\216\345\122\311\145"
  "\266\375\372\265\205\154\200\367\276\357\356\65\65\204\304\102\16\241\307\201\230\204\74\107\250\33\255\264\132\215\301\334"
  "\235\1\317\26\163\162\276\117\17\204\302\6\51\26\153\71\104\70\36\316\373\123\333\265\273\13\154\326\132\311\173\304"
  "\211\300\323\123\17\145\355\131\60\270\156\110\154\73\251\312\271\255\266\132\341\153\252\31\376\350\157\212\327\374\366\226"
  "\375\110\37\314\242\365\211\45\303\145\161\6\304\106\260\44\26\210\142\44\305\171\120\253\57";

/* lib/include/sys.intrs.h (DEFLATEd, org. size 3110) */
static unsigned char file_l_29[612] =
  "\255\126\321\216\352\40\20\175\337\144\377\141\222\175\51\306\354\336\250\361\301\157\271\211\101\72\255\344\122\150\200\172\127"
  "\277\376\322\142\210\110\121\271\331\76\330\126\347\234\71\63\3\34\277\26\360\27\17\324\30\354\16\342\14\134\132\315"
  "\245\341\314\300\342\353\375\355\375\355\243\327\264\355\50\50\311\160\174\167\361\370\155\65\35\43\261\105\15\252\107\115"
  "\55\127\322\43\76\152\154\270\104\330\357\17\3\27\226\313\75\23\227\352\233\100\125\71\104\265\40\325\340\350\133\211"
  "\65\41\324\164\25\137\257\76\135\4\161\41\144\26\155\237\242\155\36\335\253\236\251\101\332\47\24\143\230\264\131\26"
  "\255\254\300\306\221\54\101\316\360\54\41\145\34\41\304\3\162\224\232\267\307\122\116\235\162\106\215\26\302\127\52\224"
  "\154\141\374\270\345\204\360\355\225\163\273\171\322\372\142\276\227\206\121\112\372\342\170\204\360\275\171\316\274\204\7\331"
  "\136\35\335\117\345\213\307\352\167\230\306\161\167\351\136\243\5\106\215\315\155\55\152\6\27\270\136\115\375\14\211\107"
  "\21\215\120\324\336\54\35\14\204\373\146\275\312\266\222\232\11\350\371\374\143\262\141\232\73\76\36\370\162\2\267\233"
  "\130\40\104\375\252\325\160\20\30\372\21\153\335\156\36\160\173\244\347\366\317\217\126\122\163\307\315\47\356\320\363\16"
  "\73\245\317\267\307\131\345\242\6\55\101\52\70\121\61\40\311\114\301\41\15\332\252\136\302\351\272\36\116\212\217\123"
  "\230\356\213\60\175\27\140\370\5\367\327\271\370\204\237\15\27\202\4\60\311\44\140\375\271\32\361\363\11\374\155\216"
  "\235\251\376\114\2\222\200\53\224\35\221\375\61\240\116\250\5\355\363\65\165\56\342\247\162\326\334\132\65\246\362\275"
  "\66\107\245\55\120\301\251\101\27\324\242\104\315\31\70\105\46\61\222\340\37\327\250\312\275\54\341\367\373\33\270\53"
  "\235\365\56\75\20\103\170\270\112\303\1\234\30\72\10\273\113\174\55\232\30\263\377\55\325\226\111\265\145\122\155\42"
  "\325\237\251\345\152\323\223\74\321\120\210\111\164\247\230\173\361\132\131\177\370\226\253\117\374\42\243\277\30\225\124\220"
  "\374\165\110\152\320\245\65\344\115\50\221\123\14\113\252\110\141\163\145\64\324\14\331\125\24\135\223\235\354\146\335\53"
  "\246\254\335\17\102\274\104\352\117\375\335\254\343\304\244\3\65\315\163\312\320\365\135\152\212\11\243\20\324\324\245\73"
  "\50\61\257\230\364\60\332\301\215\225\344\134\46\6\135\120\253\252\236\20\0\363\240\137\11\210\251\310\123\362\147\277"
  "\103\375\3";

/* lib/include/sys.stat.h (DEFLATEd, org. size 2073) */
static unsigned char file_l_30[634] =
  "\235\124\125\223\244\60\20\176\137\371\17\175\16\173\266\356\356\356\372\264\225\205\300\244\6\302\24\204\123\356\277\137\207"
  "\364\10\141\375\151\72\237\165\17\351\312\367\41\360\231\142\220\162\225\247\222\373\160\367\33\124\203\103\246\230\202\40"
  "\227\236\22\211\4\207\335\245\302\17\271\357\302\320\367\301\276\301\276\167\255\224\205\61\203\70\361\363\210\303\133\55"
  "\177\333\205\23\351\161\74\11\351\105\271\317\141\76\373\235\175\127\277\133\74\373\326\130\54\355\76\17\204\344\160\166"
  "\273\263\271\272\277\7\340\14\377\232\34\36\36\166\253\324\332\366\151\111\215\326\251\365\35\103\215\327\251\375\103\23"
  "\310\352\324\351\306\126\111\115\327\251\263\243\265\75\115\171\165\152\147\363\10\36\240\16\316\1\51\372\33\5\315\134"
  "\320\200\5\231\13\232\252\240\21\212\166\77\327\372\26\327\107\347\333\272\321\110\265\313\25\301\243\125\370\224\340\161"
  "\13\276\272\76\2\207\322\12\262\27\244\257\112\257\267\116\217\313\317\141\365\43\170\144\330\112\46\174\164\270\326\161"
  "\13\34\312\53\50\240\40\203\325\361\342\354\324\134\233\325\222\360\151\73\332\340\70\112\275\347\5\70\224\130\120\102"
  "\101\216\252\364\354\362\374\232\266\310\42\266\166\326\315\60\66\161\141\210\351\222\250\122\170\317\116\354\2\70\16\376"
  "\174\64\73\340\302\302\102\173\227\255\44\134\207\207\345\110\132\362\365\235\107\344\110\132\162\275\135\50\274\137\216\234"
  "\45\307\35\174\70\35\111\113\216\253\372\260\34\111\113\256\67\372\301\141\72\353\236\251\64\367\224\171\137\376\16\366"
  "\1\370\374\307\255\76\337\142\61\247\1\41\23\3\140\121\2\62\22\262\151\240\262\54\301\70\361\271\301\164\125\102"
  "\370\226\345\302\67\40\26\163\370\126\21\34\266\341\260\2\167\132\247\130\265\361\44\10\14\230\211\77\235\334\273\250"
  "\251\217\206\240\3\31\210\366\244\152\263\211\327\314\332\244\22\61\271\30\226\350\251\202\361\175\240\327\5\243\104\206"
  "\144\106\126\146\163\275\140\134\3\311\114\340\277\71\375\301\371\57\305\123\11\102\52\10\364\127\167\312\312\377\2\275"
  "\67\61\324\312\334\271\272\266\107\355\45\62\123\340\65\130\212\142\246\32\65\377\227\322\306\124\20\261\320\312\52\333"
  "\76\31\140\231\242\127\271\342\246\57\322\107\307\246\305\301\133\23\241\114\122\216\242\74\353\274\51\170\155\367\45\72"
  "\257\314\31\354\103\376\360\146\147\166\260\117\147\1\140\4\32\53\161\355\50\255\47\121\140\124\170\252\223\70\120\40"
  "\202\304\216\250\253\344\3\175\276\350\275\67\132\3\100\36\263\254\351\164\43\364\352\376\7";

/* lib/include/sys.types.h (DEFLATEd, org. size 414) */
static unsigned char file_l_31[150] =
  "\165\317\201\6\303\60\20\306\161\0\112\337\341\30\120\246\154\63\350\303\124\233\134\353\64\271\124\163\35\335\323\57\121"
  "\65\326\33\40\371\175\234\177\135\201\11\336\7\206\270\105\101\17\262\315\30\241\252\313\242\54\56\363\322\215\276\203"
  "\300\6\363\73\215\43\275\261\25\240\10\375\112\116\256\304\151\273\313\177\42\226\131\26\305\362\61\213\103\36\74\37"
  "\311\41\14\103\53\315\27\326\103\330\21\117\72\131\174\351\100\34\116\160\277\45\360\301\242\56\53\131\35\306\37\70"
  "\376\241\167\323\36\336\234\163\62\32\26\335\204\74\352\142\134\60\173\353\7";

/* lib/include/time.h (DEFLATEd, org. size 1301) */
static unsigned char file_l_32[457] =
  "\225\223\325\222\245\60\20\206\257\307\336\241\327\135\57\327\335\335\235\312\111\32\350\42\162\212\64\343\363\356\113\220\105"
  "\327\216\166\345\377\332\303\205\323\160\117\60\202\260\12\230\14\302\351\13\33\253\33\253\107\226\271\110\214\0\147\45"
  "\156\254\226\320\213\367\317\236\1\171\130\24\244\371\34\331\300\205\163\117\273\30\361\104\71\102\126\352\102\41\134\363"
  "\73\376\2\357\54\321\237\117\157\214\317\245\302\270\76\57\225\322\46\213\160\367\331\313\273\117\337\106\257\356\277\211"
  "\336\336\277\13\47\57\135\154\137\317\236\235\12\240\347\274\220\14\154\140\157\143\25\200\154\260\43\217\362\52\0\204"
  "\212\120\72\253\174\50\243\223\15\331\106\56\255\202\161\44\247\256\310\257\126\162\260\306\276\112\354\324\142\151\200\213"
  "\201\123\4\343\54\247\43\316\65\71\346\304\35\24\115\206\140\15\265\255\271\4\133\210\331\50\304\0\43\133\141\323"
  "\150\344\225\347\253\15\246\51\111\31\274\330\44\233\164\373\75\270\32\6\211\333\214\271\5\251\235\314\42\256\377\117"
  "\156\72\122\247\256\376\22\225\53\26\32\101\121\34\7\357\223\341\247\144\371\322\131\150\315\213\35\336\236\231\254\202"
  "\273\115\235\136\262\231\142\275\210\201\350\1\62\15\135\11\57\53\104\72\353\31\146\243\165\160\37\235\213\331\367\117"
  "\314\177\300\332\111\241\377\302\67\217\101\160\253\347\124\327\344\317\266\202\21\333\176\367\54\324\21\152\61\66\334\36"
  "\114\133\253\356\261\106\311\250\340\325\313\267\217\77\235\323\224\41\10\245\210\251\364\251\326\330\344\16\53\3\336\365"
  "\310\315\366\302\356\163\324\204\36\234\205\167\137\0\355\46\154\212\234\104\330\145\347\331\24\302\273\126\30\374\172\371"
  "\173\327\216\166\315\165\331\165\26\377\60\302\50\237\14\345\354\77\54\152\336\261\321\27\105\74\177\23\72\257\121\212"
  "\131\317\311\12\377\243\336\237";

/* lib/include/unistd.h (DEFLATEd, org. size 1529) */
static unsigned char file_l_33[453] =
  "\322\327\2\124\116\125\133\163\302\100\370\272\366\16\123\7\352\356\275\251\273\313\35\47\44\3\344\154\10\234\144\350\376"
  "\364\351\377\260\32\262\356\303\147\23\5\113\114\13\146\4\330\256\312\152\45\71\360\132\367\40\131\160\14\120\327\240"
  "\205\210\145\106\212\2\105\14\311\215\63\47\317\234\74\337\30\126\124\14\152\315\321\75\111\315\125\53\20\236\332\316"
  "\336\230\130\256\227\317\103\234\13\314\247\270\143\134\55\65\302\217\237\57\337\175\116\137\277\373\370\352\363\27\200\350"
  "\146\74\240\276\374\372\71\347\242\133\103\352\325\367\357\13\352\266\107\275\116\277\174\30\346\374\165\310\320\376\307\41"
  "\103\327\367\11\162\67\356\207\206\107\204\106\203\265\362\77\246\4\6\231\210\244\46\310\305\125\370\127\113\1\111\326"
  "\346\127\141\106\353\370\311\212\145\154\44\341\334\63\135\317\75\235\315\256\156\131\107\170\25\352\74\117\251\377\265\110"
  "\353\122\166\17\140\167\330\24\126\26\161\264\110\362\245\127\241\107\307\45\152\216\236\155\42\45\323\152\316\10\3\243"
  "\102\135\120\31\210\271\252\355\134\30\120\322\62\242\156\225\233\116\305\146\146\62\262\206\114\112\116\317\343\47\160\43"
  "\351\13\260\145\335\52\1\31\2\203\252\125\44\33\205\156\14\160\377\356\7\110\156\14\172\60\316\321\332\150\272\106"
  "\274\144\6\222\206\121\171\165\112\126\265\10\47\327\152\45\365\210\221\277\274\353\254\224\53\126\254\365\256\64\13\124"
  "\136\176\255\104\330\302\101\313\56\32\307\41\357\240\35\243\360\307\340\107\256\315\11\274\266\253\146\303\363\265\124\320"
  "\64\140\335\232\50\75\332\34\264\66\146\207\335\277\231\73\367\142\132\373\307\336\325\175\265\45\60\72\60\306\233\226"
  "\251\204\64\133\6\24\310\27\226\150\207\60\27\214\230\355\64\137\177\123\362\220\362\56\112\212\107\162\72\40\113\214"
  "\132\333\363\307";

/* lib/include/wasi.api.h (DEFLATEd, org. size 48950) */
static unsigned char file_l_34[10409] =
  "\234\130\327\172\342\310\22\276\76\351\35\352\344\350\100\234\164\205\101\266\365\15\40\126\22\223\156\370\32\251\200\336\221"
  "\272\165\272\133\170\331\247\337\356\22\321\43\230\74\116\252\277\342\137\101\276\371\17\274\357\105\76\364\46\376\153\130"
  "\31\123\350\327\67\67\113\156\126\345\374\72\221\371\315\173\234\367\264\306\174\236\155\156\234\340\15\374\347\346\117\277"
  "\267\250\361\335\153\60\53\256\301\376\373\247\102\226\375\163\257\10\376\245\21\141\66\173\142\232\147\174\236\314\334\343"
  "\353\344\337\4\375\323\357\377\132\50\266\314\31\344\62\55\63\204\277\70\261\231\26\254\320\53\151\146\205\302\65\307"
  "\247\306\137\16\202\122\44\110\300\24\27\134\40\14\374\260\37\4\157\175\157\26\305\275\60\206\333\351\320\76\46\247"
  "\244\270\22\270\144\206\257\21\26\74\103\320\374\127\4\251\40\103\261\64\53\220\13\140\240\160\311\245\200\47\156\375"
  "\27\300\110\360\232\234\63\233\2\255\21\50\271\60\335\366\314\320\43\247\142\146\336\154\115\304\74\107\155\130\136\200"
  "\305\12\46\244\306\104\212\124\237\121\140\166\342\7\15\176\212\302\360\5\107\245\141\41\25\44\231\114\76\327\340\133"
  "\115\213\247\207\74\165\150\262\276\302\352\107\220\43\323\245\342\142\11\56\273\144\347\232\234\203\65\313\112\204\137\121"
  "\111\110\244\122\250\13\347\37\205\373\247\337\303\177\240\361\352\305\355\325\155\303\376\213\157\157\137\323\277\117\144\175"
  "\237\340\376\60\350\277\365\7\263\320\353\15\143\177\344\331\14\357\255\153\43\25\136\75\361\24\41\227\102\32\51\170"
  "\122\271\364\77\170\132\361\144\345\10\121\351\111\201\151\140\317\375\45\37\366\76\73\220\324\73\247\23\46\254\112\230"
  "\43\260\364\347\122\33\247\102\244\225\336\335\303\25\133\43\354\212\114\312\52\3\77\227\171\241\257\311\107\54\144\102"
  "\265\46\202\322\143\347\125\51\266\176\125\122\154\256\145\126\32\44\77\110\21\71\361\14\147\126\250\320\226\11\255\141"
  "\15\102\272\100\204\15\243\76\143\243\140\34\304\301\330\357\103\343\220\262\376\144\172\345\154\154\65\62\255\145\302\231"
  "\301\224\152\342\54\100\122\52\205\302\100\241\144\202\132\327\53\237\204\101\337\213\242\231\325\347\252\62\363\7\320\374"
  "\41\53\146\245\220\245\365\106\342\107\133\365\301\261\215\326\164\113\134\117\51\107\127\231\242\6\205\246\124\256\304\363"
  "\15\54\112\221\30\56\205\276\246\54\216\245\1\226\145\125\36\121\43\340\21\216\51\74\306\72\211\3\336\205\277\346"
  "\351\356\11\327\244\317\116\224\67\240\145\216\4\56\65\246\256\365\126\174\151\53\163\225\341\32\63\260\123\106\61\265"
  "\201\214\155\120\351\377\21\151\244\253\34\31\334\253\45\165\71\52\314\66\324\171\54\343\113\221\273\214\120\212\46\101"
  "\344\177\370\262\17\33\335\231\161\61\10\111\135\170\110\231\27\206\343\140\26\115\373\256\52\160\13\64\200\266\321\312"
  "\204\222\155\223\34\155\264\301\34\22\227\222\104\346\105\206\256\46\272\114\134\241\27\145\226\155\310\344\63\255\315\73"
  "\377\1\32\116\145\117\55\113\362\61\343\332\200\221\22\62\51\226\165\230\236\163\4\232\16\64\101\225\163\255\335\230"
  "\113\121\160\114\153\345\7\203\320\37\117\43\17\132\144\50\115\25\152\15\134\270\44\237\3\130\176\367\336\365\374\41"
  "\264\217\61\302\325\174\315\170\306\346\131\75\364\176\34\104\323\311\44\10\143\350\34\43\27\54\347\331\206\24\350\262"
  "\50\244\62\147\234\175\350\371\143\350\72\150\210\132\226\52\101\50\305\336\346\377\300\45\275\100\305\14\315\166\131\146"
  "\226\105\256\23\152\225\15\35\311\77\302\13\247\256\57\205\300\204\140\54\163\215\261\1\56\34\147\226\316\301\72\370"
  "\135\157\160\17\57\35\366\216\245\325\236\111\121\47\212\27\106\252\63\200\121\364\0\257\166\220\334\52\146\113\254\25"
  "\235\106\37\241\101\144\32\340\232\47\264\275\324\56\342\171\251\153\351\322\357\215\373\336\320\33\100\203\70\23\354\23"
  "\221\60\221\140\206\151\55\350\321\37\132\104\163\113\334\144\305\263\164\67\201\120\327\42\202\361\270\167\147\153\350\54"
  "\265\236\47\157\116\305\73\207\13\275\373\151\344\160\355\147\70\205\213\122\137\302\105\136\14\215\316\27\50\215\246\16"
  "\63\260\225\35\276\205\306\51\125\122\144\251\143\303\226\31\324\236\365\350\50\166\64\17\275\237\240\361\242\252\202\66"
  "\134\260\52\304\55\151\25\376\277\344\252\336\347\101\60\202\6\221\143\304\314\12\163\213\114\64\260\135\27\313\322\270"
  "\311\230\312\234\161\341\276\332\115\277\132\135\77\115\3\33\373\253\155\50\250\326\365\66\275\17\176\24\103\223\130\163"
  "\357\350\210\277\160\155\152\113\170\337\233\16\255\150\143\107\305\52\244\172\121\67\203\232\315\275\122\232\75\114\325\363"
  "\366\61\210\342\251\55\126\257\377\10\115\242\306\243\324\246\132\273\12\131\262\72\67\31\374\101\70\202\146\33\116\56"
  "\43\120\230\313\63\301\372\303\310\26\247\331\41\104\226\341\222\145\60\337\30\4\155\253\202\42\251\267\62\266\333\363"
  "\41\164\203\272\331\75\155\221\113\315\116\310\70\204\346\13\262\46\14\52\125\26\6\323\363\145\43\310\273\336\20\232"
  "\57\53\314\232\145\74\335\23\240\126\76\200\46\325\330\277\11\252\375\121\53\25\271\166\200\26\125\71\262\134\106\112"
  "\157\122\365\4\246\365\230\201\37\102\253\101\312\65\60\110\271\262\302\122\325\116\221\141\20\114\240\105\5\217\245\204"
  "\234\211\15\320\202\325\216\250\172\223\317\145\306\23\310\270\370\134\233\252\321\275\77\364\240\325\42\306\234\116\105\272"
  "\255\276\102\241\321\320\37\277\205\126\373\304\376\171\143\321\103\344\177\262\346\210\10\43\232\250\137\63\140\251\357\77"
  "\272\30\273\137\353\250\161\157\344\305\101\60\14\306\17\320\172\261\13\110\260\34\57\256\340\261\27\17\202\367\266\110"
  "\124\373\61\232\47\251\350\366\114\345\223\70\3\250\6\134\353\125\355\70\245\123\110\124\172\352\361\373\256\153\337\36"
  "\331\374\132\327\215\251\124\355\306\111\256\351\155\307\155\121\152\12\115\207\113\55\70\270\233\336\107\320\336\155\216\171"
  "\271\130\240\2\135\260\4\57\234\1\204\34\170\357\240\335\332\2\165\231\254\40\245\65\127\57\355\215\143\150\267\217"
  "\245\151\333\112\165\211\310\4\374\340\365\241\115\324\360\176\301\244\64\316\243\12\274\220\52\147\346\114\237\21\170\330"
  "\177\13\355\356\326\52\275\235\175\55\52\142\156\373\305\127\131\25\214\274\21\264\53\162\110\3\50\144\271\134\125\211"
  "\73\43\37\75\100\373\125\45\277\73\33\252\313\232\272\313\55\41\160\307\152\75\332\316\74\113\342\111\14\35\342\306"
  "\104\111\43\23\231\175\345\134\43\150\64\351\103\247\261\313\74\125\66\303\205\1\51\56\26\54\372\30\101\247\132\31"
  "\333\1\371\225\323\216\120\61\115\266\116\13\166\57\231\273\351\106\340\13\23\216\300\156\304\165\266\44\61\307\63\316"
  "\361\204\235\316\55\60\362\130\342\214\106\157\64\211\77\102\207\350\63\330\311\222\57\230\27\346\34\52\364\372\301\73"
  "\57\354\335\15\75\350\20\175\42\303\14\22\316\352\220\153\124\347\363\35\107\201\145\135\347\305\41\214\52\11\347\244"
  "\247\23\350\354\170\164\110\357\351\31\174\232\172\220\342\262\116\27\162\265\206\4\53\12\45\13\305\231\101\132\112\211"
  "\24\106\311\354\240\272\126\305\7\77\200\356\355\227\275\15\122\135\272\63\134\322\356\207\301\173\350\22\337\336\235\156"
  "\12\127\261\371\366\367\16\364\346\227\62\303\316\162\336\16\137\57\164\47\40\164\233\25\345\255\7\262\324\140\347\57"
  "\272\251\121\317\242\211\27\216\240\113\374\13\116\262\127\240\312\271\61\147\120\376\304\203\56\61\357\116\311\317\50\240"
  "\340\365\156\121\43\102\267\163\334\205\347\47\20\111\37\336\227\272\335\143\330\327\373\211\340\361\107\347\333\213\23\344"
  "\223\222\142\351\122\107\103\360\2\31\302\336\370\301\242\137\156\347\131\231\231\313\213\65\14\356\43\350\156\317\124\226"
  "\136\111\221\375\106\253\165\60\71\252\303\340\277\342\151\327\353\366\66\205\113\330\62\227\115\156\10\271\62\215\170\301"
  "\331\60\313\102\6\274\355\225\377\376\44\53\236\7\261\301\311\365\12\370\373\44\133\226\144\131\24\115\72\202\310\130"
  "\315\336\376\207\172\222\124\11\161\143\375\66\200\370\266\377\261\156\131\266\112\211\376\72\364\6\200\274\345\362\306\130"
  "\340\350\217\46\41\333\137\75\312\140\35\45\301\124\335\72\354\173\370\151\14\147\107\312\123\304\43\125\11\133\217\146"
  "\337\61\342\355\323\361\245\54\252\352\55\355\11\345\215\332\274\241\367\105\71\221\375\75\212\137\122\344\170\224\77\142"
  "\75\276\340\127\151\226\312\124\340\71\275\202\270\233\306\251\316\52\261\112\143\46\136\45\24\112\144\365\6\36\111\264"
  "\145\254\135\55\13\152\174\131\165\271\345\117\260\303\320\324\61\64\212\244\245\224\111\100\265\112\350\134\320\63\334\241"
  "\151\176\17\33\200\115\147\111\204\373\263\172\312\343\51\225\203\56\146\154\272\340\162\36\141\136\61\105\237\136\11\371"
  "\6\6\304\331\35\126\204\244\11\203\343\32\203\260\74\203\330\263\214\137\127\107\107\11\301\67\46\57\270\70\73\17"
  "\307\321\151\77\352\173\241\67\376\61\354\261\27\57\264\310\57\137\174\144\47\47\354\303\313\227\235\222\343\341\177\252"
  "\212\107\123\334\33\360\377\370\276\246\205\32\2\364\360\45\32\352\132\272\20\360\2\221\133\5\306\132\204\51\354\107"
  "\207\260\112\4\254\144\202\24\70\57\54\205\332\22\332\105\135\116\51\262\254\225\167\354\373\237\115\336\55\7\357\54"
  "\251\44\227\100\57\43\265\34\155\360\360\13\66\142\10\54\141\164\72\360\316\306\46\325\266\203\352\317\132\121\211\360"
  "\264\334\353\232\226\335\254\166\326\131\51\170\100\216\213\263\7\216\325\115\56\225\320\224\317\316\146\240\316\262\62\216"
  "\205\201\12\153\133\231\24\30\362\136\244\357\304\273\67\154\372\60\307\303\355\321\121\174\127\222\56\313\141\252\324\377"
  "\122\105\176\131\50\14\315\355\62\200\320\37\14\114\165\166\35\352\74\224\251\24\365\155\122\211\74\371\75\333\204\240"
  "\333\304\375\26\134\204\276\51\357\236\103\136\236\334\247\125\73\252\327\377\172\61\266\300\356\273\140\263\254\210\271\354"
  "\0\36\100\111\335\263\111\174\320\1\115\6\33\227\2\240\243\104\347\236\166\222\57\136\170\36\365\2\37\110\42\110"
  "\202\375\136\70\12\176\230\164\207\104\327\266\207\254\253\141\154\233\142\271\101\224\144\156\161\360\170\151\361\151\37\234"
  "\232\143\130\44\116\332\36\53\321\214\243\365\320\326\326\225\75\75\107\355\62\341\351\54\32\217\46\101\317\46\323\307"
  "\337\50\223\304\14\111\256\55\123\350\5\147\176\150\221\311\352\204\315\265\151\207\37\175\361\207\26\334\355\65\42\40"
  "\10\337\31\252\300\322\54\310\73\116\211\21\33\147\262\103\152\204\307\211\261\340\357\272\361\125\171\306\271\116\33\330"
  "\115\340\143\31\250\335\162\366\176\233\114\353\333\15\311\324\156\71\373\116\231\120\4\25\302\201\261\143\55\160\7\253"
  "\0\156\347\261\372\260\170\316\363\153\261\274\122\177\136\321\355\373\13\165\205\112\207\371\246\10\224\105\340\107\323\227"
  "\233\207\171\267\257\222\345\135\36\273\125\244\34\5\213\211\26\75\17\327\237\117\124\106\335\367\257\113\211\7\220\261"
  "\45\3\163\345\251\356\25\74\355\73\326\157\313\225\136\232\113\324\116\344\234\305\55\107\122\271\366\34\22\235\173\6"
  "\267\235\253\6\125\41\207\63\32\377\240\202\264\211\356\366\165\164\175\261\126\374\16\374\313\321\327\316\370\275\345\366"
  "\175\167\71\52\243\246\261\203\152\62\104\205\132\142\363\26\171\63\43\207\103\267\275\366\116\54\262\54\52\162\1\211"
  "\351\24\237\127\167\127\350\351\256\260\210\243\343\213\75\125\244\254\362\267\361\20\232\175\46\106\203\201\16\145\55\31"
  "\345\126\247\23\245\234\167\176\47\261\246\157\347\300\132\136\64\76\237\320\145\200\211\257\234\247\142\360\214\210\0\56"
  "\64\131\355\107\322\335\100\63\152\4\242\221\272\217\251\230\261\133\161\13\106\106\105\235\230\113\51\312\367\327\34\335"
  "\56\303\11\257\232\140\25\270\305\130\262\24\14\64\146\177\303\132\250\43\376\1\300\277\202\102\376\61\43\275\165\301"
  "\116\27\233\251\306\117\305\70\224\131\337\51\303\160\152\220\302\57\242\114\344\32\100\267\134\271\306\377\113\222\154\252"
  "\230\132\141\273\146\161\115\265\270\310\53\271\271\202\210\56\105\276\261\206\46\0\251\30\257\350\30\210\214\232\324\350"
  "\10\327\325\213\126\157\105\113\104\46\171\263\2\263\50\252\224\56\304\65\244\54\250\276\203\377\100\154\71\327\64\44"
  "\251\221\204\64\371\364\134\321\161\123\221\151\353\376\166\356\17\173\76\72\136\106\227\220\102\334\64\170\53\311\113\371"
  "\266\230\275\325\152\30\143\173\223\200\175\264\216\325\315\111\132\41\353\160\177\330\147\133\326\341\42\117\352\304\65\133"
  "\232\11\300\215\361\243\346\124\64\156\12\200\271\174\42\307\24\316\165\73\332\7\130\370\353\74\235\245\344\207\110\75"
  "\232\104\43\177\64\312\145\360\76\56\212\233\124\64\327\113\127\137\25\306\64\211\40\77\314\60\163\1\222\54\101\354"
  "\51\2\347\162\272\264\146\173\137\40\242\323\110\104\257\25\376\52\121\246\74\143\371\335\355\225\100\16\276\274\202\117"
  "\341\152\134\133\131\52\53\243\70\153\312\237\346\105\142\223\35\47\316\160\132\105\251\36\331\55\11\337\340\263\225\366"
  "\51\14\103\130\245\206\260\364\171\210\176\362\3\153\362\340\24\265\61\221\126\67\71\270\140\174\204\67\254\351\114\255"
  "\64\360\225\305\55\303\373\114\202\240\116\60\5\132\261\152\41\142\134\120\162\76\246\54\237\6\350\272\241\144\173\321"
  "\363\311\120\303\166\41\310\270\52\165\247\104\315\106\372\302\103\115\136\13\105\357\334\13\274\136\350\7\232\146\153\3"
  "\32\114\261\113\36\113\121\272\251\352\111\305\366\332\34\215\155\321\5\36\370\147\223\201\27\104\164\223\274\1\76\372"
  "\367\273\214\323\213\116\12\214\243\176\30\365\317\2\357\222\355\156\244\2\227\374\272\344\267\372\222\243\23\177\34\6"
  "\76\20\354\155\102\200\35\47\157\53\131\12\47\307\217\313\117\160\77\334\213\124\62\271\257\71\14\300\346\65\45\316"
  "\112\303\221\31\276\312\26\371\310\165\104\240\72\105\277\232\17\142\340\152\304\243\244\330\145\104\4\174\265\312\120\273"
  "\162\303\136\314\125\207\307\110\106\205\14\377\322\300\115\17\104\370\165\155\21\121\26\272\301\323\256\25\253\273\67\106"
  "\76\262\65\354\302\113\355\54\332\320\264\3\122\140\370\217\143\303\323\154\54\343\277\172\262\233\376\267\50\165\316\302"
  "\125\203\47\133\250\254\45\147\252\12\111\133\212\32\20\60\304\131\335\45\176\31\257\72\113\257\217\256\42\32\216\202"
  "\113\157\360\277\247\344\213\5\230\214\2\323\375\311\64\34\345\277\126\11\6\371\373\53\1\175\323\151\121\322\11\271"
  "\24\350\7\245\16\211\225\166\212\152\333\240\124\6\61\264\134\115\374\141\170\1\344\37\155\344\342\21\1\311\224\111"
  "\161\23\170\331\240\45\301\74\262\47\362\322\131\361\40\112\262\105\32\114\155\275\372\211\125\224\300\33\142\217\335\326"
  "\117\213\241\22\255\222\347\11\10\120\224\211\50\255\64\337\56\6\203\241\357\367\331\366\257\20\321\346\102\137\167\47"
  "\357\112\141\145\352\217\206\241\142\332\351\142\242\130\216\251\142\366\37\171\327\335\335\270\256\334\377\116\373\16\114\367"
  "\356\341\326\164\157\232\267\335\353\363\266\305\145\375\122\55\130\244\154\144\51\122\41\50\227\224\357\36\314\17\243\71"
  "\103\30\24\45\255\67\365\225\173\145\22\30\140\6\203\1\246\22\76\340\357\35\27\314\364\216\242\177\377\150\127\4"
  "\221\231\101\216\0\172\127\163\234\303\322\225\64\115\30\175\314\254\53\333\65\276\111\330\151\6\42\256\331\45\263\312"
  "\174\70\130\54\312\272\240\244\221\162\77\173\113\203\363\25\233\231\130\156\5\246\362\376\25\227\31\264\57\13\171\35"
  "\54\122\145\35\237\374\157\341\234\72\77\370\342\55\246\344\206\223\161\43\247\341\31\51\34\104\13\317\62\266\276\44"
  "\270\344\53\272\152\233\332\153\7\5\302\44\230\372\135\171\351\33\337\161\270\167\330\351\237\353\12\321\356\150\243\104"
  "\253\236\272\206\227\236\345\133\170\240\342\111\302\131\50\11\60\270\210\320\14\211\126\111\60\176\345\77\341\242\243\41"
  "\105\356\277\143\65\27\150\221\100\120\342\77\134\32\360\321\300\374\376\140\13\42\202\32\151\42\36\326\270\241\223\215"
  "\232\36\364\273\3\336\60\201\163\374\262\4\214\342\60\201\5\101\203\373\333\124\116\100\325\315\322\371\265\132\56\74"
  "\240\122\363\316\274\354\14\1\117\243\236\306\34\316\301\64\363\233\256\153\355\305\162\110\213\15\176\126\234\343\372\0"
  "\233\71\130\171\344\24\43\250\52\32\46\223\175\203\246\370\51\355\6\266\136\46\256\175\352\302\277\57\214\13\3\34"
  "\341\157\210\37\10\210\273\260\243\254\113\252\221\151\140\266\366\322\300\166\176\205\0\362\243\271\265\363\345\74\343\13"
  "\110\253\106\340\130\4\133\173\344\311\106\220\21\104\212\305\51\157\342\361\270\7\45\170\4\167\31\211\241\26\1\165"
  "\251\351\345\131\371\364\362\151\56\155\356\271\121\376\203\111\236\312\324\12\351\42\174\345\106\64\224\11\221\24\246\257"
  "\111\275\61\165\166\41\111\52\304\273\27\266\26\126\143\363\64\356\115\144\267\142\275\314\223\324\206\221\356\0\57\12"
  "\160\324\114\274\126\143\303\354\224\312\166\6\53\0\340\221\111\125\61\34\215\35\62\254\206\204\257\353\372\322\27\215"
  "\61\215\312\270\16\173\114\316\5\311\210\3\143\260\362\254\166\340\144\145\335\335\337\67\276\361\44\332\77\307\47\54"
  "\201\275\61\227\66\220\353\22\2\170\333\71\0\341\146\306\311\120\23\116\243\333\337\247\304\63\172\267\156\22\136\74"
  "\236\245\46\362\142\315\104\110\330\332\31\37\236\133\221\144\276\206\44\37\7\110\362\162\367\231\354\112\230\217\303\204"
  "\21\351\376\236\236\352\100\43\32\221\304\346\125\3\323\306\125\163\223\321\256\133\245\201\271\246\102\150\126\322\302\121"
  "\171\305\145\271\350\163\241\103\340\65\73\315\4\0\140\252\134\307\224\102\225\323\235\304\72\272\326\230\272\210\217\327"
  "\17\76\235\364\364\13\20\205\331\37\126\162\157\47\6\272\62\221\4\117\176\246\130\151\274\302\236\47\225\101\313\225"
  "\364\346\152\172\110\275\201\354\342\123\157\106\323\54\232\62\4\276\42\213\243\77\323\317\141\222\360\262\373\271\65\351"
  "\151\275\67\266\42\140\165\77\36\66\11\112\73\36\232\24\303\13\70\314\121\162\244\60\71\227\4\371\356\227\157\76"
  "\50\150\161\140\21\171\345\4\345\256\11\136\302\347\111\110\47\107\247\70\125\233\4\303\205\13\217\250\231\127\246\55"
  "\260\324\141\371\153\121\241\23\242\22\355\246\315\62\126\337\106\316\144\314\130\116\145\221\267\364\343\225\112\330\72\174"
  "\113\363\271\167\126\210\20\217\365\147\150\317\111\3\136\102\201\135\173\370\153\274\62\270\204\320\154\200\110\361\204\124"
  "\266\63\255\11\17\321\264\75\173\215\313\245\133\130\70\133\303\26\342\236\242\51\273\325\250\225\150\352\322\204\65\155"
  "\354\127\250\355\30\126\347\110\147\164\100\140\334\17\203\42\376\151\242\333\134\272\215\210\301\124\347\151\257\63\60\243"
  "\105\136\72\366\147\307\275\377\3\155\372\127\205\123\127\266\117\44\175\25\342\136\135\150\300\130\146\172\25\324\221\346"
  "\342\237\225\106\207\134\132\203\164\151\62\271\343\236\120\336\166\60\362\25\320\222\23\27\327\64\133\223\14\152\13\323"
  "\163\21\210\311\226\175\144\13\352\316\73\244\274\106\106\132\13\13\1\147\254\326\323\1\53\56\32\213\31\167\145\77"
  "\221\204\163\165\246\330\142\102\306\10\16\147\143\264\345\34\232\320\317\146\31\73\256\337\175\365\346\205\140\37\205\146"
  "\362\74\113\136\232\47\32\215\163\366\50\302\27\264\277\117\253\162\56\155\133\114\2\274\200\231\110\276\3\356\162\324"
  "\15\131\334\24\77\311\204\140\315\261\153\355\345\45\222\363\143\326\37\230\261\304\160\276\170\240\71\117\315\302\114\155"
  "\167\227\230\67\165\173\260\171\113\114\335\313\114\254\374\201\367\207\314\357\342\151\5\47\363\175\126\234\304\115\253\235"
  "\257\351\303\17\123\156\157\344\10\24\77\126\131\266\222\172\317\371\37\240\105\325\320\301\212\13\261\223\114\220\4\106"
  "\107\147\70\62\172\256\335\363\237\175\40\373\351\27\177\200\350\141\123\361\300\220\323\276\211\303\146\251\263\11\72\114"
  "\260\45\203\225\320\272\115\60\225\243\2\375\173\213\215\103\103\313\330\32\102\121\154\221\265\110\151\74\117\63\154\246"
  "\171\0\242\54\242\150\46\52\337\350\142\262\64\113\114\125\71\143\131\112\100\130\111\126\373\60\306\100\122\311\41\371"
  "\371\152\235\220\274\154\272\236\204\234\254\172\115\4\111\344\337\163\326\26\260\73\244\353\115\375\204\202\137\51\341\77"
  "\274\321\323\204\100\45\214\71\125\200\217\340\276\40\154\311\44\351\272\36\55\371\244\115\32\212\143\142\224\5\367\114"
  "\21\61\123\177\2\124\314\151\230\17\172\346\174\367\263\216\160\321\214\106\300\25\257\361\225\175\22\272\271\314\136\326"
  "\115\133\206\155\3\37\142\177\125\261\220\311\273\71\156\343\10\153\360\217\26\155\160\24\53\71\55\353\144\153\221\331"
  "\3\233\232\110\212\131\251\135\115\13\304\361\245\36\223\215\200\147\6\310\113\135\16\151\56\147\13\11\104\234\57\336"
  "\24\104\217\112\323\156\1\137\373\213\45\142\123\152\142\244\117\262\350\224\72\76\175\215\343\51\210\32\377\327\361\233"
  "\243\303\57\47\207\237\77\205\143\353\374\340\365\61\52\246\354\355\11\115\66\23\71\375\123\42\26\74\200\224\26\64"
  "\275\303\5\355\302\36\224\312\61\231\55\204\367\302\173\163\151\310\300\42\141\2\260\261\31\160\121\271\376\252\304\244"
  "\24\170\262\126\115\53\324\135\17\301\163\332\324\122\66\313\253\114\200\314\351\306\112\124\240\206\330\134\211\53\17\156"
  "\122\67\306\166\142\376\43\67\2\201\317\36\7\34\114\125\272\151\360\162\7\237\57\357\21\236\106\314\245\42\51\171"
  "\163\260\161\373\56\44\310\224\0\0\202\0\147\154\315\44\272\330\156\211\145\300\76\330\141\261\167\71\155\64\270\304"
  "\241\63\344\327\254\205\5\2\145\147\270\13\206\20\226\51\125\134\201\252\67\162\376\314\210\307\242\173\314\53\41\111"
  "\152\122\233\123\46\222\67\265\177\326\7\273\364\377\345\103\47\265\2\201\341\137\305\257\7\344\364\46\355\360\63\106"
  "\217\347\361\152\375\272\54\373\321\126\235\271\304\200\11\174\226\251\1\204\150\307\3\67\371\121\316\20\146\30\74\210"
  "\255\353\235\303\361\161\311\246\110\336\164\321\376\44\77\220\224\366\21\53\53\146\266\277\37\37\350\311\53\2\75\117"
  "\70\146\1\242\27\326\44\267\320\34\243\332\316\11\27\1\174\114\324\64\111\205\240\357\156\155\207\372\104\331\145\131"
  "\227\255\341\264\173\263\272\71\204\375\131\336\202\355\323\346\52\274\45\20\152\231\354\145\155\52\232\130\141\207\275\275"
  "\16\255\320\13\376\33\176\360\224\362\172\171\121\120\227\10\227\141\110\50\17\323\225\163\123\123\135\21\154\315\311\67"
  "\133\125\173\13\133\344\376\170\231\344\234\34\201\53\1\257\55\224\314\124\42\344\361\341\117\237\16\76\220\77\350\335"
  "\112\343\372\331\324\227\113\57\272\17\246\35\162\16\117\160\141\60\35\307\100\61\111\222\140\176\76\375\262\322\201\244"
  "\127\146\271\150\306\12\325\135\40\123\21\216\354\145\17\162\225\371\22\54\337\5\363\157\116\17\175\331\5\0\225\72"
  "\42\266\306\266\41\130\273\315\363\3\25\106\142\63\227\231\226\317\56\332\322\174\133\64\226\30\270\65\273\221\365\344"
  "\350\300\47\173\7\250\137\102\73\124\207\370\56\354\17\136\123\146\61\200\372\356\200\31\104\211\124\111\313\50\303\230"
  "\343\60\15\107\65\260\355\142\227\21\251\262\221\17\177\221\112\142\165\111\151\331\246\365\247\365\274\364\274\54\276\305"
  "\335\50\377\336\47\23\377\151\200\376\13\133\355\106\223\137\320\362\375\31\1\141\21\311\224\140\72\147\57\166\202\172"
  "\172\174\364\42\173\21\166\227\144\72\63\71\45\72\161\47\310\307\357\176\372\112\25\237\6\47\374\162\327\11\277\244"
  "\262\120\312\143\113\342\235\222\333\371\166\125\67\376\11\256\321\360\14\133\305\20\207\320\107\322\142\6\31\337\57\170"
  "\313\35\124\246\235\207\363\171\67\6\376\160\364\221\212\111\151\241\100\100\276\147\123\234\274\43\230\274\325\336\350\172"
  "\130\131\267\202\122\344\344\307\131\54\270\330\2\235\73\266\136\226\305\146\44\170\363\63\225\334\342\175\367\206\373\146"
  "\45\225\50\241\303\5\152\40\203\27\200\322\316\245\33\246\7\362\101\45\331\13\336\157\307\276\141\46\175\5\60\236"
  "\53\240\151\46\73\371\374\205\12\132\365\204\57\206\26\112\157\3\356\344\370\304\203\343\115\366\332\114\277\135\266\315"
  "\262\26\72\323\365\203\152\154\160\141\313\142\113\340\47\207\237\250\14\326\6\300\161\217\333\26\372\347\123\237\70\303"
  "\307\244\365\267\233\13\123\27\67\266\350\256\70\362\103\33\155\214\324\360\330\160\167\234\36\121\265\55\100\247\142\216"
  "\101\31\252\354\334\322\345\142\132\226\360\132\355\300\324\277\44\140\57\171\327\275\27\273\375\103\100\176\177\374\167\331"
  "\113\336\202\137\155\333\55\271\252\147\113\176\66\333\356\10\366\353\11\366\366\113\71\362\232\231\255\150\315\276\33\64"
  "\225\306\170\357\63\135\130\264\331\272\150\156\330\310\277\341\376\75\73\374\104\5\316\170\137\121\270\312\242\161\316\372"
  "\5\337\155\76\237\77\120\241\60\106\24\301\160\63\143\253\145\273\43\270\63\137\252\114\366\126\221\71\251\75\271\233"
  "\74\364\325\174\250\336\130\317\204\44\152\103\327\250\62\11\103\226\241\326\366\154\102\107\320\17\60\260\124\63\242\23"
  "\205\364\153\344\114\321\62\333\116\334\36\274\201\176\237\356\257\323\222\264\155\157\256\133\226\161\322\15\7\37\275\173"
  "\363\365\374\313\252\250\301\200\303\266\116\104\356\272\234\243\267\227\165\147\53\256\111\132\125\312\60\201\355\75\105\160"
  "\207\350\70\153\46\161\166\160\170\162\300\271\365\151\227\52\150\251\53\241\156\102\313\46\246\145\252\363\276\324\102\303"
  "\244\111\155\270\50\311\274\300\156\327\170\336\237\325\274\251\164\6\174\256\224\267\216\232\24\151\7\163\232\25\244\24"
  "\0\71\352\103\322\247\151\245\352\54\212\174\146\0\7\15\205\257\52\160\314\317\227\16\125\177\221\256\322\240\240\301"
  "\220\245\121\370\111\7\271\140\17\327\250\123\127\213\324\45\110\224\246\45\265\327\22\312\127\77\306\360\255\165\44\272"
  "\235\137\375\226\346\57\114\67\24\366\166\274\12\173\43\132\271\144\340\140\2\52\21\151\24\344\331\21\100\246\271\47"
  "\56\41\275\150\113\17\217\170\141\252\52\304\244\161\346\266\332\223\167\100\17\237\60\204\201\150\201\57\107\357\50\247"
  "\174\225\126\0\155\161\300\204\363\73\213\26\336\321\310\206\251\6\336\337\367\243\244\55\131\334\227\122\47\141\256\110"
  "\7\175\313\44\21\376\15\32\120\334\251\370\243\24\224\163\144\140\107\231\134\213\360\230\262\271\140\27\120\355\125\370"
  "\227\104\152\223\121\232\104\224\1\241\142\122\337\45\254\124\2\121\314\123\275\41\10\1\75\160\332\172\304\57\323\146"
  "\43\75\300\122\303\322\131\146\246\40\333\356\334\324\305\223\212\126\121\312\220\222\154\220\164\47\120\145\105\132\323\266"
  "\346\316\157\34\324\111\365\330\117\257\140\205\350\313\52\17\307\41\53\330\41\7\31\370\227\267\10\164\147\117\15\15"
  "\205\227\173\275\14\303\307\376\361\165\256\37\341\311\271\117\236\373\215\137\175\44\23\307\130\153\246\216\31\163\275\351"
  "\277\16\63\303\157\71\137\264\77\115\272\72\216\125\216\21\146\300\256\223\372\344\64\6\135\357\305\261\364\164\20\105"
  "\241\302\236\342\257\307\176\116\213\256\175\236\337\177\366\102\360\344\5\52\353\153\333\66\65\246\160\155\132\113\302\42"
  "\261\76\121\242\242\33\133\42\6\253\126\51\211\3\67\113\254\24\277\351\57\26\77\114\257\127\12\223\35\226\53\5"
  "\146\375\32\2\330\340\350\153\361\336\175\371\4\155\211\105\133\212\271\204\365\332\220\350\334\63\223\42\366\115\12\10"
  "\323\31\305\247\150\146\304\237\11\253\35\244\232\252\336\7\210\34\145\263\254\343\347\71\107\133\140\102\23\40\271\277"
  "\157\311\330\60\221\252\354\345\76\242\6\20\151\156\347\266\62\55\16\156\0\40\12\170\34\120\246\10\366\306\373\213"
  "\165\22\243\11\304\321\273\267\125\62\73\203\165\340\312\54\40\50\223\344\17\203\266\212\364\332\103\5\324\305\332\333"
  "\246\310\14\250\221\233\53\217\234\115\262\216\351\65\323\56\77\131\263\215\251\105\275\107\310\25\215\40\344\132\103\21"
  "\352\261\35\111\250\307\0\61\4\302\234\3\256\251\114\330\236\127\371\252\245\263\327\345\43\161\265\211\344\120\63\206"
  "\277\215\76\266\220\263\173\20\334\12\133\273\231\102\323\103\263\265\76\276\115\26\344\13\263\77\234\123\251\54\51\334"
  "\351\142\337\325\330\62\121\26\361\355\371\14\360\324\62\45\51\57\65\242\210\350\354\316\122\344\213\222\264\45\222\102"
  "\171\44\256\324\314\221\104\123\272\104\114\135\0\223\17\135\152\70\355\175\133\260\4\104\301\344\374\57\156\47\311\144"
  "\374\103\350\216\300\276\151\211\61\270\222\25\157\352\120\353\125\222\321\67\44\64\303\330\204\324\334\162\204\330\106\334"
  "\323\234\126\36\115\165\7\372\232\266\64\342\346\132\115\243\110\123\124\21\352\15\105\40\155\317\201\10\134\32\47\7"
  "\232\151\132\360\310\121\16\216\144\266\100\124\11\377\25\326\215\112\254\231\24\144\34\235\314\252\345\300\174\176\102\220"
  "\210\216\222\305\154\306\50\303\322\105\146\305\1\323\230\33\106\363\14\220\275\247\72\65\357\77\74\232\310\44\163\212"
  "\7\271\51\111\17\167\312\347\37\242\134\134\112\340\112\15\206\33\50\237\211\220\247\337\167\172\362\246\345\230\374\364"
  "\31\45\305\16\41\220\143\166\345\127\261\114\213\343\362\201\153\374\65\226\355\271\151\66\255\273\212\11\165\114\204\312"
  "\3\344\107\343\213\32\27\154\34\330\170\122\116\32\142\335\15\5\0\2\104\42\327\210\376\235\246\0\322\173\66\45"
  "\1\220\207\245\205\152\310\112\56\115\327\360\27\1\30\132\256\274\315\116\356\73\165\323\101\51\253\150\337\315\140\366"
  "\102\10\220\252\271\310\337\177\140\103\160\310\203\51\30\350\70\375\320\156\204\200\2\55\115\300\221\224\253\174\64\203"
  "\52\171\227\211\267\144\215\52\356\30\173\213\215\262\355\356\240\76\103\373\3\0\107\167\210\334\343\343\111\147\207\34"
  "\12\152\153\112\356\162\245\123\263\104\227\34\17\20\51\275\12\230\154\321\240\142\16\203\31\311\215\156\54\330\305\306"
  "\117\256\124\111\254\21\106\220\40\371\241\0\373\64\75\344\276\24\55\145\224\26\66\212\330\222\0\325\156\113\274\250"
  "\323\226\42\142\323\50\175\4\367\217\202\333\66\172\177\256\200\36\144\27\266\233\33\107\365\23\12\364\226\142\307\322"
  "\345\136\362\133\46\331\156\330\161\54\45\43\55\235\213\212\334\113\52\24\353\365\322\41\35\260\56\220\70\212\201\223"
  "\347\17\337\122\306\126\17\25\203\257\307\364\213\70\144\230\72\15\255\62\40\246\26\366\203\165\260\161\107\5\226\256"
  "\301\146\264\1\365\105\254\151\225\116\55\365\225\270\272\121\366\330\377\160\3\27\60\61\55\41\32\102\52\54\114\250"
  "\113\154\236\243\147\144\233\33\277\204\353\173\242\240\237\276\27\12\374\165\172\241\221\305\122\72\56\25\122\250\225\221"
  "\65\132\324\15\45\254\6\154\335\210\150\135\264\303\222\225\337\75\70\36\43\163\21\143\152\212\211\16\126\370\332\132"
  "\7\152\301\313\212\261\145\324\310\166\213\101\265\205\11\331\101\172\261\220\117\327\277\222\237\5\260\315\177\345\206\4"
  "\52\133\357\110\251\3\66\100\132\100\335\141\127\102\34\151\163\100\153\313\353\324\326\234\376\17\331\233\100\164\227\315"
  "\71\42\201\307\326\355\301\345\350\216\142\264\153\376\173\305\350\10\165\243\42\67\266\164\114\357\370\302\161\166\125\326"
  "\352\353\205\171\326\45\122\32\374\376\133\54\73\26\11\100\216\211\143\344\43\135\376\57\300\273\67\356\323\354\235\361"
  "\144\212\213\21\61\20\7\50\122\44\215\343\321\162\200\232\65\25\325\223\201\371\233\33\354\357\113\205\65\136\332\253"
  "\246\102\34\363\140\351\40\200\352\225\203\263\116\276\366\205\373\245\113\340\150\134\66\107\165\167\47\1\1\271\377\325"
  "\161\271\233\0\225\257\232\30\136\122\266\343\342\102\30\317\20\46\141\34\150\61\340\345\313\266\271\311\154\47\211\165"
  "\5\17\56\171\230\123\203\332\53\277\357\360\265\16\67\367\75\251\337\314\42\24\305\17\133\225\370\206\107\22\133\330"
  "\160\335\67\273\10\330\135\227\55\201\55\342\11\156\264\201\242\362\33\152\256\270\330\127\241\234\214\251\43\73\76\267"
  "\44\250\334\234\57\371\165\161\157\231\304\261\315\51\206\153\367\253\357\225\332\262\361\61\35\63\143\117\11\212\317\51"
  "\337\61\277\137\33\122\203\26\253\232\310\105\75\202\330\265\70\262\236\341\353\222\135\341\307\310\356\75\350\232\271\45"
  "\46\241\150\302\105\145\246\11\133\125\166\101\57\303\42\205\123\21\371\21\151\131\372\166\131\112\354\167\207\57\306\314"
  "\232\51\175\101\207\60\300\2\71\63\53\273\273\34\12\132\317\355\42\231\353\354\323\40\170\210\35\45\377\274\165\163"
  "\230\254\226\50\62\204\44\12\231\124\142\312\41\32\325\264\27\266\153\15\63\152\150\234\147\225\245\102\254\36\322\313"
  "\275\107\23\336\62\374\311\117\4\264\160\154\42\321\102\302\257\135\316\371\105\301\172\15\170\361\230\122\246\333\231\271"
  "\60\365\224\1\213\311\220\203\303\245\340\40\100\61\151\114\47\375\241\162\244\144\10\23\307\261\131\202\20\225\65\24"
  "\212\304\163\163\374\171\145\202\307\270\26\326\221\213\305\220\307\105\250\221\335\370\71\161\21\122\376\210\42\61\263\245"
  "\217\336\16\157\21\214\71\264\107\22\113\3\371\40\107\272\164\353\32\305\230\37\175\233\250\342\346\326\247\167\205\57"
  "\110\214\235\336\345\315\372\2\247\171\234\371\305\73\117\265\36\42\15\215\237\42\113\102\350\21\154\242\267\120\104\12"
  "\266\246\114\323\144\354\321\267\70\105\50\353\144\276\14\113\212\261\362\17\61\260\304\142\141\304\166\154\352\2\125\214"
  "\166\64\44\157\146\104\116\32\220\43\203\225\104\5\175\67\163\300\46\372\74\317\350\143\66\124\135\366\321\10\267\310"
  "\300\77\210\143\360\305\25\215\376\370\132\211\72\263\75\11\104\51\31\131\223\377\345\152\306\20\351\244\172\212\331\334"
  "\56\66\377\346\233\232\156\204\146\311\257\260\14\10\2\64\356\51\73\241\143\377\300\217\350\165\345\247\363\170\110\325"
  "\45\300\124\255\212\16\140\377\347\36\75\177\204\376\33\330\176\267\67\25\272\116\323\343\107\31\214\201\334\132\223\361"
  "\206\45\204\272\25\311\255\213\212\10\145\122\65\110\171\45\342\225\222\115\34\223\51\144\103\243\62\346\66\213\225\17"
  "\257\326\26\126\160\261\117\356\272\210\154\356\335\210\263\23\46\337\377\211\213\201\30\111\144\237\74\330\172\374\277\66"
  "\144\213\260\224\62\100\143\114\105\155\66\343\50\152\51\74\224\65\125\161\376\203\370\10\260\143\136\342\217\372\0\102"
  "\164\150\1\315\4\373\20\230\210\205\0\172\220\215\370\145\314\112\364\265\145\102\111\70\127\235\4\251\210\51\134\127"
  "\61\117\134\51\234\276\100\323\273\363\330\345\42\237\237\137\173\316\310\222\246\160\45\260\61\256\374\54\171\336\360\273"
  "\370\314\101\265\263\101\321\164\242\343\210\142\225\201\77\345\173\271\64\255\251\273\222\317\176\114\35\105\200\135\367\44"
  "\134\247\313\42\245\231\241\263\4\222\303\43\366\52\224\313\340\102\276\36\47\16\246\103\266\60\140\250\162\266\174\223"
  "\51\112\337\21\312\66\352\11\140\335\214\163\313\371\2\215\70\312\327\326\105\171\133\272\234\114\46\323\120\132\3\360"
  "\254\13\1\156\117\202\206\151\75\210\145\325\331\47\101\365\53\213\140\6\273\245\45\35\241\5\300\305\264\120\146\221"
  "\227\217\37\377\301\213\261\275\111\104\30\77\267\343\301\265\274\205\103\134\354\50\4\157\335\321\115\357\177\370\51\101"
  "\114\25\157\156\271\213\157\164\142\324\171\242\316\107\326\373\24\44\175\160\146\242\330\367\101\17\27\101\375\342\116\66"
  "\52\346\165\257\334\132\43\22\355\36\306\266\266\144\67\224\120\2\221\34\325\35\157\372\204\367\346\144\50\63\177\145"
  "\120\54\13\25\73\150\322\326\217\131\171\123\266\74\56\103\3\113\112\345\147\144\36\222\42\211\0\15\113\63\153\234"
  "\104\171\24\15\266\252\52\24\53\271\374\14\15\243\136\224\304\47\314\161\374\202\360\176\114\152\361\143\6\206\13\245"
  "\52\16\213\322\327\2\132\145\120\4\137\217\320\227\341\351\35\327\271\262\232\345\134\17\250\243\201\254\4\124\14\15"
  "\47\43\61\274\210\134\304\362\255\147\62\166\114\330\356\273\43\73\362\176\110\15\377\342\247\303\246\373\330\2\37\227"
  "\276\334\304\103\42\347\374\126\156\22\130\33\305\312\313\6\336\101\371\301\343\14\232\123\324\346\356\43\20\35\350\4"
  "\347\141\267\254\150\67\111\327\145\354\335\210\250\273\241\135\170\304\5\3\113\135\254\306\212\226\247\2\234\360\1\172"
  "\277\120\261\71\234\17\130\274\36\125\25\352\260\336\260\235\200\154\331\301\11\177\210\314\347\366\214\305\224\45\277\160"
  "\266\156\125\343\257\212\110\64\327\203\353\304\360\350\354\240\74\265\350\151\272\315\60\117\73\301\365\105\164\374\244\12"
  "\120\376\57\135\114\145\363\10\166\151\354\37\372\202\232\145\11\25\147\73\21\350\33\157\256\351\160\143\211\320\37\223"
  "\20\17\276\306\262\50\73\151\12\62\271\37\255\55\234\102\312\360\156\114\212\63\353\12\333\212\54\43\20\351\317\20"
  "\355\40\317\236\217\11\261\370\263\211\243\362\113\114\345\241\323\203\212\56\224\202\20\365\46\243\317\33\256\276\21\246"
  "\352\262\256\34\350\235\24\42\164\43\347\64\267\132\147\225\123\237\122\124\34\315\35\323\105\227\172\250\107\45\225\360"
  "\365\361\74\1\50\250\32\327\202\217\34\341\150\200\216\315\122\74\44\257\33\242\172\17\25\75\16\222\300\342\352\156"
  "\314\250\275\166\43\307\256\244\302\353\114\370\254\246\314\223\212\334\145\124\375\222\153\103\201\372\317\127\346\230\322\251"
  "\30\10\320\275\143\120\42\117\1\217\76\101\365\224\225\2\103\272\21\104\104\160\276\262\261\310\72\126\113\341\224\300"
  "\42\242\267\362\253\366\26\356\272\261\5\246\172\116\123\323\153\46\123\325\211\200\32\63\41\271\324\252\312\132\77\13"
  "\355\65\42\127\273\131\225\217\351\32\335\233\321\102\100\202\307\204\75\236\243\47\253\31\117\300\1\66\150\250\321\161"
  "\121\375\54\2\307\65\156\31\21\51\236\105\77\172\313\72\137\64\255\151\255\337\113\167\224\55\300\205\75\10\310\116"
  "\110\70\12\60\70\7\250\21\124\124\113\302\5\253\25\173\172\360\221\243\47\377\262\64\224\73\314\366\13\371\164\114"
  "\223\31\276\215\246\134\307\50\123\300\365\310\22\45\22\361\121\74\163\21\344\224\235\317\313\202\2\356\103\364\211\344"
  "\31\72\17\335\116\255\357\64\70\221\304\320\224\16\306\144\364\40\252\306\153\242\171\346\32\114\205\203\112\120\55\1"
  "\154\242\100\221\126\105\320\44\345\61\17\21\52\110\154\342\251\42\167\273\323\243\101\336\224\45\361\342\302\225\313\242"
  "\171\22\100\252\50\0\256\325\26\234\165\360\132\101\121\125\31\225\240\220\232\12\37\50\151\77\70\67\204\137\42\245"
  "\30\40\362\7\52\262\6\271\341\355\137\337\121\271\244\200\221\372\27\320\156\244\124\314\350\105\165\252\334\175\24\202"
  "\201\62\173\266\13\237\260\341\264\120\267\12\52\21\151\42\354\25\154\126\213\112\362\210\127\352\333\334\324\165\220\264"
  "\34\63\167\377\154\371\64\30\345\323\332\163\161\60\13\136\244\275\46\111\55\305\52\176\164\34\35\117\153\113\167\44"
  "\367\212\75\222\374\130\107\371\174\274\217\152\46\45\117\320\103\124\370\124\152\261\124\364\110\145\27\263\54\26\162\66"
  "\365\306\114\202\52\34\343\114\2\25\67\146\22\260\343\70\223\210\353\171\234\113\272\326\324\156\156\273\156\340\6\42"
  "\205\103\166\346\206\204\167\172\300\71\355\166\142\10\227\146\10\267\41\103\70\141\10\267\206\41\364\312\257\252\226\360"
  "\172\243\142\10\147\74\101\172\110\275\223\61\116\220\317\124\217\234\132\372\223\326\251\145\210\253\254\200\33\206\353\254"
  "\144\231\124\56\41\363\57\243\6\344\310\354\116\147\120\313\125\15\162\16\304\340\224\56\123\331\113\316\221\67\216\77"
  "\102\27\356\61\147\7\307\207\0\356\350\260\233\236\33\347\312\266\333\103\207\146\266\267\112\164\175\224\375\305\137\144"
  "\57\362\354\267\274\250\276\245\143\176\272\254\114\127\62\344\337\242\211\244\1\4\46\141\0\377\331\332\131\350\66\0"
  "\303\100\364\173\312\14\237\63\346\211\246\342\327\157\236\225\123\144\225\162\147\321\320\117\266\303\247\64\236\23\0\174"
  "\370\232\46\74\75\127\61\254\31\0\56\17\112\14\333\220\53\0\324\356\244\63\341\35\323\355\247\214\175\174\215\35"
  "\31\241\141\242\275\211\340\102\357\304\226\235\317\351\313\223\144\154\277\27\222\200\333\160\42\303\147\115\15\241\107\42"
  "\266\246\127\376\341\135\120\247\52\224\11\22\174\300\163\116\122\46\352\317\26\321\361\200\302\23\260\106\323\123\116\71"
  "\213\322\251\100\317\346\42\70\365\374\71\335\270\27\36\101\117\161\60\313\265\215\0\302\105\32\201\201\127\314\171\104"
  "\271\332\313\117\46\376\64\36\72\256\275\42\264\273\377\171\367\175\360\327\375\341\117\127\273\154\76\233\242\131\31\363"
  "\345\34\341\63\346\153\304\336\154\275\21\143\337\210\301\157\304\350\67\132\370\46\56\165\133\175\367\355\166\265\341\355"
  "\333\41\357\237\62\76\321\373\374\337\157\344\270\332\341\36\61\54\154\76\373\136\124\36\171\325\110\276\35\365\54\57"
  "\105\232\227\321\107\272\64\224\55\236\336\167\71\326\251\155\156\171\30\4\251\113\242\172\265\52\46\160\140\75\156\277"
  "\104\5\377\4\120\355\242\322\272\0\242\75\46\142\210\145\135\340\323\205\35\167\277\56\371\234\306\103\27\231\152\260"
  "\160\311\5\255\52\43\303\335\30\272\171\261\347\167\266\335\334\365\371\151\54\263\36\13\153\41\373\145\325\102\321\266"
  "\32\11\135\45\51\143\230\206\145\322\67\162\57\263\274\270\251\167\340\271\12\63\41\312\323\65\343\106\103\322\102\232"
  "\266\216\206\143\125\277\176\356\226\366\357\364\11\3\253\15\132\42\11\373\376\224\356\51\236\45\304\320\110\103\173\215"
  "\60\354\3\105\156\134\173\143\165\255\244\314\30\171\223\344\262\266\350\331\0\274\105\303\265\377\73\143\36\25\334\165"
  "\253\171\324\157\33\355\243\172\333\156\16\355\226\42\104\345\166\112\4\0\341\226\112\101\224\155\33\255\57\210\266\223"
  "\45\311\162\363\31\351\112\224\266\132\10\20\154\351\134\276\74\11\246\366\153\262\43\234\326\152\11\202\175\53\1\252"
  "\40\226\134\20\174\13\102\244\145\3\320\346\42\10\264\44\341\264\74\113\60\242\70\113\63\330\70\202\60\113\330\327"
  "\262\54\141\176\344\207\360\111\111\226\234\221\32\24\131\202\130\40\153\215\262\51\234\161\63\7\152\54\335\123\240\305"
  "\162\200\33\224\130\230\377\2";

/* lib/include/wasm.h (DEFLATEd, org. size 22452) */
static unsigned char file_l_35[2302] =
  "\234\221\305\162\305\60\14\105\327\245\177\120\331\111\231\231\231\271\135\146\34\366\3\73\143\271\364\367\125\112\52\303\133"
  "\11\257\316\334\321\220\17\373\373\227\7\275\10\267\22\353\1\252\172\74\62\72\75\230\17\240\273\257\45\120\227\221"
  "\65\10\251\261\20\112\124\21\134\255\234\35\200\51\22\53\235\261\10\376\120\133\163\133\163\147\141\145\126\227\140\164"
  "\224\120\245\164\124\273\216\23\230\107\27\53\355\6\363\305\162\211\110\124\214\215\6\356\11\245\50\215\214\106\47\42"
  "\17\202\340\164\343\344\142\347\164\43\130\73\72\74\73\137\71\74\247\356\34\235\247\163\161\222\52\235\174\41\22\342"
  "\371\240\360\75\341\171\64\27\64\37\174\234\103\124\266\230\73\71\376\312\245\364\237\134\26\61\267\74\310\134\232\177"
  "\301\115\153\106\276\161\234\376\323\61\213\230\374\172\222\331\264\361\35\233\135\247\15\270\146\21\263\77\370\116\77\372"
  "\376\356\323\44\215\305\323\346\215\121\61\370\305\27\100\336\54\76\174\227\225\76\77\272\334\4\223\246\230\270\205\141"
  "\220\65\225\351\205\207\116\316\106\247\122\57\11\342\257\243\311\146\23\147\47\346\276\215\101\75\262\46\10\12\364\314"
  "\235\171\372\155\304\275\345\377\66\234\252\46\237\216\123\105\377\254\333\207\363\1\372\363\326\275\273\237\73\245\200\22"
  "\24\370\254\3\205\53\67\51\116\240\210\135\240\246\1\345\173\370\344\3\311\213\222\107\354\11\225\4\211\240\17\366"
  "\110\110\46\373\375\161\242\40\220\352\35\162\172\230\256\121\356\276\243\330\77\130\114\142\201\24\54\246\301\130\204\141"
  "\271\334\335\163\30\150\345\144\134\33\243\371\101\242\21\161\240\105\70\12\217\105\36\66\236\371\47\5\251\76\242\225"
  "\256\1\213\311\54\220\42\30\16\303\273\46\346\42\165\15\264\162\62\274\153\142\64\42\16\264\172\70\7\273\346\77"
  "\77\70\16\264\162\72\256\215\351\374\344\351\50\70\320\52\351\200\307\252\74\213\7\267\220\151\36\306\162\363\5\361"
  "\257\313\355\250\331\342\201\336\225\215\23\175\372\26\232\117\216\213\33\203\374\123\35\151\126\177\4\132\362\217\100\376"
  "\315\10\4\75\1\132\334\110\150\23\350\264\13\364\22\22\302\344\36\220\340\250\40\301\217\151\134\116\11\163\363\26"
  "\324\116\116\160\20\50\44\365\222\116\12\275\161\312\66\323\51\327\115\247\170\143\244\114\167\367\62\24\34\31\52\327"
  "\307\73\22\355\362\123\266\315\117\172\237\253\121\201\11\121\151\120\160\310\124\307\243\362\133\240\212\5\107\6\313\365"
  "\265\21\270\263\130\53\37\177\157\52\15\5\121\163\265\104\13\135\344\242\133\127\335\356\127\102\41\64\361\143\250\11"
  "\271\313\36\267\253\272\77\226\277\135\15\273\0\175\321\0\40\117\1\364\205\1\164\263\6\0\171\12\240\233\37\246"
  "\32\202\255\327\264\157\14\126\205\260\135\10\273\120\330\26\206\261\44\332\134\22\155\62\211\126\110\242\315\46\321\246"
  "\223\150\205\44\72\255\51\41\117\366\104\21\172\242\144\173\242\244\173\242\10\75\241\45\1\171\262\47\204\44\132\65"
  "\11\30\322\75\301\222\270\377\271\336\223\331\24\261\165\117\16\363\102\270\47\207\112\50\204\151\351\161\277\46\176\334"
  "\120\325\375\261\74\373\121\373\242\1\100\236\2\350\13\3\350\146\15\0\362\24\0\275\47\257\327\264\157\14\126\205"
  "\260\135\10\273\120\330\26\6\355\272\66\227\104\233\114\242\25\222\150\263\111\264\351\44\132\41\211\116\153\112\310\223"
  "\75\121\204\236\50\331\236\50\351\236\50\102\117\150\111\100\236\354\11\41\211\66\233\104\233\115\202\336\223\137\326\345"
  "\50\366\315\56\305\77\42\10\54\21\4\127\331\334\110\227\17\6\322\27\31\4\226\74\110\137\30\110\67\313\40\260"
  "\344\101\272\231\201\264\171\220\366\0\110\113\101\272\242\203\300\222\117\244\320\104\22\40\260\344\23\251\203\254\13\6"
  "\34\354\174\201\354\15\137\130\42\10\256\262\171\322\123\76\30\110\137\144\20\130\362\40\75\115\244\233\145\20\130\362"
  "\40\335\314\100\332\74\110\173\0\244\245\40\135\321\101\140\311\47\122\150\42\171\220\66\17\162\65\152\166\136\62\350"
  "\152\247\62\20\361\123\31\27\125\117\145\236\146\245\322\254\124\232\353\225\336\207\367\247\176\46\305\240\243\365\126\135"
  "\265\144\363\374\254\355\51\241\317\155\52\335\300\266\265\223\75\152\14\320\347\30\334\300\30\336\254\323\30\240\317\61"
  "\270\201\61\74\277\376\322\30\240\317\61\270\41\254\46\343\323\140\127\145\366\370\127\40\106\167\371\140\61\226\310\130"
  "\336\304\104\240\117\45\342\6\41\21\127\311\211\300\41\47\2\26\236\110\323\253\343\25\372\124\42\156\140\14\303\50"
  "\41\100\236\43\30\106\6\160\116\22\234\263\10\147\316\60\375\267\323\30\240\317\61\270\201\63\210\71\100\237\145\30"
  "\205\341\341\52\171\170\300\241\17\17\260\360\33\306\60\313\37\13\14\251\114\26\207\200\61\146\61\306\64\106\350\321"
  "\360\102\144\307\117\111\261\106\132\225\233\247\244\141\215\24\52\315\112\245\131\251\64\327\53\141\215\124\51\6\35\255"
  "\267\352\252\45\175\11\223\72\343\301\32\11\245\253\207\74\156\140\37\345\144\217\32\3\364\71\6\67\60\206\67\353"
  "\64\6\350\163\14\156\140\14\276\154\320\30\240\117\61\354\256\221\200\141\353\145\63\147\156\127\40\253\272\166\352\6"
  "\26\143\211\214\345\115\114\4\372\124\42\156\20\22\161\225\234\10\34\162\42\140\341\211\64\275\72\136\241\117\45\342"
  "\6\306\60\214\22\2\344\71\202\141\144\0\347\44\301\71\213\160\346\14\276\204\321\30\240\317\61\270\201\63\210\71"
  "\100\237\145\30\205\341\341\52\171\170\300\241\17\17\260\360\33\306\60\253\37\13\14\271\114\26\207\200\61\146\61\306"
  "\64\6\172\164\367\265\375\346\161\272\251\276\360\7\131\23\136\331\337\70\363\165\241\27\255\125\354\113\133\257\10\231"
  "\124\321\205\244\342\123\171\355\110\111\350\244\232\213\222\24\365\157\15\43\253\12\241\124\366\123\112\352\316\243\365\117"
  "\254\56\204\122\335\117\51\375\130\233\261\114\63\257\14\251\124\373\113\114\252\117\37\43\53\15\235\124\327\225\254\250"
  "\257\134\323\317\121\260\74\6\2\177\224\202\105\162\65\4\173\324\171\340\71\302\343\66\316\363\146\235\316\3\317\21"
  "\36\267\161\36\137\121\352\74\360\34\341\161\233\220\317\153\257\363\300\163\204\307\155\2\117\163\326\171\340\71\302\343"
  "\66\316\363\64\274\377\231\136\333\164\110\60\36\40\373\362\206\270\302\357\231\141\352\334\333\213\103\326\204\337\61\213"
  "\17\176\342\324\31\52\366\245\255\127\204\114\252\350\102\122\21\123\347\156\111\350\244\232\213\222\24\305\324\131\251\12"
  "\241\124\366\123\112\352\142\352\254\324\205\120\252\373\51\45\165\61\165\222\312\220\112\265\277\304\244\272\117\164\254\64"
  "\164\122\135\127\222\242\230\72\365\207\251\230\72\201\240\74\117\165\33\35\313\230\72\45\36\170\16\360\270\215\362\140"
  "\352\224\170\340\71\300\343\66\312\203\251\123\342\201\347\0\217\333\50\17\246\116\211\7\236\3\74\156\23\170\232\263"
  "\316\3\317\21\36\267\121\36\314\200\111\50\30\363\144\230\72\311\3\202\337\143\363\276\154\152\367\316\261\243\222\274"
  "\311\354\323\366\377\225\265\347\375\353\175\173\131\41\324\266\12\121\174\125\34\146\224\277\210\37\60\207\307\307\105\31"
  "\4\213\10\246\62\230\222\202\247\212\176\140\51\304\217\0\146\20\134\304\142\12\34\301\42\202\251\14\126\175\256\121"
  "\316\163\351\237\37\334\122\171\345\44\152\303\43\225\325\213\143\27\150\143\10\40\60\134\325\374\252\126\101\10\342\46"
  "\34\212\125\40\110\12\372\210\200\70\246\0\63\20\56\142\22\203\216\140\21\301\124\6\226\2\157\307\50\216\51\254"
  "\346\110\340\142\61\5\216\140\21\301\124\6\143\7\145\103\377\253\214\63\31\24\121\34\116\124\60\56\260\257\202\34"
  "\131\20\16\66\64\242\72\202\130\215\104\115\204\314\126\121\34\100\60\145\201\3\162\75\21\277\264\311\40\266\11\142"
  "\125\22\226\310\163\171\33\346\302\72\65\252\43\10\374\40\201\236\154\40\304\126\215\142\140\154\336\302\135\36\133\225"
  "\162\360\126\215\352\10\142\125\22\23\110\170\253\106\161\0\101\253\202\3\162\61\21\336\252\121\35\101\254\112\302\22"
  "\171\37\207\265\233\310\334\22\345\21\5\27\0\313\67\103\165\301\65\26\377\126\31\337\307\62\123\226\150\121\227\240"
  "\127\226\352\204\67\102\313\107\162\264\210\223\337\225\205\334\130\240\26\47\40\30\344\111\350\312\102\372\7\152\161\124"
  "\301\40\217\254\53\113\265\217\326\125\335\151\57\231\50\344\277\223\277\12\37\46\241\356\335\75\55\14\245\126\331\225"
  "\136\232\257\322\117\173\321\107\41\175\345\351\113\250\324\165\74\132\30\112\261\262\370\43\173\77\263\322\120\212\245\61"
  "\235\125\167\206\123\43\337\246\140\20\157\122\60\150\173\145\35\307\266\161\54\303\143\152\72\372\336\31\6\155\377\14"
  "\203\236\16\307\211\6\244\43\363\360\75\224\336\73\301\240\355\50\141\340\373\51\35\47\32\220\216\314\303\323\321\173"
  "\47\30\264\235\46\14\102\72\72\116\64\350\273\316\330\73\161\154\275\371\232\177\374\363\60\275\376\55\67\241\376\325"
  "\377\157\376\205\324\125\360\357\105\260\174\253\122\243\35\207\337\173\263\130\20\126\46\61\24\135\204\310\32\177\331\347"
  "\162\245\227\327\256\373\372\313\76\317\170\27\372\27\276\354\267\61\140\166\233\313\373\375\277\324\264\174\21\311\26\357"
  "\355\305\274\313\267\34\332\202\157\375\142\242\164\260\272\151\332\246\3\144\140\133\234\267\27\253\263\375\17";

/* lib/include/wasm_simd128.h (DEFLATEd, org. size 44179) */
static unsigned char file_l_36[4173] =
  "\244\324\305\222\334\60\20\6\340\163\350\35\72\154\171\231\231\231\171\367\250\32\266\252\144\50\301\320\323\307\30\352\112"
  "\324\263\163\62\375\177\373\63\316\370\160\171\371\174\65\245\315\100\266\340\145\357\341\12\36\316\256\16\347\346\127\301"
  "\237\371\360\366\303\333\317\211\252\165\302\32\304\121\243\225\155\247\205\156\172\224\33\20\32\352\126\110\63\45\242\74"
  "\373\131\104\15\151\233\55\330\324\246\51\42\63\35\154\377\131\350\325\164\310\363\165\31\327\232\136\43\216\264\201\156"
  "\54\232\340\47\154\243\230\321\154\265\105\324\372\73\232\60\360\274\142\212\347\263\337\232\76\143\151\62\77\64\235\45"
  "\41\156\267\165\313\154\315\102\115\212\116\264\65\267\314\322\62\373\237\143\225\353\104\326\214\233\203\32\124\125\125\100"
  "\70\247\155\156\231\214\103\25\252\256\152\40\336\274\223\267\60\117\346\241\12\225\127\65\20\157\321\311\133\136\44\363"
  "\120\205\312\253\32\210\267\372\57\236\230\133\356\227\357\121\177\325\101\103\161\52\53\13\163\115\176\341\354\150\46\373"
  "\172\223\45\233\304\302\174\177\261\174\245\373\213\16\24\316\223\124\125\232\153\362\313\157\107\144\331\161\130\226\314\22"
  "\313\213\375\371\362\13\353\317\273\130\50\117\142\125\151\256\311\337\242\35\221\145\307\141\131\12\13\375\216\206\55\25"
  "\43\330\324\24\144\207\65\310\132\112\233\205\70\222\203\35\314\105\163\376\2\213\310\244\107\14\302\346\141\244\205\131"
  "\302\57\155\54\57\232\343\366\126\141\354\135\45\170\127\171\6\372\223\73\131\105\273\223\220\236\20\4\3\316\357\217"
  "\356\236\316\356\217\370\301\315\365\303\343\336\365\243\47\176\260\153\135\333\225\333\60\360\71\355\37\374\226\136\124\126"
  "\166\352\217\244\336\306\364\236\111\317\277\207\320\322\314\304\40\104\102\146\172\366\254\316\155\340\140\6\105\24\151\352"
  "\362\320\270\21\113\100\314\52\271\165\305\202\362\150\165\47\212\100\53\142\165\116\356\257\211\201\167\210\242\341\352\216"
  "\321\246\152\32\373\252\42\340\335\252\150\270\352\225\66\125\313\334\127\25\1\357\126\105\303\125\107\51\125\2\102\256"
  "\277\376\346\363\257\56\117\335\325\161\320\124\331\136\20\17\302\120\306\11\77\213\331\152\135\174\202\26\200\55\112\251"
  "\35\73\5\230\41\165\200\153\2\352\315\155\353\30\226\316\102\10\323\257\304\352\350\46\51\323\330\131\12\141\372\245"
  "\130\155\334\44\145\231\73\113\41\114\277\224\266\336\325\17\150\67\337\307\174\256\267\6\231\146\345\373\323\113\317\135"
  "\275\360\302\13\321\175\372\74\74\50\112\311\166\142\240\125\61\164\264\25\263\170\215\361\232\342\65\307\353\101\274\226"
  "\170\135\307\353\46\136\57\213\315\152\50\226\203\230\16\142\73\210\261\220\170\353\211\107\257\142\104\276\376\360\323\363"
  "\153\17\311\75\167\65\54\157\74\375\123\17\370\137\212\17\212\244\1\167\343\203\173\6\10\177\150\200\320\77\102\366"
  "\272\61\27\320\260\160\204\362\27\327\225\0\135\333\153\314\226\370\250\322\30\26\121\176\123\25\136\116\72\371\205\322"
  "\204\373\211\302\116\121\360\250\262\27\256\71\121\323\230\104\345\367\3\275\37\351\375\124\221\71\231\313\336\242\114\225"
  "\253\151\24\125\263\122\125\316\15\341\202\145\200\165\200\205\340\176\112\320\250\4\16\51\264\326\315\11\131\346\244\44"
  "\277\37\14\272\202\154\57\236\231\251\212\365\62\13\301\61\23\54\307\230\160\300\314\260\233\32\52\324\320\312\55\120"
  "\66\102\174\146\375\255\0\350\323\360\273\117\43\175\332\131\3\241\261\6\304\217\257\10\202\4\206\365\54\63\351\251"
  "\104\273\374\365\120\124\140\46\200\230\353\14\324\236\31\322\266\344\355\104\130\46\244\370\24\61\342\340\247\364\163\103"
  "\244\321\376\277\274\327\243\74\301\117\27\66\135\364\244\313\223\36\5\167\130\134\164\31\204\350\252\111\325\23\105\205"
  "\210\35\274\140\363\202\217\227\236\325\50\134\323\350\242\305\40\52\134\324\371\265\20\51\24\354\340\2\233\13\114\56"
  "\366\334\302\141\131\146\37\25\2\41\52\172\252\61\3\242\306\143\7\13\330\54\320\302\42\250\200\322\154\320\116\43"
  "\330\171\11\166\136\354\333\75\223\161\307\104\301\20\31\63\46\352\16\236\13\303\334\255\141\273\203\332\223\315\253\320"
  "\325\354\205\325\354\351\150\147\336\205\251\12\114\227\260\135\242\301\147\202\46\366\227\357\277\371\352\160\372\46\255\325"
  "\323\326\224\153\211\156\140\11\210\20\134\135\12\301\204\316\374\170\300\273\137\107\173\242\12\346\212\216\134\141\163\105"
  "\63\131\44\262\146\365\174\165\211\261\77\135\212\134\363\336\301\261\235\265\102\25\326\2\165\244\52\310\274\157\75\60"
  "\177\36\273\262\227\261\146\61\126\4\300\257\0\116\5\330\45\201\147\170\176\100\260\132\230\14\165\13\17\113\366\57"
  "\166\225\36\46\50\154\72\205\355\24\55\136\223\15\51\350\325\306\32\213\332\170\130\70\101\314\260\322\307\304\26\35"
  "\331\302\146\213\106\272\225\116\26\353\132\47\47\165\107\147\224\35\215\240\113\302\321\313\15\22\260\103\3\234\32\340"
  "\25\241\237\142\371\371\325\352\146\62\74\154\374\221\164\265\253\164\63\101\141\323\51\154\247\150\361\232\154\110\101\257"
  "\156\326\130\324\315\323\310\31\142\206\74\102\365\62\161\105\107\256\260\271\302\111\326\256\240\112\47\47\155\107\147\204"
  "\35\135\240\313\301\321\311\15\22\260\103\3\234\32\340\25\241\27\136\274\344\262\72\231\14\165\47\57\163\366\57\166"
  "\225\116\46\50\154\72\205\355\24\55\136\223\15\51\350\325\311\32\213\72\171\231\71\103\314\160\263\223\211\53\72\162"
  "\205\315\25\116\262\166\5\125\72\71\151\73\72\43\354\350\2\135\16\216\116\156\220\200\35\32\340\324\200\175\42\150"
  "\301\317\213\164\173\202\14\346\4\231\107\46\16\241\74\103\146\53\302\353\124\255\301\234\167\62\67\125\256\301\61\361"
  "\204\372\135\233\344\35\235\304\35\311\56\105\332\221\155\272\63\12\222\165\23\45\123\43\333\124\161\241\174\27\315\126"
  "\204\327\53\333\346\275\51\163\323\331\166\334\234\102\275\263\111\236\47\333\316\326\56\106\332\312\266\271\207\361\351\341"
  "\343\313\326\261\7\115\224\7\366\374\233\275\261\5\322\363\207\377\35\171\361\122\223\113\232\236\273\172\230\260\131\336"
  "\115\153\22\345\335\232\316\121\336\255\331\35\344\335\232\354\227\342\73\111\375\313\361\125\140\343\77\251\206\233\370\121"
  "\52\343\72\276\56\122\51\361\125\52\106\34\10\276\300\13\272\200\13\266\100\13\262\0\13\356\113\326\176\126\252\311"
  "\355\103\52\326\236\125\377\162\66\366\303\172\376\360\277\43\67\336\337\277\234\151\373\110\152\162\363\104\221\256\146\36"
  "\331\166\266\306\330\256\352\370\103\167\107\12\57\145\265\157\16\220\43\151\37\201\262\262\200\135\131\200\226\332\361\7"
  "\166\324\37\257\177\26\170\373\105\42\351\74\262\245\323\302\140\306\31\26\153\307\307\371\275\102\113\261\361\126\140\246"
  "\353\75\350\145\350\107\223\176\150\235\316\357\65\232\127\77\55\330\205\256\165\72\314\134\345\223\306\254\315\330\3\120"
  "\173\121\265\254\144\7\366\321\60\143\345\156\363\202\301\13\355\274\102\116\356\336\143\141\306\22\264\136\61\172\241\352"
  "\375\136\243\355\252\230\220\203\274\161\224\254\250\163\63\73\301\310\116\260\263\143\255\267\56\137\322\162\61\275\73\376"
  "\312\231\35\150\6\24\304\100\24\375\324\52\142\140\55\325\67\377\137\101\120\372\114\157\0\10\216\301\145\355\353\53"
  "\253\317\77\137\137\200\177\374\137\177\315\223\357\25\160\13\1\156\41\300\255\10\70\17\0\364\71\2\234\347\343\73"
  "\374\173\3\202\227\4\307\25\304\126\20\43\50\254\300\10\136\22\362\12\247\320\12\247\20\341\124\134\1\20\274\44"
  "\344\25\304\126\20\43\50\257\0\10\136\21\176\257\360\362\336\1\155\354\163\320\306\76\17\155\354\23\320\306\71\47"
  "\200\133\11\0\332\70\347\4\220\332\310\10\136\22\34\127\20\133\101\214\220\332\310\10\136\22\342\12\240\215\163\16"
  "\10\261\215\214\340\45\41\257\40\266\202\20\41\266\221\21\274\40\204\66\316\3\22\264\261\317\101\33\373\74\264\261"
  "\117\100\33\347\234\0\156\45\0\150\343\234\3\100\154\43\43\170\111\160\134\101\154\5\41\102\154\43\43\170\111\210"
  "\53\200\66\316\71\40\304\66\62\202\227\204\274\202\330\12\102\204\330\106\106\360\206\220\333\330\17\160\320\306\76\7"
  "\155\354\363\320\306\76\1\155\234\163\2\270\225\0\240\215\163\116\0\335\306\110\20\43\210\21\364\37\302\371\141\345"
  "\214\222\133\65\202\50\272\236\254\112\205\155\160\124\301\302\1\332\121\262\372\114\143\373\246\62\117\314\321\105\376\313"
  "\307\151\365\171\15\124\337\51\160\234\313\220\270\67\205\376\236\51\170\12\257\226\102\365\110\336\220\30\274\254\60\170"
  "\131\141\340\254\60\130\131\101\270\41\0\131\141\260\26\265\160\113\140\134\101\300\330\221\302\35\201\127\20\60\326\223"
  "\160\157\2\160\11\214\315\40\334\233\100\337\176\16\274\275\40\374\116\5\336\13\203\267\27\204\33\2\360\34\170\173"
  "\101\270\41\0\317\201\267\25\204\33\2\360\34\170\73\101\270\41\0\317\201\267\21\204\73\2\360\34\154\377\175\231"
  "\126\51\334\152\54\250\253\273\126\377\227\235\302\350\253\270\133\215\272\313\13\376\133\53\234\377\255\352\136\360\172\330"
  "\265\300\64\33\375\223\66\332\117\63\165\277\172\355\257\136\377\53\12\344\100\363\62\242\103\135\341\150\174\126\124\46"
  "\117\323\64\376\357\127\377\76\255\163\364\174\327\211\354\262\175\376\314\315\33\357\33\153\336\175\117\347\165\351\307\376"
  "\371\326\0\364\137\157\335\362\307\236\215\176\41\7\222\65\11\67\306\162\153\74\372\211\242\252\337\150\276\20\354\236"
  "\226\346\234\104\351\361\334\375\353\325\2\225\266\315\167\177\257\334\53\51\356\125\240\354\125\337\1\362\35\107\276\5"
  "\204\266\356\1\165\374\346\366\377\130\151\343\362\52\346\320\271\263\310\156\367\117\220\324\374\13\155\217\367\175\172\177"
  "\276\254\324\130\40\17\371\223\153\67\135\176\37\325\121\37\200\354\35\72\124\161\55\367\56\376\355\217\74\112\305\157"
  "\133\111\323\143\266\75\146\333\143\76\55\15\223\260\115\342\1\223\300\231\164\57\274\20\53\336\172\257\126\170\174\271"
  "\130\230\323\322\255\216\207\152\114\227\254\341\167\215\246\121\74\154\24\70\243\45\236\320\246\346\35\223\302\337\343\340"
  "\136\47\325\230\56\174\235\16\30\305\303\106\174\235\336\316\27\264\251\171\307\244\360\74\27\303\42\16\133\4\317\242"
  "\273\242\105\315\73\26\205\347\131\30\26\161\330\42\320\242\373\170\345\350\135\27\130\36\131\160\317\67\14\20\347\104"
  "\101\234\113\250\31\347\22\340\70\47\212\172\355\305\71\371\122\234\23\112\161\116\35\71\316\155\34\306\71\221\34\347"
  "\324\34\343\134\102\106\262\122\205\223\254\262\202\222\125\62\126\236\121\205\355\1\311\312\66\211\7\114\202\146\342\44"
  "\53\361\316\133\171\110\126\311\130\71\246\252\161\135\170\143\37\60\212\207\215\202\146\144\44\53\361\226\111\341\311\301"
  "\312\61\125\215\353\302\327\311\67\252\152\176\172\72\157\61\242\107\305\133\16\205\47\7\43\121\211\367\34\50\327\231"
  "\26\161\324\202\237\30\43\121\211\367\54\50\327\231\26\161\330\42\310\302\312\165\52\260\74\224\353\340\373\53\310\165"
  "\242\40\327\45\324\314\165\11\140\256\23\205\275\366\162\235\174\61\327\11\205\134\247\216\234\353\66\216\163\235\110\316"
  "\165\152\116\271\56\41\43\327\251\302\311\165\131\101\271\56\31\53\115\251\302\366\200\134\147\233\304\3\46\101\63\61"
  "\162\235\170\353\213\42\310\165\311\30\231\105\274\345\120\170\162\60\266\262\170\313\241\360\344\140\354\103\361\216\3\157"
  "\145\323\42\16\133\4\315\302\330\207\342\35\13\336\312\246\105\34\266\340\131\274\114\353\147\366\260\46\242\52\313\110"
  "\125\247\205\77\4\204\45\55\12\226\164\102\315\45\235\0\56\151\121\334\153\147\111\313\227\226\264\120\132\322\352\310"
  "\113\172\343\160\111\213\344\45\255\346\270\244\23\162\226\264\52\214\45\235\25\264\244\223\261\126\243\52\74\17\136\322"
  "\266\111\74\140\22\64\23\143\111\213\167\76\163\244\45\235\214\261\244\305\133\16\205\47\7\143\111\213\267\34\12\17"
  "\137\173\302\311\104\24\236\114\6\72\231\14\160\62\251\50\356\125\240\166\257\345\317\171\205\146\302\260\133\122\355\166"
  "\317\375\171\204\166\302\260\135\122\355\166\303\70\115\63\364\23\307\15\67\254\335\161\235\343\362\14\35\305\161\307\15"
  "\243\273\245\233\373\205\57\242\110\356\372\5\66\372\132\207\10\361\306\307\247\164\210\30\314\103\204\170\313\241\360\340"
  "\340\34\42\304\133\16\205\107\207\227\363\207\343\220\274\345\120\170\160\160\216\20\342\55\207\302\203\203\23\335\305\133"
  "\16\205\107\207\167\143\20\52\260\54\262\200\65\214\131\250\300\322\310\2\376\76\34\66\247\50\330\234\164\134\30\340"
  "\270\120\121\334\253\100\355\136\274\71\205\161\267\244\332\355\170\163\12\343\166\111\65\333\301\346\254\70\154\10\233\63"
  "\21\330\234\25\207\35\141\163\46\302\233\263\46\271\53\155\116\63\331\213\267\76\127\207\315\151\46\173\361\226\3\154"
  "\116\63\331\213\267\34\12\117\16\306\346\24\157\71\24\236\34\214\315\51\336\162\50\74\71\30\233\123\274\345\120\170"
  "\162\260\66\247\12\54\213\54\140\15\143\26\52\60\65\140\163\236\377\73\43\344\353\350\317\15\315\57\161\352\12\176"
  "\163\125\25\234\166\367\153\330\106\361\63\106\1\7\312\351\362\321\317\353\351\214\76\267\170\76\142\12\327\164\330\45"
  "\114\227\70\342\22\20\27\276\340\161\372\13\146\323\252\341\40\121\227\150\106\354\5\163\152\325\370\136\232\27\77\153"
  "\133\317\177\372\171\332\125\203\62\274\307\353\252\145\253\203\107\317\25\214\37\24\214\206\240\316\224\375\333\264\366\154"
  "\327\256\301\247\340\227\222\366\375\366\76\117\33\377\57\155\127\260\223\100\14\104\177\145\116\46\350\145\73\235\166\127"
  "\303\47\160\365\114\142\4\17\162\63\44\36\374\170\333\370\32\337\116\330\272\41\302\245\217\171\363\72\355\114\3\44"
  "\320\241\34\204\356\153\126\117\263\342\274\171\211\361\252\336\117\347\267\303\223\354\367\263\153\31\347\343\361\164\220\237"
  "\350\257\362\162\50\322\331\202\56\170\343\266\323\177\66\320\336\57\206\251\264\224\155\227\126\215\42\167\62\174\36\237"
  "\167\273\215\154\267\222\207\215\174\111\41\102\261\63\221\62\10\165\212\244\40\242\43\154\252\4\72\75\127\7\363\16"
  "\6\145\162\261\154\0\221\235\42\146\20\243\47\324\305\232\234\203\116\120\76\272\130\152\40\302\340\24\103\43\302\234"
  "\10\331\305\12\352\34\264\51\343\234\220\251\21\346\10\153\104\372\45\352\303\237\175\137\316\213\67\365\372\375\333\173"
  "\177\135\275\320\341\375\366\47\365\246\121\72\277\25\273\56\215\143\77\213\335\306\342\353\267\127\246\371\73\77\45\350"
  "\346\136\333\50\17\22\52\16\325\206\43\212\247\215\122\270\53\331\42\154\221\154\66\237\302\210\112\160\117\144\313\260"
  "\145\262\215\363\51\106\120\235\167\352\353\312\21\373\345\350\266\262\356\225\143\145\366\255\215\330\167\303\274\167\230\42"
  "\212\143\30\233\244\141\45\34\111\256\220\50\44\204\225\160\104\21\215\244\221\44\300\112\70\166\276\235\133\127\15\302"
  "\141\271\20\235\126\323\27\153\260\62\367\123\33\51\367\223\313\75\114\221\150\43\234\10\147\302\43\115\21\20\51\120"
  "\44\140\45\34\11\33\313\51\22\160\46\74\326\32\320\207\203\345\75\177\254\275\155\3\231\174\227\162\6\50\21\303"
  "\100\24\75\223\200\7\132\160\161\13\272\152\365\123\331\323\353\22\200\341\21\376\370\315\1\36\175\363\123\222\244\114"
  "\332\364\174\214\107\34\333\355\226\374\107\266\60\321\55\265\301\374\301\350\172\332\367\267\43\152\221\231\200\211\133\5"
  "\175\33\121\56\10\160\135\260\357\201\36\317\301\351\320\13\116\300\250\17\270\202\76\301\134\220\340\272\140\237\340\371"
  "\373\353\174\175\32\207\314\373\10\330\263\305\34\351\57\215\221\250\47\154\43\165\331\236\57\201\25\230\136\213\210\361"
  "\22\53\227\325\62\110\36\226\214\24\12\17\254\300\344\141\65\237\43\112\345\230\100\240\145\220\376\224\117\202\43\110"
  "\251\132\105\140\5\246\327\2\142\274\304\312\145\265\14\222\207\45\43\205\302\3\53\60\171\130\364\342\206\155\0\375"
  "\167\101\207\264\275\226\44\312\10\132\255\121\107\340\5\246\27\43\142\336\55\326\56\253\145\220\74\56\31\51\24\36"
  "\130\201\371\107\130\152\227\236\373\245\305\367\323\266\37\333\347\331\56\77\206\13\226\40\142\375\62\104\110\326\321\160"
  "\13\216\375\354\17\310\314\37\206\13\46\134\142\375\244\113\110\336\321\160\13\216\375\373\370\252\27\354\317\334\46\322"
  "\301\321\106\222\60\166\273\106\26\373\266\300\26\164\246\13\332\372\212\371\50\320\235\300\313\341\312\312\62\236\330\26"
  "\364\172\270\375\355\217\122\142\164\302\45\234\337\5\41\214\67\301\310\216\2\163\133\322\241\56\150\353\53\346\243\100"
  "\167\2\57\207\53\53\313\170\142\133\320\353\341\252\155\327\57\45\46\7\367\71\34\65\362\23\356\177\44\135\343\311"
  "\155\111\307\272\240\255\257\230\217\2\335\11\274\34\256\254\54\343\211\155\101\257\207\333\177\315\371\170\170\374\145\366"
  "\350\57\26\340\242\165\241\160\165\350\177\0";

/* lib/include/wchar.h (DEFLATEd, org. size 292) */
static unsigned char file_l_37[159] =
  "\215\120\263\242\4\1\14\254\237\376\41\317\147\243\75\333\256\326\266\167\377\376\154\247\211\223\231\211\6\140\312\123\64"
  "\220\34\156\340\244\105\33\120\127\64\333\2\134\241\240\153\133\253\60\20\375\170\375\170\375\326\14\234\225\161\120\25"
  "\222\136\345\313\105\167\265\204\132\300\233\100\330\274\144\205\171\145\75\274\152\361\212\165\321\131\237\241\150\206\127\150"
  "\230\26\153\371\1\332\256\167\300\27\216\207\143\136\226\331\230\37\226\333\266\305\204\223\211\365\306\331\174\176\6\276"
  "\47\206\353\235\321\372\366\376\313\171\57\77\73\134\334\262\121\124\160\227\112\204\17\112\360\121\165\175\166\1";

/* lib/math.wo (DEFLATEd, org. size 168538) */
static unsigned char file_l_38[14260] =
  "\214\126\167\163\333\76\26\374\373\332\167\340\351\122\344\124\200\115\200\123\310\364\344\327\173\335\73\5\65\126\42\121\12"
  "\105\367\233\174\366\3\100\131\215\216\117\36\316\140\275\170\157\337\242\12\375\311\124\37\216\115\164\155\42\232\203\177"
  "\374\65\212\372\243\311\154\132\67\121\117\325\115\57\352\315\147\327\172\121\377\335\170\52\305\70\272\346\270\175\307\104"
  "\375\311\141\23\215\222\170\157\157\357\222\234\211\231\114\353\123\227\326\202\66\155\201\111\310\330\255\112\50\260\21\155"
  "\115\165\324\363\115\55\106\163\143\116\224\231\65\56\321\36\126\52\272\346\73\367\67\272\242\376\114\324\142\262\356\124"
  "\213\106\264\203\335\227\263\350\110\324\221\30\217\336\125\217\130\324\3\41\333\237\45\305\145\54\53\172\35\65\75\33"
  "\36\134\255\327\371\112\220\4\222\301\304\227\353\215\167\325\313\241\11\224\175\6\43\141\365\127\217\273\152\177\230\172"
  "\332\125\333\375\143\244\253\171\40\306\57\377\257\246\41\305\245\54\244\355\52\216\253\370\365\233\256\144\233\140\315\323"
  "\173\60\171\321\141\76\47\365\325\267\233\122\107\17\117\63\30\313\141\304\243\65\14\251\273\351\337\357\264\65\154\12"
  "\112\100\351\352\23\24\322\202\145\77\303\32\120\16\341\142\351\37\5\344\0\132\102\10\360\100\53\3\232\102\132\376"
  "\203\367\317\162\50\1\145\241\311\343\273\120\11\232\312\47\130\15\46\40\115\327\335\117\233\356\324\317\376\17\72\53"
  "\316\140\315\206\45\105\13\107\75\201\114\100\45\244\360\6\205\54\234\233\34\304\125\116\141\123\15\236\27\340\11\130"
  "\132\301\160\230\344\6\130\134\364\101\222\137\240\70\172\223\111\1\232\201\45\355\244\103\61\150\371\153\1\102\15\154"
  "\354\77\315\236\77\53\16\174\145\246\101\5\354\340\6\51\40\162\360\170\0\341\130\366\11\64\55\274\272\30\300\222"
  "\30\304\102\62\320\270\230\377\370\326\251\101\311\103\330\4\322\100\247\147\220\266\231\335\202\225\227\354\344\121\65\152"
  "\206\357\77\254\315\100\352\327\47\136\133\234\144\15\247\153\70\137\342\256\354\167\157\246\361\147\326\274\204\225\377\204"
  "\345\33\33\371\356\363\346\361\372\361\0\147\57\141\331\303\45\367\366\173\50\165\362\140\43\206\205\125\260\204\257\264"
  "\243\353\147\154\43\246\7\26\303\44\371\172\61\120\15\233\214\262\256\355\346\170\72\234\36\231\172\70\33\155\315\210"
  "\253\145\71\204\237\227\347\337\124\136\304\252\75\320\314\43\115\177\305\315\0\164\12\353\51\31\326\300\327\172\10\316"
  "\301\75\367\4\234\74\163\255\372\36\326\370\150\351\242\14\204\14\170\40\240\174\324\176\165\55\50\305\137\77\165\355"
  "\33\220\34\46\170\156\140\304\275\140\136\201\307\320\324\143\43\101\165\53\267\7\111\135\137\140\331\143\210\101\360\230"
  "\145\140\301\63\244\14\331\134\301\160\260\64\24\115\157\314\274\261\117\303\120\62\7\247\334\241\37\301\22\276\210\265"
  "\151\100\114\16\333\234\76\54\207\324\101\233\201\332\260\40\332\300\132\360\120\221\70\300\100\262\340\224\336\207\261\36"
  "\125\177\70\11\7\46\240\166\342\11\145\77\345\355\250\224\14\163\367\362\133\310\40\300\215\55\374\206\200\21\301\226"
  "\24\270\171\350\201\311\140\44\224\17\172\164\16\113\203\207\1\7\151\323\342\37\300\104\340\344\7\230\200\250\205\244"
  "\101\204\260\177\203\171\327\44\234\102\102\234\200\172\31\242\11\204\113\40\44\202\124\120\336\155\16\233\202\7\5\223"
  "\100\160\120\237\371\37\1\116\133\55\120\11\23\354\263\14\234\33\217\4\1\115\207\201\323\345\101\150\11\64\203\265"
  "\36\337\234\177\175\161\144\362\40\242\304\57\240\131\100\34\202\315\203\45\23\277\165\355\7\60\5\165\371\261\252\146"
  "\243\151\74\74\70\336\332\234\213\3\25\332\246\374\57\64\7\215\333\163\106\171\171\366\36\324\266\154\257\204\224\260"
  "\372\172\350\334\53\367\137\336\161\335\367\113\340\25\15\21\56\213\253\17\351\42\54\53\241\45\230\35\204\160\136\206"
  "\113\57\335\167\151\17\313\77\240\331\43\227\134\224\137\175\52\275\300\223\162\14\112\236\6\231\247\45\230\202\210\237"
  "\5\271\347\45\204\112\137\54\104\135\253\44\124\376\162\41\376\252\204\221\277\275\16\45\336\224\335\61\73\124\35\214"
  "\66\57\223\157\41\245\0\311\336\71\141\150\135\200\262\273\317\177\12\3\66\274\0\227\260\71\30\205\216\101\344\34"
  "\306\256\107\130\136\134\136\145\274\375\233\156\142\163\37\75\14\354\355\263\207\40\3\0\64\115\156\100\170\355\207\220"
  "\32\112\302\222\63\60\346\172\147\133\41\234\76\274\244\314\117\133\45\352\345\217\15\124\12\43\301\31\70\137\174\212"
  "\103\132\130\13\226\264\173\374\332\33\250\270\370\10\232\377\13\326\100\345\37\41\25\244\255\242\257\240\62\50\375\2"
  "\162\120\114\300\105\3\141\141\143\110\2\231\100\332\357\37\101\23\10\142\121\103\322\302\105\300\152\150\363\303\335\160"
  "\11\151\110\353\253\327\107\327\276\74\207\340\305\375\361\373\73\317\41\123\210\330\167\121\12\55\320\203\111\366\175\60"
  "\47\213\51\14\157\305\305\330\354\114\215\305\174\76\262\247\121\337\234\264\57\314\25\327\306\107\27\157\310\153\47\221"
  "\315\323\275\250\137\233\371\341\270\175\375\106\213\210\361\124\371\167\153\155\232\43\327\206\236\13\156\62\77\336\44\306"
  "\333\304\160\350\304\274\162\20\363\175\367\324\264\232\67\121\332\22\41\354\336\334\64\27\5\326\351\167\236\76\11\214"
  "\27\271\127\233\121\325\230\172\346\42\207\66\337\120\150\214\11\305\266\313\44\361\52\335\234\64\246\322\103\327\71\234"
  "\257\330\371\101\75\74\134\245\35\327\142\66\164\174\307\236\33\153\307\233\53\330\311\354\332\32\167\62\203\332\266\125"
  "\112\122\226\15\262\25\55\52\275\372\147\132\167\34\355\250\33\323\64\117\223\214\14\342\256\364\272\315\356\0\103\211"
  "\256\205\225\64\131\21\346\343\2\333\116\375\53\127\332\214\347\146\207\121\354\126\52\331\275\324\116\23\266\113\115\322"
  "\131\307\253\147\261\223\125\231\26\317\315\330\250\346\352\21\124\272\13\72\345\326\63\34\76\254\53\217\73\27\304\150"
  "\156\375\163\323\254\256\207\13\346\177\355\175\151\167\342\270\323\357\127\231\363\114\356\332\207\34\355\226\357\362\362\356"
  "\373\377\74\357\373\260\230\16\111\232\60\100\47\300\247\277\230\144\122\143\27\342\47\53\302\115\147\310\177\231\221\261"
  "\245\252\122\355\52\111\121\312\41\136\116\351\207\127\334\113\45\275\167\336\350\102\251\302\10\253\45\11\51\161\146\344"
  "\373\365\304\100\104\147\363\51\141\171\150\236\35\105\245\165\241\204\166\336\232\242\260\136\24\147\106\161\76\234\23\212"
  "\207\146\167\25\377\143\103\272\72\115\7\327\175\364\62\335\263\151\3\21\6\61\7\303\130\241\155\131\72\125\350\102"
  "\230\322\236\204\101\320\203\171\105\332\203\113\61\11\41\236\241\247\345\367\341\143\143\222\16\117\76\302\212\373\366\43"
  "\263\367\314\324\322\203\50\126\132\355\275\241\341\343\154\376\255\53\377\334\155\146\55\167\200\36\60\167\340\62\315\174"
  "\215\103\244\231\147\346\230\177\111\275\21\250\126\31\345\75\75\335\74\55\31\206\14\10\352\236\261\137\370\245\325\217"
  "\321\51\343\255\45\75\151\121\47\4\22\263\223\205\361\332\231\42\340\263\220\71\55\155\251\235\240\137\276\255\353\341"
  "\0\47\326\174\70\232\255\111\142\336\36\234\131\167\17\270\362\366\361\312\2\313\127\125\125\205\65\137\127\173\320\106"
  "\363\343\230\274\65\347\65\56\204\130\375\133\113\342\36\232\342\165\267\151\313\337\257\47\176\311\322\167\104\370\30\53"
  "\212\46\313\257\370\13\234\231\233\330\77\304\72\246\4\13\0\17\12\124\212\7\316\146\227\270\222\154\126\340\265\51"
  "\161\366\106\336\212\277\376\55\276\130\363\333\277\376\367\277\311\133\57\244\321\245\267\325\27\41\335\157\377\376\337\320"
  "\247\337\177\74\62\252\1\366\312\312\112\244\261\342\43\241\156\134\142\15\123\262\15\136\174\10\223\370\41\314\161\170"
  "\16\303\323\65\234\114\342\246\170\336\36\177\240\353\171\245\247\217\353\167\174\247\307\330\301\272\112\116\307\152\352\247"
  "\332\226\213\101\131\26\7\176\250\352\176\336\331\200\164\243\224\336\251\32\107\357\113\255\224\67\262\27\315\374\56\51"
  "\323\146\257\304\50\304\251\51\124\153\21\355\33\21\215\373\245\54\66\14\104\242\17\47\206\245\11\156\160\216\343\40"
  "\20\2\201\11\54\52\243\307\336\13\121\330\162\274\370\122\226\356\165\2\277\34\237\300\122\224\246\364\276\124\122\52"
  "\137\12\57\275\377\25\147\20\23\27\351\371\300\164\177\200\30\106\225\246\164\205\342\261\10\62\24\203\167\115\246\231"
  "\46\343\150\207\165\333\143\313\264\160\275\373\43\105\77\77\306\317\42\67\22\170\52\31\65\254\241\107\217\325\125\177"
  "\141\222\131\303\270\237\131\260\174\154\175\145\153\340\125\15\136\275\52\173\153\255\224\322\112\125\15\204\54\270\127\25"
  "\31\132\114\227\325\146\161\62\262\250\26\353\45\12\56\130\64\61\213\214\56\350\3\171\323\352\101\265\37\350\303\203"
  "\317\31\220\0\367\236\311\2\61\74\353\154\306\241\250\247\220\221\142\117\361\23\166\153\265\176\132\126\277\75\115\247"
  "\373\76\377\243\370\163\305\222\365\74\213\162\234\277\125\31\174\36\216\27\321\15\307\73\244\360\22\2\37\16\6\137"
  "\214\161\1\277\370\223\7\116\200\163\63\260\251\272\11\30\161\314\250\64\177\150\20\175\303\136\254\237\321\224\76\15"
  "\47\111\322\200\342\103\51\224\342\21\42\31\133\214\146\242\241\344\200\24\322\31\343\12\305\144\212\351\270\336\314\75"
  "\347\317\376\35\114\144\101\307\325\354\61\230\144\14\230\313\231\150\332\266\373\126\173\326\372\271\365\253\374\325\122\165"
  "\63\221\150\31\147\222\13\231\310\221\252\143\122\250\231\24\66\310\163\37\36\25\151\174\376\151\370\313\204\20\34\47"
  "\172\46\313\247\5\40\43\202\252\251\120\336\64\274\347\323\305\73\345\357\234\64\362\202\77\222\221\106\176\136\61\220"
  "\111\267\51\127\152\225\16\61\137\111\47\34\302\265\31\41\66\40\271\151\216\23\105\16\316\310\230\132\231\122\300\375"
  "\163\47\316\251\200\57\311\77\203\23\262\342\246\227\161\12\221\2\14\72\220\374\45\276\220\307\247\61\235\55\211\33"
  "\201\366\261\22\220\62\374\251\24\312\144\317\100\63\340\21\47\322\7\220\352\100\161\223\252\357\44\225\22\112\345\347"
  "\27\73\114\146\214\165\100\136\261\34\206\355\207\144\275\361\105\365\60\360\234\43\36\117\247\176\357\217\103\100\146\364"
  "\107\46\304\201\2\272\217\67\132\34\330\164\275\225\70\252\310\354\336\163\234\302\151\74\34\214\234\67\134\230\76\76"
  "\75\55\251\116\241\361\230\252\25\256\201\304\65\220\370\305\3\11\226\371\143\257\235\57\174\200\351\250\163\104\31\3"
  "\51\12\123\224\302\10\161\15\63\76\201\277\363\170\236\60\343\307\65\314\270\206\31\327\60\43\115\354\256\141\306\65"
  "\314\210\30\365\32\146\54\237\176\314\47\331\243\11\211\126\361\133\357\377\156\133\35\246\257\340\137\303\213\153\170\301"
  "\27\40\260\167\203\160\34\310\4\373\301\26\25\310\25\6\352\24\371\132\127\207\277\147\156\242\75\66\230\252\141\3"
  "\175\365\314\77\261\147\376\273\105\276\71\275\366\11\274\363\30\347\126\176\142\347\226\40\303\176\53\315\373\325\163\315"
  "\344\271\256\227\373\147\347\366\134\127\233\153\346\73\311\247\132\155\76\257\243\213\277\134\235\247\60\215\200\300\222\305"
  "\146\144\23\66\237\161\263\65\220\311\256\46\122\174\270\254\376\347\322\354\163\70\116\34\213\376\155\11\347\246\274\76"
  "\31\340\263\314\246\14\60\17\266\340\361\346\356\373\323\144\172\162\377\305\54\146\377\305\214\331\73\140\17\263\155\307"
  "\240\7\246\375\300\266\37\270\233\153\376\347\63\232\305\131\170\177\111\272\343\361\223\155\6\375\160\264\364\336\143\221"
  "\377\144\151\34\60\333\352\206\123\4\323\360\327\345\17\34\341\204\14\62\336\200\222\274\332\373\11\104\210\21\213\372"
  "\77\272\13\307\323\317\157\372\220\173\107\371\235\75\74\241\346\46\151\353\227\70\44\161\330\136\57\100\311\4\7\24"
  "\234\215\5\312\34\154\46\65\323\345\304\254\331\64\73\105\71\243\174\162\175\364\11\152\31\260\350\331\217\230\242\253"
  "\51\162\67\227\20\104\262\227\372\217\1\57\337\222\361\225\201\57\363\341\74\276\26\171\37\174\236\214\75\267\60\361"
  "\72\77\175\252\330\335\266\325\336\235\76\47\140\266\75\235\247\145\207\6\266\332\333\126\173\27\212\163\351\301\357\122"
  "\375\215\16\72\333\166\301\251\246\315\31\321\332\62\350\150\110\204\331\26\157\175\116\117\152\163\232\323\133\244\202\320"
  "\64\21\212\270\36\231\21\237\21\207\160\216\217\335\322\17\133\270\353\166\256\3\151\242\364\61\21\312\204\46\177\47"
  "\333\61\226\0\141\72\213\22\173\245\134\352\310\367\354\370\1\65\46\263\147\354\271\142\146\344\107\37\165\371\66\307"
  "\11\146\217\233\223\23\371\30\113\150\114\13\60\26\367\57\227\325\364\266\165\143\120\130\27\40\336\242\27\110\7\321"
  "\224\322\62\65\366\13\30\226\140\352\23\116\50\141\337\143\147\174\264\377\360\341\267\233\43\207\164\110\141\64\70\365"
  "\203\146\47\20\77\74\55\176\273\71\226\106\204\273\24\11\334\335\33\234\313\257\263\51\301\371\6\266\276\211\71\337"
  "\205\353\27\216\322\141\52\216\167\30\356\217\274\141\216\374\150\111\250\327\135\67\33\352\206\144\253\201\221\75\66\21"
  "\112\205\240\6\174\203\101\245\171\62\71\347\311\66\347\311\345\236\47\227\165\236\14\115\115\253\141\251\201\255\155\307"
  "\163\143\164\64\276\300\57\111\120\21\333\356\52\302\307\252\210\220\33\20\146\275\42\47\353\371\46\353\225\107\372\336"
  "\102\326\143\50\21\113\224\131\131\257\40\6\153\65\174\110\105\110\31\253\43\266\170\342\273\351\10\51\162\316\224\224"
  "\315\251\222\52\367\134\111\225\127\237\13\232\40\326\222\324\302\116\162\146\125\261\305\347\235\61\146\1\133\74\231\126"
  "\141\217\55\74\332\16\37\104\105\124\142\20\206\61\11\54\235\316\271\113\227\356\60\317\31\123\60\247\207\305\325\254"
  "\17\220\164\305\304\341\43\142\30\51\306\146\24\306\335\161\224\70\2\174\264\260\327\311\41\302\305\223\333\36\31\166"
  "\333\201\141\267\235\31\166\173\66\206\335\166\146\330\355\171\31\26\31\375\50\206\145\24\306\335\245\63\154\320\7\342"
  "\20\1\206\335\240\231\147\375\316\233\166\117\337\64\214\254\271\341\250\142\344\45\36\62\76\135\0\340\277\333\105\5"
  "\351\240\227\307\370\136\160\61\71\1\205\111\102\130\320\244\302\316\104\174\370\315\37\261\164\1\262\46\22\155\137\111"
  "\260\27\217\1\60\240\301\10\115\371\16\44\64\77\101\206\46\100\2\172\4\146\166\167\206\231\15\317\4\357\205\33"
  "\136\362\146\315\15\327\156\315\307\244\233\122\25\306\125\141\220\313\2\300\100\263\16\146\25\314\21\275\376\211\305\225"
  "\114\252\155\232\124\167\223\220\146\375\314\172\76\103\230\115\212\304\35\123\44\215\307\64\43\311\256\176\374\4\62\310"
  "\21\252\102\151\116\261\16\127\16\220\123\17\26\36\351\213\374\225\30\64\263\375\327\323\237\71\56\122\2\304\105\60"
  "\252\45\1\303\154\204\43\43\300\15\340\252\62\10\343\12\47\113\350\353\364\60\62\27\352\11\364\307\344\142\302\23"
  "\113\215\273\304\304\6\121\77\31\52\34\53\176\176\355\300\204\71\275\304\223\53\232\34\373\165\126\177\54\327\135\167"
  "\247\356\16\17\251\250\151\366\255\125\66\265\154\66\327\355\335\252\262\135\65\325\172\360\7\173\101\264\172\20\255\17"
  "\232\315\357\255\361\343\66\13\165\71\31\274\306\371\162\267\375\154\222\367\375\154\130\245\42\365\206\157\23\304\257\45"
  "\155\25\44\1\110\330\111\210\261\301\326\224\76\105\205\235\304\31\140\3\75\165\232\377\44\222\23\167\316\244\355\211"
  "\2\223\103\106\243\363\7\240\10\10\347\32\361\256\64\176\333\177\367\305\136\331\10\237\324\15\0\44\176\352\70\60"
  "\12\104\32\337\103\143\163\266\12\254\154\142\327\43\276\123\205\126\13\67\222\2\43\165\64\56\122\254\52\204\350\156"
  "\102\167\72\205\126\144\65\236\32\36\30\341\212\375\360\242\255\151\2\154\243\306\207\104\23\104\222\243\135\362\47\135"
  "\17\16\241\3\123\150\176\64\315\104\243\301\212\141\2\174\10\142\346\114\154\215\135\364\131\140\334\144\256\347\317\40"
  "\327\207\165\326\367\140\274\235\100\34\260\223\11\62\176\200\6\30\346\346\60\310\144\342\147\310\220\162\144\160\262\210"
  "\32\351\23\275\1\202\324\161\252\121\154\365\375\357\114\102\256\345\311\150\257\270\342\377\43\342\53\311\277\342\66\114"
  "\224\205\264\212\275\271\154\325\354\201\22\255\310\123\266\331\34\211\100\117\301\3\246\326\101\176\300\76\354\32\14\306"
  "\3\235\70\356\133\63\5\6\171\345\217\256\240\320\245\207\177\117\41\301\34\307\0\15\261\164\263\322\56\124\162\347"
  "\156\302\304\12\13\212\157\10\112\231\117\120\144\127\206\131\113\40\155\130\234\262\154\17\0\143\304\157\137\136\207\30"
  "\207\104\377\107\344\6\144\260\25\201\215\204\25\205\4\235\4\205\215\277\321\235\62\53\64\24\366\360\61\331\240\16"
  "\217\166\277\127\324\151\76\365\212\265\314\32\34\104\10\143\26\14\2\241\204\141\1\35\112\156\62\144\147\233\41\63"
  "\32\215\253\321\50\217\32\215\222\327\151\107\20\65\365\206\222\270\324\71\123\363\273\264\304\373\267\244\321\31\15\167"
  "\101\136\106\107\67\343\4\10\227\12\356\32\113\40\46\141\333\225\114\270\65\141\200\121\7\31\342\124\264\210\133\61"
  "\14\12\364\25\264\357\201\51\220\40\146\6\243\201\64\150\0\53\34\160\342\153\222\261\112\302\30\222\52\301\112\35"
  "\320\211\375\32\346\24\254\263\101\346\3\130\13\234\274\25\361\7\5\123\217\371\227\67\11\233\276\327\67\167\321\53"
  "\217\213\247\27\160\130\103\307\165\310\341\246\331\336\175\275\153\77\170\154\76\130\264\337\130\264\337\330\312\146\173\335"
  "\156\253\146\173\331\154\256\132\157\67\233\77\232\315\347\146\363\205\232\370\352\273\207\146\163\73\133\355\247\211\236\45"
  "\34\155\301\217\262\300\147\125\300\263\51\124\321\174\266\122\55\172\265\47\144\325\236\220\165\373\215\65\275\101\343\150"
  "\35\72\3\343\172\6\206\52\172\77\3\103\25\271\316\300\0\347\115\220\52\317\171\170\105\170\13\147\216\125\333\175"
  "\373\361\117\325\270\252\155\324\360\161\66\377\106\137\201\3\161\310\140\165\365\322\360\122\171\334\31\22\31\362\43\170"
  "\250\56\311\200\307\115\206\43\332\362\34\3\102\314\203\251\230\177\314\114\344\334\346\47\47\340\0\166\41\15\45\12"
  "\322\352\270\61\276\351\47\357\320\262\160\174\6\51\135\130\151\214\170\275\20\167\135\317\233\327\200\350\1\212\106\100"
  "\155\261\124\136\271\242\50\134\334\6\106\25\0\22\60\77\344\44\66\50\350\42\171\127\62\131\221\7\336\47\72\221"
  "\223\4\20\337\306\367\0\26\275\331\45\45\11\75\202\375\224\60\167\301\307\270\7\301\37\203\200\263\2\330\146\216"
  "\124\30\207\27\63\102\74\345\127\210\362\240\107\114\371\331\5\120\76\341\137\362\116\16\260\162\27\140\163\62\250\174"
  "\0\177\222\246\333\162\260\304\121\260\370\35\114\350\334\24\32\167\125\75\126\343\65\102\12\364\311\254\115\43\63\160"
  "\12\261\171\365\215\306\353\202\57\261\154\132\171\42\60\105\230\103\61\61\22\271\213\7\42\254\26\63\375\52\263\370"
  "\63\172\12\135\30\351\323\157\22\301\107\40\143\60\170\12\24\114\311\6\363\74\216\367\170\21\176\32\217\341\214\6"
  "\345\371\112\245\264\56\224\320\316\133\123\24\326\213\202\45\374\342\266\70\15\303\233\354\260\26\314\36\371\245\206\230"
  "\34\300\74\301\327\46\116\350\161\114\64\334\340\165\244\236\124\304\216\251\10\6\26\41\223\20\75\344\63\317\344\236"
  "\174\74\103\263\73\105\7\76\60\376\0\120\221\163\25\107\51\141\61\144\307\155\41\37\66\122\335\354\272\252\375\356"
  "\33\241\145\376\311\275\234\35\15\70\100\26\126\226\245\24\6\7\207\301\56\164\351\205\222\122\341\56\220\240\25\240"
  "\256\60\101\13\261\327\310\50\116\253\371\363\277\233\126\313\341\154\125\125\233\161\265\130\263\64\307\154\76\15\220\66"
  "\241\337\346\353\64\320\163\265\134\123\12\234\217\225\331\21\307\256\364\247\43\140\2\45\213\34\207\71\136\131\221\63"
  "\324\225\25\271\323\305\355\21\357\217\51\177\120\126\272\156\204\53\330\35\33\310\103\40\172\153\217\71\144\353\143\337"
  "\333\277\376\55\6\352\365\173\375\347\137\240\37\10\307\133\77\312\262\55\311\234\2\360\27\346\176\274\34\203\240\220"
  "\326\24\256\345\230\336\32\243\134\151\205\12\43\22\34\345\107\34\346\146\130\211\321\324\333\311\144\152\314\36\163\367"
  "\72\160\251\154\351\124\131\126\3\41\374\21\52\120\317\0\43\253\106\136\115\253\66\106\6\121\266\211\314\63\33\222"
  "\243\367\114\135\34\335\104\40\243\343\306\301\173\201\210\143\361\142\206\313\113\327\62\0\73\50\45\376\301\151\24\44"
  "\330\132\301\325\31\166\66\34\327\252\351\207\343\14\143\266\375\133\135\363\104\271\177\134\310\262\124\266\372\42\44\27"
  "\71\34\213\363\3\36\254\106\207\341\161\200\1\173\234\147\337\172\130\55\343\71\111\135\244\141\5\132\230\17\300\256"
  "\73\234\363\47\233\14\12\322\160\332\136\151\353\234\12\270\351\220\325\37\110\56\360\120\205\53\234\214\164\246\44\30"
  "\12\301\205\231\132\346\232\74\207\12\321\261\337\212\345\5\227\333\161\160\63\126\7\246\24\363\21\126\255\23\317\106"
  "\13\276\142\225\367\64\263\260\46\377\361\201\64\326\317\106\210\336\145\31\240\64\303\136\3\302\266\74\362\47\137\357"
  "\56\304\326\23\50\161\13\60\334\153\201\250\244\313\131\174\321\264\325\316\27\242\224\352\124\22\312\52\243\274\157\360"
  "\11\146\62\351\121\375\160\317\332\200\46\217\210\317\4\212\36\321\153\77\135\153\120\203\343\301\135\166\44\170\304\274"
  "\14\131\354\267\203\36\36\123\43\247\325\21\104\126\374\11\320\27\212\177\241\140\244\252\351\117\350\367\110\325\221\314"
  "\362\336\232\35\214\334\344\360\337\351\160\64\235\276\207\230\106\171\133\110\243\112\320\21\213\171\245\237\52\147\46\54"
  "\346\215\357\107\26\306\211\141\51\47\106\12\111\61\157\241\12\345\245\62\361\35\115\274\63\303\122\217\313\75\166\166"
  "\61\320\157\35\151\341\234\50\114\7\22\15\213\112\171\63\64\326\124\325\224\72\22\256\54\254\220\74\14\345\146\246"
  "\337\137\70\157\55\217\110\300\143\224\124\254\72\215\317\67\253\241\356\271\330\122\67\341\31\361\115\73\165\340\172\304"
  "\142\107\25\377\22\4\347\77\323\142\143\245\277\74\255\6\61\275\200\226\124\51\312\34\53\152\220\40\302\274\12\272"
  "\371\26\204\7\363\157\316\324\316\342\142\270\207\100\11\233\132\376\26\316\356\260\121\216\332\250\152\134\210\262\152\147"
  "\123\113\47\213\322\25\342\150\174\260\100\134\262\343\214\76\70\214\245\246\225\220\306\216\204\234\326\231\127\137\17\66"
  "\50\156\205\362\306\111\147\353\304\141\331\165\304\157\10\271\311\130\17\205\236\116\132\310\271\322\40\343\300\335\263\111"
  "\75\120\157\121\20\247\53\37\174\36\310\273\163\35\300\76\255\247\211\77\213\243\302\135\357\124\40\345\361\353\346\154"
  "\331\14\6\136\133\63\121\356\177\16\70\4\234\165\342\23\313\51\33\160\270\277\321\143\341\4\376\350\3\145\255\203"
  "\20\332\203\0\336\301\224\332\226\361\377\366\142\370\177\53\71\270\374\211\74\315\151\153\311\124\64\350\161\255\200\122"
  "\147\126\221\203\204\241\300\46\234\165\115\357\205\265\327\256\333\66\105\255\317\267\320\160\317\240\247\21\321\32\5\116"
  "\223\113\341\265\54\112\41\160\355\7\354\1\234\335\237\170\204\106\364\322\72\57\41\225\336\73\157\164\241\124\141\204"
  "\325\362\110\366\40\136\110\171\105\252\117\330\202\16\226\365\211\145\341\252\354\300\232\132\125\371\133\41\274\165\116\271"
  "\152\40\144\101\276\124\300\154\357\370\150\101\43\362\155\375\267\234\12\274\302\4\166\344\62\371\120\132\52\255\122\45"
  "\154\40\205\63\312\52\43\335\205\211\230\370\245\4\52\135\20\36\77\73\301\360\242\341\175\362\206\364\63\134\232\372"
  "\220\124\27\1\66\123\340\142\263\373\340\232\54\17\0\360\372\57\307\377\250\53\62\357\104\370\374\144\116\137\377\312"
  "\276\372\65\347\232\61\366\266\320\207\166\70\2\116\275\347\347\320\364\267\164\205\360\116\77\76\71\303\376\120\40\144"
  "\367\311\365\254\14\151\130\6\24\326\366\167\70\224\347\201\3\365\227\43\202\130\377\364\360\217\0\341\256\244\123\225"
  "\321\254\170\323\225\132\232\102\172\5\13\271\176\104\21\151\215\115\54\157\160\100\325\264\232\16\165\131\115\31\240\22"
  "\226\67\256\217\307\374\166\354\244\30\17\275\33\353\162\61\120\345\153\320\177\133\12\343\254\321\224\0\75\163\52\34"
  "\154\227\342\235\356\76\224\173\176\211\336\3\104\110\343\364\145\347\162\137\135\275\57\20\112\367\372\107\161\13\234\73"
  "\67\226\365\177\107\325\150\122\352\305\340\165\352\152\263\244\212\327\77\37\135\72\54\255\163\303\341\124\331\111\245\306"
  "\213\201\64\165\137\356\326\111\135\130\247\125\315\6\66\36\260\321\150\142\344\330\116\224\33\115\353\105\131\361\306\124"
  "\316\152\135\12\165\350\316\105\303\346\164\341\312\102\215\252\241\231\210\175\157\266\356\315\334\112\355\245\166\276\356\353"
  "\47\57\252\22\223\341\164\53\116\344\360\337\161\206\362\240\14\110\127\241\155\153\337\142\105\340\5\320\41\210\372\362"
  "\103\365\136\113\16\32\226\155\312\130\365\132\3\113\245\242\334\150\247\37\62\170\17\175\330\324\353\220\166\34\322\360"
  "\6\347\375\263\321\274\323\206\307\135\166\127\367\376\247\126\120\201\155\262\53\154\65\342\216\125\174\174\372\326\365\76"
  "\267\273\351\352\217\346\31\172\323\360\61\206\374\334\305\177\64\233\57\335\116\114\234\74\64\333\17\341\143\12\361\61"
  "\210\217\33\176\10\341\47\76\10\20\127\121\347\277\345\67\41\147\201\217\114\111\137\133\262\346\315\45\360\102\32\135"
  "\372\303\256\11\162\11\242\343\154\260\163\27\20\42\377\115\152\271\0\307\171\24\153\100\306\202\303\313\100\14\315\113"
  "\160\132\2\5\143\110\52\317\44\201\140\226\301\351\24\340\324\217\54\327\26\142\111\316\277\375\5\217\31\271\375\205"
  "\177\346\244\121\306\360\341\321\35\155\170\65\56\357\76\214\200\206\236\201\375\74\74\11\366\264\374\351\316\307\6\63"
  "\125\174\142\71\222\145\22\356\273\15\207\2\323\0\363\61\230\361\105\364\247\56\156\210\124\343\323\64\75\115\326\215"
  "\117\107\222\121\304\303\102\335\22\250\263\142\162\74\171\70\221\116\252\216\347\275\100\66\51\324\351\41\67\245\155\121"
  "\352\161\341\26\3\255\337\366\341\12\257\244\51\353\154\222\24\70\126\207\250\207\246\65\175\223\166\206\55\331\323\324"
  "\375\0\377\110\145\51\114\226\177\160\60\56\234\257\376\161\76\116\3\273\74\246\50\305\200\271\62\66\121\3\330\7"
  "\356\67\133\45\115\332\212\250\231\270\367\204\342\141\134\365\126\172\143\75\330\242\371\361\44\354\113\173\134\143\204\21"
  "\62\14\140\0\224\373\270\323\0\312\267\277\142\72\24\206\266\277\320\174\202\357\307\205\34\133\71\361\125\341\207\264"
  "\67\104\325\177\262\364\361\375\350\162\50\312\211\50\374\330\225\324\217\264\132\152\257\213\42\123\72\24\147\63\201\352"
  "\54\65\51\0\236\336\6\70\52\123\52\123\32\245\112\155\313\167\132\277\355\64\362\361\375\24\306\71\123\272\361\110"
  "\350\111\105\264\362\322\153\133\50\25\17\317\124\112\65\231\352\312\52\143\250\37\123\224\136\226\336\141\232\347\235\215"
  "\246\273\4\153\334\236\226\100\374\2\235\256\45\3\12\30\257\131\344\61\64\11\106\32\231\133\364\63\201\135\347\316"
  "\316\143\166\203\275\257\342\136\373\7\146\217\64\3\225\307\140\367\213\153\137\16\147\156\237\40\77\113\255\22\274\75"
  "\206\163\357\334\222\33\215\313\166\16\161\212\137\212\256\111\376\155\60\207\117\151\165\234\204\277\246\331\257\151\366\153"
  "\232\375\232\146\117\227\300\153\232\35\335\13\2\156\233\241\271\301\327\55\246\147\360\31\106\334\75\147\70\202\42\205"
  "\140\374\160\27\227\232\346\124\304\131\212\155\217\13\4\27\225\333\337\36\315\72\114\253\251\226\162\52\325\110\273\305"
  "\300\34\102\141\175\353\112\243\164\51\174\355\316\150\256\74\170\77\243\361\250\30\111\253\134\145\105\105\331\13\135\123"
  "\314\120\251\43\70\374\236\274\31\76\40\332\105\206\60\325\306\110\155\105\71\165\177\75\301\122\13\51\124\131\226\70"
  "\306\216\362\303\252\315\342\203\136\330\335\254\331\176\174\152\266\307\341\373\43\271\227\266\131\215\216\273\155\174\62\172"
  "\57\154\112\336\317\32\136\374\253\361\315\343\70\5\340\223\302\53\253\255\51\233\27\74\320\255\351\51\146\14\164\201"
  "\147\206\110\234\175\301\226\264\157\342\246\47\174\75\10\173\55\371\376\31\342\0\340\341\262\353\147\0\74\250\140\372"
  "\113\131\303\123\210\362\266\360\252\220\232\337\172\235\373\160\144\14\62\367\337\13\123\112\61\261\152\242\205\225\157\60"
  "\17\12\143\157\245\326\112\226\15\240\37\71\320\75\37\2\216\271\266\220\102\110\151\33\247\342\3\121\144\135\250\102"
  "\33\137\372\106\0\1\104\221\35\116\360\70\127\377\365\277\141\106\74\337\1\210\167\263\20\134\377\363\377\234\27\56"
  "\36\33\77\61\42\7\41\40\77\21\274\0\316\15\115\72\127\31\134\33\304\251\171\67\174\374\317\175\115\162\243\40"
  "\141\275\334\273\31\265\272\347\313\244\361\113\163\353\210\350\150\215\270\33\103\116\50\43\136\305\343\22\367\166\31\227"
  "\363\42\327\47\34\14\172\75\14\371\246\311\164\330\250\152\255\265\124\42\250\133\70\337\126\106\217\275\27\242\260\345"
  "\170\257\240\113\167\340\334\352\213\26\334\326\361\160\66\251\252\206\254\123\364\367\30\224\254\267\21\47\234\21\214\315"
  "\17\126\134\350\123\316\322\370\116\56\250\163\242\45\366\272\5\350\272\5\150\234\266\42\224\101\320\33\117\306\34\144"
  "\376\163\376\55\106\233\204\345\263\144\254\37\237\22\11\221\130\260\63\106\204\240\137\270\135\213\336\314\264\375\51\71"
  "\137\314\265\3\51\224\214\114\362\342\376\320\356\50\14\340\266\257\374\140\117\373\337\151\362\271\215\115\42\260\24\102"
  "\160\252\136\147\0\316\0\256\122\21\102\274\136\1\242\265\162\332\311\262\32\150\301\167\216\103\215\307\356\203\307\211"
  "\314\357\362\247\246\62\253\126\147\233\125\353\301\164\323\174\260\224\255\27\124\253\155\132\233\324\144\253\255\132\155\235"
  "\226\133\45\164\137\132\57\314\276\335\361\47\277\313\362\310\103\45\56\66\121\213\27\244\350\123\234\217\303\253\167\264"
  "\160\3\256\36\16\337\147\271\45\241\70\157\172\270\360\322\226\306\307\345\166\77\103\206\271\346\362\114\71\146\352\52"
  "\75\313\234\153\325\167\223\162\210\345\65\15\315\141\307\324\110\77\47\55\141\216\176\271\214\163\272\372\114\55\260\303"
  "\271\273\304\202\70\234\52\6\211\336\214\30\222\56\0\30\16\202\50\16\222\160\34\204\221\234\117\362\247\265\363\355"
  "\134\32\204\136\37\320\373\351\52\360\62\122\337\311\274\224\236\366\116\27\246\236\23\337\240\23\160\121\66\32\143\334"
  "\61\271\56\235\50\204\64\246\337\344\172\72\127\345\205\3\47\275\372\110\231\217\3\126\64\175\337\305\6\155\254\330"
  "\100\252\323\133\341\156\66\253\344\144\40\175\317\127\254\376\337\361\325\52\37\136\255\12\357\174\221\201\121\331\63\200"
  "\252\352\2\251\164\115\120\323\161\126\46\5\151\305\207\15\76\12\243\154\272\0\252\125\56\224\215\110\101\131\67\206"
  "\15\115\275\342\157\51\74\12\175\157\370\367\272\3\224\113\371\241\233\217\226\22\312\50\70\353\54\112\30\226\370\146"
  "\11\210\204\252\221\160\1\325\4\156\227\6\11\373\0\273\126\271\266\365\304\250\306\212\301\302\251\232\140\137\300\240"
  "\150\165\3\57\37\205\227\60\52\210\104\12\305\7\62\114\362\164\343\306\51\203\350\20\75\126\206\315\143\62\226\313"
  "\160\130\240\336\302\2\145\3\65\145\240\23\322\47\203\243\253\143\70\26\304\364\42\165\207\46\201\263\174\274\357\220"
  "\276\10\230\306\60\70\307\25\305\13\203\300\345\321\357\271\377\72\65\315\31\210\110\4\107\260\216\155\334\45\340\22"
  "\26\251\41\203\134\370\262\150\235\363\357\141\325\213\206\111\137\57\275\374\125\63\342\214\274\145\1\300\61\1\264\4"
  "\233\323\162\234\343\16\16\240\42\230\312\102\132\25\177\172\73\121\357\47\116\374\372\223\210\371\357\262\354\101\322\151"
  "\244\137\123\330\371\222\137\17\2\243\64\70\251\37\20\357\2\56\65\110\361\307\327\300\37\112\126\277\254\337\313\20"
  "\100\45\372\22\100\45\76\201\0\42\367\161\13\213\112\276\176\175\250\226\363\352\361\353\152\66\77\136\123\362\326\334"
  "\66\233\263\155\215\145\270\344\204\37\132\260\154\66\237\233\315\331\45\355\176\233\45\27\36\360\57\245\60\106\173\351"
  "\112\327\155\53\74\137\144\111\313\74\320\374\347\50\205\6\33\74\161\242\370\371\150\15\362\333\237\230\372\241\133\14"
  "\212\327\160\120\10\177\352\340\275\335\361\365\100\41\207\102\226\143\351\344\304\56\6\122\277\227\106\13\131\172\43\225"
  "\53\75\354\215\226\365\46\225\266\305\110\116\253\142\262\357\254\254\73\123\267\205\265\205\226\272\140\305\314\0\264\312"
  "\126\316\17\325\250\34\127\243\305\100\271\3\150\352\326\12\53\12\47\164\335\135\74\154\166\122\352\241\35\216\247\23"
  "\133\214\151\71\312\372\322\225\22\37\200\223\377\27\160\261\373\154\233\53\171\366\174\224\272\226\376\114\371\126\133\77"
  "\0\305\365\234\173\227\311\330\343\52\221\320\360\311\151\253\55\3\211\321\51\12\73\234\352\333\362\167\63\316\10\74"
  "\347\52\336\212\215\237\126\300\212\241\102\311\141\253\62\161\107\155\154\324\376\330\344\267\152\127\253\226\276\73\51\277"
  "\275\3\333\172\314\136\25\333\67\363\145\44\120\75\301\175\75\143\151\145\121\324\45\306\357\326\113\152\177\370\53\43"
  "\15\4\131\302\221\264\145\255\110\334\253\361\62\136\110\353\13\335\330\330\203\41\123\105\145\246\136\224\143\253\206\173"
  "\123\250\324\1\262\77\155\241\61\165\167\105\64\154\262\252\312\152\64\31\231\221\34\233\175\157\157\206\125\370\302\26"
  "\352\165\323\121\31\17\233\237\16\253\162\124\171\257\375\144\337\233\56\336\152\200\244\266\245\63\205\253\255\241\304\326"
  "\260\377\137\260\321\74\126\246\46\274\260\136\112\46\170\211\302\222\152\176\330\31\255\11\306\25\10\343\26\333\207\56"
  "\253\141\230\264\112\151\143\112\27\177\162\245\152\220\213\116\53\225\312\362\254\144\155\37\60\104\74\33\210\223\175\351"
  "\247\327\244\337\257\307\221\232\117\316\311\121\64\120\260\222\153\327\235\373\141\367\244\373\207\74\221\260\273\140\256\7"
  "\333\114\260\57\265\36\376\324\214\300\113\263\271\342\236\125\374\16\17\356\327\21\174\227\270\207\43\335\61\243\315\355"
  "\375\247\33\362\357\126\230\155\360\116\4\36\147\342\253\207\351\343\314\356\147\372\371\145\374\156\353\242\221\305\4\372"
  "\230\52\132\200\361\303\104\42\324\33\262\34\302\215\6\110\337\222\301\160\40\365\25\171\123\67\66\355\102\12\345\5"
  "\336\273\232\176\222\45\336\356\4\324\75\332\37\305\31\261\124\162\72\262\306\30\65\221\376\335\270\26\336\352\322\113"
  "\247\1\261\231\21\15\372\353\103\247\234\326\322\330\261\50\26\3\153\137\217\242\23\116\72\131\247\301\144\1\362\23"
  "\251\227\261\276\204\323\114\11\7\122\1\322\246\7\206\211\127\30\360\12\306\177\306\205\252\64\112\347\156\224\311\323"
  "\217\21\171\372\261\56\117\77\205\312\323\217\217\56\10\276\210\224\352\256\13\156\62\23\255\165\46\132\233\114\74\355"
  "\114\246\271\317\304\323\45\321\371\242\323\15\74\77\234\260\232\263\342\72\77\242\123\376\325\22\136\25\216\263\323\230"
  "\34\114\151\57\273\114\154\230\77\126\37\222\342\115\200\32\301\56\136\362\73\74\344\21\342\275\45\317\154\254\230\173"
  "\201\320\61\314\212\5\72\224\361\0\200\21\313\344\55\351\174\301\367\142\243\17\236\361\355\116\64\353\40\270\307\313"
  "\66\140\35\12\371\376\140\367\366\113\367\33\267\51\36\170\57\315\160\54\250\311\220\201\12\45\135\200\156\3\4\347"
  "\114\237\36\341\274\260\271\147\251\45\126\122\167\31\264\135\177\50\376\135\363\171\350\240\54\127\260\77\226\225\213\374"
  "\352\371\174\353\274\57\361\31\266\145\365\375\353\142\366\244\332\151\266\72\161\105\151\266\146\263\22\315\366\274\365\372"
  "\176\42\307\315\104\34\255\100\322\105\334\273\326\305\334\233\126\373\271\325\136\264\332\17\315\366\170\270\134\156\233\217"
  "\346\135\156\2\157\165\367\275\331\374\103\264\372\272\243\366\221\264\342\364\245\325\176\156\267\177\127\202\77\222\315\107"
  "\243\305\115\163\224\151\373\301\114\266\37\250\366\3\335\176\140\332\17\154\373\201\153\77\50\332\17\174\373\101\311\0"
  "\23\354\211\144\117\24\173\242\331\23\303\236\130\366\304\261\47\5\173\342\331\23\6\263\22\354\11\247\257\142\117\64"
  "\173\142\330\23\313\236\70\366\244\140\117\152\230\111\162\276\75\76\215\336\67\76\55\327\377\156\265\270\141\352\152\317"
  "\62\341\227\331\56\13\321\166\162\350\303\25\373\20\17\177\140\320\243\376\354\154\76\133\177\275\347\307\215\327\232\242"
  "\15\226\11\37\221\113\215\143\136\61\317\152\323\210\164\335\342\202\1\61\347\256\53\270\262\361\110\331\254\140\356\346"
  "\351\322\140\302\262\266\306\137\127\34\366\347\310\364\37\173\215\303\333\310\147\42\300\225\141\57\21\54\40\241\115\63"
  "\26\242\335\37\42\324\173\343\121\347\113\63\331\234\320\354\207\116\222\373\376\372\144\264\177\364\360\333\215\272\201\204"
  "\244\353\121\236\26\277\335\310\33\164\17\42\215\101\133\245\50\303\275\173\33\176\371\165\66\245\341\337\240\321\274\363"
  "\51\227\140\57\70\206\34\236\270\343\247\371\302\316\21\4\357\61\117\362\254\175\327\4\51\345\363\231\36\131\277\74"
  "\175\175\172\256\226\173\227\205\303\226\133\221\360\160\217\247\301\127\353\247\145\305\302\363\367\27\217\116\344\214\77\1"
  "\342\25\340\303\311\362\151\301\311\200\11\3\172\277\157\367\76\132\22\267\357\221\152\64\30\347\332\256\162\144\60\205"
  "\110\226\261\40\331\46\70\256\331\364\10\72\302\77\61\243\77\175\371\53\162\5\107\56\254\273\60\162\276\211\115\331"
  "\354\235\206\7\151\205\373\174\307\322\247\252\51\240\273\147\340\13\146\36\162\141\203\3\103\232\147\222\201\362\246\57"
  "\71\54\110\364\132\15\37\145\62\214\70\253\315\120\1\216\214\125\227\356\314\352\222\10\151\210\166\255\206\145\60\34"
  "\165\44\167\177\225\363\145\65\176\372\276\370\261\256\32\362\51\145\224\62\144\243\355\370\150\151\163\313\72\76\343\345"
  "\36\15\172\110\321\315\171\370\266\16\253\75\242\343\73\135\325\115\104\231\227\62\265\222\266\267\245\23\306\31\103\207"
  "\265\343\174\20\57\106\301\11\140\322\12\154\242\60\7\233\56\102\246\171\207\61\145\147\257\364\220\256\50\12\105\167"
  "\76\6\104\225\245\171\101\215\316\121\311\66\171\270\26\204\147\147\264\145\140\355\177\367\127\237\110\135\262\247\107\224"
  "\302\236\236\40\145\310\132\362\46\42\253\375\207\10\336\335\267\332\77\33\315\31\113\307\260\257\256\271\67\44\277\340"
  "\153\272\350\102\131\136\205\35\200\165\272\127\145\150\273\12\103\202\211\10\43\372\74\202\202\363\200\276\101\165\63\330"
  "\330\334\5\147\13\136\233\17\64\33\231\30\314\203\71\202\64\220\267\40\324\150\154\266\376\207\54\361\34\73\246\150"
  "\162\63\23\11\133\7\26\117\321\63\100\114\206\147\72\171\171\26\10\233\211\313\345\54\375\141\316\272\243\304\106\250"
  "\57\134\4\172\371\104\112\47\302\56\365\214\246\157\301\363\241\125\0\202\350\32\316\273\124\305\70\357\150\345\347\130"
  "\161\37\226\267\232\116\260\351\234\261\4\311\50\316\102\217\247\335\162\323\162\313\355\115\136\357\27\163\41\10\224\210"
  "\162\111\22\166\237\164\56\271\14\117\336\231\3\3\226\173\173\367\366\71\152\11\12\232\235\101\175\146\164\134\10\35"
  "\233\17\35\162\155\155\157\251\7\251\311\247\146\55\163\223\356\241\65\245\321\265\244\323\267\332\5\36\211\233\217\67"
  "\361\137\17\107\217\325\241\217\103\307\64\132\215\5\365\375\123\35\41\6\3\75\213\67\153\136\173\357\4\337\3\203"
  "\331\153\264\144\124\361\27\100\25\16\3\75\213\246\212\221\245\321\102\147\240\12\265\42\155\260\312\276\101\146\7\242"
  "\52\140\121\200\151\340\143\245\103\212\3\153\214\111\254\367\263\353\274\12\122\277\313\346\4\56\264\320\2\254\350\56"
  "\11\300\337\51\273\56\55\175\73\275\106\53\156\232\20\7\126\110\173\367\200\370\116\64\242\60\255\323\311\104\3\207"
  "\11\316\14\134\111\42\335\154\5\246\71\332\75\43\332\353\233\250\13\102\210\33\224\212\324\276\17\340\230\267\174\163"
  "\7\320\16\362\241\156\361\241\151\20\330\160\234\302\50\101\107\206\110\311\347\131\51\32\270\335\142\100\72\40\336\10"
  "\12\46\336\312\306\206\63\34\167\76\107\270\102\303\265\20\52\156\172\134\374\114\134\217\53\157\272\327\121\74\3\200"
  "\372\251\263\0\353\205\64\15\145\317\113\372\312\347\135\323\127\145\223\257\264\270\214\125\375\353\252\276\26\275\55\353"
  "\53\117\272\223\265\312\113\130\331\227\342\203\113\373\252\350\55\300\126\226\350\307\132\356\46\301\106\360\325\177\32\215"
  "\326\376\303\356\65\173\53\277\247\235\352\66\323\7\40\25\240\14\52\355\154\352\61\171\363\127\265\251\125\154\360\333"
  "\207\177\5\250\227\201\240\304\227\71\111\273\44\102\66\371\253\361\230\210\37\116\266\163\302\340\5\216\14\13\272\270"
  "\36\201\47\365\177\331\72\17\300\314\130\347\312\137\267\322\3\373\366\351\72\210\167\102\202\322\313\274\4\312\334\222"
  "\350\106\122\332\3\340\341\322\41\14\67\110\241\367\221\331\142\22\110\326\306\40\263\316\3\110\255\43\62\63\42\66"
  "\73\244\115\313\217\267\27\341\266\61\272\144\50\122\313\36\350\121\177\300\225\344\150\244\33\7\32\32\204\1\266\267"
  "\304\231\156\254\5\65\132\234\277\212\4\216\167\131\71\276\150\101\344\233\155\43\156\62\304\335\370\202\112\102\257\304"
  "\251\57\332\2\206\357\57\2\375\354\70\55\331\234\323\60\350\332\315\172\226\303\264\66\242\105\133\31\220\153\236\354"
  "\371\277\377\355\111\161\164\372\115\26\140\55\207\43\260\237\234\15\60\262\267\274\252\156\144\314\233\55\43\242\314\212"
  "\62\214\340\11\334\233\156\166\354\7\263\5\332\367\247\164\35\321\267\331\342\52\316\250\146\333\332\326\357\276\325\156"
  "\177\257\157\302\233\140\151\141\275\176\357\360\361\241\307\303\60\64\366\33\37\350\34\312\225\0\263\335\255\211\61\71"
  "\255\211\151\223\322\141\317\45\221\367\147\347\320\46\130\201\270\336\130\332\30\142\25\326\262\34\214\355\21\11\46\150"
  "\141\105\34\76\263\221\317\141\40\346\171\341\107\21\306\357\270\34\55\231\224\24\215\226\377\240\314\74\67\205\137\44"
  "\310\114\231\123\146\254\150\251\243\143\136\301\363\57\46\63\264\133\235\20\352\103\146\32\146\266\331\262\42\116\146\212"
  "\163\311\314\363\51\231\171\116\223\231\60\133\360\103\33\117\174\364\334\110\36\101\231\101\353\366\114\142\254\352\134\265"
  "\172\132\146\164\113\146\314\147\223\31\323\333\102\213\125\44\45\254\245\103\62\323\40\10\360\43\375\245\113\24\266\102"
  "\334\113\264\11\241\273\355\26\272\203\35\225\266\150\101\344\157\362\163\174\177\173\365\56\137\76\177\127\242\177\220\271"
  "\70\61\357\203\36\2\344\176\135\326\40\352\223\225\342\263\105\122\175\341\330\363\131\125\342\6\340\214\343\155\333\137"
  "\274\155\33\21\166\243\305\364\222\23\11\232\62\306\305\226\261\232\322\211\26\104\362\252\51\317\254\51\345\5\150\112"
  "\305\246\231\36\176\156\115\51\77\263\246\124\67\10\147\174\104\211\354\117\123\66\202\320\146\213\351\245\14\371\300\216"
  "\212\326\251\10\105\253\142\163\31\116\267\20\62\237\54\377\347\114\157\214\343\24\261\12\153\351\344\220\52\46\7\242"
  "\250\373\204\304\2\326\55\351\261\244\62\235\40\363\371\40\223\16\201\226\276\134\102\21\54\232\30\227\74\61\24\12"
  "\367\60\113\105\302\54\345\7\23\117\231\17\117\131\74\24\254\214\317\250\33\274\235\74\174\45\327\67\176\10\54\77"
  "\313\65\174\62\64\225\335\4\16\206\146\367\257\241\143\236\167\47\157\124\133\237\274\215\155\72\157\266\53\321\345\100"
  "\347\371\46\174\32\164\312\355\155\365\225\375\364\344\172\76\163\346\363\231\71\204\76\363\371\314\347\71\105\131\253\63"
  "\34\242\374\11\357\4\54\224\62\132\226\226\247\351\143\75\13\31\44\313\131\355\21\337\136\224\276\155\211\367\223\244"
  "\270\73\336\73\247\113\345\155\241\42\157\217\273\333\244\36\173\262\71\165\75\134\243\4\364\326\26\242\50\235\246\333"
  "\341\340\356\153\200\242\52\145\121\304\335\42\262\375\300\211\172\43\343\344\237\67\321\55\6\332\324\330\270\133\121\24"
  "\122\10\53\351\226\147\206\322\71\131\324\4\240\147\275\342\100\206\301\234\37\167\354\301\142\372\37\230\11\300\0\130"
  "\12\160\204\215\344\10\55\113\77\124\225\320\205\50\364\142\340\376\274\106\134\51\345\234\262\173\210\324\117\340\10\227"
  "\235\43\362\343\16\174\142\46\344\351\332\222\230\54\263\306\42\342\367\256\261\212\374\32\213\120\72\73\177\372\213\324"
  "\130\4\153\377\32\13\263\24\342\210\62\277\306\352\221\43\244\270\70\225\5\220\307\52\153\220\256\263\22\257\30\56"
  "\215\162\265\142\1\116\166\277\127\103\263\133\305\370\264\30\73\325\302\115\306\345\330\173\375\176\320\237\323\316\311\262"
  "\50\24\333\257\221\160\126\40\115\46\337\354\304\200\234\107\357\3\234\203\233\276\350\245\164\63\303\167\350\341\53\35"
  "\151\314\4\215\210\257\15\6\11\52\315\202\15\260\305\202\70\232\357\137\230\327\151\250\257\167\140\330\176\316\272\234"
  "\127\61\333\70\240\41\7\173\74\371\164\276\244\272\325\130\153\50\201\16\356\274\217\302\103\345\307\43\101\377\367\224"
  "\45\41\342\324\151\101\174\112\304\341\65\104\371\37\374\5\123\200\113\111\33\163\300\223\330\70\162\347\23\265\202\132"
  "\14\370\67\31\64\312\32\163\16\326\200\251\16\0\203\27\103\267\144\320\1\340\301\235\364\120\332\364\245\111\333\125"
  "\332\114\231\117\332\0\367\36\376\0\367\136\202\264\215\274\56\207\312\52\141\312\261\134\14\244\70\250\7\177\153\12"
  "\157\124\341\334\36\140\255\176\5\161\63\347\20\67\174\30\143\172\70\145\1\300\11\341\124\16\164\161\236\231\73\215"
  "\151\273\225\0\166\375\57\154\313\42\32\104\237\12\42\130\137\230\163\215\226\71\241\66\357\67\342\125\322\70\243\255"
  "\50\170\225\121\74\353\224\147\332\247\260\11\75\111\225\30\32\5\363\364\317\131\203\2\131\5\154\340\353\145\367\224"
  "\230\205\247\102\214\73\135\67\125\211\300\70\350\172\124\1\356\271\340\136\116\202\67\324\64\202\104\30\376\331\217\166"
  "\252\346\375\56\166\313\63\64\251\327\332\347\277\43\225\272\140\251\202\14\127\244\346\53\3\26\361\7\346\244\57\12"
  "\363\336\363\143\225\230\63\305\207\113\1\157\163\327\373\145\250\370\272\122\134\102\52\135\102\175\370\256\253\366\325\14"
  "\231\371\246\271\7\276\161\204\236\305\34\117\175\364\127\77\236\361\340\102\14\72\247\27\125\113\37\75\27\317\262\173"
  "\36\61\5\267\1\73\0\140\125\354\14\253\257\137\37\252\345\274\172\244\152\72\206\300\171\375\142\245\56\336\57\126"
  "\372\23\370\305\300\45\232\237\247\106\163\65\233\37\57\317\244\235\257\240\36\223\27\105\342\12\307\63\26\323\111\167"
  "\236\142\272\364\155\21\273\213\50\305\233\375\314\122\274\115\304\371\241\101\315\107\74\232\41\320\314\33\13\342\370\54"
  "\77\330\233\260\375\341\107\37\2\253\101\67\371\360\33\330\271\17\101\156\63\167\351\321\152\36\143\61\72\252\250\356"
  "\241\356\264\36\207\206\6\236\126\302\6\72\144\14\270\313\220\237\31\311\217\355\17\263\0\42\343\247\125\16\104\314"
  "\245\116\21\131\367\34\150\332\113\230\257\354\130\311\33\174\10\353\227\371\360\74\276\7\141\324\253\357\161\365\75\256"
  "\276\7\26\262\253\253\161\165\65\176\115\203\254\377\36\6\331\174\112\203\154\57\162\362\176\175\127\143\75\274\246\71"
  "\256\256\306\317\114\163\140\101\133\17\377\256\151\16\246\22\316\345\203\364\257\127\273\135\354\36\330\263\24\130\52\316"
  "\315\111\130\215\16\23\322\305\353\223\333\367\27\315\346\37\315\346\370\344\346\376\25\65\371\366\173\256\242\37\337\332"
  "\237\162\147\264\53\265\62\36\313\163\206\362\12\46\221\161\40\61\105\101\235\321\345\263\335\117\220\1\73\360\214\232"
  "\110\37\271\65\2\365\371\347\256\7\143\307\242\130\14\354\133\241\262\124\132\151\123\227\51\27\254\137\266\157\204\51"
  "\276\4\135\233\132\43\64\231\75\47\333\14\51\12\351\214\161\170\17\370\211\56\214\321\136\272\322\5\273\140\44\57"
  "\52\243\307\336\13\121\330\162\274\370\122\226\356\60\221\325\27\55\310\11\341\250\23\305\343\357\23\242\233\303\326\210"
  "\325\370\34\306\107\320\33\316\40\160\157\223\375\353\337\142\240\137\67\46\111\367\372\127\34\41\4\357\147\120\167\144"
  "\46\116\52\241\253\221\233\26\223\305\100\325\35\15\304\255\126\326\72\353\145\211\172\42\361\32\113\153\105\345\275\61"
  "\226\40\122\102\52\251\254\126\35\40\362\112\371\221\165\336\117\365\150\61\260\157\20\11\43\204\265\126\33\33\17\122"
  "\65\255\104\141\205\34\51\357\27\3\51\137\141\22\102\24\245\264\332\224\245\211\356\113\351\111\45\305\144\72\231\26"
  "\242\334\367\165\0\113\337\232\242\324\132\212\303\15\100\26\311\172\377\277\60\125\275\350\56\0\160\302\264\36\252\102"
  "\216\375\120\115\314\150\361\105\36\46\114\335\32\241\165\151\112\330\21\201\241\206\225\55\307\266\364\303\261\177\353\107"
  "\355\237\212\322\330\302\305\303\343\204\163\143\71\362\23\41\155\75\125\157\14\344\274\127\136\227\205\212\6\110\217\374"
  "\330\216\244\252\112\345\325\142\140\336\330\247\50\204\366\322\12\327\347\174\263\73\351\30\12\107\256\204\144\212\36\24"
  "\251\157\42\236\274\44\231\263\154\167\272\365\263\273\224\154\45\123\307\57\251\373\104\257\272\375\252\333\123\176\271\352"
  "\366\253\156\137\207\57\262\374\143\271\146\35\254\142\343\61\43\274\54\101\210\230\156\137\222\243\60\300\304\207\131\77"
  "\72\325\53\374\204\300\2\163\224\41\336\203\373\304\326\47\2\205\25\67\75\321\266\157\360\136\257\357\2\365\372\24"
  "\116\343\2\376\364\315\176\230\360\104\30\64\155\154\246\302\234\67\376\70\363\56\63\360\141\60\227\260\314\306\150\351"
  "\40\216\3\60\100\236\135\100\261\176\167\200\12\157\165\351\245\113\227\153\314\72\134\173\246\303\207\130\46\101\262\347"
  "\223\214\147\341\255\343\22\106\153\276\10\31\225\312\115\257\276\211\313\335\56\117\346\175\127\47\23\275\223\351\65\265"
  "\173\115\355\166\20\236\356\53\227\74\7\12\335\27\371\32\51\110\43\155\251\34\5\11\114\102\57\46\267\73\237\364"
  "\225\332\225\252\124\306\110\27\136\51\115\363\15\1\76\11\31\326\135\246\50\174\227\55\12\337\145\212\302\167\371\242"
  "\360\135\172\24\216\372\272\106\341\104\231\134\121\370\56\117\24\276\313\26\205\357\376\206\31\326\145\272\226\313\264\364"
  "\210\165\343\22\307\1\274\21\66\230\351\273\16\323\123\302\174\36\263\244\147\257\206\341\152\30\360\57\127\303\160\65"
  "\14\273\204\364\154\212\145\41\315\264\304\51\247\154\311\113\234\322\305\61\121\316\224\356\13\232\70\154\244\322\115\15"
  "\37\40\247\251\1\54\104\257\363\344\360\144\172\51\331\341\311\24\237\77\102\57\361\107\211\11\142\372\74\41\103\174"
  "\265\365\127\133\317\176\271\332\372\253\255\357\313\124\363\225\231\20\110\57\351\346\224\351\135\154\117\231\11\305\233\165"
  "\42\127\33\72\157\300\341\53\6\262\325\126\324\146\213\23\370\162\266\331\204\137\326\366\111\327\20\244\221\276\54\225"
  "\63\275\254\41\74\275\244\156\101\11\235\362\316\20\112\337\355\102\113\16\370\70\171\102\206\347\64\360\111\356\134\200"
  "\322\266\327\340\302\361\224\265\22\166\102\176\55\240\167\263\320\25\231\264\275\45\360\351\343\23\372\24\140\104\64\114"
  "\5\214\257\201\246\103\311\103\212\304\333\64\12\251\275\262\312\177\244\150\136\51\157\254\61\237\255\150\36\137\171\102"
  "\347\363\261\22\373\176\153\104\31\100\261\353\244\336\227\336\272\17\114\176\15\155\341\175\210\177\360\121\232\223\144\377"
  "\201\57\245\45\160\6\11\122\6\30\160\240\267\11\53\344\0\127\361\261\322\121\114\357\52\1\105\276\355\4\337\341"
  "\130\110\45\104\34\53\251\144\162\371\143\70\336\132\106\260\44\362\340\141\40\27\347\141\47\35\41\152\203\20\116\3"
  "\11\300\304\23\117\377\222\165\125\234\107\157\361\127\0\354\216\333\335\177\356\272\313\366\245\123\77\322\145\352\110\253"
  "\114\35\31\237\251\43\147\62\165\344\211\330\27\236\112\131\311\17\242\232\213\366\52\27\355\115\56\106\267\271\30\275"
  "\40\106\277\214\71\127\334\176\115\122\17\260\335\200\47\1\46\133\51\0\60\210\11\100\340\302\121\313\162\170\162\176"
  "\134\101\350\164\146\114\260\57\205\113\161\167\351\165\11\215\144\124\300\274\121\214\211\56\361\44\276\210\113\217\251\146"
  "\176\154\173\200\42\275\70\367\241\231\336\372\176\72\33\166\267\75\135\140\73\333\262\202\333\146\273\365\373\327\257\353"
  "\72\252\152\74\370\135\212\346\263\335\135\142\306\215\374\226\272\337\63\136\25\305\345\211\306\353\130\110\233\224\321\133"
  "\5\356\224\330\166\41\120\115\370\63\322\150\313\240\243\41\21\231\370\267\167\333\144\62\155\143\156\343\170\344\312\40"
  "\370\16\73\354\236\227\61\153\311\157\4\143\57\203\244\347\217\217\45\75\71\352\104\132\200\347\266\147\74\23\62\241"
  "\333\14\231\320\176\153\326\267\154\155\237\57\202\100\14\266\150\2\126\340\30\41\114\23\55\100\217\212\367\110\204\151"
  "\312\336\367\70\226\114\44\356\171\16\216\374\176\372\150\310\300\235\37\241\43\26\267\340\230\302\34\33\13\10\147\352"
  "\155\20\352\156\200\373\153\240\210\225\147\242\170\140\170\305\1\136\264\57\24\227\322\2\61\12\172\176\253\352\261\32"
  "\343\335\126\111\353\113\1\301\10\164\200\104\300\65\271\132\212\146\273\154\66\175\263\131\104\310\100\161\123\177\126\367"
  "\164\350\235\106\334\323\202\172\110\332\155\307\271\317\43\156\246\376\6\121\35\226\107\341\123\223\262\30\27\123\255\225"
  "\234\250\367\22\10\155\235\54\215\51\303\275\111\301\341\13\365\67\210\351\320\335\220\15\157\316\243\154\315\253\155\265"
  "\115\253\255\133\155\254\336\16\57\35\276\74\164\167\30\203\6\176\103\130\45\234\0\311\311\246\71\331\102\335\14\116"
  "\365\223\127\165\312\354\272\63\160\242\151\202\270\177\122\175\11\274\362\206\23\206\57\23\247\220\346\241\375\211\3\273"
  "\2\23\250\302\62\12\351\173\36\160\235\1\307\150\340\130\77\270\322\40\161\63\144\74\266\133\260\146\321\313\112\54"
  "\360\255\71\56\114\100\111\157\266\355\151\331\152\373\126\33\233\320\303\113\207\57\17\335\321\30\365\340\307\173\330\205"
  "\24\214\17\274\333\363\205\331\273\273\256\140\340\253\44\171\217\167\114\4\336\3\177\117\77\155\236\226\341\53\54\123"
  "\156\310\304\25\317\114\375\4\347\253\114\66\130\214\272\270\236\137\277\52\56\245\214\63\256\56\225\225\16\236\106\302"
  "\200\46\366\74\313\370\351\324\140\100\343\153\314\356\362\36\114\173\167\272\234\360\376\222\117\40\270\317\130\75\230\361"
  "\260\150\220\303\211\337\134\301\223\220\374\102\377\344\53\20\357\170\236\237\17\25\137\0\126\50\57\12\367\241\2\60"
  "\255\265\226\252\303\146\312\161\61\222\123\75\36\216\13\243\365\136\310\204\54\337\53\300\212\113\255\0\343\357\377\24"
  "\107\242\332\54\276\113\176\300\113\342\301\30\220\323\322\353\242\326\247\353\312\327\211\37\344\50\130\42\375\315\107\204"
  "\251\317\170\114\362\3\237\124\33\17\230\302\53\253\255\51\243\325\317\245\111\3\46\103\317\207\320\20\131\135\51\71"
  "\131\323\227\122\320\120\11\365\343\254\267\201\364\136\73\135\70\335\274\124\342\107\122\51\171\272\331\354\221\263\210\173"
  "\0\233\241\2\63\56\32\57\370\300\125\374\1\344\346\144\23\313\307\301\67\342\345\163\143\57\377\244\254\331\145\372"
  "\251\174\322\172\367\371\12\51\204\224\326\202\56\56\336\141\112\77\236\26\155\212\303\64\224\126\50\151\243\247\341\45"
  "\60\341\311\150\44\370\140\57\135\16\302\304\121\325\245\361\11\344\22\154\300\60\71\323\73\303\324\315\355\375\45\101"
  "\174\365\14\257\236\341\325\63\214\107\256\17\257\221\357\216\236\315\247\135\156\47\114\165\373\2\325\237\367\360\300\324"
  "\100\275\346\305\326\142\336\147\254\305\354\77\77\212\63\223\374\313\164\327\203\57\314\165\356\12\157\324\355\37\62\274"
  "\236\321\213\237\236\377\372\262\40\46\171\75\334\115\256\171\313\227\34\303\247\33\247\345\107\57\300\204\345\17\220\122"
  "\23\305\335\277\206\211\135\130\66\300\267\101\362\361\6\277\322\274\221\167\300\227\207\362\23\33\227\62\244\252\141\336"
  "\141\272\205\2\33\146\10\122\260\231\46\176\303\314\270\232\75\376\366\257\367\63\363\264\134\377\366\117\165\353\237\376"
  "\15\366\234\116\351\262\272\17\70\354\364\361\351\151\111\343\36\232\37\35\370\320\11\34\171\371\364\143\76\241\221\17"
  "\315\264\221\203\174\176\350\23\2\262\136\356\377\235\0\71\64\77\112\202\103\47\160\344\171\65\134\216\266\173\1\246"
  "\321\337\37\175\24\202\272\243\152\265\306\14\260\334\17\115\343\37\232\241\261\337\232\325\142\275\254\45\6\102\303\236"
  "\324\137\6\147\353\60\64\204\367\161\322\200\267\156\42\170\347\151\300\316\203\220\256\366\317\106\163\10\352\353\153\4"
  "\353\153\373\102\201\375\376\64\231\22\250\165\13\0\72\113\346\202\331\51\56\250\107\306\114\273\177\213\200\255\133\0"
  "\330\155\234\364\304\357\170\251\307\304\140\16\107\53\2\263\156\175\124\252\367\135\140\113\362\264\330\256\146\337\346\64"
  "\362\237\117\316\104\44\2\357\317\201\40\214\365\141\240\4\137\335\372\50\145\352\76\340\260\213\247\27\32\165\337\350"
  "\233\153\366\103\142\5\367\364\215\140\334\67\62\333\304\175\217\61\40\110\321\0\102\212\374\140\110\1\1\151\250\172"
  "\122\364\271\200\210\261\67\7\137\231\200\70\64\363\203\361\135\106\350\274\341\206\340\250\133\210\173\23\241\132\315\207"
  "\163\260\7\20\106\304\133\320\155\102\252\0\52\237\75\105\42\210\70\233\23\21\353\326\225\210\115\42\316\42\224\367"
  "\137\151\270\157\144\26\207\175\217\21\66\156\105\40\354\33\231\101\330\367\210\303\206\341\234\100\330\67\62\203\260\36"
  "\142\52\14\33\63\61\314\77\25\165\227\30\212\306\144\14\363\317\106\335\45\206\242\61\37\303\374\23\122\167\31\5"
  "\205\152\202\241\10\216\216\347\137\60\11\215\7\124\305\10\361\135\103\212\357\362\213\361\135\214\34\337\65\4\71\67"
  "\24\165\227\61\242\374\27\50\352\126\176\141\76\6\305\376\277\377\37";

/* lib/stat.wo (DEFLATEd, org. size 9195) */
static unsigned char file_l_39[1077] =
  "\264\227\313\156\343\40\24\206\367\43\315\73\240\310\213\144\123\371\156\22\251\317\202\210\215\123\64\30\54\300\155\347\355"
  "\307\70\27\142\202\144\63\162\273\110\71\77\234\303\377\301\151\354\356\73\321\14\214\200\110\151\254\177\377\2\140\117"
  "\273\136\110\15\166\265\324\73\260\123\175\264\3\373\13\23\147\314\100\64\152\247\121\1\373\156\320\200\146\351\341\160"
  "\360\344\164\244\23\362\357\230\166\35\134\323\156\343\330\146\54\355\142\66\160\126\177\141\105\221\342\270\127\37\102\243"
  "\136\222\117\112\276\222\261\100\333\240\226\62\142\40\320\205\350\261\130\73\360\32\104\376\204\223\263\34\354\173\54\161"
  "\67\155\70\37\113\242\6\346\240\56\130\351\261\376\100\265\44\130\23\324\120\111\152\75\162\57\31\362\46\271\266\66"
  "\265\30\164\136\57\11\353\254\205\133\126\132\122\176\331\115\3\106\370\303\327\125\77\135\325\265\305\210\224\134\354\356"
  "\277\155\177\115\361\151\372\364\364\130\133\163\315\114\113\121\336\40\111\230\141\177\370\230\46\117\317\123\353\51\55\13"
  "\326\47\115\73\163\230\135\217\264\100\123\320\223\332\54\2\367\42\321\143\11\240\145\176\170\310\375\265\244\215\271\272"
  "\12\267\154\46\152\3\111\223\150\222\255\220\106\317\353\46\365\355\102\264\251\370\54\151\102\246\154\167\231\365\63\315"
  "\230\132\157\265\340\112\203\44\276\377\114\63\306\356\33\371\326\204\67\150\134\204\224\125\33\372\211\6\33\52\55\44"
  "\1\242\155\25\321\357\61\300\214\136\370\73\174\365\307\325\253\301\364\107\14\112\322\241\301\246\177\111\334\243\121\267"
  "\212\327\362\155\136\22\75\110\156\306\257\327\55\120\77\234\31\255\221\11\347\367\114\371\374\102\305\240\203\56\324\12"
  "\231\53\344\256\120\270\102\351\12\225\53\100\127\70\272\102\22\373\73\313\240\254\153\55\312\355\25\60\201\33\177\117"
  "\4\364\214\335\331\166\1\264\2\156\32\117\107\255\367\5\267\364\225\224\13\306\262\365\306\322\174\113\147\131\272\340"
  "\54\137\357\54\113\3\235\55\227\314\203\140\362\330\205\131\114\51\163\117\312\230\300\26\276\304\127\332\207\101\366\141"
  "\270\175\370\223\366\213\62\304\176\121\6\333\257\322\377\266\177\36\53\377\1\121\22\315\102\70\17\253\171\130\316\303"
  "\142\36\346\363\60\233\207\336\257\16\353\336\34\33\104\303\375\344\222\373\311\45\56\362\223\240\206\363\155\13\211\64"
  "\76\63\142\166\61\33\33\57\306\236\161\154\40\14\227\105\45\274\361\330\361\236\157\232\57\374\165\27\156\231\207\146"
  "\261\174\217\102\147\227\242\52\255\50\344\272\147\351\131\72\114\331\66\114\245\207\251\14\145\202\311\61\335\0\51\337"
  "\6\251\362\40\125\241\110\111\231\301\174\3\246\142\33\46\350\141\202\241\114\131\132\225\160\3\246\362\71\250\266\1"
  "\74\172\0\217\241\200\371\61\51\266\150\104\270\15\123\22\273\165\254\30\102\25\37\313\170\3\52\23\54\274\226\267"
  "\346\23\354\311\367\355\137\77\23\356\16\363\327\363\266\371\327\236\131\340\66\0\303\120\364\102\243\342\171\312\314\355"
  "\375\265\130\372\222\222\127\205\306\220\255\350\260\363\277\251\310\267\256\314\357\30\252\133\202\171\274\204\101\362\344\204"
  "\250\171\41\201\206\132\156\52\235\251\376\21\350\362\152\162\67\107\246\63\103\27\270\22\215\273\162\134\321\352\213\23"
  "\157\327\164\103\221\272\311\123\227\26\140\200\45\151\360\356\114\203\101\157\340\141\360\60\143\17\17\66\207\271\76\57"
  "\260\305\212\311\241\63\277\160\300\341\217\35\56\203\353\202\332\201\114\203\154\104\227\114\206\257\14\204\230\134\122\47"
  "\365\33\313\122\206\244\51\240\215\325\112\110\244\360\373\370\266\330\215\227\131\162\355\254\227\206\376\171\302\361\56\331"
  "\111\332\340\25\113\227\34\336\51\45\317\374\34\45\217\26\344\314\260\327\221\115\306\160\16\121\374\134\206\242\230\120"
  "\244\100\261\260\302\100\261\342\371\73\115\124\63\121\241\123\207\117\207\61\252\360\352\256\56\277\230\175\257\221\351\14"
  "\77\313\310\220\117\71\332\334\27\302\101\20\351\53\2\102\132\31\246\376\303\357\107\266\267\175\252\41\37\316\222\16"
  "\304\73\371\41\47\372\261\74\330\205\104\330\375\60\46\64\46\64\46\164\276\206\11\373\255\73\203\317\5\11\352\203"
  "\326\375\161\66\257\311\377\242\261\316\373\203\32\65\324\374\242\374\167\202\30\142\226\33\54\203\4\0\1\70\124\134"
  "\377\273\315\143\63\217\315\74\32\270\140\17\205\323\367\333\103\367\170\5";

/* lib/stdio.wo (DEFLATEd, org. size 192049) */
static unsigned char file_l_40[13209] =
  "\354\227\327\222\363\46\24\307\257\323\336\201\321\350\142\113\212\333\370\163\172\357\345\46\75\361\214\7\13\360\236\54\2"
  "\5\320\266\247\17\6\331\310\110\356\116\367\14\263\13\77\161\376\247\350\170\4\27\271\44\45\247\50\325\206\200\174"
  "\351\171\204\56\40\57\244\62\50\311\224\111\120\242\213\64\101\27\63\56\247\230\243\324\262\67\54\101\27\171\151\20"
  "\364\173\227\227\227\55\66\71\315\245\172\264\146\176\342\315\252\171\47\130\154\365\142\35\104\273\357\261\206\211\26\270"
  "\320\67\322\114\12\105\357\200\336\167\255\100\201\315\315\104\121\201\163\152\205\130\51\62\224\266\157\176\243\266\25\135"
  "\24\130\341\334\71\72\156\256\250\56\171\153\111\250\122\102\46\213\377\41\111\267\176\303\375\155\111\64\63\217\5\265"
  "\126\240\11\314\300\54\163\162\374\215\212\156\17\41\26\323\5\316\150\54\346\351\276\142\106\162\171\117\125\44\126\321"
  "\135\305\264\121\40\146\276\145\262\233\40\346\371\33\236\36\371\6\202\23\73\341\124\304\116\74\75\104\214\52\45\33"
  "\61\127\374\260\22\144\305\143\254\127\341\123\266\244\66\204\303\324\347\140\44\11\36\35\177\303\323\255\302\154\70\330"
  "\54\314\171\103\331\343\375\342\337\346\246\134\343\247\74\241\43\246\50\215\235\314\131\135\150\235\155\216\71\227\131\154"
  "\355\351\336\157\14\33\372\0\46\26\363\164\127\61\226\11\303\255\226\54\152\77\6\7\337\230\243\272\314\276\222\14"
  "\4\231\50\312\13\154\156\42\351\372\243\103\134\224\2\154\266\326\207\242\70\264\254\247\157\314\131\54\172\260\203\173"
  "\5\206\306\36\34\74\231\13\256\51\275\215\135\70\330\52\73\34\154\157\331\330\105\306\245\156\144\341\340\336\321\202"
  "\306\306\74\306\132\236\356\55\126\12\16\342\26\233\130\156\301\117\126\143\225\23\120\115\77\25\336\135\72\174\262\335"
  "\31\351\15\20\140\0\163\170\242\44\234\203\320\205\375\373\152\46\205\66\313\43\16\301\6\57\214\46\40\247\350\202"
  "\76\370\30\347\253\344\22\335\141\205\254\320\114\274\75\100\311\270\323\331\163\164\143\162\342\321\373\223\175\15\143\137"
  "\347\161\36\347\161\36\347\161\36\177\371\110\232\37\155\242\7\335\24\371\217\272\377\116\167\121\362\371\67\237\174\376"
  "\315\347\337\375\274\326\244\327\60\371\346\375\157\56\256\56\333\15\12\171\337\355\324\116\2\243\326\223\0\353\274\333"
  "\244\351\173\115\366\163\314\336\33\217\130\143\337\150\64\316\372\61\375\155\314\106\253\154\144\307\340\325\367\127\30\351"
  "\214\273\275\154\225\215\6\143\362\154\374\372\263\25\112\307\31\261\343\375\326\42\15\233\165\25\130\254\53\351\260\131"
  "\122\46\340\225\265\333\373\155\333\257\327\156\37\264\155\17\273\227\67\55\220\356\22\303\212\71\106\53\7\105\124\41"
  "\56\63\314\121\312\212\72\235\132\172\213\322\136\352\227\212\262\127\343\243\241\177\342\254\137\325\324\314\25\26\110\26"
  "\50\355\246\365\15\63\277\141\233\132\70\220\16\7\235\200\60\41\141\301\315\244\14\53\372\373\123\25\261\232\0\363"
  "\21\327\22\350\267\106\21\264\44\46\110\62\246\251\171\273\73\134\34\154\343\130\272\375\156\100\130\220\370\171\247\36"
  "\116\65\147\33\262\67\245\22\176\116\235\230\237\154\15\326\73\353\367\342\272\264\277\205\251\12\357\300\252\107\213\136"
  "\332\110\42\216\256\321\111\376\46\206\71\217\33\7\216\357\34\330\332\72\360\317\350\35\70\121\363\260\365\332\226\360"
  "\345\217\327\325\334\163\242\144\261\163\333\300\376\175\3\373\64\116\173\237\304\267\315\325\116\201\156\352\72\45\200\136"
  "\132\157\35\177\147\365\31\304\112\244\345\265\1\153\64\161\250\140\164\347\17\26\273\166\121\167\270\266\140\206\122\227"
  "\115\134\367\300\376\150\357\113\264\333\306\231\164\137\245\317\264\357\332\47\71\4\167\146\36\306\107\226\304\104\167\24"
  "\111\107\224\223\364\74\375\245\105\147\352\7\76\214\76\225\101\310\264\232\263\374\177\0\213\130\12\265\243\252\340\342"
  "\107\42\350\341\342\252\364\354\217\362\357\356\264\77\256\361\153\71\176\147\261\301\33\47\230\242\200\113\352\201\113\172"
  "\43\270\310\56\375\214\113\200\144\271\41\205\300\140\267\26\152\166\136\324\224\331\51\151\264\307\365\213\277\122\334\52"
  "\257\35\203\334\376\343\267\127\347\241\335\154\317\267\75\226\257\347\241\277\370\262\172\104\152\137\20\354\207\355\163\147"
  "\223\335\276\335\56\276\72\175\355\152\150\123\132\225\216\314\355\310\335\216\312\355\50\335\216\302\355\250\103\71\202\34"
  "\61\174\41\347\304\305\154\42\35\273\65\225\351\127\260\354\155\267\206\337\212\156\6\62\234\54\225\252\37\26\106\40"
  "\122\313\232\354\361\5\363\370\54\262\274\200\351\172\224\126\116\105\24\27\371\265\110\162\143\13\366\302\156\346\36\261"
  "\217\212\14\256\231\157\304\134\22\271\362\211\260\305\372\261\163\171\233\201\317\66\6\224\267\246\362\213\42\1\243\245"
  "\350\144\60\200\61\271\152\204\334\63\102\243\32\41\175\20\165\243\270\254\232\246\145\235\127\151\226\46\0\212\201\223"
  "\201\336\42\172\21\16\224\65\105\221\351\6\302\315\226\225\111\352\272\314\65\343\310\66\31\356\2\302\226\175\123\324"
  "\343\12\60\122\205\111\233\326\32\273\266\247\62\306\151\47\166\273\231\36\71\40\302\344\331\5\15\4\121\261\101\144"
  "\116\23\305\10\2\245\341\314\255\103\107\200\11\222\330\152\127\126\24\171\76\140\271\50\76\14\301\152\231\113\226\301"
  "\47\63\145\126\347\1\23\131\133\254\145\363\75\162\242\211\142\365\12\66\373\205\216\177\365\42\70\345\322\27\64\317"
  "\26\55\163\313\364\353\364\42\204\210\336\64\41\272\260\317\66\133\161\155\26\46\306\221\163\17\63\321\217\13\224\103"
  "\366\123\135\343\242\250\311\40\345\65\203\230\224\214\122\4\3\100\6\41\166\211\152\310\62\174\110\16\34\146\203\325"
  "\1\134\107\234\24\370\53\275\252\231\326\242\375\301\37\201\236\330\252\101\376\302\242\273\365\166\275\74\201\241\114\200"
  "\115\371\45\314\4\73\217\11\160\61\103\374\200\114\354\375\262\245\135\340\224\145\376\171\375\353\324\17\364\330\377\355"
  "\21\126\237\12\53\266\143\100\164\343\225\271\360\143\276\241\120\333\13\206\125\231\132\304\240\167\314\171\237\61\177\321"
  "\220\7\303\135\126\60\174\251\60\231\4\26\342\152\270\142\13\53\147\17\53\257\107\142\245\330\202\370\206\303\275\12"
  "\334\147\300\375\16\265\333\321\270\35\46\201\36\163\73\127\4\372\4\200\245\23\273\377\366\252\215\211\246\332\170\57"
  "\47\342\313\314\354\135\64\252\162\14\215\252\10\327\250\106\122\51\67\271\132\375\211\257\244\335\263\63\250\272\312\31"
  "\124\5\71\203\310\10\51\167\6\221\21\162\346\14\32\135\33\336\324\332\33\11\243\271\220\40\56\251\310\252\30\334"
  "\26\12\204\303\0\331\170\0\331\150\1\231\352\1\371\61\135\151\323\167\235\231\204\371\316\360\147\350\74\223\43\112"
  "\320\363\25\212\162\306\214\21\302\361\11\115\105\264\122\365\110\211\356\267\140\257\230\302\376\20\213\107\224\367\241\15"
  "\312\73\277\46\354\377\42\335\26\350\20\157\372\337\372\27\253\17\213\221\261\270\212\32\61\364\102\73\164\316\375\1"
  "\0\16\270\5\43\266\154\273\175\356\276\41\74\370\41\340\106\122\101\135\24\32\142\63\353\75\247\171\356\336\217\133"
  "\131\202\12\134\222\75\152\16\77\227\175\361\373\133\74\37\375\34\314\152\2\165\137\341\275\31\25\103\323\242\14\103"
  "\376\34\121\306\312\43\123\234\32\154\25\203\115\206\124\333\317\355\146\273\365\263\10\306\10\173\150\77\75\267\310\366"
  "\204\245\5\206\66\20\47\104\171\265\23\42\32\373\31\301\32\65\17\1\110\36\140\233\247\74\246\50\230\45\324\272"
  "\125\13\271\206\203\325\143\143\45\231\245\300\133\51\226\174\155\121\330\343\230\76\204\220\305\231\4\61\214\237\144\124"
  "\337\120\361\240\166\113\20\226\274\77\336\24\3\313\211\22\166\365\36\204\255\227\133\65\367\60\350\267\101\26\312\145"
  "\336\343\213\270\354\245\236\250\377\277\173\364\6\200\304\161\213\105\260\130\55\245\153\2\242\123\72\232\167\17\23\44"
  "\107\252\317\14\220\345\210\72\63\252\65\204\72\3\102\45\174\226\264\326\353\46\267\14\270\66\1\54\30\54\31\260"
  "\132\307\74\260\322\17\311\71\315\302\13\254\2\201\205\341\344\234\22\67\227\15\361\211\344\140\214\51\272\322\150\242"
  "\53\276\207\205\306\31\363\5\244\143\50\325\335\363\323\45\107\115\57\107\375\243\243\231\240\367\350\4\372\61\144\205"
  "\350\31\352\373\107\14\126\323\137\24\342\361\12\274\143\336\114\242\364\376\272\26\37\130\70\315\161\323\1\346\30\307"
  "\2\61\112\115\331\276\121\170\166\177\1\62\220\20\37\131\47\112\127\275\175\132\352\355\40\36\261\207\174\74\232\35"
  "\147\112\255\301\44\175\12\74\120\31\242\23\120\13\243\337\356\176\112\141\175\152\43\222\210\144\274\110\345\241\26\234"
  "\106\130\272\331\142\45\326\341\271\351\232\206\275\231\141\107\166\165\275\15\144\367\54\367\317\273\223\76\361\354\164\264"
  "\255\266\335\166\335\236\234\256\361\354\113\200\173\277\61\220\305\375\232\340\167\57\373\215\232\123\65\300\57\132\226\23"
  "\314\104\166\370\375\171\13\160\71\37\15\130\26\140\53\300\202\2\322\356\25\322\167\313\244\257\150\56\266\143\4\151"
  "\306\157\6\160\50\332\60\102\330\242\124\365\253\55\344\373\325\346\307\343\63\176\200\347\36\254\202\30\275\12\302\355"
  "\357\110\132\234\130\135\74\162\120\273\51\232\255\34\101\257\102\123\122\117\56\110\302\127\42\354\316\153\247\267\374\153"
  "\231\5\227\14\274\225\154\243\226\155\340\362\344\76\260\7\357\341\357\253\46\332\301\11\0\337\17\247\31\140\153\322"
  "\107\16\212\54\65\246\175\245\130\230\216\64\365\350\266\6\164\3\236\313\230\235\114\304\127\217\22\16\75\66\21\352"
  "\244\14\165\67\361\62\232\153\114\117\213\156\75\226\176\244\165\13\0\107\31\226\23\114\321\303\60\344\154\106\221\147"
  "\144\342\320\200\364\160\167\156\224\230\41\342\321\42\47\241\117\223\102\152\103\147\211\4\14\301\366\140\224\160\177\227"
  "\27\115\167\350\316\32\350\62\112\110\216\130\252\172\371\21\327\371\240\272\267\120\162\271\323\362\333\177\50\170\34\141"
  "\141\23\270\313\43\76\14\44\140\14\153\106\227\125\170\24\143\302\157\356\124\307\34\206\304\343\304\343\232\212\0\123"
  "\215\343\37\364\302\266\16\361\343\23\37\105\64\251\45\310\27\314\76\231\234\215\177\157\103\220\53\200\105\76\236\157"
  "\16\355\0\215\241\13\43\64\226\152\377\332\323\301\341\244\355\201\152\207\46\106\270\106\355\217\337\360\161\364\327\147"
  "\122\120\325\74\74\220\37\43\207\222\3\224\357\72\370\216\317\176\6\34\241\161\24\171\74\226\16\31\262\314\200\173"
  "\306\265\43\367\346\376\271\61\313\136\326\351\345\53\365\222\363\312\0\276\107\110\136\356\345\236\321\21\371\101\175\143"
  "\172\67\22\347\237\156\54\151\215\167\170\176\367\27\271\371\203\245\373\251\0\227\23\116\5\234\262\226\214\232\321\114"
  "\47\116\142\251\52\21\272\370\360\300\176\102\226\127\122\345\3\67\117\364\350\23\271\130\215\76\22\2\205\7\267\13"
  "\345\207\2\307\350\370\34\77\74\71\276\141\31\16\12\164\317\215\254\274\212\240\40\76\77\5\373\40\127\24\372\0"
  "\204\370\241\60\145\140\50\214\302\332\120\11\336\112\51\170\25\361\20\121\5\157\175\221\163\16\131\334\165\17\66\275"
  "\54\256\271\54\306\233\147\276\12\206\335\142\272\10\161\12\236\53\44\160\0\365\146\50\350\302\5\261\77\114\342\114"
  "\313\142\244\15\355\51\7\112\54\345\33\375\325\300\175\105\112\350\347\342\26\336\124\242\61\364\176\15\275\355\45\206"
  "\124\264\374\150\142\61\7\235\243\100\13\163\310\140\167\170\250\1\32\46\122\11\254\7\325\114\220\35\73\266\237\335"
  "\172\162\71\24\250\51\206\337\35\341\117\270\2\202\302\200\305\50\354\256\216\277\41\231\35\243\105\111\111\63\267\306"
  "\52\342\371\102\201\3\10\206\14\222\5\265\113\304\330\360\110\55\321\242\357\74\122\13\352\32\141\41\33\51\322\363"
  "\236\61\74\162\372\204\311\221\60\35\205\207\137\372\342\6\252\334\101\340\17\105\5\44\126\371\210\353\154\261\63\326"
  "\246\236\15\104\122\322\303\224\273\201\345\131\117\204\163\245\210\53\53\334\45\41\343\160\45\225\273\143\302\243\360\242"
  "\5\135\265\207\347\123\47\46\334\271\351\132\160\235\332\64\333\255\366\273\265\322\22\73\70\77\330\235\256\265\324\150"
  "\271\332\141\75\321\356\252\270\165\23\201\212\71\127\232\265\255\367\324\266\360\127\341\52\16\327\71\272\253\155\4\140"
  "\255\313\303\337\0\305\67\276\357\163\200\36\70\60\242\104\2\1\123\172\342\346\332\364\365\45\256\346\305\326\227\204"
  "\127\362\101\71\203\125\242\117\200\6\205\54\162\2\304\55\377\100\360\252\365\206\116\173\44\235\106\147\360\252\173\235"
  "\207\123\364\377\265\135\357\140\262\136\132\217\341\27\202\115\312\300\350\23\302\33\44\374\52\74\136\323\326\217\374\352"
  "\21\252\103\270\263\220\207\4\345\20\104\147\213\200\256\240\267\217\267\334\45\7\364\120\57\102\100\75\264\5\330\372"
  "\120\331\61\242\6\170\14\251\106\40\351\157\362\325\17\266\211\162\66\112\370\175\316\1\140\235\105\44\230\207\277\336"
  "\21\136\355\55\366\72\302\102\132\45\211\340\231\56\65\334\100\26\331\222\172\225\327\24\44\16\260\41\130\131\324\233"
  "\117\316\176\372\111\227\302\175\316\115\65\363\341\126\152\270\342\32\77\314\301\114\42\314\341\143\247\251\152\202\0\323"
  "\242\270\310\362\24\131\333\374\75\135\21\311\266\327\147\31\36\131\215\270\237\315\304\60\23\103\214\150\311\115\26\30"
  "\340\23\20\315\303\345\110\147\313\21\264\30\130\100\14\43\66\164\227\166\11\270\133\243\125\240\104\343\6\215\317\4"
  "\75\220\350\361\367\166\52\75\103\362\211\350\45\342\350\344\343\143\6\46\250\270\167\131\254\243\113\260\53\364\145\54"
  "\321\7\347\162\33\367\137\156\3\211\130\115\5\133\222\362\14\63\205\224\112\223\75\220\54\372\116\157\262\1\250\47"
  "\342\167\357\124\156\365\160\177\243\276\126\243\364\115\331\63\317\53\205\304\367\314\23\254\0\131\205\336\144\102\270\73"
  "\56\151\65\76\145\104\46\376\16\261\122\15\354\22\256\220\75\357\154\313\176\150\337\324\332\341\105\327\226\21\334\271"
  "\21\312\136\107\210\136\216\355\20\333\362\30\326\251\247\231\47\212\264\373\60\257\237\202\43\306\317\334\214\137\126\74"
  "\376\335\154\220\132\311\47\214\156\157\307\257\116\34\46\22\257\271\127\172\171\334\121\330\377\271\211\216\135\233\375\17"
  "\363\72\175\307\315\327\315\216\212\205\45\106\65\115\377\45\210\11\4\307\143\151\223\370\325\165\257\13\32\30\16\36"
  "\234\266\140\305\114\367\12\52\362\75\240\142\131\43\24\154\104\327\335\360\11\206\115\362\223\104\51\252\211\151\172\163"
  "\304\203\372\321\132\163\351\321\332\341\207\77\217\213\303\143\77\216\45\141\110\134\251\234\34\207\51\212\16\64\21\344"
  "\253\361\364\320\264\16\275\201\136\162\217\137\30\131\21\34\21\105\64\352\124\321\362\334\0\227\276\52\167\62\231\310"
  "\270\103\200\262\21\325\237\161\100\110\207\107\206\104\246\102\305\311\106\170\371\156\372\117\327\205\227\307\327\113\26\344"
  "\340\27\344\13\112\106\255\224\101\131\102\253\70\0\356\52\214\276\150\105\1\133\305\313\302\112\137\177\345\335\361\374"
  "\156\104\6\353\213\354\301\155\302\371\314\35\322\53\312\11\135\220\326\201\373\55\25\41\130\247\365\166\53\226\372\271"
  "\251\17\301\262\215\367\327\276\305\352\377\75\167\322\247\217\23\345\22\30\215\301\150\241\241\321\35\275\170\353\33\0"
  "\43\257\245\60\34\110\74\346\316\367\2\213\211\20\272\372\117\362\11\104\160\116\107\166\316\23\344\224\205\5\370\235"
  "\247\352\41\20\26\44\222\107\361\214\33\361\24\10\14\101\50\123\167\1\14\315\5\307\161\375\163\263\133\211\344\30"
  "\332\42\72\142\75\153\37\244\66\306\307\211\344\2\116\310\142\242\110\250\111\77\260\71\201\152\364\237\362\6\230\347"
  "\264\155\202\10\341\21\0\23\65\110\24\61\231\207\275\23\225\331\167\260\153\40\311\121\347\332\146\231\63\155\23\370"
  "\314\75\253\237\263\372\71\253\237\263\372\71\253\237\104\331\44\323\110\157\200\12\112\106\46\212\51\177\263\115\24\26"
  "\352\323\361\362\323\372\55\145\133\72\127\242\165\52\211\246\210\140\360\11\270\203\64\47\36\325\40\7\46\247\340\223"
  "\27\365\105\34\220\221\346\370\210\10\22\144\16\130\220\250\157\274\76\106\346\65\5\276\56\74\355\371\111\32\376\60"
  "\203\71\14\200\10\241\17\37\34\300\366\167\17\41\3\342\77\233\170\20\201\54\165\16\53\210\351\75\232\303\12\270"
  "\253\161\16\24\230\3\5\346\100\201\111\120\140\374\253\177\246\5\150\3\2\372\345\376\260\236\1\174\355\140\126\56"
  "\74\71\361\175\277\132\143\316\75\261\204\107\172\10\260\104\103\127\157\327\302\72\22\350\61\320\203\253\317\240\47\207"
  "\236\2\172\176\357\140\122\226\137\256\40\371\24\164\266\200\267\214\303\74\264\105\71\246\121\55\142\147\273\171\372\322"
  "\36\327\153\115\312\123\36\162\3\47\22\4\106\11\276\140\10\160\227\120\16\74\235\53\300\54\55\124\262\15\113\275"
  "\111\263\260\233\371\305\32\276\351\3\343\356\3\307\164\74\350\222\135\234\303\0\271\156\0\134\101\251\33\101\266\144"
  "\327\317\225\241\47\160\241\231\7\75\367\241\247\327\162\24\172\55\306\240\327\142\34\172\205\104\363\14\213\42\207\36"
  "\165\300\113\150\210\254\232\343\206\207\167\336\136\43\47\341\117\157\102\22\77\74\144\245\227\324\174\141\165\204\312\204"
  "\100\20\314\2\201\255\206\233\371\151\64\43\102\363\362\112\255\367\357\10\360\365\107\207\70\20\172\106\203\46\253\130"
  "\216\214\3\332\317\367\276\261\137\52\17\127\26\300\171\125\370\15\370\50\352\224\211\202\132\14\334\361\75\143\6\45"
  "\267\164\322\303\1\15\233\363\141\40\212\160\134\316\36\102\254\221\170\354\310\4\270\103\103\120\2\235\14\50\322\105"
  "\125\40\141\15\362\135\130\341\212\342\126\256\141\230\271\34\247\12\64\367\232\270\116\23\215\317\304\273\25\256\60\340"
  "\112\311\127\134\161\1\375\110\230\276\355\40\22\127\53\207\316\343\246\173\334\57\117\217\253\76\201\346\344\324\0\372"
  "\306\53\271\277\374\312\363\70\54\100\243\326\125\1\125\14\135\24\157\313\51\346\140\131\255\227\37\30\54\125\54\260"
  "\174\133\377\372\47\202\105\137\320\111\267\262\246\212\266\151\223\244\372\135\337\170\373\145\21\155\373\125\22\270\173\102"
  "\23\335\351\270\333\256\167\303\300\102\20\175\267\343\165\137\374\272\312\315\336\15\235\264\142\354\351\210\346\54\53\370"
  "\352\126\11\356\274\265\216\70\340\137\367\342\355\327\27\255\225\257\350\301\350\352\321\362\7\144\160\215\341\357\307\300"
  "\74\160\160\0\16\202\142\213\323\176\3\330\305\357\153\340\266\346\332\7\342\66\354\35\15\330\21\67\113\10\336\201"
  "\66\203\62\130\137\1\164\203\254\20\237\125\304\235\114\247\352\56\267\60\24\40\106\71\253\250\216\263\321\277\161\1"
  "\147\301\325\213\335\143\257\277\266\353\243\215\354\347\67\167\54\136\272\131\375\2\346\332\363\140\273\257\237\226\353\44"
  "\347\261\240\157\30\356\252\367\161\207\345\171\107\5\200\222\343\21\370\220\253\150\16\306\166\263\135\107\6\242\60\232"
  "\326\175\254\122\237\353\203\240\24\2\234\37\103\230\137\6\211\363\62\10\74\244\211\344\246\167\374\367\377\201\132\12"
  "\66\102\43\257\371\233\15\122\356\327\133\274\134\137\73\362\174\216\75\167\71\71\234\245\73\261\27\34\66\103\206\204"
  "\260\215\370\257\60\364\253\272\360\100\31\175\67\114\232\145\350\335\62\367\346\177\112\201\23\235\365\332\323\342\151\273"
  "\176\31\345\145\340\227\271\136\246\177\131\21\321\73\145\373\234\153\142\230\254\327\246\42\45\122\311\5\45\133\130\372"
  "\246\205\231\22\126\146\112\167\151\251\167\151\162\160\154\151\231\142\151\352\13\334\342\312\105\344\61\27\121\222\105\134"
  "\12\326\145\311\134\304\152\342\334\246\355\205\203\236\337\264\177\264\32\176\23\217\171\150\370\1\22\374\130\104\376\272"
  "\356\266\337\314\152\375\175\177\352\301\132\346\322\307\261\45\210\244\141\31\343\142\213\330\310\73\33\117\66\242\312\212"
  "\6\114\25\336\247\105\347\40\323\317\315\352\364\315\356\72\364\272\263\243\26\43\276\311\320\257\35\253\345\105\75\371"
  "\353\376\324\365\140\260\73\373\341\355\354\302\176\315\62\371\153\337\323\301\301\354\366\100\105\253\211\221\223\130\323\140"
  "\116\223\360\150\116\134\253\201\236\314\42\335\257\333\375\323\177\51\62\307\323\227\356\360\0\26\154\17\244\13\77\146"
  "\246\261\174\327\301\167\174\366\363\211\160\277\113\217\61\374\107\253\45\377\315\53\52\1\5\236\121\71\374\371\202\117"
  "\27\374\275\147\332\40\206\3\131\227\130\17\276\27\152\6\2\260\330\152\317\115\16\143\254\103\306\26\53\355\331\362"
  "\62\342\114\150\265\6\115\357\103\1\24\273\73\321\364\173\175\174\265\71\256\227\247\337\314\251\377\120\376\337\265\334"
  "\271\311\223\27\243\276\146\201\343\147\70\76\267\163\330\2\375\326\25\47\12\41\115\351\23\112\304\76\205\323\130\276"
  "\340\246\272\2\233\76\250\147\222\243\271\31\33\315\271\275\202\166\360\40\377\25\65\321\203\51\176\146\70\65\201\256"
  "\222\54\364\311\242\44\206\375\336\310\2\223\360\120\253\140\274\115\76\10\305\300\231\0\114\232\304\25\271\165\255\20"
  "\271\72\122\323\276\372\106\222\11\356\225\324\40\364\123\150\211\310\34\244\70\257\303\270\366\217\311\213\46\231\104\361"
  "\45\227\224\246\234\105\345\107\22\225\205\236\176\365\71\373\263\250\234\11\140\26\225\245\222\324\344\23\350\273\153\122"
  "\343\62\120\316\4\376\301\75\362\265\335\54\130\62\150\115\344\36\315\345\64\211\156\204\2\107\50\65\43\310\26\301"
  "\221\216\61\162\22\330\345\273\20\363\34\174\146\40\52\146\26\361\12\124\217\21\161\260\224\345\370\343\372\365\161\6"
  "\323\360\216\325\101\336\61\175\332\252\76\373\365\76\275\143\343\251\74\202\164\117\107\53\50\127\72\163\117\70\241\325"
  "\53\337\341\375\71\206\152\10\103\253\356\204\241\315\14\115\242\201\77\70\103\63\111\20\107\153\224\34\115\76\201\276"
  "\121\70\332\314\321\112\37\107\253\274\34\255\202\230\33\344\150\240\236\66\26\107\63\311\235\260\264\231\245\111\66\335"
  "\107\147\151\201\127\230\332\73\114\371\6\72\107\342\152\63\127\153\174\134\315\44\36\266\146\167\17\137\362\74\51\77"
  "\213\130\372\103\356\40\10\34\243\161\326\307\343\156\377\345\374\237\232\312\316\62\73\104\7\101\224\17\104\375\51\210"
  "\220\25\111\300\45\223\163\307\57\70\110\144\125\22\10\301\243\324\151\266\25\0\0\372\100\72\245\0\22\164\155\140"
  "\101\223\356\164\74\355\267\133\300\340\343\32\62\72\157\273\246\147\377\242\50\242\40\247\312\122\165\62\27\71\42\75"
  "\355\340\222\145\77\152\212\41\361\232\335\30\361\232\35\215\314\304\204\45\47\306\262\355\217\360\215\31\114\334\143\211"
  "\22\2\176\362\62\277\42\343\122\227\310\373\17\121\305\320\13\152\102\300\46\127\156\51\116\300\137\61\222\63\45\261"
  "\154\306\73\172\170\262\262\2\347\204\157\316\101\150\321\356\65\370\155\172\247\313\221\136\236\376\76\254\277\364\22\161"
  "\377\163\175\104\104\43\77\215\203\157\172\374\211\237\255\170\107\370\43\150\43\332\65\171\204\100\153\356\355\102\263\274"
  "\167\310\240\341\67\302\172\324\272\47\55\230\15\72\306\52\100\307\320\145\200\264\341\31\40\275\67\0\263\77\372\161"
  "\245\143\267\376\152\375\302\233\16\262\163\232\177\232\204\347\213\160\215\207\245\207\144\274\326\367\173\346\213\360\132\337"
  "\345\25\325\277\253\71\357\344\302\157\172\4\346\77\352\161\170\116\114\371\140\136\327\71\61\45\252\212\74\253\310\61"
  "\362\176\204\337\114\75\347\147\251\114\130\11\247\347\71\224\60\211\314\117\52\306\117\140\263\24\340\0\274\343\272\375"
  "\374\362\243\337\32\357\252\313\315\203\367\210\4\220\340\201\263\340\204\227\211\243\102\11\206\347\361\226\103\302\370\253"
  "\174\337\354\240\357\257\241\117\346\21\232\17\67\61\302\157\11\202\157\57\144\263\77\326\307\123\350\365\205\2\201\215"
  "\111\42\123\110\375\56\24\222\6\122\110\157\116\371\210\4\13\73\41\372\356\26\273\173\305\51\364\103\160\147\366\34"
  "\301\60\11\267\271\100\27\234\113\110\307\223\214\312\347\30\225\362\25\372\165\104\355\175\101\270\274\14\137\103\341\5"
  "\227\42\304\44\0\62\106\126\305\223\227\42\126\136\346\332\173\134\74\320\357\226\327\270\65\17\223\62\144\362\207\71"
  "\113\373\235\302\176\160\162\356\207\43\261\77\262\174\30\201\52\320\61\343\341\324\321\301\323\216\227\53\37\356\47\137"
  "\163\216\225\303\173\70\370\207\342\72\155\126\215\47\340\146\366\147\260\317\151\121\163\132\324\335\45\117\316\201\371\134"
  "\321\11\260\122\146\175\147\116\171\222\101\347\44\116\374\311\314\64\143\63\115\70\324\145\174\123\217\137\54\244\221\375"
  "\205\11\167\200\261\65\46\46\356\32\313\346\322\32\11\63\237\223\275\146\176\76\363\312\71\6\111\103\173\132\177\253"
  "\174\3\235\367\33\206\64\55\156\233\7\105\12\101\235\43\61\127\313\331\134\235\113\143\114\114\265\50\303\124\13\255"
  "\147\134\276\201\316\177\240\152\201\146\136\341\63\363\112\217\15\147\365\342\167\370\217\370\71\340\64\342\145\152\111\340"
  "\125\364\44\360\121\63\243\61\11\172\345\113\201\366\216\77\147\76\377\350\166\335\162\261\153\345\115\372\337\135\362\54"
  "\175\120\272\122\273\77\176\137\234\354\276\37\13\232\25\55\131\116\257\135\362\262\222\364\74\55\72\247\347\362\343\217"
  "\47\147\100\273\271\132\167\316\17\66\147\372\164\163\234\116\313\157\316\270\277\354\127\161\376\266\323\244\66\143\344\73"
  "\105\170\37\247\242\11\120\15\117\200\62\6\172\322\67\345\73\25\127\144\100\125\320\123\103\17\254\71\115\240\7\341"
  "\233\102\117\334\67\175\322\52\175\207\344\252\1\307\371\257\6\34\277\206\43\247\145\302\304\11\256\212\152\45\344\225"
  "\262\201\245\300\252\17\344\301\361\3\177\344\123\373\242\67\37\236\344\114\157\272\356\260\130\212\244\160\137\163\224\15"
  "\25\232\31\215\142\106\30\124\247\114\37\274\132\232\255\203\111\43\147\65\22\161\156\56\137\211\252\316\344\175\132\336"
  "\271\145\206\7\317\155\62\16\330\60\266\60\47\332\163\367\117\234\22\177\170\367\245\143\142\240\224\127\34\75\271\33"
  "\122\305\324\160\5\334\121\31\276\111\231\142\164\346\51\325\300\205\33\66\367\317\15\147\156\70\207\37\207\160\303\332"
  "\307\15\33\17\67\264\172\345\73\31\355\203\223\213\134\250\223\67\325\103\24\342\230\164\60\277\340\244\247\3\176\203"
  "\367\211\237\237\143\356\341\307\370\303\361\222\122\145\33\70\313\370\31\200\134\365\341\110\41\310\312\251\47\270\176\33"
  "\132\355\341\272\206\37\344\77\26\236\56\30\277\274\74\201\174\101\211\27\366\5\171\232\206\73\63\244\36\12\226\244"
  "\16\66\313\345\376\312\237\374\344\74\344\142\370\123\53\130\300\345\373\363\66\200\47\313\51\260\242\75\0\63\21\205"
  "\262\360\120\314\302\272\274\320\342\147\52\57\320\13\144\63\307\363\122\73\355\312\151\227\116\273\160\332\271\323\166\307"
  "\117\235\266\161\332\211\335\116\33\247\135\73\355\312\151\227\116\273\160\332\271\323\316\234\166\352\264\215\323\166\326\147"
  "\32\247\135\73\355\312\151\227\116\273\160\332\271\37\141\2\121\210\243\75\174\263\301\307\211\312\342\122\172\21\276\217"
  "\124\343\10\215\156\4\264\322\252\104\65\102\226\340\10\106\67\2\322\125\135\353\106\100\356\325\50\327\120\340\10\225"
  "\16\222\71\216\320\350\326\200\43\230\104\167\30\246\361\14\141\164\373\50\74\103\244\272\41\112\317\20\231\156\210\312"
  "\63\104\256\203\205\17\234\112\372\112\74\103\224\272\125\370\140\121\353\206\360\234\210\121\22\151\345\31\102\211\27\306"
  "\63\104\252\133\105\351\31\242\320\15\221\171\206\120\236\110\355\31\102\111\353\51\16\221\46\272\41\62\317\20\72\160"
  "\212\344\73\212\132\361\252\252\344\1\212\44\222\334\350\156\103\260\325\122\237\42\5\336\55\60\356\360\3\174\151\41"
  "\263\0\123\214\4\230\372\66\200\321\302\345\255\140\51\37\24\63\373\6\250\244\345\47\61\335\160\226\273\53\115\254"
  "\226\261\132\251\325\312\254\126\156\265\12\253\125\132\255\312\152\325\126\313\132\113\226\130\55\143\265\122\253\225\131\255"
  "\334\152\25\126\253\264\132\225\316\242\361\131\111\7\77\140\205\115\40\51\5\271\52\144\32\45\141\301\312\117\266\261"
  "\342\50\377\171\346\264\123\247\155\234\166\102\25\103\231\323\303\142\163\303\224\72\362\175\102\104\67\35\40\25\36\57"
  "\273\357\41\56\143\323\312\257\166\340\261\324\323\142\24\52\243\101\52\232\221\125\311\31\15\177\102\173\34\335\77\66"
  "\371\353\335\324\370\6\315\63\106\362\236\175\237\275\227\362\344\165\237\171\160\311\216\127\310\213\371\376\352\376\357\257"
  "\346\347\244\103\356\257\362\334\233\276\352\213\243\261\273\345\123\144\41\322\372\320\64\364\141\156\61\320\171\235\73\56"
  "\264\332\70\355\304\156\127\215\323\256\235\166\345\264\113\247\135\70\155\67\312\252\164\332\205\323\166\177\237\71\355\324"
  "\151\33\247\355\354\47\157\234\166\355\264\53\342\270\43\102\275\52\230\333\216\174\137\61\247\35\371\276\301\357\65\132"
  "\121\315\34\166\344\373\322\4\152\145\15\161\326\261\375\347\201\132\135\115\34\165\24\0\71\161\323\321\35\224\304\111"
  "\107\7\360\155\41\323\340\100\102\274\153\24\6\45\161\111\321\1\62\342\220\242\3\44\304\35\105\361\240\242\236\44"
  "\262\202\202\370\221\350\0\226\171\120\132\2\266\262\132\265\325\362\110\330\23\222\304\250\351\271\47\17\254\161\2\226"
  "\120\207\322\21\162\123\136\17\340\373\372\373\376\370\367\347\166\263\335\372\107\302\122\116\372\264\41\365\56\65\233\63"
  "\114\153\255\303\127\354\237\330\220\211\233\130\23\247\274\376\115\244\231\63\66\263\211\65\163\316\146\116\43\315\234\145"
  "\221\236\123\305\2\41\267\17\125\151\124\236\353\20\227\255\230\110\334\243\62\44\144\61\243\27\277\120\344\110\312\67"
  "\243\161\106\223\207\143\340\333\342\215\212\167\72\104\102\71\171\311\50\307\43\330\311\61\211\43\127\221\134\34\10\337"
  "\46\233\46\174\33\312\23\313\110\360\45\345\367\13\143\347\64\45\17\43\37\203\270\324\40\150\153\230\335\132\115\32"
  "\70\75\146\205\217\172\53\20\215\5\200\26\252\117\301\120\370\35\203\47\43\347\255\130\212\175\374\221\43\231\4\23"
  "\360\3\325\235\316\322\242\231\354\141\344\320\156\271\325\360\123\115\356\120\115\361\160\225\136\206\253\104\70\360\52\4"
  "\361\70\224\177\47\113\354\121\42\300\122\170\265\330\226\105\46\366\43\264\162\151\51\40\253\73\155\25\354\353\33\300"
  "\76\215\106\174\0\373\104\340\13\55\43\255\53\224\111\357\325\34\0\260\243\367\134\310\213\112\133\60\126\354\156\122"
  "\310\64\300\247\260\141\223\10\147\20\370\334\377\5\135\204\107\123\10\113\111\213\102\272\26\210\212\204\174\165\372\212"
  "\234\246\360\370\22\27\331\41\324\261\107\111\237\33\306\171\232\267\24\31\213\231\246\234\250\157\66\345\33\354\274\327"
  "\273\115\341\265\276\134\144\273\173\300\66\41\30\213\15\326\66\33\154\156\310\6\147\66\70\263\301\372\41\234\305\305"
  "\344\106\372\222\265\362\15\166\336\77\67\362\346\2\113\367\320\252\245\205\353\11\347\64\341\227\104\32\166\305\155\142"
  "\0\310\224\43\52\70\354\110\370\230\102\33\347\352\17\241\316\64\330\335\57\377\300\13\311\62\261\132\306\152\245\126"
  "\53\263\132\271\325\52\254\126\111\123\362\206\52\153\166\274\207\33\217\222\71\355\324\151\33\247\235\330\355\262\161\332"
  "\165\140\174\104\23\30\137\120\245\201\167\353\225\11\275\133\257\3\257\306\253\54\360\152\134\16\351\50\47\376\212\65"
  "\265\325\152\244\45\237\101\336\60\240\224\177\164\57\354\152\335\0\251\264\374\200\110\64\343\111\113\257\326\241\152\20"
  "\362\210\60\164\313\252\375\302\100\257\157\340\233\303\73\330\306\57\200\47\24\251\203\162\226\41\260\103\101\14\222\206"
  "\111\257\270\202\310\30\375\203\256\376\112\174\101\176\155\30\35\45\24\370\216\255\323\374\171\134\34\36\67\145\316\264"
  "\73\131\131\100\250\372\257\13\21\347\124\26\126\271\325\52\254\226\375\113\233\65\324\126\313\56\12\223\130\55\63\21"
  "\242\217\115\337\150\301\374\375\117\242\357\0\44\26\120\41\22\267\375\276\51\32\113\113\303\27\50\100\24\346\252\174"
  "\204\126\122\66\156\141\10\331\267\323\110\37\142\34\55\226\72\156\235\72\307\255\267\310\161\173\340\345\213\27\207\7"
  "\126\277\130\112\34\277\266\217\153\371\231\275\253\176\64\157\51\225\141\77\355\147\253\134\363\146\367\330\156\266\76\367"
  "\12\232\151\254\156\252\124\172\1\54\226\272\320\236\122\332\254\200\54\326\223\221\357\310\31\71\107\4\47\64\335\23"
  "\221\277\276\360\162\371\353\376\351\76\16\306\75\31\357\321\74\75\267\123\247\236\176\211\355\372\10\303\364\335\276\76"
  "\33\376\307\315\356\353\227\376\277\104\52\177\244\43\204\102\357\103\7\77\304\345\376\171\167\272\307\163\225\276\141\217"
  "\236\243\352\117\173\327\37\367\143\367\1\17\374\207\53\361\176\160\221\247\257\330\77\5\31\305\101\341\102\2\1\241"
  "\337\370\204\104\1\337\77\0\200\320\176\0\110\46\315\161\71\244\166\0\252\140\76\311\301\167\7\214\215\74\64\262"
  "\177\76\375\353\136\4\152\354\121\221\376\205\45\357\53\43\2\67\174\70\3\126\337\17\102\357\244\360\125\61\264\320"
  "\161\124\22\320\160\361\326\163\110\300\257\37\73\345\65\1\207\364\356\171\273\35\37\316\154\132\141\177\121\216\267\75"
  "\260\167\122\262\377\356\341\24\172\236\235\110\50\176\132\152\363\134\6\247\350\261\11\271\232\305\40\74\374\72\174\302"
  "\304\172\50\257\343\376\116\277\364\257\357\352\211\0\54\346\35\116\363\130\337\233\207\71\240\137\213\255\2\316\11\371"
  "\174\273\175\356\276\365\334\133\330\220\334\266\53\71\323\161\375\303\346\20\175\147\20\217\370\157\345\61\374\102\336\224"
  "\22\1\275\135\174\355\250\331\322\235\26\307\323\143\277\12\207\273\60\141\43\254\105\206\200\137\237\227\0\276\133\214"
  "\203\241\101\10\174\124\103\106\175\113\24\175\112\136\151\24\67\70\74\316\243\215\301\172\126\75\317\203\117\307\51\104"
  "\72\364\51\34\237\134\335\220\356\63\372\277\41\344\345\377\200\243\66\102\244\246\120\70\167\342\112\103\367\206\221\150"
  "\140\124\237\225\337\163\371\47\374\30\137\74\344\217\372\335\35\246\50\336\147\12\340\117\55\171\333\211\1\20\331\44"
  "\236\51\343\15\355\375\23\276\346\331\37\151\170\67\307\105\366\356\264\137\74\212\111\370\36\142\173\267\376\272\70\155"
  "\176\254\235\357\26\235\323\163\350\341\27\46\360\247\373\2\343\150\132\203\237\224\106\326\51\302\363\2\345\324\161\27"
  "\322\300\31\302\336\241\342\233\115\175\273\325\155\133\123\164\220\227\361\107\221\153\247\330\162\365\254\357\33\150\7\371"
  "\251\37\200\354\241\366\24\7\342\260\321\253\0\361\37\1\307\274\365\232\32\155\302\236\123\57\173\116\55\25\12\264"
  "\126\71\71\21\305\52\2\124\35\33\242\241\376\350\146\74\111\303\360\44\367\340\211\325\213\317\326\160\234\50\275\110"
  "\101\301\41\300\246\63\44\151\316\244\215\4\360\160\4\341\114\51\74\11\205\243\275\314\22\312\324\103\354\24\334\40"
  "\106\175\342\201\343\26\342\257\134\110\4\303\330\365\313\16\320\102\262\64\314\265\61\163\61\157\274\270\242\236\312\4"
  "\316\173\76\142\372\272\36\244\125\214\175\302\126\44\364\14\162\317\263\70\265\352\262\17\376\241\7\336\44\1\123\276"
  "\105\151\212\14\217\100\223\163\222\160\256\20\316\5\207\263\322\54\6\115\160\172\160\250\21\16\331\350\160\250\247\17"
  "\207\306\177\63\242\247\73\374\7\372\67\271\347\225\370\114\31\344\270\122\17\147\46\76\127\377\265\44\367\206\156\367"
  "\273\257\161\174\241\77\26\333\347\365\273\73\77\145\131\257\35\347\267\60\355\256\76\24\337\356\150\245\303\161\241\142"
  "\362\210\204\354\13\366\102\174\77\376\30\261\126\154\22\222\27\300\147\77\357\200\307\142\42\262\275\236\32\167\266\12"
  "\6\102\24\140\205\174\103\46\225\257\374\145\212\214\177\101\221\314\170\274\374\25\217\25\272\263\370\312\120\217\74\256"
  "\277\313\115\57\104\52\10\130\6\264\344\205\46\156\357\61\224\151\144\221\260\33\70\10\50\67\302\175\35\171\255\235"
  "\117\126\356\127\247\313\2\53\303\7\130\144\202\72\372\105\42\204\200\342\205\36\270\364\122\142\341\152\363\343\361\31"
  "\120\100\276\304\253\167\34\74\276\341\203\45\114\370\25\175\44\131\255\44\104\275\326\215\107\205\336\302\60\105\0\57"
  "\73\303\223\317\100\163\210\257\76\224\371\165\352\103\231\317\352\303\304\324\207\62\177\27\365\1\337\3\353\156\247\124"
  "\0\304\360\161\62\351\335\255\157\243\207\224\271\350\41\220\34\377\116\232\311\254\231\314\232\111\231\207\153\46\172\212"
  "\213\257\314\314\312\14\262\231\217\244\336\264\375\320\61\225\232\366\166\312\112\333\16\263\15\75\266\12\45\275\77\277"
  "\355\267\116\327\351\373\301\376\262\75\56\226\366\117\44\105\214\253\102\123\17\106\273\67\225\254\77\272\337\77\372\225"
  "\174\116\376\365\177\16\177\45\177\374\357\177\377\43\371\343\337\377\217\107\326\267\55\345\343\320\333\242\56\23\203\205"
  "\101\26\352\252\53\15\102\176\114\377\151\110\251\45\6\261\127\205\67\371\145\76\267\377\372\77\207\277\114\222\146\57"
  "\107\364\311\174\256\232\252\154\62\223\255\377\312\222\272\77\61\371\174\173\172\27\200\173\342\203\76\26\300\57\300\373"
  "\22\270\277\106\5\167\300\315\24\150\62\376\163\313\240\112\261\347\107\20\275\205\163\13\235\51\327\314\325\325\311\243"
  "\315\152\271\52\13\141\243\151\163\106\232\365\137\111\322\40\256\250\202\273\24\274\302\273\210\117\236\125\154\245\144\123"
  "\154\324\305\245\307\320\370\326\275\132\26\176\334\104\210\202\332\113\155\16\353\224\136\236\221\20\237\3\162\152\304\0"
  "\335\334\42\156\277\22\253\151\74\357\6\256\273\204\271\345\170\25\21\334\112\133\13\311\11\121\14\314\152\351\370\32"
  "\30\172\203\146\323\364\303\267\11\204\170\70\244\174\242\10\3\47\164\163\72\366\306\326\143\133\346\350\154\32\54\221"
  "\53\130\212\374\226\120\237\154\312\225\167\207\375\117\223\20\20\101\44\304\367\347\255\165\122\62\217\57\33\273\226\77"
  "\367\37\302\251\366\66\226\17\52\350\17\351\155\57\140\236\362\265\364\311\17\1\42\317\10\21\131\212\50\376\256\310"
  "\263\55\207\117\346\154\71\174\56\270\152\44\353\321\75\357\310\267\360\276\347\370\125\354\34\52\110\144\53\210\265\24"
  "\46\100\25\374\55\315\220\143\104\61\45\23\220\3\115\102\302\332\325\50\3\121\72\301\150\47\147\305\3\202\20\277"
  "\30\40\342\60\63\41\230\360\63\37\361\111\200\160\66\302\243\233\21\144\343\144\360\105\246\124\242\17\167\122\337\110"
  "\225\105\25\22\42\52\323\21\175\0\227\210\252\321\4\63\226\30\107\110\60\116\107\46\12\57\162\102\246\305\300\14"
  "\24\201\130\35\102\120\102\225\156\45\217\22\131\10\125\106\122\307\1\123\302\161\16\10\370\116\64\373\54\54\341\256"
  "\364\140\200\325\53\107\256\73\344\111\2\53\107\140\225\241\317\370\126\26\105\324\367\3\254\202\61\106\21\130\204\63"
  "\166\341\234\121\61\257\260\306\16\66\145\177\6\274\261\172\20\312\250\75\224\141\365\312\317\247\123\300\50\44\353\76"
  "\152\51\202\271\24\101\143\207\345\44\17\141\211\350\361\103\20\246\53\363\312\60\231\147\22\37\151\133\335\162\134\323"
  "\317\253\222\363\230\363\252\6\70\314\171\125\172\70\314\171\125\34\162\361\357\33\325\261\106\353\217\25\153\104\2\211"
  "\372\62\323\51\364\364\13\261\373\376\363\274\52\151\246\166\373\373\146\47\313\222\110\42\350\203\122\226\267\216\102\212"
  "\32\101\144\312\150\21\104\372\330\36\255\326\27\71\24\103\370\211\162\135\21\142\162\356\372\312\275\35\361\312\175\214"
  "\213\164\242\164\211\20\213\240\247\175\324\213\170\24\215\160\267\114\66\351\57\26\135\137\170\53\355\362\75\232\254\277"
  "\110\131\304\166\367\355\70\64\55\173\74\311\53\362\241\5\102\11\104\106\250\147\227\14\302\101\242\161\60\212\37\132"
  "\11\20\371\363\157\275\73\311\212\246\51\323\52\253\222\274\51\140\77\326\217\313\244\62\165\232\233\272\116\222\304\124"
  "\171\122\313\117\366\107\371\267\377\374\174\354\270\254\27\151\135\77\225\311\262\176\312\16\237\262\341\242\306\124\145\322"
  "\230\264\220\0\43\331\244\100\10\111\327\67\103\226\347\46\53\222\246\255\232\366\351\360\51\35\146\310\22\223\244\115"
  "\277\361\141\6\271\64\227\306\142\345\307\147\166\163\213\113\250\75\341\260\106\156\243\344\246\15\77\115\353\152\121\225"
  "\131\331\346\131\151\176\257\276\357\155\212\264\51\213\202\255\36\43\40\0\217\6\205\111\1\320\105\223\345\155\322\124"
  "\315\42\253\114\57\250\136\226\224\175\316\122\323\244\165\322\340\212\364\267\163\174\375\204\140\364\233\112\313\247\47\363"
  "\364\364\124\24\205\51\137\67\225\366\130\222\26\165\201\233\12\305\311\62\135\347\151\273\156\27\131\263\156\377\13\0"
  "\57\102\76\257\114\155\160\272\113\367\261\230\24\361\237\326\327\0\261\377\344\14\106\156\12\160\237\227\171\232\234\33"
  "\346\304\251\131\361\226\261\224\353\66\2\327\230\322\107\10\31\265\72\74\137\364\335\372\177\307\116\51\140\30\301\16"
  "\374\63\234\127\171\365\310\236\243\16\370\66\247\4\322\273\340\201\1\304\370\213\220\24\105\54\242\362\162\171\100\3"
  "\350\205\135\51\135\307\362\21\247\0\270\222\223\76\5\5\44\212\23\44\220\275\46\342\273\270\26\116\311\365\111\226"
  "\164\260\117\326\150\137\207\321\230\327\33\343\314\345\240\304\277\100\24\163\321\65\211\307\216\332\270\213\254\134\247\353"
  "\47\263\314\263\164\325\213\227\174\220\57\57\72\233\155\330\322\354\104\62\321\272\316\353\104\24\33\63\4\250\17\115"
  "\64\124\365\167\7\150\131\342\371\341\131\251\107\0\52\123\105\253\222\235\301\357\203\14\265\375\21\306\364\176\225\20"
  "\54\204\137\2\143\41\261\104\74\74\2\21\53\300\62\15\103\35\375\241\152\174\312\2\126\353\130\374\64\217\275\160"
  "\70\0\104\204\33\37\5\267\207\13\343\130\213\137\40\0\70\152\151\157\273\241\154\122\330\22\201\324\333\61\245\273"
  "\110\76\342\227\121\276\164\24\301\217\27\373\156\376\354\126\277\302\201\305\375\243\74\12\226\123\203\277\336\107\232\344"
  "\15\342\33\172\41\371\343\44\210\370\350\56\153\220\271\231\340\272\13\367\370\44\113\34\104\47\272\236\266\66\207\174"
  "\103\264\10\104\160\374\265\140\270\176\245\210\125\154\231\202\253\114\30\242\16\56\344\1\165\34\21\133\302\136\100\342"
  "\171\134\363\13\110\341\344\246\310\255\302\237\360\235\207\137\122\57\356\374\222\372\373\142\167\332\164\135\277\201\62\267"
  "\172\317\25\164\72\373\307\120\145\3\356\267\177\364\363\322\252\141\107\136\255\143\370\354\117\163\136\55\364\146\37\276"
  "\364\206\54\54\201\36\3\75\51\364\144\320\223\107\275\222\317\353\273\277\222\237\257\344\347\53\171\105\355\230\371\132"
  "\336\237\327\237\251\74\115\363\275\74\271\105\214\170\235\56\223\212\32\300\217\123\164\203\253\152\62\261\37\15\52\3"
  "\271\335\143\71\246\370\43\313\213\236\42\74\161\277\366\347\160\143\50\145\75\57\72\330\301\133\111\167\305\343\60\56"
  "\32\151\334\102\43\220\302\261\370\112\120\234\341\171\206\307\237\207\126\6\345\251\75\41\45\100\311\53\370\223\116\11"
  "\125\126\307\124\124\275\324\244\65\41\166\223\223\234\176\345\112\75\222\117\31\265\170\272\146\1\75\31\54\105\370\201"
  "\360\114\202\127\167\16\127\376\234\127\2\223\230\64\334\131\313\0\257\177\372\75\176\25\43\223\351\223\235\77\40\16"
  "\24\223\50\162\244\177\131\236\234\33\307\0\374\10\307\206\35\60\4\200\135\36\307\257\16\32\240\313\124\130\210\100"
  "\376\130\77\60\145\25\363\44\120\133\345\265\317\121\313\371\23\171\214\0\216\337\257\353\52\242\10\320\363\263\207\105"
  "\242\137\75\127\230\137\255\65\262\310\37\41\60\162\176\107\375\23\251\362\211\140\240\176\313\147\337\115\75\162\341\304"
  "\200\311\371\13\217\170\4\341\351\334\310\253\265\163\1\47\275\360\131\251\260\36\31\145\204\143\67\54\257\10\104\153"
  "\1\167\0\207\223\123\211\66\346\207\220\306\345\3\101\307\10\257\52\310\370\23\172\105\101\26\25\341\325\4\371\145"
  "\250\300\43\217\52\310\170\60\21\370\262\270\30\300\357\244\274\6\31\376\366\6\254\135\16\307\152\124\322\210\25\126"
  "\307\267\164\277\32\175\305\353\365\304\61\175\271\113\324\350\335\255\310\46\120\106\146\377\120\165\50\273\241\72\224\51"
  "\361\124\76\373\170\122\270\346\256\114\331\40\214\245\244\63\275\314\224\213\260\231\17\252\12\152\230\64\321\127\26\271"
  "\167\47\240\111\160\145\1\17\251\337\75\270\2\252\202\117\276\54\322\364\241\237\116\241\152\322\364\301\224\115\241\250"
  "\322\364\301\224\337\107\315\45\6\330\351\127\144\372\321\355\16\307\315\356\324\52\43\136\375\1\256\303\13\165\177\272"
  "\121\257\44\164\325\16\160\175\355\202\2\110\203\211\272\351\66\373\235\335\355\64\241\100\322\117\34\305\356\171\175\222"
  "\137\172\344\71\136\247\353\317\106\176\47\235\46\361\366\32\273\167\353\54\303\371\353\237\46\167\366\321\75\156\267\163"
  "\324\53\364\344\320\123\100\117\11\75\25\364\324\320\3\153\116\23\350\101\370\246\320\223\101\117\16\75\5\364\224\320"
  "\123\101\117\15\75\260\346\54\231\166\241\56\20\6\327\106\3\276\362\26\231\336\33\17\230\123\377\276\60\172\224\10"
  "\150\343\311\53\34\257\154\123\270\361\356\171\273\205\301\373\77\250\137\6\102\241\342\215\367\313\355\77\327\217\35\110"
  "\110\131\165\274\301\105\142\127\240\323\306\232\372\176\223\316\24\25\226\244\217\200\222\354\225\321\216\244\347\240\116\252"
  "\251\327\42\175\221\226\253\170\345\114\264\52\177\214\13\276\33\40\115\270\257\67\251\323\66\116\73\261\333\215\335\254"
  "\355\146\25\207\25\40\136\373\165\156\54\115\343\72\201\45\62\56\301\21\12\335\10\306\153\36\51\106\150\160\0\335"
  "\22\270\213\221\14\40\125\327\5\23\164\5\326\15\257\250\20\136\130\45\26\315\361\107\375\274\317\65\10\340\25\331"
  "\231\141\320\51\75\320\51\247\12\235\346\112\350\344\143\101\7\220\125\372\246\6\35\341\74\32\317\110\30\170\0\133"
  "\245\157\202\340\61\327\162\236\162\54\370\0\276\112\337\4\341\223\222\327\304\341\143\305\353\62\73\367\275\42\371\265"
  "\325\310\311\32\304\45\24\105\35\100\217\122\357\140\130\255\227\217\166\146\200\336\135\55\343\112\376\364\306\277\63\324"
  "\51\243\32\50\171\172\345\23\165\336\56\240\34\13\127\61\247\201\54\37\37\167\121\44\361\214\42\65\171\66\320\117"
  "\260\355\125\147\370\323\377\205\66\21\20\171\275\164\106\67\45\256\267\51\304\35\171\33\144\366\107\63\216\136\305\213"
  "\36\217\141\173\203\161\344\33\354\214\175\244\253\343\376\160\227\14\125\260\157\146\252\202\236\74\212\255\323\46\246\360"
  "\360\14\55\127\200\173\125\40\215\364\135\270\35\372\54\263\7\273\335\70\355\332\151\127\116\273\164\332\205\323\316\157"
  "\345\373\350\167\202\114\321\307\251\262\13\316\213\302\63\106\251\34\243\362\214\121\53\307\310\161\14\243\135\107\211\143"
  "\244\251\162\214\372\137\75\41\215\245\160\347\327\32\373\305\150\26\11\316\51\235\61\51\51\2\372\42\206\274\135\366"
  "\27\46\35\15\302\150\375\110\147\174\136\45\270\146\331\125\262\52\372\354\136\75\32\44\112\37\44\312\273\300\265\374"
  "\355\270\126\216\247\145\242\173\112\72\337\17\325\312\167\140\153\265\17\22\361\135\121\176\0\340\261\304\347\72\340\152"
  "\222\316\333\3\240\276\75\6\244\140\20\113\347\215\1\40\162\36\376\204\332\142\232\330\355\302\151\347\215\323\256\235"
  "\266\133\362\243\164\332\205\323\316\235\166\346\264\123\247\355\216\357\254\57\155\234\166\355\264\53\247\135\72\355\302\151"
  "\347\116\333\205\127\352\264\315\255\264\343\176\46\274\334\207\271\345\147\250\14\312\141\332\11\153\232\41\122\317\20\215"
  "\156\210\14\325\374\52\121\16\141\160\10\243\34\242\360\7\166\153\140\201\103\64\225\162\210\32\207\320\256\242\302\41"
  "\264\47\122\172\14\236\104\271\14\343\31\103\173\46\251\147\214\124\71\106\342\31\43\123\216\221\173\306\320\22\212\147"
  "\57\306\50\307\50\75\143\50\341\221\327\236\61\12\345\30\306\63\206\26\323\63\34\43\325\342\130\56\162\115\44\325"
  "\120\366\307\130\255\324\152\145\126\53\267\132\205\325\52\255\126\324\250\17\204\203\154\131\237\141\27\173\215\65\130\267"
  "\230\370\204\116\103\21\140\103\0\157\374\105\43\301\361\352\2\365\24\326\332\324\327\54\65\145\113\5\266\165\341\3"
  "\132\11\337\124\220\303\200\372\61\365\123\306\306\314\0\277\113\166\265\1\24\163\67\50\147\340\22\2\171\101\374\205"
  "\340\155\210\52\13\222\143\127\246\307\56\145\161\124\365\232\122\374\170\112\10\61\121\341\40\270\202\322\101\357\4\25"
  "\350\307\277\266\142\205\133\241\130\357\255\53\65\53\112\240\372\357\315\310\173\53\100\321\144\46\30\106\176\13\105\106"
  "\302\267\264\75\301\264\42\300\340\303\147\370\51\336\363\251\362\272\360\75\13\362\250\205\100\123\345\172\272\21\262\23"
  "\232\107\144\377\263\211\217\356\62\127\350\225\60\216\345\107\156\66\75\101\144\266\130\206\264\221\320\163\64\314\54\121"
  "\200\42\330\337\13\113\241\64\256\166\167\46\255\77\314\366\114\11\373\173\257\105\241\126\17\274\302\44\67\144\26\46"
  "\31\221\133\230\104\317\56\56\124\327\142\77\237\76\303\300\23\27\360\204\153\162\21\20\344\366\332\41\34\347\35\253"
  "\51\167\160\222\212\53\270\144\226\335\267\0\163\132\24\270\326\73\23\344\162\311\126\144\174\267\23\25\353\346\226\142"
  "\335\114\231\32\231\247\223\7\11\170\323\205\157\227\57\154\337\230\324\126\253\271\315\135\107\131\334\336\133\73\175\11"
  "\305\237\20\216\202\355\13\362\212\55\301\146\314\46\227\316\133\143\163\146\267\314\155\260\271\112\146\154\236\14\66\267"
  "\141\330\214\221\311\322\171\163\154\116\255\126\146\265\162\253\125\334\350\216\72\311\46\177\15\121\231\21\156\41\322\44"
  "\257\247\166\51\130\66\167\10\373\370\54\161\146\211\353\60\226\210\274\105\72\157\316\22\313\7\236\133\276\125\344\341"
  "\362\367\63\45\350\324\176\31\46\253\21\56\133\354\41\160\340\153\237\337\40\77\12\260\55\163\313\356\226\3\222\37"
  "\115\347\156\342\276\216\144\204\147\373\263\306\242\246\74\231\251\351\146\324\224\47\76\152\262\272\345\200\24\216\15\314"
  "\271\221\316\133\113\212\334\74\114\46\323\231\110\166\53\34\212\375\126\136\171\160\244\174\167\72\356\172\154\201\107\74"
  "\207\202\245\143\6\116\341\230\270\124\375\317\205\36\141\237\260\27\101\313\10\162\76\117\155\316\224\75\360\355\110\47"
  "\307\145\166\62\63\213\72\12\324\155\26\145\165\313\111\341\253\230\271\175\204\36\25\366\240\267\156\22\105\114\146\370"
  "\333\355\172\303\216\120\241\374\105\373\256\236\174\47\320\16\10\4\75\140\217\2\23\125\207\167\207\204\341\175\5\324"
  "\352\26\374\37\103\121\313\113\233\226\252\231\35\276\303\251\127\276\123\267\272\345\244\24\32\33\34\246\164\336\136\143"
  "\253\37\170\116\206\140\3\367\43\145\157\50\264\305\236\11\21\350\15\125\337\3\236\25\271\303\10\12\214\162\341\317"
  "\235\225\323\17\217\271\227\343\41\7\61\211\373\164\312\262\320\355\46\235\67\147\131\15\311\306\376\307\124\100\307\300"
  "\1\351\274\365\251\24\311\130\267\141\163\151\373\14\141\51\235\267\75\130\151\15\177\362\152\104\326\225\51\46\54\47"
  "\341\16\132\74\20\376\224\76\216\206\122\6\307\345\106\232\154\76\42\232\20\100\204\277\154\324\236\237\65\352\41\326"
  "\77\164\272\77\236\376\370\267\327\216\177\33\46\227\47\213\16\276\47\214\354\276\305\341\201\75\141\44\257\34\275\266"
  "\217\153\371\231\275\257\176\64\217\244\275\360\320\110\273\331\372\156\132\271\233\357\165\63\36\115\100\16\15\137\202\202"
  "\305\365\133\41\325\120\121\341\220\357\310\71\271\307\204\247\64\341\123\221\77\277\220\267\24\2\335\77\241\25\211\314"
  "\352\303\237\135\347\36\136\347\77\275\247\347\166\362\104\326\257\261\135\37\141\234\276\373\116\316\152\7\207\265\273\362"
  "\264\226\373\347\335\351\303\37\240\364\15\33\372\210\207\370\303\75\303\37\234\135\362\147\370\76\30\177\43\40\152\1"
  "\106\172\311\317\201\66\35\121\115\300\321\1\70\364\114\232\303\143\142\134\225\200\144\207\60\11\347\205\34\116\123\144"
  "\136\4\126\307\365\156\361\175\275\70\11\250\176\367\270\220\332\157\127\355\352\165\323\322\165\130\234\276\331\235\273\365"
  "\317\166\5\135\303\357\210\360\130\37\217\373\243\137\134\14\323\143\357\260\202\113\375\2\233\36\44\137\277\364\377\205"
  "\166\316\353\252\175\275\366\360\322\117\206\227\77\374\134\164\233\307\156\267\70\164\337\366\247\307\303\161\375\143\263\376"
  "\151\276\274\174\377\70\300\32\354\347\63\34\170\42\320\333\174\305\232\301\305\62\33\76\334\355\277\234\377\23\151\333"
  "\302\64\362\304\2\307\107\27\33\5\27\1\361\364\130\326\177\334\133\261\355\312\126\124\372\257\245\227\74\310\372\101"
  "\136\337\354\320\77\143\121\7\173\235\240\135\356\116\333\57\155\157\364\367\210\272\35\276\260\60\125\140\211\10\101\160"
  "\21\350\210\275\277\234\203\372\241\130\146\77\117\300\62\255\135\352\375\203\260\145\345\130\271\214\45\233\266\271\167\200"
  "\153\205\75\213\122\225\112\116\240\137\13\141\10\337\367\77\54\206\320\267\201\41\134\105\370\36\372\76\376\23\310\175"
  "\44\132\247\24\44\351\5\357\213\104\176\7\147\30\5\133\102\114\300\366\274\333\364\150\372\345\171\267\335\354\376\143"
  "\161\2\230\35\365\321\100\366\131\23\110\145\46\54\273\103\61\125\125\302\124\1\61\75\241\147\2\107\160\374\336\17"
  "\266\70\241\271\256\332\144\221\217\201\316\74\305\367\70\62\227\74\234\265\72\341\222\103\333\345\222\235\207\53\212\225"
  "\54\112\270\374\354\72\337\100\11\302\331\145\112\364\20\120\215\76\357\0\206\352\173\1\164\335\10\41\167\35\277\310"
  "\13\302\364\16\173\210\365\0\24\153\121\10\352\2\355\317\343\346\264\226\107\261\0\265\353\353\206\71\74\237\226\60"
  "\12\270\201\364\243\40\41\310\171\102\337\215\141\143\222\267\355\12\310\262\377\277\377\17";

/* lib/stdlib.wo (DEFLATEd, org. size 70385) */
static unsigned char file_l_41[6320] =
  "\264\226\147\217\253\70\24\206\77\157\373\17\26\303\207\144\53\46\5\222\355\275\367\276\213\204\14\330\271\350\202\215\154"
  "\63\145\177\375\72\230\70\140\23\45\167\212\64\223\230\207\363\276\347\270\147\126\263\242\255\60\360\205\54\252\62\173"
  "\355\145\0\146\145\335\60\56\201\227\163\351\1\117\64\276\7\146\273\212\145\250\2\276\142\133\105\300\254\156\45\50"
  "\27\341\174\76\237\320\324\270\146\374\116\311\164\103\313\372\166\140\24\347\263\250\4\126\264\220\274\244\73\235\42\157"
  "\366\51\110\113\163\340\153\276\325\24\314\32\304\121\335\311\117\267\71\26\155\65\331\11\223\104\65\150\136\67\166\226"
  "\36\77\132\32\125\265\300\322\316\242\351\143\366\245\302\324\116\242\351\245\146\230\163\312\74\363\155\346\253\173\336\166"
  "\237\147\346\114\334\40\147\70\173\174\276\247\356\152\223\167\15\126\306\245\50\312\135\51\215\161\307\267\75\75\337\71"
  "\333\114\64\50\307\266\231\246\57\156\166\73\135\332\355\275\152\223\254\142\67\230\133\156\75\275\324\254\106\362\231\362"
  "\252\12\174\333\30\247\75\334\166\350\140\103\326\313\351\125\246\136\330\226\55\55\205\54\224\251\310\370\163\343\251\351"
  "\166\317\56\255\355\6\211\62\25\24\65\342\31\223\151\303\361\165\211\157\240\62\46\105\172\303\113\211\215\371\164\344"
  "\366\20\167\331\276\171\140\121\230\136\227\234\321\164\207\345\271\272\6\241\117\122\202\50\377\303\342\322\102\214\340\321"
  "\313\151\70\313\123\174\133\236\55\303\4\16\123\116\136\15\214\112\314\353\164\357\46\254\73\142\370\112\33\234\326\347"
  "\254\245\322\325\233\127\47\365\373\220\222\42\263\364\264\324\120\267\176\223\102\337\251\333\264\37\162\60\303\267\332\370"
  "\100\274\271\271\110\125\113\175\276\235\63\52\244\271\43\13\44\221\261\51\4\364\201\176\217\252\162\107\337\207\300\233"
  "\375\360\373\167\337\315\223\40\360\46\4\315\376\120\20\51\43\51\14\300\65\342\275\54\6\236\22\130\177\376\107\56"
  "\373\333\141\161\234\344\213\61\215\227\111\21\45\233\350\343\356\41\110\160\20\335\251\240\217\77\115\140\104\223\140\225"
  "\144\372\57\116\66\213\57\22\262\112\310\346\303\4\157\222\140\361\143\374\175\230\300\42\120\344\253\233\44\16\377\171"
  "\57\311\210\110\42\222\24\305\217\11\134\265\336\364\40\270\243\360\365\17\137\174\375\303\327\277\375\355\214\203\321\204"
  "\216\346\207\217\177\230\275\176\152\344\70\242\305\62\116\5\306\305\140\340\102\260\337\63\357\257\47\4\270\156\344\235"
  "\231\347\243\144\251\45\313\311\252\26\156\125\45\225\230\123\124\1\314\71\343\133\320\122\224\125\30\110\6\70\226\274"
  "\304\327\30\364\111\152\114\45\320\27\250\110\350\211\176\144\155\376\34\113\341\26\4\203\170\42\274\225\44\116\145\227"
  "\161\250\360\366\363\32\234\373\213\310\64\307\203\166\76\174\265\76\266\211\322\106\272\75\312\105\116\370\4\371\110\113"
  "\206\251\343\3\217\247\175\140\150\153\341\120\16\73\156\374\307\76\60\36\153\165\352\205\46\240\243\130\77\330\56\20"
  "\273\312\361\220\55\115\373\105\377\314\344\353\343\311\154\177\104\313\174\217\301\341\210\362\305\350\220\367\251\176\354\143"
  "\52\226\357\17\55\122\164\324\74\147\215\77\6\304\6\45\264\101\350\17\235\365\161\370\366\16\113\363\313\136\277\350"
  "\302\337\26\130\166\131\316\4\37\117\107\270\76\22\321\146\43\235\260\165\27\145\47\156\222\320\15\52\206\150\147\164"
  "\107\46\61\356\106\303\211\23\203\202\45\343\30\60\102\224\347\373\301\141\217\271\316\156\105\313\43\100\105\341\346\15"
  "\335\274\364\305\363\26\227\224\2\57\11\212\47\352\125\361\325\331\237\156\72\264\340\254\161\322\144\215\177\146\272\71"
  "\226\55\357\372\75\261\45\322\156\117\220\343\105\334\3\157\76\336\47\244\226\343\235\202\32\177\142\257\50\74\136\370"
  "\142\374\170\215\252\61\300\343\107\113\135\73\352\253\310\322\137\155\54\207\53\30\130\46\127\20\272\76\60\264\215\340"
  "\322\161\132\71\116\353\11\247\310\161\162\152\12\235\232\102\253\246\107\74\143\14\130\330\140\151\203\225\15\326\66\210"
  "\154\20\333\140\363\304\107\133\360\44\147\233\265\215\220\33\247\230\106\231\142\317\201\17\115\4\153\200\37\272\36\244"
  "\226\307\122\53\206\212\70\25\366\371\2\373\10\162\117\265\65\72\213\350\110\50\376\277\275\63\121\123\34\327\365\370"
  "\243\334\245\346\356\33\11\153\235\363\60\174\20\122\335\231\16\41\223\204\132\346\351\157\102\65\237\332\371\341\22\52"
  "\7\252\172\232\263\116\134\136\44\131\376\113\226\154\163\354\333\331\247\170\220\52\315\353\324\107\3\252\373\61\66\230"
  "\352\364\217\357\364\24\33\105\64\1\244\326\276\26\202\301\160\22\24\31\331\70\216\242\51\130\6\247\242\162\50\202"
  "\265\7\243\320\133\41\20\66\16\263\130\367\207\30\235\122\53\32\161\226\210\100\173\1\265\163\45\135\245\17\377\213"
  "\115\136\237\274\231\322\133\261\31\106\137\6\127\203\321\350\23\251\1\132\264\326\354\54\267\153\364\66\46\244\350\30"
  "\354\310\120\344\136\376\46\103\376\117\34\115\346\223\305\170\66\131\370\4\350\251\16\132\266\236\111\305\310\130\11\171"
  "\263\254\145\104\126\103\137\60\134\66\102\120\137\224\133\376\101\354\323\304\261\117\343\337\234\77\116\151\256\250\120\16"
  "\275\230\326\362\54\307\176\113\235\227\222\52\335\56\367\120\247\5\325\111\34\364\5\27\325\221\173\162\245\215\277\311"
  "\36\227\173\260\41\115\70\347\137\232\143\203\165\265\314\36\104\256\355\370\316\207\310\77\100\237\206\235\235\230\53\167"
  "\172\276\164\241\235\203\322\66\46\155\13\3\155\240\10\45\47\251\6\221\264\40\237\324\160\104\363\117\156\70\356\346"
  "\357\66\35\6\313\162\167\117\333\162\27\1\174\205\40\110\11\344\12\276\336\105\221\203\231\163\7\120\147\56\240\56"
  "\260\332\205\22\303\312\150\233\160\161\20\111\204\270\53\41\353\302\104\202\37\134\331\112\301\327\231\100\252\363\41\323"
  "\201\125\65\250\354\247\3\3\23\265\23\152\54\25\177\146\200\212\76\73\100\105\361\131\61\272\120\204\212\70\172\27"
  "\111\302\330\102\22\4\5\212\35\220\232\271\121\211\221\203\122\367\56\112\105\360\320\204\32\333\122\231\162\251\314\116"
  "\142\304\14\22\275\34\112\105\221\201\4\5\245\146\26\224\272\27\140\352\175\104\43\300\324\45\144\77\37\30\246\250"
  "\241\320\145\251\371\123\343\124\114\234\322\43\146\3\23\21\217\224\0\330\47\102\115\335\257\23\216\102\140\363\224\147"
  "\27\323\166\12\115\176\330\74\345\333\305\221\13\214\143\7\66\243\270\207\233\160\301\204\34\323\332\215\107\134\273\364"
  "\255\204\76\316\247\0\47\172\332\240\211\273\273\334\237\124\157\14\276\121\20\331\253\376\36\12\40\21\371\363\375\374"
  "\124\317\205\15\364\47\6\371\51\250\37\233\174\323\50\26\244\307\327\230\270\177\1\335\271\37\32\367\271\304\144\61"
  "\262\346\165\161\137\110\66\375\303\272\352\322\63\374\253\123\52\371\234\341\222\251\253\146\367\103\46\265\375\102\32\265"
  "\346\271\301\177\144\20\235\72\111\131\327\115\325\354\66\347\21\225\71\104\145\12\121\310\352\36\217\105\310\147\372\105"
  "\152\71\204\142\52\13\275\112\333\233\55\317\126\353\26\127\4\346\236\126\65\347\62\14\132\133\113\226\336\256\207\302"
  "\76\2\267\10\362\332\305\1\314\30\163\51\267\344\276\236\164\352\306\354\310\350\6\143\210\223\62\341\164\42\102\173"
  "\151\111\267\103\101\160\63\107\215\346\103\250\221\234\240\26\65\242\371\363\237\70\331\356\363\213\110\303\76\377\13\314"
  "\277\174\140\102\105\233\347\47\265\171\316\220\215\60\44\32\241\73\42\205\130\120\75\277\122\200\152\241\113\105\304\334"
  "\101\304\374\206\210\67\104\274\41\342\15\21\177\145\104\354\101\242\212\211\263\11\61\261\55\74\37\23\333\312\377\233"
  "\76\67\55\205\313\366\157\313\372\206\224\67\244\274\41\45\327\5\327\271\324\373\324\370\11\116\244\364\172\250\72\233"
  "\350\250\332\325\42\304\100\332\26\124\225\55\372\162\223\276\175\347\240\114\334\357\226\15\267\240\35\255\364\306\15\176"
  "\100\333\26\276\173\147\206\253\125\322\144\273\242\253\57\245\35\312\27\151\321\270\245\345\306\155\233\270\237\47\373\106"
  "\341\166\125\64\365\237\156\331\146\327\224\273\32\275\365\253\225\47\107\20\213\22\172\344\131\263\45\107\11\352\365\316"
  "\251\164\24\217\126\361\50\62\34\140\43\66\276\312\121\257\370\52\134\325\52\2\16\354\307\77\313\104\307\11\104\60"
  "\23\15\27\17\51\221\77\275\203\46\100\236\331\240\107\240\41\146\104\205\323\145\55\50\340\204\207\1\111\316\354\162"
  "\56\245\27\110\326\202\330\322\200\103\33\372\221\106\166\27\4\344\203\300\126\53\131\117\247\232\206\202\315\60\153\372"
  "\131\73\373\54\121\127\164\21\223\172\151\204\271\127\72\135\110\311\27\360\202\172\332\200\224\235\161\46\210\162\72\17"
  "\347\235\175\356\254\235\1\217\320\162\260\313\147\42\254\7\161\17\236\107\377\73\372\361\137\345\177\215\376\361\337\377"
  "\376\217\243\177\374\373\177\364\75\2\335\301\20\41\302\213\27\270\236\50\250\62\240\72\122\340\324\66\256\340\20\354"
  "\12\65\43\64\3\324\256\17\41\30\215\22\321\51\127\43\30\41\21\225\360\73\371\111\230\3\116\276\134\105\254\104"
  "\357\134\350\237\360\370\63\50\167\26\315\143\132\65\313\166\104\354\357\305\75\365\102\223\130\15\231\172\213\370\105\72"
  "\263\373\1\117\175\274\217\204\150\24\351\207\76\270\4\351\330\331\325\67\134\176\23\375\372\122\310\122\102\3\372\345"
  "\3\317\306\144\174\51\206\224\133\32\123\36\52\306\220\341\261\5\364\332\112\220\12\311\70\354\307\101\245\36\212\305"
  "\0\302\27\170\265\70\276\330\64\125\316\301\143\337\161\344\251\174\350\24\174\234\373\23\177\224\373\103\311\310\142\306"
  "\126\17\37\46\326\351\207\102\353\75\336\17\200\305\324\255\242\225\176\345\60\155\124\164\154\144\167\341\202\120\143\45"
  "\126\36\247\121\4\247\362\72\113\140\374\6\310\352\153\0\256\304\202\107\204\122\171\355\255\277\144\376\47\53\36\372"
  "\145\377\165\50\343\312\10\132\26\165\232\247\111\203\125\170\172\31\107\47\227\161\164\134\306\62\325\22\56\344\355\27"
  "\367\266\56\137\130\242\65\330\60\254\16\265\321\71\25\231\110\354\145\341\6\360\357\265\276\141\23\374\247\157\105\2"
  "\320\42\221\354\311\23\265\13\371\263\143\140\41\131\230\32\73\3\365\327\212\56\266\264\1\325\312\273\44\234\66\104"
  "\346\235\17\231\201\213\203\275\154\33\74\363\44\122\337\144\217\312\256\3\270\157\34\201\363\252\357\153\206\107\270\111"
  "\70\302\5\101\21\350\166\62\25\136\371\212\30\217\43\52\363\141\117\276\174\115\237\77\113\362\305\236\126\341\253\71"
  "\267\74\313\55\317\22\220\147\171\276\45\132\156\211\226\133\242\345\315\116\247\147\46\132\246\27\114\264\140\351\41\122"
  "\163\113\303\334\322\60\201\241\272\133\32\6\141\107\277\11\314\217\306\210\46\117\263\233\323\71\272\361\306\131\230\24"
  "\260\347\175\160\207\323\77\340\375\374\102\74\107\243\170\160\246\101\267\60\215\321\275\232\103\231\350\243\315\246\241\243"
  "\301\266\63\11\70\263\47\1\245\347\133\236\217\257\67\174\130\236\57\212\177\201\74\337\55\317\167\313\363\335\362\174"
  "\267\74\337\55\317\167\313\363\301\365\233\334\22\175\37\223\350\123\42\347\103\257\117\372\125\166\345\344\212\344\302\22"
  "\43\50\277\311\244\170\231\267\144\313\365\223\55\307\225\356\313\255\230\157\125\77\354\363\334\55\111\14\267\254\65\114"
  "\355\172\37\376\362\340\260\101\174\305\267\100\114\74\370\162\37\367\56\372\226\105\144\251\305\52\224\67\2\303\257\253"
  "\351\163\115\235\17\164\377\321\155\310\55\101\371\351\72\112\123\251\72\274\120\303\205\44\262\21\227\73\242\313\115\120"
  "\54\214\103\25\241\231\224\202\11\77\324\21\45\32\302\162\353\170\52\27\262\137\277\75\67\262\255\231\152\7\121\5"
  "\244\75\17\133\334\215\245\340\52\271\141\152\256\127\261\164\334\246\346\206\337\372\16\130\17\241\132\372\23\206\164\176"
  "\362\170\116\70\301\13\65\42\151\31\47\322\337\224\274\140\350\123\241\141\261\360\104\76\3\142\377\347\330\241\230\163"
  "\113\17\233\256\275\54\171\317\251\41\101\351\101\165\170\76\376\4\227\121\246\172\220\32\203\170\176\236\45\372\315\57"
  "\311\32\76\55\54\355\260\322\100\367\176\235\274\146\336\133\11\270\26\324\140\173\334\316\26\203\200\113\2\145\377\5"
  "\142\226\103\256\352\305\47\110\75\215\206\133\325\261\155\125\337\215\375\353\272\35\354\243\243\200\50\22\202\103\327\134"
  "\261\52\76\150\375\114\76\164\375\250\377\240\133\161\273\235\346\323\52\372\156\352\220\61\153\163\345\356\356\51\361\104"
  "\243\376\252\147\102\70\173\127\72\12\22\307\141\274\52\257\311\45\130\306\140\313\50\212\331\364\102\242\270\37\135\135"
  "\22\344\112\227\4\203\40\132\260\242\150\166\373\355\252\167\322\276\50\233\12\21\12\224\255\127\165\352\226\24\352\103"
  "\163\333\254\330\367\177\130\326\75\301\316\307\214\30\303\70\53\40\161\30\352\275\17\330\75\332\302\24\305\171\152\325"
  "\311\325\356\33\304\323\51\157\73\371\343\35\26\245\44\155\54\64\155\307\245\211\347\107\217\13\226\30\317\133\26\41"
  "\17\356\171\246\55\170\246\16\312\324\325\361\47\2\120\211\201\236\201\34\320\263\206\34\333\374\115\73\33\376\5\371"
  "\363\353\235\356\66\165\340\150\75\136\342\241\53\166\274\361\375\45\241\206\41\57\35\117\354\324\134\77\60\106\52\76"
  "\66\64\306\71\326\165\117\137\41\261\276\102\40\256\31\252\211\346\12\163\312\262\371\374\352\171\15\74\270\70\250\101"
  "\63\365\271\243\343\311\6\100\63\350\266\202\153\321\354\243\201\355\6\154\67\140\123\102\32\372\325\240\13\270\362\342"
  "\252\43\244\241\375\44\24\56\124\16\247\264\232\267\222\67\201\141\215\107\377\0\334\172\361\236\4\111\265\335\222\170"
  "\374\253\240\175\300\75\217\343\226\335\32\264\144\133\45\45\42\104\333\217\320\311\236\200\117\77\353\317\73\77\112\25"
  "\34\111\104\105\31\377\254\230\110\326\306\104\344\20\207\224\311\101\216\341\43\45\212\136\141\152\270\300\320\1\360\207"
  "\61\37\375\64\13\144\21\52\212\13\361\316\370\127\70\357\171\217\361\174\50\256\333\217\353\163\55\325\237\252\125\331"
  "\335\371\72\133\16\20\104\376\113\314\377\276\317\367\376\127\325\200\175\16\121\374\52\72\100\374\333\377\2\370\367\173"
  "\265\52\66\223\205\313\344\163\153\151\63\365\60\370\363\273\37\245\1\157\207\1\135\107\73\232\55\367\175\27\43\146"
  "\316\144\72\146\330\34\236\344\336\72\140\34\72\40\67\317\352\57\156\174\315\341\357\132\110\236\14\115\362\70\16\41"
  "\271\76\220\334\167\357\342\151\34\115\356\107\343\373\150\116\376\270\107\160\216\116\150\324\170\10\360\110\121\167\171\175"
  "\50\252\213\126\174\344\150\346\121\141\122\244\357\205\171\62\207\364\132\165\256\132\356\77\222\315\211\302\346\130\147\223"
  "\172\372\141\154\16\77\13\12\170\167\330\55\66\253\373\72\232\53\302\66\317\20\275\42\377\262\116\123\47\331\110\303"
  "\240\276\150\250\230\127\227\314\356\223\107\345\133\42\332\160\11\314\214\325\256\350\214\162\345\203\305\361\150\76\322\247"
  "\136\37\54\174\121\37\345\62\234\252\352\364\6\254\116\322\313\330\261\250\173\60\103\272\352\255\326\255\312\211\352\35"
  "\76\133\325\243\116\213\372\77\255\352\154\131\27\253\262\376\272\153\226\145\225\76\146\351\123\364\267\262\332\45\313\364"
  "\71\153\316\31\265\351\52\312\260\257\337\175\225\177\50\64\17\113\124\337\271\350\372\345\370\243\327\273\242\111\253\355"
  "\62\331\355\213\306\203\212\370\245\171\127\13\234\176\72\36\152\175\44\275\206\32\20\223\366\265\336\236\257\153\241\63"
  "\305\222\77\24\172\350\212\361\263\201\116\337\270\252\100\105\350\52\256\232\175\355\361\214\137\377\50\12\372\52\255\116"
  "\126\131\321\352\325\31\4\144\105\326\144\55\237\177\246\313\264\170\314\252\135\41\344\360\157\257\304\211\12\246\125\265"
  "\253\134\270\55\366\333\145\331\124\265\133\372\275\375\162\275\177\70\375\7\66\131\227\75\40\177\220\2\5\373\261\42"
  "\344\147\340\221\374\352\312\374\225\271\152\44\76\254\374\326\274\76\172\307\22\53\37\147\346\50\163\173\364\326\370\133"
  "\370\22\62\225\24\111\132\141\213\51\304\242\114\261\13\12\170\36\125\240\156\25\255\136\266\335\142\331\36\324\114\67"
  "\12\232\244\344\272\255\217\75\147\377\4\34\60\275\340\43\53\101\77\2\362\56\262\46\102\26\334\263\155\373\261\113"
  "\50\107\131\205\141\104\371\130\73\105\113\362\46\55\272\204\174\52\356\162\203\141\37\252\124\256\255\12\23\114\100\200"
  "\24\165\24\135\227\57\246\305\1\302\120\330\36\112\200\104\35\342\131\20\124\25\33\122\243\141\322\102\165\13\274\67"
  "\51\306\361\157\272\177\140\247\300\346\113\307\350\157\72\265\172\55\161\70\231\321\71\135\104\63\253\1\170\330\54\237"
  "\252\254\111\373\171\117\314\106\272\55\233\27\321\243\101\25\115\167\225\332\76\332\236\305\75\172\375\106\10\170\265\115"
  "\125\327\75\117\13\270\101\36\27\257\353\317\2\11\154\51\353\272\312\212\57\135\0\270\35\36\256\110\127\106\20\240"
  "\13\350\270\11\261\356\276\140\240\266\134\216\137\360\110\256\324\320\55\61\176\111\102\50\302\121\17\212\305\76\244\64"
  "\241\300\104\272\105\262\55\225\103\30\241\243\162\211\32\176\114\55\12\77\100\67\34\375\252\117\105\370\347\221\7\322"
  "\244\243\53\24\22\357\133\364\76\142\371\240\250\164\364\130\327\351\252\112\276\12\174\174\57\350\343\307\267\364\105\75"
  "\70\260\115\267\153\267\250\363\235\335\222\126\11\125\30\332\146\233\154\363\374\275\232\224\355\326\277\273\145\233\354\341"
  "\101\332\33\256\104\164\204\102\173\360\127\117\24\154\223\75\56\367\230\255\127\232\365\263\17\376\272\7\141\51\101\3"
  "\31\361\50\21\364\322\116\24\312\74\125\333\271\20\250\130\146\305\46\253\322\244\71\116\125\73\56\147\12\76\100\67"
  "\7\106\77\231\104\51\57\216\251\243\360\75\70\214\243\110\233\2\306\334\101\73\70\243\72\150\370\17\161\111\277\151"
  "\256\353\14\233\211\334\14\227\116\354\170\121\244\117\137\126\245\213\15\155\201\176\176\240\253\246\275\42\312\240\66\227"
  "\235\253\176\47\372\14\377\121\116\225\150\323\123\274\121\344\314\232\164\52\315\324\21\375\141\121\126\63\15\244\117\370"
  "\37\265\23\215\76\174\212\161\10\265\5\204\177\121\47\211\237\271\237\75\53\120\106\275\357\330\375\256\237\126\145\231"
  "\156\250\224\134\317\224\33\153\172\177\345\204\30\6\207\124\174\113\70\203\230\32\101\146\254\75\22\253\136\71\74\112"
  "\201\257\11\53\355\262\223\46\15\325\312\110\71\273\234\251\222\27\316\11\224\242\371\76\7\173\352\312\164\246\123\40"
  "\143\171\55\0\254\206\360\251\230\170\315\336\250\51\201\62\36\316\134\333\155\46\330\4\101\302\22\366\32\355\274\326"
  "\117\136\20\243\116\322\122\235\71\177\231\361\20\163\46\73\171\215\143\335\103\220\106\70\11\335\373\230\362\64\264\77"
  "\172\1\271\350\226\101\46\164\357\76\277\42\313\103\337\77\50\331\360\165\365\355\260\262\172\270\257\237\267\52\75\270"
  "\273\346\344\171\156\153\101\207\373\263\50\166\173\137\124\351\52\371\272\132\347\162\163\110\27\67\111\231\372\34\267\257"
  "\371\217\372\276\57\262\126\76\7\341\160\375\62\357\146\170\4\123\363\207\113\75\136\13\250\34\102\226\244\103\327\235"
  "\162\237\347\357\324\235\143\35\51\350\177\307\203\275\43\50\5\323\176\301\354\355\223\32\353\175\362\55\155\152\115\257"
  "\224\234\260\22\260\200\176\111\357\226\360\233\64\324\306\272\174\277\23\65\372\15\112\14\267\71\56\75\125\214\203\333"
  "\305\241\363\302\56\211\16\66\154\345\16\102\151\352\15\233\163\175\103\107\337\253\112\3\203\266\142\305\327\361\137\151"
  "\145\7\34\206\22\151\130\126\31\33\353\117\233\13\251\6\365\217\255\354\115\355\10\162\135\340\230\15\56\177\23\102"
  "\310\102\206\223\247\30\365\175\221\147\305\133\56\341\361\273\134\207\37\217\44\217\372\301\3\75\34\171\315\171\216\116"
  "\315\163\270\175\20\207\315\56\42\232\260\113\121\130\154\2\272\326\347\220\22\260\17\140\107\112\135\312\66\371\350\136"
  "\164\375\365\234\345\366\56\77\172\215\337\163\136\147\135\36\74\141\151\275\117\222\13\372\334\77\106\221\132\171\124\57"
  "\237\304\62\327\142\265\260\3\102\255\140\207\154\60\307\207\71\15\374\6\34\10\226\152\330\114\76\30\333\112\4\117"
  "\326\222\50\127\77\107\274\267\75\67\126\244\147\145\263\365\341\242\271\214\107\141\207\46\245\245\53\205\62\261\305\260"
  "\257\350\311\20\367\122\206\206\366\112\104\113\326\37\337\336\60\51\1\1\125\260\344\172\112\200\341\76\136\11\110\331"
  "\225\225\300\60\375\376\177\220\252\340\354\222\33\31\335\301\12\367\117\350\41\15\260\215\261\273\36\24\143\370\250\321"
  "\314\174\340\205\233\72\252\244\376\74\244\220\275\300\132\277\256\127\76\175\227\322\350\276\332\103\126\154\326\337\232\143"
  "\276\136\234\263\325\113\207\72\152\334\163\235\177\333\146\305\351\254\46\51\176\355\24\242\217\46\363\311\142\74\163\256"
  "\324\40\252\256\366\143\120\23\236\152\70\162\302\13\13\30\35\25\163\315\211\210\117\273\101\360\250\364\174\230\317\252"
  "\161\4\220\44\350\241\135\273\211\347\347\134\215\114\17\207\322\45\333\376\275\240\237\157\57\361\142\213\242\121\160\370"
  "\63\367\273\50\335\357\135\365\245\350\267\270\233\364\167\3\167\323\341\166\2\24\377\73\155\7\25\130\363\106\3\135"
  "\220\364\217\213\145\127\110\32\136\116\3\77\353\214\252\307\225\61\74\271\152\112\117\106\266\237\306\271\154\144\130\301"
  "\63\157\102\115\244\60\274\60\75\367\253\40\112\66\324\21\174\170\1\352\326\145\75\200\107\127\330\335\36\140\263\307"
  "\163\127\237\247\22\164\346\31\274\322\36\32\102\47\35\342\222\212\167\200\240\207\42\355\331\141\320\240\125\43\242\310"
  "\261\3\36\10\111\312\27\317\33\305\372\376\10\123\201\150\30\131\327\177\226\45\144\114\164\252\350\17\334\102\202\366"
  "\335\304\4\333\62\226\64\66\344\345\356\246\357\264\256\154\251\137\22\12\131\310\34\230\73\43\132\146\175\223\301\316"
  "\303\367\77\234\104\356\200\6\42\156\146\270\216\20\164\75\140\333\163\110\267\360\107\175\16\150\60\306\52\224\45\75"
  "\312\22\17\145\356\31\324\77\15\7\226\224\223\221\104\324\372\317\353\170\3\330\336\221\12\36\232\126\115\164\341\351"
  "\111\166\323\60\174\101\163\114\130\12\365\370\25\52\170\112\61\155\274\106\311\176\332\111\256\247\212\122\166\137\334\274"
  "\135\166\303\63\122\104\16\156\365\367\110\152\341\250\375\70\177\365\353\376\254\74\352\153\330\256\263\133\375\267\337\13"
  "\13\307\271\303\162\376\153\360\334\143\332\303\365\154\242\76\17\210\347\161\245\200\134\353\117\352\262\333\1\271\156\1"
  "\122\170\156\77\372\34\127\345\157\275\130\313\176\353\26\154\322\202\146\344\63\76\23\22\315\2\237\11\1\140\77\240"
  "\314\263\157\333\157\121\326\212\315\65\123\165\300\165\366\40\137\124\43\256\112\267\166\342\72\305\61\134\154\157\215\321"
  "\256\172\151\313\312\227\241\257\207\347\216\212\347\177\75\35\277\351\370\115\307\173\112\176\276\226\317\46\256\226\267\5"
  "\146\55\277\151\271\230\146\321\362\356\363\264\42\55\206\170\32\45\266\23\47\132\256\23\147\327\362\150\166\161\65\337"
  "\256\363\264\20\65\77\174\342\135\66\75\65\244\173\230\265\262\203\72\222\323\354\236\264\255\272\124\24\302\17\337\132"
  "\122\253\266\346\270\262\136\16\53\31\365\276\335\317\242\367\235\273\237\215\147\211\123\126\274\146\126\72\127\21\263\274"
  "\116\377\100\263\254\324\323\116\320\372\204\121\210\134\17\124\24\11\211\106\62\43\340\62\122\341\273\225\51\274\343\46"
  "\54\163\374\373\346\141\261\154\272\60\10\30\150\224\207\131\232\113\76\313\222\170\212\54\77\236\222\170\57\36\312\234"
  "\32\370\101\104\21\255\225\324\203\376\70\103\176\156\267\121\354\247\112\124\327\322\341\214\35\312\266\221\172\305\120\210"
  "\142\303\162\263\237\123\44\206\205\200\226\10\276\201\11\0\3\212\254\27\135\113\50\234\15\171\60\136\274\220\242\347"
  "\135\245\205\271\211\134\350\361\76\106\13\105\62\174\120\230\231\110\317\357\202\356\52\2\347\333\357\365\64\54\1\323"
  "\243\267\147\1\301\305\263\37\362\71\112\0\122\246\367\220\126\125\261\373\333\341\177\337\365\322\353\123\322\354\266\353"
  "\145\362\365\333\233\176\304\123\342\176\157\127\317\171\252\333\344\334\144\202\33\233\133\55\163\323\222\67\224\141\104\253"
  "\200\133\273\227\265\173\307\111\0\61\146\173\202\364\355\147\264\225\1\126\211\77\261\176\156\77\13\157\276\336\243\72"
  "\260\76\146\353\254\363\234\340\65\173\201\271\41\362\217\62\307\23\347\331\222\51\271\111\354\17\114\170\32\162\113\253"
  "\212\33\45\106\125\253\241\151\350\13\73\116\130\105\373\164\201\363\61\146\110\231\101\314\266\66\233\225\363\10\205\357"
  "\151\12\231\155\315\363\271\276\351\364\240\36\212\15\347\123\245\221\325\116\7\30\132\331\4\277\176\163\373\116\263\253"
  "\346\231\152\217\75\124\236\100\206\355\127\66\361\165\267\153\257\205\203\143\11\66\362\155\131\340\136\176\347\356\345\261"
  "\267\227\267\244\364\15\172\107\16\217\133\225\346\215\74\336\25\14\360\254\231\363\263\237\376\263\137\102\11\337\0\350"
  "\27\302\374\33\16\113\70\111\160\367\60\325\226\267\124\6\337\112\135\367\227\373\145\116\51\124\26\31\236\23\335\351"
  "\173\102\343\353\107\376\171\145\67\232\10\104\143\57\57\1\172\150\203\152\215\175\211\351\141\210\341\265\55\344\315\110"
  "\116\3\21\27\204\140\242\220\26\123\314\147\335\31\253\132\314\317\261\104\61\241\142\215\114\173\127\332\234\335\240\66"
  "\310\263\350\250\264\201\66\210\152\245\33\40\172\123\227\267\102\44\224\46\150\256\154\56\240\222\346\321\50\1\335\153"
  "\12\267\125\227\366\152\155\270\252\3\277\352\224\243\57\205\326\341\114\100\64\214\11\320\265\45\174\233\55\14\333\315"
  "\317\304\340\264\14\217\357\75\160\47\170\267\377\371\177";

/* lib/string.wo (DEFLATEd, org. size 23745) */
static unsigned char file_l_42[2033] =
  "\314\126\335\152\363\70\20\275\137\330\167\30\202\56\334\233\22\273\24\112\240\354\213\54\4\105\232\44\336\310\226\42\311"
  "\64\351\323\257\353\111\52\224\161\352\232\302\107\357\74\307\147\146\216\346\107\250\150\254\356\14\202\10\321\327\355\356"
  "\357\277\0\212\272\161\326\107\130\50\37\27\260\10\116\54\240\330\31\273\221\6\104\217\255\172\4\212\246\213\120\77"
  "\125\17\17\17\43\76\15\66\326\237\173\67\372\40\267\313\367\62\171\114\145\241\4\31\73\104\155\352\315\107\12\151"
  "\214\125\275\317\266\153\25\10\302\127\204\102\341\244\227\315\340\16\205\307\320\231\121\261\350\175\153\373\130\153\154\302"
  "\56\245\27\3\276\32\320\21\11\52\236\35\366\136\321\166\316\241\377\224\60\340\253\13\72\45\201\7\63\366\215\5"
  "\43\164\42\130\322\115\75\134\205\350\243\75\254\367\326\150\364\251\123\304\325\62\312\117\246\16\117\117\2\224\155\103"
  "\4\151\352\135\373\132\302\342\237\256\75\264\366\255\5\364\336\372\177\227\313\5\171\222\260\213\347\346\35\275\205\2"
  "\117\164\214\301\44\36\134\325\12\35\50\361\47\320\222\111\54\143\225\64\217\73\214\3\221\260\376\367\43\251\131\62"
  "\122\113\10\115\321\343\266\66\206\0\217\261\363\303\317\21\231\75\133\51\167\116\102\57\300\244\324\340\125\16\334\230"
  "\55\157\6\134\102\16\232\101\34\11\274\232\56\67\325\76\267\353\122\114\25\207\260\360\201\35\31\53\170\305\130\216"
  "\220\115\17\35\100\224\342\112\260\16\104\45\170\201\71\162\333\225\62\1\241\333\260\204\127\376\226\105\72\162\344\213"
  "\330\122\353\257\217\34\21\207\222\261\250\216\43\163\362\344\174\143\245\176\131\7\260\333\155\300\370\272\274\256\110\142"
  "\340\51\142\373\301\141\342\324\76\261\102\264\36\137\130\30\46\165\360\141\221\31\153\214\224\240\343\124\17\322\276\0"
  "\364\336\227\31\361\151\44\10\244\217\14\115\63\304\366\164\162\11\367\76\355\40\331\267\53\30\176\270\157\301\335\131"
  "\240\300\172\34\176\321\146\204\157\14\334\117\147\200\147\143\103\300\170\34\232\267\111\311\101\173\353\376\320\224\65\56"
  "\115\31\331\154\312\312\233\173\276\232\73\147\212\42\44\273\312\155\175\157\14\113\126\41\125\162\126\305\131\25\53\7"
  "\343\350\337\63\320\252\34\207\346\214\216\52\47\227\202\205\254\236\237\23\44\331\114\123\35\31\64\127\130\365\163\141"
  "\167\13\33\21\123\47\375\272\336\122\47\347\155\16\173\76\174\157\165\330\43\151\346\33\151\142\171\330\173\206\275\136"
  "\46\336\173\312\272\363\275\120\323\247\153\260\111\247\43\373\366\164\173\171\16\121\252\3\73\121\146\42\152\203\71\326"
  "\114\336\30\347\374\202\70\345\346\177\271\171\310\115\63\132\305\44\227\115\50\257\22\211\146\304\23\43\66\137\335\12"
  "\273\270\356\256\73\237\35\366\316\6\241\11\310\63\44\62\36\337\377\157\347\12\173\33\66\201\350\237\351\347\151\115"
  "\263\356\347\114\155\112\324\65\266\353\6\147\113\377\375\254\10\351\11\136\331\63\47\346\146\51\122\245\312\27\0\356"
  "\270\343\216\307\1\324\331\322\56\215\63\337\256\162\47\50\315\35\146\206\77\270\354\2\170\116\227\210\123\336\241\353"
  "\72\367\250\3\26\163\214\261\363\70\20\127\124\244\203\140\165\341\203\356\262\203\104\245\337\172\53\363\133\157\13\74"
  "\131\37\257\257\220\253\203\2\133\344\117\165\76\171\170\302\233\320\207\122\22\255\154\54\221\3\165\103\22\317\231\321"
  "\231\47\227\333\322\22\100\205\262\346\172\56\317\123\71\227\356\222\230\217\215\237\327\22\223\215\32\270\25\115\142\240"
  "\171\243\373\225\326\57\343\120\170\175\123\272\323\151\335\261\356\32\301\54\354\175\33\331\373\303\135\364\343\157\167\314"
  "\166\205\225\125\114\213\20\271\224\351\47\123\12\103\314\117\354\232\40\103\222\206\14\331\71\262\34\242\310\362\1\63"
  "\224\174\154\361\141\334\207\365\357\177\71\304\133\201\160\103\341\244\167\123\304\336\374\55\271\253\305\333\156\11\60\152"
  "\347\354\357\247\21\254\5\102\312\133\237\354\241\373\115\36\346\115\360\317\34\72\72\365\43\343\241\61\141\223\3\110"
  "\173\216\122\106\56\264\141\104\361\172\366\314\32\3\142\36\247\76\13\166\12\220\364\103\367\126\6\136\146\360\131\26"
  "\42\6\255\233\256\17\352\142\115\255\17\115\33\161\56\155\224\363\277\335\223\167\21\264\5\232\206\267\354\170\26\116"
  "\52\256\37\337\342\371\132\23\234\72\331\301\51\234\27\40\274\115\116\377\126\2\262\230\11\61\42\201\140\231\267\363"
  "\20\206\222\37\357\367\327\204\316\140\236\123\142\232\123\101\254\243\2\0\262\15\314\112\64\210\327\143\66\14\342\302"
  "\43\5\231\105\301\2\372\214\31\227\207\72\31\246\13\274\260\257\176\132\202\315\265\261\63\50\242\1\157\242\356\210"
  "\122\164\344\142\366\104\271\256\225\16\220\137\152\76\351\346\174\322\167\271\236\346\141\260\136\107\106\126\160\70\123\220"
  "\242\102\126\65\256\235\242\262\211\154\350\76\306\203\36\130\174\334\107\313\52\41\124\107\247\230\320\122\107\316\21\32"
  "\117\372\14\115\267\307\21\176\34\240\337\201\120\301\213\370\104\275\227\73\11\137\30\224\54\77\256\336\63\0\252\235"
  "\21\272\377\172\122\313\303\24\114\351\226\120\127\73\167\366\61\361\220\30\370\104\333\114\51\165\101\34\47\11\104\324"
  "\306\37\131\16\311\225\111\5\214\240\202\55\342\323\343\141\77\54\255\371\345\24\107\204\363\67\331\262\64\335\156\141"
  "\174\367\365\376\246\163\203\226\143\264\260\166\150\50\311\45\246\242\31\120\134\355\45\226\204\373\35\143\324\230\107\133"
  "\350\320\271\170\151\235\277\313\147\303\373\345\251\165\245\113\247\57\135\72\271\62\223\112\54\250\346\236\311\127\261\240"
  "\201\1\77\20\327\117\150\333\275\266\4\267\377\61\206\170\32\307\153\301\20\61\242\206\41\222\311\117\261\271\27\240"
  "\210\53\334\112\220\247\160\166\250\262\135\150\370\211\27\32\30\61\145\165\252\166\135\101\35\123\222\342\112\126\355\233"
  "\335\201\34\373\365\73\365\346\324\33\10\333\134\164\212\302\16\165\163\344\311\64\205\313\276\272\213\203\260\252\232\26"
  "\273\106\346\106\363\301\25\315\351\303\176\225\327\344\117\123\274\151\310\2\116\5\66\350\143\0\212\361\50\141\203\32"
  "\240\242\311\365\35\313\111\122\120\55\276\243\341\135\347\166\323\12\40\230\341\334\273\42\64\46\232\31\230\137\266\165"
  "\123\344\147\160\37\21\22\27\10\102\117\373\247\363\134\350\33\341\271\60\2\226\222\276\26\144\104\372\164\122\374\312"
  "\150\40\13\103\313\110\370\65\124\272\72\350\161\174\76\36\240\245\201\120\43\241\3\104\271\161\260\35\100\331\327\243"
  "\333\76\353\153\147\175\355\254\317\310\22\31\17\346\120\130\30\217\32\343\43\21\350\125\211\62\15\15\251\206\274\46"
  "\355\337\117\103\10\344\24\33\241\160\365\124\305\274\57\133\365\121\210\74\267\327\237\250\210\261\152\75\112\363\114\132"
  "\232\111\163\75\67\340\172\304\72\375\3\163\120\374\24\373\213\371\117\277\367\200\7\36\164\152\76\352\213\37\304\316"
  "\237\36\150\40\262\150\46\275\16\75\377\55\24\221\33\43\201\204\237\306\130\36\57\256\373\263\57\135\1\235\371\20"
  "\61\214\41\126\372\124\343\267\14\235\374\227\131\370\101\10\20\167\272\137\242\341\70\133\206\220\143\112\241\45\272\132"
  "\20\7\311\206\346\107\137\365\163\111\77\74\215\305\127\16\360\266\141\144\336\27\212\210\7\241\311\40\205\7\22\225"
  "\166\113\164\257\160\242\103\277\213\240\147\257\205\210\271\262\47\122\143\104\132\361\261\174\220\43\40\307\146\220\226\0"
  "\221\54\61\210\0\224\314\31\316\5\12\233\252\53\53\253\141\331\175\36\223\205\165\37\23\110\165\57\357\204\206\261"
  "\207\47\126\151\22\347\106\263\205\151\71\172\114\175\63\352\171\324\23\15\302\6\56\34\224\330\5\232\374\372\361\123"
  "\203\16\2\73\227\206\147\267\67\260\251\215\255\31\33\144\246\343\2\350\137\251\116\200\102\66\100\272\254\257\64\274"
  "\167\35\154\76\20\254\173\115\35\42\157\62\231\126\375\270\154\270\347\375\261\307\160\3\201\316\266\135\245\27\73\134"
  "\321\223\35\304\325\300\327\127\271\245\174\314\254\245\161\171\367\30\342\10\224\44\300\10\257\104\153\206\103\301\177\261"
  "\307\156\372\303\33\223\51\104\47\277\77\106\47\65\76\163\210\162\164\373\137\370\135\350\154\270\214\12\321\123\331\313"
  "\206\264\5\241\77\321\173\103\332\274\243\311\233\377\376\1";

/* lib/time.wo (DEFLATEd, org. size 46152) */
static unsigned char file_l_43[4434] =
  "\354\132\347\262\243\74\22\375\275\351\35\124\56\373\313\343\102\10\60\366\346\234\163\216\24\66\302\103\55\351\202\270\351"
  "\351\27\220\55\33\132\255\145\264\71\114\24\115\237\116\352\76\142\140\76\50\252\244\313\71\131\213\254\340\237\373\64"
  "\41\37\144\105\135\65\202\254\116\215\130\221\125\133\257\127\344\203\163\136\35\343\234\254\173\331\241\227\220\17\212\116"
  "\220\214\271\37\176\370\241\6\123\360\242\152\136\172\230\134\110\330\145\355\0\4\352\105\72\230\150\77\305\155\26\265"
  "\145\134\267\157\53\21\325\15\177\314\370\23\355\15\234\362\352\364\347\150\310\41\72\163\321\333\112\273\362\104\326\172"
  "\375\303\124\233\174\120\307\115\134\214\356\324\72\360\76\234\312\33\336\166\271\66\351\126\44\171\166\354\203\350\155\361"
  "\362\121\71\227\362\203\224\56\67\326\144\345\171\65\56\116\365\213\62\46\345\7\51\235\30\263\60\134\352\55\227\6"
  "\323\126\361\27\65\364\62\110\377\126\303\71\57\241\341\101\272\324\30\157\232\262\132\251\277\125\347\215\327\207\361\317"
  "\1\6\67\72\33\120\155\131\367\76\105\252\142\30\157\34\256\142\220\235\105\71\145\100\162\52\17\221\170\155\271\220"
  "\103\47\1\375\237\333\123\125\266\202\70\72\300\360\307\153\125\162\362\1\177\226\241\137\45\253\17\315\146\222\130\304"
  "\27\43\111\374\322\106\131\31\25\125\51\336\222\307\270\41\161\236\235\313\57\172\144\365\7\232\376\301\161\56\277\50"
  "\127\353\145\362\277\27\66\121\353\25\10\276\17\72\32\22\300\343\376\274\132\375\116\255\236\157\326\367\273\333\372\350"
  "\337\326\211\167\133\247\354\56\32\372\7\207\312\265\243\126\77\122\253\102\255\140\254\117\175\240\121\31\27\174\22\154"
  "\233\275\362\57\272\341\240\114\206\16\111\267\367\173\323\262\265\274\103\6\351\266\356\104\324\253\220\52\115\133\56\276"
  "\350\134\356\1\224\147\102\171\30\312\67\241\102\14\25\230\120\324\305\140\73\43\54\300\140\241\11\346\242\5\331\33"
  "\141\36\234\212\241\362\104\316\214\334\51\112\126\77\357\112\270\261\262\336\100\367\207\25\246\353\3\335\137\164\34\321"
  "\15\200\356\257\171\202\350\356\240\335\267\35\242\33\2\335\157\65\31\242\273\207\165\210\5\336\340\151\227\347\357\334"
  "\345\224\132\265\71\165\255\372\234\62\253\106\247\236\135\247\123\337\256\325\151\140\327\353\164\367\356\315\116\251\256\333"
  "\373\375\104\232\202\272\272\216\67\350\63\135\327\267\6\200\247\153\375\322\10\361\165\23\320\230\20\201\156\16\14\372"
  "\73\335\54\164\15\104\250\323\11\31\5\17\37\205\275\325\50\270\216\325\50\270\324\152\24\134\327\156\24\134\146\67"
  "\12\256\147\67\12\256\157\304\341\145\61\217\36\136\27\343\350\61\274\56\241\21\207\327\305\330\54\36\132\27\146\356"
  "\26\75\105\300\203\340\173\61\166\310\271\16\234\54\176\304\224\51\44\223\270\301\224\41\363\174\265\106\225\31\264\214"
  "\116\267\353\301\4\73\64\101\137\243\234\143\312\1\214\271\73\143\312\32\222\341\65\246\34\2\345\37\237\4\246\14"
  "\167\360\107\325\43\242\314\340\16\176\203\237\60\232\63\35\373\70\327\61\327\212\353\30\263\342\72\346\131\161\35\363"
  "\355\270\216\5\166\134\307\166\166\134\307\102\73\256\143\173\73\256\363\34\73\256\363\250\35\327\171\256\35\327\171\354"
  "\335\271\216\271\72\256\353\342\6\343\16\306\164\174\327\230\20\236\216\364\116\157\61\165\137\107\173\131\216\251\7\357"
  "\100\174\154\247\43\76\216\151\207\72\346\103\155\357\165\324\327\265\30\107\171\216\216\375\170\161\344\15\206\240\72\12"
  "\254\14\0\127\107\203\106\27\114\307\205\70\42\156\117\303\337\321\261\113\357\350\220\112\72\164\3\240\177\56\200\272"
  "\142\117\6\325\43\361\72\60\55\64\355\123\127\247\374\30\347\232\60\174\255\341\44\315\305\104\171\105\10\321\46\171"
  "\11\102\275\172\222\327\253\17\141\12\132\376\127\336\226\237\0\313\201\332\241\366\141\53\376\342\167\310\216\7\240\15"
  "\307\114\250\116\25\66\307\246\45\233\166\303\22\262\331\272\311\101\375\101\66\311\37\112\304\341\16\66\345\227\61\125"
  "\266\134\325\203\261\305\144\163\44\233\76\230\147\262\371\55\206\323\260\315\17\61\135\110\65\77\101\165\167\60\236\144"
  "\273\51\266\170\44\41\104\174\347\260\371\341\141\363\163\14\261\207\10\245\52\137\344\252\267\247\263\67\247\122\207\134"
  "\137\332\256\153\61\171\161\53\277\26\134\124\362\352\164\171\223\134\65\243\226\22\35\353\365\124\220\316\5\31\235\13"
  "\334\271\200\315\5\336\105\40\335\313\327\300\333\63\27\352\63\212\274\61\252\157\333\101\176\224\62\203\362\355\275\60"
  "\15\156\222\266\73\116\160\355\4\267\300\273\340\174\110\32\312\62\12\34\73\27\101\340\155\371\263\340\145\22\365\367"
  "\242\366\46\155\105\325\160\105\10\227\75\15\161\63\327\204\234\353\17\25\305\65\142\25\131\57\311\27\176\73\2\251"
  "\214\373\76\217\42\360\175\346\337\204\161\231\334\56\370\303\353\345\42\5\21\151\153\345\256\201\236\222\15\171\346\125"
  "\234\300\272\340\145\120\167\222\354\61\352\226\25\230\347\55\137\26\55\133\317\135\277\241\13\175\224\11\160\121\13\270"
  "\301\112\120\162\130\110\205\203\221\171\153\64\3\274\216\366\241\57\265\255\40\160\120\341\300\65\134\164\115\71\254\1"
  "\213\215\175\172\243\261\361\122\361\330\337\237\267\160\22\372\77\11\271\140\372\376\55\270\307\74\102\140\152\255\333\363"
  "\337\147\46\222\54\115\247\247\273\224\300\23\136\120\71\34\352\332\31\257\325\344\244\303\5\10\116\120\50\272\353\41"
  "\325\273\251\54\353\43\157\104\324\337\220\35\145\214\274\345\247\66\22\125\44\212\131\234\323\60\153\1\77\51\317\47"
  "\174\370\54\72\242\224\144\60\76\225\274\360\270\231\211\32\136\110\44\163\47\302\1\14\204\22\77\221\76\234\242\323"
  "\313\51\347\63\261\136\372\240\225\216\137\242\147\262\341\333\322\124\362\2\44\71\217\353\277\337\203\226\22\370\163\101"
  "\60\27\354\346\202\360\42\200\155\2\346\55\330\355\235\320\17\203\75\165\174\352\273\367\7\165\56\24\7\245\223\235"
  "\6\304\211\237\326\320\343\325\141\270\13\303\275\33\336\73\74\53\207\134\321\107\252\234\51\206\100\207\336\354\170\357"
  "\323\336\153\350\350\46\105\261\233\354\322\271\317\60\360\34\23\111\253\347\232\26\34\100\103\63\203\330\254\175\64\274"
  "\210\356\220\117\115\134\17\223\15\222\270\14\14\116\301\223\315\5\361\1\70\14\161\340\370\44\1\351\112\44\314\130"
  "\225\1\264\15\310\26\154\17\122\117\16\217\30\206\133\204\321\50\325\70\1\206\166\177\237\215\220\274\141\267\13\172"
  "\354\16\57\277\202\200\221\60\156\202\27\70\373\335\262\336\6\351\2\377\212\175\55\174\377\35\172\136\71\261\353\171"
  "\143\214\346\246\327\216\271\252\6\60\12\37\64\361\52\302\255\304\243\145\201\357\172\112\50\267\15\24\12\213\352\16"
  "\310\37\320\72\131\346\264\70\45\263\103\74\327\242\313\261\10\144\332\370\6\323\277\126\261\7\44\4\327\137\124\262"
  "\7\313\66\260\52\31\100\202\134\355\53\246\12\157\56\230\172\66\263\152\61\24\155\256\227\202\131\324\113\41\361\114"
  "\315\345\152\27\133\127\57\143\360\7\53\270\221\366\217\141\140\210\140\14\252\120\12\10\331\123\351\250\164\345\343\356"
  "\142\136\242\23\6\5\163\24\242\4\173\126\256\60\15\301\271\174\32\307\166\317\154\111\135\234\71\176\72\330\333\67"
  "\267\314\213\351\274\306\247\10\355\23\320\255\150\311\251\343\340\115\202\133\61\237\325\327\370\200\155\160\246\341\46\244"
  "\113\171\241\75\155\357\112\162\34\137\76\255\135\370\106\5\240\344\77\352\256\342\252\46\153\52\121\362\353\16\362\377"
  "\221\101\32\243\24\333\20\163\335\165\57\36\74\174\200\24\214\107\272\251\75\66\121\226\252\324\257\225\370\113\173\347"
  "\275\50\73\152\203\361\227\231\77\123\14\256\363\62\333\157\157\247\246\75\175\346\372\344\52\61\277\145\77\30\341\170"
  "\122\266\133\73\126\343\103\200\100\270\77\145\31\36\157\153\121\364\264\336\0\143\320\212\112\123\304\234\74\42\176\171"
  "\370\162\367\315\257\206\214\125\213\355\103\74\225\312\356\104\140\21\257\307\274\263\62\176\110\272\206\136\146\351\116\206"
  "\310\44\4\165\235\26\305\330\37\206\171\130\372\151\220\13\201\67\376\174\104\123\315\177\157\252\57\352\355\217\355\62"
  "\33\153\366\15\116\344\262\234\111\347\274\47\270\256\242\157\330\311\177\65\53\75\224\250\33\46\241\156\161\77\213\355"
  "\165\213\102\267\174\214\5\253\346\312\305\101\50\67\234\104\32\301\41\173\21\262\307\23\61\326\110\266\22\75\235\112"
  "\163\130\375\324\165\130\275\370\25\124\355\62\27\53\70\121\275\374\17\56\74\256\323\237\32\56\102\103\207\2\230\235"
  "\211\175\10\73\221\364\360\317\35\224\157\224\314\31\211\242\255\11\356\43\322\63\172\243\210\276\264\35\65\356\242\224"
  "\45\243\340\341\220\156\76\155\152\6\311\365\332\161\243\217\172\334\240\260\172\270\351\6\177\365\164\71\126\265\236\340"
  "\335\266\357\343\303\317\333\6\376\345\325\343\223\154\341\247\277\135\252\64\113\367\142\364\342\141\145\7\273\56\252\155"
  "\147\274\313\367\217\251\67\102\312\174\134\162\333\240\66\211\137\175\156\313\224\170\162\10\126\333\257\234\106\271\345\14"
  "\3\5\351\314\302\136\312\364\267\244\314\170\113\312\54\134\240\354\44\152\234\271\232\125\226\163\305\353\167\225\165\144"
  "\144\161\107\204\345\142\171\251\210\65\76\145\210\305\313\123\360\101\74\146\233\220\126\56\256\256\145\55\364\57\370\201"
  "\162\270\126\305\336\261\325\170\74\231\323\261\22\347\32\275\114\203\116\347\241\255\11\304\11\340\165\340\117\56\116\110"
  "\223\12\6\45\162\347\312\7\272\112\257\323\170\340\100\17\275\57\114\305\340\273\36\375\221\243\357\45\57\224\14\265"
  "\376\23\252\203\74\70\21\257\77\70\161\213\347\311\50\175\365\223\43\44\53\374\343\107\324\170\163\355\3\124\131\321"
  "\321\160\16\51\217\177\212\103\226\265\213\264\276\210\313\42\270\104\377\224\331\230\350\144\152\71\313\336\313\22\370\6"
  "\76\55\41\334\142\252\244\1\372\173\255\201\163\60\44\2\375\63\121\132\344\326\25\343\143\365\366\310\73\354\214\44"
  "\350\301\312\221\123\246\262\35\212\3\42\30\361\301\226\375\157\237\111\313\123\377\154\146\275\333\303\4\340\201\223\123"
  "\266\13\206\337\342\200\351\26\372\177\60\114\234\17\165\355\305\212\21\215\57\36\202\115\260\320\100\375\77\120\313\43"
  "\272\275\45\377\303\114\261\367\364\230\302\205\150\263\241\143\134\166\237\24\230\256\271\245\64\67\334\331\31\313\5\277"
  "\253\332\105\107\40\140\137\36\162\263\317\361\154\24\357\316\260\120\141\51\127\301\217\136\102\324\253\345\324\271\274\315"
  "\220\313\15\265\162\1\235\26\300\377\343\137\202\316\247\352\376\257\302\260\110\272\264\254\47\172\271\227\321\266\302\326"
  "\107\333\7\263\35\217\257\145\366\377\57\4\316\347\114\170\337\340\171\123\14\267\271\107\25\20\132\375\133\76\140\120"
  "\332\324\101\67\143\152\100\332\330\264\271\226\23\13\254\352\130\207\44\246\111\67\244\46\72\41\263\371\351\116\357\52"
  "\61\376\225\61\227\211\371\127\337\77\3\16\360\220\160\337\153\277\47\230\343\115\314\6\7\23\311\73\64\260\165\214"
  "\175\120\100\120\224\164\62\245\233\107\135\175\310\316\373\320\354\101\362\117\144\320\105\220\25\242\312\223\173\321\257\210"
  "\365\245\312\106\6\254\47\350\252\101\300\306\53\154\201\376\124\244\334\376\55\60\234\334\136\22\212\212\51\277\153\20"
  "\376\364\141\133\322\273\76\263\240\367\223\274\262\343\211\125\253\247\247\265\346\25\325\244\11\351\371\351\347\357\337\76"
  "\74\362\167\337\177\145\371\102\6\230\326\360\107\320\176\372\365\316\36\363\331\336\63\16\2\242\371\161\52\50\73\130"
  "\2\327\146\106\3\125\347\356\267\353\132\136\132\0\200\23\326\341\124\26\15\340\211\134\212\52\257\111\323\212\232\165"
  "\116\105\351\177\316\55\312\32\45\114\131\374\44\153\45\117\266\312\114\320\53\123\232\215\212\27\72\335\217\216\343\235"
  "\316\76\347\136\41\173\32\335\344\323\371\315\332\236\35\236\336\347\0\341\166\260\116\102\330\250\307\233\343\235\72\320"
  "\370\246\331\123\335\114\210\255\165\106\305\122\243\240\266\226\131\134\322\107\174\121\136\46\256\10\203\125\226\306\370\330"
  "\340\16\116\306\77\137\211\17\73\300\17\351\232\14\42\224\35\72\55\67\171\271\23\202\366\72\27\170\314\5\211\102"
  "\265\374\44\317\156\116\114\16\36\33\211\47\217\77\155\17\307\134\356\117\164\34\75\276\274\215\61\147\312\345\22\372"
  "\212\214\112\304\366\2\226\150\331\131\61\363\301\1\2\346\312\15\13\54\12\371\205\212\374\21\142\114\2\1\304\126"
  "\161\17\366\204\111\33\304\40\137\156\17\205\354\153\117\37\205\75\245\52\107\41\150\130\332\10\132\204\234\151\150\43"
  "\147\20\162\226\256\215\34\325\100\147\15\70\256\317\50\133\57\236\344\42\73\204\130\245\112\66\277\221\174\302\250\210"
  "\31\353\216\262\261\312\22\162\214\255\62\364\257\77\117\203\277\21\375\205\47\256\214\374\177\301\235\167\204\246\377\16"
  "\104\235\351\150\175\271\245\236\177\250\333\214\264\111\124\136\125\34\131\275\124\333\224\27\346\102\331\231\120\375\104\310"
  "\272\213\356\33\373\40\177\130\366\330\151\142\143\211\204\75\202\17\300\152\355\355\252\345\131\271\60\54\102\23\233\356"
  "\326\256\376\234\160\63\20\247\45\177\4\133\26\133\360\273\115\310\170\323\271\350\102\172\215\200\25\202\51\52\7\4"
  "\66\14\125\265\206\20\215\124\72\252\161\114\333\337\263\4\267\324\67\161\155\306\261\345\321\145\7\365\331\67\271\61"
  "\353\254\320\164\140\101\24\172\374\371\325\253\17\311\216\305\335\66\170\77\76\375\370\360\364\375\247\365\153\132\322\317"
  "\137\331\345\256\232\274\313\314\74\71\215\57\274\333\316\124\67\51\133\155\315\371\132\25\265\170\324\365\101\246\121\362"
  "\46\347\123\60\2\31\126\160\364\252\37\250\204\326\310\20\331\136\265\142\341\332\277\63\63\355\342\112\333\247\207\344"
  "\62\336\27\12\343\312\143\322\21\76\375\370\227\55\345\365\47\233\350\130\337\321\221\350\41\271\211\366\313\363\347\247"
  "\55\351\202\366\213\116\240\131\11\63\172\233\121\336\176\171\176\10\161\113\303\153\172\325\161\364\75\265\106\70\103\261"
  "\16\224\0\112\4\245\7\145\0\145\4\145\2\145\6\145\1\5\72\307\16\24\372\67\202\322\203\62\200\62\202\62"
  "\201\62\203\262\200\2\235\373\16\224\0\12\101\321\203\62\200\62\202\62\201\62\203\262\200\2\235\207\16\224\175\57"
  "\210\137\272\177\113\101\37\176\165\211\41\372\44\351\32\127\162\153\142\124\3\131\251\217\250\316\131\203\136\375\301\346"
  "\176\276\256\74\347\45\22\312\72\257\157\341\161\243\373\260\65\45\316\311\363\224\74\217\311\163\372\176\237\74\307\344"
  "\71\44\317\335\366\71\234\223\347\45\171\236\223\347\51\171\36\223\347\41\171\356\223\347\230\74\207\344\71\321\57\121"
  "\57\321\56\121\56\321\155\24\60\301\54\204\273\166\25\300\262\206\147\206\320\300\306\74\42\0\150\5\151\23\70\114"
  "\143\25\207\211\14\246\52\6\13\30\314\261\212\101\10\344\320\327\161\210\344\60\327\161\30\31\40\53\165\230\311\141"
  "\254\343\260\220\103\245\25\147\162\130\352\360\24\310\341\134\307\141\0\207\163\127\307\141\44\207\72\77\214\144\120\347"
  "\206\231\14\352\274\300\206\10\135\235\27\102\107\26\335\124\307\242\47\213\256\316\216\60\220\105\250\354\336\23\131\204"
  "\112\114\165\144\21\53\101\25\311\42\206\72\26\66\110\75\330\210\374\217\42\40\2\256\317\216\362\366\322\164\152\272"
  "\270\167\174\342\302\245\301\124\122\67\304\155\377\71\236\374\63\46\326\4\144\77\241\357\266\324\277\153\115\113\315\127"
  "\206\253\341\137\141\65\137\3\253\245\34\126\141\72\20\126\102\203\20\256\304\125\277\43\256\370\211\146\267\245\7\300"
  "\352\134\166\65\110\227\363\360\220\101\330\146\265\146\271\252\144\53\233\352\330\110\247\67\72\271\357\245\375\257\117\347"
  "\341\222\112\64\201\270\260\110\372\62\350\313\132\366\261\14\307\330\133\133\26\205\145\303\325\127\100\21\47\241\20\47"
  "\243\307\233\313\15\303\144\332\335\260\203\120\62\67\104\111\74\65\160\111\114\134\2\225\137\222\325\145\133\55\140\216"
  "\270\155\334\54\156\27\300\174\321\150\240\236\67\203\345\263\137\373\3\1\33\272\206\210\355\13\343\132\10\236\376\37"
  "\363\150\327\37\327\70\30\54\41\36\143\71\201\46\372\321\1\110\354\217\161\315\356\135\260\127\206\267\234\132\14\245"
  "\135\320\65\267\10\223\337\333\107\365\300\351\30\303\217\217\364\55\347\46\143\51\314\26\217\267\207\33\236\302\206\363"
  "\76\226\35\17\224\330\162\112\60\271\46\261\345\345\174\176\51\241\367\336\67\315\4\303\230\113\341\244\314\371\352\164"
  "\272\62\51\22\346\322\54\204\153\26\326\335\160\337\214\161\167\313\216\352\233\175\303\276\271\154\236\316\231\236\12\141"
  "\154\12\377\321\204\145\344\202\363\361\325\307\127\77\363\360\204\35\230\324\107\21\263\310\37\64\76\214\337\315\201\173"
  "\164\50\177\70\176\247\166\370\215\305\351\326\331\265\262\30\156\71\322\55\173\231\166\74\124\316\15\241\22\234\73\5"
  "\163\333\235\202\30\235\372\54\215\365\51\315\334\364\235\13\157\30\132\17\310\120\70\66\67\302\361\306\37\326\35\373"
  "\330\260\73\16\245\160\353\367\361\70\257\115\142\223\334\0\336\206\343\254\327\210\74\76\143\330\217\307\371\347\370\150"
  "\325\53\357\114\107\171\147\377\140\65\50\333\347\206\301\152\74\351\233\375\264\103\373\130\227\336\330\373\124\5\355\234"
  "\116\271\331\306\371\124\311\152\56\215\357\113\346\334\360\376\261\365\354\76\124\277\347\141\203\206\51\70\216\265\326\170"
  "\305\173\276\264\106\47\246\50\317\377\165\104\152\144\147\235\256\76\370\105\105\305\125\270\74\27\306\323\153\160\21\76"
  "\125\151\305\41\11\325\176\231\273\237\151\55\201\333\204\214\147\270\6\77\205\130\165\65\333\157\326\303\76\344\34\227"
  "\261\205\316\374\214\273\172\245\10\372\205\5\71\76\27\263\154\307\74\340\50\253\170\264\67\14\74\132\113\266\363\307"
  "\127\150\147\370\13\64\251\44\35\210\120\24\166\14\224\355\375\313\157\47\150\336\277\47\163\127\234\262\243\326\342\163"
  "\13\2\6\262\273\123\15\215\143\12\364\136\27\161\371\353\357";

/* lib/unistd.wo (DEFLATEd, org. size 25993) */
static unsigned char file_l_44[1892] =
  "\314\127\327\162\344\40\20\174\276\364\17\224\312\71\347\354\157\121\261\142\130\123\207\100\7\43\247\257\77\214\164\50\260"
  "\127\302\71\233\35\365\164\67\43\342\132\251\131\55\201\54\325\112\130\144\277\276\23\262\46\312\112\33\44\131\141\60"
  "\43\231\255\226\62\262\66\227\172\106\45\131\162\261\13\27\41\153\145\215\104\34\36\254\257\257\57\310\51\241\324\346"
  "\301\245\65\215\46\255\155\357\371\214\64\25\47\60\102\337\121\53\162\253\150\145\157\64\346\225\201\133\1\167\373\216"
  "\200\263\334\0\145\216\204\327\252\40\113\213\201\27\55\214\254\125\324\320\322\13\244\265\15\330\132\206\56\247\132\272"
  "\63\2\41\301\223\307\175\224\251\102\152\233\142\312\343\336\102\220\121\244\366\101\25\323\232\1\372\26\262\234\131\244"
  "\230\317\1\23\204\73\360\133\227\233\13\11\236\333\202\373\25\217\51\245\217\162\26\232\72\71\172\261\251\52\161\262"
  "\124\111\263\45\62\365\372\252\125\111\123\47\0\77\303\242\5\370\75\155\320\303\322\154\274\336\122\342\54\173\223\31"
  "\126\121\274\361\3\65\165\226\105\11\317\135\360\336\306\262\24\152\362\265\5\140\202\225\17\264\156\200\262\124\373\1"
  "\374\25\154\227\372\26\162\46\14\24\250\315\103\212\375\50\351\175\55\332\207\62\261\260\1\373\71\265\254\325\223\266"
  "\237\106\123\146\307\370\367\63\146\164\221\303\275\300\51\107\1\30\44\143\25\213\106\250\171\346\33\22\124\240\154\342"
  "\27\115\64\325\62\30\243\164\26\376\207\3\246\377\174\341\377\56\70\144\362\102\241\164\131\134\50\226\33\220\117\205"
  "\14\76\374\303\213\376\243\364\132\266\34\355\141\373\242\331\133\341\276\121\365\73\262\107\221\226\305\211\261\1\353\322"
  "\254\346\303\200\232\75\40\304\152\244\245\221\272\150\273\353\303\41\60\253\226\206\1\76\16\210\375\161\340\140\251\117"
  "\335\124\162\147\16\30\216\353\315\3\17\337\261\200\136\145\2\354\10\167\12\255\54\222\375\223\56\142\353\331\40\317"
  "\106\171\23\352\10\340\273\24\305\304\176\57\346\363\175\115\173\322\250\15\20\315\271\323\274\336\43\124\212\271\272\76"
  "\212\162\170\334\201\243\56\100\31\213\225\17\142\145\377\362\236\257\315\122\354\354\247\200\316\26\170\166\170\71\165\175"
  "\212\172\7\306\214\271\117\216\217\17\217\173\374\212\215\21\173\135\100\101\333\346\221\355\27\121\237\366\106\23\374\371"
  "\307\75\230\42\377\251\5\110\13\257\361\0\212\105\143\267\277\334\214\171\266\243\27\25\115\233\170\370\33\300\332\250"
  "\221\340\337\166\256\160\71\131\40\6\276\121\247\125\112\351\323\70\124\120\235\42\70\200\343\127\237\376\3\257\215\345"
  "\326\114\232\236\364\254\275\231\376\41\122\10\311\146\67\71\0\56\325\105\225\146\37\250\112\10\125\56\47\75\103\145"
  "\246\11\47\56\73\156\6\62\13\144\166\345\144\166\304\151\140\263\241\17\201\315\266\303\316\154\353\330\232\221\311\370"
  "\155\46\377\337\315\160\121\162\101\206\153\114\226\221\123\114\274\270\112\213\243\273\374\137\333\41\146\326\375\66\153\116"
  "\326\242\245\255\205\375\317\223\304\67\240\3\231\63\151\36\211\343\267\267\325\261\352\343\35\153\202\6\313\341\20\3"
  "\360\323\250\14\354\106\360\207\70\201\257\161\64\340\365\111\364\16\261\204\176\377\250\136\62\120\14\65\304\160\12\54"
  "\0\113\316\30\60\226\311\63\163\375\117\367\116\107\4\1\326\343\353\17\312\266\65\205\230\355\337\50\334\101\270\203"
  "\160\7\341\276\235\351\54\50\167\34\5\351\146\1\26\244\273\70\336\45\47\345\76\156\362\302\15\242\114\266\375\52"
  "\57\347\366\32\142\34\71\10\365\365\257\20\242\14\43\315\110\224\155\342\6\232\373\310\240\36\200\42\261\122\237\317"
  "\233\140\162\340\12\257\13\155\100\230\343\221\1\220\75\365\23\311\245\311\140\321\326\335\146\372\271\225\47\223\104\12"
  "\105\136\56\333\225\330\251\67\7\263\13\20\2\134\213\71\40\224\133\163\340\366\364\337\364\312\51\105\123\163\220\12"
  "\231\171\62\355\72\253\332\75\142\350\236\200\132\363\270\44\41\266\337\344\321\372\205\31\222\311\232\234\242\356\314\104"
  "\106\41\55\353\46\155\333\267\123\136\314\366\67\22\343\271\143\230\116\374\164\14\264\233\142\362\0\360\325\337\3\126"
  "\255\2\315\375\117\164\302\311\154\147\353\337\3\260\66\136\234\320\106\74\140\27\41\353\157\302\315\132\323\330\151\326"
  "\42\247\51\40\50\114\217\317\36\122\3\41\273\150\327\321\274\324\237\46\220\156\13\6\220\165\71\27\131\242\314\367"
  "\103\132\250\212\214\341\363\356\160\174\131\164\273\120\112\20\73\170\34\75\263\73\55\6\300\36\131\135\155\355\303\354"
  "\312\72\117\347\253\364\245\260\0\205\336\223\53\207\341\171\314\113\43\167\324\150\160\43\324\252\0\320\10\147\143\216"
  "\123\317\166\100\224\135\116\45\100\323\116\344\362\262\256\366\300\203\35\30\120\352\60\23\30\345\150\234\106\20\375\301"
  "\260\312\355\172\72\237\347\115\47\253\247\352\41\223\324\256\367\317\117\16\55\351\246\312\162\313\324\56\212\164\51\226"
  "\136\121\125\257\273\355\254\337\267\31\324\140\37\251\312\222\357\315\272\244\11\300\213\236\77\377\304\12\200\211\46\240"
  "\43\321\66\175\16\362\145\222\207\0\325\272\200\24\344\375\326\0\106\200\262\61\104\243\324\146\11\373\233\62\141\214"
  "\324\231\15\237\220\326\265\161\374\73\32\347\346\210\152\314\111\242\272\376\25\122\254\53\371\32\265\153\377\161\344\270"
  "\366\37\322\246\354\174\251\166\217\342\360\265\214\107\256\301\243\223\31\267\110\203\247\111\144\33\47\252\341\344\41\346"
  "\246\223\51\366\33\115\136\344\363\366\164\302\252\146\102\242\251\206\311\70\261\211\43\205\237\170\70\305\170\105\353\233"
  "\70\115\261\76\226\71\53\122\236\320\317\227\301\345\106\47\323\355\235\332\77\263\115\275\237\320\353\111\175\135\266\256"
  "\27\331\137\270\135\203\52\217\272\215\257\37\1\325\367\361\372\346\230\361\24\173\227\126\162\137\340\72\356\251\6\131"
  "\243\51\230\60\300\134\272\56\314\313\167\237\7\43\143\321\317\105\362\20\204\243\16\323\212\217\241\141\224\60\247\226"
  "\225\176\320\275\31\12\265\105\355\10\215\44\76\274\305\27\60\31\107\331\112\360\337\263\171\132\266\247\322\261\13\207"
  "\227\224\240\37\101\77\334\364\103\124\12\102\345\345\205\302\226\211\363\42\121\25\31\1\371\144\102\251\50\363\375\42"
  "\3\23\355\67\312\272\32\104\117\136\20\221\245\311\165\204\200\323\302\252\15\31\252\132\166\230\133\102\325\255\346\120"
  "\322\170\73\57\71\260\346\14\347\246\134\363\166\107\105\353\301\11\244\361\227\265\152\250\124\250\123\174\231\12\372\105"
  "\167\220\120\302\372\377\276\175\141\303\302\160\323\66\212\45\242\201\7\42\226\220\376\341\155\235\233\224\134\265\233\24"
  "\57\7\25\204\113\126\36\53\322\50\252\273\236\342\142\207\257\226\204\271\257\374\266\261\345\235\114\66\117\264\313\26"
  "\171\2\36\161\53\137\373\235\34\106\101\72\21\143\127\110\20\0\203\374\143\354\356\372\363\36\275\40\101\10\62\200"
  "\30\13\60\0\124\30\244\0\251\141\226\102\232\160\234\261\50\172\110\217\356\274\213\137\127\262\211\227\154\252\5\70"
  "\172\117\20\14\375\163\51\12\36\276\255\262\31\155\135\15\136\246\103\233\11\275\313\115\156\3\5\222\221\160\247\124"
  "\133\313\143\325\53\124\53\210\230\276\66\203\244\5\75\163\323\55\5\51\121\104\1\332\43\210\334\246\273\244\201\302"
  "\31\203\54\157\272\1\342\52\357\370\204\326\237\40\140\1\0\31\63\264\367\201\13\107\351\341\11\202\227\47\267\5"
  "\175\15\236\320\115\46\44\70\167\72\343\271\10\276\117\357\203\201\374\177\67\116\341\105\356\217\20\27\26\144\74\302"
  "\345\226\241\62\271\5\254\230\257\121\23\126\372\115\350\235\372\147\152\167\15\203\4\363\43\217\6\374\366\365\31\257"
  "\272\277\377";

/* l directory (sorted by path) */
struct memdir directory_l[45] = {
  { "lib/crt.args.wo", 881, 1, 4765, &file_l_0[0] },
  { "lib/crt.argv.wo", 876, 1, 4717, &file_l_1[0] },
  { "lib/crt.void.wo", 450, 1, 1416, &file_l_2[0] },
  { "lib/crt.wo", 121, 1, 264, &file_l_3[0] },
  { "lib/ctype.wo", 489, 1, 4203, &file_l_4[0] },
  { "lib/dirent.wo", 2161, 1, 21870, &file_l_5[0] },
  { "lib/errno.wo", 1474, 1, 9609, &file_l_6[0] },
  { "lib/fcntl.wo", 2337, 1, 20902, &file_l_7[0] },
  { "lib/fenv.wo", 260, 1, 1286, &file_l_8[0] },
  { "lib/include/assert.h", 137, 1, 182, &file_l_9[0] },
  { "lib/include/ctype.h", 154, 1, 495, &file_l_10[0] },
  { "lib/include/dirent.h", 504, 1, 1506, &file_l_11[0] },
  { "lib/include/errno.h", 599, 1, 1694, &file_l_12[0] },
  { "lib/include/fcntl.h", 715, 1, 2242, &file_l_13[0] },
  { "lib/include/fenv.h", 317, 1, 780, &file_l_14[0] },
  { "lib/include/float.h", 278, 1, 665, &file_l_15[0] },
  { "lib/include/inttypes.h", 557, 1, 3738, &file_l_16[0] },
  { "lib/include/limits.h", 230, 1, 635, &file_l_17[0] },
  { "lib/include/locale.h", 154, 1, 215, &file_l_18[0] },
  { "lib/include/math.h", 417, 1, 1519, &file_l_19[0] },
  { "lib/include/stdarg.h", 267, 1, 583, &file_l_20[0] },
  { "lib/include/stdbool.h", 81, 1, 122, &file_l_21[0] },
  { "lib/include/stddef.h", 100, 1, 178, &file_l_22[0] },
  { "lib/include/stdint.h", 490, 1, 2186, &file_l_23[0] },
  { "lib/include/stdio.h", 1054, 1, 3869, &file_l_24[0] },
  { "lib/include/stdlib.h", 597, 1, 1993, &file_l_25[0] },
  { "lib/include/string.h", 414, 1, 1854, &file_l_26[0] },
  { "lib/include/sys.cdefs.h", 113, 1, 191, &file_l_27[0] },
  { "lib/include/sys.crt.h", 154, 1, 243, &file_l_28[0] },
  { "lib/include/sys.intrs.h", 612, 1, 3110, &file_l_29[0] },
  { "lib/include/sys.stat.h", 634, 1, 2073, &file_l_30[0] },
  { "lib/include/sys.types.h", 150, 1, 414, &file_l_31[0] },
  { "lib/include/time.h", 457, 1, 1301, &file_l_32[0] },
  { "lib/include/unistd.h", 453, 1, 1529, &file_l_33[0] },
  { "lib/include/wasi.api.h", 10409, 1, 48950, &file_l_34[0] },
  { "lib/include/wasm.h", 2302, 1, 22452, &file_l_35[0] },
  { "lib/include/wasm_simd128.h", 4173, 1, 44179, &file_l_36[0] },
  { "lib/include/wchar.h", 159, 1, 292, &file_l_37[0] },
  { "lib/math.wo", 14260, 1, 168538, &file_l_38[0] },
  { "lib/stat.wo", 1077, 1, 9195, &file_l_39[0] },
  { "lib/stdio.wo", 13209, 1, 192049, &file_l_40[0] },
  { "lib/stdlib.wo", 6320, 1, 70385, &file_l_41[0] },
  { "lib/string.wo", 2033, 1, 23745, &file_l_42[0] },
  { "lib/time.wo", 4434, 1, 46152, &file_l_43[0] },
  { "lib/unistd.wo", 1892, 1, 25993, &file_l_44[0] },
};

/* end of in-memory archive */


/* 
 This code is an altered version of simple zlib format decoder by
 Mark Adler (https://github.com/madler/zlib/tree/master/contrib/puff)
 Please see the original for extensive comments on the code
  
 Copyright (C) 2002-2013 Mark Adler, all rights reserved
 version 2.3, 21 Jan 2013

 This software is provided 'as-is', without any express or implied
 warranty. In no event will the author be held liable for any damages
 arising from the use of this software.

 Permission is granted to anyone to use this software for any purpose,
 including commercial applications, and to alter it and redistribute it
 freely, subject to the following restrictions:

 1. The origin of this software must not be misrepresented; you must not
  claim that you wrote the original software. If you use this software
  in a product, an acknowledgment in the product documentation would be
  appreciated but is not required.
 2. Altered source versions must be plainly marked as such, and must not be
  misrepresented as being the original software.
 3. This notice may not be removed or altered from any source distribution.

 Mark Adler  madler@alumni.caltech.edu
*/

#define Z_MAXBITS 15
#define Z_MAXLCODES 286
#define Z_MAXDCODES 30
#define Z_MAXCODES (Z_MAXLCODES+Z_MAXDCODES)
#define Z_FIXLCODES 288

typedef struct zdst {
  const uint8_t *in; 
  size_t inlen, incnt;
  uint8_t *out;
  size_t outlen, outcnt;
  int bitbuf, bitcnt;
} zdst_t;

static int z_bits(zdst_t *s, int need, int *err)
{
  long val = s->bitbuf;
  while (s->bitcnt < need) {
    if (s->incnt == s->inlen) return *err = -12, 0;
    val |= (long)(s->in[s->incnt++]) << s->bitcnt;
    s->bitcnt += 8;
  }
  s->bitbuf = (int)(val >> need);
  s->bitcnt -= need;
  return (int)(val & ((1L << need) - 1));
}

static int z_stored(zdst_t *s)
{
  unsigned len;
  s->bitbuf = 0;
  s->bitcnt = 0;
  if (s->incnt + 4 > s->inlen) return 2;
  len = s->in[s->incnt++];
  len |= s->in[s->incnt++] << 8;
  if (s->in[s->incnt++] != (~len & 0xff) || s->in[s->incnt++] != ((~len >> 8) & 0xff)) return -2;
  if (s->incnt + len > s->inlen) return 2;
  if (s->out != NULL) {
    if (s->outcnt + len > s->outlen) return 1;
    while (len--) s->out[s->outcnt++] = s->in[s->incnt++];
  } else {
    s->outcnt += len;
    s->incnt += len;
  }
  return 0;
}

typedef struct zdhuff { short *count, *symbol; } zdhuff_t;

static int z_decode(zdst_t *s, const zdhuff_t *h)
{
  int len, code, first, count, index, bitbuf, left;
  short *next;
  bitbuf = s->bitbuf;
  left = s->bitcnt;
  code = first = index = 0;
  len = 1;
  next = h->count + 1;
  while (1) {
    while (left--) {
      code |= bitbuf & 1;
      bitbuf >>= 1;
      count = *next++;
      if (code - count < first) {
        s->bitbuf = bitbuf;
        s->bitcnt = (s->bitcnt - len) & 7;
        return h->symbol[index + (code - first)];
      }
      index += count;
      first += count;
      first <<= 1;
      code <<= 1;
      len++;
    }
    left = (Z_MAXBITS+1) - len;
    if (left == 0) break;
    if (s->incnt == s->inlen) return -12;
    bitbuf = s->in[s->incnt++];
    if (left > 8) left = 8;
  }
  return -10;
}

static int z_construct(zdhuff_t *h, const short *length, int n)
{
  int symbol, len, left;
  short offs[Z_MAXBITS+1];

  for (len = 0; len <= Z_MAXBITS; len++) h->count[len] = 0;
  for (symbol = 0; symbol < n; symbol++) (h->count[length[symbol]])++;
  if (h->count[0] == n) return 0;
  left = 1;
  for (len = 1; len <= Z_MAXBITS; len++) {
    left <<= 1;
    left -= h->count[len];
    if (left < 0) return left;
  }
  offs[1] = 0;
  for (len = 1; len < Z_MAXBITS; len++) offs[len + 1] = offs[len] + h->count[len];
  for (symbol = 0; symbol < n; symbol++)
    if (length[symbol] != 0)
      h->symbol[offs[length[symbol]]++] = symbol;

  return left;
}

static const short z_lens[29] = {
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
  35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const short z_lext[29] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
  3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const short z_dists[30] = {
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
  257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
  8193, 12289, 16385, 24577};
static const short z_dext[30] = { 
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
  7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
  12, 12, 13, 13};

static int z_codes(zdst_t *s, const zdhuff_t *lencode, const zdhuff_t *distcode)
{
  int symbol, len; unsigned dist;
  do {
    symbol = z_decode(s, lencode);
    if (symbol < 0) return symbol;
    if (symbol < 256) {
      if (s->out != NULL) {
        if (s->outcnt == s->outlen) return 1;
        s->out[s->outcnt] = symbol;
      }
      s->outcnt++;
    } else if (symbol > 256) {
      int err = 0;
      symbol -= 257;
      if (symbol >= 29) return -10;
      len = z_lens[symbol] + z_bits(s, z_lext[symbol], &err);
      if (err != 0) return err;
      symbol = z_decode(s, distcode);
      if (symbol < 0) return symbol;
      dist = z_dists[symbol] + z_bits(s, z_dext[symbol], &err);
      if (err != 0) return err;
      if (s->out != NULL) {
        if (s->outcnt + len > s->outlen) return 1;
        while (len--) {
          s->out[s->outcnt] = dist > s->outcnt ? 0 : s->out[s->outcnt - dist];
          s->outcnt++;
        }
      } else {
        s->outcnt += len;
      }
    }
  } while (symbol != 256);
  return 0;
}

static int z_virgin = 1;
static short z_lencnt[Z_MAXBITS+1], z_lensym[Z_FIXLCODES];
static short z_distcnt[Z_MAXBITS+1], z_distsym[Z_MAXDCODES];
static zdhuff_t z_lencode, z_distcode;

static int z_fixed(zdst_t *s)
{
  if (z_virgin) {
    int symbol;
    short lengths[Z_FIXLCODES];
    z_lencode.count = &z_lencnt[0], z_lencode.symbol = &z_lensym[0];
    z_distcode.count = &z_distcnt[0], z_distcode.symbol = &z_distsym[0];
    for (symbol = 0; symbol < 144; symbol++) lengths[symbol] = 8;
    for (; symbol < 256; symbol++) lengths[symbol] = 9;
    for (; symbol < 280; symbol++) lengths[symbol] = 7;
    for (; symbol < Z_FIXLCODES; symbol++) lengths[symbol] = 8;
    z_construct(&z_lencode, lengths, Z_FIXLCODES);
    for (symbol = 0; symbol < Z_MAXDCODES; symbol++) lengths[symbol] = 5;
    z_construct(&z_distcode, lengths, Z_MAXDCODES);
    z_virgin = 0;
  }
  return z_codes(s, &z_lencode, &z_distcode);
}

static const short z_order[19] =
  {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

static int z_dynamic(zdst_t *s)
{
  int nlen, ndist, ncode, index, err;
  short lengths[Z_MAXCODES], lencnt[Z_MAXBITS+1], lensym[Z_MAXLCODES];
  short distcnt[Z_MAXBITS+1], distsym[Z_MAXDCODES];
  zdhuff_t lencode, distcode;

  lencode.count = &lencnt[0], lencode.symbol = &lensym[0];
  distcode.count = &distcnt[0], distcode.symbol = &distsym[0];
  err = 0;
  nlen = z_bits(s, 5, &err) + 257;
  ndist = z_bits(s, 5, &err) + 1;
  ncode = z_bits(s, 4, &err) + 4;
  if (err != 0) return err;
  if (nlen > Z_MAXLCODES || ndist > Z_MAXDCODES) return -3;
  for (index = 0; index < ncode; index++) {
    lengths[z_order[index]] = z_bits(s, 3, &err);
    if (err != 0) return err;
  }
  for (; index < 19; index++) lengths[z_order[index]] = 0;
  err = z_construct(&lencode, lengths, 19);
  if (err != 0) return -4;

  index = 0;
  while (index < nlen + ndist) {
    int symbol, len;
    symbol = z_decode(s, &lencode);
    if (symbol < 0) return symbol;
    if (symbol < 16) {
      lengths[index++] = symbol;
    } else {
      len = 0, err = 0;
      if (symbol == 16) {
        if (index == 0) return -5;
        len = lengths[index - 1];
        symbol = 3 + z_bits(s, 2, &err);
      } else if (symbol == 17) {
        symbol = 3 + z_bits(s, 3, &err);
      } else {
        symbol = 11 + z_bits(s, 7, &err);
      }
      if (err) return err;
      if (index + symbol > nlen + ndist) return -6;
      while (symbol--) lengths[index++] = len;
    }
  }

  if (lengths[256] == 0) return -9;

  err = z_construct(&lencode, lengths, nlen);
  if (err && (err < 0 || nlen != lencode.count[0] + lencode.count[1])) return -7;
  err = z_construct(&distcode, &lengths[nlen], ndist);
  if (err && (err < 0 || ndist != distcode.count[0] + distcode.count[1])) return -8;

  return z_codes(s, &lencode, &distcode);
}

int zinflate(uint8_t *dest, size_t *destlen, const uint8_t *source, size_t *sourcelen)
{
  zdst_t s;
  int last, type, err;
  s.out = dest; /* can be NULL -- only outcnt is updated */
  s.outlen = *destlen;
  s.outcnt = 0;
  s.in = source;
  s.inlen = *sourcelen;
  s.incnt = 0;
  s.bitbuf = 0;
  s.bitcnt = 0;
  do {
    err = 0;
    last = z_bits(&s, 1, &err);
    type = z_bits(&s, 2, &err);
    if (err != 0) break;
    switch (type) {
      case 0:  err = z_stored(&s);  break;
      case 1:  err = z_fixed(&s);   break;
      case 2:  err = z_dynamic(&s); break;
      default: err = -1; break;
    }
    if (err != 0) break;
  } while (!last);
  if (err <= 0) {
    *destlen = s.outcnt;
    *sourcelen = s.incnt;
  }
  return err;
}


static int memdir_cmp(const void *p1, const void *p2)
{
  const struct memdir *phe1 = p1, *phe2 = p2;
  const char *name1 = phe1->path, *name2 = phe2->path; 
  return strcmp(name1, name2);
}

MEM *mopen(const char *path)
{
  struct memdir md, *pmd;
  md.path = path;
  pmd = bsearch(&md, &directory_l[0], sizeof(directory_l)/sizeof(struct memdir), sizeof(struct memdir), &memdir_cmp);
  if (pmd) {
    MEM *mp = emalloc(sizeof(MEM));
    if (pmd->compression == 1) {
      size_t size = pmd->size, org_size = pmd->org_size;
      char *dst = emalloc(pmd->org_size + 1);
      int err = zinflate((uint8_t *)dst, &org_size, (const uint8_t *)pmd->data, &size);
      if (err != 0 || org_size != pmd->org_size) 
        eprintf("%s: internal inflate error: %d", path, err);
      dst[pmd->org_size] = '\0'; /* 0-terminate for debugging &c. */
      mp->base = mp->curp = dst;
      mp->end = mp->base + pmd->org_size; 
      mp->inheap = true;
      return mp;
    } else if (pmd->compression == 0) {
      assert(*mp->end == '\0'); /* already 0-terminated */
      mp->base = mp->curp = (char*)pmd->data;
      mp->end = mp->base + pmd->size;
      mp->inheap = false;
      return mp;
    } else assert(false);
  }
  return NULL;
}

char *mgetlb(cbuf_t *pcb, MEM *mp)
{
  assert(mp); assert(pcb);
  cbclear(pcb);
  if (mp->curp == mp->end) return NULL;
  while (mp->curp < mp->end) {
    int c = *mp->curp++;
    if (c == '\r' && *mp->curp == '\n') continue;
    if (c == '\n') break;
    cbputc(c, pcb);
  }
  return cbdata(pcb);
}

int mclose(MEM *mp)
{
  if (mp) {
    if (mp->inheap) free((void*)mp->base);
    return 0;
  }
  return -1;
}
