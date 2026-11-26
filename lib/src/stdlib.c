#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
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

/* Decimal <-> binary conversion core, shared by strtod and printf's
 * %e/%f/%g.  Values are carried as double-doubles (hi + lo, ~106 bits),
 * so scaling by a power of ten is accurate to ~2^-100 relative: results
 * are correctly rounded except within ~1e-30 (relative) of a halfway
 * point, where the last digit/bit may be off by one. */

typedef struct dd { double hi, lo; } dd_t;

/* 10^(2^i) and 10^-(2^i), i=0..8, as double-double (hi, lo) pairs */
static const double p10dd[18] = {
  0x1.4000000000000p+3, 0x0.0p+0,
  0x1.9000000000000p+6, 0x0.0p+0,
  0x1.3880000000000p+13, 0x0.0p+0,
  0x1.7d78400000000p+26, 0x0.0p+0,
  0x1.1c37937e08000p+53, 0x0.0p+0,
  0x1.3b8b5b5056e17p+106, -0x1.3107f00000000p+52,
  0x1.84f03e93ff9f5p+212, -0x1.2ac340948e389p+157,
  0x1.27748f9301d32p+425, -0x1.901cc86649e4ap+371,
  0x1.54fdd7f73bf3cp+850, -0x1.7222446fe4670p+795 };
static const double n10dd[18] = {
  0x1.999999999999ap-4, -0x1.999999999999ap-58,
  0x1.47ae147ae147bp-7, -0x1.eb851eb851eb8p-63,
  0x1.a36e2eb1c432dp-14, -0x1.6a161e4f765fep-68,
  0x1.5798ee2308c3ap-27, -0x1.03023df2d4c94p-82,
  0x1.cd2b297d889bcp-54, 0x1.5b4c2ebe68799p-109,
  0x1.9f623d5a8a733p-107, -0x1.a2cc10f3892d4p-161,
  0x1.50ffd44f4a73dp-213, 0x1.a53f2398d747bp-268,
  0x1.bba08cf8c979dp-426, -0x1.afa9c1a60497dp-480,
  0x1.8062864ac6f43p-851, 0x1.39fa911155ff0p-906 };

static const uint64_t ip10[18] = { 
  1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL, 
  10000000ULL, 100000000ULL, 1000000000ULL, 10000000000ULL, 
  100000000000ULL, 1000000000000ULL, 10000000000000ULL, 
  100000000000000ULL, 1000000000000000ULL, 10000000000000000ULL, 
  100000000000000000ULL };

/* exact product a*b = p->hi + p->lo (Dekker, no FMA needed) */
static void two_prod(double a, double b, dd_t *p)
{
  double c, ah, al, bh, bl, x = a * b;
  c = 134217729.0 * a; ah = c - (c - a); al = a - ah;
  c = 134217729.0 * b; bh = c - (c - b); bl = b - bh;
  p->hi = x; p->lo = ((ah * bh - x) + ah * bl + al * bh) + al * bl;
}

/* *pa *= (bhi + blo), renormalized */
static void dd_mul(dd_t *pa, double bhi, double blo)
{
  dd_t p; double s;
  two_prod(pa->hi, bhi, &p);
  p.lo += pa->hi * blo + pa->lo * bhi;
  s = p.hi + p.lo; pa->lo = p.lo - (s - p.hi); pa->hi = s;
}

/* *pa *= 10^n, |n| < 512 */
static void dd_mul_p10(dd_t *pa, int n)
{
  const double *t = n < 0 ? &n10dd[0] : &p10dd[0]; int i;
  if (n < 0) n = -n;
  for (i = 8; i >= 0; --i) if ((n >> i) & 1) dd_mul(pa, t[2*i], t[2*i+1]);
}

/* nearest integer to dd value 0 <= v < 2^63, ties to even; v within 
 * v*rtol of a tie counts as a tie, to absorb dd rounding errors */
static uint64_t dd_round(dd_t *pv, double rtol)
{
  uint64_t i = (uint64_t)pv->hi; double f = pv->hi - (double)i, t, tol; /* exact */
  while (f + pv->lo < 0.0) { --i; f += 1.0; }
  while (f + pv->lo >= 1.0) { ++i; f -= 1.0; }
  /* compare f + lo with 1/2 without rounding lo away: f - 0.5 is exact 
   * near the tie, so t is accurate where it matters */
  t = (f - 0.5) + pv->lo; tol = pv->hi * rtol;
  if (t > tol || (t >= -tol && (i & 1) != 0)) ++i;
  return i;
}

/* *ps = v * 10^n as dd; v > 0 finite, |n| < 512, result in normal range */
static void dd_scale(double v, int n, dd_t *ps)
{
  int k = 0;
  ps->hi = v; ps->lo = 0.0;
  if (v < 0x1p-900) { ps->hi = v * 0x1p+128; k = 1; }
  else if (v > 0x1p+900) { ps->hi = v * 0x1p-128; k = 2; }
  dd_mul_p10(ps, n);
  if (k == 1) { ps->hi *= 0x1p-128; ps->lo *= 0x1p-128; }
  else if (k == 2) { ps->hi *= 0x1p+128; ps->lo *= 0x1p+128; }
}

/* v > 0 finite: nd (1..17) significant digits -> *pd, 
 * decimal exponent of the first digit -> *pe */
static void dd_digits(double v, int nd, uint64_t *pd, int *pe)
{
  union { uint64_t u; double d; } c; int e2, e, it, dir; dd_t s; uint64_t d;
  /* e: estimate of floor(log10(v)) from the binary exponent */
  if (v < 0x1p-1022) { c.d = v * 0x1p+64; e2 = (int)((c.u >> 52) & 0x7FF) - 1023 - 64; } 
  else { c.d = v; e2 = (int)((c.u >> 52) & 0x7FF) - 1023; }
  e = (e2 * 78913) >> 18; /* 78913/2^18 ~ log10(2) */
  /* adjust e until 10^(nd-1) <= s < 10^nd; move in one direction only: 
   * a reversal means v is a power of 10 that scaled to a hair under the 
   * bound, which the rounding and carry below handle */
  for (it = 0, dir = 0; it < 4; ++it) {
    double P = (double)ip10[nd], Q = (double)ip10[nd-1];
    dd_scale(v, nd - 1 - e, &s);
    if (s.hi > P || (s.hi == P && s.lo >= 0.0)) { if (dir < 0) break; dir = 1; ++e; continue; }
    if (s.hi < Q || (s.hi == Q && s.lo < 0.0)) { if (dir > 0) break; dir = -1; --e; continue; }
    break;
  }
  /* exact decimal ties (2.5 to 1 digit, 0.125 to 2, ...) come out of the
   * dd arithmetic with ~2^-100 relative error; genuine near-ties that 
   * close are far beyond what 17 digits can show */
  d = dd_round(&s, 0x1p-90);
  if (d >= ip10[nd]) { d = ip10[nd-1]; ++e; }
  *pd = d; *pe = e;
}

/* v > 0 finite.  If fixed is 0, round v to n (1..17) significant digits;
 * otherwise round it to n (>= 0) digits after the decimal point, but to 17
 * significant digits at most.  The digits go to *pd, the decimal exponent
 * of the first digit to *pe; returns the number of digits in *pd (0 if v
 * rounds to 0 in fixed mode). */
int _fptodigits(double v, int fixed, int n, unsigned long long *pd, int *pe)
{
  int nd = n, e;
  if (fixed) {
    dd_digits(v, 17, pd, &e); /* to get the exact decimal exponent */
    nd = e + 1 + n;
    if (nd >= 17) { *pe = e; return 17; }
    if (nd <= 0) { /* v < 10^-n: result is 0 or 10^-n */
      if (nd == 0) {
        dd_t s; dd_scale(v, n, &s); /* in [0.1, 1) */
        if (s.hi > 0.5 || (s.hi == 0.5 && s.lo > 0.0)) { *pd = 1; *pe = -n; return 1; }
      }
      *pd = 0; *pe = -n; return 0;
    }
  }
  dd_digits(v, nd, pd, pe);
  return nd;
}

/* decimal strtod: up to 36 significant digits are kept (as a dd), the
 * rest only matters as a 'sticky' nonzero indicator */
static double strtod_dec(const char *s, const char *pc, bool neg, char **endp)
{
  uint64_t d1 = 0, d2 = 0; int n1 = 0, n2 = 0, dexp = 0, ex = 0, k = 0, m;
  bool seendig = false, seendot = false, eneg = false, sticky = false;
  /* r starts at zero because the early returns below -- an all-zero
   * mantissa, and an exponent too small for any subnormal -- answer it
   * without computing anything.  A wasm local would read as zero anyway,
   * but only because the spec initializes locals, and that is not a
   * promise C makes. */
  dd_t x; double r = 0.0;
  for (;; ++pc) {
    int c = *pc;
    if (c == '.' && !seendot) { seendot = true; continue; }
    if (c < '0' || c > '9') break;
    seendig = true;
    if (c == '0' && n1 == 0) { if (seendot) --dexp; } /* leading zero */
    else if (n1 < 19) { d1 = d1 * 10 + (uint64_t)(c - '0'); ++n1; if (seendot) --dexp; }
    else if (n2 < 17) { d2 = d2 * 10 + (uint64_t)(c - '0'); ++n2; if (seendot) --dexp; }
    else { if (c != '0') sticky = true; if (!seendot) ++dexp; }
  }
  if (!seendig) { /* no conversion */
    if (endp != NULL) *endp = (char *)s;
    return 0.0;
  }
  if (*pc == 'e' || *pc == 'E') { /* exponent is used only if it has digits */
    const char *pe = pc + 1;
    if (*pe == '-') { eneg = true; ++pe; } else if (*pe == '+') ++pe;
    if (isdigit(*pe)) {
      for (; isdigit(*pe); ++pe) if (ex < 100000) ex = ex * 10 + (*pe - '0');
      pc = pe;
    }
  }
  if (endp != NULL) *endp = (char *)pc;
  r = 0.0;
  if (n1 == 0) return neg ? -r : r; /* all zeros */
  dexp += eneg ? -ex : ex; m = dexp + n1 + n2; /* v ~ 0.ddd * 10^m */
  if (m > 310) { errno = ERANGE; r = HUGE_VAL; return neg ? -r : r; }
  if (m < -326) { errno = ERANGE; return neg ? -r : r; }
  x.hi = (double)d1; x.lo = (double)(int64_t)(d1 - (uint64_t)x.hi);
  if (n2 > 0) { /* x = x * 10^n2 + d2 (+ sticky) */
    double h, l, s1, bb, e1;
    dd_mul(&x, (double)ip10[n2], 0.0);
    h = (double)d2; l = (double)(int64_t)(d2 - (uint64_t)h);
    if (sticky) l += 0.5;
    s1 = x.hi + h; bb = s1 - x.hi; e1 = (x.hi - (s1 - bb)) + (h - bb); e1 += x.lo + l;
    x.hi = s1 + e1; x.lo = e1 - (x.hi - s1); 
  }
  if (m < -290) { x.hi *= 0x1p+200; x.lo *= 0x1p+200; k = 200; }
  else if (m > 290) { x.hi *= 0x1p-128; x.lo *= 0x1p-128; k = -128; }
  dd_mul_p10(&x, dexp);
  if (!k) r = x.hi;
  else if (k < 0) r = x.hi * 0x1p+128; /* may overflow to inf */
  else if (x.hi * 0x1p-200 >= 0x1p-1022) r = x.hi * 0x1p-200; /* normal: hi already rounded */
  else { /* subnormal: round to the 2^-1074 grid once */
    dd_t q; q.hi = x.hi * 0x1p+874; q.lo = x.lo * 0x1p+874;
    /* a tie window of 2^-103 is the measured best compromise between exact
     * midpoints (dd error ~2^-100) and near-midpoint inputs of 30+ digits */
    r = (double)dd_round(&q, 0x1p-103) * 0x1p-1074;
  }
  if (r == HUGE_VAL || r < 0x1p-1022) errno = ERANGE; /* overflow, underflow */
  return neg ? -r : r;
}

/* hexadecimal strtod: exact, rounds to nearest even once */
static double strtod_hex(const char *s, const char *pc, bool neg, char **endp)
{
  uint64_t m = 0, kept, rem, half, bits; int nd = 0, bexp = 0, ex = 0, drop, e;
  bool seendig = false, seendot = false, eneg = false, sticky = false;
  /* r starts at zero, as in strtod_dec above: a zero mantissa and an
   * exponent below the subnormal range return it uncomputed */
  union { uint64_t u; double d; } c; double r = 0.0;
  for (;; ++pc) {
    int ch = *pc, v;
    if (ch == '.' && !seendot) { seendot = true; continue; }
    if ('0' <= ch && ch <= '9') v = ch - '0';
    else if ('a' <= ch && ch <= 'f') v = ch - 'a' + 10;
    else if ('A' <= ch && ch <= 'F') v = ch - 'A' + 10;
    else break;
    seendig = true;
    if (v == 0 && nd == 0) { if (seendot) bexp -= 4; } /* leading zero */
    else if (nd < 16) { m = (m << 4) | (uint64_t)v; ++nd; if (seendot) bexp -= 4; }
    else { if (v != 0) sticky = true; if (!seendot) bexp += 4; }
  }
  if (!seendig) { /* "0x" without hex digits: the subject is "0" */
    if (endp != NULL) *endp = (char *)pc - (seendot ? 2 : 1); 
    r = 0.0; return neg ? -r : r;
  }
  if (*pc == 'p' || *pc == 'P') { /* exponent is used only if it has digits */
    const char *pe = pc + 1;
    if (*pe == '-') { eneg = true; ++pe; } else if (*pe == '+') ++pe;
    if (isdigit(*pe)) {
      for (; isdigit(*pe); ++pe) if (ex < 100000) ex = ex * 10 + (*pe - '0');
      pc = pe;
    }
  }
  if (endp != NULL) *endp = (char *)pc;
  r = 0.0;
  if (m == 0) return neg ? -r : r;
  bexp += eneg ? -ex : ex;
  while (!(m >> 63)) { m <<= 1; --bexp; } /* v = m * 2^bexp, m in [2^63, 2^64) */
  e = bexp + 63; /* v in [2^e, 2^(e+1)) */
  if (e > 1023) { errno = ERANGE; r = HUGE_VAL; return neg ? -r : r; }
  if (e < -1075) { errno = ERANGE; return neg ? -r : r; }
  drop = e >= -1022 ? 11 : 11 + (-1022 - e); /* 11..64 low bits to drop */
  kept = drop == 64 ? 0 : m >> drop;
  rem = drop == 64 ? m : m & ((1ULL << drop) - 1);
  half = 1ULL << (drop - 1);
  if (rem > half || (rem == half && (sticky || (kept & 1)))) ++kept;
  if (rem != 0) sticky = true; /* now: result is inexact */
  /* kept includes the implicit bit, so adding it to the exponent field 
   * one below the real one handles carries into the exponent and inf */
  bits = (e >= -1022 ? (uint64_t)(e + 1022) << 52 : 0) + kept;
  c.u = bits; r = c.d;
  if (r == HUGE_VAL || (r < 0x1p-1022 && sticky)) errno = ERANGE; /* overflow, underflow */
  return neg ? -r : r;
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
      union { uint64_t u; double d; } c; /* quiet NaN with the sign given */
      c.u = neg ? 0xFFF8000000000000ULL : 0x7FF8000000000000ULL;
      return c.d;
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

/* common integer scanner: magnitude is returned, sign in *pneg; if the
 * magnitude does not fit in uintmax_t, *povf is set and UINTMAX_MAX is
 * returned; if no digits are found, *endptr is nptr and 0 is returned */
static uintmax_t strntou(const char *nptr, char **endptr, int base, size_t n, bool *pneg, bool *povf)
{
  const char *s = nptr; uintmax_t v = 0, lim; int d, dlim; bool any = false;
  *pneg = *povf = false;
  if (base < 0 || base == 1 || base > 36) {
    errno = EINVAL;
    if (endptr) *endptr = (char *)nptr;
    return 0;
  }
  while (n && isspace((unsigned char)*s)) { ++s; --n; }
  if (n && (*s == '-' || *s == '+')) { *pneg = (*s == '-'); ++s; --n; }
  /* "0x" prefix is skipped only if a hex digit follows it */
  if ((base == 0 || base == 16) && n >= 3 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')
      && (d = digitval(s[2])) >= 0 && d < 16) {
    s += 2; n -= 2; base = 16;
  } else if (base == 0) {
    base = (n && s[0] == '0') ? 8 : 10;
  }
  lim = UINTMAX_MAX / (uintmax_t)base; dlim = (int)(UINTMAX_MAX % (uintmax_t)base);
  while (n && (d = digitval(*s)) >= 0 && d < base) {
    if (v > lim || (v == lim && d > dlim)) *povf = true;
    else v = v * (uintmax_t)base + (uintmax_t)d;
    any = true; ++s; --n;
  }
  if (endptr) *endptr = (char *)(any ? s : nptr);
  if (!any) *pneg = false;
  return *povf ? UINTMAX_MAX : v;
}

/* signed conversion to [min, max], ERANGE and clamping outside of it */
static intmax_t strntos(const char *nptr, char **endptr, int base, size_t n, intmax_t min, intmax_t max)
{
  bool neg, ovf; uintmax_t v = strntou(nptr, endptr, base, n, &neg, &ovf);
  if (neg) {
    if (ovf || v > (uintmax_t)max + 1) { errno = ERANGE; return min; }
    return v == (uintmax_t)max + 1 ? min : -(intmax_t)v;
  }
  if (ovf || v > (uintmax_t)max) { errno = ERANGE; return max; }
  return (intmax_t)v;
}

/* unsigned conversion to [0, max]; a '-' negates the result in the
 * unsigned type, as required by C */
static uintmax_t strntous(const char *nptr, char **endptr, int base, size_t n, uintmax_t max)
{
  bool neg, ovf; uintmax_t v = strntou(nptr, endptr, base, n, &neg, &ovf);
  if (ovf || v > max) { errno = ERANGE; return max; }
  return neg ? (0 - v) & max : v;
}

intmax_t strntoimax(const char *nptr, char **endptr, int base, size_t n)
{
  return strntos(nptr, endptr, base, n, INTMAX_MIN, INTMAX_MAX);
}

static uintmax_t strntoumax(const char *nptr, char **endptr, int base, size_t n)
{
  return strntous(nptr, endptr, base, n, UINTMAX_MAX);
}

intmax_t strtoimax(const char *nptr, char **endptr, int base)
{
  return strntos(nptr, endptr, base, ~(size_t)0, INTMAX_MIN, INTMAX_MAX);
}

long strtol(const char *nptr, char **endptr, int base)
{
  return (long)strntos(nptr, endptr, base, ~(size_t)0, LONG_MIN, LONG_MAX);
}

long long strtoll(const char *nptr, char **endptr, int base)
{
  return (long long)strntos(nptr, endptr, base, ~(size_t)0, LLONG_MIN, LLONG_MAX);
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
  return (unsigned long)strntous(nptr, endptr, base, ~(size_t)0, ULONG_MAX);
}

unsigned long long strtoull(const char *nptr, char **endptr, int base)
{
  return (unsigned long long)strntous(nptr, endptr, base, ~(size_t)0, ULLONG_MAX);
}

uintmax_t strtoumax(const char *nptr, char **endptr, int base)
{
  return strntous(nptr, endptr, base, ~(size_t)0, UINTMAX_MAX);
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
