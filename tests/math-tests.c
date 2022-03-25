#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>


#define REQUIRE(x) do { ++testno; if (!(x)) fail(testno, buffer); } while(0)

static int testno = 0;
static int failcnt = 0;
static void fail(int tno, const char *buf)
{
  printf("  test %d (%s) failed -- see above\n", testno, buf);
  ++failcnt;
}

static void start(const char *test)
{
  printf("\n** test set %s **\n", test);
  testno = 0;
}

static int feq(double got, double exp)
{
  if (got == exp) return 1;
  printf("\n");
  printf(">>> expected result was %a (%.9g)\n", exp, exp);
  printf(">>>>>> actual result is %a (%.9g)\n", got, got);
  return 0;
}

#define FEQ(x, y) (feq((x), (y))) //(((x) == (y)) ? 1 : ((x)), 0))

#define inf (asdouble(0x7FF0000000000000ULL)) // INFINITY
#define NaN (asdouble(0x7FF8000000000000ULL)) // NAN

#define M_PI		3.1415926535897932384626433832795029  /* pi */
#define M_PI_2	1.5707963267948966192313216916397514  /* pi/2 */
#define M_PI_4	0.7853981633974483096156608458198757  /* pi/4 */
#define M_1_PI	0.3183098861837906715377675267450287  /* 1/pi */
#define M_2_PI	0.6366197723675813430755350534900574  /* 2/pi */
#define M_2_SQRTPI	1.1283791670955125738961589031215452 /* 2/sqrt(pi) */
#define M_PI_6          0.52359877559829887307710723054658383
#define M_E2            7.389056098930650227230427460575008
#define M_E3            20.085536923187667740928529654581719
#define M_2_SQRT_PI		  3.5449077018110320545963349666822903	/* 2 sqrt (M_PI)  */
#define M_SQRT_PI		    1.7724538509055160272981674833411451	/* sqrt (M_PI)  */
#define M_LOG_SQRT_PI   0.57236494292470008707171367567652933	/* log(sqrt(M_PIl))  */
#define M_LOG_2_SQRT_PI 1.265512123484645396488945797134706	/* log(2*sqrt(M_PIl))  */
#define M_PI_34         (M_PI - M_PI_4)		/* 3*pi/4 */
#define M_PI_34_LOG10E  ((M_PI - M_PI_4) * M_LOG10E)
#define M_PI2_LOG10E    (M_PI_2 * M_LOG10E)
#define M_PI4_LOG10E    (M_PI_4 * M_LOG10E)
#define M_PI_LOG10E     (M_PI * M_LOG10E)
#define M_E		2.7182818284590452353602874713526625  /* e */
#define M_LOG2E	1.4426950408889634073599246810018922  /* log_2 e */
#define M_LOG10E	0.4342944819032518276511289189166051  /* log_10 e */
#define M_LN2		0.6931471805599453094172321214581766  /* log_e 2 */
#define M_LN10	2.3025850929940456840179914546843642  /* log_e 10 */
#define M_SQRT2	1.4142135623730950488016887242096981  /* sqrt(2) */
#define M_SQRT1_2	0.7071067811865475244008443621048490  /* 1/sqrt(2) */

void test_math_acos(void) {
  start("tests for acos()");
  const char *buffer = "";
  double x, y, z;

  buffer = "acos (inf) == NaN plus invalid exception";
  y = acos(inf);
  REQUIRE(!(y == y));

  buffer = "acos (-inf) == NaN plus invalid exception";
  y = acos(-inf);
  REQUIRE(!(y == y));

  buffer = "acos (NaN) == NaN";
  y = acos(NaN);
  REQUIRE(!(y == y));

  buffer = "acos (1.1) == NaN plus invalid exception";
  y = acos(1.1);
  REQUIRE(!(y == y));

  buffer = "acos (-1.1) == NaN plus invalid exception";
  y = acos(-1.1);
  REQUIRE(!(y == y));

  buffer = "acos (0) == pi/2";
  y = acos(0); z = M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "acos (-0) == pi/2";
  y = acos(-0); z = M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "acos (1) == 0";
  y = acos(1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "acos (-1) == pi";
  y = acos(-1); z = M_PI;
  REQUIRE(FEQ(y, z));

  buffer = "acos (0.5) == M_PI_6*2.0";
  y = acos(0.5); z = M_PI_6 * 2.0;
  REQUIRE(FEQ(y, z));

  buffer = "acos (-0.5) == M_PI_6*4.0";
  y = acos(-0.5); z = M_PI_6 * 4.0;
  REQUIRE(FEQ(y, z));

  buffer = "acos (0.7) == 0.79539883018414355549096833892476432";
  y = acos(0.7); z = 0.79539883018414355549096833892476432;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_acosh(void) {
  start("tests for acosh()");
  const char *buffer = "";
  double x, y, z;

  buffer = "acosh (inf) == inf";
  y = acosh(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "acosh (-inf) == NaN plus invalid exception";
  y = acosh(-inf);
  REQUIRE(!(y == y));

  buffer = "acosh (-1.1) == NaN plus invalid exception";
  y = acosh(-1.1);
  REQUIRE(!(y == y));

  buffer = "acosh (1) == 0";
  y = acosh(1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "acosh (7) == 2.633915793849633417250092694615937";
  y = acosh(7); z = 2.633915793849633417250092694615937;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_asin(void) {
  start("tests for asin()");
  const char *buffer = "";
  double x, y, z;

  buffer = "asin (inf) == NaN plus invalid exception";
  y = asin(inf);
  REQUIRE(!(y == y));

  buffer = "asin (-inf) == NaN plus invalid exception";
  y = asin(-inf);
  REQUIRE(!(y == y));

  buffer = "asin (NaN) == NaN";
  y = asin(NaN);
  REQUIRE(!(y == y));

  buffer = "asin (1.1) == NaN plus invalid exception";
  y = asin(1.1);
  REQUIRE(!(y == y));

  buffer = "asin (-1.1) == NaN plus invalid exception";
  y = asin(-1.1);
  REQUIRE(!(y == y));

  buffer = "asin (0) == 0";
  y = asin(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "asin (-0) == -0";
  y = asin(-0); z = -0;
  REQUIRE(FEQ(y, z));

  buffer = "asin (0.5) == pi/6";
  y = asin(0.5); z = M_PI_6;
  REQUIRE(FEQ(y, z));

  buffer = "asin (-0.5) == -pi/6";
  y = asin(-0.5); z = -M_PI_6;
  REQUIRE(FEQ(y, z));

  buffer = "asin (1.0) == pi/2";
  y = asin(1.0); z = M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "asin (-1.0) == -pi/2";
  y = asin(-1.0); z = -M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "asin (0.7) == 0.77539749661075306374035335271498708";
  y = asin(0.7); z = 0.77539749661075306374035335271498708;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_asinh(void) {
  start("tests for asinh()");
  const char *buffer = "";
  double x, y, z;

  buffer = "asinh (0) == 0";
  y = asinh(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "asinh (-0) == -0";
  y = asinh(-0); z = -0;
  REQUIRE(FEQ(y, z));

  buffer = "asinh (inf) == inf";
  y = asinh(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "asinh (-inf) == -inf";
  y = asinh(-inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "asinh (NaN) == NaN";
  y = asinh(NaN);
  REQUIRE(!(y == y));

  buffer = "asinh (0.7) == 0.652666566082355786";
  y = asinh(0.7); z = 0.652666566082355786;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_atan(void) {
  start("tests for atan()");
  const char *buffer = "";
  double x, y, z;

  buffer = "atan (0) == 0";
  y = atan(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "atan (-0) == -0";
  y = atan(-0); z = -0;
  REQUIRE(FEQ(y, z));

  buffer = "atan (inf) == pi/2";
  y = atan(inf); z = M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "atan (-inf) == -pi/2";
  y = atan(-inf); z = -M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "atan (NaN) == NaN";
  y = atan(NaN);
  REQUIRE(!(y == y));

  buffer = "atan (1) == pi/4";
  y = atan(1); z = M_PI_4;
  REQUIRE(FEQ(y, z));

  buffer = "atan (-1) == -pi/4";
  y = atan(-1); z = -M_PI_4;
  REQUIRE(FEQ(y, z));

  buffer = "atan (0.7) == 0.61072596438920861654375887649023613";
  y = atan(0.7); z = 0.61072596438920861654375887649023613;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_atanh(void) {
  start("tests for atanh()");
  const char *buffer = "";
  double x, y, z;

  buffer = "atanh (0) == 0";
  y = atanh(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "atanh (-0) == -0";
  y = atanh(-0); z = -0;
  REQUIRE(FEQ(y, z));

  buffer = "atanh (1) == inf plus division by zero exception";
  y = atanh(1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "atanh (-1) == -inf plus division by zero exception";
  y = atanh(-1); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "atanh (NaN) == NaN";
  y = atanh(NaN);
  REQUIRE(!(y == y));

  buffer = "atanh (1.1) == NaN plus invalid exception";
  y = atanh(1.1);
  REQUIRE(!(y == y));

  buffer = "atanh (-1.1) == NaN plus invalid exception";
  y = atanh(-1.1);
  REQUIRE(!(y == y));

  buffer = "atanh (0.7) == 0.8673005276940531944";
  y = atanh(0.7); z = 0.8673005276940531944;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_atan2(void) {
  start("tests for atan2()");
  const char *buffer = "";
  double x, y, z;

  buffer = "atan2 (0, 1) == 0";
  y = atan2(0, 1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-0.0, 1) == -0.0";
  y = atan2(-0.0, 1); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (0, 0) == 0";
  y = atan2(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-0.0, 0) == -0.0";
  y = atan2(-0.0, 0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (0, -1) == pi";
  y = atan2(0, -1); z = M_PI;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-0.0, -1) == -pi";
  y = atan2(-0.0, -1); z = -M_PI;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (0, -0.0) == pi";
  y = atan2(0, -0.0); z = M_PI;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-0.0, -0.0) == -pi";
  y = atan2(-0.0, -0.0); z = -M_PI;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (1, 0) == pi/2";
  y = atan2(1, 0); z = M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (1, -0.0) == pi/2";
  y = atan2(1, -0.0); z = M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-1, 0) == -pi/2";
  y = atan2(-1, 0); z = -M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-1, -0.0) == -pi/2";
  y = atan2(-1, -0.0); z = -M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (1, inf) == 0";
  y = atan2(1, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-1, inf) == -0.0";
  y = atan2(-1, inf); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (inf, -1) == pi/2";
  y = atan2(inf, -1); z = M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-inf, 1) == -pi/2";
  y = atan2(-inf, 1); z = -M_PI_2;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (1, -inf) == pi";
  y = atan2(1, -inf); z = M_PI;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-1, -inf) == -pi";
  y = atan2(-1, -inf); z = -M_PI;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (inf, inf) == pi/4";
  y = atan2(inf, inf); z = M_PI_4;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-inf, inf) == -pi/4";
  y = atan2(-inf, inf); z = -M_PI_4;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (inf, -inf) == 3/4 pi";
  y = atan2(inf, -inf); z = M_PI_34;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-inf, -inf) == -3/4 pi";
  y = atan2(-inf, -inf); z = -M_PI_34;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (NaN, NaN) == NaN";
  y = atan2(NaN, NaN);
  REQUIRE(!(y == y));

  buffer = "atan2 (0.7, 1) == 0.61072596438920861654375887649023613";
  y = atan2(0.7, 1); z = 0.61072596438920861654375887649023613;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-0.7, 1.0) == -0.61072596438920861654375887649023613";
  y = atan2(-0.7, 1.0); z = -0.61072596438920861654375887649023613;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (0.7, -1.0) == 2.530866689200584621918884506789267";
  y = atan2(0.7, -1.0); z = 2.530866689200584621918884506789267;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (-0.7, -1.0) == -2.530866689200584621918884506789267";
  y = atan2(-0.7, -1.0); z = -2.530866689200584621918884506789267;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (0.4, 0.0003) == 1.5700463269355215717704032607580829";
  y = atan2(0.4, 0.0003); z = 1.5700463269355215717704032607580829;
  REQUIRE(FEQ(y, z));

  buffer = "atan2 (1.4, -0.93) == 2.1571487668237843754887415992772736";
  y = atan2(1.4, -0.93); z = 2.1571487668237843754887415992772736;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_cbrt(void) {
  start("tests for cbrt()");
  const char *buffer = "";
  double x, y, z;

  buffer = "cbrt (0.0) == 0.0";
  y = cbrt(0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "cbrt (-0.0) == -0.0";
  y = cbrt(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "cbrt (inf) == inf";
  y = cbrt(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "cbrt (-inf) == -inf";
  y = cbrt(-inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "cbrt (NaN) == NaN";
  y = cbrt(NaN);
  REQUIRE(!(y == y));

  buffer = "cbrt (-0.001) == -0.1";
  y = cbrt(-0.001); z = -0.1;
  REQUIRE(FEQ(y, z));

  buffer = "cbrt (8) == 2";
  y = cbrt(8); z = 2;
  REQUIRE(FEQ(y, z));

  buffer = "cbrt (-27.0) == -3.0";
  y = cbrt(-27.0); z = -3.0;
  REQUIRE(FEQ(y, z));

  buffer = "cbrt (0.970299) == 0.99";
  y = cbrt(0.970299); z = 0.99;
  REQUIRE(FEQ(y, z));

  buffer = "cbrt (0.7) == 0.8879040017426007084";
  y = cbrt(0.7); z = 0.8879040017426007084;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_ceil(void) {
  start("tests for ceil()");
  const char *buffer = "";
  double x, y, z;

  buffer = "ceil (0.0) == 0.0";
  y = ceil(0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "ceil (-0.0) == -0.0";
  y = ceil(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "ceil (inf) == inf";
  y = ceil(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "ceil (-inf) == -inf";
  y = ceil(-inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "ceil (NaN) == NaN";
  y = ceil(NaN);
  REQUIRE(!(y == y));

  buffer = "ceil (pi) == 4.0";
  y = ceil(M_PI); z = 4.0;
  REQUIRE(FEQ(y, z));

  buffer = "ceil (-pi) == -3.0";
  y = ceil(-M_PI); z = -3.0;
  REQUIRE(FEQ(y, z));
}

void test_math_copysign(void) {
  start("tests for copysign()");
  const char *buffer = "";
  double x, y, z;

  buffer = "copysign (0.0, 4.0) == 0.0";
  y = copysign(0.0, 4.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (0, -4) == -0.0";
  y = copysign(0, -4); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (-0.0, 4) == 0";
  y = copysign(-0.0, 4); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (-0.0, -4) == -0.0";
  y = copysign(-0.0, -4); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (inf, 0) == inf";
  y = copysign(inf, 0); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (inf, -0.0) == -inf";
  y = copysign(inf, -0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (-inf, 0) == inf";
  y = copysign(-inf, 0); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (-inf, -0.0) == -inf";
  y = copysign(-inf, -0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (0, inf) == 0";
  y = copysign(0, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (0, -0.0) == -0.0";
  y = copysign(0, -0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (-0.0, inf) == 0";
  y = copysign(-0.0, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (-0.0, -0.0) == -0.0";
  y = copysign(-0.0, -0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "copysign (NaN, 0) == NaN";
  y = copysign(NaN, 0);
  REQUIRE(!(y == y));

  buffer = "copysign (NaN, -0.0) == NaN";
  y = copysign(NaN, -0.0);
  REQUIRE(!(y == y));

  buffer = "copysign (-NaN, 0) == NaN";
  y = copysign(-NaN, 0);
  REQUIRE(!(y == y));

  buffer = "copysign (-NaN, -0.0) == NaN";
  y = copysign(-NaN, -0.0);
  REQUIRE(!(y == y));
}

void test_math_cos(void) {
  start("tests for cos()");
  const char *buffer = "";
  double x, y, z;

  buffer = "cos (0) == 1";
  y = cos(0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "cos (-0.0) == 1";
  y = cos(-0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "cos (inf) == NaN plus invalid exception";
  y = cos(inf);
  REQUIRE(!(y == y));

  buffer = "cos (-inf) == NaN plus invalid exception";
  y = cos(-inf);
  REQUIRE(!(y == y));

  buffer = "cos (NaN) == NaN";
  y = cos(NaN);
  REQUIRE(!(y == y));

  buffer = "cos (M_PI_6 * 2.0) == 0.5";
  y = cos(M_PI_6 * 2.0); z = 0.5;
  REQUIRE(FEQ(y, z));

  buffer = "cos (M_PI_6 * 4.0) == -0.5";
  y = cos(M_PI_6 * 4.0); z = -0.5;
  REQUIRE(FEQ(y, z));

  buffer = "cos (pi/2) == 0";
  y = cos(M_PI_2); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "cos (0.7) == 0.76484218728448842625585999019186495";
  y = cos(0.7); z = 0.76484218728448842625585999019186495;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_cosh(void) {
  start("tests for cosh()");
  const char *buffer = "";
  double x, y, z;

  buffer = "cosh (0) == 1";
  y = cosh(0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "cosh (-0.0) == 1";
  y = cosh(-0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "cosh (inf) == inf";
  y = cosh(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "cosh (-inf) == inf";
  y = cosh(-inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "cosh (NaN) == NaN";
  y = cosh(NaN);
  REQUIRE(!(y == y));

  buffer = "cosh (0.7) == 1.255169005630943018";
  y = cosh(0.7); z = 1.255169005630943018;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_erf(void) {
  start("tests for erf()");
  const char *buffer = "";
  double x, y, z;

  buffer = "erf (0) == 0";
  y = erf(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "erf (-0.0) == -0.0";
  y = erf(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "erf (inf) == 1";
  y = erf(inf); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "erf (-inf) == -1";
  y = erf(-inf); z = -1;
  REQUIRE(FEQ(y, z));

  buffer = "erf (NaN) == NaN";
  y = erf(NaN);
  REQUIRE(!(y == y));

  buffer = "erf (0.7) == 0.67780119383741847297";
  y = erf(0.7); z = 0.67780119383741847297;
  REQUIRE(FEQ(y, z));

  buffer = "erf (1.2) == 0.91031397822963538024";
  y = erf(1.2); z = 0.91031397822963538024;
  REQUIRE(FEQ(y, z));

  buffer = "erf (2.0) == 0.99532226501895273416";
  y = erf(2.0); z = 0.99532226501895273416;
  REQUIRE(FEQ(y, z));

  buffer = "erf (4.1) == 0.99999999329997234592";
  y = erf(4.1); z = 0.99999999329997234592;
  REQUIRE(FEQ(y, z));

  buffer = "erf (27) == 1.0";
  y = erf(27); z = 1.0;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_erfc(void) {
  start("tests for erfc()");
  const char *buffer = "";
  double x, y, z;

  buffer = "erfc (inf) == 0.0";
  y = erfc(inf); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "erfc (-inf) == 2.0";
  y = erfc(-inf); z = 2.0;
  REQUIRE(FEQ(y, z));

  buffer = "erfc (0.0) == 1.0";
  y = erfc(0.0); z = 1.0;
  REQUIRE(FEQ(y, z));

  buffer = "erfc (-0.0) == 1.0";
  y = erfc(-0.0); z = 1.0;
  REQUIRE(FEQ(y, z));

  buffer = "erfc (NaN) == NaN";
  y = erfc(NaN);
  REQUIRE(!(y == y));

  buffer = "erfc (0.7) == 0.32219880616258152702";
  y = erfc(0.7); z = 0.32219880616258152702;
  REQUIRE(FEQ(y, z));

  buffer = "erfc (1.2) == 0.089686021770364619762";
  y = erfc(1.2); z = 0.089686021770364619762;
  REQUIRE(FEQ(y, z));

  buffer = "erfc (2.0) == 0.0046777349810472658379";
  y = erfc(2.0); z = 0.0046777349810472658379;
  REQUIRE(FEQ(y, z));

  buffer = "erfc (4.1) == 0.67000276540848983727e-8";
  y = erfc(4.1); z = 0.67000276540848983727e-8;
  REQUIRE(FEQ(y, z));

  buffer = "erfc (9) == 0.41370317465138102381e-36";
  y = erfc(9); z = 0.41370317465138102381e-36;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_exp(void) {
  start("tests for exp()");
  const char *buffer = "";
  double x, y, z;

  buffer = "exp (0) == 1";
  y = exp(0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "exp (-0.0) == 1";
  y = exp(-0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "exp (inf) == inf";
  y = exp(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "exp (-inf) == 0";
  y = exp(-inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "exp (NaN) == NaN";
  y = exp(NaN);
  REQUIRE(!(y == y));

  buffer = "exp (1) == e";
  y = exp(1); z = M_E;
  REQUIRE(FEQ(y, z));

  buffer = "exp (2) == e^2";
  y = exp(2); z = M_E2;
  REQUIRE(FEQ(y, z));

  buffer = "exp (3) == e^3";
  y = exp(3); z = M_E3;
  REQUIRE(FEQ(y, z));

  buffer = "exp (0.7) == 2.0137527074704765216";
  y = exp(0.7); z = 2.0137527074704765216;
  REQUIRE(FEQ(y, z));

  buffer = "exp (50.0) == 5184705528587072464087.45332293348538";
  y = exp(50.0); z = 5184705528587072464087.45332293348538;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_exp10(void) {
  start("tests for exp10()");
  const char *buffer = "";
  double x, y, z;

  buffer = "exp10 (0) == 1";
  y = exp10(0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "exp10 (-0.0) == 1";
  y = exp10(-0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "exp10 (inf) == inf";
  y = exp10(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "exp10 (-inf) == 0";
  y = exp10(-inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "exp10 (NaN) == NaN";
  y = exp10(NaN);
  REQUIRE(!(y == y));

  buffer = "exp10 (3) == 1000";
  y = exp10(3); z = 1000;
  REQUIRE(FEQ(y, z));

  buffer = "exp10 (-1) == 0.1";
  y = exp10(-1); z = 0.1;
  REQUIRE(FEQ(y, z));

  buffer = "exp10 (1e6) == inf";
  y = exp10(1e6); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "exp10 (-1e6) == 0";
  y = exp10(-1e6); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "exp10 (0.7) == 5.0118723362727228500155418688494574";
  y = exp10(0.7); z = 5.0118723362727228500155418688494574;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_exp2(void) {
  start("tests for exp2()");
  const char *buffer = "";
  double x, y, z;

  buffer = "exp2 (0) == 1";
  y = exp2(0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "exp2 (-0.0) == 1";
  y = exp2(-0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "exp2 (inf) == inf";
  y = exp2(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "exp2 (-inf) == 0";
  y = exp2(-inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "exp2 (NaN) == NaN";
  y = exp2(NaN);
  REQUIRE(!(y == y));

  buffer = "exp2 (10) == 1024";
  y = exp2(10); z = 1024;
  REQUIRE(FEQ(y, z));

  buffer = "exp2 (-1) == 0.5";
  y = exp2(-1); z = 0.5;
  REQUIRE(FEQ(y, z));

  buffer = "exp2 (1e6) == inf";
  y = exp2(1e6); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "exp2 (-1e6) == 0";
  y = exp2(-1e6); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "exp2 (0.7) == 1.6245047927124710452";
  y = exp2(0.7); z = 1.6245047927124710452;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_expm1(void) {
  start("tests for expm1()");
  const char *buffer = "";
  double x, y, z;

  buffer = "expm1 (0) == 0";
  y = expm1(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "expm1 (-0.0) == -0.0";
  y = expm1(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "expm1 (inf) == inf";
  y = expm1(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "expm1 (-inf) == -1";
  y = expm1(-inf); z = -1;
  REQUIRE(FEQ(y, z));

  buffer = "expm1 (NaN) == NaN";
  y = expm1(NaN);
  REQUIRE(!(y == y));

  buffer = "expm1 (1) == M_El - 1.0";
  y = expm1(1); z = M_E - 1.0;
  REQUIRE(FEQ(y, z));

  buffer = "expm1 (0.7) == 1.0137527074704765216";
  y = expm1(0.7); z = 1.0137527074704765216;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_fabs(void) {
  start("tests for fabs()");
  const char *buffer = "";
  double x, y, z;

  buffer = "fabs (0) == 0";
  y = fabs(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fabs (-0.0) == 0";
  y = fabs(-0.0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fabs (inf) == inf";
  y = fabs(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fabs (-inf) == inf";
  y = fabs(-inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fabs (NaN) == NaN";
  y = fabs(NaN);
  REQUIRE(!(y == y));

  buffer = "fabs (38.0) == 38.0";
  y = fabs(38.0); z = 38.0;
  REQUIRE(FEQ(y, z));

  buffer = "fabs (-e) == e";
  y = fabs(-M_E); z = M_E;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_fdim(void) {
  start("tests for fdim()");
  const char *buffer = "";
  double x, y, z;

  buffer = "fdim (0, 0) == 0";
  y = fdim(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (9, 0) == 9";
  y = fdim(9, 0); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (0, 9) == 0";
  y = fdim(0, 9); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (-9, 0) == 0";
  y = fdim(-9, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (0, -9) == 9";
  y = fdim(0, -9); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (inf, 9) == inf";
  y = fdim(inf, 9); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (inf, -9) == inf";
  y = fdim(inf, -9); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (-inf, 9) == 0";
  y = fdim(-inf, 9); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (-inf, -9) == 0";
  y = fdim(-inf, -9); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (9, -inf) == inf";
  y = fdim(9, -inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (-9, -inf) == inf";
  y = fdim(-9, -inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (9, inf) == 0";
  y = fdim(9, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (-9, inf) == 0";
  y = fdim(-9, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fdim (0, NaN) == NaN";
  y = fdim(0, NaN);
  REQUIRE(!(y == y));

  buffer = "fdim (9, NaN) == NaN";
  y = fdim(9, NaN);
  REQUIRE(!(y == y));

  buffer = "fdim (-9, NaN) == NaN";
  y = fdim(-9, NaN);
  REQUIRE(!(y == y));

  buffer = "fdim (NaN, 9) == NaN";
  y = fdim(NaN, 9);
  REQUIRE(!(y == y));

  buffer = "fdim (NaN, -9) == NaN";
  y = fdim(NaN, -9);
  REQUIRE(!(y == y));

  buffer = "fdim (inf, NaN) == NaN";
  y = fdim(inf, NaN);
  REQUIRE(!(y == y));

  buffer = "fdim (-inf, NaN) == NaN";
  y = fdim(-inf, NaN);
  REQUIRE(!(y == y));

  buffer = "fdim (NaN, inf) == NaN";
  y = fdim(NaN, inf);
  REQUIRE(!(y == y));

  buffer = "fdim (NaN, -inf) == NaN";
  y = fdim(NaN, -inf);
  REQUIRE(!(y == y));

  buffer = "fdim (NaN, NaN) == NaN";
  y = fdim(NaN, NaN);
  REQUIRE(!(y == y));
}
*/

void test_math_floor(void) {
  start("tests for floor()");
  const char *buffer = "";
  double x, y, z;

  buffer = "floor (0.0) == 0.0";
  y = floor(0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "floor (-0.0) == -0.0";
  y = floor(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "floor (inf) == inf";
  y = floor(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "floor (-inf) == -inf";
  y = floor(-inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "floor (NaN) == NaN";
  y = floor(NaN);
  REQUIRE(!(y == y));

  buffer = "floor (pi) == 3.0";
  y = floor(M_PI); z = 3.0;
  REQUIRE(FEQ(y, z));

  buffer = "floor (-pi) == -4.0";
  y = floor(-M_PI); z = -4.0;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_fma(void) {
  start("tests for fma()");
  const char *buffer = "";
  double x, y, z;

  buffer = "fma (1.0, 2.0, 3.0) == 5.0";
  y = fma(1.0, 2.0, 3.0); z = 5.0;
  REQUIRE(FEQ(y, z));

  buffer = "fma (NaN, 2.0, 3.0) == NaN";
  y = fma(NaN, 2.0, 3.0);
  REQUIRE(!(y == y));

  buffer = "fma (1.0, NaN, 3.0) == NaN";
  y = fma(1.0, NaN, 3.0);
  REQUIRE(!(y == y));

  buffer = "fma (1.0, 2.0, NaN) == NaN plus invalid exception allowed";
  y = fma(1.0, 2.0, NaN);
  REQUIRE(!(y == y));

  buffer = "fma (inf, 0.0, NaN) == NaN plus invalid exception allowed";
  y = fma(inf, 0.0, NaN);
  REQUIRE(!(y == y));

  buffer = "fma (-inf, 0.0, NaN) == NaN plus invalid exception allowed";
  y = fma(-inf, 0.0, NaN);
  REQUIRE(!(y == y));

  buffer = "fma (0.0, inf, NaN) == NaN plus invalid exception allowed";
  y = fma(0.0, inf, NaN);
  REQUIRE(!(y == y));

  buffer = "fma (0.0, -inf, NaN) == NaN plus invalid exception allowed";
  y = fma(0.0, -inf, NaN);
  REQUIRE(!(y == y));

  buffer = "fma (inf, 0.0, 1.0) == NaN plus invalid exception";
  y = fma(inf, 0.0, 1.0);
  REQUIRE(!(y == y));

  buffer = "fma (-inf, 0.0, 1.0) == NaN plus invalid exception";
  y = fma(-inf, 0.0, 1.0);
  REQUIRE(!(y == y));

  buffer = "fma (0.0, inf, 1.0) == NaN plus invalid exception";
  y = fma(0.0, inf, 1.0);
  REQUIRE(!(y == y));

  buffer = "fma (0.0, -inf, 1.0) == NaN plus invalid exception";
  y = fma(0.0, -inf, 1.0);
  REQUIRE(!(y == y));

  buffer = "fma (inf, inf, -inf) == NaN plus invalid exception";
  y = fma(inf, inf, -inf);
  REQUIRE(!(y == y));

  buffer = "fma (-inf, inf, inf) == NaN plus invalid exception";
  y = fma(-inf, inf, inf);
  REQUIRE(!(y == y));

  buffer = "fma (inf, -inf, inf) == NaN plus invalid exception";
  y = fma(inf, -inf, inf);
  REQUIRE(!(y == y));

  buffer = "fma (-inf, -inf, -inf) == NaN plus invalid exception";
  y = fma(-inf, -inf, -inf);
  REQUIRE(!(y == y));
}
*/

void test_math_fmax(void) {
  start("tests for fmax()");
  const char *buffer = "";
  double x, y, z;

  buffer = "fmax (0, 0) == 0";
  y = fmax(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (-0.0, -0.0) == -0.0";
  y = fmax(-0.0, -0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (9, 0) == 9";
  y = fmax(9, 0); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (0, 9) == 9";
  y = fmax(0, 9); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (-9, 0) == 0";
  y = fmax(-9, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (0, -9) == 0";
  y = fmax(0, -9); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (inf, 9) == inf";
  y = fmax(inf, 9); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (0, inf) == inf";
  y = fmax(0, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (-9, inf) == inf";
  y = fmax(-9, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (inf, -9) == inf";
  y = fmax(inf, -9); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (-inf, 9) == 9";
  y = fmax(-inf, 9); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (-inf, -9) == -9";
  y = fmax(-inf, -9); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (9, -inf) == 9";
  y = fmax(9, -inf); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (-9, -inf) == -9";
  y = fmax(-9, -inf); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (0, NaN) == 0";
  y = fmax(0, NaN); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (9, NaN) == 9";
  y = fmax(9, NaN); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (-9, NaN) == -9";
  y = fmax(-9, NaN); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (NaN, 0) == 0";
  y = fmax(NaN, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (NaN, 9) == 9";
  y = fmax(NaN, 9); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (NaN, -9) == -9";
  y = fmax(NaN, -9); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (inf, NaN) == inf";
  y = fmax(inf, NaN); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (-inf, NaN) == -inf";
  y = fmax(-inf, NaN); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (NaN, inf) == inf";
  y = fmax(NaN, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (NaN, -inf) == -inf";
  y = fmax(NaN, -inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmax (NaN, NaN) == NaN";
  y = fmax(NaN, NaN);
  REQUIRE(!(y == y));
}

void test_math_fmin(void) {
  start("tests for fmin()");
  const char *buffer = "";
  double x, y, z;

  buffer = "fmin (0, 0) == 0";
  y = fmin(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (-0.0, -0.0) == -0.0";
  y = fmin(-0.0, -0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (9, 0) == 0";
  y = fmin(9, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (0, 9) == 0";
  y = fmin(0, 9); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (-9, 0) == -9";
  y = fmin(-9, 0); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (0, -9) == -9";
  y = fmin(0, -9); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (inf, 9) == 9";
  y = fmin(inf, 9); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (9, inf) == 9";
  y = fmin(9, inf); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (inf, -9) == -9";
  y = fmin(inf, -9); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (-9, inf) == -9";
  y = fmin(-9, inf); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (-inf, 9) == -inf";
  y = fmin(-inf, 9); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (-inf, -9) == -inf";
  y = fmin(-inf, -9); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (9, -inf) == -inf";
  y = fmin(9, -inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (-9, -inf) == -inf";
  y = fmin(-9, -inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (0, NaN) == 0";
  y = fmin(0, NaN); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (9, NaN) == 9";
  y = fmin(9, NaN); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (-9, NaN) == -9";
  y = fmin(-9, NaN); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (NaN, 0) == 0";
  y = fmin(NaN, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (NaN, 9) == 9";
  y = fmin(NaN, 9); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (NaN, -9) == -9";
  y = fmin(NaN, -9); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (inf, NaN) == inf";
  y = fmin(inf, NaN); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (-inf, NaN) == -inf";
  y = fmin(-inf, NaN); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (NaN, inf) == inf";
  y = fmin(NaN, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (NaN, -inf) == -inf";
  y = fmin(NaN, -inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "fmin (NaN, NaN) == NaN";
  y = fmin(NaN, NaN);
  REQUIRE(!(y == y));
}

void test_math_fmod(void) {
  start("tests for fmod()");
  const char *buffer = "";
  double x, y, z;

  buffer = "fmod (0, 3) == 0";
  y = fmod(0, 3); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "fmod (-0.0, 3) == -0.0";
  y = fmod(-0.0, 3); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "fmod (inf, 3) == NaN plus invalid exception";
  y = fmod(inf, 3);
  REQUIRE(!(y == y));

  buffer = "fmod (-inf, 3) == NaN plus invalid exception";
  y = fmod(-inf, 3);
  REQUIRE(!(y == y));

  buffer = "fmod (3, 0) == NaN plus invalid exception";
  y = fmod(3, 0);
  REQUIRE(!(y == y));

  buffer = "fmod (3, -0.0) == NaN plus invalid exception";
  y = fmod(3, -0.0);
  REQUIRE(!(y == y));

  buffer = "fmod (3.0, inf) == 3.0";
  y = fmod(3.0, inf); z = 3.0;
  REQUIRE(FEQ(y, z));

  buffer = "fmod (3.0, -inf) == 3.0";
  y = fmod(3.0, -inf); z = 3.0;
  REQUIRE(FEQ(y, z));

  buffer = "fmod (NaN, NaN) == NaN";
  y = fmod(NaN, NaN);
  REQUIRE(!(y == y));

  buffer = "fmod (6.5, 2.3) == 1.9";
  y = fmod(6.5, 2.3); z = 1.9;
  REQUIRE(FEQ(y, z));

  buffer = "fmod (-6.5, 2.3) == -1.9";
  y = fmod(-6.5, 2.3); z = -1.9;
  REQUIRE(FEQ(y, z));

  buffer = "fmod (6.5, -2.3) == 1.9";
  y = fmod(6.5, -2.3); z = 1.9;
  REQUIRE(FEQ(y, z));

  buffer = "fmod (-6.5, -2.3) == -1.9";
  y = fmod(-6.5, -2.3); z = -1.9;
  REQUIRE(FEQ(y, z));
}

void test_math_frexp(void) {
  start("tests for frexp()");
  const char *buffer = "";
  int x;  double y, z;

  buffer = "frexp (inf, &x) == inf";
  y = frexp(inf, &x); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "frexp (-inf, &x) == -inf";
  y = frexp(-inf, &x); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "frexp (NaN, &x) == NaN";
  y = frexp(NaN, &x);
  REQUIRE(!(y == y));

  buffer = "frexp (0.0, &x) == 0.0";
  y = frexp(0.0, &x); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "frexp (-0.0, &x) == -0.0";
  y = frexp(-0.0, &x); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "frexp (12.8, &x) == 0.8";
  y = frexp(12.8, &x); z = 0.8;
  REQUIRE(FEQ(y, z));

  buffer = "frexp (-27.34, &x) == -0.854375";
  y = frexp(-27.34, &x); z = -0.854375;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_gamma(void) {
  start("tests for gamma()");
  const char *buffer = "";
  double x, y, z;

  buffer = "gamma (inf) == inf";
  y = gamma(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "gamma (0) == inf plus division by zero exception";
  y = gamma(0); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "gamma (-3) == inf plus division by zero exception";
  y = gamma(-3); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "gamma (-inf) == inf";
  y = gamma(-inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "gamma (NaN) == NaN";
  y = gamma(NaN);
  REQUIRE(!(y == y));

  buffer = "gamma (1) == 0";
  y = gamma(1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "gamma (3) == M_LN2l";
  y = gamma(3); z = M_LN2l;
  REQUIRE(FEQ(y, z));

  buffer = "gamma (0.5) == log(sqrt(pi))";
  y = gamma(0.5); z = log(sqrt(pi));
  REQUIRE(FEQ(y, z));

  buffer = "gamma (-0.5) == log(2*sqrt(pi))";
  y = gamma(-0.5); z = log(2 * sqrt(pi));
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_hypot(void) {
  start("tests for hypot()");
  const char *buffer = "";
  double x, y, z;

  buffer = "hypot (inf, 1) == inf plus sign of zero/inf not specified";
  y = hypot(inf, 1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (-inf, 1) == inf plus sign of zero/inf not specified";
  y = hypot(-inf, 1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (inf, NaN) == inf";
  y = hypot(inf, NaN); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (-inf, NaN) == inf";
  y = hypot(-inf, NaN); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (NaN, inf) == inf";
  y = hypot(NaN, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (NaN, -inf) == inf";
  y = hypot(NaN, -inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (NaN, NaN) == NaN";
  y = hypot(NaN, NaN);
  REQUIRE(!(y == y));

  buffer = "hypot (0.7, 12.4) == 12.419742348374220601176836866763271";
  y = hypot(0.7, 12.4); z = 12.419742348374220601176836866763271;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (-0.7, 12.4) == 12.419742348374220601176836866763271";
  y = hypot(-0.7, 12.4); z = 12.419742348374220601176836866763271;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (0.7, -12.4) == 12.419742348374220601176836866763271";
  y = hypot(0.7, -12.4); z = 12.419742348374220601176836866763271;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (-0.7, -12.4) == 12.419742348374220601176836866763271";
  y = hypot(-0.7, -12.4); z = 12.419742348374220601176836866763271;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (12.4, 0.7) == 12.419742348374220601176836866763271";
  y = hypot(12.4, 0.7); z = 12.419742348374220601176836866763271;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (-12.4, 0.7) == 12.419742348374220601176836866763271";
  y = hypot(-12.4, 0.7); z = 12.419742348374220601176836866763271;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (12.4, -0.7) == 12.419742348374220601176836866763271";
  y = hypot(12.4, -0.7); z = 12.419742348374220601176836866763271;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (-12.4, -0.7) == 12.419742348374220601176836866763271";
  y = hypot(-12.4, -0.7); z = 12.419742348374220601176836866763271;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (0.7, 0) == 0.7";
  y = hypot(0.7, 0); z = 0.7;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (-0.7, 0) == 0.7";
  y = hypot(-0.7, 0); z = 0.7;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (-5.7e7, 0) == 5.7e7";
  y = hypot(-5.7e7, 0); z = 5.7e7;
  REQUIRE(FEQ(y, z));

  buffer = "hypot (0.7, 1.2) == 1.3892443989449804508432547041028554";
  y = hypot(0.7, 1.2); z = 1.3892443989449804508432547041028554;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_j0(void) {
  start("tests for j0()");
  const char *buffer = "";
  double x, y, z;

  buffer = "j0 (NaN) == NaN";
  y = j0(NaN);
  REQUIRE(!(y == y));

  buffer = "j0 (inf) == 0";
  y = j0(inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (-1.0) == 0.76519768655796655145";
  y = j0(-1.0); z = 0.76519768655796655145;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (0.0) == 1.0";
  y = j0(0.0); z = 1.0;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (0.1) == 0.99750156206604003228";
  y = j0(0.1); z = 0.99750156206604003228;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (0.7) == 0.88120088860740528084";
  y = j0(0.7); z = 0.88120088860740528084;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (1.0) == 0.76519768655796655145";
  y = j0(1.0); z = 0.76519768655796655145;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (1.5) == 0.51182767173591812875";
  y = j0(1.5); z = 0.51182767173591812875;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (2.0) == 0.22389077914123566805";
  y = j0(2.0); z = 0.22389077914123566805;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (8.0) == 0.17165080713755390609";
  y = j0(8.0); z = 0.17165080713755390609;
  REQUIRE(FEQ(y, z));

  buffer = "j0 (10.0) == -0.24593576445134833520";
  y = j0(10.0); z = -0.24593576445134833520;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_j1(void) {
  start("tests for j1()");
  const char *buffer = "";
  double x, y, z;

  buffer = "j1 (NaN) == NaN";
  y = j1(NaN);
  REQUIRE(!(y == y));

  buffer = "j1 (inf) == 0";
  y = j1(inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (-1.0) == -0.44005058574493351596";
  y = j1(-1.0); z = -0.44005058574493351596;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (0.0) == 0.0";
  y = j1(0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (0.1) == 0.049937526036241997556";
  y = j1(0.1); z = 0.049937526036241997556;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (0.7) == 0.32899574154005894785";
  y = j1(0.7); z = 0.32899574154005894785;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (1.0) == 0.44005058574493351596";
  y = j1(1.0); z = 0.44005058574493351596;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (1.5) == 0.55793650791009964199";
  y = j1(1.5); z = 0.55793650791009964199;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (2.0) == 0.57672480775687338720";
  y = j1(2.0); z = 0.57672480775687338720;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (8.0) == 0.23463634685391462438";
  y = j1(8.0); z = 0.23463634685391462438;
  REQUIRE(FEQ(y, z));

  buffer = "j1 (10.0) == 0.043472746168861436670";
  y = j1(10.0); z = 0.043472746168861436670;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_jn(void) {
  start("tests for jn()");
  const char *buffer = "";
  double x, y, z;

  buffer = "jn (0, NaN) == NaN";
  y = jn(0, NaN);
  REQUIRE(!(y == y));

  buffer = "jn (0, inf) == 0";
  y = jn(0, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, -1.0) == 0.76519768655796655145";
  y = jn(0, -1.0); z = 0.76519768655796655145;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, 0.0) == 1.0";
  y = jn(0, 0.0); z = 1.0;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, 0.1) == 0.99750156206604003228";
  y = jn(0, 0.1); z = 0.99750156206604003228;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, 0.7) == 0.88120088860740528084";
  y = jn(0, 0.7); z = 0.88120088860740528084;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, 1.0) == 0.76519768655796655145";
  y = jn(0, 1.0); z = 0.76519768655796655145;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, 1.5) == 0.51182767173591812875";
  y = jn(0, 1.5); z = 0.51182767173591812875;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, 2.0) == 0.22389077914123566805";
  y = jn(0, 2.0); z = 0.22389077914123566805;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, 8.0) == 0.17165080713755390609";
  y = jn(0, 8.0); z = 0.17165080713755390609;
  REQUIRE(FEQ(y, z));

  buffer = "jn (0, 10.0) == -0.24593576445134833520";
  y = jn(0, 10.0); z = -0.24593576445134833520;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, NaN) == NaN";
  y = jn(1, NaN);
  REQUIRE(!(y == y));

  buffer = "jn (1, inf) == 0";
  y = jn(1, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, -1.0) == -0.44005058574493351596";
  y = jn(1, -1.0); z = -0.44005058574493351596;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, 0.0) == 0.0";
  y = jn(1, 0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, 0.1) == 0.049937526036241997556";
  y = jn(1, 0.1); z = 0.049937526036241997556;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, 0.7) == 0.32899574154005894785";
  y = jn(1, 0.7); z = 0.32899574154005894785;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, 1.0) == 0.44005058574493351596";
  y = jn(1, 1.0); z = 0.44005058574493351596;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, 1.5) == 0.55793650791009964199";
  y = jn(1, 1.5); z = 0.55793650791009964199;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, 2.0) == 0.57672480775687338720";
  y = jn(1, 2.0); z = 0.57672480775687338720;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, 8.0) == 0.23463634685391462438";
  y = jn(1, 8.0); z = 0.23463634685391462438;
  REQUIRE(FEQ(y, z));

  buffer = "jn (1, 10.0) == 0.043472746168861436670";
  y = jn(1, 10.0); z = 0.043472746168861436670;
  REQUIRE(FEQ(y, z));

  buffer = "jn (3, NaN) == NaN";
  y = jn(3, NaN);
  REQUIRE(!(y == y));

  buffer = "jn (3, inf) == 0";
  y = jn(3, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "jn (3, -1.0) == -0.019563353982668405919";
  y = jn(3, -1.0); z = -0.019563353982668405919;
  REQUIRE(FEQ(y, z));

  buffer = "jn (3, 0.0) == 0.0";
  y = jn(3, 0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "jn (3, 0.1) == 0.000020820315754756261429";
  y = jn(3, 0.1); z = 0.000020820315754756261429;
  REQUIRE(FEQ(y, z));

  buffer = "jn (3, 0.7) == 0.0069296548267508408077";
  y = jn(3, 0.7); z = 0.0069296548267508408077;
  REQUIRE(FEQ(y, z));

  buffer = "jn (3, 1.0) == 0.019563353982668405919";
  y = jn(3, 1.0); z = 0.019563353982668405919;
  REQUIRE(FEQ(y, z));

  buffer = "jn (3, 2.0) == 0.12894324947440205110";
  y = jn(3, 2.0); z = 0.12894324947440205110;
  REQUIRE(FEQ(y, z));

  buffer = "jn (3, 10.0) == 0.058379379305186812343";
  y = jn(3, 10.0); z = 0.058379379305186812343;
  REQUIRE(FEQ(y, z));

  buffer = "jn (10, NaN) == NaN";
  y = jn(10, NaN);
  REQUIRE(!(y == y));

  buffer = "jn (10, inf) == 0";
  y = jn(10, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "jn (10, -1.0) == 0.26306151236874532070e-9";
  y = jn(10, -1.0); z = 0.26306151236874532070e-9;
  REQUIRE(FEQ(y, z));

  buffer = "jn (10, 0.0) == 0.0";
  y = jn(10, 0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "jn (10, 0.1) == 0.26905328954342155795e-19";
  y = jn(10, 0.1); z = 0.26905328954342155795e-19;
  REQUIRE(FEQ(y, z));

  buffer = "jn (10, 0.7) == 0.75175911502153953928e-11";
  y = jn(10, 0.7); z = 0.75175911502153953928e-11;
  REQUIRE(FEQ(y, z));

  buffer = "jn (10, 1.0) == 0.26306151236874532070e-9";
  y = jn(10, 1.0); z = 0.26306151236874532070e-9;
  REQUIRE(FEQ(y, z));

  buffer = "jn (10, 2.0) == 0.25153862827167367096e-6";
  y = jn(10, 2.0); z = 0.25153862827167367096e-6;
  REQUIRE(FEQ(y, z));

  buffer = "jn (10, 10.0) == 0.20748610663335885770";
  y = jn(10, 10.0); z = 0.20748610663335885770;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_ldexp(void) {
  start("tests for ldexp()");
  const char *buffer = "";
  int x; double y, z;

  buffer = "ldexp (0, 0) == 0";
  y = ldexp(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "ldexp (-0.0, 0) == -0.0";
  y = ldexp(-0.0, 0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "ldexp (inf, 1) == inf";
  y = ldexp(inf, 1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "ldexp (-inf, 1) == -inf";
  y = ldexp(-inf, 1); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "ldexp (NaN, 1) == NaN";
  y = ldexp(NaN, 1);
  REQUIRE(!(y == y));

  buffer = "ldexp (0.8, 4) == 12.8";
  y = ldexp(0.8, 4); z = 12.8;
  REQUIRE(FEQ(y, z));

  buffer = "ldexp (-0.854375, 5) == -27.34";
  y = ldexp(-0.854375, 5); z = -27.34;
  REQUIRE(FEQ(y, z));

  buffer = "ldexp (1.0, 0) == 1.0";
  y = ldexp(1.0, 0); z = 1.0;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_lgamma(void) {
  start("tests for lgamma()");
  const char *buffer = "";
  double x, y, z;

  buffer = "lgamma (inf) == inf";
  y = lgamma(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (0) == inf plus division by zero exception";
  y = lgamma(0); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (NaN) == NaN";
  y = lgamma(NaN);
  REQUIRE(!(y == y));

  buffer = "lgamma (-3) == inf plus division by zero exception";
  y = lgamma(-3); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (-inf) == inf";
  y = lgamma(-inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (1) == 0";
  y = lgamma(1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (3) == M_LN2l";
  y = lgamma(3); z = M_LN2;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (0.5) == log(sqrt(pi))";
  y = lgamma(0.5); z = M_LOG_SQRT_PI;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (-0.5) == log(2*sqrt(pi))";
  y = lgamma(-0.5); z = M_LOG_2_SQRT_PI;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (0.7) == 0.26086724653166651439";
  y = lgamma(0.7); z = 0.26086724653166651439;
  REQUIRE(FEQ(y, z));

  buffer = "lgamma (1.2) == -0.853740900033158497197e-1";
  y = lgamma(1.2); z = -0.853740900033158497197e-1;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_log(void) {
  start("tests for log()");
  const char *buffer = "";
  double x, y, z;

  buffer = "log (0) == -inf plus division by zero exception";
  y = log(0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "log (-0.0) == -inf plus division by zero exception";
  y = log(-0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "log (1) == 0";
  y = log(1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "log (-1) == NaN plus invalid exception";
  y = log(-1);
  REQUIRE(!(y == y));

  buffer = "log (inf) == inf";
  y = log(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "log (e) == 1";
  y = log(M_E); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "log (1.0 / M_El) == -1";
  y = log(1.0 / M_E); z = -1;
  REQUIRE(FEQ(y, z));

  buffer = "log (2) == M_LN2l";
  y = log(2); z = M_LN2;
  REQUIRE(FEQ(y, z));

  buffer = "log (10) == M_LN10l";
  y = log(10); z = M_LN10;
  REQUIRE(FEQ(y, z));

  buffer = "log (0.7) == -0.35667494393873237891263871124118447";
  y = log(0.7); z = -0.35667494393873237891263871124118447;
  REQUIRE(FEQ(y, z));
}

void test_math_log10(void) {
  start("tests for log10()");
  const char *buffer = "";
  double x, y, z;

  buffer = "log10 (0) == -inf plus division by zero exception";
  y = log10(0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (-0.0) == -inf plus division by zero exception";
  y = log10(-0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (1) == 0";
  y = log10(1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (-1) == NaN plus invalid exception";
  y = log10(-1);
  REQUIRE(!(y == y));

  buffer = "log10 (inf) == inf";
  y = log10(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (NaN) == NaN";
  y = log10(NaN);
  REQUIRE(!(y == y));

  buffer = "log10 (0.1) == -1";
  y = log10(0.1); z = -1;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (10.0) == 1";
  y = log10(10.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (100.0) == 2";
  y = log10(100.0); z = 2;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (10000.0) == 4";
  y = log10(10000.0); z = 4;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (e) == log10(e)";
  y = log10(M_E); z = M_LOG10E;
  REQUIRE(FEQ(y, z));

  buffer = "log10 (0.7) == -0.15490195998574316929";
  y = log10(0.7); z = -0.15490195998574316929;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_log1p(void) {
  start("tests for log1p()");
  const char *buffer = "";
  double x, y, z;

  buffer = "log1p (0) == 0";
  y = log1p(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "log1p (-0.0) == -0.0";
  y = log1p(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "log1p (-1) == -inf plus division by zero exception";
  y = log1p(-1); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "log1p (-2) == NaN plus invalid exception";
  y = log1p(-2);
  REQUIRE(!(y == y));

  buffer = "log1p (inf) == inf";
  y = log1p(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "log1p (NaN) == NaN";
  y = log1p(NaN);
  REQUIRE(!(y == y));

  buffer = "log1p (M_El - 1.0) == 1";
  y = log1p(M_E - 1.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "log1p (-0.3) == -0.35667494393873237891263871124118447";
  y = log1p(-0.3); z = -0.35667494393873237891263871124118447;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_log2(void) {
  start("tests for log2()");
  const char *buffer = "";
  double x, y, z;

  buffer = "log2 (0) == -inf plus division by zero exception";
  y = log2(0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "log2 (-0.0) == -inf plus division by zero exception";
  y = log2(-0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "log2 (1) == 0";
  y = log2(1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "log2 (-1) == NaN plus invalid exception";
  y = log2(-1);
  REQUIRE(!(y == y));

  buffer = "log2 (inf) == inf";
  y = log2(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "log2 (NaN) == NaN";
  y = log2(NaN);
  REQUIRE(!(y == y));

  buffer = "log2 (e) == M_LOG2El";
  y = log2(M_E); z = M_LOG2E;
  REQUIRE(FEQ(y, z));

  buffer = "log2 (2.0) == 1";
  y = log2(2.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "log2 (16.0) == 4";
  y = log2(16.0); z = 4;
  REQUIRE(FEQ(y, z));

  buffer = "log2 (256.0) == 8";
  y = log2(256.0); z = 8;
  REQUIRE(FEQ(y, z));

  buffer = "log2 (0.7) == -0.51457317282975824043";
  y = log2(0.7); z = -0.51457317282975824043;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_logb(void) {
  start("tests for logb()");
  const char *buffer = "";
  double x, y, z;

  buffer = "logb (inf) == inf";
  y = logb(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "logb (-inf) == inf";
  y = logb(-inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "logb (0) == -inf plus division by zero exception";
  y = logb(0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "logb (-0.0) == -inf plus division by zero exception";
  y = logb(-0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "logb (NaN) == NaN";
  y = logb(NaN);
  REQUIRE(!(y == y));

  buffer = "logb (1) == 0";
  y = logb(1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "logb (e) == 1";
  y = logb(M_E); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "logb (1024) == 10";
  y = logb(1024); z = 10;
  REQUIRE(FEQ(y, z));

  buffer = "logb (-2000) == 10";
  y = logb(-2000); z = 10;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_modf(void) {
  start("tests for modf()");
  const char *buffer = "";
  double x, y, z;

  buffer = "modf (inf, &x) == 0";
  y = modf(inf, &x); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "modf (inf, &x) sets x to plus_infty";
  modf(inf, &x); z = inf;
  REQUIRE(FEQ(x, z));

  buffer = "modf (-inf, &x) == -0.0";
  y = modf(-inf, &x); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "modf (-inf, &x) sets x to minus_infty";
  modf(-inf, &x); z = -inf;
  REQUIRE(FEQ(x, z));

  buffer = "modf (NaN, &x) == NaN";
  y = modf(NaN, &x);
  REQUIRE(!(y == y));

  buffer = "modf (NaN, &x) sets x to nan_value";
  modf(NaN, &x);
  REQUIRE(!(x == x));

  buffer = "modf (0, &x) == 0";
  y = modf(0, &x); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "modf (0, &x) sets x to 0";
  modf(0, &x); z = 0;
  REQUIRE(FEQ(x, z));

  buffer = "modf (1.5, &x) == 0.5";
  y = modf(1.5, &x); z = 0.5;
  REQUIRE(FEQ(y, z));

  buffer = "modf (1.5, &x) sets x to 1";
  modf(1.5, &x); z = 1;
  REQUIRE(FEQ(x, z));

  buffer = "modf (2.5, &x) == 0.5";
  y = modf(2.5, &x); z = 0.5;
  REQUIRE(FEQ(y, z));

  buffer = "modf (2.5, &x) sets x to 2";
  modf(2.5, &x); z = 2;
  REQUIRE(FEQ(x, z));

  buffer = "modf (-2.5, &x) == -0.5";
  y = modf(-2.5, &x); z = -0.5;
  REQUIRE(FEQ(y, z));

  buffer = "modf (-2.5, &x) sets x to -2";
  modf(-2.5, &x); z = -2;
  REQUIRE(FEQ(x, z));

  buffer = "modf (20, &x) == 0";
  y = modf(20, &x); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "modf (20, &x) sets x to 20";
  modf(20, &x); z = 20;
  REQUIRE(FEQ(x, z));

  buffer = "modf (21, &x) == 0";
  y = modf(21, &x); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "modf (21, &x) sets x to 21";
  modf(21, &x); z = 21;
  REQUIRE(FEQ(x, z));

  buffer = "modf (89.5, &x) == 0.5";
  y = modf(89.5, &x); z = 0.5;
  REQUIRE(FEQ(y, z));

  buffer = "modf (89.5, &x) sets x to 89";
  modf(89.5, &x); z = 89;
  REQUIRE(FEQ(x, z));
}

/*
void test_math_nearbyint(void) {
  start("tests for nearbyint()");
  const char *buffer = "";
  double x, y, z;

  buffer = "nearbyint (0.0) == 0.0";
  y = nearbyint(0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "nearbyint (-0.0) == -0.0";
  y = nearbyint(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "nearbyint (inf) == inf";
  y = nearbyint(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "nearbyint (-inf) == -inf";
  y = nearbyint(-inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "nearbyint (NaN) == NaN";
  y = nearbyint(NaN);
  REQUIRE(!(y == y));

  buffer = "nearbyint (0.5) == 0.0";
  y = nearbyint(0.5); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "nearbyint (1.5) == 2.0";
  y = nearbyint(1.5); z = 2.0;
  REQUIRE(FEQ(y, z));

  buffer = "nearbyint (-0.5) == -0";
  y = nearbyint(-0.5); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "nearbyint (-1.5) == -2.0";
  y = nearbyint(-1.5); z = -2.0;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_nextafter(void) {
  start("tests for nextafter()");
  const char *buffer = "";
  double x, y, z;

  buffer = "nextafter (0, 0) == 0";
  y = nextafter(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "nextafter (-0.0, 0) == 0";
  y = nextafter(-0.0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "nextafter (0, -0.0) == -0.0";
  y = nextafter(0, -0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "nextafter (-0.0, -0.0) == -0.0";
  y = nextafter(-0.0, -0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "nextafter (9, 9) == 9";
  y = nextafter(9, 9); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "nextafter (-9, -9) == -9";
  y = nextafter(-9, -9); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "nextafter (inf, inf) == inf";
  y = nextafter(inf, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "nextafter (-inf, -inf) == -inf";
  y = nextafter(-inf, -inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "nextafter (NaN, 1.1) == NaN";
  y = nextafter(NaN, 1.1);
  REQUIRE(!(y == y));

  buffer = "nextafter (1.1, NaN) == NaN";
  y = nextafter(1.1, NaN);
  REQUIRE(!(y == y));

  buffer = "nextafter (NaN, NaN) == NaN";
  y = nextafter(NaN, NaN);
  REQUIRE(!(y == y));
}
*/

/*
void test_math_nexttoward(void) {
  start("tests for nexttoward()");
  const char *buffer = "";
  double x, y, z;

  buffer = "nexttoward (0, 0) == 0";
  y = nexttoward(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "nexttoward (-0.0, 0) == 0";
  y = nexttoward(-0.0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "nexttoward (0, -0.0) == -0.0";
  y = nexttoward(0, -0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "nexttoward (-0.0, -0.0) == -0.0";
  y = nexttoward(-0.0, -0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "nexttoward (9, 9) == 9";
  y = nexttoward(9, 9); z = 9;
  REQUIRE(FEQ(y, z));

  buffer = "nexttoward (-9, -9) == -9";
  y = nexttoward(-9, -9); z = -9;
  REQUIRE(FEQ(y, z));

  buffer = "nexttoward (inf, inf) == inf";
  y = nexttoward(inf, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "nexttoward (-inf, -inf) == -inf";
  y = nexttoward(-inf, -inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "nexttoward (NaN, 1.1) == NaN";
  y = nexttoward(NaN, 1.1);
  REQUIRE(!(y == y));

  buffer = "nexttoward (1.1, NaN) == NaN";
  y = nexttoward(1.1, NaN);
  REQUIRE(!(y == y));

  buffer = "nexttoward (NaN, NaN) == NaN";
  y = nexttoward(NaN, NaN);
  REQUIRE(!(y == y));
}
*/

void test_math_pow(void) {
  start("tests for pow()");
  const char *buffer = "";
  double x, y, z;

  buffer = "pow (0, 0) == 1";
  y = pow(0, 0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0, -0.0) == 1";
  y = pow(0, -0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, 0) == 1";
  y = pow(-0.0, 0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, -0.0) == 1";
  y = pow(-0.0, -0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (10, 0) == 1";
  y = pow(10, 0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (10, -0.0) == 1";
  y = pow(10, -0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-10, 0) == 1";
  y = pow(-10, 0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-10, -0.0) == 1";
  y = pow(-10, -0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (NaN, 0) == 1";
  y = pow(NaN, 0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (NaN, -0.0) == 1";
  y = pow(NaN, -0.0); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (1.1, inf) == inf";
  y = pow(1.1, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, inf) == inf";
  y = pow(inf, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-1.1, inf) == inf";
  y = pow(-1.1, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, inf) == inf";
  y = pow(-inf, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0.9, inf) == 0";
  y = pow(0.9, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (1e-7, inf) == 0";
  y = pow(1e-7, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.9, inf) == 0";
  y = pow(-0.9, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-1e-7, inf) == 0";
  y = pow(-1e-7, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (1.1, -inf) == 0";
  y = pow(1.1, -inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, -inf) == 0";
  y = pow(inf, -inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-1.1, -inf) == 0";
  y = pow(-1.1, -inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -inf) == 0";
  y = pow(-inf, -inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0.9, -inf) == inf";
  y = pow(0.9, -inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (1e-7, -inf) == inf";
  y = pow(1e-7, -inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.9, -inf) == inf";
  y = pow(-0.9, -inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-1e-7, -inf) == inf";
  y = pow(-1e-7, -inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, 1e-7) == inf";
  y = pow(inf, 1e-7); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, 1) == inf";
  y = pow(inf, 1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, 1e7) == inf";
  y = pow(inf, 1e7); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, -1e-7) == 0";
  y = pow(inf, -1e-7); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, -1) == 0";
  y = pow(inf, -1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, -1e7) == 0";
  y = pow(inf, -1e7); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 1) == -inf";
  y = pow(-inf, 1); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 11) == -inf";
  y = pow(-inf, 11); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 1001) == -inf";
  y = pow(-inf, 1001); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 2) == inf";
  y = pow(-inf, 2); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 12) == inf";
  y = pow(-inf, 12); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 1002) == inf";
  y = pow(-inf, 1002); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 0.1) == inf";
  y = pow(-inf, 0.1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 1.1) == inf";
  y = pow(-inf, 1.1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 11.1) == inf";
  y = pow(-inf, 11.1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 1001.1) == inf";
  y = pow(-inf, 1001.1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -1) == -0.0";
  y = pow(-inf, -1); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -11) == -0.0";
  y = pow(-inf, -11); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -1001) == -0.0";
  y = pow(-inf, -1001); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -2) == 0";
  y = pow(-inf, -2); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -12) == 0";
  y = pow(-inf, -12); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -1002) == 0";
  y = pow(-inf, -1002); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -0.1) == 0";
  y = pow(-inf, -0.1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -1.1) == 0";
  y = pow(-inf, -1.1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -11.1) == 0";
  y = pow(-inf, -11.1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -1001.1) == 0";
  y = pow(-inf, -1001.1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (NaN, NaN) == NaN";
  y = pow(NaN, NaN);
  REQUIRE(!(y == y));

  buffer = "pow (0, NaN) == NaN";
  y = pow(0, NaN);
  REQUIRE(!(y == y));

  buffer = "pow (1, NaN) == 1";
  y = pow(1, NaN); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-1, NaN) == NaN";
  y = pow(-1, NaN);
  REQUIRE(!(y == y));

  buffer = "pow (NaN, 1) == NaN";
  y = pow(NaN, 1);
  REQUIRE(!(y == y));

  buffer = "pow (NaN, -1) == NaN";
  y = pow(NaN, -1);
  REQUIRE(!(y == y));

  buffer = "pow (3.0, NaN) == NaN";
  y = pow(3.0, NaN);
  REQUIRE(!(y == y));

  buffer = "pow (1, inf) == 1";
  y = pow(1, inf); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-1, inf) == 1";
  y = pow(-1, inf); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (1, -inf) == 1";
  y = pow(1, -inf); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-1, -inf) == 1";
  y = pow(-1, -inf); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.1, 1.1) == NaN plus invalid exception";
  y = pow(-0.1, 1.1);
  REQUIRE(!(y == y));

  buffer = "pow (-0.1, -1.1) == NaN plus invalid exception";
  y = pow(-0.1, -1.1);
  REQUIRE(!(y == y));

  buffer = "pow (-10.1, 1.1) == NaN plus invalid exception";
  y = pow(-10.1, 1.1);
  REQUIRE(!(y == y));

  buffer = "pow (-10.1, -1.1) == NaN plus invalid exception";
  y = pow(-10.1, -1.1);
  REQUIRE(!(y == y));

  buffer = "pow (0, -1) == inf plus division by zero exception";
  y = pow(0, -1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0, -11) == inf plus division by zero exception";
  y = pow(0, -11); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, -1) == -inf plus division by zero exception";
  y = pow(-0.0, -1); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, -11) == -inf plus division by zero exception";
  y = pow(-0.0, -11); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0, -2) == inf plus division by zero exception";
  y = pow(0, -2); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0, -11.1) == inf plus division by zero exception";
  y = pow(0, -11.1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, -2) == inf plus division by zero exception";
  y = pow(-0.0, -2); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, -11.1) == inf plus division by zero exception";
  y = pow(-0.0, -11.1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0, 1) == 0";
  y = pow(0, 1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0, 11) == 0";
  y = pow(0, 11); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, 1) == -0.0";
  y = pow(-0.0, 1); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, 11) == -0.0";
  y = pow(-0.0, 11); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0, 2) == 0";
  y = pow(0, 2); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0, 11.1) == 0";
  y = pow(0, 11.1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, 2) == 0";
  y = pow(-0.0, 2); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, 11.1) == 0";
  y = pow(-0.0, 11.1); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (1.5, inf) == inf";
  y = pow(1.5, inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0.5, inf) == 0.0";
  y = pow(0.5, inf); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (1.5, -inf) == 0.0";
  y = pow(1.5, -inf); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0.5, -inf) == inf";
  y = pow(0.5, -inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, 2) == inf";
  y = pow(inf, 2); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (inf, -1) == 0.0";
  y = pow(inf, -1); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 27) == -inf";
  y = pow(-inf, 27); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, 28) == inf";
  y = pow(-inf, 28); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -3) == -0.0";
  y = pow(-inf, -3); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-inf, -2.0) == 0.0";
  y = pow(-inf, -2.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0.0, 27) == 0.0";
  y = pow(0.0, 27); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, 27) == -0.0";
  y = pow(-0.0, 27); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0.0, 4) == 0.0";
  y = pow(0.0, 4); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-0.0, 4) == 0.0";
  y = pow(-0.0, 4); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "pow (0.7, 1.2) == 0.65180494056638638188";
  y = pow(0.7, 1.2); z = 0.65180494056638638188;
  REQUIRE(FEQ(y, z));

  buffer = "pow (-7.49321e+133, -9.80818e+16) == 0";
  y = pow(-7.49321e+133, -9.80818e+16); z = 0;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_remainder(void) {
  start("tests for remainder()");
  const char *buffer = "";
  double x, y, z;

  buffer = "remainder (1, 0) == NaN plus invalid exception";
  y = remainder(1, 0);
  REQUIRE(!(y == y));

  buffer = "remainder (1, -0.0) == NaN plus invalid exception";
  y = remainder(1, -0.0);
  REQUIRE(!(y == y));

  buffer = "remainder (inf, 1) == NaN plus invalid exception";
  y = remainder(inf, 1);
  REQUIRE(!(y == y));

  buffer = "remainder (-inf, 1) == NaN plus invalid exception";
  y = remainder(-inf, 1);
  REQUIRE(!(y == y));

  buffer = "remainder (NaN, NaN) == NaN";
  y = remainder(NaN, NaN);
  REQUIRE(!(y == y));

  buffer = "remainder (1.625, 1.0) == -0.375";
  y = remainder(1.625, 1.0); z = -0.375;
  REQUIRE(FEQ(y, z));

  buffer = "remainder (-1.625, 1.0) == 0.375";
  y = remainder(-1.625, 1.0); z = 0.375;
  REQUIRE(FEQ(y, z));

  buffer = "remainder (1.625, -1.0) == -0.375";
  y = remainder(1.625, -1.0); z = -0.375;
  REQUIRE(FEQ(y, z));

  buffer = "remainder (-1.625, -1.0) == 0.375";
  y = remainder(-1.625, -1.0); z = 0.375;
  REQUIRE(FEQ(y, z));

  buffer = "remainder (5.0, 2.0) == 1.0";
  y = remainder(5.0, 2.0); z = 1.0;
  REQUIRE(FEQ(y, z));

  buffer = "remainder (3.0, 2.0) == -1.0";
  y = remainder(3.0, 2.0); z = -1.0;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_remquo(void) {
  start("tests for remquo()");
  const char *buffer = "";
  int x; double y, z;

  buffer = "remquo (1, 0, &x) == NaN plus invalid exception";
  y = remquo(1, 0, &x);
  REQUIRE(!(y == y));

  buffer = "remquo (1, -0.0, &x) == NaN plus invalid exception";
  y = remquo(1, -0.0, &x);
  REQUIRE(!(y == y));

  buffer = "remquo (inf, 1, &x) == NaN plus invalid exception";
  y = remquo(inf, 1, &x);
  REQUIRE(!(y == y));

  buffer = "remquo (-inf, 1, &x) == NaN plus invalid exception";
  y = remquo(-inf, 1, &x);
  REQUIRE(!(y == y));

  buffer = "remquo (NaN, NaN, &x) == NaN";
  y = remquo(NaN, NaN, &x);
  REQUIRE(!(y == y));

  buffer = "remquo (1.625, 1.0, &x) == -0.375";
  y = remquo(1.625, 1.0, &x); z = -0.375;
  REQUIRE(FEQ(y, z));

  buffer = "remquo (-1.625, 1.0, &x) == 0.375";
  y = remquo(-1.625, 1.0, &x); z = 0.375;
  REQUIRE(FEQ(y, z));

  buffer = "remquo (1.625, -1.0, &x) == -0.375";
  y = remquo(1.625, -1.0, &x); z = -0.375;
  REQUIRE(FEQ(y, z));

  buffer = "remquo (-1.625, -1.0, &x) == 0.375";
  y = remquo(-1.625, -1.0, &x); z = 0.375;
  REQUIRE(FEQ(y, z));

  buffer = "remquo (5, 2, &x) == 1";
  y = remquo(5, 2, &x); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "remquo (3, 2, &x) == -1";
  y = remquo(3, 2, &x); z = -1;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_rint(void) {
  start("tests for rint()");
  const char *buffer = "";
  double x, y, z;

  buffer = "rint (0.0) == 0.0";
  y = rint(0.0); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (-0.0) == -0.0";
  y = rint(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (inf) == inf";
  y = rint(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "rint (-inf) == -inf";
  y = rint(-inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "rint (0.5) == 0.0";
  y = rint(0.5); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (1.5) == 2.0";
  y = rint(1.5); z = 2.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (2.5) == 2.0";
  y = rint(2.5); z = 2.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (3.5) == 4.0";
  y = rint(3.5); z = 4.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (4.5) == 4.0";
  y = rint(4.5); z = 4.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (-0.5) == -0.0";
  y = rint(-0.5); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (-1.5) == -2.0";
  y = rint(-1.5); z = -2.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (-2.5) == -2.0";
  y = rint(-2.5); z = -2.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (-3.5) == -4.0";
  y = rint(-3.5); z = -4.0;
  REQUIRE(FEQ(y, z));

  buffer = "rint (-4.5) == -4.0";
  y = rint(-4.5); z = -4.0;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_round(void) {
  start("tests for round()");
  const char *buffer = "";
  double x, y, z;

  buffer = "round (0) == 0";
  y = round(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "round (-0.0) == -0.0";
  y = round(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (0.2) == 0.0";
  y = round(0.2); z = 0.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (-0.2) == -0";
  y = round(-0.2); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (0.5) == 1.0";
  y = round(0.5); z = 1.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (-0.5) == -1.0";
  y = round(-0.5); z = -1.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (0.8) == 1.0";
  y = round(0.8); z = 1.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (-0.8) == -1.0";
  y = round(-0.8); z = -1.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (1.5) == 2.0";
  y = round(1.5); z = 2.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (-1.5) == -2.0";
  y = round(-1.5); z = -2.0;
  REQUIRE(FEQ(y, z));

  buffer = "round (2097152.5) == 2097153";
  y = round(2097152.5); z = 2097153;
  REQUIRE(FEQ(y, z));

  buffer = "round (-2097152.5) == -2097153";
  y = round(-2097152.5); z = -2097153;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_scalbn(void) {
  start("tests for scalbn()");
  const char *buffer = "";
  double x, y, z;

  buffer = "scalbn (0, 0) == 0";
  y = scalbn(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "scalbn (-0.0, 0) == -0.0";
  y = scalbn(-0.0, 0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "scalbn (inf, 1) == inf";
  y = scalbn(inf, 1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "scalbn (-inf, 1) == -inf";
  y = scalbn(-inf, 1); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "scalbn (NaN, 1) == NaN";
  y = scalbn(NaN, 1);
  REQUIRE(!(y == y));

  buffer = "scalbn (0.8, 4) == 12.8";
  y = scalbn(0.8, 4); z = 12.8;
  REQUIRE(FEQ(y, z));

  buffer = "scalbn (-0.854375, 5) == -27.34";
  y = scalbn(-0.854375, 5); z = -27.34;
  REQUIRE(FEQ(y, z));

  buffer = "scalbn (1, 0) == 1";
  y = scalbn(1, 0); z = 1;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_scalbln(void) {
  start("tests for scalbln()");
  const char *buffer = "";
  double x, y, z;

  buffer = "scalbln (0, 0) == 0";
  y = scalbln(0, 0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "scalbln (-0.0, 0) == -0.0";
  y = scalbln(-0.0, 0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "scalbln (inf, 1) == inf";
  y = scalbln(inf, 1); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "scalbln (-inf, 1) == -inf";
  y = scalbln(-inf, 1); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "scalbln (NaN, 1) == NaN";
  y = scalbln(NaN, 1);
  REQUIRE(!(y == y));

  buffer = "scalbln (0.8, 4) == 12.8";
  y = scalbln(0.8, 4); z = 12.8;
  REQUIRE(FEQ(y, z));

  buffer = "scalbln (-0.854375, 5) == -27.34";
  y = scalbln(-0.854375, 5); z = -27.34;
  REQUIRE(FEQ(y, z));

  buffer = "scalbln (1, 0) == 1";
  y = scalbln(1, 0); z = 1;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_sin(void) {
  start("tests for sin()");
  const char *buffer = "";
  double x, y, z;

  buffer = "sin (0) == 0";
  y = sin(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "sin (-0.0) == -0.0";
  y = sin(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "sin (inf) == NaN plus invalid exception";
  y = sin(inf);
  REQUIRE(!(y == y));

  buffer = "sin (-inf) == NaN plus invalid exception";
  y = sin(-inf);
  REQUIRE(!(y == y));

  buffer = "sin (NaN) == NaN";
  y = sin(NaN);
  REQUIRE(!(y == y));

  buffer = "sin (pi/6) == 0.5";
  y = sin(M_PI_6); z = 0.5;
  REQUIRE(FEQ(y, z));

  buffer = "sin (-pi/6) == -0.5";
  y = sin(-M_PI_6); z = -0.5;
  REQUIRE(FEQ(y, z));

  buffer = "sin (pi/2) == 1";
  y = sin(M_PI_2); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "sin (-pi/2) == -1";
  y = sin(-M_PI_2); z = -1;
  REQUIRE(FEQ(y, z));

  buffer = "sin (0.7) == 0.64421768723769105367261435139872014";
  y = sin(0.7); z = 0.64421768723769105367261435139872014;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_sinh(void) {
  start("tests for sinh()");
  const char *buffer = "";
  double x, y, z;

  buffer = "sinh (0) == 0";
  y = sinh(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "sinh (-0.0) == -0.0";
  y = sinh(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "sinh (inf) == inf";
  y = sinh(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "sinh (-inf) == -inf";
  y = sinh(-inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "sinh (NaN) == NaN";
  y = sinh(NaN);
  REQUIRE(!(y == y));

  buffer = "sinh (0.7) == 0.75858370183953350346";
  y = sinh(0.7); z = 0.75858370183953350346;
  REQUIRE(FEQ(y, z));

  buffer = "sinh (0x8p-32) == 1.86264514923095703232705808926175479e-9";
  y = sinh(0x8p-32); z = 1.86264514923095703232705808926175479e-9;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_sqrt(void) {
  start("tests for sqrt()");
  const char *buffer = "";
  double x, y, z;

  buffer = "sqrt (0) == 0";
  y = sqrt(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (NaN) == NaN";
  y = sqrt(NaN);
  REQUIRE(!(y == y));

  buffer = "sqrt (inf) == inf";
  y = sqrt(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (-0.0) == -0.0";
  y = sqrt(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (-1) == NaN plus invalid exception";
  y = sqrt(-1);
  REQUIRE(!(y == y));

  buffer = "sqrt (-inf) == NaN plus invalid exception";
  y = sqrt(-inf);
  REQUIRE(!(y == y));

  buffer = "sqrt (NaN) == NaN";
  y = sqrt(NaN);
  REQUIRE(!(y == y));

  buffer = "sqrt (2209) == 47";
  y = sqrt(2209); z = 47;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (4) == 2";
  y = sqrt(4); z = 2;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (2) == M_SQRT2l";
  y = sqrt(2); z = M_SQRT2;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (0.25) == 0.5";
  y = sqrt(0.25); z = 0.5;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (6642.25) == 81.5";
  y = sqrt(6642.25); z = 81.5;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (15239.9025) == 123.45";
  y = sqrt(15239.9025); z = 123.45;
  REQUIRE(FEQ(y, z));

  buffer = "sqrt (0.7) == 0.83666002653407554797817202578518747";
  y = sqrt(0.7); z = 0.83666002653407554797817202578518747;
  REQUIRE(FEQ(y, z));
}

void test_math_tan(void) {
  start("tests for tan()");
  const char *buffer = "";
  double x, y, z;

  buffer = "tan (0) == 0";
  y = tan(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "tan (-0.0) == -0.0";
  y = tan(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "tan (inf) == NaN plus invalid exception";
  y = tan(inf);
  REQUIRE(!(y == y));

  buffer = "tan (-inf) == NaN plus invalid exception";
  y = tan(-inf);
  REQUIRE(!(y == y));

  buffer = "tan (NaN) == NaN";
  y = tan(NaN);
  REQUIRE(!(y == y));

  buffer = "tan (pi/4) == 1";
  y = tan(M_PI_4); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "tan (0.7) == 0.84228838046307944812813500221293775";
  y = tan(0.7); z = 0.84228838046307944812813500221293775;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_tanh(void) {
  start("tests for tanh()");
  const char *buffer = "";
  double x, y, z;

  buffer = "tanh (0) == 0";
  y = tanh(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "tanh (-0.0) == -0.0";
  y = tanh(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "tanh (inf) == 1";
  y = tanh(inf); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "tanh (-inf) == -1";
  y = tanh(-inf); z = -1;
  REQUIRE(FEQ(y, z));

  buffer = "tanh (NaN) == NaN";
  y = tanh(NaN);
  REQUIRE(!(y == y));

  buffer = "tanh (0.7) == 0.60436777711716349631";
  y = tanh(0.7); z = 0.60436777711716349631;
  REQUIRE(FEQ(y, z));

  buffer = "tanh (-0.7) == -0.60436777711716349631";
  y = tanh(-0.7); z = -0.60436777711716349631;
  REQUIRE(FEQ(y, z));

  buffer = "tanh (1.0) == 0.7615941559557648881194582826047935904";
  y = tanh(1.0); z = 0.7615941559557648881194582826047935904;
  REQUIRE(FEQ(y, z));

  buffer = "tanh (-1.0) == -0.7615941559557648881194582826047935904";
  y = tanh(-1.0); z = -0.7615941559557648881194582826047935904;
  REQUIRE(FEQ(y, z));

  buffer = "tanh (6.938893903907228377647697925567626953125e-18) == 6.938893903907228377647697925567626953125e-18";
  y = tanh(6.938893903907228377647697925567626953125e-18); z = 6.938893903907228377647697925567626953125e-18;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_tgamma(void) {
  start("tests for tgamma()");
  const char *buffer = "";
  double x, y, z;

  buffer = "tgamma (inf) == inf";
  y = tgamma(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "tgamma (0) == inf plus divide-by-zero";
  y = tgamma(0); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "tgamma (-0.0) == inf plus divide-by-zero";
  y = tgamma(-0.0); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "tgamma (-2) == NaN plus invalid exception";
  y = tgamma(-2);
  REQUIRE(!(y == y));

  buffer = "tgamma (-inf) == NaN plus invalid exception";
  y = tgamma(-inf);
  REQUIRE(!(y == y));

  buffer = "tgamma (NaN) == NaN";
  y = tgamma(NaN);
  REQUIRE(!(y == y));

  buffer = "tgamma (0.5) == sqrt (pi)";
  y = tgamma(0.5); z = M_SQRT_PI;
  REQUIRE(FEQ(y, z));

  buffer = "tgamma (-0.5) == -2 sqrt (pi)";
  y = tgamma(-0.5); z = -M_2_SQRT_PI;
  REQUIRE(FEQ(y, z));

  buffer = "tgamma (1) == 1";
  y = tgamma(1); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "tgamma (4) == 6";
  y = tgamma(4); z = 6;
  REQUIRE(FEQ(y, z));

  buffer = "tgamma (0.7) == 1.29805533264755778568";
  y = tgamma(0.7); z = 1.29805533264755778568;
  REQUIRE(FEQ(y, z));

  buffer = "tgamma (1.2) == 0.91816874239976061064";
  y = tgamma(1.2); z = 0.91816874239976061064;
  REQUIRE(FEQ(y, z));
}
*/

void test_math_trunc(void) {
  start("tests for trunc()");
  const char *buffer = "";
  double x, y, z;

  buffer = "trunc (inf) == inf";
  y = trunc(inf); z = inf;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (-inf) == -inf";
  y = trunc(-inf); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (NaN) == NaN";
  y = trunc(NaN);
  REQUIRE(!(y == y));

  buffer = "trunc (0) == 0";
  y = trunc(0); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (-0.0) == -0.0";
  y = trunc(-0.0); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (0.625) == 0";
  y = trunc(0.625); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (-0.625) == -0.0";
  y = trunc(-0.625); z = -0.0;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (1) == 1";
  y = trunc(1); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (-1) == -1";
  y = trunc(-1); z = -1;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (1.625) == 1";
  y = trunc(1.625); z = 1;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (-1.625) == -1";
  y = trunc(-1.625); z = -1;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (1048580.625) == 1048580";
  y = trunc(1048580.625); z = 1048580;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (-1048580.625) == -1048580";
  y = trunc(-1048580.625); z = -1048580;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (8388610.125) == 8388610.0";
  y = trunc(8388610.125); z = 8388610.0;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (-8388610.125) == -8388610.0";
  y = trunc(-8388610.125); z = -8388610.0;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (4294967296.625) == 4294967296.0";
  y = trunc(4294967296.625); z = 4294967296.0;
  REQUIRE(FEQ(y, z));

  buffer = "trunc (-4294967296.625) == -4294967296.0";
  y = trunc(-4294967296.625); z = -4294967296.0;
  REQUIRE(FEQ(y, z));
}

/*
void test_math_y0(void) {
  start("tests for y0()");
  const char *buffer = "";
  double x, y, z;

  buffer = "y0 (-1.0) == NaN";
  y = y0(-1.0);
  REQUIRE(!(y == y));

  buffer = "y0 (0.0) == -inf";
  y = y0(0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "y0 (NaN) == NaN";
  y = y0(NaN);
  REQUIRE(!(y == y));

  buffer = "y0 (inf) == 0";
  y = y0(inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "y0 (0.1) == -1.5342386513503668441";
  y = y0(0.1); z = -1.5342386513503668441;
  REQUIRE(FEQ(y, z));

  buffer = "y0 (0.7) == -0.19066492933739506743";
  y = y0(0.7); z = -0.19066492933739506743;
  REQUIRE(FEQ(y, z));

  buffer = "y0 (1.0) == 0.088256964215676957983";
  y = y0(1.0); z = 0.088256964215676957983;
  REQUIRE(FEQ(y, z));

  buffer = "y0 (1.5) == 0.38244892379775884396";
  y = y0(1.5); z = 0.38244892379775884396;
  REQUIRE(FEQ(y, z));

  buffer = "y0 (2.0) == 0.51037567264974511960";
  y = y0(2.0); z = 0.51037567264974511960;
  REQUIRE(FEQ(y, z));

  buffer = "y0 (8.0) == 0.22352148938756622053";
  y = y0(8.0); z = 0.22352148938756622053;
  REQUIRE(FEQ(y, z));

  buffer = "y0 (10.0) == 0.055671167283599391424";
  y = y0(10.0); z = 0.055671167283599391424;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_y1(void) {
  start("tests for y1()");
  const char *buffer = "";
  double x, y, z;

  buffer = "y1 (-1.0) == NaN";
  y = y1(-1.0);
  REQUIRE(!(y == y));

  buffer = "y1 (0.0) == -inf";
  y = y1(0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "y1 (inf) == 0";
  y = y1(inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "y1 (NaN) == NaN";
  y = y1(NaN);
  REQUIRE(!(y == y));

  buffer = "y1 (0.1) == -6.4589510947020269877";
  y = y1(0.1); z = -6.4589510947020269877;
  REQUIRE(FEQ(y, z));

  buffer = "y1 (0.7) == -1.1032498719076333697";
  y = y1(0.7); z = -1.1032498719076333697;
  REQUIRE(FEQ(y, z));

  buffer = "y1 (1.0) == -0.78121282130028871655";
  y = y1(1.0); z = -0.78121282130028871655;
  REQUIRE(FEQ(y, z));

  buffer = "y1 (1.5) == -0.41230862697391129595";
  y = y1(1.5); z = -0.41230862697391129595;
  REQUIRE(FEQ(y, z));

  buffer = "y1 (2.0) == -0.10703243154093754689";
  y = y1(2.0); z = -0.10703243154093754689;
  REQUIRE(FEQ(y, z));

  buffer = "y1 (8.0) == -0.15806046173124749426";
  y = y1(8.0); z = -0.15806046173124749426;
  REQUIRE(FEQ(y, z));

  buffer = "y1 (10.0) == 0.24901542420695388392";
  y = y1(10.0); z = 0.24901542420695388392;
  REQUIRE(FEQ(y, z));
}
*/

/*
void test_math_yn(void) {
  start("tests for yn()");
  const char *buffer = "";
  double x, y, z;

  buffer = "yn (0, -1.0) == NaN";
  y = yn(0, -1.0);
  REQUIRE(!(y == y));

  buffer = "yn (0, 0.0) == -inf";
  y = yn(0, 0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "yn (0, NaN) == NaN";
  y = yn(0, NaN);
  REQUIRE(!(y == y));

  buffer = "yn (0, inf) == 0";
  y = yn(0, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "yn (0, 0.1) == -1.5342386513503668441";
  y = yn(0, 0.1); z = -1.5342386513503668441;
  REQUIRE(FEQ(y, z));

  buffer = "yn (0, 0.7) == -0.19066492933739506743";
  y = yn(0, 0.7); z = -0.19066492933739506743;
  REQUIRE(FEQ(y, z));

  buffer = "yn (0, 1.0) == 0.088256964215676957983";
  y = yn(0, 1.0); z = 0.088256964215676957983;
  REQUIRE(FEQ(y, z));

  buffer = "yn (0, 1.5) == 0.38244892379775884396";
  y = yn(0, 1.5); z = 0.38244892379775884396;
  REQUIRE(FEQ(y, z));

  buffer = "yn (0, 2.0) == 0.51037567264974511960";
  y = yn(0, 2.0); z = 0.51037567264974511960;
  REQUIRE(FEQ(y, z));

  buffer = "yn (0, 8.0) == 0.22352148938756622053";
  y = yn(0, 8.0); z = 0.22352148938756622053;
  REQUIRE(FEQ(y, z));

  buffer = "yn (0, 10.0) == 0.055671167283599391424";
  y = yn(0, 10.0); z = 0.055671167283599391424;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, -1.0) == NaN";
  y = yn(1, -1.0);
  REQUIRE(!(y == y));

  buffer = "yn (1, 0.0) == -inf";
  y = yn(1, 0.0); z = -inf;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, inf) == 0";
  y = yn(1, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, NaN) == NaN";
  y = yn(1, NaN);
  REQUIRE(!(y == y));

  buffer = "yn (1, 0.1) == -6.4589510947020269877";
  y = yn(1, 0.1); z = -6.4589510947020269877;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, 0.7) == -1.1032498719076333697";
  y = yn(1, 0.7); z = -1.1032498719076333697;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, 1.0) == -0.78121282130028871655";
  y = yn(1, 1.0); z = -0.78121282130028871655;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, 1.5) == -0.41230862697391129595";
  y = yn(1, 1.5); z = -0.41230862697391129595;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, 2.0) == -0.10703243154093754689";
  y = yn(1, 2.0); z = -0.10703243154093754689;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, 8.0) == -0.15806046173124749426";
  y = yn(1, 8.0); z = -0.15806046173124749426;
  REQUIRE(FEQ(y, z));

  buffer = "yn (1, 10.0) == 0.24901542420695388392";
  y = yn(1, 10.0); z = 0.24901542420695388392;
  REQUIRE(FEQ(y, z));

  buffer = "yn (3, inf) == 0";
  y = yn(3, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "yn (3, NaN) == NaN";
  y = yn(3, NaN);
  REQUIRE(!(y == y));

  buffer = "yn (3, 0.1) == -5099.3323786129048894";
  y = yn(3, 0.1); z = -5099.3323786129048894;
  REQUIRE(FEQ(y, z));

  buffer = "yn (3, 0.7) == -15.819479052819633505";
  y = yn(3, 0.7); z = -15.819479052819633505;
  REQUIRE(FEQ(y, z));

  buffer = "yn (3, 1.0) == -5.8215176059647288478";
  y = yn(3, 1.0); z = -5.8215176059647288478;
  REQUIRE(FEQ(y, z));

  buffer = "yn (3, 2.0) == -1.1277837768404277861";
  y = yn(3, 2.0); z = -1.1277837768404277861;
  REQUIRE(FEQ(y, z));

  buffer = "yn (3, 10.0) == -0.25136265718383732978";
  y = yn(3, 10.0); z = -0.25136265718383732978;
  REQUIRE(FEQ(y, z));

  buffer = "yn (10, inf) == 0";
  y = yn(10, inf); z = 0;
  REQUIRE(FEQ(y, z));

  buffer = "yn (10, NaN) == NaN";
  y = yn(10, NaN);
  REQUIRE(!(y == y));

  buffer = "yn (10, 0.1) == -0.11831335132045197885e19";
  y = yn(10, 0.1); z = -0.11831335132045197885e19;
  REQUIRE(FEQ(y, z));

  buffer = "yn (10, 0.7) == -0.42447194260703866924e10";
  y = yn(10, 0.7); z = -0.42447194260703866924e10;
  REQUIRE(FEQ(y, z));

  buffer = "yn (10, 1.0) == -0.12161801427868918929e9";
  y = yn(10, 1.0); z = -0.12161801427868918929e9;
  REQUIRE(FEQ(y, z));

  buffer = "yn (10, 2.0) == -129184.54220803928264";
  y = yn(10, 2.0); z = -129184.54220803928264;
  REQUIRE(FEQ(y, z));

  buffer = "yn (10, 10.0) == -0.35981415218340272205";
  y = yn(10, 10.0); z = -0.35981415218340272205;
  REQUIRE(FEQ(y, z));
}
*/

int main()
{
  test_math_acos();
  //test_math_acosh();
  test_math_asin();
  //test_math_asinh();
  test_math_atan();
  //test_math_atanh();
  test_math_atan2();
  //test_math_cbrt();
  test_math_ceil();
  test_math_copysign();
  test_math_cos();
  //test_math_cosh();
  //test_math_erf();
  //test_math_erfc();
  test_math_exp();
  //test_math_exp10();
  //test_math_exp2();
  //test_math_expm1();
  test_math_fabs();
  //test_math_fdim();
  test_math_floor();
  //test_math_fma();
  test_math_fmax();
  test_math_fmin();
  test_math_fmod();
  test_math_frexp();
  //test_math_gamma();
  //test_math_hypot();
  //test_math_j0();
  //test_math_j1();
  //test_math_jn();
  test_math_ldexp();
  //test_math_lgamma();
  test_math_log();
  test_math_log10();
  //test_math_log1p();
  //test_math_log2();
  //test_math_logb();
  test_math_modf();
  //test_math_nearbyint();
  //test_math_nextafter();
  //test_math_nexttoward();
  test_math_pow();
  //test_math_remainder();
  //test_math_remquo();
  //test_math_rint();
  test_math_round();
  //test_math_scalbn();
  //test_math_scalbln();
  test_math_sin();
  //test_math_sinh();
  test_math_sqrt();
  test_math_tan();
  //test_math_tanh();
  //test_math_tgamma();
  test_math_trunc();
  //test_math_y0();
  //test_math_y1();
  //test_math_yn();
  printf("\nFAILURES: %d\n", failcnt);
  fflush(stdout);
  return failcnt;
}


