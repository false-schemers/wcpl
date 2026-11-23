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


/* NB: the section below is generated with the help of baz archiver (see https://github.com/false-schemers/baz)
*  by `make lib', which runs (from a staging copy of lib/ with CRLF line ends):
*  baz cfz l.c --exclude="src" --exclude="README.md" --zopfli=1000 wcpl/lib */

struct memdir {
  const char *path;
  size_t size;
  int compression;
  size_t org_size;
  const unsigned char *data;
};

/* start of in-memory archive */

/* lib/include/stdbool.h (DEFLATEd, org. size 122) */
static unsigned char file_l_0[81] =
  "\323\327\122\160\312\317\317\111\115\314\123\50\251\54\110\125\110\314\113\121\50\113\314\51\115\55\126\320\322\347\345\342\345"
  "\122\56\50\112\114\317\115\124\310\317\113\116\5\361\201\32\222\200\32\24\62\213\25\222\112\63\163\112\164\63\363\200"
  "\52\301\342\45\105\245\251\330\304\323\22\163\212\61\45\0";

/* lib/include/locale.h (DEFLATEd, org. size 215) */
static unsigned char file_l_1[154] =
  "\115\216\275\112\300\60\24\106\367\100\336\341\243\56\115\350\337\350\132\134\343\344\340\50\151\162\133\3\361\106\154\122\301"
  "\247\267\241\124\34\317\271\334\303\67\152\230\344\154\14\77\66\207\304\320\243\24\122\74\174\176\331\355\303\42\261\243"
  "\312\243\306\236\313\62\300\131\306\102\50\73\171\4\206\57\66\366\47\300\45\117\310\11\147\150\143\160\342\376\165\176"
  "\171\226\2\32\261\346\11\337\41\277\243\312\133\34\301\142\247\174\121\153\236\336\146\143\72\64\103\311\353\143\243\356"
  "\41\236\326\300\204\353\214\351\317\374\173\215\256\253\115\205\266\75\122\360\152\122\122\374\2";

/* lib/include/stdlib.h (DEFLATEd, org. size 2146) */
static unsigned char file_l_2[664] =
  "\255\225\145\223\244\60\20\206\77\257\375\207\76\147\250\261\163\167\167\227\157\24\322\314\246\56\44\154\322\254\326\375\367"
  "\43\204\145\10\254\373\110\367\363\276\35\72\66\361\341\65\12\124\41\207\202\30\147\304\120\203\77\131\131\134\131\274"
  "\220\140\312\4\302\313\77\157\177\4\257\236\276\375\360\363\333\113\270\332\211\177\377\371\374\371\313\357\337\141\72\217"
  "\177\174\26\74\377\371\55\370\370\364\17\334\200\322\277\240\164\164\247\362\54\277\174\372\371\341\3\60\15\121\301\70"
  "\215\230\60\361\106\371\355\351\247\27\225\156\272\171\73\265\77\53\213\264\225\143\231\7\115\252\210\11\166\200\11\202"
  "\265\102\322\20\24\146\367\341\37\44\154\75\240\373\173\220\134\212\231\213\362\203\331\75\4\265\242\32\274\146\333\30"
  "\120\167\370\46\263\21\257\206\312\115\325\135\304\115\102\45\40\221\105\304\21\102\222\251\27\113\241\11\214\2\174\75"
  "\270\337\60\114\220\1\330\276\200\31\236\41\370\301\104\203\365\271\316\160\64\51\222\211\113\15\353\17\76\212\244\343"
  "\154\171\276\77\77\254\236\41\12\65\166\225\55\371\161\365\205\320\154\46\60\151\171\24\247\362\160\214\134\247\306\150"
  "\117\27\23\122\241\110\274\165\311\332\275\61\137\101\127\231\246\220\106\354\41\176\34\162\56\143\257\136\107\142\10\365"
  "\47\275\335\105\123\205\350\131\121\116\252\147\224\71\106\175\271\257\320\22\215\305\1\265\302\110\52\252\310\336\142\304"
  "\115\146\63\340\371\151\41\342\201\305\272\16\25\146\24\232\102\52\164\53\155\333\71\103\102\261\356\164\132\204\31\16"
  "\354\266\252\204\133\232\60\163\210\70\113\6\60\32\201\220\124\42\360\373\351\367\267\146\117\271\317\31\151\14\125\274"
  "\132\13\155\354\57\156\15\241\35\60\323\70\204\225\105\330\243\361\166\242\75\177\0\216\207\153\320\173\340\65\155\132"
  "\166\346\65\234\346\107\272\152\251\350\155\45\223\61\237\372\51\373\62\7\272\224\75\52\315\253\265\266\3\163\266\271"
  "\45\314\133\135\143\150\135\34\150\227\232\143\16\333\23\230\52\131\304\121\164\266\155\323\254\36\113\162\43\366\166\117"
  "\125\77\337\210\207\160\64\351\106\114\62\213\274\6\263\36\46\336\2\153\155\26\151\123\107\73\205\364\201\225\134\203"
  "\222\66\325\164\135\256\221\72\176\256\101\275\342\215\113\310\201\263\110\205\152\13\242\220\161\131\224\302\111\206\132\207"
  "\63\354\255\364\40\17\5\213\335\313\43\315\150\10\343\361\330\370\266\134\357\101\202\61\313\112\373\204\315\30\151\220"
  "\51\254\303\43\230\102\52\25\344\252\344\122\360\64\42\150\112\70\213\306\361\240\51\127\167\61\110\163\222\126\355\325"
  "\167\305\272\135\54\51\333\304\304\176\24\303\275\316\125\77\257\323\176\136\155\360\377";

/* lib/include/wchar.h (DEFLATEd, org. size 292) */
static unsigned char file_l_3[159] =
  "\215\120\263\242\4\1\14\254\237\376\41\317\147\243\75\333\256\326\266\167\377\376\154\247\211\223\231\211\6\140\312\123\64"
  "\220\34\156\340\244\105\33\120\127\64\333\2\134\241\240\153\133\253\60\20\375\170\375\170\375\326\14\234\225\161\120\25"
  "\222\136\345\313\105\167\265\204\132\300\233\100\330\274\144\205\171\145\75\274\152\361\212\165\321\131\237\241\150\206\127\150"
  "\230\26\153\371\1\332\256\167\300\27\216\207\143\136\226\331\230\37\226\333\266\305\204\223\211\365\306\331\174\176\6\276"
  "\47\206\353\235\321\372\366\376\313\171\57\77\73\134\334\262\121\124\160\227\112\204\17\112\360\121\165\175\166\1";

/* lib/include/stddef.h (DEFLATEd, org. size 178) */
static unsigned char file_l_4[100] =
  "\323\327\122\160\316\317\315\315\317\123\110\111\115\313\314\313\54\311\314\317\53\126\320\322\347\345\342\345\122\56\50\112\114"
  "\317\115\124\310\317\113\116\5\361\201\152\375\102\175\174\24\62\213\25\222\112\63\163\112\164\63\363\200\52\301\342\371"
  "\151\151\305\251\45\371\151\330\344\12\112\212\122\62\323\322\342\113\260\111\26\147\126\245\142\227\51\117\316\110\54\302"
  "\42\5\0";

/* lib/include/time.h (DEFLATEd, org. size 1301) */
static unsigned char file_l_5[457] =
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

/* lib/include/sys.cdefs.h (DEFLATEd, org. size 191) */
static unsigned char file_l_6[113] =
  "\323\327\122\110\316\317\315\315\317\123\50\256\54\56\111\315\125\110\111\115\313\314\313\54\311\314\317\53\126\320\322\347\345"
  "\342\345\122\56\50\112\114\317\115\124\310\317\113\116\5\363\301\112\122\25\202\135\135\275\343\203\135\103\24\64\14\64"
  "\25\200\346\330\52\204\73\6\173\52\204\173\270\372\71\273\202\44\200\372\321\124\73\207\6\51\150\30\142\252\6\111"
  "\140\252\166\365\163\121\320\60\302\124\15\222\200\272\15\0";

/* lib/include/dirent.h (DEFLATEd, org. size 1506) */
static unsigned char file_l_7[504] =
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

/* lib/include/stdarg.h (DEFLATEd, org. size 583) */
static unsigned char file_l_8[267] =
  "\165\120\265\162\300\60\14\235\113\377\240\262\235\162\227\362\324\261\314\143\56\174\56\310\71\103\312\377\136\311\201\362\244"
  "\207\6\255\104\160\235\30\225\244\367\5\44\246\362\17\5\72\13\321\312\304\350\304\350\164\155\222\352\41\1\215\131"
  "\301\234\262\115\22\123\12\224\205\324\253\173\267\244\60\144\7\47\166\337\274\304\116\214\102\4\36\225\306\276\373\12"
  "\101\3\120\350\130\243\261\323\113\36\255\252\260\310\173\317\263\331\171\367\32\53\326\170\376\56\364\256\147\360\255\62"
  "\364\376\57\176\151\177\255\133\365\122\304\216\165\106\103\65\327\236\266\305\162\213\372\170\263\266\276\331\306\31\15\252"
  "\126\71\104\44\62\250\235\151\365\167\340\21\266\354\236\353\42\57\312\141\201\121\170\254\262\374\361\151\62\24\206\273"
  "\62\355\321\11\11\42\0\135\212\345\345\145\51\201\66\177\263\177\172\110\147\175\113\133\227\30\47\222\172\21\32\252"
  "\20\220\260\7\334\370\226\52\60\47\213\3\374\70\111\230\122\307\127\207\207\122\376\270\273\176\26\271\165\213\140\115"
  "\306\161\302\174\240\140\52\371\23\37";

/* lib/include/NDEBUG/assert.h (DEFLATEd, org. size 150) */
static unsigned char file_l_9[116] =
  "\105\214\265\25\2\101\20\100\143\254\7\334\265\4\74\43\43\346\255\357\335\272\266\217\303\215\333\237\325\264\175\50\0"
  "\323\46\304\2\205\366\164\325\252\267\352\75\353\1\123\240\155\64\42\257\376\171\24\171\21\332\117\3\355\313\341\270"
  "\273\236\333\234\0\114\374\23\170\257\103\4\117\376\6\102\40\76\276\356\140\52\144\134\24\372\367\21\23\132\150\362"
  "\203\67\325\344\203\214\311\244\75\36\147\123\340\311\172\322\252\77\0";

/* lib/include/string.h (DEFLATEd, org. size 1854) */
static unsigned char file_l_10[414] =
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

/* lib/include/stdint.h (DEFLATEd, org. size 2186) */
static unsigned char file_l_11[490] =
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

/* lib/include/unistd.h (DEFLATEd, org. size 1529) */
static unsigned char file_l_12[453] =
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

/* lib/include/assert.h (DEFLATEd, org. size 507) */
static unsigned char file_l_13[292] =
  "\145\120\305\232\204\60\14\76\257\275\103\326\141\374\272\356\56\267\75\363\165\40\100\107\132\276\46\214\274\375\66\343\212"
  "\376\232\320\254\300\263\126\231\261\304\72\46\250\64\17\166\17\166\217\13\247\262\276\2\153\142\224\167\57\342\134\23"
  "\370\103\201\303\254\354\51\7\71\252\4\235\70\204\47\126\76\40\122\104\350\130\204\355\122\367\270\256\315\134\20\333"
  "\176\241\173\10\103\315\71\324\237\177\237\137\36\377\336\200\55\144\310\76\35\141\206\14\320\221\266\246\6\303\34\35"
  "\302\62\120\201\261\165\133\314\107\114\60\325\146\341\152\315\306\34\72\315\110\160\224\372\252\313\236\27\134\316\22\174"
  "\44\244\312\243\311\221\224\22\373\331\135\115\212\15\260\123\305\164\165\34\61\72\3\3\253\23\210\146\313\104\142\13"
  "\142\153\210\41\316\225\337\130\302\153\240\15\203\64\204\127\323\17\200\14\355\261\4\153\13\201\116\241\44\114\102\351"
  "\112\173\45\345\240\231\74\351\120\365\11\332\230\132\207\122\16\175\44\122\31\156\265\7\225\145\277\370\303\100\140\351"
  "\132\356\76\345\3\14\41\220\313\35\114\65\55\270\334\30\76\212\136\77\276\137\242\250\346\361\357\217\137\377\24\206"
  "\7\273\377";

/* lib/include/stdio.h (DEFLATEd, org. size 3869) */
static unsigned char file_l_14[1054] =
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

/* lib/include/sys.stat.h (DEFLATEd, org. size 2073) */
static unsigned char file_l_15[634] =
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

/* lib/include/math.h (DEFLATEd, org. size 1519) */
static unsigned char file_l_16[417] =
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

/* lib/include/float.h (DEFLATEd, org. size 665) */
static unsigned char file_l_17[278] =
  "\322\327\122\360\314\55\310\111\315\115\315\53\111\54\311\314\317\123\310\311\314\315\54\51\126\320\110\53\320\124\320\322\347"
  "\345\342\345\122\56\50\112\114\317\115\124\310\317\113\116\5\363\123\122\323\62\363\0\145\220\5\212\105\61\14\105\267"
  "\122\164\264\217\150\223\240\343\356\372\321\207\165\377\113\370\256\55\316\65\16\267\246\233\253\227\361\346\361\76\241\36"
  "\152\257\227\157\77\113\103\271\65\46\43\302\170\73\371\110\14\336\171\113\3\201\244\161\36\337\66\255\343\314\140\47"
  "\275\275\362\20\10\117\366\1\357\136\126\34\345\100\332\361\221\264\306\216\317\73\153\205\107\255\161\100\147\47\235\273"
  "\201\43\355\330\276\56\157\36\47\211\346\162\173\105\302\301\302\112\60\262\170\41\106\65\160\324\172\306\320\203\45\36"
  "\4\310\211\245\24\166\45\167\15\107\232\207\375\356\160\371\366\343\373\361\345\375\55\321\100\4\42\5\44\110\201\221"
  "\301\301\265\146\54\355\267\253\254\202\261\253\53\30\1\262\63\104\315\35\311\146\34\7\14\202\40\17\125\60\107\322"
  "\232\301\356\332\143\226\101\123\11\141\5\47\42\67\5\13\253\171\211\75\3";

/* lib/include/ctype.h (DEFLATEd, org. size 495) */
static unsigned char file_l_18[154] =
  "\164\320\101\152\5\61\10\6\340\375\203\271\203\320\115\233\115\16\320\145\117\342\230\220\110\23\23\214\241\163\374\62\333"
  "\171\270\123\76\104\177\143\200\237\212\212\144\131\241\242\244\306\122\40\304\343\165\274\76\246\142\351\10\103\50\337\175"
  "\276\54\253\0\213\1\57\154\262\373\347\135\323\327\367\233\315\212\216\221\230\66\307\22\27\66\307\212\342\254\216\265"
  "\361\227\325\261\251\54\346\331\26\362\154\115\244\354\330\236\323\335\167\371\41\154\370\203\66\36\41\142\0\31\262\14"
  "\45\241\46\70\267\1\215\336\207\100\210\317\167\57\142\166\316\71\33\312\357\377\20\162\0";

/* lib/include/fenv.h (DEFLATEd, org. size 780) */
static unsigned char file_l_19[317] =
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

/* lib/include/wasm.h (DEFLATEd, org. size 22452) */
static unsigned char file_l_20[2302] =
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

/* lib/include/wasi.api.h (DEFLATEd, org. size 48950) */
static unsigned char file_l_21[10409] =
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

/* lib/include/inttypes.h (DEFLATEd, org. size 3738) */
static unsigned char file_l_22[557] =
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

/* lib/include/fcntl.h (DEFLATEd, org. size 2242) */
static unsigned char file_l_23[715] =
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

/* lib/include/errno.h (DEFLATEd, org. size 1694) */
static unsigned char file_l_24[599] =
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

/* lib/include/sys.crt.h (DEFLATEd, org. size 243) */
static unsigned char file_l_25[154] =
  "\125\215\1\7\303\60\20\205\1\10\371\17\247\203\266\120\66\300\300\60\100\67\146\30\63\25\351\165\216\345\122\311\145"
  "\266\375\372\265\205\154\200\367\276\357\356\65\65\204\304\102\16\241\307\201\230\204\74\107\250\33\255\264\132\215\301\334"
  "\235\1\317\26\163\162\276\117\17\204\302\6\51\26\153\71\104\70\36\316\373\123\333\265\273\13\154\326\132\311\173\304"
  "\211\300\323\123\17\145\355\131\60\270\156\110\154\73\251\312\271\255\266\132\341\153\252\31\376\350\157\212\327\374\366\226"
  "\375\110\37\314\242\365\211\45\303\145\161\6\304\106\260\44\26\210\142\44\305\171\120\253\57";

/* lib/include/sys.types.h (DEFLATEd, org. size 414) */
static unsigned char file_l_26[150] =
  "\165\317\201\6\303\60\20\306\161\0\112\337\341\30\120\246\154\63\350\303\124\233\134\353\64\271\124\163\35\335\323\57\121"
  "\65\326\33\40\371\175\234\177\135\201\11\336\7\206\270\105\101\17\262\315\30\241\252\313\242\54\56\363\322\215\276\203"
  "\300\6\363\73\215\43\275\261\25\240\10\375\112\116\256\304\151\273\313\177\42\226\131\26\305\362\61\213\103\36\74\37"
  "\311\41\14\103\53\315\27\326\103\330\21\117\72\131\174\351\100\34\116\160\277\45\360\301\242\56\53\131\35\306\37\70"
  "\376\241\167\323\36\336\234\163\62\32\26\335\204\74\352\142\134\60\173\353\7";

/* lib/include/sys.intrs.h (DEFLATEd, org. size 3110) */
static unsigned char file_l_27[612] =
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

/* lib/include/limits.h (DEFLATEd, org. size 635) */
static unsigned char file_l_28[230] =
  "\164\120\63\202\103\1\20\255\127\167\210\155\227\153\306\256\222\174\333\274\375\142\366\33\123\215\37\332\265\314\207\40\363"
  "\204\100\210\72\242\63\222\230\341\31\201\321\265\114\255\175\167\175\167\235\227\125\204\22\220\214\44\142\4\324\70\101"
  "\62\42\221\171\172\177\130\237\37\77\266\231\111\244\67\173\70\146\272\275\161\264\373\61\317\124\232\335\336\244\352\17"
  "\146\217\347\351\313\34\16\6\231\137\32\206\116\66\47\0\353\156\174\314\267\60\316\164\354\61\371\37\241\41\274\375"
  "\373\333\364\27\2\0\323\305\374\15\356\335\77\277\40\26\242\11\375\236\213\342\255\301\47\367\143\312\32\354\171\164"
  "\302\61\235\306\26\101\261\167\324\354\116\247\1\152\233\144\257\66\251\146\155\336\327\41\57\102\223\230\21\201\303\235"
  "\207\324\33\16\3\155\357\141\307\16\373\272\3\33\334\1\304\56\60\365\134\330\375\14\177\250\206\242\204\127\32\32"
  "\10\5\5\30\0";

/* lib/include/wasm_simd128.h (DEFLATEd, org. size 44179) */
static unsigned char file_l_29[4173] =
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

/* lib/string.wo (DEFLATEd, org. size 23745) */
static unsigned char file_l_30[2033] =
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

/* lib/errno.wo (DEFLATEd, org. size 9609) */
static unsigned char file_l_31[1474] =
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
static unsigned char file_l_32[2337] =
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

/* lib/dirent.wo (DEFLATEd, org. size 21870) */
static unsigned char file_l_33[2161] =
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

/* lib/crt.argv.wo (DEFLATEd, org. size 4717) */
static unsigned char file_l_34[876] =
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

/* lib/fenv.wo (DEFLATEd, org. size 1286) */
static unsigned char file_l_35[260] =
  "\265\221\105\162\304\100\14\105\327\241\73\50\314\274\233\333\164\154\331\201\246\222\324\201\333\307\130\32\346\61\252\236\340"
  "\367\57\135\271\220\47\213\160\126\240\377\76\332\7\270\372\160\61\220\300\111\106\162\2\47\34\317\116\340\252\264\341"
  "\315\130\70\253\330\240\42\160\345\222\300\307\353\313\365\365\365\224\36\207\56\320\137\325\326\6\155\133\27\77\165\35"
  "\105\362\131\53\72\50\60\263\150\10\177\63\214\2\127\370\333\316\32\301\47\115\127\325\27\15\31\7\147\316\360\127"
  "\163\0\150\71\241\44\362\165\74\145\174\211\322\116\51\254\51\125\140\54\61\56\121\263\330\152\364\250\55\346\245\205"
  "\311\174\60\216\373\122\254\276\326\126\340\131\326\170\307\326\4\131\306\235\51\135\140\354\212\220\223\225\41\261\72\174"
  "\310\202\147\201\247\345\66\112\41\371\134\265\225\365\312\33\213\360\204\210\262\161\173\15\334\236\271\52\120\325\236\214"
  "\153\126\50\56\275\257\367\140\363\311\175\51\235\71\174\163\77\74\341\207\67\367\223\142\156\4\107\347\52\134\151\164"
  "\365\374\3";

/* lib/stdio.wo (DEFLATEd, org. size 191886) */
static unsigned char file_l_36[13041] =
  "\354\130\301\216\352\66\24\335\127\352\77\130\121\26\320\305\210\4\232\105\244\267\350\246\322\154\146\325\115\245\112\221\211"
  "\155\306\32\307\116\155\347\361\170\137\337\340\33\102\160\342\102\6\132\265\22\222\5\366\311\275\347\134\333\47\304\141"
  "\121\51\322\10\212\142\143\11\127\77\376\200\320\202\127\265\322\26\105\245\266\21\212\114\35\107\150\261\23\152\213\5"
  "\212\133\54\157\21\264\250\32\213\370\72\135\56\227\23\71\25\255\224\76\264\151\320\201\264\256\277\362\63\302\52\40"
  "\160\21\275\307\206\27\106\342\332\274\53\133\324\232\176\345\164\237\264\4\65\266\357\205\246\22\127\264\45\142\215\54"
  "\121\74\35\234\17\102\321\242\306\32\127\116\350\276\276\246\246\21\223\113\202\215\241\156\216\105\1\335\202\211\306\274"
  "\17\246\13\160\176\171\175\142\362\124\153\251\242\376\273\317\167\343\334\175\116\144\225\366\120\323\66\213\33\302\167\334"
  "\366\253\343\360\274\103\303\223\11\221\231\32\227\324\47\3\164\56\231\125\102\355\251\366\310\72\364\126\62\143\65\227"
  "\73\60\137\371\176\46\3\74\7\364\336\275\354\104\134\107\120\351\213\0\372\31\62\252\265\32\325\334\341\237\133\202"
  "\262\76\370\174\35\374\110\163\33\113\4\337\302\34\254\42\275\42\340\71\240\127\211\131\266\371\173\142\41\106\314\0"
  "\317\253\377\232\114\23\320\151\36\50\304\64\245\276\310\21\33\22\205\162\53\54\204\52\375\154\100\147\357\30\266\364"
  "\33\267\76\31\240\263\311\12\126\133\345\176\111\214\317\70\270\324\321\272\375\176\244\13\131\51\255\150\253\120\365\340"
  "\236\164\140\176\204\356\241\144\134\222\102\123\161\174\150\170\324\303\113\237\221\150\44\67\226\264\32\232\142\322\163\3"
  "\232\37\261\0\351\174\201\275\346\226\372\12\16\174\230\204\60\224\176\370\22\16\234\244\315\66\301\73\47\50\121\12"
  "\145\106\263\160\340\354\152\271\301\326\36\174\56\100\147\223\65\122\160\371\201\255\117\167\302\37\266\306\272\42\134\217"
  "\165\72\170\16\165\167\162\200\103\137\316\45\267\34\13\376\235\22\70\330\101\132\373\371\122\52\151\154\177\146\43\330"
  "\342\123\122\301\325\26\55\350\67\250\361\70\212\226\350\53\326\250\45\332\311\57\33\24\375\261\132\315\154\211\207\74"
  "\272\245\377\260\126\346\151\375\373\355\331\236\355\331\236\355\331\236\55\32\77\264\211\331\244\61\202\207\72\74\247\23"
  "\24\275\276\375\372\372\366\372\333\357\301\224\365\50\345\355\227\267\305\117\313\120\102\226\116\45\4\243\307\364\22\313"
  "\140\364\146\252\376\140\364\317\243\150\56\131\37\15\347\50\210\166\307\171\126\37\141\164\161\144\102\35\44\124\211\5"
  "\212\131\75\104\267\55\372\201\342\64\206\241\246\354\305\77\44\301\25\227\375\142\250\75\62\234\40\125\243\70\211\207"
  "\1\73\10\270\306\166\76\232\145\233\325\31\302\204\234\7\302\26\315\171\104\377\374\336\125\254\13\316\240\342\301\4"
  "\326\301\52\200\113\141\202\24\143\206\332\57\111\166\72\342\371\265\44\353\144\120\213\44\376\365\325\260\234\256\317\302"
  "\263\247\266\321\22\372\124\222\276\163\265\130\20\133\247\336\272\4\166\141\253\141\17\72\166\157\220\306\243\111\370\325"
  "\215\234\4\357\44\130\10\337\70\374\176\347\360\253\326\341\377\15\357\360\7\231\207\205\271\133\104\364\67\257\133\163"
  "\300\211\126\365\315\266\341\363\175\303\157\66\316\25\237\270\377\171\237\76\231\347\223\364\36\227\270\25\377\377\270\304"
  "\173\73\277\364\11\117\142\360\111\17\244\35\0\161\360\216\17\63\360\231\310\304\246\161\66\371\123\7\53\350\375\107"
  "\322\147\334\354\241\44\13\56\230\245\324\315\306\133\167\300\46\335\261\12\231\43\33\40\352\257\366\276\105\271\155\134"
  "\331\366\127\246\316\350\76\167\315\24\301\267\346\174\214\113\261\244\104\167\24\112\45\312\116\346\176\375\141\230\370\364"
  "\6\26\344\245\166\123\14\255\160\77\7\260\330\0\32\375\106\67\160\222\177\156\317\207\323\6\277\226\355\17\46\153"
  "\133\70\247\24\5\136\322\10\136\322\221\360\42\253\214\251\67\101\122\20\75\26\6\103\0\201\334\23\72\355\146\160"
  "\341\44\14\120\6\337\41\175\313\22\270\24\76\155\276\5\211\45\226\365\243\343\273\211\370\333\113\50\155\261\335\355"
  "\373\63\103\57\300\266\350\216\117\275\36\61\20\137\261\41\217\335\262\174\336\75\154\367\253\217\101\337\166\335\267\71"
  "\303\113\107\26\166\344\141\107\25\166\224\141\107\21\166\324\106\261\42\164\202\137\310\76\161\213\56\221\216\146\103\314"
  "\307\253\254\203\175\273\201\337\212\33\0\346\42\231\52\265\164\75\212\0\242\16\346\44\360\205\362\310\50\62\75\333"
  "\160\35\111\53\207\42\66\262\374\132\214\1\347\333\6\205\337\314\143\226\3\330\102\70\147\276\20\367\232\336\226\117"
  "\104\266\326\17\155\50\40\35\174\266\163\340\47\54\253\213\372\114\320\50\266\122\6\0\234\313\125\20\362\10\204\245"
  "\12\102\272\20\233\245\170\335\13\112\313\72\257\322\54\115\0\25\337\45\331\5\343\47\213\2\312\226\105\221\351\0"
  "\341\142\313\312\45\165\135\346\32\70\262\114\106\273\100\260\145\327\24\13\273\2\212\124\121\322\156\353\301\256\375\241"
  "\234\13\332\211\337\136\116\217\35\220\140\362\354\65\63\6\110\161\211\304\234\46\32\10\75\226\144\317\275\115\17\20"
  "\46\104\2\266\133\126\24\171\336\123\271\130\117\224\300\152\31\113\246\301\7\163\145\126\347\206\201\274\45\326\262\370"
  "\216\70\321\317\361\172\205\232\343\112\47\76\173\121\234\162\322\16\346\353\26\203\100\236\367\330\52\125\10\127\275\151"
  "\102\14\352\230\203\267\346\46\61\14\214\220\363\270\60\121\302\5\316\41\353\251\256\211\206\325\4\110\171\15\20\227"
  "\22\50\205\31\1\2\204\72\67\12\220\245\35\44\103\16\167\344\152\203\324\301\140\252\374\112\157\152\246\265\130\177"
  "\360\107\340\47\66\153\320\277\60\351\166\263\337\74\236\301\333\46\310\246\362\22\106\202\225\337\32\341\315\346\62\42"
  "\223\140\275\154\152\227\45\145\231\377\271\371\172\356\0\75\164\177\173\200\331\247\42\212\375\304\33\35\274\62\27\171"
  "\114\26\144\366\275\20\254\302\325\342\16\175\340\316\307\234\371\113\216\74\70\356\60\203\356\113\215\313\44\270\220\120"
  "\303\25\113\130\7\153\130\107\43\22\153\305\22\44\274\154\217\52\360\230\1\217\73\324\141\307\62\354\160\11\364\270"
  "\361\102\21\30\23\0\221\116\374\376\361\115\33\167\63\323\46\166\16\66\202\316\314\176\212\105\125\16\141\121\25\166"
  "\213\152\40\223\162\227\123\363\147\174\43\355\236\203\101\325\125\301\240\312\20\14\242\20\122\22\14\342\20\162\26\14"
  "\32\334\32\336\325\332\143\15\247\71\325\40\41\251\333\232\142\170\344\50\30\266\41\162\31\101\344\122\213\310\124\217"
  "\310\367\31\112\233\176\350\314\45\54\166\206\77\303\340\231\154\121\2\221\57\63\311\71\67\104\266\320\37\350\52\242"
  "\227\252\47\112\14\277\231\243\142\12\377\103\74\36\61\336\277\267\301\170\347\307\204\335\137\244\333\103\35\322\115\367"
  "\333\370\144\365\31\130\2\113\143\242\332\263\174\214\240\163\36\17\0\164\340\51\30\114\0\363\107\20\37\174\23\160"
  "\41\251\220\56\52\15\361\231\365\221\323\74\17\317\307\275\12\121\5\55\311\32\65\233\237\313\272\370\371\55\356\217"
  "\166\14\356\65\241\271\257\217\336\14\102\241\151\121\332\210\77\107\222\361\152\10\25\273\6\113\305\214\225\357\5\333"
  "\177\156\167\373\175\134\104\60\101\330\141\373\303\323\26\304\236\210\64\133\152\3\15\102\224\127\7\41\156\46\176\6"
  "\360\106\335\302\100\344\6\337\74\245\211\111\166\221\120\353\147\55\354\152\103\153\304\307\112\62\317\200\367\312\153\15"
  "\30\265\210\307\141\142\10\366\311\271\4\51\314\274\223\66\53\264\130\250\303\22\104\44\37\116\243\122\140\71\121\306"
  "\256\306\147\154\324\133\366\0\134\155\132\106\174\242\134\347\75\174\123\227\235\326\23\363\377\245\107\357\0\110\52\270"
  "\170\4\253\365\243\164\115\100\165\112\307\162\274\330\274\135\314\362\42\24\231\16\232\63\203\171\103\150\63\40\126\354"
  "\243\244\265\306\66\31\77\153\333\131\104\60\172\62\350\265\16\266\141\345\145\114\316\225\32\60\231\302\103\26\316\25"
  "\70\361\75\27\162\14\251\272\322\33\250\256\361\42\54\74\317\230\114\40\35\302\250\156\237\76\134\16\324\364\172\64"
  "\16\35\335\4\175\104\307\32\307\220\31\102\144\250\353\37\60\131\115\177\120\210\333\53\370\276\345\311\44\152\357\217"
  "\33\211\201\331\171\216\273\16\60\306\60\36\210\323\131\312\301\211\302\123\370\13\320\201\204\371\310\74\121\273\352\375"
  "\323\222\372\101\206\214\75\220\343\303\373\161\256\324\72\114\322\247\240\3\225\43\72\276\131\70\376\351\356\37\51\314"
  "\117\347\104\162\225\214\7\251\74\325\202\363\10\53\67\133\255\305\73\354\233\241\153\330\271\31\176\146\127\333\371\100"
  "\176\317\343\341\251\71\353\13\317\316\47\337\153\153\366\233\355\71\350\32\316\277\4\274\167\13\3\135\334\315\51\374"
  "\135\277\336\333\326\124\365\370\273\125\225\23\216\104\126\370\371\151\17\170\351\267\6\74\13\360\25\140\102\32\233\376"
  "\355\332\167\117\264\157\140\262\113\140\4\171\46\356\6\120\54\6\70\102\334\242\126\215\233\55\344\373\365\356\371\341"
  "\11\76\300\175\267\233\40\116\151\202\30\374\157\243\25\47\136\27\317\34\324\57\112\137\362\154\267\253\320\225\324\263"
  "\13\262\360\225\4\333\104\375\364\55\377\132\106\301\51\203\154\45\313\250\57\156\110\343\235\7\76\36\216\377\134\65"
  "\120\203\73\200\162\337\312\63\40\326\244\217\154\24\231\352\210\376\25\231\230\202\65\365\344\266\1\162\3\231\313\204"
  "\35\37\110\146\217\32\16\42\66\67\271\222\247\277\354\24\17\243\271\305\364\141\325\156\206\262\217\264\141\1\224\50"
  "\375\164\354\34\335\203\41\173\63\210\76\143\3\333\23\322\355\341\134\113\316\220\75\242\205\73\241\57\223\102\156\303"
  "\140\211\44\14\221\224\233\306\34\357\222\125\1\144\10\147\365\174\251\113\311\61\171\252\166\375\141\17\76\50\316\55"
  "\324\122\356\374\370\351\157\205\214\43\42\154\2\147\171\44\206\201\14\214\151\315\30\262\262\147\61\46\374\344\116\261"
  "\315\126\42\36\46\37\327\125\4\231\12\32\177\327\7\266\265\61\216\117\142\24\166\255\105\210\317\44\76\121\317\216"
  "\177\156\103\210\313\42\42\277\137\51\25\44\150\364\135\230\241\361\250\216\257\175\70\6\222\164\173\244\326\241\273\105"
  "\272\106\175\41\177\3\45\372\313\143\73\150\152\36\27\344\307\50\241\144\3\345\273\26\276\343\243\367\210\43\74\216"
  "\52\217\347\322\241\100\226\21\160\315\70\167\224\336\74\76\67\344\15\253\165\372\372\221\172\111\144\245\101\356\161\226"
  "\227\163\271\47\10\104\276\327\330\230\76\214\304\345\147\230\113\132\307\317\360\60\374\105\116\376\140\352\161\56\300\351"
  "\330\271\200\163\326\43\343\146\164\323\111\220\130\156\225\260\117\336\232\330\117\330\362\112\256\134\20\367\104\101\76\143"
  "\137\126\243\310\204\100\345\101\375\102\371\241\340\361\326\364\74\201\364\144\273\143\151\107\5\204\347\6\66\136\105\121"
  "\220\230\237\136\174\360\43\12\236\200\60\136\52\114\151\112\205\41\321\35\203\342\255\364\212\227\347\103\214\240\170\353"
  "\327\44\347\217\52\356\272\103\233\136\27\327\134\27\343\311\63\237\5\243\156\161\135\204\71\205\316\15\32\130\301\275"
  "\31\52\72\273\42\216\247\111\364\274\54\116\332\367\366\224\23\45\36\345\33\375\321\300\175\145\112\350\307\342\36\336"
  "\24\262\61\154\7\70\334\367\102\107\352\166\365\321\304\143\66\354\243\140\13\152\310\140\165\260\251\66\13\23\270\4"
  "\347\203\146\46\350\216\206\255\247\331\114\256\206\2\54\105\373\331\21\377\11\30\40\250\14\170\216\102\163\165\376\15"
  "\251\354\30\54\113\112\232\271\7\253\270\151\54\264\276\144\313\364\232\5\255\113\244\130\173\246\226\130\321\167\236\251"
  "\5\367\32\341\105\66\361\113\172\306\316\341\221\335\47\102\216\244\351\150\42\374\322\67\132\242\312\73\115\374\241\244"
  "\200\314\52\37\161\233\355\266\25\153\323\257\6\42\45\351\46\343\256\27\171\301\363\360\334\50\342\306\12\17\111\10"
  "\34\156\244\362\160\214\75\13\357\146\111\127\333\343\323\271\25\27\256\157\206\36\134\253\166\315\232\365\241\331\50\75"
  "\261\143\360\203\346\174\255\247\306\257\253\355\347\163\313\263\52\342\335\14\317\305\104\52\375\154\153\153\266\266\360\127"
  "\166\23\207\333\34\355\265\76\2\212\326\307\343\77\200\305\67\276\357\163\204\36\330\60\142\104\2\3\163\176\242\356"
  "\332\364\355\45\273\231\147\266\227\104\126\162\240\124\300\52\311\307\140\101\201\210\234\2\163\313\77\40\172\325\166\103"
  "\253\335\222\126\143\63\104\315\275\66\42\51\272\377\333\157\32\30\254\323\326\3\304\205\160\221\2\30\143\102\170\202"
  "\204\137\331\363\65\175\373\350\262\171\104\256\244\157\55\257\21\312\46\210\315\66\50\271\242\335\76\360\164\37\31\242"
  "\137\356\213\20\124\177\157\13\262\365\251\262\103\144\15\360\34\122\225\102\122\237\344\253\37\154\23\343\154\220\364\373"
  "\234\43\0\335\311\341\161\156\177\275\303\172\333\233\141\36\243\244\264\112\21\301\23\237\252\331\101\26\335\222\106\215"
  "\327\24\65\16\212\41\234\331\115\117\76\211\370\351\6\175\24\351\323\67\15\302\7\275\124\203\341\72\162\232\203\233"
  "\104\232\303\173\55\123\325\47\1\246\105\161\111\344\351\253\266\331\173\272\242\222\375\250\317\243\75\263\32\151\77\233"
  "\231\141\146\206\341\263\45\5\214\41\301\307\220\315\303\364\110\353\353\21\364\30\110\102\14\145\66\14\227\266\211\241"
  "\2\126\131\322\200\316\15\72\237\11\106\40\61\342\37\355\124\104\206\344\23\261\113\40\320\311\340\143\5\246\230\270"
  "\167\174\131\107\233\140\227\365\145\54\261\7\357\377\272\215\371\272\15\144\142\65\27\354\111\311\63\214\144\270\52\115"
  "\326\300\252\350\133\265\313\206\250\236\110\334\275\325\207\325\215\361\106\355\135\215\322\67\345\310\274\375\246\20\173\144"
  "\236\120\5\350\52\210\46\63\306\155\270\246\325\304\224\221\230\370\73\304\112\63\260\275\42\200\373\324\370\236\175\337"
  "\36\327\333\341\227\256\75\16\37\316\35\376\332\353\341\263\227\107\10\210\355\171\16\353\324\313\314\23\105\331\275\55"
  "\352\247\220\210\343\127\156\332\257\25\37\377\154\326\144\126\362\1\107\360\267\107\270\235\330\240\22\257\71\127\372\366"
  "\270\243\210\377\276\211\201\135\137\374\177\37\67\350\73\355\76\356\32\252\26\36\41\253\151\372\57\101\114\40\71\36"
  "\257\66\31\341\166\335\153\222\6\136\66\36\202\266\350\305\274\353\43\50\303\71\240\142\132\346\13\33\61\164\367\302"
  "\255\230\66\311\167\22\265\250\46\247\351\315\31\17\352\107\153\335\245\107\153\345\207\137\116\253\343\103\7\307\323\60"
  "\44\257\124\166\216\341\24\124\7\272\10\362\325\240\166\150\132\133\117\240\37\171\304\317\306\126\204\106\304\20\35\162"
  "\250\321\352\334\220\226\76\52\127\62\231\314\270\243\336\330\30\47\236\161\4\114\333\63\103\106\340\102\363\316\32\136"
  "\276\233\376\323\165\206\353\361\155\271\164\50\301\343\372\5\64\243\126\313\240\56\341\267\70\40\355\352\234\276\133\134"
  "\12\270\125\276\54\254\210\365\127\321\25\317\357\106\144\172\306\260\311\231\245\101\316\334\51\277\202\236\320\47\151\35"
  "\111\334\122\227\202\165\336\354\367\342\251\367\115\175\12\226\347\274\277\364\255\326\377\357\251\275\124\120\64\120\40\116"
  "\234\301\161\122\103\157\30\350\305\123\137\13\216\320\123\170\331\220\21\205\73\256\5\47\63\160\352\352\257\32\23\260"
  "\7\247\107\14\316\123\342\154\326\206\270\363\164\43\4\42\202\104\363\150\236\161\43\221\2\301\41\50\145\22\56\0"
  "\320\134\161\234\66\137\166\315\132\64\307\367\66\250\216\301\237\265\267\230\215\43\320\104\162\221\46\310\144\354\32\152"
  "\242\17\154\116\346\66\372\77\362\45\12\317\367\353\23\230\216\202\4\47\52\224\50\163\62\217\207\40\53\263\353\140"
  "\307\100\122\243\316\255\315\62\47\326\46\312\231\173\67\77\147\363\163\66\77\147\363\163\66\77\211\261\311\207\51\163"
  "\203\11\312\40\353\15\323\136\61\160\203\5\143\72\161\171\132\277\345\332\226\66\324\150\255\135\243\141\6\303\45\5"
  "\167\224\346\304\263\32\144\303\144\27\142\372\242\276\100\3\0\151\316\217\30\136\203\314\11\13\222\365\215\307\307\50"
  "\274\246\23\126\50\163\321\140\42\114\41\324\74\247\1\20\45\364\356\223\3\310\372\356\41\145\100\342\147\223\117\42"
  "\300\251\316\151\5\303\107\217\346\264\2\36\152\234\23\5\346\104\201\71\121\140\12\34\70\302\321\77\261\2\324\11"
  "\1\335\164\237\275\147\0\177\164\20\57\27\237\234\370\174\130\157\260\346\236\170\302\3\75\4\130\242\243\253\367\153"
  "\141\36\11\364\70\350\301\331\147\320\223\103\117\1\75\57\53\230\224\347\227\53\130\76\105\233\315\364\226\261\41\102"
  "\133\224\103\72\325\242\166\366\273\17\177\155\117\233\215\246\344\51\267\234\300\211\6\101\50\366\3\6\103\270\204\110"
  "\340\351\34\1\146\151\241\322\155\170\325\233\64\13\277\231\277\172\207\157\272\240\322\275\227\230\101\4\135\252\213\163"
  "\0\220\353\0\340\14\112\45\204\154\21\275\77\127\100\117\340\100\63\67\75\367\241\347\327\162\20\176\55\206\340\327"
  "\142\30\176\205\102\363\14\56\105\266\155\265\341\45\64\44\126\375\166\343\303\73\157\273\43\47\341\117\157\102\21\77"
  "\76\144\245\327\324\164\142\265\375\146\102\3\103\324\213\370\122\355\156\176\172\63\47\102\363\362\112\255\217\357\10\362"
  "\365\133\207\64\140\336\243\336\222\125\114\107\340\200\365\363\271\153\34\36\337\140\132\44\204\331\207\73\1\37\304\234"
  "\162\143\223\226\45\312\251\71\45\103\315\55\235\270\71\334\302\106\71\314\231\302\110\313\331\102\357\215\214\41\216\234"
  "\51\34\152\40\11\14\62\200\112\27\123\201\244\65\310\167\266\213\53\212\261\102\303\60\162\71\330\55\320\44\152\22"
  "\6\115\64\61\223\370\122\250\301\200\63\145\137\21\303\5\355\43\21\372\101\200\110\102\255\34\73\17\273\366\341\360"
  "\170\176\130\167\5\64\347\340\16\240\117\374\46\367\157\277\212\74\16\13\330\250\25\267\200\352\100\27\305\133\153\212"
  "\31\132\326\233\307\167\214\226\352\126\150\371\264\371\372\53\242\105\177\241\223\156\146\313\152\370\105\213\260\320\257\172"
  "\344\345\227\305\315\226\137\45\226\325\163\236\150\317\247\146\277\151\36\132\237\41\272\356\40\352\276\372\172\125\230\275"
  "\175\351\44\67\306\236\117\350\316\262\13\137\303\133\202\333\370\135\107\14\361\57\153\211\367\253\57\255\225\257\370\306"
  "\150\357\243\45\17\310\340\34\315\357\307\300\70\270\161\200\16\102\142\253\363\141\207\324\105\317\153\56\234\326\360\24"
  "\354\35\177\107\3\127\104\334\22\102\167\150\315\240\16\126\337\0\272\103\121\10\317\52\302\112\246\166\353\256\302\303"
  "\40\50\6\75\253\270\35\147\247\177\343\2\367\202\232\27\315\103\147\277\156\67\47\237\330\373\67\167\74\131\272\133"
  "\177\5\341\332\311\140\277\257\33\226\333\44\75\54\350\353\301\135\371\76\156\77\275\50\124\104\50\333\36\301\17\71"
  "\212\146\150\334\356\366\233\33\43\121\4\315\66\174\254\122\137\353\203\250\24\6\234\37\103\230\137\6\271\311\313\40"
  "\370\220\46\262\233\76\360\337\375\17\130\51\320\260\147\136\363\67\33\344\272\337\370\345\345\352\273\43\373\175\354\244"
  "\313\71\220\54\355\231\275\340\260\353\53\44\104\154\214\360\12\103\67\253\313\17\224\361\167\303\244\131\332\317\226\131"
  "\64\377\217\24\44\121\157\327\236\127\37\366\233\157\120\276\1\376\66\326\267\341\277\315\210\332\235\262\174\46\65\243"
  "\151\262\350\123\221\53\122\311\1\45\233\130\372\246\211\271\22\146\346\312\160\152\351\205\251\345\127\116\55\123\116\115"
  "\165\200\133\134\71\211\374\226\223\50\57\117\202\46\353\262\142\56\342\65\161\151\263\355\224\203\136\336\154\177\333\152"
  "\344\315\360\302\103\55\17\220\341\7\144\362\37\363\336\166\213\131\157\76\37\316\35\132\313\134\372\70\265\130\130\32"
  "\247\61\54\265\210\217\334\370\164\262\23\123\126\54\140\152\360\176\130\265\1\61\175\331\255\317\237\374\256\243\330\316"
  "\57\246\0\322\233\200\376\321\261\176\174\325\116\376\170\70\267\35\32\374\316\16\274\137\135\330\315\371\307\340\322\367"
  "\341\30\120\366\126\72\24\271\233\366\232\304\232\46\163\272\204\147\163\342\134\35\364\144\36\353\176\334\37\76\374\267"
  "\41\163\72\377\325\36\27\340\301\166\110\272\374\143\352\32\313\167\55\174\307\107\357\167\204\307\135\72\212\341\77\132"
  "\77\322\337\274\220\22\160\140\117\312\366\347\13\376\170\45\336\333\363\6\161\34\310\274\304\173\300\27\152\136\30\300"
  "\23\253\235\64\71\16\61\17\201\55\136\332\223\27\145\304\221\320\153\65\15\37\45\1\120\273\215\130\372\235\75\276"
  "\336\235\66\217\347\27\341\324\175\50\377\15\75\167\356\362\344\305\240\257\131\40\374\14\341\63\77\207\117\60\356\135"
  "\161\246\20\326\224\76\341\104\354\323\5\215\345\13\236\124\314\251\351\175\107\46\51\231\273\301\311\234\372\53\350\7"
  "\177\327\377\212\73\321\315\34\77\13\234\232\143\127\303\26\372\142\121\222\303\176\147\154\201\105\170\140\125\160\331\46"
  "\37\130\51\160\146\0\227\46\267\125\271\165\255\121\271\12\126\323\277\372\106\212\11\356\224\325\60\365\123\170\211\350"
  "\34\340\270\170\300\270\216\303\344\227\46\271\104\361\45\327\224\256\174\117\252\162\126\225\205\236\177\365\65\373\263\252"
  "\234\31\140\126\225\245\222\325\344\23\350\273\147\126\343\72\120\366\4\377\201\107\344\153\277\131\260\142\320\232\350\75"
  "\132\313\351\22\35\204\2\41\224\32\10\262\104\10\244\107\162\344\44\261\53\137\134\23\361\310\34\146\305\330\125\374"
  "\54\341\54\31\7\217\62\235\150\136\277\46\317\140\122\321\261\332\20\35\323\227\255\52\252\137\357\76\72\146\67\171"
  "\204\350\104\30\145\42\214\244\63\217\246\23\346\160\346\210\2\15\244\167\351\11\264\152\26\150\167\42\320\44\33\370"
  "\235\13\64\227\230\44\332\122\53\321\344\23\354\233\45\332\0\22\255\214\111\264\52\52\321\52\310\271\101\211\6\346"
  "\351\322\223\150\56\231\105\332\235\210\64\251\246\173\357\42\315\170\204\251\77\303\224\157\260\163\226\152\103\110\265\145"
  "\114\252\271\44\46\326\274\356\376\113\132\47\45\42\2\347\216\51\167\230\4\216\331\70\233\323\251\71\374\325\377\257"
  "\346\146\147\31\35\263\203\60\313\7\263\376\24\114\310\56\111\300\51\223\175\307\57\10\112\144\126\222\10\301\263\324"
  "\131\265\25\40\0\373\100\73\245\210\22\14\155\300\205\46\355\371\164\76\354\367\100\301\247\15\124\164\216\73\247\247"
  "\370\244\50\241\240\244\312\122\175\61\27\337\42\55\357\340\224\145\75\112\216\41\371\232\355\20\371\232\55\146\146\322"
  "\202\245\46\310\226\354\266\360\215\25\114\74\142\211\32\42\374\111\77\276\242\342\122\127\310\373\213\230\142\30\5\65"
  "\325\77\313\221\133\212\3\360\127\214\144\117\111\56\233\213\103\267\26\53\153\150\16\345\346\234\204\66\374\271\6\75"
  "\115\157\165\65\322\217\347\177\216\233\277\72\215\170\370\262\71\41\241\221\237\16\117\157\172\372\31\276\132\361\236\351"
  "\107\310\6\255\353\313\172\133\353\356\65\326\52\357\6\5\64\374\106\104\217\332\366\204\73\221\250\215\261\66\330\30"
  "\272\12\220\255\275\2\244\213\6\100\365\107\17\127\54\227\315\107\371\305\245\162\220\46\150\376\356\22\136\57\302\55"
  "\36\126\36\222\51\357\372\36\277\136\204\334\365\135\136\161\373\167\65\327\235\134\374\115\117\300\374\107\35\15\317\205"
  "\51\357\75\352\72\27\246\314\46\362\360\46\316\260\165\77\42\157\246\136\363\363\250\54\130\261\363\363\234\112\230\334"
  "\130\236\124\124\236\340\142\11\302\1\171\247\315\366\317\157\77\172\261\170\327\155\236\56\242\133\44\210\204\10\234\207"
  "\47\74\114\34\24\113\0\236\347\133\366\5\343\57\372\175\327\100\337\277\166\15\40\123\170\336\356\142\330\117\11\154"
  "\247\27\262\330\347\315\351\154\74\276\320\20\260\163\311\215\71\244\376\51\34\222\31\71\244\163\247\342\114\2\27\73"
  "\1\371\66\253\346\376\150\12\343\4\44\230\75\147\60\114\61\154\276\333\102\160\211\360\361\244\262\362\11\105\245\164"
  "\206\121\33\121\177\136\140\327\227\366\71\24\227\321\305\123\114\214\230\161\27\147\205\233\70\374\315\313\334\172\37\213"
  "\16\364\253\345\167\334\272\305\244\34\231\174\61\127\151\377\254\264\37\30\234\307\341\110\356\217\114\37\40\120\3\372"
  "\226\371\160\352\354\340\151\347\313\225\213\173\252\327\234\163\345\334\202\25\242\31\216\323\146\323\170\374\60\163\274\202"
  "\175\56\213\232\313\242\356\256\170\162\116\314\347\206\216\301\113\231\355\235\271\344\111\200\316\105\234\370\223\131\150\216"
  "\47\64\137\2\270\343\273\172\170\260\220\336\70\136\230\260\0\30\237\143\342\156\73\307\162\171\171\216\134\230\317\305"
  "\136\263\74\237\145\345\234\203\244\340\75\175\274\125\276\301\316\373\117\103\232\200\264\315\115\231\102\160\317\221\270\253"
  "\345\354\256\316\127\143\114\314\264\50\155\246\205\76\62\56\337\140\347\57\147\132\200\233\127\304\334\274\62\342\303\171"
  "\275\360\35\376\303\370\65\340\230\361\62\265\42\360\152\374\42\160\113\145\64\26\101\257\57\225\100\113\225\2\331\244"
  "\62\176\267\251\172\221\227\367\145\256\271\176\156\233\366\161\325\154\345\65\374\227\56\171\20\337\124\50\265\75\234\76"
  "\257\316\176\337\363\212\326\143\113\175\325\113\227\274\351\44\105\113\253\66\350\171\375\331\311\163\0\320\157\256\67\155"
  "\360\203\135\57\31\302\352\252\363\343\247\0\356\127\377\75\236\177\374\2\255\335\40\225\126\303\277\314\123\321\322\253"
  "\45\57\275\162\16\172\322\67\125\132\25\127\324\136\125\320\123\103\17\314\71\115\240\7\361\233\102\317\155\137\23\112"
  "\253\364\147\224\165\365\64\316\176\365\102\343\327\350\202\264\114\230\42\303\131\61\173\210\274\217\366\103\244\300\254\217"
  "\344\251\363\43\177\136\124\373\226\70\7\117\252\265\167\155\173\134\75\212\246\10\337\221\224\5\25\232\21\235\156\104"
  "\0\252\60\343\217\27\355\103\261\376\244\221\323\333\31\141\154\256\137\211\223\300\364\175\132\336\271\117\210\33\117\275"
  "\101\202\130\273\130\230\113\374\171\340\151\370\313\5\361\264\116\57\304\300\35\250\200\74\15\1\120\42\324\350\14\170"
  "\210\224\57\222\17\61\264\360\224\173\310\105\32\56\147\151\170\317\322\160\116\174\266\113\303\72\46\15\227\121\151\270"
  "\204\33\125\5\332\35\260\213\34\345\223\327\334\15\6\361\10\174\60\277\35\245\340\3\0\217\262\222\357\137\350\356"
  "\341\307\370\303\1\313\141\161\31\62\312\320\265\207\334\364\341\104\41\304\312\271\307\176\163\34\172\355\166\133\43\212"
  "\362\347\125\244\13\340\227\257\17\40\137\20\346\305\165\141\205\250\343\301\14\271\211\5\57\303\66\273\345\162\162\26"
  "\57\273\12\236\220\161\374\221\27\274\72\346\363\323\336\42\223\145\27\310\165\101\200\63\121\205\62\161\63\145\301\215"
  "\300\320\342\173\52\157\337\13\146\263\40\362\122\7\355\52\150\227\101\273\10\332\171\320\16\341\247\101\333\5\355\304"
  "\157\247\313\240\135\7\355\52\150\227\101\273\10\332\171\320\316\202\166\32\264\135\320\16\346\347\226\101\273\16\332\125"
  "\320\56\203\166\21\264\363\70\301\330\111\210\220\75\174\263\303\147\221\312\342\325\302\46\170\231\251\106\10\113\35\4"
  "\364\322\252\104\5\41\113\20\202\323\101\100\276\252\153\35\4\224\136\113\345\34\12\204\120\351\60\231\43\204\245\156"
  "\16\10\301\45\272\315\160\313\10\10\247\133\107\21\1\221\352\100\224\21\20\231\16\104\25\1\221\353\160\21\103\247"
  "\222\277\222\10\210\122\67\213\30\56\152\35\210\310\216\70\45\223\126\21\20\112\272\160\21\20\251\156\26\145\4\104"
  "\241\3\221\105\100\50\167\244\216\200\120\362\172\212\40\322\104\7\42\213\200\120\241\263\327\174\342\31\324\236\251\222"
  "\33\14\111\144\271\301\303\206\340\253\245\61\103\12\243\133\340\334\341\7\360\306\103\346\41\246\30\10\61\365\70\210"
  "\321\342\345\255\150\51\27\212\221\143\0\52\151\305\131\114\7\316\13\167\245\211\327\162\136\53\365\132\231\327\312\275"
  "\126\341\265\112\257\125\171\255\332\153\171\163\311\22\257\345\274\126\352\265\62\257\225\173\255\302\153\225\136\253\322\171"
  "\64\61\57\351\30\107\254\210\11\140\45\133\250\102\206\121\62\26\314\374\354\73\53\201\361\237\147\101\73\15\332\56"
  "\150\47\327\30\206\347\213\42\66\167\304\250\143\337\47\104\165\63\0\262\300\223\254\276\307\270\300\146\167\316\372\51"
  "\317\162\223\27\341\120\201\206\105\160\356\337\65\317\322\233\25\372\343\30\376\361\331\137\37\246\306\327\157\236\60\207"
  "\270\217\175\166\121\312\163\64\174\26\241\45\77\137\41\57\346\363\253\373\77\277\232\37\262\266\234\137\345\171\264\160"
  "\66\232\107\343\165\313\247\40\102\244\365\276\171\350\335\234\142\140\360\72\17\102\150\265\13\332\211\337\256\226\101\273"
  "\16\332\125\320\56\203\166\21\264\303\54\253\62\150\27\101\73\374\175\26\264\323\240\355\202\166\260\236\174\31\264\353"
  "\240\135\221\300\35\121\352\125\101\302\166\354\373\212\4\355\330\367\113\374\136\143\25\325\44\140\307\276\57\235\321\52"
  "\133\222\140\35\133\177\156\264\352\152\32\250\43\10\310\111\230\216\256\240\44\101\72\12\40\266\204\114\103\3\11\215"
  "\256\21\34\224\44\44\105\1\144\44\40\105\1\44\64\34\105\350\240\42\221\44\72\203\202\304\221\50\0\317\75\50"
  "\75\5\133\171\255\332\153\105\64\354\31\131\142\320\302\340\163\14\327\60\0\53\345\3\355\210\265\51\77\66\340\363"
  "\346\363\341\364\317\237\333\335\176\37\203\204\66\340\333\13\226\64\253\124\54\316\61\253\265\266\317\70\76\260\43\3"
  "\57\157\65\160\112\157\336\271\325\310\31\33\331\335\152\344\234\215\234\336\150\344\54\273\325\103\256\160\65\311\370\251"
  "\52\113\105\344\332\26\262\25\27\211\105\124\136\12\262\230\323\213\137\350\252\63\345\233\141\44\243\313\355\24\370\266"
  "\174\243\342\47\155\42\341\234\274\144\234\23\121\354\144\233\44\220\253\51\153\266\341\167\231\115\23\277\113\52\23\313"
  "\33\341\227\134\374\137\70\277\246\51\131\14\275\15\315\346\142\322\226\214\376\62\233\324\60\74\324\243\17\177\52\60"
  "\274\10\40\341\106\155\11\6\217\73\332\7\343\373\255\230\212\277\375\67\314\144\2\112\220\17\364\147\72\217\36\317"
  "\144\213\201\123\273\345\124\43\316\65\171\300\65\305\342\52\273\14\147\211\170\340\367\37\334\116\102\305\127\362\210\75"
  "\112\2\170\24\131\55\276\145\221\211\377\10\255\134\132\12\314\352\166\133\205\373\172\4\334\247\67\143\76\300\175\42"
  "\370\205\226\223\326\25\306\144\364\150\16\20\330\322\163\56\224\105\245\257\30\53\162\66\51\154\152\211\51\354\330\40"
  "\42\31\4\77\367\177\100\67\364\163\55\134\244\244\105\41\135\53\44\105\312\276\32\173\105\166\123\144\174\211\223\154"
  "\21\353\330\243\344\317\35\223\74\313\267\134\157\166\313\62\345\104\175\262\51\337\140\347\275\236\155\212\254\215\326\42"
  "\173\335\75\265\11\303\370\142\260\366\305\340\162\26\203\263\30\34\121\14\326\13\273\210\273\245\64\322\137\226\53\337"
  "\140\347\375\113\243\170\55\260\164\367\255\132\132\70\37\273\244\261\37\22\51\305\25\361\211\1\41\123\316\250\340\270"
  "\43\351\143\12\153\234\233\77\204\73\337\176\320\200\333\201\7\222\145\342\265\234\327\112\275\126\346\265\162\257\125\170"
  "\255\362\212\222\274\17\253\100\113\227\141\76\112\26\264\323\240\355\202\166\342\267\313\145\320\256\215\371\21\113\143\176"
  "\101\225\32\317\326\53\147\75\133\257\215\107\343\125\146\74\32\227\115\72\375\330\161\241\232\332\153\55\245\45\237\141"
  "\335\160\234\244\20\172\4\167\265\16\100\52\255\70\42\22\15\74\151\351\315\72\64\15\54\317\27\103\267\314\72\256"
  "\14\364\366\6\276\166\334\300\62\276\2\76\371\255\236\26\334\241\42\6\115\303\264\327\360\212\10\131\133\251\305\243"
  "\67\361\331\342\332\0\35\64\24\304\216\275\335\374\162\132\35\37\166\145\316\254\73\231\231\41\125\375\353\53\31\347"
  "\124\27\126\271\327\52\274\226\377\113\137\64\324\136\313\277\24\46\361\132\156\252\114\77\60\177\243\7\363\317\257\304"
  "\337\66\42\376\347\22\21\157\273\165\123\62\226\226\106\56\20\204\350\334\125\371\10\274\244\154\330\213\41\144\335\101"
  "\43\135\50\266\326\160\325\361\66\270\347\170\33\275\344\170\173\344\327\27\257\216\13\162\177\261\134\161\374\322\76\155"
  "\344\147\376\252\72\150\27\257\122\71\155\266\177\172\327\65\357\232\207\355\156\37\13\257\120\67\15\356\115\225\233\136"
  "\200\212\345\136\350\310\45\336\364\2\131\270\117\106\276\43\173\24\154\21\354\320\164\167\104\376\372\115\226\313\137\17"
  "\37\356\143\143\302\235\211\156\315\207\247\355\324\271\247\233\342\166\163\2\60\135\367\245\76\301\377\151\327\174\354\344"
  "\373\111\264\362\273\332\302\46\334\303\346\272\115\174\74\74\65\347\273\332\127\354\353\327\30\331\252\156\267\233\156\273"
  "\37\332\167\270\341\317\241\306\173\346\52\117\177\143\377\104\164\24\101\105\210\11\104\204\176\341\23\122\5\174\375\200"
  "\0\316\373\166\224\114\123\342\22\114\65\200\52\273\234\244\350\273\3\301\106\36\32\71\74\235\377\175\55\202\65\366"
  "\250\310\156\375\65\376\312\210\340\15\36\316\200\331\167\100\350\231\24\276\147\106\74\164\201\112\137\77\210\236\172\376"
  "\50\300\257\37\132\355\61\1\305\164\363\264\337\17\215\147\76\254\210\277\233\154\357\366\310\336\111\311\56\75\234\102"
  "\367\263\25\15\305\167\113\357\236\13\160\106\36\73\313\321\54\46\341\341\327\366\1\23\357\211\276\66\36\357\344\332"
  "\277\276\257\47\2\340\62\157\73\317\343\375\336\74\315\1\342\132\164\26\260\117\50\347\267\373\247\366\123\47\275\105"
  "\14\311\151\273\122\62\235\66\317\276\204\350\72\55\62\342\262\76\206\137\310\233\122\242\240\367\253\217\55\165\133\332"
  "\363\352\164\176\220\131\274\110\27\246\154\104\264\10\10\370\165\77\5\210\335\142\36\14\115\102\340\120\35\201\372\226"
  "\54\372\224\274\17\51\141\160\170\234\107\233\203\365\244\172\236\7\36\255\123\250\164\354\123\4\76\271\271\41\335\75"
  "\371\277\41\345\345\377\140\240\166\370\114\115\341\160\36\304\225\206\356\15\43\261\300\250\75\53\277\347\372\117\344\61"
  "\276\265\310\37\365\273\53\112\121\276\317\144\220\117\133\362\266\23\101\40\212\111\330\123\56\33\266\367\316\370\272\147"
  "\177\244\21\135\34\127\331\315\371\260\172\20\227\360\147\250\355\146\363\161\165\336\75\157\202\357\126\155\320\163\354\360"
  "\147\127\370\323\174\201\161\60\253\41\316\112\3\333\24\366\272\100\331\165\134\5\66\144\4\333\73\124\174\261\151\154"
  "\265\272\145\153\57\35\44\327\370\203\312\365\112\154\271\171\326\367\365\274\23\227\247\210\100\366\104\174\212\200\70\156"
  "\270\11\60\376\363\343\130\267\136\123\247\115\304\163\32\25\317\251\147\102\241\325\52\73\227\57\14\106\75\337\266\70"
  "\31\352\267\156\246\223\324\106\47\171\204\116\274\136\174\266\206\323\104\171\331\76\43\350\110\256\34\41\111\163\246\155"
  "\44\201\207\23\10\27\112\366\42\24\106\366\62\212\135\250\133\374\24\134\40\146\175\302\206\343\22\106\230\271\260\10"
  "\220\360\33\246\155\260\102\262\324\26\332\230\245\130\74\137\334\160\237\312\370\373\75\157\61\177\135\17\313\52\206\335"
  "\141\57\23\172\106\171\344\131\234\132\173\330\207\377\240\107\336\344\20\123\276\305\150\272\61\76\354\56\347\364\360\134"
  "\41\236\13\45\317\163\151\214\226\340\344\360\120\43\36\262\301\361\120\117\37\17\313\370\311\210\236\357\360\37\60\276"
  "\311\43\257\54\146\112\60\307\215\172\330\63\70\335\14\216\45\171\64\164\177\150\76\336\46\26\372\274\332\77\155\176"
  "\172\360\123\246\365\243\243\177\13\323\357\352\122\361\375\216\255\164\4\41\124\54\36\221\224\175\241\136\314\357\307\37"
  "\3\325\212\117\102\352\2\370\350\375\12\170\56\46\22\333\217\135\243\301\126\241\100\314\2\254\100\156\310\240\362\225"
  "\60\32\212\4\234\320\320\156\74\71\374\75\34\343\341\54\234\31\263\43\117\233\317\17\117\221\114\5\100\113\117\226"
  "\374\242\211\321\43\206\62\214\114\22\127\3\33\1\327\215\360\130\107\136\153\307\223\231\307\315\351\262\300\233\341\15"
  "\36\231\220\216\176\222\210\41\340\170\341\7\242\275\364\124\270\336\75\77\74\1\11\310\227\160\364\16\300\307\163\174"
  "\44\21\202\34\321\217\252\253\71\43\352\254\156\334\52\214\26\32\15\1\70\354\64\27\237\241\345\160\173\363\241\314"
  "\257\63\37\312\174\142\346\303\154\76\224\371\370\346\103\364\75\260\166\104\243\2\60\6\217\223\111\157\263\31\307\16"
  "\51\363\320\16\301\342\370\331\62\231\55\223\261\55\223\62\267\133\46\112\216\233\215\231\21\214\31\24\63\323\67\157"
  "\60\263\373\170\33\273\46\260\114\232\210\11\242\313\200\301\175\323\36\171\66\257\325\104\100\214\231\344\340\337\133\202"
  "\242\76\3\106\237\227\270\356\150\371\226\126\364\326\140\35\113\327\347\63\257\55\330\175\14\354\345\146\355\267\277\372"
  "\315\175\363\377\3\123\331\157\156\375\346\6\354\161\350\70\256\202\21\273\322\307\157\142\42\60\343\17\201\21\337\6"
  "\140\127\155\360\203\125\60\363\277\25\116\300\164\323\60\207\167\106\312\174\24\147\204\33\256\75\61\136\365\364\121\112"
  "\355\315\353\14\215\126\172\243\345\125\65\367\251\256\273\64\77\117\324\131\134\11\53\375\342\163\373\112\337\335\357\371"
  "\231\3\332\240\200\356\305\13\232\242\160\155\57\65\103\372\21\136\170\376\52\134\262\312\275\327\215\317\55\331\354\350"
  "\340\145\356\25\357\345\365\45\2\51\63\112\150\237\116\17\117\146\317\23\17\124\71\153\61\254\164\326\143\334\260\37"
  "\362\100\26\117\100\371\304\207\74\10\105\51\302\307\147\347\216\200\123\216\151\133\62\41\100\174\41\243\257\356\317\355"
  "\277\377\353\370\57\227\244\331\157\377\373\77\177\163\177\126\313\252\134\146\56\333\374\53\113\352\337\376\363\377\310\307"
  "\37\317\44\215\305\272\56\103\22\35\102\302\313\55\326\155\231\302\235\356\221\37\101\42\253\374\372\246\223\313\257\231"
  "\134\254\74\12\251\21\346\5\323\27\202\125\126\215\223\370\16\161\375\320\13\377\310\323\370\31\60\64\114\210\377\16"
  "\163\357\214\313\341\213\127\6\164\365\111\275\230\54\41\236\65\160\214\173\130\210\15\63\155\154\71\260\373\165\47\325"
  "\105\56\55\57\363\124\134\375\335\52\320\242\30\175\166\372\25\22\341\176\204\0\316\303\160\55\261\375\50\6\15\263"
  "\22\326\43\321\107\30\132\376\32\214\334\375\353\325\250\30\376\226\17\312\274\237\114\237\176\34\233\174\142\252\246\0"
  "\306\344\213\42\106\145\362\147\362\357\377\72\376\53\371\146\121\46\142\103\202\203\302\121\205\231\332\304\352\304\125\41"
  "\22\15\201\3\215\347\56\114\270\337\175\350\256\47\71\236\17\375\311\15\172\17\150\150\362\23\21\265\237\155\240\64"
  "\127\371\247\153\6\110\354\40\115\106\104\33\174\224\271\362\331\310\77\114\227\220\360\206\13\70\114\130\53\116\23\340"
  "\162\224\370\165\36\364\6\246\370\275\50\341\333\235\213\353\36\300\126\27\222\344\65\115\43\40\21\235\54\275\24\321"
  "\161\54\164\50\107\365\175\123\346\314\317\355\371\371\254\71\24\12\70\227\76\100\102\242\305\1\34\13\223\150\252\30"
  "\141\261\151\151\311\126\254\276\354\102\271\52\120\140\65\64\273\224\103\262\113\345\117\266\176\73\273\160\323\37\231\207"
  "\270\173\273\150\350\230\134\7\122\17\272\315\245\214\21\64\140\137\57\7\266\121\201\14\22\300\57\365\1\174\104\130"
  "\134\173\301\361\212\301\30\325\233\116\176\340\133\46\57\64\115\165\261\36\254\234\130\344\1\363\30\254\341\204\237\72"
  "\0\122\70\231\352\327\46\0\160\127\343\126\212\175\342\174\134\133\161\77\211\224\311\174\7\276\115\5\31\136\120\210"
  "\177\265\357\131\74\314\330\237\76\43\170\350\221\237\5\214\103\176\12\4\114\176\205\43\140\302\31\156\76\265\203\371"
  "\110\372\63\15\45\275\311\364\24\207\213\353\303\371\106\24\71\204\210\173\113\202\143\142\246\145\334\6\75\250\30\47"
  "\254\276\276\25\36\34\135\333\225\162\245\127\312\176\234\67\361\2\275\301\103\201\316\55\256\231\145\212\263\224\117\344"
  "\160\35\73\225\141\326\215\276\14\25\63\145\145\7\321\374\227\372\50\34\106\357\332\360\101\131\71\314\112\202\234\260"
  "\31\2\224\344\311\211\365\275\24\203\322\157\10\31\220\15\112\211\175\371\76\210\144\71\160\155\274\375\360\151\174\371"
  "\352\154\342\221\154\12\250\60\330\162\321\132\114\73\305\303\117\370\347\61\254\61\140\5\44\126\230\26\67\345\347\203"
  "\351\311\336\305\74\37\211\317\107\342\323\241\337\351\220\154\136\337\222\144\207\323\201\70\357\373\43\125\241\13\61\334"
  "\63\364\13\310\225\361\261\247\35\154\33\263\177\65\24\354\262\300\327\310\371\4\44\226\315\155\235\333\4\225\143\34"
  "\230\113\330\377\116\345\340\253\273\303\47\214\161\156\227\212\367\1\255\114\132\152\223\221\253\64\330\275\362\227\310\347"
  "\302\350\275\43\2\241\270\112\40\134\315\356\145\300\356\325\342\55\322\105\173\224\372\367\40\157\353\374\315\345\220\102"
  "\77\353\145\325\337\32\131\65\313\252\152\130\131\125\210\74\202\126\271\270\215\357\211\51\143\313\170\212\214\275\76\373"
  "\176\150\0\13\107\240\47\303\167\362\211\45\171\337\350\22\171\134\373\127\74\54\131\100\20\305\351\360\1\101\345\25"
  "\317\362\311\374\26\215\133\106\323\25\244\273\157\325\322\232\123\206\7\56\331\137\115\263\202\232\24\114\313\25\13\120"
  "\262\234\372\75\237\127\315\171\327\266\335\2\312\334\353\375\236\340\107\152\242\273\65\4\35\335\270\364\136\243\123\344"
  "\362\243\355\266\107\205\377\331\357\256\237\55\364\146\357\276\104\132\46\226\100\217\203\236\24\172\62\350\311\157\132\174"
  "\235\327\67\53\276\276\277\242\304\141\347\365\307\53\23\373\343\265\231\355\317\43\5\25\311\56\32\156\146\301\124\174"
  "\121\23\162\301\205\101\344\17\137\140\260\247\241\15\61\165\206\277\113\133\131\61\75\112\235\214\313\264\65\53\307\5"
  "\277\233\313\132\55\257\110\53\56\322\170\331\73\51\226\117\223\274\142\37\256\232\150\22\66\246\321\245\157\137\307\177"
  "\23\160\136\44\131\261\134\226\151\225\125\111\276\54\160\32\62\250\130\6\174\73\305\134\270\352\156\77\372\243\336\212"
  "\210\171\344\51\253\100\212\347\123\342\365\22\111\212\370\224\365\352\256\344\170\31\31\274\113\141\64\24\371\260\252\200"
  "\315\62\124\267\10\363\262\360\172\343\271\112\22\207\305\147\202\32\16\367\323\376\30\201\375\72\103\226\61\324\232\356"
  "\55\44\117\167\117\356\345\55\303\225\176\212\253\372\130\302\124\373\52\165\223\235\234\376\165\173\172\42\177\107\244\225"
  "\55\364\321\72\221\7\42\63\31\135\375\152\170\315\161\142\11\14\342\122\153\344\230\43\336\360\136\65\331\25\103\11"
  "\147\6\71\122\224\203\337\41\15\24\157\317\157\264\125\132\310\47\372\347\260\371\276\161\12\300\217\20\266\71\261\255"
  "\17\102\251\74\74\64\3\266\333\301\154\231\312\343\250\222\25\330\135\66\316\313\53\253\100\211\317\41\1\70\30\132"
  "\20\247\273\327\123\160\206\161\227\0\351\171\37\164\51\75\277\272\63\25\20\234\314\121\100\76\157\116\147\131\164\337"
  "\15\34\50\163\141\213\343\354\42\237\10\5\352\227\334\207\163\60\262\144\13\50\31\6\347\325\372\270\5\366\304\67"
  "\224\325\332\261\120\222\136\376\254\124\170\217\234\63\254\324\15\323\53\214\144\55\350\66\113\70\331\236\341\141\276\17"
  "\155\134\56\70\71\332\257\202\47\360\247\162\365\273\114\152\340\253\336\345\227\126\205\107\156\202\27\170\60\20\306\262"
  "\270\32\200\357\244\254\234\200\37\337\201\245\345\351\244\304\233\253\175\373\222\356\304\242\347\257\114\226\143\271\276\44"
  "\44\352\364\341\126\24\23\250\43\263\137\324\34\312\106\64\207\62\15\235\302\147\323\327\302\254\60\224\342\105\140\51"
  "\370\314\376\224\372\54\7\171\345\246\375\21\365\73\16\2\272\4\147\146\250\160\275\173\164\71\103\101\360\324\337\310"
  "\236\76\366\323\51\74\241\75\175\64\145\123\170\141\173\372\150\312\337\343\3\334\166\304\332\237\347\36\72\377\365\271"
  "\155\216\247\135\163\336\52\223\140\343\71\257\337\237\325\372\75\114\204\45\331\254\176\316\153\337\45\231\261\322\325\273"
  "\250\273\166\167\150\374\156\257\51\311\271\2\11\241\170\75\57\357\210\113\217\274\41\32\164\375\276\224\337\111\247\113"
  "\242\275\316\357\335\7\323\10\376\372\273\313\203\165\264\17\373\375\234\10\13\75\71\364\24\320\123\102\117\5\75\65"
  "\364\300\234\323\4\172\20\277\51\364\144\320\223\103\117\1\75\45\364\124\320\123\103\17\314\71\113\156\232\50\354\112"
  "\143\242\60\50\203\53\263\1\137\144\213\371\71\42\21\364\250\21\320\307\223\167\62\340\245\277\346\151\277\7\340\335"
  "\37\324\167\365\223\272\112\134\32\277\314\377\206\300\105\143\127\334\246\265\17\175\267\365\77\54\41\26\110\104\372\10"
  "\52\311\132\11\357\310\373\5\344\346\41\362\332\225\364\335\152\272\302\150\224\347\305\252\212\347\270\144\160\103\264\64"
  "\341\274\336\245\101\333\5\355\304\157\57\361\270\37\123\3\206\146\31\244\353\174\301\253\265\344\127\22\4\226\314\270"
  "\4\41\24\72\10\56\342\36\251\40\54\21\200\156\12\54\304\110\0\310\176\341\15\11\325\102\175\151\314\341\24\47"
  "\125\302\163\344\122\355\333\362\34\111\6\156\56\134\224\134\353\336\73\261\143\247\214\140\247\234\52\166\226\127\142\47"
  "\37\12\73\100\254\322\67\65\354\210\344\121\104\106\214\350\1\152\225\276\11\242\307\135\53\171\312\241\360\3\364\52"
  "\175\23\304\317\245\147\115\371\307\101\43\42\161\33\117\73\220\47\0\310\34\44\44\64\270\71\200\21\245\56\300\260"
  "\336\74\76\364\307\151\206\160\265\300\225\222\352\135\174\145\142\123\216\342\240\344\327\76\356\163\251\13\70\107\150\25"
  "\153\32\310\364\201\205\276\250\237\5\260\153\115\122\15\364\5\175\173\276\207\360\71\176\241\52\4\104\131\57\235\143"
  "\271\22\327\370\24\22\216\34\207\230\113\116\314\274\256\323\56\370\235\143\153\3\70\362\15\164\336\174\113\327\247\303"
  "\361\56\5\252\120\337\54\124\205\74\151\26\233\375\275\56\122\167\300\245\2\236\253\2\153\244\343\113\73\214\131\106"
  "\156\177\134\6\355\72\150\127\101\273\14\332\105\320\316\307\212\175\164\53\101\241\30\223\124\331\53\301\213\42\2\243"
  "\124\302\250\42\60\152\45\214\34\141\70\355\74\112\204\221\246\32\30\262\373\160\223\221\114\221\233\55\305\140\36\11"
  "\216\51\235\67\346\244\341\311\27\51\344\255\272\277\160\351\140\30\6\357\107\72\107\220\125\102\153\131\364\256\77\152"
  "\5\245\365\140\230\50\143\230\50\357\202\326\362\267\323\132\71\234\225\211\341\51\351\374\171\244\126\376\4\261\126\307"
  "\60\61\112\50\12\21\40\333\62\242\324\301\120\223\164\216\216\200\172\174\12\110\321\41\226\316\321\21\260\134\304\377"
  "\204\326\142\232\370\355\42\150\347\313\240\135\7\355\360\312\217\62\150\27\101\73\17\332\131\320\116\203\166\10\77\230"
  "\137\272\14\332\165\320\256\202\166\31\264\213\240\235\7\355\20\137\151\320\166\143\131\307\375\110\160\270\217\64\347\56"
  "\33\203\71\36\314\225\205\16\104\32\1\261\324\201\310\320\314\257\22\45\10\207\40\234\22\104\21\111\354\126\342\2"
  "\101\54\53\45\210\32\101\150\147\121\41\10\355\216\224\21\207\47\121\116\303\105\140\150\367\44\215\300\110\225\60\222"
  "\10\214\114\11\43\217\300\320\62\112\144\55\316\51\141\224\21\30\112\174\344\165\4\106\241\204\341\42\60\264\224\236"
  "\41\214\124\113\143\271\350\265\136\123\211\136\113\235\327\112\275\126\346\265\162\257\125\170\255\322\153\335\64\353\3\361"
  "\40\113\126\125\330\215\62\307\72\360\156\311\363\271\256\304\4\306\125\273\31\165\322\302\160\344\166\201\172\12\163\135"
  "\326\327\114\65\45\123\105\261\165\371\3\152\36\377\341\52\250\141\100\373\230\304\51\307\247\114\205\57\234\135\341\0"
  "\215\264\32\321\63\364\375\274\221\46\202\247\41\332\52\110\102\135\231\236\272\364\227\243\352\346\224\342\307\123\42\210"
  "\311\51\7\241\25\256\35\170\70\2\260\77\302\261\25\271\270\25\56\353\35\371\362\146\305\25\250\361\163\263\256\74"
  "\46\16\137\150\104\63\22\200\221\337\302\45\43\366\45\355\317\60\254\50\60\370\360\11\176\52\347\174\266\272\256\346"
  "\174\130\75\354\17\315\307\376\177\342\331\350\310\161\44\364\64\56\261\163\236\107\142\377\175\71\2\271\313\130\306\43"
  "\141\200\25\47\156\72\74\41\144\66\131\102\264\67\45\317\41\50\263\4\5\112\320\76\66\225\342\325\270\212\325\271"
  "\264\176\67\313\163\45\256\157\374\111\241\125\37\227\25\56\31\121\130\270\304\42\55\0\230\136\134\134\276\135\213\317"
  "\167\372\2\3\167\134\320\143\267\344\206\47\220\361\255\103\334\316\173\65\123\356\140\47\125\107\160\311\254\273\307\100"
  "\163\132\24\70\327\73\123\344\162\310\126\144\174\265\23\125\353\156\114\265\356\246\316\215\44\322\111\222\4\242\345\302"
  "\343\325\13\373\47\46\265\327\132\216\163\326\121\26\343\107\153\337\211\206\332\362\110\324\240\324\56\357\52\306\11\235"
  "\122\63\124\223\113\347\350\324\234\371\55\67\16\65\127\311\170\324\74\123\263\375\145\72\45\201\103\262\262\164\216\117"
  "\340\251\327\312\274\126\356\265\212\221\104\371\162\362\7\23\225\43\347\22\343\63\352\314\250\31\166\271\221\216\251\262"
  "\367\113\261\162\317\363\360\162\16\5\206\164\216\56\347\312\5\257\41\337\53\352\155\371\73\231\222\134\352\277\0\223"
  "\325\210\227\75\366\150\57\136\274\314\75\374\316\360\54\275\327\127\326\263\72\372\306\221\164\367\255\112\132\123\73\203"
  "\270\257\55\321\263\26\162\323\322\343\246\74\31\215\233\146\156\312\223\10\67\371\335\262\101\212\0\6\324\326\110\347"
  "\350\232\42\167\213\311\124\64\23\133\311\113\173\142\277\275\250\345\333\363\251\351\250\5\37\353\354\57\46\35\72\101"
  "\12\141\342\124\165\77\27\176\204\165\342\132\204\54\207\327\363\171\352\113\246\154\301\227\43\235\234\226\161\65\50\242"
  "\146\21\225\105\104\224\337\55\73\205\257\137\346\376\26\106\114\330\243\336\372\116\24\271\227\326\67\332\15\216\7\160"
  "\41\374\105\373\176\236\174\47\330\66\44\174\36\261\107\101\211\212\315\273\113\306\210\275\366\351\167\13\375\17\141\250"
  "\345\245\317\113\325\54\16\177\302\256\127\221\135\367\273\145\247\24\26\33\156\246\164\216\156\261\325\13\122\173\241\213"
  "\45\145\331\233\57\324\212\75\7\2\330\353\157\167\267\77\37\162\117\231\22\230\315\302\237\65\53\247\237\6\163\57"
  "\333\103\66\142\22\347\346\124\144\141\330\115\72\107\27\131\113\136\165\375\113\334\164\216\11\2\322\71\372\256\24\311"
  "\120\107\134\363\25\366\31\340\122\72\307\336\130\151\365\177\212\132\104\336\71\50\26\46\47\366\0\55\156\10\177\62"
  "\37\241\201\226\1\270\304\111\223\305\217\113\46\200\10\373\13\106\333\376\371\242\16\143\233\257\307\303\351\374\333\177"
  "\374\350\370\217\357\203\313\323\104\307\330\123\105\176\337\352\270\40\117\25\311\153\106\57\355\323\106\176\346\257\253\203"
  "\166\121\323\106\37\24\331\356\366\261\223\100\32\346\173\131\114\304\22\220\115\303\27\237\140\162\335\122\310\255\247\150"
  "\160\310\167\144\237\302\155\302\135\232\360\256\310\237\277\261\267\134\370\171\370\200\136\44\10\253\367\277\167\155\270\171"
  "\155\174\367\76\74\155\47\317\144\335\34\267\233\23\300\351\272\357\144\257\32\330\254\346\312\335\172\74\74\65\347\167"
  "\274\201\320\327\57\350\75\156\342\163\270\207\317\104\134\262\347\366\336\235\174\343\50\332\2\216\270\346\327\43\155\42"
  "\252\232\243\243\5\164\50\205\64\307\307\304\244\52\107\111\203\70\261\313\102\216\247\311\11\57\216\253\323\246\131\175"
  "\336\254\316\202\252\227\236\20\123\207\375\172\273\366\221\320\165\35\127\347\117\176\147\263\371\262\135\103\127\377\73\246"
  "\74\66\247\323\341\164\101\135\364\303\103\357\217\31\274\326\57\270\351\120\362\361\257\356\377\320\317\371\61\353\130\57"
  "\200\207\376\50\170\371\303\227\125\273\173\150\233\325\261\375\164\70\77\34\117\233\347\335\346\213\373\353\333\367\17\337"
  "\161\15\376\163\217\7\136\360\363\226\130\261\22\270\170\146\375\207\315\341\257\376\177\221\267\75\112\43\117\51\160\172"
  "\14\251\121\150\21\11\117\115\145\335\307\235\27\273\135\373\206\112\367\265\364\362\207\127\337\303\53\233\55\304\147\10"
  "\327\4\257\20\154\37\233\363\376\257\155\347\364\167\204\272\17\277\350\50\125\160\211\4\101\150\21\370\210\275\263\234"
  "\203\371\241\230\146\67\216\141\232\301\52\265\361\101\130\262\22\126\56\260\144\321\276\364\66\204\126\330\363\47\125\251"
  "\227\4\312\271\20\201\360\371\360\354\11\204\256\15\2\341\52\306\217\360\367\351\127\140\367\201\170\235\162\20\136\61"
  "\67\76\21\135\16\160\32\70\70\124\142\202\266\247\146\327\221\351\137\117\315\176\327\374\275\72\3\316\116\372\154\40"
  "\177\257\11\246\62\147\253\76\120\14\125\225\70\224\55\247\307\260\47\270\5\247\317\35\260\325\31\335\165\325\42\213"
  "\174\40\162\46\245\274\247\201\245\344\261\267\352\104\112\176\157\207\122\262\215\110\105\361\222\305\10\227\237\135\27\33"
  "\50\3\345\214\102\211\156\2\232\321\375\12\0\324\346\204\250\153\7\110\271\153\371\101\236\211\322\133\354\41\336\3"
  "\160\254\307\41\150\13\154\277\234\166\347\215\74\176\5\244\135\137\7\346\370\164\176\4\50\20\6\322\103\101\106\220"
  "\375\304\276\161\161\343\222\267\255\12\330\262\373\317\177\1";

/* lib/math.wo (DEFLATEd, org. size 168474) */
static unsigned char file_l_37[14254] =
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
  "\156\375\163\323\254\256\207\13\346\177\355\175\351\162\42\71\323\356\255\114\174\303\131\47\160\150\57\351\54\77\317\276"
  "\277\361\375\357\140\51\332\330\156\114\3\155\3\127\177\0\173\234\123\225\310\217\220\105\65\355\301\357\62\243\242\112"
  "\312\114\345\256\224\224\244\34\322\345\224\176\170\301\75\50\351\275\363\106\127\112\125\106\130\55\111\110\211\63\23\337"
  "\77\114\14\102\164\72\233\20\226\207\346\331\121\124\132\127\112\150\347\255\251\52\353\105\165\146\24\147\203\31\241\170"
  "\150\236\256\342\177\254\111\127\347\352\340\37\353\116\246\173\72\151\40\302\40\346\140\30\53\264\15\301\251\112\127\302"
  "\4\373\56\14\202\36\314\152\322\36\134\212\111\10\361\14\75\56\276\15\36\32\223\164\170\362\21\126\334\265\37\230"
  "\275\147\246\226\36\44\261\322\162\347\15\355\134\242\331\327\123\371\347\166\75\155\271\3\364\200\271\3\27\151\346\17"
  "\70\44\232\171\146\216\371\227\324\33\201\152\225\121\336\323\323\365\343\202\141\310\200\240\356\31\373\305\137\132\376\30"
  "\276\147\274\265\244\47\55\352\304\100\142\166\262\62\136\73\123\105\174\26\62\247\301\6\355\4\375\362\165\265\37\16"
  "\160\342\236\17\207\323\25\111\314\353\203\63\353\356\76\127\336\76\135\131\140\371\252\353\272\262\346\313\162\7\332\160"
  "\166\34\223\327\346\154\217\13\41\166\370\255\51\161\367\115\361\272\135\267\345\357\327\23\277\154\351\73\42\174\214\25"
  "\105\223\345\227\374\5\316\314\115\354\357\123\35\123\202\5\202\7\4\52\303\3\347\263\113\134\111\66\53\362\332\204"
  "\70\173\55\157\304\137\377\346\177\130\363\333\277\376\367\277\311\33\57\244\321\301\333\372\17\351\176\373\367\377\206\276"
  "\374\366\343\201\21\15\161\127\111\116\42\205\225\36\10\235\306\44\326\60\35\333\140\305\373\70\205\357\343\14\207\247"
  "\60\76\133\203\361\70\155\206\147\355\361\373\172\77\255\364\364\141\365\206\357\344\30\67\130\127\313\311\110\115\374\104"
  "\333\60\357\207\120\35\330\241\336\367\363\306\6\244\32\245\364\116\355\161\364\76\150\245\274\221\235\50\346\67\101\231"
  "\64\173\45\106\41\116\315\241\132\213\150\137\211\150\314\55\145\241\141\54\20\275\177\147\130\232\340\6\347\70\16\2"
  "\41\20\231\300\252\66\172\344\275\20\225\15\243\371\37\41\270\227\11\374\343\370\4\6\21\114\360\76\50\51\225\17"
  "\302\113\357\177\305\31\304\304\105\152\76\62\335\37\40\206\121\301\4\127\51\36\212\40\73\321\177\323\144\232\153\62"
  "\216\166\124\267\75\60\313\322\326\273\77\162\364\363\103\372\54\162\43\201\247\222\121\303\32\172\364\120\137\365\27\46"
  "\231\65\214\373\231\5\53\307\326\127\266\6\116\125\377\305\251\262\67\326\112\51\255\124\165\137\126\334\251\112\14\54"
  "\46\213\172\75\177\67\256\250\347\253\5\10\55\170\54\61\115\214\55\350\3\331\153\365\240\332\17\364\341\301\347\14"
  "\107\200\163\317\104\201\370\235\165\66\145\120\34\246\220\221\142\107\361\167\314\326\162\365\270\250\177\173\234\114\166\175"
  "\376\107\361\347\172\45\353\171\232\344\67\177\255\13\270\74\34\57\242\33\216\166\110\337\345\204\75\34\14\266\24\343"
  "\42\156\361\347\16\233\0\343\26\340\122\325\213\230\160\314\247\64\175\150\20\335\143\57\356\237\321\214\76\16\306\31"
  "\302\200\243\103\51\224\342\361\41\231\132\214\146\246\231\344\200\124\322\31\343\52\305\104\212\251\270\316\214\75\347\317"
  "\356\335\113\144\100\107\365\364\41\232\141\214\130\313\251\150\232\266\273\126\173\332\372\271\365\253\374\325\362\164\123\221"
  "\151\30\247\222\13\231\50\221\247\143\122\250\33\122\310\310\163\27\37\25\51\174\376\151\374\313\274\0\34\244\171\306"
  "\213\307\71\46\143\34\52\246\120\136\65\274\347\323\305\73\345\357\274\153\343\5\177\44\23\155\374\254\146\40\223\156"
  "\123\56\150\225\17\61\133\106\47\34\342\205\31\161\66\40\271\151\214\223\104\16\316\310\230\132\345\362\277\335\163\47"
  "\314\250\200\57\311\75\3\23\262\144\246\227\163\12\221\2\14\332\227\374\45\276\212\107\323\230\317\226\234\33\201\366"
  "\261\22\223\62\362\251\24\312\24\317\77\63\340\41\47\322\7\230\352\21\305\115\252\76\103\52\45\226\312\317\57\166"
  "\230\314\20\353\210\274\142\71\214\333\17\311\173\143\53\352\34\370\70\107\74\274\237\370\275\73\16\1\231\321\37\205"
  "\20\7\12\350\56\335\150\161\140\363\365\126\346\250\242\270\173\317\160\212\44\361\160\60\162\356\160\141\362\360\370\270"
  "\240\42\205\306\143\52\125\370\265\3\211\153\40\161\15\44\170\342\217\275\166\266\360\1\247\243\316\21\145\364\245\250"
  "\114\25\204\21\342\32\146\174\2\177\347\341\74\141\306\217\153\230\161\15\63\256\141\106\206\330\135\303\214\153\230\221"
  "\66\352\65\314\130\74\376\230\215\213\107\23\22\55\342\267\336\377\335\266\72\314\137\300\277\206\27\327\360\202\57\100"
  "\140\357\6\341\330\227\31\366\203\55\52\220\53\14\325\51\366\265\256\16\177\207\334\104\33\154\60\125\143\6\372\123"
  "\173\346\127\317\374\167\213\174\163\172\355\63\170\347\11\316\255\374\304\316\55\101\206\375\126\232\367\253\347\132\310\163"
  "\135\55\166\317\316\355\271\56\327\327\314\167\226\117\265\134\177\136\107\27\177\271\74\117\141\32\1\1\45\213\317\357"
  "\72\142\76\223\147\253\57\263\135\115\240\370\62\212\352\73\242\331\247\162\234\70\26\335\333\22\316\115\145\175\62\300"
  "\147\135\230\62\142\36\154\301\323\315\335\267\307\361\344\335\355\27\323\224\355\27\123\146\357\200\75\54\266\33\203\36"
  "\230\366\3\333\176\340\172\327\374\317\147\64\213\323\370\366\222\174\307\343\47\333\14\372\341\150\351\275\307\42\377\231"
  "\322\70\170\266\125\217\123\4\321\360\27\347\17\20\341\304\14\62\334\200\222\277\332\373\11\104\210\23\213\272\71\266"
  "\13\307\323\317\257\372\220\173\107\345\235\75\74\241\246\227\265\363\113\34\222\70\154\257\27\240\144\206\3\12\16\306"
  "\2\145\16\266\214\232\71\351\270\254\351\244\70\105\71\243\174\122\175\364\271\152\31\200\350\331\217\230\242\253\51\162"
  "\275\13\10\42\371\113\335\307\200\227\157\311\370\312\300\37\263\301\54\275\26\171\27\174\276\33\173\156\140\342\165\366"
  "\376\221\142\267\233\126\173\373\376\61\1\323\315\373\171\132\166\142\140\253\275\151\265\267\261\70\227\36\374\56\325\337"
  "\350\224\263\115\62\116\257\264\71\43\132\33\6\35\15\211\60\333\340\255\317\371\111\155\116\163\172\213\124\20\232\46"
  "\102\21\327\43\63\342\63\342\20\316\351\261\133\376\131\13\267\247\35\353\100\232\50\177\114\204\62\241\311\337\51\166"
  "\206\45\100\230\16\242\204\136\51\223\72\362\75\117\376\200\32\343\351\23\366\134\1\63\262\203\217\116\375\266\304\371"
  "\145\17\353\167\47\362\41\215\320\230\26\170\54\346\137\56\352\311\115\353\272\240\270\56\100\274\105\57\220\16\242\51"
  "\245\145\152\344\27\60\54\301\324\147\36\120\302\277\7\316\370\160\367\341\375\157\275\43\207\164\110\141\64\70\365\203"
  "\146\47\22\77\74\316\177\353\35\113\43\302\135\212\4\356\366\25\316\305\227\351\204\340\174\5\133\367\122\216\167\341"
  "\372\205\243\164\230\212\343\35\306\373\43\157\230\43\77\134\20\352\207\256\33\15\325\153\310\26\141\144\217\115\204\122"
  "\61\250\1\337\140\120\151\236\114\311\171\262\315\171\162\245\347\311\25\235\47\103\123\323\152\130\152\0\153\173\372\271"
  "\61\72\31\137\340\227\144\250\210\315\351\52\302\247\252\210\230\33\20\147\275\252\44\353\371\46\353\205\43\175\157\40"
  "\353\61\224\210\45\102\121\326\253\210\301\132\15\37\123\21\122\246\352\210\115\174\342\363\164\204\24\45\147\112\312\346"
  "\124\111\125\172\256\244\52\253\317\5\115\20\153\311\64\125\261\71\207\252\330\240\343\316\30\263\200\55\236\134\253\360"
  "\307\26\236\154\207\17\242\42\52\61\10\343\230\104\226\116\147\334\245\313\167\230\147\214\51\230\323\303\342\152\326\7"
  "\110\272\142\342\360\21\61\214\24\143\63\12\343\356\70\112\34\1\76\132\334\353\344\20\341\342\311\115\207\14\273\71"
  "\201\141\67\47\63\354\346\154\14\273\71\231\141\67\347\145\130\140\364\323\30\226\121\30\167\227\315\260\161\37\210\103"
  "\4\30\166\215\146\236\365\73\153\332\75\335\153\30\131\323\343\250\142\344\45\36\62\75\135\0\340\277\335\46\5\351"
  "\240\227\207\364\136\160\61\71\1\205\111\102\130\320\244\302\316\104\172\370\315\37\261\164\1\262\46\22\154\137\311\261"
  "\27\17\21\60\260\301\210\114\371\26\47\64\177\351\14\15\47\1\177\4\146\166\173\206\231\215\317\4\357\205\33\136"
  "\362\146\115\217\153\267\346\143\322\115\127\205\221\247\60\310\145\1\140\340\131\7\263\12\346\210\136\377\304\342\112\46"
  "\325\66\115\252\353\345\247\131\77\245\236\57\20\146\223\42\161\107\24\111\363\61\315\110\276\253\237\74\201\14\162\204"
  "\252\120\232\123\54\371\302\1\166\242\66\227\15\376\105\371\112\14\232\331\256\353\351\317\36\27\51\201\343\42\20\325"
  "\222\200\141\66\202\221\21\344\6\160\117\31\200\161\211\223\45\374\353\374\274\107\76\352\271\364\307\344\342\302\223\112"
  "\215\333\354\304\6\121\77\37\52\20\53\176\176\355\300\204\71\277\304\223\53\232\22\373\165\226\337\27\253\123\167\247"
  "\156\17\17\251\250\151\372\265\125\66\265\150\66\127\355\335\252\262\135\65\325\172\360\235\275\40\132\75\210\326\7\315"
  "\346\267\326\370\151\233\205\116\70\31\374\200\363\345\156\373\131\147\357\373\131\263\112\105\352\15\337\45\210\137\313\332"
  "\52\110\2\220\261\223\20\143\203\255\51\175\212\12\73\211\63\300\6\172\352\364\74\47\221\104\256\234\101\173\242\262"
  "\46\207\214\306\311\37\200\42\40\234\153\304\273\322\370\125\377\247\57\366\312\106\370\244\172\30\220\324\251\143\300\50"
  "\20\151\174\213\217\315\331\52\276\262\211\134\217\304\116\25\132\55\134\113\12\214\324\321\270\110\261\252\20\242\273\211"
  "\135\351\24\133\221\325\170\152\170\140\204\53\366\343\213\266\246\11\260\115\32\37\22\115\20\111\216\166\311\237\234\172"
  "\160\10\35\230\102\363\243\151\46\32\15\126\14\23\341\103\20\63\227\143\153\340\242\117\143\343\146\163\75\173\6\271"
  "\76\256\263\276\105\343\355\14\342\200\235\114\220\361\343\64\0\60\67\207\201\46\23\76\103\206\224\43\203\223\105\324"
  "\310\237\350\165\104\220\62\247\32\305\126\337\376\316\44\344\132\236\214\366\222\53\376\357\11\137\111\376\25\267\141\42"
  "\124\322\52\366\346\242\125\263\7\112\264\22\117\331\146\163\44\242\75\105\16\230\132\305\371\1\372\260\53\64\30\13"
  "\164\322\270\157\305\24\30\344\225\357\247\202\102\167\36\376\75\205\4\163\34\7\64\302\322\315\112\273\130\311\235\353"
  "\305\211\25\27\24\337\20\224\120\116\120\344\251\14\263\222\100\332\260\70\25\332\36\0\306\110\335\276\274\212\61\16"
  "\211\376\217\304\15\310\140\53\2\33\11\53\12\31\351\4\12\33\177\343\164\312\54\361\120\310\303\307\144\203\72\74"
  "\331\375\136\122\247\345\324\53\326\62\53\174\20\41\216\131\0\10\204\22\204\5\164\50\271\311\220\47\333\14\131\320"
  "\150\134\215\106\70\152\64\2\257\323\116\40\152\316\15\45\351\251\163\246\346\267\171\211\367\257\131\243\63\32\156\243"
  "\274\214\216\156\306\11\20\56\25\334\65\226\100\114\342\266\53\233\160\53\302\0\243\216\62\304\371\150\21\267\2\30"
  "\24\350\53\152\337\43\123\40\101\314\14\106\3\151\320\10\126\70\340\304\327\44\143\225\204\61\44\125\202\225\72\244"
  "\123\344\127\316\51\130\147\203\314\7\260\26\70\171\53\322\17\12\246\36\313\57\157\22\66\135\257\157\156\223\127\36"
  "\347\217\317\231\207\65\104\326\41\7\353\146\173\373\345\266\375\340\241\371\140\336\176\143\336\176\143\43\233\355\125\273"
  "\255\232\355\105\263\271\154\275\335\154\376\150\66\237\232\315\147\152\342\253\357\356\233\315\315\164\271\233\46\172\226\161"
  "\264\5\77\312\2\237\125\1\317\246\120\125\363\331\122\265\350\325\236\220\145\173\102\126\355\67\126\364\6\215\243\365"
  "\365\14\214\330\31\30\252\352\374\14\14\125\225\72\3\3\234\67\101\252\274\334\341\25\221\55\234\305\126\155\167\355"
  "\207\77\125\343\162\157\243\6\17\323\331\127\372\12\34\210\103\6\353\124\57\15\57\225\47\235\41\221\221\37\311\32"
  "\52\75\31\360\260\56\160\104\133\231\143\100\210\171\60\25\273\31\63\203\234\233\362\344\4\34\300\56\244\241\104\101"
  "\136\35\67\306\67\377\344\35\132\26\116\317\40\345\13\53\215\221\256\27\322\256\353\171\365\32\20\75\100\321\10\250"
  "\55\226\312\53\127\125\225\113\333\300\250\42\100\2\346\207\234\304\6\5\135\144\357\112\46\53\162\37\351\223\151\121"
  "\40\200\221\333\370\356\301\242\67\273\244\44\243\107\260\237\22\346\56\370\30\167\40\370\143\20\160\126\0\333\314\241"
  "\12\143\360\142\106\110\247\374\22\121\36\364\210\51\77\275\0\312\147\374\113\311\311\1\126\356\2\154\116\21\225\237"
  "\1\77\324\164\33\16\226\70\12\26\335\301\4\317\115\141\343\56\353\207\172\264\102\110\201\76\231\265\151\144\6\336"
  "\103\154\126\177\245\361\116\301\227\130\66\263\74\21\230\42\314\241\230\30\331\334\305\3\21\126\213\231\177\225\131\342"
  "\31\75\225\256\214\364\371\67\211\340\43\220\61\30\74\5\12\246\144\215\171\36\307\173\274\10\77\217\307\160\106\203"
  "\362\174\101\51\255\53\45\264\363\326\124\225\365\242\342\11\277\244\55\116\203\370\46\73\254\5\363\43\277\302\41\46"
  "\7\260\114\360\265\116\26\172\20\23\15\326\140\35\251\73\25\261\145\52\202\201\105\310\144\104\17\345\314\63\271\47"
  "\31\46\232\143\35\247\3\37\30\177\0\250\310\270\212\241\224\265\30\262\145\266\220\15\233\254\156\266\247\252\375\323"
  "\67\102\313\242\223\173\171\73\32\100\200\54\254\14\101\12\23\17\16\141\27\72\170\241\244\124\250\13\54\150\25\250"
  "\53\314\320\102\354\65\62\212\223\172\366\364\357\46\365\142\60\135\326\365\172\124\317\127\54\315\61\235\115\42\244\315"
  "\350\267\371\72\15\364\124\57\126\224\2\347\143\225\167\304\201\53\375\351\10\230\101\311\252\304\141\216\127\126\344\14"
  "\165\145\105\356\164\161\173\304\373\143\312\37\224\225\256\232\341\12\164\307\372\362\20\210\336\130\346\220\65\73\243\357"
  "\355\137\377\346\175\365\362\275\376\363\57\332\17\200\343\265\37\145\331\226\144\116\1\370\13\163\77\236\217\101\120\111"
  "\153\52\327\162\114\157\214\121\56\130\241\342\210\104\107\371\221\206\271\31\324\142\70\361\166\74\236\30\263\303\334\275"
  "\14\34\224\15\116\205\120\367\205\77\102\4\352\30\40\144\325\320\253\111\335\106\310\40\302\66\161\171\142\103\162\354"
  "\236\250\213\243\173\10\144\162\330\330\177\253\17\161\54\134\54\160\167\351\112\106\140\7\225\304\77\30\215\342\4\133"
  "\51\270\70\303\216\206\343\112\65\377\154\234\101\312\256\177\253\367\74\21\166\217\53\31\202\262\365\37\222\13\34\216"
  "\304\371\361\16\126\303\243\360\30\274\200\73\316\263\153\75\256\224\301\224\344\57\321\104\112\336\300\230\140\317\35\312"
  "\370\223\105\6\345\150\70\151\257\264\165\116\105\234\164\310\351\367\44\26\170\250\312\125\116\46\272\122\22\15\5\340"
  "\302\114\55\113\115\236\103\145\350\330\153\305\362\202\213\355\70\270\5\153\3\163\112\371\10\253\326\171\147\303\71\137"
  "\257\52\173\226\131\134\221\377\370\120\22\353\347\42\104\357\262\374\117\226\135\77\0\302\66\74\362\47\137\156\57\303"
  "\324\23\50\251\313\57\314\151\201\250\344\313\131\172\311\264\325\316\127\42\110\365\136\12\312\52\243\274\157\360\11\146"
  "\62\351\101\365\160\327\332\200\46\217\210\317\4\212\36\321\153\77\135\153\120\203\343\301\75\166\54\170\304\274\34\131"
  "\340\266\203\36\36\162\343\246\345\21\104\226\374\11\320\27\212\177\241\140\234\252\351\117\350\267\70\325\221\314\66\173"
  "\343\35\14\335\370\360\337\311\140\70\231\274\5\230\106\171\133\111\243\2\350\210\105\274\322\117\224\63\143\26\361\246"
  "\367\43\53\343\304\40\310\261\221\102\122\304\133\251\112\171\251\114\172\107\143\357\314\40\350\121\330\141\147\347\175\375"
  "\332\221\26\316\211\312\234\100\242\101\125\53\157\6\306\232\272\236\120\107\302\205\312\12\311\243\120\156\146\272\375\205"
  "\363\326\342\210\4\74\44\111\305\62\175\174\276\125\15\166\117\317\42\242\25\231\21\337\264\123\7\256\207\54\166\114"
  "\361\57\100\154\336\241\305\316\120\372\213\367\325\40\246\27\320\222\52\107\231\143\105\15\322\103\230\127\101\67\137\243"
  "\360\140\376\55\231\331\231\137\14\367\20\50\161\123\313\337\302\311\35\66\312\121\33\125\217\52\21\352\166\56\65\70"
  "\131\5\127\211\243\361\301\34\161\311\226\63\172\377\60\226\232\324\102\32\73\24\162\262\317\273\372\375\140\375\352\106"
  "\50\157\234\164\266\356\213\160\352\200\137\21\156\343\221\36\10\75\31\267\160\163\301\100\333\300\274\263\361\141\240\256"
  "\202\40\116\126\76\370\54\222\164\347\52\200\175\172\230\45\366\54\215\12\267\235\123\201\164\307\257\233\261\145\63\30"
  "\171\155\305\45\271\373\71\140\20\160\326\111\117\53\147\355\276\141\356\106\147\125\23\340\243\217\326\264\366\143\150\367"
  "\43\170\107\63\152\33\306\377\233\213\341\377\215\344\340\362\47\362\175\116\133\111\246\242\101\217\53\205\224\72\67\212"
  "\34\44\14\5\262\340\254\153\172\57\256\275\266\247\355\121\324\372\174\353\14\167\14\172\32\21\55\121\44\124\321\13"
  "\257\145\25\204\200\205\37\270\7\160\160\177\362\371\31\231\353\365\274\176\124\172\357\274\321\225\122\225\21\126\113\305"
  "\235\335\164\41\345\345\250\76\153\377\71\130\323\47\226\5\153\262\175\153\366\252\312\337\10\341\255\163\312\325\175\131"
  "\221\53\25\261\332\133\76\130\324\206\174\135\375\55\147\2\254\57\341\335\270\114\74\224\226\112\253\134\1\353\113\341"
  "\214\262\312\110\167\141\22\46\176\65\171\312\24\204\207\317\117\60\264\144\170\227\275\31\375\14\27\246\336\147\25\105"
  "\340\215\24\240\320\354\56\276\42\313\375\177\270\372\313\361\77\352\211\314\62\365\116\51\62\347\257\176\25\137\373\232"
  "\161\315\230\172\123\350\175\53\32\101\47\336\363\63\150\272\133\270\302\170\147\36\235\134\140\157\50\22\262\273\374\132"
  "\126\216\64\50\2\212\153\373\133\30\311\363\270\201\372\53\22\100\254\176\176\364\107\200\60\117\322\251\332\150\126\270"
  "\351\202\226\246\222\136\301\62\256\37\111\104\132\141\23\313\33\34\120\65\251\47\3\35\352\11\3\124\302\332\306\325"
  "\361\220\337\216\234\24\243\201\167\43\35\346\175\25\136\142\376\233\40\214\263\106\123\372\363\214\171\160\274\123\212\167"
  "\272\375\120\342\371\71\175\373\17\41\15\223\227\47\127\372\352\372\155\165\120\272\227\77\12\133\340\324\271\221\334\377"
  "\167\130\17\307\101\317\373\57\63\267\267\112\252\172\371\363\311\125\303\322\72\67\30\114\224\35\327\152\64\357\313\103"
  "\70\345\156\234\324\225\165\132\325\175\141\323\341\32\16\307\106\216\354\130\271\341\144\277\40\53\136\131\312\131\255\203"
  "\120\373\336\134\62\144\116\127\56\124\152\130\17\314\130\354\72\263\373\316\314\215\324\136\152\347\167\135\375\374\345\124"
  "\342\60\220\151\305\71\34\376\73\116\116\36\24\1\351\51\264\135\355\153\52\377\77\3\72\104\121\137\174\250\322\153"
  "\301\101\303\202\115\311\252\156\253\137\251\110\224\33\354\354\303\5\357\260\377\232\171\15\322\226\103\32\337\330\274\173"
  "\66\234\235\264\321\161\133\334\315\275\373\171\265\123\170\173\354\22\233\214\264\343\24\37\36\277\236\172\217\333\355\144"
  "\371\275\171\166\336\44\176\174\41\77\157\361\37\315\346\363\151\47\45\216\357\233\355\373\370\361\204\370\370\303\207\65"
  "\77\174\360\223\37\0\210\353\247\313\336\356\233\221\257\300\107\245\344\57\53\131\363\352\20\170\41\215\16\176\277\135"
  "\202\34\202\364\20\33\157\330\5\164\50\173\201\132\101\300\101\12\305\32\224\254\140\360\62\20\143\323\22\231\225\110"
  "\241\30\220\311\163\311\37\230\144\160\46\5\76\353\243\300\145\205\130\216\313\157\173\301\143\46\156\173\341\237\71\151"
  "\224\61\174\170\174\63\33\132\206\53\273\377\42\242\237\247\140\37\17\117\177\75\56\176\272\353\261\206\114\225\232\122"
  "\116\147\231\214\133\156\343\201\300\44\302\174\14\146\164\375\374\373\327\65\44\152\361\111\236\232\46\333\306\247\43\307"
  "\44\342\141\261\156\211\24\130\61\71\36\337\277\223\110\252\217\147\274\120\36\51\322\351\41\53\245\155\25\364\250\162"
  "\363\276\326\257\273\157\205\127\322\4\135\367\245\200\201\72\306\74\76\253\271\73\263\13\354\303\236\344\156\3\370\107"
  "\76\107\41\262\374\203\201\161\351\154\365\217\363\60\32\336\333\61\101\351\5\310\224\311\111\32\300\75\160\227\331\62"
  "\153\316\226\104\314\354\35\47\24\13\243\142\267\340\215\365\140\143\346\307\263\257\317\355\161\215\21\106\310\70\200\21"
  "\120\356\322\216\0\10\257\177\325\144\40\14\155\172\241\371\4\337\217\52\71\262\162\354\353\312\17\150\107\210\332\377"
  "\311\340\323\373\321\141\40\302\130\124\176\344\2\365\43\255\226\332\353\252\52\227\12\5\231\114\240\71\203\46\371\147"
  "\171\155\204\243\62\101\231\140\224\12\332\206\67\132\277\356\57\362\351\375\124\306\71\23\334\150\50\364\270\46\132\171"
  "\351\265\255\224\112\207\147\42\245\32\117\164\155\225\61\324\217\251\202\227\301\73\114\363\262\263\321\164\226\140\151\333"
  "\343\2\210\137\244\323\225\144\100\1\333\65\115\74\172\46\307\106\3\153\13\176\46\260\17\171\263\163\131\335\110\357"
  "\313\264\327\376\1\230\40\333\100\225\261\327\335\342\332\221\273\131\332\45\50\317\121\313\114\137\217\341\334\75\263\24"
  "\104\343\322\135\103\220\334\227\342\324\364\376\46\236\275\247\204\72\116\277\137\23\354\327\4\373\65\301\176\115\260\147"
  "\311\337\65\301\16\356\1\301\267\313\320\334\340\353\25\363\163\367\34\43\346\232\63\34\101\161\102\64\166\270\115\113"
  "\112\163\52\342\14\305\246\223\245\201\13\314\352\157\216\146\34\46\365\104\113\71\221\152\250\335\274\157\16\141\260\276"
  "\161\301\50\35\204\257\373\122\163\335\301\273\31\216\206\325\120\132\345\152\53\152\112\134\350\75\301\14\125\67\202\263"
  "\356\311\225\141\3\202\175\143\30\121\155\214\324\126\204\211\373\353\201\225\132\110\241\102\10\70\274\116\162\302\352\365"
  "\374\203\56\330\355\264\331\176\170\154\266\107\361\353\42\271\213\266\136\16\217\373\154\174\62\272\257\147\312\337\301\32"
  "\131\365\73\340\233\357\65\45\300\47\205\127\126\133\23\32\367\71\320\45\351\31\126\14\166\201\147\206\110\134\176\245"
  "\226\224\157\336\76\47\174\33\10\177\55\367\272\31\342\0\340\336\262\333\146\20\74\240\106\372\217\260\207\247\22\341"
  "\246\362\252\222\232\137\162\135\374\54\144\14\62\163\336\53\23\244\30\133\65\326\302\312\127\230\373\225\261\67\122\153"
  "\45\103\3\350\7\16\164\367\147\176\3\256\255\244\20\122\332\306\41\370\100\24\131\27\252\322\306\7\337\10\37\200"
  "\50\262\343\10\36\146\352\277\376\67\300\210\147\75\361\360\166\32\203\353\177\376\237\363\302\305\3\343\107\106\344\10"
  "\4\344\46\302\27\300\101\241\31\347\50\303\133\202\70\65\157\7\17\377\271\243\111\156\126\42\254\26\73\67\143\257"
  "\356\371\2\151\372\252\334\52\41\70\132\41\356\306\220\23\312\210\127\361\270\304\275\247\214\313\171\221\353\23\16\6"
  "\275\36\207\174\335\144\72\154\124\265\326\132\52\21\325\55\234\157\153\243\107\336\13\121\331\60\332\51\350\340\16\234"
  "\133\377\241\5\263\165\74\232\315\53\247\41\353\224\376\75\6\245\344\345\303\31\207\2\143\363\3\24\27\376\224\263"
  "\64\276\202\13\353\234\64\211\275\156\373\271\156\373\31\345\55\5\25\220\362\306\223\21\7\231\377\134\176\133\321\72"
  "\153\335\54\23\353\207\307\34\102\344\27\352\214\20\41\350\27\156\324\222\67\60\155\176\126\276\27\160\155\137\12\45"
  "\223\22\274\270\77\274\43\12\3\270\351\52\67\330\321\176\167\232\174\156\140\263\10\54\205\20\234\252\170\6\256\63"
  "\0\253\123\204\20\57\367\175\150\255\234\166\62\324\175\55\370\116\161\254\361\370\335\357\50\213\371\115\376\324\74\146"
  "\335\352\154\275\154\75\230\254\233\17\26\262\365\202\152\265\115\153\143\232\154\265\125\253\255\363\22\253\204\356\163\353"
  "\205\351\327\133\376\344\167\31\216\74\124\342\222\263\264\140\61\212\76\305\311\70\270\162\107\213\66\350\232\341\370\335"
  "\225\33\22\212\363\346\206\53\57\155\60\76\55\261\373\11\322\313\7\56\57\221\140\246\256\62\122\314\345\127\174\327"
  "\231\147\126\136\163\320\34\166\104\215\354\163\321\62\346\350\227\113\67\347\253\317\374\312\72\224\270\313\257\204\203\151"
  "\142\220\344\55\202\40\251\2\214\140\77\212\141\77\7\305\176\34\307\331\270\213\214\166\356\166\245\176\354\365\76\275"
  "\237\255\0\57\44\353\235\315\112\371\31\357\174\121\352\54\345\15\72\301\67\142\343\61\106\47\246\325\245\23\225\220"
  "\306\164\233\126\317\147\252\262\160\340\214\127\27\311\362\121\314\204\146\157\266\130\243\335\24\153\114\165\172\53\332\315"
  "\172\231\235\11\244\357\371\132\325\377\73\276\116\345\343\353\124\361\355\56\62\62\52\173\6\120\125\247\100\52\135\23"
  "\324\174\234\225\311\101\132\361\141\243\217\342\50\233\123\0\325\252\24\312\106\344\240\254\33\303\306\246\136\361\267\24"
  "\36\205\276\67\374\173\175\2\224\13\371\241\113\216\26\22\310\50\74\334\54\111\30\26\360\26\11\214\204\72\254\110"
  "\105\124\23\270\106\32\144\353\43\354\132\227\332\314\223\242\32\153\6\13\243\152\226\175\301\203\342\245\15\260\166\24"
  "\131\277\250\61\22\31\24\357\313\70\311\363\215\33\247\14\242\103\362\130\5\266\214\311\164\56\103\121\201\172\215\12"
  "\224\215\124\223\201\116\110\237\364\217\56\215\341\110\20\323\213\324\35\232\4\316\362\351\276\103\376\12\140\16\303\200"
  "\4\127\72\57\364\43\367\104\277\45\376\367\171\151\306\100\104\42\74\202\165\174\267\56\1\227\277\102\215\31\344\42"
  "\327\104\17\11\377\56\226\274\150\230\374\305\322\213\137\62\43\316\50\133\23\0\34\23\100\113\260\51\255\300\241\355"
  "\350\314\51\202\51\124\322\252\364\243\332\211\172\77\161\342\127\237\104\314\177\227\241\3\111\247\221\176\111\141\347\353"
  "\175\135\10\214\322\340\130\176\100\274\113\270\301\40\303\37\137\41\177\50\137\375\262\176\57\101\0\225\350\112\0\225"
  "\370\4\2\210\334\307\15\254\50\371\362\345\276\136\314\352\207\57\313\351\354\170\101\311\153\163\323\154\116\67\173\54"
  "\343\365\46\374\254\202\105\263\371\324\154\116\57\151\337\333\64\273\352\200\177\51\205\61\332\113\27\334\151\133\340\371"
  "\32\113\136\346\201\346\277\104\21\64\336\332\11\62\274\117\107\253\217\137\377\304\304\17\334\274\137\275\204\203\102\370"
  "\367\16\333\333\36\137\15\24\162\40\144\30\111\47\307\166\336\227\372\255\50\132\310\340\215\124\56\170\330\33\255\352"
  "\215\153\155\253\241\234\324\325\170\327\131\70\4\204\67\225\265\225\226\272\152\324\61\143\310\152\133\73\77\120\303\60"
  "\252\207\363\276\162\7\310\324\215\25\126\124\116\350\135\157\351\220\331\161\320\3\73\30\115\306\266\32\321\132\224\365"
  "\301\5\11\317\274\51\374\13\276\300\175\272\51\225\71\173\72\112\133\113\177\46\274\226\324\367\101\115\75\147\335\105"
  "\76\366\260\76\44\66\174\166\316\152\303\100\142\164\2\330\245\346\371\66\374\335\162\63\202\117\266\112\67\141\243\307"
  "\45\62\141\240\104\162\320\252\111\334\122\33\133\264\357\353\362\46\355\152\322\362\67\45\225\67\166\140\67\217\331\151"
  "\142\373\152\273\214\204\252\47\262\235\147\44\255\254\252\175\161\361\233\351\222\332\37\376\102\242\175\40\63\70\224\66"
  "\354\25\211\173\261\134\306\13\151\175\245\33\373\171\40\140\252\252\315\304\213\60\262\152\260\63\203\112\35\0\373\323"
  "\16\32\263\353\255\112\206\114\326\165\250\207\343\241\31\312\221\331\165\366\152\123\205\257\154\245\16\73\215\102\72\144"
  "\176\62\250\303\260\366\136\373\361\256\63\135\275\326\376\110\155\203\63\225\253\373\122\142\113\330\375\57\330\140\36\53"
  "\116\23\136\130\57\45\23\272\14\101\311\67\75\354\104\326\14\303\12\5\161\3\154\103\376\62\130\204\264\112\151\143"
  "\202\113\77\247\122\65\310\105\147\223\112\145\171\72\162\157\33\60\104\74\15\210\263\174\371\347\325\344\337\242\307\221"
  "\232\215\317\311\121\64\120\264\202\153\173\62\367\343\356\111\357\17\170\6\141\173\251\134\217\67\227\140\77\152\65\370"
  "\251\251\200\347\146\163\311\275\252\364\175\35\334\247\43\370\56\161\347\106\276\123\106\373\331\273\317\63\224\337\243\60"
  "\135\343\375\7\74\306\304\27\14\323\307\345\135\317\314\23\313\370\15\326\25\117\137\306\365\61\225\262\140\343\7\210"
  "\104\250\67\144\71\206\33\15\220\277\21\203\341\100\352\13\337\307\235\150\332\205\24\312\13\274\143\65\367\350\112\274"
  "\311\11\250\173\274\53\212\63\142\120\162\62\264\306\30\65\226\376\315\270\126\336\352\340\245\323\200\330\334\210\306\274"
  "\365\201\123\116\153\151\354\110\124\363\276\265\57\207\317\11\47\235\254\352\276\254\100\152\42\367\312\325\347\170\206\51"
  "\343\10\52\100\331\374\230\60\363\276\2\136\271\370\317\270\100\225\106\71\271\33\145\312\364\143\104\231\176\254\53\323"
  "\117\245\312\364\343\23\12\201\57\50\233\272\75\5\67\131\210\326\272\20\255\115\41\236\166\246\320\334\27\342\351\100"
  "\164\276\350\154\3\117\15\147\254\342\54\271\316\117\350\224\177\265\300\27\202\343\304\64\44\7\123\332\213\123\46\66"
  "\316\37\313\17\111\361\72\106\215\130\27\317\345\375\35\162\10\361\236\222\47\66\126\312\45\100\350\334\145\305\342\34"
  "\112\170\0\300\210\145\312\226\162\76\243\13\260\361\7\117\370\52\47\232\165\34\350\343\25\233\210\273\214\135\177\274"
  "\145\373\371\364\253\265\51\34\170\53\311\160\54\246\51\220\200\212\344\134\220\156\3\4\347\114\237\37\340\74\263\271"
  "\147\231\45\126\112\167\31\264\135\175\50\374\135\361\171\70\101\131\56\161\177\74\51\227\364\325\323\371\226\170\237\323"
  "\23\154\213\372\333\227\371\364\121\265\262\154\207\274\25\145\331\232\315\132\64\333\263\326\353\273\211\34\65\363\160\264"
  "\370\110\67\156\157\133\67\160\257\133\355\247\126\173\336\152\337\67\333\243\301\142\261\151\76\232\235\162\345\167\253\273"
  "\157\315\346\167\321\352\353\226\332\107\262\212\223\347\126\373\251\335\376\135\11\376\110\66\37\15\347\275\346\50\223\366"
  "\203\251\154\77\120\355\7\272\375\300\264\37\330\366\3\327\176\120\265\37\370\366\203\300\0\23\354\211\144\117\24\173"
  "\242\331\23\303\236\130\366\304\261\47\25\173\342\331\23\6\263\22\354\11\247\257\142\117\64\173\142\330\23\313\236\70"
  "\366\244\142\117\366\60\223\344\174\175\170\34\276\155\170\132\254\376\335\162\336\143\352\152\307\62\361\227\331\356\12\321"
  "\166\162\350\303\45\373\20\17\177\140\320\243\376\354\164\66\135\175\271\143\7\214\37\64\105\33\54\23\77\24\227\32"
  "\307\274\142\236\324\246\21\351\156\305\71\3\142\306\135\127\160\77\43\167\324\152\301\334\315\367\113\202\11\313\275\65"
  "\376\262\344\260\77\245\146\377\330\153\14\336\106\72\23\1\256\14\173\211\140\101\371\154\232\261\10\355\276\213\130\357"
  "\215\107\47\337\220\311\346\204\146\77\166\174\334\267\227\47\303\335\243\373\337\172\252\7\11\111\367\241\74\316\177\353"
  "\311\36\272\364\220\306\240\55\122\224\340\336\276\16\277\370\62\235\274\16\117\320\150\336\371\204\113\260\27\34\103\16"
  "\117\332\201\323\174\135\347\10\202\167\230\47\171\322\376\324\4\51\245\363\231\36\131\75\77\176\171\174\252\27\73\227"
  "\205\303\126\132\221\360\160\217\147\301\227\253\307\105\315\302\363\267\27\217\116\344\224\77\1\342\25\341\303\361\342\161"
  "\316\311\0\10\203\173\277\153\367\76\134\20\267\357\220\152\64\30\347\332\123\345\310\140\12\221\54\143\101\262\115\160"
  "\134\263\351\21\164\204\177\146\106\177\362\374\127\344\52\216\134\134\167\141\344\174\23\233\320\354\235\206\7\151\205\273"
  "\162\7\321\147\251\51\254\273\247\340\13\156\36\312\141\3\2\103\232\147\222\201\320\353\112\16\53\22\275\126\303\47"
  "\231\14\43\316\152\63\124\234\43\223\324\245\73\263\272\44\102\32\242\135\253\141\71\14\307\34\311\355\137\345\174\121"
  "\217\36\277\315\177\254\352\206\174\112\231\244\14\331\150\133\76\132\336\334\262\216\317\170\235\107\203\36\122\234\346\74"
  "\174\135\305\325\36\321\361\215\256\252\227\120\345\245\16\47\307\333\233\340\204\161\306\320\1\355\70\35\304\113\121\160"
  "\142\232\170\235\317\23\142\140\163\212\214\151\336\141\112\321\331\13\71\244\253\252\112\321\35\217\21\111\145\131\136\120"
  "\241\163\124\260\115\31\246\305\321\331\271\114\31\130\372\337\376\325\45\122\227\354\350\21\245\260\243\47\110\27\262\226"
  "\354\45\44\265\277\213\350\145\175\313\335\263\341\214\261\164\12\373\36\266\155\104\344\27\175\115\67\133\50\313\153\260"
  "\43\260\116\166\232\14\155\124\141\110\60\21\141\104\237\45\120\160\26\327\67\250\152\6\330\232\333\330\154\341\53\362"
  "\241\146\343\26\6\113\153\176\214\6\322\26\204\32\215\315\227\377\200\41\236\41\277\24\117\156\171\42\1\353\300\303"
  "\51\172\6\210\311\361\314\45\57\113\2\1\63\161\331\234\245\77\314\131\267\224\327\210\367\205\113\100\57\237\110\271"
  "\104\330\346\36\315\364\65\172\46\264\212\100\220\134\301\171\233\253\30\147\47\132\371\31\126\334\207\325\255\246\17\154"
  "\116\116\130\202\134\24\147\241\207\367\275\162\323\362\312\155\257\274\367\213\270\20\304\111\104\271\14\11\273\313\72\213"
  "\134\106\46\357\374\201\1\113\275\275\171\373\34\265\14\5\315\116\236\76\63\72\56\206\216\55\207\16\271\266\266\263"
  "\314\203\324\344\123\263\226\351\345\173\150\115\151\164\55\351\364\255\166\205\107\342\346\343\125\374\127\203\341\103\175\350"
  "\343\320\361\353\150\204\5\357\273\173\107\210\301\100\317\322\315\232\327\336\73\301\167\300\140\366\32\56\30\125\374\5"
  "\120\205\303\100\317\222\251\142\144\60\132\350\2\124\241\126\242\15\126\305\267\307\154\161\124\5\54\12\60\15\174\254"
  "\114\110\161\140\215\61\111\365\176\266\247\54\202\320\273\64\47\211\353\54\264\376\52\62\45\41\356\357\204\123\127\226"
  "\276\276\277\104\53\172\115\210\43\13\244\235\173\100\174\37\32\121\230\226\351\144\266\201\343\4\7\6\56\220\110\67"
  "\133\221\151\116\166\317\210\366\272\227\164\53\10\161\203\122\211\332\367\36\237\356\126\150\356\40\332\21\76\324\55\76"
  "\64\15\2\33\216\123\34\45\354\310\20\51\331\74\53\105\3\263\126\33\110\207\304\33\100\301\304\133\331\344\160\206"
  "\343\316\346\10\27\150\270\26\102\125\257\303\265\317\314\345\270\320\73\275\214\342\11\3\324\101\231\5\130\56\244\151"
  "\10\35\257\350\53\137\166\111\137\205\46\137\151\161\135\324\277\214\105\175\55\72\133\325\127\236\164\47\153\205\113\130"
  "\330\227\342\203\53\373\252\352\54\300\126\226\350\307\132\256\227\141\43\370\342\77\215\106\113\377\161\367\232\275\125\332"
  "\323\316\167\233\351\3\220\12\120\6\126\166\66\364\230\354\375\125\155\152\225\32\374\166\346\137\1\352\225\40\50\361"
  "\145\31\322\56\210\220\15\376\152\76\46\342\107\222\355\234\60\140\201\243\314\202\56\256\107\140\111\375\137\266\314\3"
  "\363\62\126\271\362\327\55\364\300\256\175\276\12\212\167\362\135\164\62\57\221\42\267\14\272\221\220\166\2\170\274\162"
  "\10\300\215\63\350\135\44\266\230\4\222\261\61\310\252\363\370\121\353\204\304\214\110\115\16\151\323\162\343\355\105\170"
  "\155\214\56\5\152\324\212\307\171\324\37\362\44\71\32\271\266\201\106\6\101\200\355\54\155\246\33\53\101\315\26\143"
  "\257\52\203\341\135\121\206\257\132\20\371\146\333\210\136\221\250\33\335\112\111\350\5\234\370\242\375\137\340\322\42\334"
  "\317\226\323\222\315\71\15\3\356\332\74\314\162\234\326\106\264\150\53\143\142\315\122\75\377\367\277\75\52\216\116\267"
  "\251\2\254\344\100\374\365\363\163\1\106\166\226\125\325\215\174\171\263\145\104\222\125\121\206\23\74\227\173\163\254\216"
  "\375\140\256\100\373\356\224\256\43\372\66\133\134\305\231\366\126\47\333\372\335\267\332\355\357\165\57\272\3\226\226\325"
  "\17\357\35\76\76\364\170\30\346\165\154\342\3\135\102\271\22\140\366\164\153\142\114\111\153\142\332\244\164\330\161\311"
  "\344\375\351\71\264\11\126\40\256\63\226\66\206\130\205\265\54\7\143\163\104\202\11\132\134\17\207\317\153\144\163\30"
  "\11\171\236\371\61\204\351\333\55\207\13\46\45\125\243\345\77\50\63\117\115\341\27\31\62\23\112\312\214\25\55\165"
  "\164\314\53\170\372\305\144\206\266\252\23\102\135\310\114\323\314\66\132\126\244\311\114\165\56\231\171\172\117\146\236\262"
  "\145\46\302\26\374\304\306\306\107\321\321\71\255\271\314\240\125\173\46\61\126\235\124\263\212\167\7\353\226\314\230\317"
  "\46\63\246\263\145\26\253\110\112\130\113\307\144\206\23\44\356\107\372\313\227\50\150\205\230\227\150\63\102\167\33\367"
  "\36\62\266\123\332\252\5\221\357\225\347\370\356\166\352\135\276\174\376\256\104\367\40\163\161\142\336\7\75\4\310\375"
  "\312\254\101\324\47\53\105\263\305\245\372\262\261\147\263\252\104\17\342\214\342\155\333\135\274\155\33\21\166\243\305\364"
  "\222\23\31\232\62\305\305\226\251\232\322\211\26\104\362\252\51\317\254\51\345\5\150\112\305\247\231\36\176\152\115\51"
  "\77\263\246\124\75\210\63\74\237\104\166\247\51\33\101\150\243\305\365\122\241\174\340\11\212\326\251\4\105\253\122\163"
  "\31\116\267\20\62\237\54\377\347\114\147\214\343\24\261\12\153\351\354\220\52\45\7\242\250\373\214\304\2\326\55\371"
  "\261\244\62\47\101\346\313\101\46\35\2\55\177\271\204\42\130\64\61\56\173\142\50\24\356\140\226\252\354\131\42\60"
  "\273\230\62\37\237\262\164\50\130\21\237\121\75\274\231\74\176\35\327\127\176\2\54\77\310\65\176\54\64\125\335\104"
  "\116\205\146\167\257\241\63\236\267\357\336\246\266\172\367\46\266\311\254\331\256\305\51\247\71\317\326\221\243\240\363\156"
  "\156\73\334\323\117\117\256\207\63\27\76\234\231\103\350\113\35\316\174\326\43\224\265\72\303\11\312\237\360\76\300\112"
  "\51\243\145\260\74\115\237\352\131\310\10\131\316\154\217\370\346\242\374\115\113\274\237\54\305\175\342\235\163\72\50\157"
  "\53\225\170\163\334\355\72\367\320\223\365\173\127\303\65\52\100\157\154\45\252\340\64\335\14\7\166\54\103\24\125\220"
  "\125\225\166\205\310\346\3\347\351\15\215\223\177\336\102\67\357\153\263\307\306\335\210\252\222\102\130\111\27\74\23\106"
  "\347\347\120\23\5\236\367\212\343\30\202\271\43\324\271\377\212\250\177\140\245\30\10\230\237\60\73\330\104\166\320\62"
  "\370\201\252\205\256\104\245\347\175\367\347\355\341\112\51\347\224\255\373\352\47\260\203\353\204\35\362\121\307\356\60\223"
  "\357\154\105\111\34\126\134\131\21\355\273\126\126\125\171\145\105\30\235\233\73\375\45\52\53\2\265\143\145\205\371\11"
  "\263\103\50\257\254\272\143\7\51\56\112\133\141\334\261\266\352\347\253\253\314\113\205\203\121\116\253\40\201\153\335\361"
  "\145\320\354\42\61\66\53\306\116\264\160\343\121\30\171\257\337\16\367\163\332\71\31\252\112\261\135\32\31\347\3\322"
  "\144\362\35\116\14\310\131\362\346\277\31\274\334\213\136\312\266\60\174\133\36\274\305\161\62\313\327\206\340\242\140\234"
  "\225\322\54\302\0\373\52\210\241\371\246\205\331\76\367\364\345\26\14\333\315\361\226\263\72\145\357\6\60\341\160\137"
  "\47\237\315\347\134\157\32\53\15\45\320\131\235\167\111\170\250\362\170\344\151\377\16\122\43\104\234\103\56\20\37\14"
  "\161\170\15\121\376\7\177\301\124\361\153\110\371\34\304\63\327\40\134\137\60\264\260\22\303\276\115\276\102\131\141\306"
  "\301\372\57\337\372\63\160\1\160\13\16\34\202\35\334\101\17\144\115\137\232\254\135\145\315\204\102\262\206\231\367\360"
  "\7\230\367\2\144\155\350\165\30\50\253\204\11\43\71\357\113\161\320\15\376\306\124\336\250\312\271\272\257\325\257\40"
  "\154\346\134\302\206\317\136\314\13\244\54\0\70\43\220\52\201\56\116\54\163\207\61\143\173\22\306\256\373\225\154\131"
  "\45\203\350\163\101\4\13\12\63\246\317\112\247\321\146\335\6\273\112\32\147\264\25\25\57\53\112\147\235\160\246\215"
  "\11\353\310\223\154\211\241\121\60\117\377\224\105\47\234\120\0\346\375\260\316\236\25\257\260\54\210\161\357\27\112\325"
  "\42\66\16\270\14\125\200\153\55\230\217\223\343\13\65\215\40\21\206\177\366\243\235\245\171\273\171\335\262\344\114\366"
  "\45\366\345\157\104\245\56\130\232\240\300\205\250\345\352\176\105\372\1\71\371\253\300\274\367\362\130\345\147\113\301\141"
  "\122\300\331\334\166\176\365\51\276\234\24\327\214\112\227\121\20\276\75\125\373\152\206\314\154\335\334\364\336\70\61\317"
  "\142\216\247\76\272\53\30\57\170\116\41\2\75\116\257\5\221\247\131\101\147\371\255\216\220\202\233\210\35\0\260\52"
  "\166\146\325\227\57\367\365\142\126\77\120\371\34\103\340\274\176\261\122\27\357\27\53\375\11\374\142\340\22\315\316\123"
  "\224\271\234\316\216\327\143\322\126\127\120\200\311\253\40\161\111\343\31\253\347\244\73\133\365\134\346\76\210\355\105\324"
  "\336\115\177\146\355\335\72\351\270\320\210\346\43\36\55\33\150\346\305\202\31\361\131\341\62\223\270\375\341\107\35\2"
  "\253\101\27\367\360\373\326\271\17\101\156\63\167\351\321\112\36\143\61\72\233\150\337\303\276\323\375\70\64\64\364\264"
  "\62\166\314\1\143\300\134\206\362\314\110\176\154\167\230\105\20\31\75\56\113\40\142\56\165\212\310\272\227\100\323\136"
  "\302\174\25\307\112\366\360\241\253\177\314\6\347\361\75\10\243\256\175\217\253\357\161\365\75\200\220\135\135\215\253\253"
  "\361\153\32\144\375\367\60\310\346\123\32\144\173\301\223\367\53\273\32\253\301\117\116\163\134\135\215\153\232\3\10\332"
  "\152\360\267\113\163\160\225\160\146\37\244\163\275\172\332\75\356\221\235\112\221\245\142\304\111\345\325\350\40\43\135\274"
  "\172\167\277\376\274\331\374\336\154\216\336\335\315\277\244\46\337\157\317\125\364\303\153\373\123\156\205\166\101\53\343\261"
  "\74\27\50\257\340\22\231\6\22\127\24\324\31\335\65\33\77\62\46\153\223\260\121\143\351\341\256\210\264\76\377\334"
  "\360\140\354\110\124\363\276\175\55\122\226\112\53\155\352\276\254\130\267\154\307\10\323\173\31\252\66\267\104\150\74\175"
  "\312\66\31\122\124\322\31\343\336\331\363\215\273\60\106\173\351\202\213\165\301\51\136\325\106\217\274\27\242\262\141\64"
  "\377\43\4\167\230\307\372\17\55\310\7\341\250\23\305\323\257\17\242\173\302\126\210\323\370\34\246\7\320\153\306\40"
  "\170\127\223\375\353\337\274\257\137\266\44\111\367\362\127\161\102\34\351\247\277\357\310\214\235\124\102\327\103\67\251\306"
  "\363\276\332\167\324\27\67\132\131\353\254\227\1\365\104\322\65\222\326\212\332\173\143\54\101\244\204\124\122\131\255\116"
  "\200\310\53\345\207\326\171\77\321\303\171\337\276\102\44\214\20\326\132\155\154\72\110\365\244\26\225\25\162\250\274\237"
  "\367\245\174\201\111\10\121\5\151\265\11\301\44\367\245\364\270\226\142\74\31\117\52\21\166\175\35\300\322\67\246\12"
  "\132\113\341\353\276\260\130\324\273\376\205\51\352\371\351\374\17\347\113\353\201\252\344\310\17\324\330\14\347\177\310\303"
  "\174\251\33\43\264\16\46\300\216\10\14\65\250\155\30\331\340\7\43\377\332\217\332\75\25\301\330\312\245\303\343\204"
  "\163\43\71\364\143\41\355\176\246\136\371\307\171\257\274\16\225\112\6\110\17\375\310\16\245\252\203\362\152\336\67\257"
  "\334\123\125\102\173\151\205\353\162\276\331\15\164\14\205\43\367\77\62\75\17\112\324\327\11\117\236\263\254\131\261\33"
  "\334\72\331\126\312\155\53\151\343\347\214\15\242\127\325\176\125\355\271\277\134\125\373\125\265\257\342\267\126\176\137\254"
  "\130\7\313\324\140\314\10\57\3\216\17\363\315\113\106\10\206\231\370\60\353\107\247\172\231\360\344\371\344\71\312\14"
  "\366\360\36\261\325\73\121\302\222\33\236\144\313\327\177\253\325\167\221\132\175\12\245\161\361\176\376\76\77\114\167\116"
  "\30\74\153\64\121\21\306\33\175\234\167\27\45\330\60\226\107\130\224\342\263\174\10\107\21\20\40\313\316\241\120\277"
  "\171\77\225\267\72\170\351\362\245\32\163\16\327\235\271\360\141\216\311\20\354\331\270\340\271\167\253\264\144\321\212\257"
  "\77\46\145\161\363\13\157\322\322\266\213\167\123\276\313\167\163\274\343\311\65\253\173\315\352\236\40\74\247\57\132\362"
  "\374\47\164\136\344\113\230\40\215\264\101\71\12\21\230\204\136\114\136\167\66\356\52\255\53\125\120\306\110\27\131\44"
  "\315\363\14\61\76\31\331\325\155\241\20\174\133\54\4\337\26\12\301\267\345\102\360\155\146\10\216\373\272\206\340\140"
  "\276\62\102\360\155\231\20\174\133\54\4\337\376\15\263\253\213\54\45\127\160\321\21\153\306\5\216\2\170\43\156\56"
  "\363\267\33\346\147\203\331\64\226\312\314\136\315\302\325\54\134\315\2\350\346\152\26\266\71\231\331\14\273\102\212\151"
  "\301\323\115\345\363\226\70\231\213\343\241\362\311\334\147\74\157\320\104\345\32\32\66\100\101\103\203\71\210\136\347\171"
  "\341\361\344\122\22\303\343\11\76\166\204\277\104\217\62\163\303\364\171\126\162\370\152\351\257\226\376\152\351\131\67\127"
  "\113\337\201\241\346\213\62\161\220\236\363\255\51\127\273\300\234\62\13\12\267\350\244\57\64\234\274\355\206\57\26\310"
  "\126\133\121\233\257\113\300\73\330\246\143\176\47\333\47\135\76\220\106\372\20\224\63\235\54\37\74\76\347\156\74\211"
  "\235\353\316\20\312\337\343\102\253\15\370\0\171\102\206\47\64\360\331\355\134\200\362\66\325\340\172\361\234\145\22\166"
  "\46\376\136\100\157\247\261\233\60\151\123\113\344\323\207\107\364\51\300\210\150\230\5\30\137\376\314\207\222\107\24\371"
  "\327\147\124\122\173\145\225\377\110\255\274\122\336\130\143\76\141\255\74\270\343\204\116\345\343\225\365\235\326\206\62\200"
  "\122\227\110\275\17\336\272\17\114\376\36\332\312\373\70\377\240\3\64\307\371\376\3\137\105\313\340\14\22\244\2\60"
  "\300\70\157\35\127\310\161\256\342\143\145\243\230\333\125\6\212\174\267\11\276\252\261\222\112\210\64\126\122\331\344\362"
  "\307\160\274\261\214\140\131\344\301\303\140\56\56\302\116\72\101\324\372\61\234\372\22\200\211\47\36\330\242\334\5\161"
  "\36\275\245\237\373\277\75\156\167\377\371\324\275\265\317\47\365\43\135\241\216\264\52\324\221\361\205\72\162\246\120\107"
  "\236\210\175\341\251\224\245\374\50\252\205\110\246\112\321\336\224\142\164\133\212\321\53\142\364\313\230\163\305\355\327\70"
  "\367\330\332\65\170\22\141\262\245\2\0\203\230\0\4\56\34\265\102\107\46\227\307\25\204\116\147\305\4\373\122\270"
  "\12\167\233\137\224\320\110\106\305\314\33\305\230\340\312\116\342\213\264\364\230\152\346\307\66\7\50\362\353\162\357\233"
  "\351\255\157\357\147\303\156\67\357\327\326\116\67\274\326\266\321\156\375\376\345\313\152\37\125\65\36\374\56\105\363\331"
  "\366\66\57\343\106\176\313\241\337\63\136\17\305\345\211\306\73\261\206\66\53\243\267\214\334\44\261\111\46\320\53\341"
  "\317\110\243\15\203\216\206\104\144\342\337\336\156\262\311\264\111\271\203\343\201\53\203\350\73\354\210\173\136\301\254\45"
  "\273\5\214\275\214\222\236\77\76\226\364\344\250\23\151\1\236\233\216\361\314\310\204\156\212\144\102\273\54\127\337\260"
  "\245\175\266\10\202\61\330\240\11\130\342\303\203\0\115\264\0\75\52\336\43\21\246\51\173\337\322\130\62\203\270\347"
  "\73\56\362\333\373\7\102\106\156\372\210\35\254\270\1\207\23\226\330\123\100\70\123\157\375\130\167\175\334\137\3\105"
  "\254\74\363\305\3\300\53\16\360\262\212\330\354\62\132\46\106\300\363\133\326\17\365\10\157\264\312\132\137\212\10\6"
  "\350\40\46\2\256\311\325\122\64\333\241\331\364\315\146\225\40\3\125\157\377\331\276\247\103\357\64\342\216\26\324\103"
  "\336\106\73\306\175\36\161\63\365\327\117\352\60\34\205\117\215\103\65\252\46\132\53\71\126\157\45\20\332\72\31\214"
  "\11\361\336\244\340\360\305\372\353\247\164\350\172\144\303\233\363\50\133\363\152\133\155\323\152\353\126\33\253\267\303\113"
  "\207\57\17\335\35\306\170\35\230\20\126\31\347\76\162\262\151\116\266\130\67\375\367\372\51\253\72\145\161\335\31\77"
  "\307\64\137\334\77\235\276\214\173\345\314\355\4\327\207\123\110\163\337\376\304\201\15\201\71\124\141\31\205\354\15\17"
  "\270\316\200\143\324\167\274\37\134\151\220\267\17\62\35\333\15\130\263\350\144\45\26\370\326\34\27\46\240\244\67\333"
  "\366\64\264\332\276\325\306\46\364\360\322\341\313\103\167\64\306\176\360\343\75\154\143\12\306\107\336\355\370\222\354\355"
  "\355\351\140\240\13\44\171\217\267\114\4\336\2\177\117\77\255\37\27\221\213\53\63\357\305\304\5\317\114\375\104\347"
  "\53\344\32\54\116\135\134\315\257\137\24\227\122\306\31\347\353\276\164\370\30\22\6\63\161\147\371\341\363\151\301\141"
  "\306\127\227\335\226\75\214\366\366\375\142\302\273\113\76\172\340\256\140\355\140\261\3\242\161\6\47\175\147\5\113\101"
  "\362\53\374\263\257\75\274\345\131\176\76\124\172\371\127\245\274\250\334\207\312\277\264\326\132\252\23\366\121\216\252\241"
  "\234\350\321\140\124\31\255\167\102\46\144\170\253\377\252\56\257\376\13\310\114\267\156\104\275\236\177\223\374\144\227\274"
  "\23\61\60\247\345\127\105\255\336\257\52\137\145\176\120\242\134\211\364\67\33\21\47\76\323\61\51\17\174\106\145\74"
  "\144\12\257\254\266\46\44\253\237\113\223\6\114\206\216\117\237\41\262\272\40\71\131\363\27\122\320\120\31\325\343\254"
  "\267\276\364\136\73\135\71\335\274\110\342\107\176\41\171\226\331\354\220\263\210\173\0\233\241\362\62\56\32\317\370\230"
  "\125\374\1\346\346\154\23\313\307\201\267\340\25\166\143\57\372\210\254\351\205\372\251\154\322\72\367\371\52\51\204\224"
  "\326\342\56\56\333\141\312\77\225\26\157\211\103\64\224\126\50\151\223\247\341\71\62\341\31\150\144\373\140\317\47\36"
  "\200\11\242\252\113\343\23\314\45\330\200\101\162\346\167\206\251\133\332\373\313\202\370\352\31\136\75\303\253\147\230\216"
  "\134\27\136\43\337\33\75\235\115\116\271\221\60\327\355\213\324\176\336\301\223\122\43\325\232\27\133\211\171\127\260\22"
  "\263\373\374\50\316\114\362\57\363\135\17\276\54\167\162\127\170\233\156\367\220\341\345\214\116\374\364\362\127\226\105\61"
  "\51\353\341\256\13\314\133\341\344\30\76\326\70\57\77\172\1\46\254\174\200\224\237\50\76\361\153\234\330\305\105\3"
  "\174\23\44\33\257\377\213\315\33\171\7\174\171\250\60\261\161\41\103\276\32\346\35\146\131\50\274\135\206\40\5\133"
  "\151\322\267\313\214\352\351\303\157\377\172\67\63\217\213\325\157\377\264\157\375\323\277\301\236\323\173\272\154\337\7\34"
  "\166\362\360\370\270\240\161\17\315\217\16\174\350\4\216\274\170\374\61\33\323\310\207\146\356\310\21\76\77\364\11\1"
  "\131\55\166\377\116\200\34\232\37\45\301\241\23\70\362\254\36\54\206\233\235\0\323\350\157\217\76\12\301\276\243\172"
  "\271\302\14\260\330\15\115\343\37\232\261\261\137\233\365\174\265\330\113\14\204\206\75\331\177\31\235\255\303\320\20\336"
  "\207\161\3\336\103\23\300\73\313\3\166\26\205\164\271\173\66\234\101\120\137\136\43\130\137\332\27\12\354\267\307\361"
  "\204\100\335\267\0\240\323\154\56\230\276\307\5\373\221\61\323\356\336\42\140\367\55\0\354\46\115\172\322\367\273\354"
  "\307\304\140\16\206\113\2\163\337\372\250\124\357\272\300\226\344\161\276\131\116\277\316\150\344\77\237\234\211\110\4\336"
  "\353\100\230\333\366\47\201\22\174\373\326\107\51\263\357\3\16\73\177\174\246\121\167\215\256\271\146\67\44\126\160\217"
  "\137\11\306\135\243\260\115\334\365\230\2\202\24\15\40\244\50\17\206\24\20\220\206\252\47\105\137\12\210\24\173\163"
  "\360\225\11\210\103\263\74\30\337\144\202\316\33\254\11\216\175\13\161\157\46\124\313\331\140\206\166\0\242\210\170\3"
  "\272\315\110\25\100\345\263\243\110\2\21\247\63\42\342\276\325\44\342\225\210\323\4\345\375\127\32\356\32\205\305\141"
  "\327\143\202\215\133\22\10\273\106\141\20\166\75\342\260\141\60\43\20\166\215\302\40\254\6\230\12\203\306\114\14\312"
  "\117\305\276\113\14\105\143\62\6\345\147\143\337\45\206\242\61\37\203\362\23\262\357\62\11\12\325\4\103\21\34\47"
  "\236\176\301\45\64\31\120\225\42\304\267\15\51\276\55\57\306\267\51\162\174\333\20\344\322\120\354\273\114\21\345\333"
  "\206\54\337\226\27\346\143\120\354\376\373\377\1";

/* lib/ctype.wo (DEFLATEd, org. size 4203) */
static unsigned char file_l_38[489] =
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

/* lib/crt.void.wo (DEFLATEd, org. size 1416) */
static unsigned char file_l_39[450] =
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
static unsigned char file_l_40[121] =
  "\215\214\103\2\2\121\30\200\327\351\16\331\256\125\207\31\173\346\331\247\317\366\157\176\175\200\2\121\204\365\266\117\171"
  "\255\134\257\367\101\10\20\65\307\176\163\256\373\241\306\210\362\172\363\324\67\7\365\345\340\170\33\27\310\163\213\323"
  "\55\303\355\333\341\276\151\16\366\54\301\353\351\152\271\257\366\161\352\43\310\170\175\275\134\57\7\157\336\271\353\347"
  "\266\347\262\360\106\271\315\232\203\3\347\17\114\22\272\370\211\162\35\175\206\354\155\7";

/* lib/stdlib.wo (DEFLATEd, org. size 100557) */
static unsigned char file_l_41[9210] =
  "\264\130\367\223\243\66\30\375\71\355\177\140\274\276\211\67\327\44\114\21\276\336\173\172\317\227\161\204\312\206\71\14\4"
  "\311\133\47\371\333\43\214\27\33\204\157\175\133\146\134\304\323\367\336\367\324\145\217\146\71\237\247\302\31\52\315\323"
  "\44\376\342\123\307\31\45\263\42\57\265\63\140\245\36\70\3\125\14\7\316\150\57\315\143\232\72\103\203\115\14\342"
  "\214\146\163\355\44\143\167\167\167\267\207\63\23\263\274\74\62\264\272\120\323\226\145\144\61\66\147\61\11\72\321\124"
  "\51\261\40\114\247\165\161\52\151\222\32\252\234\147\314\31\326\330\244\125\351\214\12\132\322\331\102\254\51\333\316\225"
  "\56\223\154\257\66\317\212\243\106\262\306\47\65\152\153\365\225\113\241\346\251\376\140\22\123\310\330\254\350\146\131\302"
  "\227\226\306\270\126\102\167\263\324\350\145\266\45\25\131\67\111\215\156\53\46\312\62\313\7\315\167\63\23\26\317\223"
  "\305\147\115\333\334\316\3\152\165\147\15\157\323\235\326\74\326\107\205\60\302\211\342\311\136\262\352\301\5\76\131\242"
  "\147\65\316\26\123\5\145\302\22\133\240\37\53\246\363\64\77\20\145\107\154\211\156\53\66\317\22\245\271\121\123\161"
  "\371\276\221\252\321\111\205\155\53\164\100\125\62\125\31\55\324\337\271\236\26\245\330\117\304\1\66\302\222\117\17\312"
  "\104\257\32\335\37\71\71\215\333\146\254\56\154\112\144\373\111\231\147\323\75\241\317\362\265\26\172\45\26\124\162\54"
  "\324\266\106\32\302\245\333\51\312\234\115\305\141\162\246\215\46\260\111\271\351\20\310\63\55\312\331\264\122\123\235\275"
  "\175\275\252\26\330\314\147\371\74\323\66\277\251\332\310\257\102\222\214\66\123\257\246\66\250\355\277\111\121\237\205\223"
  "\351\262\313\235\221\70\254\205\117\221\301\156\163\0\232\222\371\274\305\362\114\351\346\154\343\124\323\106\206\53\74\164"
  "\352\172\232\46\173\331\75\354\14\106\137\377\364\366\355\56\40\64\350\41\24\30\161\356\354\323\162\31\117\234\201\211"
  "\354\274\206\17\155\154\323\353\267\255\143\11\1\66\336\56\232\170\300\103\210\302\107\37\14\102\40\120\170\144\104\37"
  "\75\331\30\204\303\14\220\17\161\375\42\20\215\237\267\352\103\11\30\215\215\6\110\37\144\364\0\104\4\150\374\15"
  "\171\7\44\2\61\176\11\332\3\312\200\271\300\42\27\60\107\46\350\345\1\20\367\367\327\20\11\16\44\0\306\0"
  "\341\135\143\370\56\304\122\125\212\234\177\3\330\237\27\317\101\212\347\103\30\0\15\101\342\276\341\310\172\206\43\242"
  "\20\105\255\127\154\174\131\350\157\20\263\23\300\306\236\170\1\2\37\3\361\36\124\55\304\2\210\17\42\376\256\172"
  "\100\302\104\335\174\2\230\31\4\204\33\0\246\17\100\12\1\62\4\341\121\240\30\342\0\342\170\2\204\41\23\0"
  "\104\34\75\273\17\221\367\366\46\160\271\203\200\43\210\251\21\251\372\203\23\210\102\210\135\340\56\104\354\156\345\203"
  "\30\231\240\322\146\56\304\336\265\150\134\265\225\22\340\376\16\310\340\225\341\170\20\271\306\210\4\206\157\200\60\122"
  "\376\275\105\177\170\117\101\362\12\247\276\173\242\201\360\10\244\373\303\344\66\104\274\312\102\30\60\11\210\230\354\177"
  "\134\373\367\325\137\200\251\311\11\222\126\57\212\237\344\100\31\37\1\12\200\21\310\100\242\51\140\37\60\6\32\101"
  "\44\177\0\24\366\365\167\122\140\324\235\375\270\177\346\144\275\60\357\17\26\4\320\270\7\307\10\276\354\201\51\2"
  "\22\330\231\37\76\6\44\173\246\171\24\100\104\372\262\342\152\326\42\277\213\63\12\21\275\143\105\173\200\342\37\1"
  "\271\35\347\373\57\1\207\135\327\324\257\106\116\220\216\355\362\153\300\4\364\312\361\161\25\54\307\277\267\374\262\0"
  "\250\367\37\20\336\352\24\206\163\220\56\220\140\147\55\230\376\171\370\214\1\352\135\34\134\271\310\332\335\136\175\375"
  "\374\325\327\257\176\374\315\260\67\160\260\305\371\372\321\327\243\257\66\355\210\45\315\270\107\246\112\210\365\205\350\72"
  "\325\131\170\57\350\41\210\131\241\217\232\375\173\105\361\152\212\327\353\312\363\54\127\111\165\320\144\64\165\104\131\346"
  "\345\304\231\147\64\116\205\243\163\247\24\272\114\304\276\160\226\111\146\42\323\116\175\355\124\220\155\150\107\74\147\357"
  "\205\126\266\41\214\110\257\45\337\356\50\123\161\133\225\354\166\35\163\213\155\354\144\337\275\0\227\134\200\33\235\237"
  "\33\240\13\160\307\347\345\316\265\44\123\135\215\154\153\144\6\100\266\70\7\103\331\217\213\265\62\133\257\12\126\145"
  "\151\270\341\152\115\256\360\15\72\210\265\270\162\75\65\71\305\111\277\16\166\273\134\274\116\307\13\274\326\267\164\60"
  "\351\162\245\154\366\15\147\201\212\46\242\245\202\205\315\154\167\231\327\224\77\366\325\214\146\175\275\153\256\117\64\113"
  "\130\5\73\247\127\274\241\152\135\222\207\131\375\270\214\111\163\126\135\372\44\137\240\315\163\134\14\333\200\354\2\11"
  "\356\2\356\160\135\271\276\116\336\332\23\272\371\107\243\256\130\204\337\122\102\57\262\234\21\274\272\135\342\140\205\250"
  "\171\334\342\51\213\267\115\166\151\47\161\355\40\336\100\247\172\262\255\245\205\130\364\206\25\247\326\14\353\274\24\116"
  "\56\245\321\274\207\116\327\230\255\154\73\362\126\0\345\334\316\353\332\171\263\217\317\313\267\261\202\267\11\42\75\176"
  "\115\174\172\346\117\337\72\224\227\171\321\111\143\117\23\173\270\113\241\347\345\242\335\75\113\142\272\130\23\162\365\103"
  "\146\11\14\166\333\353\104\316\164\173\245\320\142\150\257\225\12\156\117\174\325\176\334\247\151\33\20\355\307\16\173\146"
  "\261\167\302\16\177\47\352\50\354\140\324\21\331\301\330\326\301\156\127\10\173\226\222\157\51\5\75\112\226\45\154\171"
  "\162\55\117\156\307\323\45\356\61\15\60\356\2\136\27\360\273\100\320\5\302\56\100\272\100\164\305\133\33\272\202\275"
  "\315\132\106\324\216\63\130\15\305\6\173\357\14\161\23\221\27\316\320\265\65\344\114\257\254\246\71\345\144\252\272\373"
  "\13\136\106\310\13\261\233\336\31\207\53\44\23\247\332\255\377\171\66\354\124\42\125\242\337\203\35\276\171\217\275\270"
  "\153\361\317\322\117\306\317\350\232\13\130\125\377\267\167\55\272\215\353\106\373\125\12\354\2\370\201\305\6\22\165\165"
  "\373\22\75\117\140\370\242\44\152\154\331\225\344\44\173\236\376\67\343\65\246\324\307\350\23\271\164\216\163\326\213\242"
  "\47\242\105\162\156\234\31\16\207\243\361\36\132\7\243\223\200\64\362\347\123\34\147\200\62\140\52\42\7\115\140\355"
  "\1\121\220\133\1\20\154\34\160\261\33\116\21\331\304\12\215\70\266\10\101\7\341\376\251\224\156\253\373\73\10\222"
  "\15\301\313\311\150\315\72\214\274\4\27\203\50\272\72\61\220\36\332\232\115\162\273\242\161\235\120\301\300\200\216\114"
  "\45\330\343\157\62\345\167\25\247\105\132\46\171\132\332\10\110\136\67\140\331\2\123\161\146\373\112\330\364\363\16\147"
  "\224\327\140\54\60\134\156\200\300\373\42\334\362\207\330\247\324\260\117\311\127\343\307\14\315\25\12\224\1\57\260\165"
  "\77\311\261\337\242\314\113\113\133\155\347\7\20\47\213\133\54\16\172\211\213\352\214\75\140\105\347\137\327\317\363\3"
  "\240\261\35\341\371\103\177\356\260\154\347\365\275\246\253\314\157\74\10\375\375\344\51\74\167\24\256\334\154\72\165\121"
  "\72\103\302\226\40\154\245\3\154\0\21\264\130\241\6\40\321\202\134\251\341\210\213\53\67\34\137\12\157\323\341\140"
  "\131\276\314\140\156\275\341\203\251\5\40\241\22\376\152\16\244\367\211\206\316\54\14\205\232\233\12\265\204\325\56\220"
  "\70\254\14\335\5\26\7\150\22\1\356\243\64\153\71\5\4\256\134\241\27\325\257\271\250\124\343\101\330\201\253\52"
  "\44\355\263\300\212\11\245\123\304\30\137\374\304\12\52\276\166\5\25\253\111\61\272\137\325\120\161\152\123\121\31\314"
  "\55\40\1\241\214\237\101\111\345\146\124\42\62\264\324\314\324\122\61\172\150\2\215\323\122\311\160\251\344\126\35\221"
  "\3\105\57\247\245\4\75\6\2\327\122\271\213\226\232\211\142\32\74\304\21\121\123\141\150\137\4\127\123\50\241\42"
  "\313\370\346\347\325\123\12\365\24\213\230\5\7\102\105\44\0\166\105\132\223\370\165\200\221\257\332\264\171\166\12\154"
  "\247\300\64\242\66\155\276\235\212\115\305\230\30\152\63\126\3\275\211\56\230\200\343\262\166\225\305\305\100\337\112\340"
  "\103\176\212\342\204\221\326\320\305\334\135\36\254\342\15\223\257\271\106\26\361\47\135\321\211\61\176\236\25\266\221\33"
  "\67\245\237\72\320\217\150\175\345\344\233\306\112\64\75\74\45\250\367\57\40\73\263\320\172\37\227\230\54\106\174\363"
  "\243\365\176\263\366\370\143\331\352\343\31\374\325\150\225\363\234\160\207\251\213\176\367\77\47\251\307\47\70\106\355\314"
  "\74\334\373\74\325\347\146\20\104\7\231\104\132\167\175\333\357\326\323\200\252\15\240\152\2\24\234\352\236\323\42\344"
  "\261\172\70\65\0\240\300\312\206\276\242\107\163\73\147\353\270\305\25\202\231\271\364\316\147\31\16\122\333\311\51\275"
  "\273\34\12\372\30\270\305\40\257\53\71\100\147\44\260\224\65\270\247\114\47\75\247\6\103\117\206\41\116\244\11\262"
  "\23\42\264\27\246\264\236\12\10\227\33\142\124\204\20\43\271\337\41\142\4\346\157\44\343\144\173\330\204\247\206\27"
  "\377\113\340\277\74\0\103\105\232\13\253\64\27\30\262\21\204\104\42\270\43\322\210\5\345\347\53\15\102\55\160\61"
  "\215\270\61\64\342\346\246\21\157\32\361\246\21\157\32\361\167\326\210\3\225\110\165\142\236\242\116\324\215\123\165\242"
  "\176\371\256\172\355\217\20\316\217\277\315\273\233\246\274\151\312\233\246\304\165\201\353\134\336\273\152\375\11\230\110\353"
  "\307\151\325\74\345\132\125\277\105\265\152\236\272\152\325\376\145\67\337\267\273\265\251\107\27\247\135\377\371\161\151\76"
  "\356\155\316\346\352\355\35\311\261\176\34\74\157\314\347\345\343\340\171\360\373\253\74\222\313\12\20\164\206\226\237\4"
  "\271\67\144\121\170\372\372\363\147\241\357\153\174\27\275\375\123\247\377\354\277\251\342\37\377\367\257\177\304\111\252\342"
  "\242\120\263\177\374\13\31\266\200\131\214\250\30\304\234\240\105\206\20\66\312\3\104\312\37\51\346\372\45\62\310\46"
  "\14\356\113\177\334\161\10\216\373\22\161\137\142\13\301\175\211\132\211\147\217\231\362\142\277\236\121\362\134\244\322\355"
  "\122\10\362\33\60\264\51\331\127\44\0\33\161\203\43\352\7\15\45\366\337\20\210\74\372\263\371\11\325\211\262\133"
  "\257\347\347\121\105\231\55\314\73\43\313\307\172\240\374\66\273\143\3\70\222\235\156\14\175\27\342\363\135\300\102\131"
  "\27\45\146\75\301\53\55\102\123\333\217\357\304\67\1\143\345\161\217\211\57\154\335\106\140\17\200\355\146\7\62\76"
  "\151\310\222\20\220\54\33\207\65\4\204\105\150\170\27\202\200\200\204\216\35\122\44\200\376\344\140\241\113\352\101\16"
  "\146\305\10\355\1\155\110\111\105\50\351\300\1\116\220\120\213\316\367\161\204\232\224\336\123\355\7\52\357\35\47\256"
  "\341\51\311\30\231\220\313\31\130\1\203\136\341\330\307\362\126\203\122\331\173\203\350\27\23\201\75\7\244\322\53\124"
  "\171\320\265\66\362\7\54\32\157\354\144\271\322\70\110\6\310\237\306\61\263\202\115\75\140\65\66\133\367\330\316\107"
  "\167\174\230\150\202\353\102\232\340\360\126\341\344\20\320\103\322\341\257\242\63\211\376\13\5\13\154\174\3\102\12\6"
  "\125\34\42\330\307\46\114\130\40\71\0\205\117\366\312\261\31\17\302\110\21\325\70\355\356\320\254\7\372\346\331\324"
  "\67\155\277\73\155\42\307\342\177\265\31\377\273\67\275\270\176\360\170\36\20\145\360\231\20\133\66\344\175\173\104\145"
  "\176\174\161\176\100\22\371\15\14\154\221\55\334\263\256\33\130\353\311\306\167\77\367\216\327\106\11\240\324\312\303\66"
  "\63\272\213\344\237\336\146\106\172\227\31\351\335\245\214\53\161\50\56\214\76\341\234\332\216\50\156\210\21\322\130\103"
  "\212\50\42\211\375\243\244\302\221\64\74\107\374\360\174\250\302\161\104\240\10\316\21\140\67\160\44\265\162\44\305\213"
  "\123\256\260\174\217\337\344\370\56\263\103\343\315\57\364\110\2\350\16\255\62\307\203\143\307\27\240\27\316\154\14\363"
  "\320\173\145\254\72\314\320\124\17\40\222\60\135\0\371\154\300\372\106\254\117\123\261\323\60\103\344\76\140\65\311\104"
  "\70\1\267\273\335\261\103\145\332\335\147\63\76\322\230\126\170\337\331\334\376\247\353\252\13\301\17\355\236\160\165\165"
  "\223\202\10\317\356\373\274\316\143\173\353\145\117\71\100\202\2\321\162\263\350\244\163\357\342\62\211\62\25\317\252\357"
  "\252\210\271\365\26\224\161\153\353\3\310\267\130\225\32\220\344\56\215\124\251\222\274\250\276\45\245\300\41\152\215\220"
  "\200\234\372\75\311\262\366\2\363\47\275\312\273\64\123\105\254\262\352\233\52\42\323\264\272\221\53\365\42\327\367\237"
  "\344\122\167\263\244\54\222\254\54\253\357\311\354\127\311\245\220\134\66\125\7\130\101\23\124\140\200\160\203\274\56\263"
  "\0\337\360\176\27\45\47\234\343\112\33\15\366\204\247\167\170\225\221\133\360\313\257\5\77\131\133\234\305\312\233\305"
  "\170\104\56\155\234\4\341\65\120\150\26\227\26\374\312\353\302\17\365\1\363\77\336\262\21\272\161\7\144\75\360\100"
  "\206\317\225\315\43\251\324\150\221\257\272\67\237\327\165\73\150\60\3\11\377\66\43\7\177\310\343\345\53\145\135\346"
  "\270\50\121\341\217\213\74\155\126\244\324\111\311\50\225\105\105\122\346\107\45\23\225\304\331\140\345\376\274\314\170\236"
  "\236\274\236\62\115\363\42\115\343\352\133\354\257\315\361\364\152\174\225\12\157\62\105\243\35\217\55\336\142\125\121\132"
  "\70\155\172\136\332\305\136\107\223\60\15\110\45\243\167\250\244\213\75\66\130\51\320\371\204\141\312\302\260\337\213\322"
  "\204\224\350\142\311\157\62\136\121\316\342\144\74\374\134\16\302\364\70\43\71\207\210\160\37\332\323\167\264\206\145\247"
  "\25\60\112\212\107\56\116\307\25\160\12\124\333\334\314\306\75\10\117\344\13\202\265\100\215\177\373\3\210\241\371\217"
  "\7\377\17\134\254\316\120\243\64\343\317\276\305\114\41\302\21\340\270\370\337\1\102\140\201\0\30\271\65\76\161\226"
  "\70\227\151\174\102\15\17\1\342\140\242\25\310\71\53\254\163\244\61\52\32\24\55\232\175\212\52\260\65\22\210\103"
  "\60\361\17\364\151\76\126\212\376\270\42\51\332\364\37\41\105\17\134\212\276\373\213\21\252\54\56\106\162\156\104\216"
  "\144\335\156\55\113\7\176\50\353\241\134\171\314\360\24\2\213\212\142\226\144\171\121\175\127\342\304\213\132\206\3\137"
  "\254\46\360\361\166\133\176\176\250\244\146\301\165\32\151\24\325\100\372\316\36\306\133\117\212\345\12\320\223\43\40\325"
  "\244\250\147\305\252\245\7\57\107\176\277\357\167\247\270\204\134\107\222\306\316\274\224\4\341\212\373\372\265\132\103\246"
  "\24\15\140\220\253\236\177\155\151\356\317\34\201\150\340\55\24\362\23\323\336\57\350\2\253\251\40\313\4\223\134\155"
  "\101\57\174\237\127\307\341\253\32\120\267\107\24\255\32\253\200\374\60\241\10\133\273\361\127\117\214\370\342\106\10\275"
  "\226\73\252\70\124\335\350\203\42\35\170\117\210\137\363\335\322\25\354\167\74\122\37\2\354\205\302\3\167\115\373\244"
  "\100\36\356\176\344\230\230\47\21\170\231\146\260\371\176\231\256\16\153\73\310\322\166\160\143\122\104\351\202\4\314\56"
  "\116\300\50\44\1\271\312\42\124\26\122\20\43\10\203\206\363\347\244\112\321\174\135\215\177\166\151\277\62\237\233\352"
  "\301\154\70\22\145\377\136\351\44\71\66\212\315\163\243\265\62\237\233\170\120\264\103\231\317\353\352\165\157\266\124\257"
  "\346\363\323\350\347\151\272\252\152\216\224\265\64\356\372\301\270\115\65\174\255\257\127\117\77\314\266\326\74\350\132\231"
  "\277\356\53\363\371\321\174\173\143\76\166\261\371\274\134\232\317\225\374\376\351\276\77\3\200\105\320\22\103\213\372\174"
  "\356\265\157\35\205\165\354\335\123\115\50\301\20\117\170\207\216\163\132\175\374\255\352\225\277\363\104\137\71\57\325\211"
  "\57\356\372\11\200\65\25\35\356\274\314\341\214\7\216\146\140\220\351\167\103\366\53\176\173\37\157\21\303\351\117\56"
  "\55\123\35\71\241\225\234\24\221\7\356\233\331\113\45\214\361\211\207\216\21\333\22\343\336\216\136\65\231\41\53\60"
  "\46\52\300\361\20\73\210\256\3\122\16\54\154\142\262\247\162\340\231\207\170\170\174\204\10\364\6\17\15\113\27\374"
  "\316\5\241\106\74\103\101\261\103\205\135\231\177\53\241\110\312\125\301\315\47\163\171\35\23\114\111\320\122\72\174\166"
  "\376\52\14\161\114\345\257\272\142\376\52\212\51\347\257\372\344\374\45\304\105\220\140\166\64\333\62\145\0\313\307\111"
  "\301\371\4\244\340\177\120\237\201\116\53\35\34\317\272\300\172\371\123\112\357\7\211\270\221\236\241\276\120\133\65\36"
  "\27\360\302\104\120\35\34\76\124\126\161\320\62\370\76\60\344\63\122\4\337\123\110\245\202\166\345\16\124\232\41\120"
  "\134\103\210\373\217\341\27\247\145\125\215\321\330\7\235\304\206\116\30\100\171\330\257\12\124\45\15\12\332\341\315\315"
  "\160\123\142\212\33\24\322\313\161\142\334\227\306\157\113\237\270\23\143\335\361\214\332\25\123\342\133\220\143\351\127\144"
  "\61\240\356\57\74\170\127\164\360\220\331\147\2\103\204\120\136\334\132\250\161\13\32\320\134\0\276\255\337\226\15\121"
  "\223\152\171\336\256\234\21\213\205\361\133\274\123\152\327\153\255\120\46\214\161\104\227\12\270\32\12\167\260\224\322\3"
  "\326\67\254\74\240\6\54\273\21\353\146\307\254\141\247\322\152\174\320\55\4\62\143\110\346\262\233\304\274\104\116\125"
  "\155\333\354\376\371\366\377\103\241\377\126\67\367\50\334\277\251\220\2\325\277\47\52\7\313\341\117\366\337\215\240\22"
  "\54\147\211\123\61\57\360\101\16\35\375\253\244\245\166\170\160\140\257\162\50\120\233\3\220\354\50\222\74\212\60\242"
  "\34\0\13\227\174\76\165\371\264\172\7\53\114\252\373\130\302\102\74\213\377\321\322\27\232\36\275\171\13\363\41\254"
  "\22\324\10\31\316\331\170\246\213\130\75\105\51\237\344\271\56\200\232\62\31\176\4\337\147\361\221\72\250\313\0\160"
  "\333\0\133\362\222\256\200\67\357\156\247\113\25\207\110\175\147\225\23\205\154\343\63\142\126\7\243\125\25\7\252\323"
  "\350\177\43\34\161\362\27\5\377\42\210\16\376\207\232\221\235\53\40\361\41\327\301\325\271\62\106\36\345\263\244\214"
  "\322\352\133\36\205\270\43\172\55\367\301\203\40\210\27\46\43\122\163\203\210\203\232\271\331\171\374\146\35\20\153\166"
  "\205\365\37\374\353\335\106\200\241\64\136\13\212\202\216\36\325\243\252\210\220\6\66\330\36\225\105\150\210\237\153\107"
  "\334\77\272\24\273\160\126\160\27\257\363\100\360\11\17\326\367\237\352\46\277\123\52\211\342\114\35\305\53\217\1\254"
  "\0\125\4\240\274\335\307\342\342\117\142\250\363\315\124\101\374\365\62\22\124\26\77\313\43\250\154\226\145\52\211\253"
  "\157\52\117\302\153\73\304\321\125\31\372\27\210\276\136\12\360\5\220\150\330\146\167\145\36\25\171\234\250\243\374\53"
  "\176\377\316\143\137\32\313\152\113\357\146\151\224\147\171\232\353\331\122\56\353\344\174\30\342\63\20\261\223\266\0\147"
  "\206\155\310\362\44\202\205\107\224\114\372\177\366\160\231\177\212\366\143\365\372\1\51\332\133\63\43\373\251\332\367\146"
  "\113\133\15\136\171\134\154\356\315\226\145\335\167\106\13\336\320\243\211\334\353\166\67\174\343\143\163\271\37\315\237\237"
  "\155\271\335\267\57\140\370\346\55\157\47\244\44\257\371\73\313\60\51\311\230\366\162\313\67\176\374\73\47\34\3\156"
  "\350\165\254\36\361\226\43\107\30\311\226\25\60\216\3\222\144\170\310\30\0\52\74\277\77\337\254\270\30\322\161\244"
  "\202\143\215\240\13\326\60\75\270\207\123\211\222\147\27\43\112\21\205\247\11\102\36\212\46\120\272\35\376\360\317\204"
  "\177\6\204\75\22\341\327\237\54\21\336\156\255\110\251\270\345\324\104\151\344\106\76\61\174\262\5\220\170\321\267\15"
  "\262\224\165\332\265\350\4\160\64\150\26\364\372\123\63\217\57\207\353\113\202\46\224\0\66\111\227\140\271\317\267\334"
  "\147\244\211\102\42\135\142\145\164\325\246\132\365\50\367\341\363\352\156\251\107\376\311\343\261\372\313\223\307\313\350\226"
  "\74\176\113\36\277\45\217\337\222\307\157\311\343\277\140\344\266\256\105\130\156\31\345\313\317\235\121\316\335\150\121\125"
  "\205\241\61\313\257\134\174\362\144\132\111\157\171\111\34\135\76\172\74\145\353\310\267\201\113\227\33\247\100\232\126\123"
  "\2\365\206\321\52\244\43\323\346\311\270\27\122\331\153\226\337\22\362\57\273\306\53\114\231\211\212\54\154\106\376\215"
  "\242\112\141\305\103\276\221\200\235\4\374\6\263\0\60\314\223\251\154\345\161\333\35\56\337\164\174\43\164\346\7\67"
  "\260\74\35\117\132\4\24\256\147\355\337\134\76\235\274\342\270\176\50\42\304\24\33\335\117\64\257\27\172\33\257\257"
  "\224\342\27\42\154\324\151\53\7\60\231\31\161\63\127\372\274\33\227\146\5\102\40\57\352\241\264\45\70\114\222\277"
  "\230\311\237\307\344\104\56\302\336\70\10\210\235\54\4\317\257\127\102\344\325\141\251\303\20\254\343\101\132\205\32\201"
  "\360\131\333\65\204\100\210\102\341\276\165\10\31\154\366\263\47\50\224\166\27\13\157\111\42\136\76\137\263\331\370\13"
  "\213\225\321\343\34\324\211\62\136\127\343\244\63\51\275\351\235\263\53\163\177\256\74\63\4\305\103\313\371\27\245\165"
  "\360\62\157\31\155\306\227\355\60\201\255\343\205\337\7\71\127\207\315\306\154\131\115\375\342\155\303\25\236\36\335\355"
  "\273\361\335\344\30\336\175\240\244\35\166\44\56\61\317\330\13\134\10\71\132\317\31\10\271\205\226\254\30\324\264\57"
  "\232\166\216\1\312\316\205\327\270\62\374\117\44\331\71\160\347\23\335\356\167\233\335\113\325\42\65\311\253\341\211\32"
  "\204\110\102\33\74\300\35\321\231\216\123\65\343\51\56\106\253\165\255\64\350\335\300\73\42\104\1\316\132\47\350\123"
  "\371\36\307\351\171\370\51\216\316\43\35\170\230\72\53\112\332\256\202\233\57\311\365\177\213\343\112\123\147\341\45\333"
  "\241\143\147\137\66\334\52\161\313\102\16\321\272\375\142\45\373\203\140\253\335\177\15\162\104\370\1\160\140\154\240\3"
  "\260\223\333\224\360\347\277\1\314\225\177\142\202\177\305\133\76\117\374\336\74\261\212\56\234\0\301\141\50\1\127\241"
  "\243\167\142\242\133\42\220\360\26\267\27\170\16\52\113\36\57\236\370\157\0\202\12\174\221\374\345\171\55\161\224\371"
  "\45\266\100\141\225\165\247\242\257\357\223\35\76\342\216\116\107\130\152\300\360\166\1\276\202\264\67\362\31\47\237\143"
  "\372\357\22\324\200\100\7\254\37\360\316\302\256\214\320\204\124\237\247\166\152\150\175\121\376\365\372\42\216\302\351\213"
  "\330\115\137\174\111\100\143\100\0\366\143\326\162\62\145\55\13\300\36\253\231\7\121\345\213\106\262\352\225\312\342\142"
  "\66\53\343\44\57\63\225\226\303\27\146\112\305\261\212\124\122\104\151\34\315\242\54\217\270\106\10\36\223\375\14\112"
  "\46\373\74\112\206\374\101\75\256\0\76\325\272\132\205\16\252\276\45\71\76\57\66\346\246\177\145\15\242\362\333\157"
  "\136\127\236\56\173\367\215\347\73\42\253\371\235\267\100\250\306\112\371\343\212\76\46\116\10\212\21\320\162\44\105\236"
  "\135\210\24\263\350\343\51\1\130\121\112\100\354\216\307\330\232\176\167\60\127\127\263\357\133\210\252\101\333\162\321\125"
  "\343\337\274\205\273\372\373\335\363\375\140\345\142\160\256\203\53\352\306\367\305\352\255\331\260\66\137\137\353\27\214\226"
  "\105\363\203\105\342\342\113\176\101\14\130\255\311\213\221\146\337\73\347\317\64\342\246\51\0\100\150\346\114\362\251\65"
  "\317\354\273\30\230\326\60\213\45\372\220\203\241\145\24\361\74\17\320\313\175\130\55\230\0\135\200\17\55\261\71\342"
  "\21\217\70\360\124\111\156\46\143\122\215\243\234\262\3\317\13\336\323\223\222\276\350\55\343\122\160\160\247\34\126\10"
  "\52\107\207\140\162\303\155\204\347\321\242\312\104\223\353\164\36\22\222\46\346\345\162\307\134\15\311\60\273\330\321\123"
  "\170\222\247\131\320\335\262\17\4\111\260\230\51\321\337\351\327\360\24\243\12\370\152\304\117\10\306\254\1\340\27\334"
  "\26\344\323\215\1\301\36\342\33\7\27\301\11\277\234\40\370\24\10\224\277\376\74\244\273\340\161\110\170\262\50\74"
  "\326\204\135\63\150\10\64\242\20\72\13\4\70\55\335\20\326\302\251\137\126\61\212\250\30\304\7\336\21\115\40\150"
  "\371\250\42\74\262\340\33\310\346\212\126\74\4\16\201\110\340\41\100\14\323\356\327\263\134\327\165\375\214\105\357\365"
  "\246\61\320\370\155\265\235\37\4\361\227\166\261\237\37\333\141\106\275\15\305\12\126\170\13\70\74\377\256\134\37\40"
  "\275\103\251\204\147\150\21\266\303\115\5\37\13\301\347\250\376\353\107\13\221\27\330\322\371\244\353\322\335\173\6\273"
  "\167\272\315\46\124\361\130\112\326\357\130\370\334\30\170\346\265\233\60\24\162\55\273\245\324\262\133\62\132\105\165\134"
  "\170\173\236\343\64\35\215\135\75\10\121\3\144\4\172\34\213\261\371\361\301\141\73\125\370\104\271\34\54\332\263\175"
  "\275\322\230\30\220\155\112\220\267\273\110\220\167\133\67\32\307\377\151\130\274\236\32\60\304\13\61\135\222\115\371\251"
  "\22\37\311\122\201\305\7\372\22\7\40\347\277\322\106\364\27\270\3\22\365\7\115\212\243\163\161\204\125\105\172\303"
  "\226\52\260\121\326\62\350\167\27\15\235\5\377\273\60\10\125\335\204\277\341\34\20\173\176\257\22\21\2\57\105\16"
  "\273\21\122\270\22\33\350\326\367\325\311\135\170\31\202\31\302\313\220\317\230\344\130\61\274\311\271\131\230\337\311\302"
  "\304\267\265\352\157\221\375\113\102\60\275\75\102\310\5\57\11\212\150\5\123\70\365\21\26\271\57\44\155\162\147\50"
  "\214\42\102\275\23\164\125\302\316\141\246\124\222\24\52\322\351\174\151\121\144\145\204\51\175\360\112\361\356\262\354\246"
  "\153\160\315\334\117\115\272\370\175\345\64\221\14\26\261\12\40\125\301\151\201\351\77\127\42\106\375\156\63\40\336\46"
  "\30\345\22\165\21\312\101\213\212\323\42\55\223\74\55\247\25\145\120\347\367\13\373\373\204\246\20\133\236\104\145\40"
  "\363\124\72\337\44\364\60\244\335\341\363\311\250\175\217\167\40\372\317\117\326\16\33\44\330\147\220\266\360\266\341\200"
  "\266\101\67\375\156\264\370\117\273\150\326\151\151\42\375\332\35\226\65\55\167\161\332\314\171\134\234\6\272\234\46\64"
  "\275\343\70\107\367\130\141\172\155\226\140\136\26\256\45\327\11\125\220\11\141\137\111\212\375\100\124\307\5\344\64\64"
  "\310\211\362\4\131\66\215\257\103\151\125\231\212\323\131\224\314\342\2\361\303\23\46\54\325\207\320\160\0\220\212\364"
  "\162\237\364\105\55\313\111\53\247\14\161\156\27\141\204\210\147\105\260\174\337\127\42\163\244\266\352\137\202\146\112\320"
  "\114\10\232\42\247\327\200\146\170\56\20\345\255\165\267\330\60\375\164\66\137\250\266\361\2\337\111\363\317\273\252\202"
  "\100\220\30\206\221\0\222\206\173\222\271\65\301\324\217\120\14\104\3\361\274\330\200\231\361\260\53\34\121\222\20\37"
  "\47\121\21\161\326\363\311\2\54\352\237\164\341\242\32\36\136\257\325\11\360\102\130\125\304\75\0\102\114\364\26\313"
  "\243\310\211\350\275\75\152\321\3\231\26\361\177\131\164\365\274\153\26\373\356\161\327\317\367\155\365\134\127\57\361\77"
  "\367\355\156\65\257\136\353\176\312\254\275\176\121\246\75\75\17\105\376\276\241\36\126\35\217\6\273\167\115\137\265\333"
  "\371\152\167\150\172\233\126\224\14\35\311\240\61\245\300\30\107\343\320\361\231\370\33\54\350\14\321\102\322\77\65\77"
  "\116\200\203\21\113\176\337\260\24\5\210\260\206\273\250\145\212\2\12\202\176\161\321\37\72\253\147\174\376\121\4\364"
  "\104\55\115\253\272\71\312\325\4\0\352\246\356\353\43\236\177\126\363\252\171\256\333\135\43\340\340\157\47\340\104\4"
  "\253\266\335\265\203\242\112\207\355\174\337\267\235\331\372\263\377\174\171\270\267\376\0\135\74\213\61\135\364\370\47\121"
  "\341\217\177\360\345\63\147\316\64\367\113\7\362\271\164\55\171\215\232\261\276\207\110\150\27\210\362\74\213\100\167\24"
  "\264\156\176\34\26\226\355\233\230\161\243\100\50\45\5\5\365\200\374\4\206\350\1\362\315\37\131\11\74\67\330\13"
  "\254\124\300\2\367\154\173\174\330\255\220\216\262\12\177\15\250\367\120\263\301\262\32\207\205\123\310\56\342\210\215\71"
  "\355\175\133\111\141\76\101\2\17\251\0\24\76\13\227\345\213\111\261\77\61\10\332\41\11\50\132\7\365\131\200\144"
  "\31\204\206\351\244\222\273\5\357\225\61\111\323\257\334\77\360\202\300\301\227\126\60\136\226\271\172\55\352\327\301\214"
  "\247\14\21\347\256\6\340\176\75\177\151\353\263\273\42\205\360\201\33\325\166\337\377\20\71\12\55\150\304\125\72\216"
  "\161\34\131\334\243\323\63\204\204\27\333\212\272\356\233\252\1\67\310\352\342\235\306\363\121\11\322\123\326\165\133\67"
  "\17\72\374\173\234\36\134\21\335\206\112\0\135\100\370\372\67\161\137\140\242\143\73\174\62\34\324\11\261\304\220\375"
  "\113\276\60\16\144\361\234\122\272\0\301\204\272\315\152\273\347\27\6\374\147\205\345\345\166\357\47\217\375\77\75\34"
  "\36\176\352\123\241\372\147\71\364\317\124\273\202\100\272\174\202\325\176\145\235\152\217\145\127\55\332\325\243\250\217\237"
  "\15\103\375\361\124\375\240\111\27\333\152\273\64\233\264\357\154\266\34\205\220\252\241\155\275\256\327\257\77\137\223\266"
  "\335\362\77\146\333\272\276\277\227\376\16\27\356\65\240\50\75\370\253\55\12\146\275\364\165\206\231\347\215\214\274\253"
  "\211\105\202\6\62\343\231\42\60\312\221\121\320\146\177\125\363\102\124\305\274\156\326\165\133\255\372\63\253\216\363\42"
  "\247\300\7\320\74\360\363\223\5\50\226\206\106\147\301\57\204\301\74\204\332\110\140\340\35\110\7\162\164\124\151\220"
  "\113\72\62\156\265\41\62\3\335\204\156\36\145\21\334\364\105\123\275\74\54\366\246\156\70\66\360\54\204\267\327\330"
  "\47\74\41\250\15\313\316\20\77\353\230\263\0\267\264\31\320\244\30\41\36\2\12\327\144\120\351\106\147\264\207\105"
  "\341\65\257\211\10\303\377\333\31\321\150\375\10\301\100\177\133\200\352\137\304\111\342\147\346\343\300\12\354\343\301\263"
  "\62\237\273\227\305\176\137\255\121\50\161\75\43\335\354\157\342\375\56\324\141\350\220\212\157\211\316\40\262\106\64\63"
  "\256\75\4\226\125\145\72\123\1\277\25\114\372\325\126\223\6\257\355\143\162\341\270\146\224\27\314\101\121\202\344\173"
  "\175\177\270\266\317\65\146\1\300\152\10\236\304\304\63\173\103\217\4\366\52\234\271\246\66\223\243\51\0\1\112\260"
  "\327\70\362\125\113\33\323\116\42\223\140\251\46\362\257\166\274\112\133\313\116\236\143\314\75\4\351\104\76\323\14\327"
  "\153\111\364\102\350\342\140\31\344\126\201\134\325\224\345\101\367\17\344\64\174\331\76\275\255\254\201\336\347\371\126\147"
  "\15\17\253\6\231\27\233\165\147\310\227\160\204\213\142\267\55\161\262\14\335\234\42\232\375\257\324\56\272\256\72\6"
  "\133\346\363\323\37\363\373\105\275\31\271\343\315\21\311\354\156\337\51\363\110\346\75\64\365\21\312\67\322\342\352\307"
  "\123\73\207\217\4\61\157\172\317\243\275\240\150\3\160\102\41\47\142\305\70\101\260\40\162\273\77\154\66\236\162\173"
  "\176\107\32\206\317\212\36\373\5\54\364\370\16\131\227\207\325\123\325\167\114\52\311\171\64\11\226\200\164\56\153\237"
  "\320\237\164\44\163\135\176\134\22\331\46\25\47\111\305\200\340\254\42\61\170\37\162\160\134\160\110\242\133\210\136\207"
  "\335\13\351\112\56\376\301\372\6\31\365\25\245\300\52\237\170\20\113\365\367\133\331\136\211\130\102\15\227\125\206\235"
  "\261\104\41\202\352\41\376\312\25\275\314\103\203\174\250\342\310\203\323\337\105\103\310\102\106\7\223\30\365\103\263\251"
  "\233\21\167\124\12\100\57\203\244\146\42\216\54\351\201\204\102\77\226\317\261\215\317\1\354\203\270\173\36\44\42\46"
  "\54\30\204\315\332\167\150\302\103\102\1\62\201\257\246\344\124\366\244\17\361\242\273\307\51\313\315\313\217\136\166\177"
  "\16\137\250\365\31\374\12\133\273\303\152\165\321\357\36\112\4\353\110\217\366\307\225\130\346\116\254\26\354\200\340\255"
  "\137\166\310\2\73\76\2\234\346\64\214\210\0\313\153\346\126\364\336\261\257\104\17\145\55\211\160\261\242\6\244\226"
  "\166\123\115\72\111\347\323\305\205\314\207\304\366\75\20\307\241\10\144\142\213\301\276\302\110\16\61\67\62\65\110\257"
  "\104\323\144\375\221\57\132\61\41\100\205\52\272\344\103\204\0\246\273\26\41\0\310\76\130\10\34\331\157\377\103\136"
  "\5\314\56\277\221\41\16\226\277\177\202\36\122\250\155\214\257\353\201\144\364\237\25\163\361\270\64\261\115\335\262\346"
  "\337\76\40\245\24\77\326\53\317\174\205\206\370\152\367\165\263\136\76\365\347\134\1\161\316\26\77\264\326\241\161\317"
  "\345\346\151\133\67\366\23\125\204\370\64\350\173\365\50\214\353\74\20\321\307\161\374\305\4\63\52\316\230\340\145\11"
  "\230\35\136\334\60\47\102\241\33\144\365\250\370\131\234\325\252\221\31\240\234\35\273\362\243\212\51\327\62\253\267\204"
  "\170\71\351\377\331\60\74\353\337\103\245\35\42\121\340\360\327\346\163\263\67\237\167\355\103\63\354\361\45\35\356\6"
  "\276\144\341\166\2\110\176\117\333\201\2\314\274\121\177\27\104\134\202\20\147\63\45\236\315\314\176\345\154\146\311\253"
  "\254\1\65\54\72\274\300\165\25\0\331\231\5\331\324\27\131\262\303\12\122\11\74\174\124\232\350\122\162\24\250\251"
  "\20\204\25\171\144\141\105\346\317\212\306\16\16\60\202\167\104\333\23\236\374\334\56\56\3\370\242\374\36\51\71\131"
  "\45\173\16\234\112\66\22\142\127\60\163\161\357\35\72\224\101\264\255\100\50\274\324\267\35\42\122\304\22\140\140\257"
  "\211\66\303\144\15\114\243\131\355\177\310\162\142\12\10\110\51\43\102\34\17\121\47\137\222\16\60\47\16\112\344\7"
  "\35\132\60\30\137\122\47\223\41\163\111\147\207\23\305\57\231\237\140\141\117\276\220\225\377\102\306\211\161\117\207\76"
  "\5\333\36\61\254\374\167\156\300\104\330\273\5\2\56\167\270\304\361\113\227\52\266\3\127\172\153\172\322\304\165\366"
  "\327\261\34\262\325\0\262\325\10\144\222\271\373\247\103\232\27\311\47\105\215\332\5\312\57\312\121\256\313\44\11\345"
  "\113\300\266\26\161\300\104\165\152\340\33\373\110\22\105\100\263\31\100\102\144\105\6\330\351\20\50\40\63\264\352\355"
  "\46\315\53\313\113\256\4\213\110\353\47\334\264\136\166\243\27\61\222\43\266\254\6\114\47\30\35\37\246\353\16\144"
  "\201\357\167\151\11\147\145\257\100\314\371\64\214\67\6\312\233\337\3\347\1\322\166\254\261\372\74\257\104\34\111\3"
  "\140\175\36\6\372\214\15\33\20\353\243\202\24\234\217\17\103\214\333\375\327\101\214\351\260\65\33\326\125\3\106\310"
  "\277\64\313\365\127\346\27\205\255\121\232\266\353\73\154\241\355\110\66\323\114\165\1\112\10\170\170\262\34\270\266\332"
  "\272\3\247\5\307\241\230\300\321\30\355\332\37\307\266\375\217\320\127\362\67\206\210\157\156\62\176\223\361\277\237\214"
  "\17\204\174\272\224\347\251\41\345\272\301\113\312\157\122\56\137\41\355\344\321\52\110\245\203\320\204\221\162\371\204\51"
  "\7\316\127\312\343\374\342\142\276\135\156\252\106\304\374\355\21\152\341\211\220\373\357\353\273\321\35\224\200\323\357\136"
  "\330\106\137\136\24\300\117\317\344\60\217\41\202\271\264\203\263\273\125\64\170\66\37\233\301\363\306\174\354\337\131\342"
  "\110\53\274\332\267\67\256\177\326\233\256\372\57\164\253\367\374\270\15\244\176\205\61\214\15\17\163\64\53\4\32\17"
  "\122\374\257\160\65\357\336\204\25\334\341\366\61\4\115\16\375\175\71\357\27\107\231\6\4\172\122\14\247\277\144\51"
  "\234\225\275\211\207\32\201\1\366\313\236\253\310\3\37\210\107\142\157\162\160\101\12\142\154\246\16\33\53\6\225\26"
  "\135\227\1\163\34\120\266\215\50\127\30\12\41\66\154\303\375\34\140\270\303\102\200\236\20\174\3\44\100\61\100\223"
  "\353\345\342\275\10\234\227\346\201\371\124\51\115\257\273\226\5\311\121\163\301\210\63\5\75\10\145\260\210\63\236\143"
  "\312\373\306\234\273\26\25\347\170\215\244\36\133\0\351\150\234\13\20\134\234\134\74\351\114\1\240\62\371\14\233\107"
  "\165\335\227\125\277\333\56\347\253\307\247\121\77\342\145\5\137\42\334\124\334\46\157\234\114\160\357\344\126\13\157\64"
  "\170\141\15\43\364\362\277\353\174\61\273\167\146\2\0\343\141\117\344\360\367\232\154\145\130\253\264\251\240\310\4\35"
  "\247\174\367\264\337\56\72\334\372\120\353\314\161\136\341\27\4\104\315\5\70\275\24\36\247\106\251\230\14\261\131\371"
  "\26\365\200\216\270\245\345\344\16\362\65\173\273\244\165\366\35\47\130\105\17\166\1\346\11\162\210\160\20\270\115\270"
  "\331\32\205\77\354\345\100\204\333\334\363\371\170\323\211\132\217\53\103\222\227\53\235\74\354\264\257\241\225\115\360\351"
  "\31\267\357\150\166\351\71\123\147\267\207\254\354\64\330\176\262\211\357\364\256\275\23\14\316\55\260\221\77\266\375\342"
  "\136\176\147\356\345\141\157\57\365\273\370\6\135\203\203\311\132\173\347\215\74\324\162\364\365\254\361\314\317\43\167\320"
  "\343\362\115\200\15\0\277\10\147\335\160\70\204\223\100\357\152\126\73\124\240\371\320\255\24\152\41\127\5\324\330\323"
  "\112\220\250\330\344\130\302\165\307\367\204\116\25\247\106\370\212\303\160\22\210\304\136\220\2\304\103\13\57\65\174\211"
  "\361\60\304\305\244\315\277\116\47\260\1\65\56\0\2\214\202\143\61\142\76\73\155\254\72\61\77\347\26\142\102\65"
  "\60\76\173\127\260\71\273\260\66\310\276\350\120\150\175\155\20\11\36\242\1\102\157\352\103\255\20\377\224\210\52\310"
  "\346\102\104\322\167\66\240\0\361\232\302\331\252\340\136\255\267\145\341\212\237\70\345\60\26\205\65\234\11\210\3\232"
  "\0\56\233\376\333\154\101\330\313\374\244\256\116\113\120\375\216\312\175\240\274\217\377\373\177";

/* lib/time.wo (DEFLATEd, org. size 46152) */
static unsigned char file_l_42[4434] =
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

/* lib/stat.wo (DEFLATEd, org. size 9195) */
static unsigned char file_l_43[1077] =
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

/* lib/unistd.wo (DEFLATEd, org. size 26203) */
static unsigned char file_l_44[1954] =
  "\314\127\345\166\344\40\24\376\275\366\16\234\234\272\273\367\111\126\162\230\160\231\162\226\100\26\156\152\117\277\51\314\320"
  "\310\264\120\367\62\227\317\260\310\122\251\131\55\201\314\325\112\130\144\77\276\22\262\44\312\112\33\44\131\141\60\43"
  "\231\255\346\62\262\64\226\172\104\45\231\153\152\47\115\205\54\225\65\22\261\273\263\274\274\74\203\123\102\251\315\115"
  "\103\363\15\117\233\264\267\2\43\352\342\14\272\350\53\152\105\156\25\255\354\205\306\274\62\160\51\340\152\273\21\340"
  "\54\67\100\131\43\302\153\125\220\271\331\300\223\11\214\54\125\324\320\322\31\244\265\15\330\132\206\41\247\106\272\62"
  "\2\41\236\311\343\336\53\124\41\265\115\11\345\160\257\141\310\50\122\173\243\212\270\147\200\276\206\55\147\26\51\346"
  "\143\300\270\161\13\374\332\323\315\205\4\247\155\241\371\25\267\51\123\77\340\314\14\165\260\367\354\120\125\342\141\251"
  "\322\117\113\10\365\52\263\126\45\35\235\0\374\210\210\26\340\157\74\240\203\245\305\170\171\244\304\123\366\52\47\254"
  "\242\170\341\66\152\352\51\33\20\236\176\301\173\215\310\122\250\350\262\5\140\74\312\173\106\67\100\131\152\374\0\376"
  "\14\261\113\175\11\71\23\6\12\324\346\46\55\176\217\364\266\21\355\115\231\70\261\1\373\61\163\131\253\73\157\167"
  "\214\242\141\173\370\267\13\146\164\221\303\265\300\130\242\0\14\226\103\27\152\55\270\107\302\74\367\315\234\123\41\203"
  "\262\257\235\164\72\333\152\217\50\133\64\102\215\63\327\220\240\202\244\257\237\370\152\352\144\200\61\112\147\341\177\170"
  "\164\165\237\117\334\337\31\217\257\274\120\50\33\26\27\212\345\6\344\335\22\205\34\256\363\244\335\365\224\125\142\24"
  "\351\364\61\376\204\331\243\71\122\150\145\221\120\51\306\352\174\233\144\122\214\66\255\51\66\75\144\243\370\265\265\225"
  "\71\252\263\17\124\177\303\207\153\37\370\356\223\107\221\151\200\71\316\72\201\346\106\65\357\26\324\350\6\141\30\224"
  "\114\144\244\56\46\63\345\312\241\60\252\346\272\5\336\57\210\355\176\141\147\256\55\355\27\141\143\14\30\336\41\174"
  "\207\203\157\130\100\347\22\1\67\202\33\176\356\266\17\356\53\266\36\165\170\266\317\213\271\43\200\33\322\240\46\266"
  "\133\65\307\167\163\332\262\106\155\200\150\316\55\340\371\326\144\105\367\6\34\76\34\200\3\371\2\145\154\350\274\63"
  "\164\166\213\367\164\157\226\22\147\73\5\164\64\43\163\203\227\261\167\272\301\350\300\230\276\366\301\376\376\356\176\113"
  "\137\261\76\142\353\276\240\140\322\346\275\330\317\224\76\154\355\46\370\67\325\356\34\221\7\346\2\244\205\227\144\0"
  "\305\372\173\267\163\245\352\353\254\367\27\312\35\233\310\366\67\200\265\121\301\360\361\245\226\232\262\351\256\72\172\160"
  "\127\375\157\347\12\233\32\104\142\350\77\162\264\105\16\177\115\247\127\240\166\244\320\1\72\75\375\365\7\246\263\133"
  "\366\65\263\106\242\253\165\147\374\102\212\20\222\227\367\222\5\220\234\364\12\225\321\144\140\270\354\175\63\222\131\44"
  "\263\237\115\146\264\30\24\331\154\352\103\144\263\303\264\63\73\314\153\315\254\211\374\246\345\210\337\315\160\111\246\310"
  "\160\35\145\31\71\205\342\305\125\132\232\334\25\377\365\3\142\126\303\157\253\316\132\253\336\154\225\356\77\57\262\360"
  "\200\216\144\316\247\131\237\343\17\267\325\261\312\343\235\112\202\6\153\364\30\3\360\223\124\6\167\43\370\143\234\300"
  "\327\64\231\360\372\42\71\103\54\263\277\237\253\327\32\50\206\62\142\260\201\105\140\171\63\6\214\105\171\146\256\377"
  "\237\373\131\107\4\1\226\341\353\217\312\266\63\205\320\366\157\24\356\50\334\121\270\243\160\337\316\164\26\225\73\115"
  "\242\164\263\0\213\322\135\215\267\356\255\162\277\157\242\160\163\242\154\155\247\347\242\336\270\153\210\151\62\103\250\303"
  "\257\20\112\145\30\151\306\117\331\24\67\320\334\107\6\365\0\24\37\53\215\371\274\35\46\107\256\10\264\320\206\204"
  "\251\117\6\110\366\116\77\221\151\223\101\331\267\303\346\372\262\225\67\46\37\51\124\105\275\355\237\275\235\172\367\106"
  "\273\0\41\300\265\320\1\241\334\272\67\176\317\340\115\257\67\245\150\242\353\241\102\26\75\56\367\343\252\132\53\142"
  "\340\236\7\265\364\14\247\101\354\270\311\243\365\3\63\44\233\65\137\212\206\63\33\62\212\151\331\165\353\276\177\265"
  "\171\241\355\117\44\46\160\307\260\134\4\352\30\150\67\321\344\1\340\153\77\7\254\126\4\232\373\357\350\204\263\325"
  "\321\325\277\7\140\155\274\70\117\33\361\200\135\204\137\177\63\156\326\132\246\363\146\255\272\160\2\202\302\364\370\24"
  "\40\65\20\62\325\256\243\373\267\275\230\100\306\55\227\44\166\365\306\313\22\165\161\232\322\102\123\345\14\237\17\207"
  "\343\313\142\330\145\325\61\330\361\37\7\351\127\167\61\0\367\310\333\346\340\36\246\55\312\73\170\126\16\362\230\54"
  "\55\275\134\177\350\20\313\23\56\235\256\343\155\352\44\275\6\163\147\273\24\146\376\172\256\30\304\171\2\15\307\151"
  "\127\107\140\331\1\20\376\152\240\235\254\313\333\266\71\1\211\16\110\102\235\304\64\142\212\222\257\351\42\321\37\14"
  "\253\277\327\137\157\66\105\67\150\262\55\75\143\362\365\372\343\163\233\123\313\172\337\344\205\143\352\313\152\275\365\326"
  "\155\325\64\57\307\303\152\334\267\73\357\154\33\262\306\321\376\375\256\246\361\41\124\63\360\364\35\313\7\24\115\100"
  "\107\346\347\25\65\355\243\344\41\100\245\56\40\177\205\274\257\0\21\200\154\134\242\321\327\243\171\366\247\62\141\215"
  "\304\273\316\223\331\342\36\220\171\353\344\332\20\322\174\345\30\322\374\206\345\125\252\53\361\65\12\156\34\244\211\302"
  "\215\203\230\66\101\333\154\153\167\24\207\217\145\74\121\10\36\235\214\334\62\32\274\314\22\327\270\20\115\66\17\51"
  "\67\332\54\261\337\350\212\252\330\364\366\204\115\313\204\104\122\15\13\375\330\120\141\110\374\204\303\11\146\63\273\70"
  "\212\243\30\347\143\135\260\42\25\0\375\134\31\150\317\135\324\355\331\366\217\266\235\336\317\323\353\361\175\135\276\153"
  "\313\374\57\334\353\101\225\107\335\306\327\236\134\252\247\170\311\307\14\272\175\22\134\132\311\175\77\327\341\43\21\2"
  "\215\246\140\302\0\243\135\27\364\72\341\345\140\104\26\371\134\344\37\202\160\324\341\133\161\165\15\53\163\245\226\225"
  "\176\220\275\353\352\326\26\305\300\216\44\241\274\205\127\112\31\107\331\112\10\337\263\205\133\363\247\260\271\205\303\112"
  "\112\324\217\250\37\12\372\301\50\5\20\272\266\120\270\62\161\135\44\232\52\67\100\266\46\224\212\272\70\225\71\230"
  "\160\77\315\165\65\214\236\150\101\204\134\321\36\41\340\264\156\131\132\103\323\372\35\346\226\120\145\253\71\46\151\327"
  "\355\202\65\35\112\64\132\51\327\214\175\276\242\215\340\4\322\370\313\132\65\125\52\324\51\276\114\375\372\105\267\237"
  "\120\302\306\377\276\151\141\203\302\120\320\66\33\113\104\3\17\104\54\41\371\223\337\102\67\51\271\2\67\41\136\112"
  "\123\24\271\42\73\126\42\121\124\122\72\375\305\216\20\55\11\163\123\372\165\357\312\373\331\204\12\337\157\173\344\11"
  "\170\76\256\176\1\336\220\215\202\346\104\214\135\40\101\0\14\343\37\157\237\251\77\347\350\105\11\102\220\1\304\30"
  "\200\1\240\342\40\205\110\215\263\24\322\204\302\214\105\321\103\172\324\345\135\372\64\223\113\274\144\223\57\300\341\113"
  "\206\144\30\237\113\21\360\360\215\227\215\326\272\32\276\211\7\66\12\375\234\233\334\4\5\224\221\170\203\133\124\313"
  "\372\365\12\325\12\42\46\257\315\50\151\121\317\346\353\226\200\224\50\242\50\105\352\105\263\37\56\151\242\160\144\20"
  "\310\233\170\200\10\177\307\47\266\376\56\4\34\0\160\214\31\271\60\162\241\176\17\117\20\324\47\267\322\174\337\336"
  "\240\333\230\220\340\24\350\14\270\210\375\342\176\170\6\12\361\321\71\201\27\105\70\102\54\35\310\4\204\313\55\103"
  "\145\161\13\130\241\357\153\33\254\214\233\320\73\215\317\324\36\73\6\11\364\43\242\201\377\232\367\25\257\206\277\377"
  "\1";

/* lib/assert.wo (DEFLATEd, org. size 3707) */
static unsigned char file_l_45[729] =
  "\334\127\301\156\333\60\14\275\17\330\77\10\206\17\316\45\260\323\264\310\2\364\117\12\30\162\114\245\302\144\331\223\244"
  "\266\333\327\317\215\354\260\62\135\307\50\260\313\56\201\114\221\174\44\105\76\51\131\323\326\136\1\113\271\265\140\334"
  "\367\157\214\145\262\351\132\343\130\162\62\56\141\211\355\322\204\145\147\325\126\134\261\264\227\35\173\11\313\32\357\230"
  "\274\333\155\66\233\31\233\6\232\326\374\356\315\302\42\230\15\353\34\55\156\241\4\200\110\373\225\133\131\132\315\73"
  "\373\334\272\262\63\360\42\341\265\350\35\210\272\174\65\322\101\357\105\170\175\142\351\274\346\161\324\143\131\307\15\157"
  "\56\20\353\326\6\254\127\121\322\41\334\261\166\307\262\14\213\122\50\157\237\131\6\157\41\346\130\236\154\260\170\54"
  "\353\177\267\247\126\133\167\255\113\315\35\277\272\254\355\56\145\141\237\53\171\326\217\5\113\216\54\154\312\126\63\301"
  "\245\202\372\111\77\345\171\22\314\103\362\203\371\45\123\256\324\373\16\33\263\111\155\224\134\252\100\7\301\240\245\332"
  "\323\173\122\325\120\377\253\100\114\5\262\230\12\166\351\107\117\241\74\333\63\270\353\211\206\215\213\372\326\202\273\240"
  "\334\120\306\12\25\17\50\261\276\212\354\54\261\133\203\56\106\131\325\13\177\262\264\270\252\264\35\113\167\150\60\72"
  "\122\240\247\121\345\50\70\273\322\17\137\202\230\212\30\337\1\134\52\110\364\54\372\263\256\65\300\132\41\54\270\307"
  "\174\350\200\75\365\114\113\265\107\1\257\153\212\273\234\332\62\62\242\354\326\204\122\254\121\72\314\304\333\353\253\233"
  "\143\274\160\32\172\334\24\321\354\176\22\37\50\13\313\221\252\226\327\143\111\16\264\44\4\37\176\15\236\365\220\120"
  "\145\112\51\130\72\177\346\137\106\46\47\154\321\43\75\337\57\243\340\304\41\12\372\254\14\216\113\310\67\54\42\51"
  "\115\35\11\140\141\220\15\70\157\56\70\310\160\63\234\313\245\102\312\215\304\311\46\346\77\41\25\114\50\120\152\230"
  "\343\300\116\307\4\247\140\42\360\370\371\177\163\46\162\207\130\6\231\153\306\116\123\367\263\167\346\252\151\136\351\13"
  "\111\244\224\272\226\6\116\156\70\357\353\135\276\301\206\305\133\0\133\66\216\203\166\76\136\25\264\261\103\227\55\220"
  "\54\226\12\247\357\120\332\51\347\26\37\371\344\117\304\43\273\370\356\272\303\101\303\217\345\253\253\210\142\131\236\356"
  "\202\314\61\116\370\352\334\361\114\350\23\205\250\113\275\304\356\312\225\166\25\277\347\13\374\216\30\240\151\376\76\252"
  "\357\175\364\66\330\307\305\177\240\65\350\150\271\351\50\342\140\241\101\374\76\40\67\373\201\40\171\2\224\243\304\100"
  "\123\372\117\172\16\336\34\350\276\353\246\117\216\303\174\27\256\306\254\345\113\351\151\71\261\147\346\352\345\157\77\254"
  "\260\367\367\330\200\223\217\373\177\164\22\224\24\356\17\153\313\106\103\101\321\172\76\245\136\60\376\345\251\62\40\266"
  "\344\157\5\1\375\161\323\217\327\6\370\351\231\127\63\323\375\167\44\326\343\100\4\0";

/* lib/crt.args.wo (DEFLATEd, org. size 4765) */
static unsigned char file_l_46[881] =
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

/* l directory (sorted by path) */
struct memdir directory_l[47] = {
  { "lib/assert.wo", 729, 1, 3707, &file_l_45[0] },
  { "lib/crt.args.wo", 881, 1, 4765, &file_l_46[0] },
  { "lib/crt.argv.wo", 876, 1, 4717, &file_l_34[0] },
  { "lib/crt.void.wo", 450, 1, 1416, &file_l_39[0] },
  { "lib/crt.wo", 121, 1, 264, &file_l_40[0] },
  { "lib/ctype.wo", 489, 1, 4203, &file_l_38[0] },
  { "lib/dirent.wo", 2161, 1, 21870, &file_l_33[0] },
  { "lib/errno.wo", 1474, 1, 9609, &file_l_31[0] },
  { "lib/fcntl.wo", 2337, 1, 20902, &file_l_32[0] },
  { "lib/fenv.wo", 260, 1, 1286, &file_l_35[0] },
  { "lib/include/NDEBUG/assert.h", 116, 1, 150, &file_l_9[0] },
  { "lib/include/assert.h", 292, 1, 507, &file_l_13[0] },
  { "lib/include/ctype.h", 154, 1, 495, &file_l_18[0] },
  { "lib/include/dirent.h", 504, 1, 1506, &file_l_7[0] },
  { "lib/include/errno.h", 599, 1, 1694, &file_l_24[0] },
  { "lib/include/fcntl.h", 715, 1, 2242, &file_l_23[0] },
  { "lib/include/fenv.h", 317, 1, 780, &file_l_19[0] },
  { "lib/include/float.h", 278, 1, 665, &file_l_17[0] },
  { "lib/include/inttypes.h", 557, 1, 3738, &file_l_22[0] },
  { "lib/include/limits.h", 230, 1, 635, &file_l_28[0] },
  { "lib/include/locale.h", 154, 1, 215, &file_l_1[0] },
  { "lib/include/math.h", 417, 1, 1519, &file_l_16[0] },
  { "lib/include/stdarg.h", 267, 1, 583, &file_l_8[0] },
  { "lib/include/stdbool.h", 81, 1, 122, &file_l_0[0] },
  { "lib/include/stddef.h", 100, 1, 178, &file_l_4[0] },
  { "lib/include/stdint.h", 490, 1, 2186, &file_l_11[0] },
  { "lib/include/stdio.h", 1054, 1, 3869, &file_l_14[0] },
  { "lib/include/stdlib.h", 664, 1, 2146, &file_l_2[0] },
  { "lib/include/string.h", 414, 1, 1854, &file_l_10[0] },
  { "lib/include/sys.cdefs.h", 113, 1, 191, &file_l_6[0] },
  { "lib/include/sys.crt.h", 154, 1, 243, &file_l_25[0] },
  { "lib/include/sys.intrs.h", 612, 1, 3110, &file_l_27[0] },
  { "lib/include/sys.stat.h", 634, 1, 2073, &file_l_15[0] },
  { "lib/include/sys.types.h", 150, 1, 414, &file_l_26[0] },
  { "lib/include/time.h", 457, 1, 1301, &file_l_5[0] },
  { "lib/include/unistd.h", 453, 1, 1529, &file_l_12[0] },
  { "lib/include/wasi.api.h", 10409, 1, 48950, &file_l_21[0] },
  { "lib/include/wasm.h", 2302, 1, 22452, &file_l_20[0] },
  { "lib/include/wasm_simd128.h", 4173, 1, 44179, &file_l_29[0] },
  { "lib/include/wchar.h", 159, 1, 292, &file_l_3[0] },
  { "lib/math.wo", 14254, 1, 168474, &file_l_37[0] },
  { "lib/stat.wo", 1077, 1, 9195, &file_l_43[0] },
  { "lib/stdio.wo", 13041, 1, 191886, &file_l_36[0] },
  { "lib/stdlib.wo", 9210, 1, 100557, &file_l_41[0] },
  { "lib/string.wo", 2033, 1, 23745, &file_l_30[0] },
  { "lib/time.wo", 4434, 1, 46152, &file_l_42[0] },
  { "lib/unistd.wo", 1954, 1, 26203, &file_l_44[0] },
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
