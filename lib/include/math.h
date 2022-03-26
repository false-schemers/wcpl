/* Mathematics */

#pragma once

/* HUGE_VAL is built-in */
/* uint64_t asuint64(double) is built-in */
/* double asdouble(uint64_t) is built-in */
/* uint32_t asuint32(float) is built-in */
/* double asfloat(uint32_t) is built-in */

#define NAN (asfloat(0x7FC00000U))
#define INFINITY (asfloat(0x7F800000U))

extern double acos(double x);
extern double asin(double x);
extern double atan(double x);
extern double atan2(double x, double y);
extern double cos(double x);
extern double sin(double x);
extern double tan(double x);
extern double cosh(double x);
extern double sinh(double x);
extern double tanh(double x);
extern double exp(double x); 
extern double frexp(double value, int *exp);
extern double ldexp(double x, int exp);
extern double log(double x);
extern double log10(double x);
extern double modf(double value, double *iptr);
extern double pow(double x, double y);
extern double sqrt(double x);
extern double ceil(double x);
extern double fabs(double x);
extern double floor(double x);
extern double fmod(double x, double y);

/* selected C99 additions for doubles */
enum { FP_NAN, FP_INFINITE, FP_ZERO, FP_SUBNORMAL, FP_NORMAL };
extern int fpclassify(double x);
extern int isfinite(double x);
extern int isinf(double x);
extern int isnan(double x);
extern int isnormal(double x);
extern int signbit(double x);
extern double copysign(double x, double y);
extern double scalbn(double x, int n);
extern double round(double x);
extern double trunc(double x);
extern double fmax(double x, double y);
extern double fmin(double x, double y);
extern double expm1(double x); 


