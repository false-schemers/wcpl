/* Regression tests for fixed WCPL compiler and library defects (most found
 * while porting a bignum program).  Each test set names the defect.
 * Defects that are compile-time errors (negative array size, overflowing
 * literals) cannot be checked by a running program and are not here. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#define REQUIRE(x) do { ++testno; if (!(x)) fail(testno, __LINE__); } while(0)

static int testno = 0;
static int failcnt = 0;
static void fail(int tno, int line)
{
  printf("  test %d (line %d) failed\n", tno, line);
  ++failcnt;
}

static void start(const char *test)
{
  printf("\n** test set %s **\n", test);
  testno = 0;
}

/* raw bits of a double */
static uint64_t dbits(double d)
{
  union { double d; uint64_t u; } c; c.d = d;
  return c.u;
}

/* sprintf one double, compare with expected text */
static char sbuf[400];
static bool fmt_is(const char *fmt, double v, const char *exp)
{
  snprintf(&sbuf[0], 400, fmt, v);
  if (strcmp(&sbuf[0], exp) == 0) return true;
  printf("  >>> %s: got [%s], expected [%s]\n", fmt, &sbuf[0], exp);
  return false;
}

/* strtod one string, compare bits and end offset */
static bool strtod_is(const char *s, uint64_t bits, int endoff)
{
  char *e; double d = strtod(s, &e);
  if (dbits(d) == bits && (int)(e - s) == endoff) return true;
  printf("  >>> strtod(\"%s\"): got %016llx end+%d, expected %016llx end+%d\n",
         s, (unsigned long long)dbits(d), (int)(e - s), (unsigned long long)bits, endoff);
  return false;
}


/* 1: long vs unsigned int: same rank, so both convert to unsigned long */

#define LIMB_MAX UINT32_MAX

void test_long_unsigned(void)
{
  long den = 2, m1 = -1; unsigned u = 4294967295u; long x = u;
  start("1: long vs unsigned comparisons");
  static_assert((-1L < 1u) == 0, "folded comparison");
  REQUIRE(!(den >= LIMB_MAX));
  REQUIRE(!(den >= u));
  REQUIRE(!(m1 < 1u));
  REQUIRE((m1 + 0u) > 5);
  REQUIRE(x == -1); /* unsigned -> long assignment is still allowed */
}


/* 2, 8: array dimensions that are constant expressions */

typedef uint32_t limb_t; typedef int32_t slimb_t;
#define INT64_LIMBS (1+(sizeof(int64_t)+sizeof(slimb_t)-1)/sizeof(limb_t))
typedef struct bignum_ll { 
  size_t size; int dupcount; int isneg; limb_t limb[INT64_LIMBS]; 
} bignumll_t;
static void mkll(bignumll_t *b, int64_t v) { b->size = 1; b->limb[0] = (limb_t)v; }
bignumll_t bnll(int64_t v);
bignumll_t bnll(int64_t v) { bignumll_t b; mkll(&b, v); return b; }

typedef struct s4 { int a[sizeof(int)]; } s4_t;
static s4_t mks4(void) { s4_t x; x.a[0] = 7; x.a[3] = 9; return x; }

typedef char ok1[(sizeof(int) == 4) ? 1 : -1];
typedef int arr3[3u];
typedef int arr3b[1+2];
enum { N4 = 4 };
struct sdims { int a[N4]; char b[sizeof(int64_t)]; };
static arr3 *pick(arr3b *p) { return p; } /* arr3 and arr3b are the same type */

void test_array_dims(void)
{
  bignumll_t b = bnll(5); s4_t y = mks4(); arr3 a3;
  start("2, 8: constant-expression array dimensions");
  REQUIRE(b.limb[0] == 5);
  REQUIRE(sizeof(bignumll_t) == 12 + 4 * INT64_LIMBS);
  REQUIRE(y.a[0] == 7 && y.a[3] == 9);
  REQUIRE(sizeof(s4_t) == 16);
  REQUIRE(sizeof(ok1) == 1);
  REQUIRE(sizeof(struct sdims) == 24);
  a3[2] = 5;
  REQUIRE((*pick(&a3))[2] == 5);
}


/* 12: casts of address constants in static initializers */

typedef struct bignum { int size; unsigned limb[2]; } bignum_t;
static const bignum_t bnzero = { 1, { 0, 0 } };
static const bignum_t bnone = { 1, { 1, 0 } };
static int sarr[4] = { 10, 20, 30, 40 };
bignum_t *bn0 = (bignum_t *)&bnzero;
static bignum_t *bn1 = (bignum_t *)(void *)&bnone;
void *vp = (void *)&sarr[0];
const char *smsg = (const char *)"hello";
int *np = (int *)0;
struct holder { bignum_t *p; void *q; } hld = { (bignum_t *)&bnone, (void *)&bnzero };

void test_static_casts(void)
{
  start("12: pointer casts in static initializers");
  REQUIRE(bn0 == &bnzero && bn0->size == 1);
  REQUIRE(bn1->limb[0] == 1);
  REQUIRE(((int *)vp)[2] == 30);
  REQUIRE(strcmp(smsg, "hello") == 0);
  REQUIRE(np == NULL);
  REQUIRE(hld.p->limb[0] == 1 && ((bignum_t *)hld.q)->limb[0] == 0);
}


/* 13: several declarators in one typedef */

typedef struct bn2 { int size; unsigned limb[3]; } bn2_t, bn2ll_t, *pbn2_t, bn2arr_t[2];
typedef int myint_t, *pint_t;
static bn2ll_t mkbn2(int v) { bn2_t b; b.size = v; b.limb[0] = 0; return b; }

void test_typedef_list(void)
{
  bn2arr_t a; pbn2_t p = &a[1]; myint_t i = 3; pint_t q = &i;
  start("13: typedef with several declarators");
  a[1] = mkbn2(7);
  REQUIRE(p->size == 7);
  REQUIRE(*q == 3);
  REQUIRE(sizeof(bn2arr_t) == 2 * sizeof(bn2_t));
  REQUIRE(sizeof(bn2ll_t) == 16);
}


/* 7: __LINE__ and __FILE__, also through macros */

#define HERE() (__LINE__)
#define NESTED() (HERE() + 0)

void test_line_file(void)
{
  int l0 = __LINE__;
  start("7: __LINE__ and __FILE__");
  REQUIRE(HERE() == l0 + 2);
  REQUIRE(NESTED() == l0 + 3);
  REQUIRE(HERE() 
          == l0 + 4); /* a macro call is at the line where it starts */
  REQUIRE(strstr(__FILE__, "regress-tests.c") != NULL);
}


/* 6: assert (a failing assert traps, so only passing ones are here) */

void test_assert(void)
{
  int x = 2;
  start("6: assert");
  REQUIRE(NDEBUG == 0); /* -DNDEBUG makes it 1 and assert a no-op */
  assert(x == 2);
  REQUIRE(x == 2);
}


/* E1: subnormal and underflowing literals are accepted */

void test_literals(void)
{
  start("E1: subnormal literals");
  REQUIRE(dbits(5e-324) == 0x0000000000000001ULL);
  REQUIRE(dbits(1e-310) == 0x000012688b70e62bULL);
  REQUIRE(dbits(2.2250738585072014e-308) == 0x0010000000000000ULL);
  REQUIRE(dbits(1e-400) == 0);
}


/* 3, 4: printf %e, %f, %g */

void test_printf_float(void)
{
  start("3, 4: printf %e %f %g");
  REQUIRE(fmt_is("%.17g", 0.1, "0.10000000000000001"));
  REQUIRE(fmt_is("%.17g", 1.5, "1.5"));
  REQUIRE(fmt_is("%.17g", 1.0/3, "0.33333333333333331"));
  REQUIRE(fmt_is("%.20g", 0.1, "0.10000000000000001")); /* no zeros past 17 digits */
  REQUIRE(fmt_is("%g", 100000., "100000"));
  REQUIRE(fmt_is("%g", 1e6, "1e+06"));
  REQUIRE(fmt_is("%g", 0.0001, "0.0001"));
  REQUIRE(fmt_is("%g", 0.00001, "1e-05"));
  REQUIRE(fmt_is("%#g", 1.5, "1.50000"));
  REQUIRE(fmt_is("%g", 0.0, "0"));
  REQUIRE(fmt_is("%g", -0.0, "-0"));
  REQUIRE(fmt_is("%e", 0.0, "0.000000e+00"));
  REQUIRE(fmt_is("%.16e", 2.2250738585072014e-308, "2.2250738585072014e-308"));
  REQUIRE(fmt_is("%.16e", 4.9406564584124654e-324, "4.9406564584124654e-324"));
  REQUIRE(fmt_is("%g", 1e-310, "1e-310"));
  REQUIRE(fmt_is("%.3e", 1.7976931348623157e308, "1.798e+308"));
  REQUIRE(fmt_is("%f", 1e20, "100000000000000000000.000000"));
  REQUIRE(fmt_is("%.12f", 42.89522312345678, "42.895223123457"));
  REQUIRE(fmt_is("%.0f", 2.5, "2"));   /* exact ties go to even */
  REQUIRE(fmt_is("%.0f", 3.5, "4"));
  REQUIRE(fmt_is("%.2f", 0.125, "0.12"));
  REQUIRE(fmt_is("%.3f", 0.0005, "0.001")); /* 0.0005 is a bit above the tie */
  REQUIRE(fmt_is("%.1f", 0.95, "0.9"));     /* 0.95 is a bit below it */
  REQUIRE(fmt_is("%+010.2f", 3.14159, "+000003.14"));
  REQUIRE(fmt_is("%-10.1e|", 12345., "1.2e+04   |"));
}


/* 4a: strtol overflow */

void test_strtol(void)
{
  char *e; long l; unsigned long ul; long long ll;
  start("4a: strto* overflow and endptr");
  errno = 0; l = strtol("4294967295", &e, 10);
  REQUIRE(l == LONG_MAX && errno == ERANGE && *e == 0);
  errno = 0; l = strtol("-2147483649", &e, 10);
  REQUIRE(l == LONG_MIN && errno == ERANGE);
  errno = 0; l = strtol("-2147483648", &e, 10);
  REQUIRE(l == LONG_MIN && errno == 0);
  errno = 0; ul = strtoul("4294967296", &e, 10);
  REQUIRE(ul == ULONG_MAX && errno == ERANGE);
  errno = 0; ul = strtoul("-1", &e, 10);
  REQUIRE(ul == ULONG_MAX && errno == 0);
  errno = 0; ll = strtoll("9223372036854775808", &e, 10);
  REQUIRE(ll == LLONG_MAX && errno == ERANGE);
  l = strtol("0x", &e, 0);
  REQUIRE(l == 0 && *e == 'x');
  l = strtol("  12abc", &e, 10);
  REQUIRE(l == 12 && *e == 'a');
}


/* 4b, 4c, 5: strtod */

void test_strtod(void)
{
  double d; int n;
  start("4b, 4c, 5: strtod");
  REQUIRE(strtod_is("2.2250738585072014e-308", 0x0010000000000000ULL, 23));
  REQUIRE(strtod_is("1e-310", 0x000012688b70e62bULL, 6));
  REQUIRE(strtod_is("5e-324", 0x0000000000000001ULL, 6));
  REQUIRE(strtod_is("0.000000000012345678909876543210e15", 0x40c81cd6e684d258ULL, 35));
  REQUIRE(strtod_is(".0e011110", 0, 9));
  REQUIRE(strtod_is("1e-600", 0, 6));
  REQUIRE(strtod_is("0.1", 0x3fb999999999999aULL, 3));
  REQUIRE(strtod_is("123456789012345678901234567890", 0x45f8ee90ff6c373eULL, 30));
  REQUIRE(strtod_is("1e", 0x3ff0000000000000ULL, 1));
  REQUIRE(strtod_is("0x1p-1074", 0x0000000000000001ULL, 9));
  REQUIRE(strtod_is("0x1.8p1", 0x4008000000000000ULL, 7));
  REQUIRE(strtod_is("1e400", 0x7ff0000000000000ULL, 5));
  errno = 0; d = strtod("1e400", NULL);
  REQUIRE(errno == ERANGE);
  /* sscanf goes through strtod */
  d = 0; n = sscanf("1e-310", "%lf", &d);
  REQUIRE(n == 1 && dbits(d) == 0x000012688b70e62bULL);
}


/* B1: && and || yield 0 or 1 */

enum b2color { B2RED, B2GREEN = 5, B2BLUE };

static int b1seven(void) { return 7; }

void test_logical_values(void)
{
  int a = 1, b = 5, z = 0, r, cnt = 0; unsigned u = 4; long l = 3;
  long long ll = 1LL << 40; double d = 0.5, dz = 0.0;
  char *p = "x", *n = NULL; bool t = true;
  start("B1: && and || yield 0 or 1");
  r = a && b;      REQUIRE(r == 1);
  r = z || b;      REQUIRE(r == 1);
  r = a || b;      REQUIRE(r == 1);
  r = z && b;      REQUIRE(r == 0);
  r = l && u;      REQUIRE(r == 1);
  r = ll && 1;     REQUIRE(r == 1);
  r = z || ll;     REQUIRE(r == 1);
  r = d && b;      REQUIRE(r == 1);
  r = dz || dz;    REQUIRE(r == 0);
  r = p && *p;     REQUIRE(r == 1);
  r = n || p;      REQUIRE(r == 1);
  r = p || n;      REQUIRE(r == 1);
  r = n && *n;     REQUIRE(r == 0);
  r = t && b;      REQUIRE(r == 1);
  r = b1seven() && b1seven(); REQUIRE(r == 1);
  r = (a && b) + (z || b) + (b && b); REQUIRE(r == 3);
  r = !(a || b);   REQUIRE(r == 0);
  r = a < b && b;  REQUIRE(r == 1);
  cnt += (b && 9); cnt += (z || 3); REQUIRE(cnt == 2);
  r = (int)sizeof(a && b); REQUIRE(r == sizeof(int));
  /* tests (no value needed) still work */
  r = 0; if (p && *p) r = 1; REQUIRE(r == 1);
  r = 0; while (b && z) r = 9; REQUIRE(r == 0);
}


/* B2: enum values in arithmetic */

void test_enum_arith(void)
{
  enum b2color c = B2BLUE, c2 = B2GREEN; int arr[8], *q;
  start("B2: enum values in arithmetic");
  REQUIRE(c + 1 == 7);
  REQUIRE(c * 2 == 12);
  REQUIRE(c - B2GREEN == 1);
  REQUIRE((B2GREEN | 1) == 5);
  REQUIRE(-c == -6 && ~c == -7 && !c == 0);
  REQUIRE(c + 0.5 == 6.5);
  c2 += 1; ++c2;
  REQUIRE((int)c2 == 7);
  arr[B2GREEN] = 42; q = &arr[0] + B2GREEN;
  REQUIRE(*q == 42 && *(B2GREEN + &arr[0]) == 42 && arr[c - 1] == 42);
}


int main(void)
{
  test_long_unsigned();
  test_array_dims();
  test_static_casts();
  test_typedef_list();
  test_line_file();
  test_assert();
  test_literals();
  test_printf_float();
  test_strtol();
  test_strtod();
  test_logical_values();
  test_enum_arith();
  printf("\nFAILURES: %d\n", failcnt);
  return failcnt;
}
