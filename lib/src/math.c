#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>
#include <fenv.h>

/* fake exceptions used below */
#define FE_OVERFLOW 0
#define FE_UNDERFLOW 0

#define __ieee754_fabs(x) (asdouble(asuint64(x) & 0x7FFFFFFFFFFFFFFFULL))
#define __ieee754_copysign(x, y) (asdouble((asuint64(x) & 0x7FFFFFFFFFFFFFFFULL) | (asuint64(y) & 0x8000000000000000ULL)))

#define __builtin_nan(s) (asdouble(0x7FF8000000000000ULL)) /* +nan */
#define math_force_eval(x) ((void)(x)) /* supposed to raise exception but probably won't */

/* wasm raises no exceptions, so these are straightforward */
#define isgreater(x, y)      ((x) > (y))
#define isgreaterequal(x, y) ((x) >= (y))
#define isless(x, y)         ((x) < (y))
#define islessequal(x, y)    ((x) <= (y))

/* code below is based on fdlibm with the following licensing note:
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved. */

#define IEEE754_DOUBLE_MAXEXP 0x7ff
#define IEEE754_DOUBLE_BIAS 0x3ff
#define IEEE754_DOUBLE_SHIFT 20

#define GET_DOUBLE_WORDS(ix0,ix1,d)          \
do { uint64_t __t = asuint64(d);             \
     (ix0) = (int32_t)(__t >> 32);           \
     (ix1) = (int32_t)(__t); } while (0)

#define GET_HIGH_WORD(i,d)                   \
 ((i) = (int32_t)(asuint64(d) >> 32))

#define GET_LOW_WORD(i,d)                    \
 ((i) = (int32_t)asuint64(d))

#define INSERT_WORDS(d,ix0,ix1)              \
 ((d) = asdouble(((uint64_t)(ix0) << 32)     \
     | ((uint64_t)(ix1) & 0xFFFFFFFFULL)))
     
#define SET_HIGH_WORD(d,v)                   \
do { double *__p = &(d);                     \
     uint64_t __t = (asuint64(*__p) &        \
     0xFFFFFFFFULL) | ((uint64_t)(v) << 32); \
     *__p = asdouble(__t); } while (0)
     
#define SET_LOW_WORD(d,v)                    \
do { double *__p = &(d);                     \
     uint64_t __t = (asuint64(*__p) &~       \
     0xFFFFFFFFULL) | (uint64_t)(v);         \
     *__p = asdouble(__t); } while (0)

#define IC(x) ((int32_t) x)
#define UC(x) ((uint32_t) x)

enum {
  FP_NAN,
  FP_INFINITE,
  FP_ZERO,
  FP_SUBNORMAL,
  FP_NORMAL
};

/* Constants:
 * The hexadecimal values are the intended ones for most of the 
 * constants. The decimal values may be used, provided that the 
 * compiler will convert from decimal to binary accurately enough 
 * to produce the hexadecimal values shown. */

#define two54   (asdouble(0x4350000000000000ULL)) /* 2^54 = 0x1p54 = 1.80143985094819840000e+16 */
#define twom54  (asdouble(0x3C90000000000000ULL)) /* 2^-54 = 0x1p-54 = 5.55111512312578270212e-17 */
#define hugeval (1.0e+300)
#define tiny    (1.0e-300)

static const double bp[] = { 1.0, 1.5 };
static const double dp_h[] = { 0.0, 5.84962487220764160156e-01 }; /* 0x3FE2B803, 0x40000000 */
static const double dp_l[] = { 0.0, 1.35003920212974897128e-08 }; /* 0x3E4CFDEB, 0x43CFD006 */
static const double Zero[] = { 0.0, -0.0 };
static const double zero = 0.0;
static const double one = 1.0;
static const double two = 2.0;
static const double two53 = 9007199254740992.0; /* 0x43400000, 0x00000000 */
/* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
static const double L1 = 5.99999999999994648725e-01; /* 0x3FE33333, 0x33333303 */
static const double L2 = 4.28571428578550184252e-01; /* 0x3FDB6DB6, 0xDB6FABFF */
static const double L3 = 3.33333329818377432918e-01; /* 0x3FD55555, 0x518F264D */
static const double L4 = 2.72728123808534006489e-01; /* 0x3FD17460, 0xA91D4101 */
static const double L5 = 2.30660745775561754067e-01; /* 0x3FCD864A, 0x93C9DB65 */
static const double L6 = 2.06975017800338417784e-01; /* 0x3FCA7E28, 0x4A454EEF */
static const double P1 = 1.66666666666666019037e-01; /* 0x3FC55555, 0x5555553E */
static const double P2 = -2.77777777770155933842e-03; /* 0xBF66C16C, 0x16BEBD93 */
static const double P3 = 6.61375632143793436117e-05; /* 0x3F11566A, 0xAF25DE2C */
static const double P4 = -1.65339022054652515390e-06; /* 0xBEBBBD41, 0xC5D26BF1 */
static const double P5 = 4.13813679705723846039e-08; /* 0x3E663769, 0x72BEA4D0 */
static const double lg2 = 6.93147180559945286227e-01; /* 0x3FE62E42, 0xFEFA39EF */
static const double lg2_h = 6.93147182464599609375e-01; /* 0x3FE62E43, 0x00000000 */
static const double lg2_l = -1.90465429995776804525e-09; /* 0xBE205C61, 0x0CA86C39 */
static const double ovt = 8.0085662595372944372e-0017; /* -(1024-log2(ovfl+.5ulp)) */
static const double cp = 9.61796693925975554329e-01; /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
static const double cp_h = 9.61796700954437255859e-01; /* 0x3FEEC709, 0xE0000000 =(float)cp */
static const double cp_l = -7.02846165095275826516e-09; /* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h */
static const double ivln2 = 1.44269504088896338700e+00; /* 0x3FF71547, 0x652B82FE =1/ln2 */
static const double ivln2_h = 1.44269502162933349609e+00; /* 0x3FF71547, 0x60000000 =24b 1/ln2 */
static const double ivln2_l = 1.92596299112661746887e-08; /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail */
static const double ln2_hi = 6.93147180369123816490e-01; /* 3fe62e42 fee00000 */
static const double ln2_lo = 1.90821492927058770002e-10; /* 3dea39ef 35793c76 */
static const double Lg1 = 6.666666666666735130e-01; /* 3FE55555 55555593 */
static const double Lg2 = 3.999999999940941908e-01; /* 3FD99999 9997FA04 */
static const double Lg3 = 2.857142874366239149e-01; /* 3FD24924 94229359 */
static const double Lg4 = 2.222219843214978396e-01; /* 3FCC71C5 1D8E78AF */
static const double Lg5 = 1.818357216161805012e-01; /* 3FC74664 96CB03DE */
static const double Lg6 = 1.531383769920937332e-01; /* 3FC39A09 D078C69F */
static const double Lg7 = 1.479819860511658591e-01; /* 3FC2F112 DF3E5244 */
static const double ivln10 = 4.34294481903251816668e-01; /* 0x3FDBCB7B, 0x1526E50E */
static const double log10_2hi = 3.01029995663611771306e-01; /* 0x3FD34413, 0x509F6000 */
static const double log10_2lo = 3.69423907715893078616e-13; /* 0x3D59FEF3, 0x11F12B36 */
/* exp constants */
static const double halF[2] = { 0.5, -0.5 };
static const double twom1000 = 9.33263618503218878990e-302;  /* 2**-1000=0x01700000,0 */
static const double o_threshold = 7.09782712893383973096e+02; /* 0x40862E42, 0xFEFA39EF */
static const double u_threshold = -7.45133219101941108420e+02; /* 0xc0874910, 0xD52D3051 */
static const double ln2HI[2] = {
  6.93147180369123816490e-01, /* 0x3fe62e42, 0xfee00000 */
  -6.93147180369123816490e-01 /* 0xbfe62e42, 0xfee00000 */
};
static const double ln2LO[2] = {
  1.90821492927058770002e-10, /* 0x3dea39ef, 0x35793c76 */
  -1.90821492927058770002e-10 /* 0xbdea39ef, 0x35793c76 */
};

/* check for NaNs */
static int isnan(double x)
{
  uint64_t ux = asuint64(x);
  return (ux & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL
      && (ux & 0x000FFFFFFFFFFFFFULL) != 0x0000000000000000ULL;
}

/* check for signalling NaN */
static int issignaling(double x)
{
 uint32_t hxi, lxi;
 GET_DOUBLE_WORDS(hxi, lxi, x);
 /* To keep the following comparison simple, toggle the quiet/signaling bit,
    so that it is set for sNaNs.  This is inverse to IEEE 754-2008 (as well as
    common practice for IEEE 754-1985).  */
 hxi ^= UC(0x00080000);
 /* If lxi != 0, then set any suitable bit of the significand in hxi.  */
 hxi |= (lxi | -lxi) >> 31;
 /* We have to compare for greater (instead of greater or equal), because x's
    significand being all-zero designates infinity not NaN.  */
 return (hxi & UC(0x7FFFFFFF)) > UC(0x7FF80000);
}

/* scalbn (double x, int n)
 * scalbn(x,n) returns x* 2**n  computed by  exponent  
 * manipulation rather than by actually performing an 
 * exponentiation or a multiplication. */
static double __ieee754_scalbn(double x, int n)
{
  int32_t k, hx, lx;

  GET_DOUBLE_WORDS(hx, lx, x);
  k = (hx >> IEEE754_DOUBLE_SHIFT) & IEEE754_DOUBLE_MAXEXP; /* extract exponent */
  if (k == 0) { /* 0 or subnormal x */
    if ((lx | (hx & IC(0x7FFFFFFF))) == 0) return x; /* +-0 */
    x *= two54;
    GET_HIGH_WORD(hx, x);
    k = ((hx & IC(0x7FF00000)) >> IEEE754_DOUBLE_SHIFT) - 54;
  }
  if (k == IEEE754_DOUBLE_MAXEXP)
    return x + x; /* NaN or Inf */
  if ((int32_t)n < IC(-30000))
    return tiny * __ieee754_copysign(tiny, x); /* underflow */
  if ((int32_t)n > IC(30000) || k + n > 0x7fe)
    return hugeval * __ieee754_copysign(hugeval, x); /* overflow  */
  /* Now k and n are bounded we know that k = k+n does not overflow */
  k = k + n;
  if (k > 0) { /* normal result */
    SET_HIGH_WORD(x, (hx & UC(0x800FFFFFU)) | (k << IEEE754_DOUBLE_SHIFT));
    return x;
  }
  if (k <= -54) return tiny * __ieee754_copysign(tiny, x); /*underflow */
  k += 54; /* subnormal result */
  SET_HIGH_WORD(x, (hx & UC(0x800FFFFFU)) | (k << IEEE754_DOUBLE_SHIFT));
  return x * twom54;
}

/* for non-zero x, x = frexp(arg,&exp)
 * return a double fp quantity x such that 0.5 <= |x| <1.0
 * and the corresponding binary exponent "exp". That is arg = x*2^exp.
 * If arg is inf, 0.0, or NaN, then frexp(arg,&exp) returns arg 
 * with *exp=0. */
static double __ieee754_frexp(double x, int *eptr)
{
  int32_t hx, ix, lx;

  GET_DOUBLE_WORDS(hx, lx, x);
  ix = IC(0x7fffffffU) & hx;
  *eptr = 0;
  if (ix >= IC(0x7ff00000U) || ((ix | lx) == 0))
    return x; /* 0,inf,nan */
  if (ix < IC(0x00100000U)) { /* subnormal */
    x *= two54;
    GET_HIGH_WORD(hx, x);
    ix = hx & IC(0x7fffffffU);
    *eptr = -54;
  }
  *eptr += (int)(ix >> 20) - 1022;
  hx = (hx & IC(0x800fffffU)) | IC(0x3fe00000U);
  SET_HIGH_WORD(x, hx);
  return x;
}

/* ceil(x)
 * Return x rounded toward -inf to integral value
 * Method: Bit twiddling.
 * Exception: Inexact flag raised if x not equal to ceil(x). */
static double __ieee754_ceil(double x)
{
  int32_t i0, j0;
  uint32_t i, j, i1;

  GET_DOUBLE_WORDS(i0, i1, x);
  j0 = ((i0 >> IEEE754_DOUBLE_SHIFT) & IEEE754_DOUBLE_MAXEXP) - IEEE754_DOUBLE_BIAS;
  if (j0 < IEEE754_DOUBLE_SHIFT) {
    if (j0 < 0) { /* raise inexact if x != 0 */
      math_force_eval(hugeval + x);
      /* return 0*sign(x) if |x|<1 */
      if (i0 < 0) {
        i0 = IC(0x80000000U);
        i1 = 0;
      } else if ((i0 | i1) != 0) {
        i0 = IC(0x3ff00000U);
        i1 = 0;
      }
    } else {
      i = UC(0x000fffffU) >> j0;
      if (((i0 & i) | i1) == 0)
        return x; /* x is integral */
      math_force_eval(hugeval + x); /* raise inexact flag */
      if (i0 > 0) i0 += IC(0x00100000U) >> j0;
      i0 &= (int32_t)(~i);
      i1 = 0;
    }
  } else if (j0 > 51) {
    if (j0 == (IEEE754_DOUBLE_MAXEXP - IEEE754_DOUBLE_BIAS))
      return x + x; /* inf or NaN */
    else
      return x; /* x is integral */
  } else {
    i = UC(0xffffffffU) >> (j0 - IEEE754_DOUBLE_SHIFT);
    if ((i1 & i) == 0) return x; /* x is integral */
    math_force_eval(hugeval + x); /* raise inexact flag */
    if (i0 > 0) {
      if (j0 == IEEE754_DOUBLE_SHIFT)
        i0 += 1;
      else {
        j = i1 + (UC(1) << (52 - j0));
        if (j < i1)
          i0 += 1; /* got a carry */
        i1 = j;
      }
    }
    i1 &= (~i);
  }
  INSERT_WORDS(x, i0, i1);
  return x;
}

/* floor(x)
 * Return x rounded toward -inf to integral value
 * Method: Bit twiddling.
 * Exception: Inexact flag raised if x not equal to floor(x). */
double __ieee754_floor(double x)
{
  int32_t i0, j0;
  uint32_t i, j, i1;

  GET_DOUBLE_WORDS(i0, i1, x);
  j0 = ((i0 >> IEEE754_DOUBLE_SHIFT) & IEEE754_DOUBLE_MAXEXP) - IEEE754_DOUBLE_BIAS;
  if (j0 < IEEE754_DOUBLE_SHIFT) {
    if (j0 < 0) { /* raise inexact if x != 0 */
      math_force_eval(hugeval + x); /* return 0*sign(x) if |x|<1 */
      if (i0 >= 0) {
        i0 = 0; i1 = 0;
      } else if (((i0 & IC(0x7fffffffU)) | i1) != 0) {
        i0 = IC(0xbff00000U);
        i1 = 0;
      }
    } else {
      i = UC(0x000fffffU) >> j0;
      if (((i0 & i) | i1) == 0) return x; /* x is integral */
      math_force_eval(hugeval + x); /* raise inexact flag */
      if (i0 < 0) i0 += (int32_t)(UC(0x00100000U) >> j0);
      i0 &= (int32_t)(~i);
      i1 = 0;
    }
  } else if (j0 > 51) {
    if (j0 == (IEEE754_DOUBLE_MAXEXP - IEEE754_DOUBLE_BIAS))
      return x + x; /* inf or NaN */
    else
      return x; /* x is integral */
  } else {
    i = UC(0xffffffffU) >> (j0 - IEEE754_DOUBLE_SHIFT);
    if ((i1 & i) == 0) return x; /* x is integral */
    math_force_eval(hugeval + x); /* raise inexact flag */
    if (i0 < 0) {
      if (j0 == IEEE754_DOUBLE_SHIFT)
        i0 += 1;
      else {
        j = i1 + (UC(1) << (52 - j0));
        if (j < i1)
          i0 += 1; /* got a carry */
        i1 = j;
      }
    }
    i1 &= (~i);
  }
  INSERT_WORDS(x, i0, i1);
  return x;
} 

/* The round() functions round their argument to the nearest integer
 * value in floating-point format, rounding halfway cases away from zero,
 * regardless of the current rounding direction.  (While the "inexact"
 * floating-point exception behavior is unspecified by the C standard, the
 * <<round>> functions are written so that "inexact" is not raised if the
 * result does not equal the argument, which behavior is as recommended by
 * IEEE 754 for its related functions.) */
static double __ieee754_round(double x)
{
  int32_t i0, j0;
  uint32_t i1;

  GET_DOUBLE_WORDS(i0, i1, x);
  j0 = ((i0 >> IEEE754_DOUBLE_SHIFT) & IEEE754_DOUBLE_MAXEXP) - IEEE754_DOUBLE_BIAS;
  if (j0 < IEEE754_DOUBLE_SHIFT) {
    if (j0 < 0) {
      math_force_eval(hugeval + x);
      i0 &= IC(0x80000000U);
      if (j0 == -1) /* Result is +1.0 or -1.0. */
        i0 |= IC(0x3ff00000U);
      i1 = 0;
    } else {
      uint32_t i = UC(0x000fffffU) >> j0;

      if (((i0 & i) | i1) == 0) return x; /* X is integral. */

      /* Raise inexact if x != 0.  */
      math_force_eval(hugeval + x);

      i0 += (int32_t)(UC(0x00080000U) >> j0);
      i0 &= (int32_t)~i;
      i1 = 0;
    }
  } else if (j0 > 51) {
    if (j0 == (IEEE754_DOUBLE_MAXEXP - IEEE754_DOUBLE_BIAS))
      /* Inf or NaN.  */
      return x + x;
    else
      return x;
  } else {
    uint32_t i = UC(0xffffffffU) >> (j0 - IEEE754_DOUBLE_SHIFT);
    uint32_t j;

    if ((i1 & i) == 0) return x; /* X is integral. */

    /* Raise inexact if x != 0.  */
    math_force_eval(hugeval + x);

    j = i1 + (UC(1) << (51 - j0));

    if (j < i1)
      i0 += 1;
    i1 = j;
    i1 &= ~i;
  }

  INSERT_WORDS(x, i0, i1);
  return x;
}

/* The trunc functions round their argument to the integer value, in floating format,
 * nearest to but no larger in magnitude than the argument. */
static double __ieee754_trunc(double x)
{
  int32_t i0, j0;
  uint32_t i1;
  int32_t sx;

  GET_DOUBLE_WORDS(i0, i1, x);
  sx = i0 & IC(0x80000000U);
  j0 = ((i0 >> IEEE754_DOUBLE_SHIFT) & IEEE754_DOUBLE_MAXEXP) - IEEE754_DOUBLE_BIAS;
  if (j0 < IEEE754_DOUBLE_SHIFT) {
    if (j0 < 0) /* The magnitude of the number is < 1 so the result is +-0.  */
      INSERT_WORDS(x, sx, 0);
    else
      INSERT_WORDS(x, sx | (i0 & ~(UC(0x000fffffU) >> j0)), 0);
  } else if (j0 > 51) {
    if (j0 == (IEEE754_DOUBLE_MAXEXP - IEEE754_DOUBLE_BIAS))
      return x + x; /* x is inf or NaN.  */
  } else {
    INSERT_WORDS(x, i0, i1 & ~(UC(0xffffffffU) >> (j0 - IEEE754_DOUBLE_SHIFT)));
  }

  return x;
}


/* modf(double x, double *iptr) 
 * return fraction part of x, and return x's integral part in *iptr.
 * Method: Bit twiddling.
 * Exception: No exception. */
static double __ieee754_modf(double x, double *iptr)
{
  int32_t i0, i1, j0;
  uint32_t i;

  GET_DOUBLE_WORDS(i0, i1, x);
  j0 = ((i0 >> IEEE754_DOUBLE_SHIFT) & IEEE754_DOUBLE_MAXEXP) - IEEE754_DOUBLE_BIAS; /* exponent of x */
  if (j0 < IEEE754_DOUBLE_SHIFT) { /* integer part in high x */
    if (j0 < 0) { /* |x|<1 */
      INSERT_WORDS(*iptr, i0 & UC(0x80000000U), 0); /* *iptr = +-0 */
      return x;
    } else {
      i = UC(0x000fffff) >> j0;
      if (((i0 & i) | i1) == 0) { /* x is integral */
        *iptr = x;
        INSERT_WORDS(x, i0 & UC(0x80000000U), 0); /* return +-0 */
        return x;
      } else {
        INSERT_WORDS(*iptr, i0 & (~i), 0);
        return x - *iptr;
      }
    }
  } else if (j0 > 51) { /* no fraction part */
    *iptr = x * one;
    /* We must handle NaNs separately.  */
    if (j0 == (IEEE754_DOUBLE_MAXEXP - IEEE754_DOUBLE_BIAS) && ((i0 & UC(0xfffff)) | i1) != 0)
      return x * one;
    INSERT_WORDS(x, i0 & UC(0x80000000U), 0); /* return +-0 */
    return x;
  } else { /* fraction part in low x */
    i = UC(0xffffffffU) >> (j0 - IEEE754_DOUBLE_SHIFT);
    if ((i1 & i) == 0) { /* x is integral */
      *iptr = x;
      INSERT_WORDS(x, i0 & UC(0x80000000U), 0); /* return +-0 */
      return x;
    } else {
      INSERT_WORDS(*iptr, i0, i1 & (~i));
      return x - *iptr;
    }
  }
  return __builtin_nan(""); /* to silence WCPL */
}

/* __ieee754_fmod(x,y)
 * Return x mod y in exact arithmetic
 * Method: shift and subtract */
static double __ieee754_fmod(double x, double y)
{
  int32_t n, hx, hy, hz, ix, iy, sx, i;
  uint32_t lx, ly, lz;

  GET_DOUBLE_WORDS(hx, lx, x);
  GET_DOUBLE_WORDS(hy, ly, y);
  sx = hx & IC(0x80000000U); /* sign of x */
  hx ^= sx; /* |x| */
  hy &= IC(0x7fffffff); /* |y| */

  /* purge off exception values */
  if ((hy | ly) == 0 || (hx >= IC(0x7ff00000U)) || /* y=0,or x not finite */
    ((hy | ((ly | -ly) >> 31)) > IC(0x7ff00000U))) /* or y is NaN */
    return (x * y) / (x * y);
  if (hx <= hy) {
    if ((hx < hy) || (lx < ly))
      return x; /* |x|<|y| return x */
    if (lx == ly)
      return Zero[(uint32_t) sx >> 31]; /* |x|=|y| return x*0 */
  }

  /* determine ix = ilogb(x) */
  if (hx < IC(0x00100000U)) { /* subnormal x */
    if (hx == 0) {
      for (ix = -1043, i = (int32_t)lx; i > 0; i <<= 1)
        ix -= 1;
    } else {
      for (ix = -1022, i = (hx << 11); i > 0; i <<= 1)
        ix -= 1;
    }
  } else {
    ix = (hx >> 20) - 1023;
  }

  /* determine iy = ilogb(y) */
  if (hy < IC(0x00100000U)) { /* subnormal y */
    if (hy == 0) {
      for (iy = -1043, i = (int32_t)ly; i > 0; i <<= 1)
        iy -= 1;
    } else {
      for (iy = -1022, i = (hy << 11); i > 0; i <<= 1)
        iy -= 1;
    }
  } else {
    iy = (hy >> 20) - 1023;
  }

  /* set up {hx,lx}, {hy,ly} and align y to x */
  if (ix >= -1022)
    hx = IC(0x00100000U) | (IC(0x000fffffU) & hx);
  else { /* subnormal x, shift x to normal */
    n = -1022 - ix;
    if (n <= 31) {
      hx = (hx << n) | (int32_t)(lx >> (32 - n));
      lx <<= n;
    } else {
      hx = (int32_t)(lx << (n - 32));
      lx = 0;
    }
  }
  if (iy >= -1022)
    hy = IC(0x00100000U) | (IC(0x000fffffU) & hy);
  else { /* subnormal y, shift y to normal */
    n = -1022 - iy;
    if (n <= 31) {
      hy = (hy << n) | (int32_t)(ly >> (32 - n));
      ly <<= n;
    } else {
      hy = (int32_t)(ly << (n - 32));
      ly = 0;
    }
  }

  /* fix point fmod */
  n = ix - iy;
  while (n--) {
    hz = hx - hy;
    lz = lx - ly;
    if (lx < ly)
      hz -= 1;
    if (hz < 0) {
      hx = hx + hx + (int32_t)(lx >> 31);
      lx = lx + lx;
    } else {
      if ((hz | lz) == 0)   /* return sign(x)*0 */
        return Zero[(uint32_t) sx >> 31];
      hx = hz + hz + (int32_t)(lz >> 31);
      lx = lz + lz;
    }
  }
  hz = hx - hy;
  lz = lx - ly;
  if (lx < ly)
    hz -= 1;
  if (hz >= 0) {
    hx = hz;
    lx = lz;
  }

  /* convert back to floating value and restore the sign */
  if ((hx | lx) == 0) { /* return sign(x)*0 */
    return Zero[(uint32_t) sx >> 31];
  }
  while (hx < IC(0x00100000U)) { /* normalize x */
    hx = hx + hx + (int32_t)(lx >> 31);
    lx = lx + lx;
    iy -= 1;
  }
  if (iy >= -1022) { /* normalize output */
    hx = ((hx - IC(0x00100000U)) | ((iy + 1023) << 20));
    INSERT_WORDS(x, hx | sx, lx);
  } else { /* subnormal output */
    n = -1022 - iy;
    if (n <= 20) {
      lx = (lx >> n) | ((uint32_t) hx << (32 - n));
      hx >>= n;
    } else if (n <= 31) {
      lx = (hx << (32 - n)) | (lx >> n);
      hx = sx;
    } else {
      lx = hx >> (n - 32);
      hx = sx;
    }
    INSERT_WORDS(x, hx | sx, lx);
    x *= one; /* create necessary signal */
  }
  return x; /* exact output */
}

/* __ieee754_sqrt(x)
 * Return correctly rounded sqrt.
 *
 * Method: 
 *   Bit by bit method using integer arithmetic. (Slow, but portable) 
 *   1. Normalization
 * Scale x to y in [1,4) with even powers of 2: 
 * find an integer k such that  1 <= (y=x*2^(2k)) < 4, then
 *  sqrt(x) = 2^k * sqrt(y)
 *   2. Bit by bit computation
 * Let q  = sqrt(y) truncated to i bit after binary point (q = 1),
 *      i        0
 *                                     i+1         2
 *     s  = 2*q , and y  =  2   * ( y - q  ).  (1)
 *      i      i            i                 i
 *                                                        
 * To compute q    from q , one checks whether 
 *      i+1       i                       
 *
 *         -(i+1) 2
 *   (q + 2      ) <= y.   (2)
 *          i
 *             -(i+1)
 * If (2) is false, then q   = q ; otherwise q   = q  + 2      .
 *           i+1   i             i+1   i
 *
 * With some algebric manipulation, it is not difficult to see
 * that (2) is equivalent to 
 *                             -(i+1)
 *   s  +  2       <= y   (3)
 *    i                i
 *
 * The advantage of (3) is that s  and y  can be computed by 
 *          i      i
 * the following recurrence formula:
 *     if (3) is false
 *
 *     s     =  s  , y    = y   ;   (4)
 *      i+1      i   i+1    i
 *
 *     otherwise,
 *                         -i                     -(i+1)
 *     s   =  s  + 2  ,  y    = y  -  s  - 2    (5)
 *           i+1      i          i+1    i     i
 *    
 * One may easily use induction to prove (4) and (5). 
 * Note. Since the left hand side of (3) contain only i+2 bits,
 *       it does not necessary to do a full (53-bit) comparison 
 *       in (3).
 *   3. Final rounding
 * After generating the 53 bits result, we compute one more bit.
 * Together with the remainder, we can decide whether the
 * result is exact, bigger than 1/2ulp, or less than 1/2ulp
 * (it will never equal to 1/2ulp).
 * The rounding mode can be detected by checking whether
 * huge + tiny is equal to huge, and whether huge - tiny is
 * equal to huge for some floating point number "huge" and "tiny".
 *  
 * Special cases:
 * sqrt(+-0) = +-0  ... exact
 * sqrt(inf) = inf
 * sqrt(-ve) = NaN  ... with invalid signal
 * sqrt(NaN) = NaN  ... with invalid signal for signaling NaN */
static double __ieee754_sqrt(double x)
{
  double z;
  int32_t sign = IC(0x80000000U);
  uint32_t r, t1, s1, ix1, q1;
  int32_t ix0, s0, q, m, t, i;

  GET_DOUBLE_WORDS(ix0, ix1, x);

  /* take care of Inf and NaN */
  if ((ix0 & IC(0x7ff00000U)) == IC(0x7ff00000U)) {
    return x * x + x; /* sqrt(NaN)=NaN, sqrt(+inf)=+inf, sqrt(-inf)=sNaN */
  }
  /* take care of zero */
  if (ix0 <= 0) {
    if (((ix0 & (~sign)) | ix1) == 0)
      return x; /* sqrt(+-0) = +-0 */
    else if (ix0 < 0)
      return (x - x) / (x - x); /* sqrt(-ve) = sNaN */
  }
  /* normalize x */
  m = (ix0 >> 20);
  if (m == 0) { /* subnormal x */
    while (ix0 == 0) {
      m -= 21;
      ix0 |= (int32_t)(ix1 >> 11);
      ix1 <<= 21;
    }
    for (i = 0; (ix0 & IC(0x00100000U)) == 0; i++)
      ix0 <<= 1;
    m -= i - 1;
    ix0 |= (int32_t)(ix1 >> (32 - i));
    ix1 <<= i;
  }
  m -= 1023; /* unbias exponent */
  ix0 = (ix0 & IC(0x000fffffU)) | IC(0x00100000U);
  if (m & 1) { /* odd m, double x to make it even */
    ix0 += ix0 + (int32_t)((ix1 & sign) >> 31);
    ix1 += ix1;
  }
  m >>= 1; /* m = [m/2] */

  /* generate sqrt(x) bit by bit */
  ix0 += ix0 + (int32_t)((ix1 & sign) >> 31);
  ix1 += ix1;
  q = s0 = 0; q1 = s1 = 0; /* [q,q1] = sqrt(x) */
  r = IC(0x00200000U); /* r = moving bit from right to left */

  while (r != 0) {
    t = s0 + (int32_t)r;
    if (t <= ix0) {
      s0 = t + (int32_t)r;
      ix0 -= t;
      q += (int32_t)r;
    }
    ix0 += ix0 + (int32_t)((ix1 & sign) >> 31);
    ix1 += ix1;
    r >>= 1;
  }

  r = sign;
  while (r != 0) {
    t1 = s1 + r;
    t = s0;
    if ((t < ix0) || ((t == ix0) && (t1 <= ix1))) {
      s1 = t1 + r;
      if (((int32_t)(t1 & sign) == sign) && (s1 & sign) == 0)
        s0 += 1;
      ix0 -= t;
      if (ix1 < t1)
        ix0 -= 1;
      ix1 -= t1;
      q1 += r;
    }
    ix0 += ix0 + (int32_t)((ix1 & sign) >> 31);
    ix1 += ix1;
    r >>= 1;
  }

  /* use floating add to find out rounding direction */
  if ((ix0 | ix1) != 0) {
    z = one - tiny; /* trigger inexact flag */
    if (z >= one) {
      z = one + tiny;
      if (q1 == UC(0xffffffffU)) {
        q1 = 0;
        q += 1;
      } else if (z > one) {
        if (q1 == UC(0xfffffffeU))
          q += 1;
        q1 += 2;
      } else
        q1 += (q1 & 1);
    }
  }
  ix0 = (q >> 1) + IC(0x3fe00000U);
  ix1 = q1 >> 1;
  if ((q & 1) == 1)
    ix1 |= sign;
  ix0 += (m << 20);
  INSERT_WORDS(z, ix0, ix1);
  return z;
}

/* __ieee754_pow(x,y) return x**y
 *
 *        n
 * Method:  Let x =  2   * (1+f)
 * 1. Compute and return log2(x) in two pieces:
 *  log2(x) = w1 + w2,
 *    where w1 has 53-24 = 29 bit trailing zeros.
 * 2. Perform y*log2(x) = n+y' by simulating multi-precision 
 *    arithmetic, where |y'|<=0.5.
 * 3. Return x**y = 2**n*exp(y'*log2)
 *
 * Special cases:
 * 1.  (anything) ** 0  is 1
 * 2.  (anything) ** 1  is itself
 * 3a. (anything) ** NAN is NAN except
 * 3b. +1         ** NAN is 1
 * 4.  NAN ** (anything except 0) is NAN
 * 5.  +-(|x| > 1) **  +INF is +INF
 * 6.  +-(|x| > 1) **  -INF is +0
 * 7.  +-(|x| < 1) **  +INF is +0
 * 8.  +-(|x| < 1) **  -INF is +INF
 * 9.  +-1         ** +-INF is 1
 * 10. +0 ** (+anything except 0, NAN)               is +0
 * 11. -0 ** (+anything except 0, NAN, odd integer)  is +0
 * 12. +0 ** (-anything except 0, NAN)               is +INF
 * 13. -0 ** (-anything except 0, NAN, odd integer)  is +INF
 * 14. -0 ** (odd integer) = -( +0 ** (odd integer) )
 * 15. +INF ** (+anything except 0,NAN) is +INF
 * 16. +INF ** (-anything except 0,NAN) is +0
 * 17. -INF ** (anything)  = -0 ** (-anything)
 * 18. (-anything) ** (integer) is (-1)**(integer)*(+anything**integer)
 * 19. (-anything except 0 and inf) ** (non-integer) is NAN
 *
 * Accuracy:
 * pow(x,y) returns x**y nearly rounded. In particular
 *   pow(integer,integer)
 * always returns the correct integer provided it is 
 * representable. */
static double __ieee754_pow(double x, double y)
{
  double z, ax, z_h, z_l, p_h, p_l;
  double y1, t1, t2, r, s, t, u, v, w;
  int32_t i, j, k, yisint, n;
  int32_t hx, hy, ix, iy;
  uint32_t lx, ly;

  GET_DOUBLE_WORDS(hx, lx, x);
  GET_DOUBLE_WORDS(hy, ly, y);
  ix = hx & IC(0x7fffffff);
  iy = hy & IC(0x7fffffff);

  /* y==zero: x**0 = 1 */
  if ((iy | ly) == 0) {
    /* unless x is signaling NaN */
    if (issignaling(x))
      return __builtin_nan("");
    return one;
  }

  /* x|y==NaN return NaN unless x==1 then return 1 */
  if (ix > IC(0x7ff00000) || ((ix == IC(0x7ff00000)) && (lx != 0)) 
   || iy > IC(0x7ff00000) || ((iy == IC(0x7ff00000)) && (ly != 0))) {
    if (((ix - IC(0x3ff00000)) | lx) == 0 && !(hx & UC(0x80000000U)))
      return one;
    else
      return __builtin_nan("");
  }

  /* determine if y is an odd int when x < 0
   * yisint = 0   ... y is not an integer
   * yisint = 1   ... y is an odd int
   * yisint = 2   ... y is an even int */
  yisint = 0;
  if (hx < 0) {
    if (iy >= IC(0x43400000))
      yisint = 2; /* even integer y */
    else if (iy >= IC(0x3ff00000)) {
      k = (iy >> IEEE754_DOUBLE_SHIFT) - IEEE754_DOUBLE_BIAS; /* exponent */
      if (k > IEEE754_DOUBLE_SHIFT) {
        j = (int32_t)(ly >> (52 - k));
        if ((j << (52 - k)) == (int32_t)ly)
          yisint = 2 - (j & 1);
      } else if (ly == 0) {
        j = iy >> (IEEE754_DOUBLE_SHIFT - k);
        if ((j << (IEEE754_DOUBLE_SHIFT - k)) == iy)
          yisint = 2 - (j & 1);
      }
    }
  }

  /* special value of y */
  if (ly == 0) {
    if (iy == IC(0x7ff00000)) { /* y is +-inf */
      if (((ix - IC(0x3ff00000)) | lx) == 0)
        return one; /* +-1**+-inf = 1 */
      else if (ix >= IC(0x3ff00000)) /* (|x|>1)**+-inf = inf,0 */
        return (hy >= 0) ? y : zero;
      else /* (|x|<1)**-,+inf = inf,0 */
        return (hy < 0) ? -y : zero;
    }
    if (iy == IC(0x3ff00000)) { /* y is  +-1 */
      if (hy < 0)
        return one / x;
      else
        return x;
    }
    if (hy == IC(0x40000000))
      return x * x; /* y is  2 */
    if (hy == IC(0x3fe00000)) { /* y is  0.5 */
      if (hx >= 0) /* x >= +0 */
        return __ieee754_sqrt(x);
    }
  }

  ax = __ieee754_fabs(x);
  /* special value of x */
  if (lx == 0) {
    if (ix == IC(0x7ff00000) || ix == 0 || ix == IC(0x3ff00000)) {
      z = ax; /* x is +-0,+-inf,+-1 */
      if (hy < 0)
        z = one / z; /* z = (1/|x|) */
      if (hx < 0) {
        if (((ix - IC(0x3ff00000)) | yisint) == 0) {
          z = (z - z) / (z - z); /* (-1)**non-int is NaN */
        } else if (yisint == 1)
          z = -z; /* (x<0)**odd = -(|x|**odd) */
      }
      return z;
    }
  }

  /* (x<0)**(non-int) is NaN */
  if (((((uint32_t) hx >> 31) - 1) | yisint) == 0)
    return (x - x) / (x - x);

  /* |y| is huge */
  if (iy > IC(0x41e00000)) { /* if |y| > 2**31 */
    if (iy > IC(0x43f00000)) { /* if |y| > 2**64, must o/uflow */
      if (ix <= IC(0x3fefffff)) {
        if (hy < 0) {
          feraiseexcept(FE_OVERFLOW);
          return HUGE_VAL;
        }
        feraiseexcept(FE_UNDERFLOW);
        return 0;
      }
      if (ix >= IC(0x3ff00000)) {
        if (hy > 0) {
          feraiseexcept(FE_OVERFLOW);
          return HUGE_VAL;
        }
        feraiseexcept(FE_UNDERFLOW);
        return 0;
      }
    }
    /* over/underflow if x is not close to one */
    if (ix < IC(0x3fefffff)) {
      if (hy < 0) {
        feraiseexcept(FE_OVERFLOW);
        return HUGE_VAL;
      }
      feraiseexcept(FE_UNDERFLOW);
      return 0;
    }
    if (ix > IC(0x3ff00000)) {
      if (hy > 0) {
        feraiseexcept(FE_OVERFLOW);
        return HUGE_VAL;
      }
      feraiseexcept(FE_UNDERFLOW);
      return 0;
    }
    /* now |1-x| is tiny <= 2**-20, suffice to compute 
       log(x) by x-x^2/2+x^3/3-x^4/4 */
    t = ax - 1; /* t has 20 trailing zeros */
    w = (t * t) * (0.5 - t * (0.3333333333333333333333 - t * 0.25));
    u = ivln2_h * t; /* ivln2_h has 21 sig. bits */
    v = t * ivln2_l - w * ivln2;
    t1 = u + v;
    SET_LOW_WORD(t1, 0);
    t2 = v - (t1 - u);
  } else {
    double s2, s_h, s_l, t_h, t_l;
    n = 0;
    /* take care subnormal number */
    if (ix < IC(0x00100000)) {
      ax *= two53;
      n -= 53;
      GET_HIGH_WORD(ix, ax);
    }
    n += ((ix) >> IEEE754_DOUBLE_SHIFT) - IEEE754_DOUBLE_BIAS;
    j = ix & IC(0x000fffff);
    /* determine interval */
    ix = j | IC(0x3ff00000); /* normalize ix */
    if (j <= IC(0x3988E))
      k = 0; /* |x|<sqrt(3/2) */
    else if (j < IC(0xBB67A))
      k = 1; /* |x|<sqrt(3)   */
    else {
      k = 0;
      n += 1;
      ix -= IC(0x00100000);
    }
    SET_HIGH_WORD(ax, ix);

    /* compute s = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
    u = ax - bp[k]; /* bp[0]=1.0, bp[1]=1.5 */
    v = one / (ax + bp[k]);
    s = u * v;
    s_h = s;
    SET_LOW_WORD(s_h, 0);
    /* t_h=ax+bp[k] High */
    t_h = zero;
    SET_HIGH_WORD(t_h, ((ix >> 1) | IC(0x20000000)) + IC(0x00080000) + (k << 18));
    t_l = ax - (t_h - bp[k]);
    s_l = v * ((u - s_h * t_h) - s_h * t_l);
    /* compute log(ax) */
    s2 = s * s;
    r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
    r += s_l * (s_h + s);
    s2 = s_h * s_h;
    t_h = 3.0 + s2 + r;
    SET_LOW_WORD(t_h, 0);
    t_l = r - ((t_h - 3.0) - s2);
    /* u+v = s*(1+...) */
    u = s_h * t_h;
    v = s_l * t_h + t_l * s;
    /* 2/(3log2)*(s+...) */
    p_h = u + v;
    SET_LOW_WORD(p_h, 0);
    p_l = v - (p_h - u);
    z_h = cp_h * p_h; /* cp_h+cp_l = 2/(3*log2) */
    z_l = cp_l * p_h + p_l * cp + dp_l[k];
    /* log2(ax) = (s+..)*2/(3*log2) = n + dp_h + z_h + z_l */
    t = (double) n;
    t1 = (((z_h + z_l) + dp_h[k]) + t);
    SET_LOW_WORD(t1, 0);
    t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
  }

  s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
  if (((((uint32_t) hx >> 31) - 1) | (yisint - 1)) == 0)
    s = -one; /* (-ve)**(odd int) */

  /* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
  y1 = y;
  SET_LOW_WORD(y1, 0);
  p_l = (y - y1) * t1 + y * t2;
  p_h = y1 * t1;
  z = p_l + p_h;
  GET_DOUBLE_WORDS(j, i, z);
  if (j >= IC(0x40900000)) { /* z >= 1024 */
    if (((j - IC(0x40900000)) | i) != 0) { /* if z > 1024 */
      feraiseexcept(FE_OVERFLOW);
      return __ieee754_copysign(HUGE_VAL, s);
    }
    if (p_l + ovt > z - p_h) {
      feraiseexcept(FE_OVERFLOW);
      return __ieee754_copysign(HUGE_VAL, s);
    }
  } else if ((j & IC(0x7fffffff)) >= IC(0x4090cc00)) { /* z <= -1075 */
    if (((j - IC(0xc090cc00U)) | i) != 0) { /* z < -1075 */
      feraiseexcept(FE_UNDERFLOW);
      return __ieee754_copysign(0.0, s);
    }
    if (p_l <= z - p_h) {
      feraiseexcept(FE_UNDERFLOW);
      return __ieee754_copysign(0.0, s);
    }
  }
  /* compute 2**(p_h+p_l) */
  i = j & IC(0x7fffffff);
  k = (i >> IEEE754_DOUBLE_SHIFT) - IEEE754_DOUBLE_BIAS;
  n = 0;
  if (i > IC(0x3fe00000)) { /* if |z| > 0.5, set n = [z+0.5] */
    n = j + (IC(0x00100000) >> (k + 1));
    k = ((n & IC(0x7fffffff)) >> IEEE754_DOUBLE_SHIFT) - IEEE754_DOUBLE_BIAS; /* new k for n */
    t = zero;
    SET_HIGH_WORD(t, n & ~(UC(0x000fffff) >> k));
    n = ((n & IC(0x000fffff)) | IC(0x00100000)) >> (IEEE754_DOUBLE_SHIFT - k);
    if (j < 0)
      n = -n;
    p_h -= t;
  }
  t = p_l + p_h;
  SET_LOW_WORD(t, 0);
  u = t * lg2_h;
  v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
  z = u + v;
  w = v - (z - u);
  t = z * z;
  t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
  r = (z * t1) / (t1 - two) - (w + z * w);
  z = one - (r - z);
  GET_HIGH_WORD(j, z);
  j += (n << IEEE754_DOUBLE_SHIFT);
  if ((j >> IEEE754_DOUBLE_SHIFT) <= 0) {
    z = __ieee754_scalbn(z, (int) n); /* subnormal output */
  } else {
    SET_HIGH_WORD(z, j);
  }
  return s * z;
}

/* __ieee754_log(x)
 * Return the logrithm of x
 *
 * Method :                  
 *   1. Argument Reduction: find k and f such that 
 *   x = 2^k * (1+f), 
 *    where  sqrt(2)/2 < 1+f < sqrt(2) .
 *
 *   2. Approximation of log(1+f).
 * Let s = f/(2+f) ; based on log(1+f) = log(1+s) - log(1-s)
 *   = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *        = 2s + s*R
 *      We use a special Reme algorithm on [0,0.1716] to generate 
 *  a polynomial of degree 14 to approximate R The maximum error 
 * of this polynomial approximation is bounded by 2**-58.45. In
 * other words,
 *          2      4      6      8      10      12      14
 *     R(z) ~ Lg1*s +Lg2*s +Lg3*s +Lg4*s +Lg5*s  +Lg6*s  +Lg7*s
 *   (the values of Lg1 to Lg7 are listed in the program)
 * and
 *     |      2          14          |     -58.45
 *     | Lg1*s +...+Lg7*s    -  R(z) | <= 2 
 *     |                             |
 * Note that 2s = f - s*f = f - hfsq + s*hfsq, where hfsq = f*f/2.
 * In order to guarantee error in log below 1ulp, we compute log
 * by
 *  log(1+f) = f - s*(f - R) (if f is not too large)
 *  log(1+f) = f - (hfsq - s*(hfsq+R)). (better accuracy)
 * 
 * 3. Finally,  log(x) = k*ln2 + log(1+f).  
 *       = k*ln2_hi+(f-(hfsq-(s*(hfsq+R)+k*ln2_lo)))
 *    Here ln2 is split into two floating point number: 
 *   ln2_hi + ln2_lo,
 *    where n*ln2_hi is always exact for |n| < 2000.
 *
 * Special cases:
 * log(x) is NaN with signal if x < 0 (including -INF) ; 
 * log(+INF) is +INF; log(0) is -INF with signal;
 * log(NaN) is that NaN with no signal.
 *
 * Accuracy:
 * according to an error analysis, the error is always less than
 * 1 ulp (unit in the last place). */
static double __ieee754_log(double x)
{
  double hfsq, f, s, z, R, w, t1, t2, dk;
  int32_t k, hx, i, j;
  uint32_t lx;

  GET_DOUBLE_WORDS(hx, lx, x);
  k = 0;
  if (hx < IC(0x00100000)) { /* x < 2**-1022  */
    if (((hx & IC(0x7fffffff)) | lx) == 0)
      return -two54 / zero; /* log(+-0)=-inf */
    if (hx < 0)
      return (x - x) / zero; /* log(-#) = NaN */
    k -= 54;
    x *= two54; /* subnormal number, scale up x */
    GET_HIGH_WORD(hx, x);
  }

  if (hx >= IC(0x7ff00000)) return x + x;
  k += (hx >> 20) - 1023;
  hx &= IC(0x000fffff);
  i = (hx + IC(0x95f64)) & IC(0x100000);
  SET_HIGH_WORD(x, hx | (i ^ IC(0x3ff00000))); /* normalize x or x/2 */
  k += (i >> 20);
  f = x - 1.0;
  if ((IC(0x000fffff) & (2 + hx)) < 3) { /* |f| < 2**-20 */
    if (f == zero) {
      if (k == 0) return zero;
      dk = (double) k;
      return dk * ln2_hi + dk * ln2_lo;
    }
    R = f * f * (0.5 - 0.33333333333333333 * f);
    if (k == 0) return f - R;
    dk = (double) k;
    return dk * ln2_hi - ((R - dk * ln2_lo) - f);
  }
  s = f / (2.0 + f);
  dk = (double) k;
  z = s * s;
  i = hx - IC(0x6147a);
  w = z * z;
  j = IC(0x6b851) - hx;
  t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
  t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
  i |= j;
  R = t2 + t1;
  if (i > 0) {
    hfsq = 0.5 * f * f;
    if (k == 0) return f - (hfsq - s * (hfsq + R));
    return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
  }
  if (k == 0) return f - s * (f - R);
  return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}

/* __ieee754_log10(x)
 * Return the base 10 logarithm of x
 *
 * Method :
 * Let log10_2hi = leading 40 bits of log10(2) and
 *     log10_2lo = log10(2) - log10_2hi,
 *     ivln10   = 1/log(10) rounded.
 * Then
 *  n = ilogb(x),
 *  if(n<0)  n = n+1;
 *  x = scalbn(x,-n);
 *  log10(x) := n*log10_2hi + (n*log10_2lo + ivln10*log(x))
 *
 * Note 1:
 * To guarantee log10(10**n)=n, where 10**n is normal, the rounding
 * mode must set to Round-to-Nearest.
 * Note 2:
 * [1/log(10)] rounded to 53 bits has error  .198   ulps;
 * log10 is monotonic at all binary break points.
 *
 * Special cases:
 * log10(x) is NaN with signal if x < 0;
 * log10(+INF) is +INF with no signal; log10(0) is -INF with signal;
 * log10(NaN) is that NaN with no signal;
 * log10(10**N) = N  for N=0,1,...,22. */
static double __ieee754_log10(double x)
{
  double y, z;
  int32_t i, k, hx;
  uint32_t lx;

  GET_DOUBLE_WORDS(hx, lx, x);
  k = 0;
  if (hx < IC(0x00100000U)) { /* x < 2**-1022  */
    if (((hx & IC(0x7fffffffU)) | lx) == 0)
      return -two54 / zero; /* log(+-0)=-inf */
    if (hx < 0)
      return (x - x) / zero; /* log(-#) = NaN */
    k -= 54;
    x *= two54; /* subnormal number, scale up x */
    GET_HIGH_WORD(hx, x);
  }
  if (hx >= IC(0x7ff00000U))
    return x + x;
  k += (hx >> 20) - 1023;
  i = (int32_t)(((uint32_t)k & UC(0x80000000U)) >> 31);
  hx = (hx & IC(0x000fffffU)) | ((0x3ff - i) << 20);
  y = (double) (k + i);
  SET_HIGH_WORD(x, hx);
  z = y * log10_2lo + ivln10 * __ieee754_log(x);
  return z + y * log10_2hi;
}

/* __ieee754_exp(x)
 * Returns the exponential of x.
 *
 * Method
 *   1. Argument reduction:
 *      Reduce x to an r so that |r| <= 0.5*ln2 ~ 0.34658.
 *	Given x, find r and integer k such that
 *
 *               x = k*ln2 + r,  |r| <= 0.5*ln2.  
 *
 *      Here r will be represented as r = hi-lo for better 
 *	accuracy.
 *
 *   2. Approximation of exp(r) by a special rational function on
 *	the interval [0,0.34658]:
 *	Write
 *	    R(r**2) = r*(exp(r)+1)/(exp(r)-1) = 2 + r*r/6 - r**4/360 + ...
 *      We use a special Reme algorithm on [0,0.34658] to generate 
 * 	a polynomial of degree 5 to approximate R. The maximum error 
 *	of this polynomial approximation is bounded by 2**-59. In
 *	other words,
 *	    R(z) ~ 2.0 + P1*z + P2*z**2 + P3*z**3 + P4*z**4 + P5*z**5
 *  	(where z=r*r, and the values of P1 to P5 are listed below)
 *	and
 *	    |                  5          |     -59
 *	    | 2.0+P1*z+...+P5*z   -  R(z) | <= 2 
 *	    |                             |
 *	The computation of exp(r) thus becomes
 *                             2*r
 *		exp(r) = 1 + -------
 *		              R - r
 *                                 r*R1(r)	
 *		       = 1 + r + ----------- (for better accuracy)
 *		                  2 - R1(r)
 *	where
 *			         2       4             10
 *		R1(r) = r - (P1*r  + P2*r  + ... + P5*r   ).
 *	
 *   3. Scale back to obtain exp(x):
 *	From step 1, we have
 *	   exp(x) = 2^k * exp(r)
 *
 * Special cases:
 *	exp(INF) is INF, exp(NaN) is NaN;
 *	exp(-INF) is 0, and
 *	for finite argument, only exp(0)=1 is exact.
 *
 * Accuracy:
 *	according to an error analysis, the error is always less than
 *	1 ulp (unit in the last place).
 *
 * Misc. info.
 *	For IEEE double 
 *	    if x >  7.09782712893383973096e+02 then exp(x) overflow
 *	    if x < -7.45133219101941108420e+02 then exp(x) underflow */
static double __ieee754_exp(double x) 
{
  double y, hi, lo, c, t;
  int32_t k, xsb;
  uint32_t hx;

  GET_HIGH_WORD(hx, x); /* high word of x */
  xsb = (int32_t)((hx >> 31) & 1); /* sign bit of x */
  hx &= IC(0x7fffffffU); /* high word of |x| */

  /* filter out non-finite argument */
  if (hx >= IC(0x40862E42U)) { /* if |x|>=709.78... */
    if (hx >= IC(0x7ff00000U)) {
      GET_LOW_WORD(k, x);
      if (((hx & IC(0xfffffU)) | k) != 0) return x; /* NaN */
      return (xsb == 0) ? x : 0.0; /* exp(+-inf)={inf,0} */
    }
    if (x > o_threshold) { /* overflow */
      feraiseexcept(FE_OVERFLOW);
      return HUGE_VAL;
    }
    if (x < u_threshold) { /* underflow */
      feraiseexcept(FE_UNDERFLOW);
      return 0;
    }
  }

  /* argument reduction */
  if (hx > IC(0x3fd62e42U)) { /* if  |x| > 0.5 ln2 */
    if (hx < IC(0x3FF0A2B2U)) { /* and |x| < 1.5 ln2 */
      hi = x - ln2HI[xsb];
      lo = ln2LO[xsb];
      k = 1 - xsb - xsb;
    } else {
      k = (int32_t)(ivln2 * x + halF[xsb]);
      t = k;
      hi = x - t * ln2HI[0]; /* t*ln2HI is exact here */
      lo = t * ln2LO[0];
    }
    x = hi - lo;
  } else if (hx < IC(0x3e300000U)) { /* when |x|<2**-28 */
    if (hugeval + x > one) return one + x; /* trigger inexact */
    return one;
  } else {
    k = 0;
    lo = 0;
    hi = 0;
  }

  /* x is now in primary range */
  t = x * x;
  c = x - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
  if (k == 0)  return one - ((x * c) / (c - 2.0) - x);
  y = one - ((lo - (x * c) / (2.0 - c)) - hi);
  GET_HIGH_WORD(hx, y);
  if (k >= -1021) {
    hx += (k << IEEE754_DOUBLE_SHIFT); /* add k to y's exponent */
    SET_HIGH_WORD(y, hx);
    return y;
  } else {
    hx += ((k + 1000) << IEEE754_DOUBLE_SHIFT); /* add k to y's exponent */
    SET_HIGH_WORD(y, hx);
    return y * twom1000;
  }
  return __builtin_nan(""); /* to silence WCPL */
}


/* public entries: follow IEEE semantics */

double ceil(double x)
{
  return __ieee754_ceil(x);
}

double floor(double x)
{
  return __ieee754_floor(x);
}

double round(double x)
{
  return __ieee754_round(x);
}

double trunc(double x)
{
  return __ieee754_trunc(x);
}

double frexp(double x, int *eptr)
{
 return __ieee754_frexp(x, eptr);
}

double ldexp(double x, int n)
{
 return __ieee754_scalbn(x, n);
}

double modf(double x, double *iptr)
{
  return __ieee754_modf(x, iptr);
}

double fmod(double x, double y)
{
  return __ieee754_fmod(x, y);
}

double fabs(double x)
{
  return __ieee754_fabs(x);
}

double copysign(double x, double y) 
{
 return __ieee754_copysign(x, y);
}

double sqrt(double x)
{
  return __ieee754_sqrt(x);
}

double pow(double x, double y)
{
  return __ieee754_pow(x, y);
}

double log(double x)
{
  return __ieee754_log(x);
}

double log10(double x)
{
  return __ieee754_log10(x);
}

double exp(double x)
{
  return __ieee754_exp(x);
}

double fmax(double x, double y)
{
  return (isgreaterequal(x, y) || isnan(y)) ? x : y;
}

double fmin(double x, double y)
{
  return (islessequal(x, y) || isnan(y)) ? x : y;
}


/* to be continued.. */
