#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>
#include <fenv.h>

/* fake exceptions used below */
#define FE_OVERFLOW 0
#define FE_UNDERFLOW 0
#define FE_INEXACT 0

#define __ieee754_fabs(x) (asdouble(asuint64(x) & 0x7FFFFFFFFFFFFFFFULL))
#define __ieee754_copysign(x, y) (asdouble((asuint64(x) & 0x7FFFFFFFFFFFFFFFULL) | (asuint64(y) & 0x8000000000000000ULL)))

#define __builtin_nan(s) (asdouble(0x7FF8000000000000ULL)) /* +nan */
#define math_force_eval(x) ((void)(x)) /* supposed to raise exception but probably won't */

/* wasm raises no exceptions, so these are straightforward */
#define isgreater(x, y)      ((x) > (y))
#define isgreaterequal(x, y) ((x) >= (y))
#define isless(x, y)         ((x) < (y))
#define islessequal(x, y)    ((x) <= (y))

/* ported from freemint's version of fdlibm */

/* code below is portef feom freemint's version of fdlibm 
 * with the following licensing note:
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
static const double dp_h[] = { 0.0, 5.84962487220764160156e-01 }; /* 0x3FE2B80340000000 */
static const double dp_l[] = { 0.0, 1.35003920212974897128e-08 }; /* 0x3E4CFDEB43CFD006 */
static const double Zero[] = { 0.0, -0.0 };
static const double zero = 0.0;
static const double half = 5.00000000000000000000e-01; /* 0x3FE0000000000000 */
//static const double one = 1.0;
static const double one = 1.00000000000000000000e+00;	/* 0x3FF0000000000000 */
static const double two = 2.0;
static const double two53 = 9007199254740992.0; /* 0x4340000000000000 */
/* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
static const double L1 = 5.99999999999994648725e-01;        /* 0x3FE3333333333303 */
static const double L2 = 4.28571428578550184252e-01;        /* 0x3FDB6DB6DB6FABFF */
static const double L3 = 3.33333329818377432918e-01;        /* 0x3FD55555518F264D */
static const double L4 = 2.72728123808534006489e-01;        /* 0x3FD17460A91D4101 */
static const double L5 = 2.30660745775561754067e-01;        /* 0x3FCD864A93C9DB65 */
static const double L6 = 2.06975017800338417784e-01;        /* 0x3FCA7E284A454EEF */
static const double P1 = 1.66666666666666019037e-01;        /* 0x3FC555555555553E */
static const double P2 = -2.77777777770155933842e-03;       /* 0xBF66C16C16BEBD93 */
static const double P3 = 6.61375632143793436117e-05;        /* 0x3F11566AAF25DE2C */
static const double P4 = -1.65339022054652515390e-06;       /* 0xBEBBBD41C5D26BF1 */
static const double P5 = 4.13813679705723846039e-08;        /* 0x3E66376972BEA4D0 */
static const double lg2 = 6.93147180559945286227e-01;       /* 0x3FE62E42FEFA39EF */
static const double lg2_h = 6.93147182464599609375e-01;     /* 0x3FE62E4300000000 */
static const double lg2_l = -1.90465429995776804525e-09;    /* 0xBE205C610CA86C39 */
static const double ovt = 8.0085662595372944372e-0017;      /* -(1024-log2(ovfl+.5ulp)) */
static const double cp = 9.61796693925975554329e-01;        /* 0x3FEEC709DC3A03FD =2/(3ln2) */
static const double cp_h = 9.61796700954437255859e-01;      /* 0x3FEEC709E0000000 =(float)cp */
static const double cp_l = -7.02846165095275826516e-09;     /* 0xBE3E2FE0145B01F5 =tail of cp_h */
static const double ivln2 = 1.44269504088896338700e+00;     /* 0x3FF71547652B82FE =1/ln2 */
static const double ivln2_h = 1.44269502162933349609e+00;   /* 0x3FF7154760000000 =24b 1/ln2 */
static const double ivln2_l = 1.92596299112661746887e-08;   /* 0x3E54AE0BF85DDF44 =1/ln2 tail */
static const double ln2_hi = 6.93147180369123816490e-01;    /* 0x3FE62E42FEE00000 */
static const double ln2_lo = 1.90821492927058770002e-10;    /* 0x3DEA39EF35793C76 */
static const double Lg1 = 6.666666666666735130e-01;         /* 0x3FE5555555555593 */
static const double Lg2 = 3.999999999940941908e-01;         /* 0x3FD999999997FA04 */
static const double Lg3 = 2.857142874366239149e-01;         /* 0x3FD2492494229359 */
static const double Lg4 = 2.222219843214978396e-01;         /* 0x3FCC71C51D8E78AF */
static const double Lg5 = 1.818357216161805012e-01;         /* 0x3FC7466496CB03DE */
static const double Lg6 = 1.531383769920937332e-01;         /* 0x3FC39A09D078C69F */
static const double Lg7 = 1.479819860511658591e-01;         /* 0x3FC2F112DF3E5244 */
static const double ivln10 = 4.34294481903251816668e-01;    /* 0x3FDBCB7B1526E50E */
static const double log10_2hi = 3.01029995663611771306e-01; /* 0x3FD34413509F6000 */
static const double log10_2lo = 3.69423907715893078616e-13; /* 0x3D59FEF311F12B36 */
/* exp constants */
static const double halF[2] = { 0.5, -0.5 };
static const double twom1000 = 9.33263618503218878990e-302;  /* 2**-1000=0x01700000,0 */
static const double o_threshold = 7.09782712893383973096e+02;  /* 0x40862E42FEFA39EF */
static const double u_threshold = -7.45133219101941108420e+02; /* 0xC0874910D52D3051 */
static const double ln2HI[2] = { /* ln2_hi, -ln2_hi */
  +6.93147180369123816490e-01, /* 0x3FE62E42FEE00000 */
  -6.93147180369123816490e-01  /* 0xBFE62E42FEE00000 */
};
static const double ln2LO[2] = { /* ln2_lo, -ln2_lo */
  +1.90821492927058770002e-10, /* 0x3DEA39EF35793C76 */
  -1.90821492927058770002e-10  /* 0xBDEA39EF35793C76 */
};
/* scaled coefficients related to expm1 */
static const double Q[] = {
  1.0,
  -3.33333333333331316428e-02,		/* BFA11111 111110F4 */
  1.58730158725481460165e-03,			/* 3F5A01A0 19FE5585 */
  -7.93650757867487942473e-05,		/* BF14CE19 9EAADBB7 */
  4.00821782732936239552e-06,			/* 3ED0CFCA 86E65239 */
  -2.01099218183624371326e-07			/* BE8AFDB7 6E09C32D */
};
/* trig constants */
static const double S1 = -1.66666666666666324348e-01;  /* 0xBFC5555555555549 */
static const double S2 = 8.33333333332248946124e-03;   /* 0x3F8111111110F8A6 */
static const double S3 = -1.98412698298579493134e-04;  /* 0xBF2A01A019C161D5 */
static const double S4 = 2.75573137070700676789e-06;   /* 0x3EC71DE357B1FE7D */
static const double S5 = -2.50507602534068634195e-08;  /* 0xBE5AE5E68A2B9CEB */
static const double S6 = 1.58969099521155010221e-10;   /* 0x3DE5D93A5ACFD57C */
static const double C1 = 4.16666666666666019037e-02;	 /* 0x3FA555555555554C */
static const double C2 = -1.38888888888741095749e-03;	 /* 0xBF56C16C16C15177 */
static const double C3 = 2.48015872894767294178e-05;	 /* 0x3EFA01A019CB1590 */
static const double C4 = -2.75573143513906633035e-07;	 /* 0xBE927E4F809C52AD */
static const double C5 = 2.08757232129817482790e-09;	 /* 0x3E21EE9EBDB4B1C4 */
static const double C6 = -1.13596475577881948265e-11;	 /* 0xBDA8FAE9BE8838D4 */
static const double T[] = {
  3.33333333333334091986e-01,			/* 0x3FD5555555555563 */
  1.33333333333201242699e-01,			/* 0x3FC111111110FE7A */
  5.39682539762260521377e-02,			/* 0x3FABA1BA1BB341FE */
  2.18694882948595424599e-02,			/* 0x3F9664F48406D637 */
  8.86323982359930005737e-03,			/* 0x3F8226E3E96E8493 */
  3.59207910759131235356e-03,			/* 0x3F6D6D22C9560328 */
  1.45620945432529025516e-03,			/* 0x3F57DBC8FEE08315 */
  5.88041240820264096874e-04,			/* 0x3F4344D8F2F26501 */
  2.46463134818469906812e-04,			/* 0x3F3026F71A8D1068 */
  7.81794442939557092300e-05,			/* 0x3F147E88A03792A6 */
  7.14072491382608190305e-05,			/* 0x3F12B80F32F0A7E9 */
  -1.85586374855275456654e-05,		/* 0xBEF375CBDB605373 */
  2.59073051863633712884e-05			/* 0x3EFB2A7074BF7AD4 */
};
static const double pio4 = 7.85398163397448278999e-01;		/* 0x3FE921FB54442D18 */
static const double pio4lo = 3.06161699786838301793e-17;	/* 0x3C81A62633145C07 */
static const int init_jk[] = { 2, 3, 4, 6 };	/* initial value for jk */
static const double PIo2[] = {
  1.57079625129699707031e+00,			/* 0x3FF921FB40000000 */
  7.54978941586159635335e-08,			/* 0x3E74442D00000000 */
  5.39030252995776476554e-15,			/* 0x3CF8469880000000 */
  3.28200341580791294123e-22,			/* 0x3B78CC5160000000 */
  1.27065575308067607349e-29,			/* 0x39F01B8380000000 */
  1.22933308981111328932e-36,			/* 0x387A252040000000 */
  2.73370053816464559624e-44,			/* 0x36E3822280000000 */
  2.16741683877804819444e-51			/* 0x3569F31D00000000 */
};
/* Table of constants for 2/pi, 396 Hex digits (476 decimal) of 2/pi */
static const int32_t two_over_pi[] = {
  IC(0xA2F983U), IC(0x6E4E44U), IC(0x1529FCU), IC(0x2757D1U), IC(0xF534DDU), IC(0xC0DB62U),
  IC(0x95993CU), IC(0x439041U), IC(0xFE5163U), IC(0xABDEBBU), IC(0xC561B7U), IC(0x246E3AU),
  IC(0x424DD2U), IC(0xE00649U), IC(0x2EEA09U), IC(0xD1921CU), IC(0xFE1DEBU), IC(0x1CB129U),
  IC(0xA73EE8U), IC(0x8235F5U), IC(0x2EBB44U), IC(0x84E99CU), IC(0x7026B4U), IC(0x5F7E41U),
  IC(0x3991D6U), IC(0x398353U), IC(0x39F49CU), IC(0x845F8BU), IC(0xBDF928U), IC(0x3B1FF8U),
  IC(0x97FFDEU), IC(0x05980FU), IC(0xEF2F11U), IC(0x8B5A0AU), IC(0x6D1F6DU), IC(0x367ECFU),
  IC(0x27CB09U), IC(0xB74F46U), IC(0x3F669EU), IC(0x5FEA2DU), IC(0x7527BAU), IC(0xC7EBE5U),
  IC(0xF17B3DU), IC(0x0739F7U), IC(0x8A5292U), IC(0xEA6BFBU), IC(0x5FB11FU), IC(0x8D5D08U),
  IC(0x560330U), IC(0x46FC7BU), IC(0x6BABF0U), IC(0xCFBC20U), IC(0x9AF436U), IC(0x1DA9E3U),
  IC(0x91615EU), IC(0xE61B08U), IC(0x659985U), IC(0x5F14A0U), IC(0x68408DU), IC(0xFFD880U),
  IC(0x4D7327U), IC(0x310606U), IC(0x1556CAU), IC(0x73A8C9U), IC(0x60E27BU), IC(0xC08C6BU)
};
static const double two24 = 1.67772160000000000000e+07;		/* 0x4170000000000000 */
static const double twon24 = 5.96046447753906250000e-08;	/* 0x3E70000000000000 */
static const int32_t npio2_hw[] = {
  IC(0x3FF921FBU), IC(0x400921FBU), IC(0x4012D97CU), IC(0x401921FBU), IC(0x401F6A7AU), IC(0x4022D97CU),
  IC(0x4025FDBBU), IC(0x402921FBU), IC(0x402C463AU), IC(0x402F6A7AU), IC(0x4031475CU), IC(0x4032D97CU),
  IC(0x40346B9CU), IC(0x4035FDBBU), IC(0x40378FDBU), IC(0x403921FBU), IC(0x403AB41BU), IC(0x403C463AU),
  IC(0x403DD85AU), IC(0x403F6A7AU), IC(0x40407E4CU), IC(0x4041475CU), IC(0x4042106CU), IC(0x4042D97CU),
  IC(0x4043A28CU), IC(0x40446B9CU), IC(0x404534ACU), IC(0x4045FDBBU), IC(0x4046C6CBU), IC(0x40478FDBU),
  IC(0x404858EBU), IC(0x404921FBU)
};
static const double invpio2 = 6.36619772367581382433e-01;	/* 0x3FE45F306DC9C883 */ /* 53 bits of 2/pi */
static const double pio2_1 = 1.57079632673412561417e+00;	/* 0x3FF921FB54400000 */ /* first  33 bit of pi/2 */
static const double pio2_1t = 6.07710050650619224932e-11;	/* 0x3DD0B4611A626331 */ /* pi/2 - pio2_1 */
static const double pio2_2 = 6.07710050630396597660e-11;	/* 0x3DD0B4611A600000 */ /* 33 bit of pi/2 */
static const double pio2_2t = 2.02226624879595063154e-21;	/* 0x3BA3198A2E037073 */ /* pi/2 - (pio2_1+pio2_2) */
static const double pio2_3 = 2.02226624871116645580e-21;	/* 0x3BA3198A2E000000 */ /* third  33 bit of pi/2 */
static const double pio2_3t = 8.47842766036889956997e-32;	/* 0x397B839A252049C1 */ /* pi/2 - (pio2_1+pio2_2+pio2_3) */

/* asin/acos constants */
static const double pi = 3.14159265358979311600e+00;	    /* 0x400921FB54442D18 */
static const double pio2_hi = 1.57079632679489655800e+00;	/* 0x3FF921FB54442D18 */
static const double pio2_lo = 6.12323399573676603587e-17;	/* 0x3C91A62633145C07 */
static const double pio4_hi = 7.85398163397448278999e-01;	/* 0x3FE921FB54442D18 */
/* coefficient for R(x^2) */
static const double pS0 = 1.66666666666666657415e-01;   /* 0x3FC5555555555555 */
static const double pS1 = -3.25565818622400915405e-01;  /* 0xBFD4D61203EB6F7D */
static const double pS2 = 2.01212532134862925881e-01;   /* 0x3FC9C1550E884455 */
static const double pS3 = -4.00555345006794114027e-02;  /* 0xBFA48228B5688F3B */
static const double pS4 = 7.91534994289814532176e-04;   /* 0x3F49EFE07501B288 */
static const double pS5 = 3.47933107596021167570e-05;   /* 0x3F023DE10DFDF709 */
static const double qS1 = -2.40339491173441421878e+00;  /* 0xC0033A271C8A2D4B */
static const double qS2 = 2.02094576023350569471e+00;   /* 0x40002AE59C598AC8 */
static const double qS3 = -6.88283971605453293030e-01;  /* 0xBFE6066C1B8D0159 */
static const double qS4 = 7.70381505559019352791e-02;   /* 0x3FB3B8C5B12E9282 */

/* atan/atan2 constants */
static const double atanhi[] = {
  4.63647609000806093515e-01,			/* atan(0.5)hi 0x3FDDAC670561BB4F */
  7.85398163397448278999e-01,			/* atan(1.0)hi 0x3FE921FB54442D18 */
  9.82793723247329054082e-01,			/* atan(1.5)hi 0x3FEF730BD281F69B */
  1.57079632679489655800e+00			/* atan(inf)hi 0x3FF921FB54442D18 */
};
static const double atanlo[] = {
  2.26987774529616870924e-17,			/* atan(0.5)lo 0x3C7A2B7F222F65E2 */
  3.06161699786838301793e-17,			/* atan(1.0)lo 0x3C81A62633145C07 */
  1.39033110312309984516e-17,			/* atan(1.5)lo 0x3C7007887AF0CBBD */
  6.12323399573676603587e-17			/* atan(inf)lo 0x3C91A62633145C07 */
};
static const double aT[] = {
  3.33333333333329318027e-01,			/* 0x3FD555555555550D */
  -1.99999999998764832476e-01,		/* 0xBFC999999998EBC4 */
  1.42857142725034663711e-01,			/* 0x3FC24924920083FF */
  -1.11111104054623557880e-01,		/* 0xBFBC71C6FE231671 */
  9.09088713343650656196e-02,			/* 0x3FB745CDC54C206E */
  -7.69187620504482999495e-02,		/* 0xBFB3B0F2AF749A6D */
  6.66107313738753120669e-02,			/* 0x3FB10D66A0D03D51 */
  -5.83357013379057348645e-02,		/* 0xBFADDE2D52DEFD9A */
  4.97687799461593236017e-02,			/* 0x3FA97B4B24760DEB */
  -3.65315727442169155270e-02,		/* 0xBFA2B4442C6A6C2F */
  1.62858201153657823623e-02			/* 0x3F90AD3AE322DA11 */
};
static const double pi_o_4 = 7.8539816339744827900E-01;	/* 0x3FE921FB54442D18 */
static const double pi_o_2 = 1.5707963267948965580E+00;	/* 0x3FF921FB54442D18 */
static const double pi_lo = 1.2246467991473531772E-16;	/* 0x3CA1A62633145C07 */
/* hyperbolic trig. constants */
static const double shuge = 1.0e307;


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

/* expm1(x)
 * Returns exp(x)-1, the exponential of x minus 1. */
static double __ieee754_expm1(double x)
{
  double y, hi, lo, c, t, e, hxs, hfx, r1, h2, h4, R1, R2, R3;
  int32_t k, xsb;
  uint32_t hx;

  GET_HIGH_WORD(hx, x);
  xsb = (int32_t)(hx & IC(0x80000000U)); /* sign bit of x */
  if (xsb == 0) y = x;
  else y = -x; /* y = |x| */
  hx &= UC(0x7fffffffU); /* high word of |x| */

  /* filter out hugeval and non-finite argument */
  if (hx >= UC(0x4043687AU)) { /* if |x|>=56*ln2 */
    if (hx >= UC(0x40862E42U)) { /* if |x|>=709.78... */
      if (hx >= UC(0x7ff00000U)) {
        uint32_t low;
        GET_LOW_WORD(low, x);
        if (((hx & UC(0xfffff)) | low) != 0)
          return x + x; /* NaN */
        return (xsb == 0) ? x : -one; /* exp(+-inf)={inf,-1} */
      }
      if (x > o_threshold) {
        feraiseexcept(FE_OVERFLOW);
        return HUGE_VAL; /* overflow */
      }
    }
    if (xsb != 0) { /* x < -56*ln2, return -1.0 with inexact */
      feraiseexcept(FE_INEXACT);	/* raise inexact */
      return -one; /* return -1 */
    }
  }

  /* argument reduction */
  if (hx > UC(0x3fd62e42U)) { /* if  |x| > 0.5 ln2 */
    if (hx < UC(0x3FF0A2B2U)) { /* and |x| < 1.5 ln2 */
      if (xsb == 0) {
        hi = x - ln2_hi;
        lo = ln2_lo;
        k = 1;
      } else {
        hi = x + ln2_hi;
        lo = -ln2_lo;
        k = -1;
      }
    } else {
      k = (int32_t)(ivln2 * x + ((xsb == 0) ? 0.5 : -0.5));
      t = k;
      hi = x - t * ln2_hi; /* t*ln2_hi is exact here */
      lo = t * ln2_lo;
    }
    x = hi - lo;
    c = (hi - x) - lo;
  } else if (hx < UC(0x3c900000U)) { /* when |x|<2**-54, return x */
    t = hugeval + x; /* return x with inexact flags when x!=0 */
    return x - (t - (hugeval + x));
  } else {
    k = 0;
    c = 0;
  }

  /* x is now in primary range */
  hfx = 0.5 * x;
  hxs = x * hfx;
 //DO_NOT_USE_THIS
 // r1 = one + hxs * (Q1 + hxs * (Q2 + hxs * (Q3 + hxs * (Q4 + hxs * Q5))));
 // use this...
  R1 = one + hxs * Q[1];
  h2 = hxs * hxs;
  R2 = Q[2] + hxs * Q[3];
  h4 = h2 * h2;
  R3 = Q[4] + hxs * Q[5];
  r1 = R1 + h2 * R2 + h4 * R3;
 // ... instead
  t = 3.0 - r1 * hfx;
  e = hxs * ((r1 - t) / (6.0 - x * t));
  if (k == 0) {
    return x - (x * e - hxs);		/* c is 0 */
  } else {
    e = (x * (e - c) - c);
    e -= hxs;
    if (k == -1)
      return 0.5 * (x - e) - 0.5;
    if (k == 1) {
      if (x < -0.25)
        return -2.0 * (e - (x + 0.5));
      else
        return one + 2.0 * (x - e);
    }
    if (k <= -2 || k > 56) { /* suffice to return exp(x)-1 */
      uint32_t high;
      y = one - (e - x);
      GET_HIGH_WORD(high, y);
      SET_HIGH_WORD(y, high + (k << 20)); /* add k to y's exponent */
      return y - one;
    }
    t = one;
    if (k < 20) {
      uint32_t high;
      SET_HIGH_WORD(t, UC(0x3ff00000U) - (UC(0x200000U) >> k)); /* t=1-2^-k */
      y = t - (e - x);
      GET_HIGH_WORD(high, y);
      SET_HIGH_WORD(y, high + (k << 20)); /* add k to y's exponent */
    } else {
      uint32_t high;
      SET_HIGH_WORD(t, ((UC(0x3ff) - k) << 20)); /* 2^-k */
      y = x - (e + t);
      y += one;
      GET_HIGH_WORD(high, y);
      SET_HIGH_WORD(y, high + (k << 20)); /* add k to y's exponent */
    }
  }
  return y;
}


/* trigonometric functions */

static double __kernel_sin(double x, double y, int iy)
{
  double z, r, v;
  int32_t ix;

  GET_HIGH_WORD(ix, x);
  ix &= IC(0x7fffffffU); /* high word of x */
  if (ix < IC(0x3e400000U)) { /* |x| < 2**-27 */
    if ((int32_t) x == 0) return x; /* generate inexact */
  }
  z = x * x; v = z * x;
  r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
  if (iy == 0) return x + v * (S1 + z * r);
  return x - ((z * (half * y - v * r) - y) - v * S1);
}

static double __kernel_cos(double x, double y)
{
  double a, hz, z, r, qx;
  int32_t ix;

  GET_HIGH_WORD(ix, x);
  ix &= IC(0x7fffffffU); /* ix = |x|'s high word */
  if (ix < IC(0x3e400000U)) { /* if x < 2**27 */
    if (((int) x) == 0) return one; /* generate inexact */
  }
  z = x * x;
  r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
  if (ix < IC(0x3FD33333U)) { /* if |x| < 0.3 */
    return one - (0.5 * z - (z * r - x * y));
  } else {
    if (ix > IC(0x3fe90000U)) { /* x > 0.78125 */
      qx = 0.28125;
    } else {
      INSERT_WORDS(qx, ix - IC(0x00200000U), 0);	/* x/4 */
    }
    hz = 0.5 * z - qx;
    a = one - qx;
    return a - (hz - (z * r - x * y));
  }
  return __builtin_nan(""); /* to silence WCPL */
}

static double __kernel_tan(double x, double y, int iy)
{
  double z, r, v, w, s;
  int32_t ix, hx;

  GET_HIGH_WORD(hx, x);
  ix = hx & IC(0x7fffffffU); /* high word of |x| */
  if (ix < IC(0x3e300000U)) { /* x < 2**-28 */
    if ((int32_t) x == 0) { /* generate inexact */
      uint32_t low;
      GET_LOW_WORD(low, x);
      if (((ix | low) | (iy + 1)) == 0)
        return one / __ieee754_fabs(x);
      else
        return (iy == 1) ? x : -one / x;
    }
  }
  if (ix >= IC(0x3FE59428U)) { /* |x| >= 0.6744 */
    if (hx < 0) {
      x = -x;
      y = -y;
    }
    z = pio4 - x;
    w = pio4lo - y;
    x = z + w;
    y = 0.0;
  }
  z = x * x;
  w = z * z;
  /* Break x^5*(T[1]+x^2*T[2]+...) into
   * x^5(T[1]+x^4*T[3]+...+x^20*T[11]) +
   * x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12])) */
  r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
  v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
  s = z * x;
  r = y + z * (s * (r + v) + y);
  r += T[0] * s;
  w = x + r;
  if (ix >= IC(0x3FE59428U)) {
    v = (double) iy;
    return (double) (1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r)));
  }
  if (iy != 1) {
    /* if allow error up to 2 ulp, simply return -1.0 / (x+r) here */
    /* compute -1.0 / (x+r) accurately */
    double a, t;
    z = w;
    SET_LOW_WORD(z, 0);
    v = r - (z - x); /* z+v = r+x */
    t = a = -1.0 / w; /* a = -1.0/w */
    SET_LOW_WORD(t, 0);
    s = 1.0 + t * z;
    return t + a * (s + t * v);
  }
  return w;
}

/* __kernel_rem_pio2 returns the last three digits of N with
 *		y = x - N*pi/2 so that |y| < pi/2. */
static int32_t __kernel_rem_pio2(double *x, double *y, int32_t e0, int32_t nx, int prec)
{
  int32_t jz, jx, jv, jp, jk, carry, n, iq[20], i, j, k, m, q0, ih;
  double z, fw, f[20], fq[20], q[20];

  /* initialize jk */
  jk = init_jk[prec];
  jp = jk;

  /* determine jx,jv,q0, note that 3>q0 */
  jx = nx - 1;
  jv = (e0 - 3) / 24;
  if (jv < 0) jv = 0;
  q0 = e0 - 24 * (jv + 1);

  /* set up f[0] to f[jx+jk] where f[jx+jk] = two_over_pi[jv+jk] */
  j = jv - jx;
  m = jx + jk;
  for (i = 0; i <= m; i++, j++)
    f[i] = (j < 0) ? zero : (double) two_over_pi[j];

  /* compute q[0],q[1],...q[jk] */
  for (i = 0; i <= jk; i++) {
    for (j = 0, fw = 0.0; j <= jx; j++)
      fw += x[j] * f[jx + i - j];
    q[i] = fw;
  }

  jz = jk;
  { recompute:
    /* distill q[] into iq[] reversingly */
    for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
      fw = (double) ((int32_t) (twon24 * z));
      iq[i] = (int32_t) (z - two24 * fw);
      z = q[j - 1] + fw;
    }

    /* compute n */
    z = __ieee754_scalbn(z, (int)q0); /* actual value of z */
    z -= 8.0 * __ieee754_floor(z * 0.125); /* trim off integer >= 8 */
    n = (int32_t) z;
    z -= (double) n;
    ih = 0;
    if (q0 > 0) { /* need iq[jz-1] to determine n */
      i = (iq[jz - 1] >> (24 - q0));
      n += i;
      iq[jz - 1] -= i << (24 - q0);
      ih = iq[jz - 1] >> (23 - q0);
    } else if (q0 == 0) {
      ih = iq[jz - 1] >> 23;
    } else if (z >= 0.5) {
      ih = 2;
    }

    if (ih > 0) { /* q > 0.5 */
      n += 1;
      carry = 0;
      for (i = 0; i < jz; i++) { /* compute 1-q */
        j = iq[i];
        if (carry == 0) {
          if (j != 0) {
            carry = 1;
            iq[i] = IC(0x1000000) - j;
          }
        } else
          iq[i] = IC(0xffffff) - j;
      }
      if (q0 > 0) { /* rare case: chance is 1 in 12 */
        switch ((int)q0) {
          case 1:
            iq[jz - 1] &= IC(0x7fffff);
            break;
          case 2:
            iq[jz - 1] &= IC(0x3fffff);
            break;
        }
      }
      if (ih == 2) {
        z = one - z;
        if (carry != 0)
          z -= __ieee754_scalbn(one, (int)q0);
      }
    }

    /* check if recomputation is needed */
    if (z == zero) {
      j = 0;
      for (i = jz - 1; i >= jk; i--)
        j |= iq[i];
      if (j == 0) { /* need recomputation */
        for (k = 1; iq[jk - k] == 0; k++) /* k = no. of terms needed */
          ;
        for (i = jz + 1; i <= jz + k; i++) { /* add q[jz+1] to q[jz+k] */
          f[jx + i] = (double) two_over_pi[jv + i];
          for (j = 0, fw = 0.0; j <= jx; j++)
            fw += x[j] * f[jx + i - j];
          q[i] = fw;
        }
        jz += k;
        goto recompute;
      }
    }
  }
  
  /* chop off zero terms */
  if (z == 0.0) {
    jz -= 1;
    q0 -= 24;
    while (iq[jz] == 0) {
      jz--;
      q0 -= 24;
    }
  } else { /* break z into 24-bit if necessary */
    z = __ieee754_scalbn(z, (int)-q0);
    if (z >= two24) {
      fw = (double) ((int32_t) (twon24 * z));
      iq[jz] = (int32_t) (z - two24 * fw);
      jz += 1;
      q0 += 24;
      iq[jz] = (int32_t) fw;
    } else {
      iq[jz] = (int32_t) z;
    }
  }

  /* convert integer "bit" chunk to floating-point value */
  fw = __ieee754_scalbn(one, (int)q0);
  for (i = jz; i >= 0; i--) {
    q[i] = fw * (double) iq[i];
    fw *= twon24;
  }

  /* compute PIo2[0,...,jp]*q[jz,...,0] */
  for (i = jz; i >= 0; i--) {
    for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++)
      fw += PIo2[k] * q[i + k];
    fq[jz - i] = fw;
  }

  /* compress fq[] into y[] */
  switch (prec) {
    case 0:
      fw = 0.0;
      for (i = jz; i >= 0; i--)
        fw += fq[i];
      y[0] = (ih == 0) ? fw : -fw;
      break;
    case 1:
    case 2: {
        volatile double fv = 0.0;
        for (i = jz; i >= 0; i--)
          fv += fq[i];
        y[0] = (ih == 0) ? fv : -fv;
        fv = fq[0] - fv;
        for (i = 1; i <= jz; i++)
          fv += fq[i];
        y[1] = (ih == 0) ? fv : -fv;
      }
      break;
    case 3:							/* painful */
      for (i = jz; i > 0; i--) {
        volatile double fv = (double) (fq[i - 1] + fq[i]);
        fq[i] += fq[i - 1] - fv;
        fq[i - 1] = fv;
      }
      for (i = jz; i > 1; i--) {
        volatile double fv = (double) (fq[i - 1] + fq[i]);
        fq[i] += fq[i - 1] - fv;
        fq[i - 1] = fv;
      }
      for (fw = 0.0, i = jz; i >= 2; i--)
        fw += fq[i];
      if (ih == 0) {
        y[0] = fq[0];
        y[1] = fq[1];
        y[2] = fw;
      } else {
        y[0] = -fq[0];
        y[1] = -fq[1];
        y[2] = -fw;
      }
  }
  return n & 7;
}

/* __ieee754_rem_pio2(x,y)
 * return the remainder of x rem pi/2 in y[0]+y[1]
 * use __kernel_rem_pio2() */
static int32_t __ieee754_rem_pio2(double x, double *y)
{
  double z, w, t, r, fn;
  double tx[3];
  int32_t e0, i, j, nx, n, ix, hx;
  uint32_t low;

  GET_HIGH_WORD(hx, x); /* high word of x */
  ix = hx & IC(0x7fffffffU);
  if (ix <= IC(0x3fe921fbU)) { /* |x| ~<= pi/4 , no need for reduction */
    y[0] = x;
    y[1] = 0;
    return 0;
  }

  if (ix < IC(0x4002d97cU)) { /* |x| < 3pi/4, special case with n=+-1 */
    if (hx > 0) {
      z = x - pio2_1;
      if (ix != IC(0x3ff921fbU)) { /* 33+53 bit pi is good enough */
        y[0] = z - pio2_1t;
        y[1] = (z - y[0]) - pio2_1t;
      } else { /* near pi/2, use 33+33+53 bit pi */
        z -= pio2_2;
        y[0] = z - pio2_2t;
        y[1] = (z - y[0]) - pio2_2t;
      }
      return 1;
    } else { /* negative x */
      z = x + pio2_1;
      if (ix != IC(0x3ff921fbU)) { /* 33+53 bit pi is good enough */
        y[0] = z + pio2_1t;
        y[1] = (z - y[0]) + pio2_1t;
      } else { /* near pi/2, use 33+33+53 bit pi */
        z += pio2_2;
        y[0] = z + pio2_2t;
        y[1] = (z - y[0]) + pio2_2t;
      }
      return -1;
    }
  }

  if (ix <= IC(0x413921fbU)) { /* |x| ~<= 2^19*(pi/2), medium size */
    t = __ieee754_fabs(x);
    n = (int32_t)(t * invpio2 + half);
    fn = (double)n;
    r = t - fn * pio2_1;
    w = fn * pio2_1t; /* 1st round good to 85 bit */
    if (n < 32 && ix != npio2_hw[n - 1]) {
      y[0] = r - w; /* quick check no cancellation */
    } else {
      uint32_t high;
      j = ix >> IEEE754_DOUBLE_SHIFT;
      y[0] = r - w;
      GET_HIGH_WORD(high, y[0]);
      i = j - (int32_t)((high >> IEEE754_DOUBLE_SHIFT) & IEEE754_DOUBLE_MAXEXP);
      if (i > 16) { /* 2nd iteration needed, good to 118 */
        t = r;
        w = fn * pio2_2;
        r = t - w;
        w = fn * pio2_2t - ((t - r) - w);
        y[0] = r - w;
        GET_HIGH_WORD(high, y[0]);
        i = j - (int32_t)((high >> IEEE754_DOUBLE_SHIFT) & IEEE754_DOUBLE_MAXEXP);
        if (i > 49) { /* 3rd iteration need, 151 bits acc */
          t = r; /* will cover all possible cases */
          w = fn * pio2_3;
          r = t - w;
          w = fn * pio2_3t - ((t - r) - w);
          y[0] = r - w;
        }
      }
    }
    y[1] = (r - y[0]) - w;
    if (hx < 0) {
      y[0] = -y[0];
      y[1] = -y[1];
      return -n;
    } else
      return n;
  }

  /* all other (large) arguments */
  if (ix >= IC(0x7ff00000U)) { /* x is inf or NaN */
    y[0] = y[1] = x - x;
    return 0;
  }
  /* set z = scalbn(|x|,ilogb(x)-23) */
  GET_LOW_WORD(low, x);
  e0 = (ix >> IEEE754_DOUBLE_SHIFT) - (IEEE754_DOUBLE_BIAS + 23);			/* e0 = ilogb(z)-23; */
  INSERT_WORDS(z, ix - (e0 << IEEE754_DOUBLE_SHIFT), low);
  for (i = 0; i < 2; i++) {
    tx[i] = (double) ((int32_t) (z));
    z = (z - tx[i]) * two24;
  }
  tx[2] = z;
  nx = 3;
  while (tx[nx - 1] == zero) --nx; /* skip zero term */
  n = __kernel_rem_pio2(tx, y, e0, nx, 2);
  if (hx < 0) {
    y[0] = -y[0];
    y[1] = -y[1];
    return -n;
  }
  return n;
}

/* sin(x)
 * Return sine function of x. */
static double __ieee754_sin(double x)
{
  double y[2], z = 0.0;
  int32_t n, ix;

  /* High word of x. */
  GET_HIGH_WORD(ix, x);

  /* |x| ~< pi/4 */
  ix &= IC(0x7fffffffU);
  if (ix <= IC(0x3fe921fbU))
    return __kernel_sin(x, z, 0);

  /* sin(Inf or NaN) is NaN */
  else if (ix >= IC(0x7ff00000))
    return x - x;

  /* argument reduction needed */
  else {
    n = __ieee754_rem_pio2(x, y);
    switch ((int)(n & 3)) {
      case 0:
        return __kernel_sin(y[0], y[1], 1);
      case 1:
        return __kernel_cos(y[0], y[1]);
      case 2:
        return -__kernel_sin(y[0], y[1], 1);
      default:
        return -__kernel_cos(y[0], y[1]);
    }
  }
  return __builtin_nan(""); /* to silence WCPL */
}

/* cos(x)
 * Return cosine function of x.*/
static double __ieee754_cos(double x)
{
  double y[2];
  double z = 0.0;
  int32_t n, ix;

  /* High word of x. */
  GET_HIGH_WORD(ix, x);

  /* |x| ~< pi/4 */
  ix &= IC(0x7fffffffU);
  if (ix <= IC(0x3fe921fbU))
    return __kernel_cos(x, z);

  /* cos(Inf or NaN) is NaN */
  else if (ix >= IC(0x7ff00000U))
    return x - x;

  /* argument reduction needed */
  else {
    n = __ieee754_rem_pio2(x, y);
    switch ((int)(n & 3)) {
      case 0:
        return __kernel_cos(y[0], y[1]);
      case 1:
        return -__kernel_sin(y[0], y[1], 1);
      case 2:
        return -__kernel_cos(y[0], y[1]);
      default:
        return __kernel_sin(y[0], y[1], 1);
    }
  }
  return __builtin_nan(""); /* to silence WCPL */
} 

/* tan(x)
 * Return tangent function of x. */
static double __ieee754_tan(double x)
{
  double y[2], z = 0.0;
  int32_t n, ix;

  /* High word of x. */
  GET_HIGH_WORD(ix, x);

  /* |x| ~< pi/4 */
  ix &= IC(0x7fffffffU);
  if (ix <= IC(0x3fe921fbU))
    return __kernel_tan(x, z, 1);

  /* tan(Inf or NaN) is NaN */
  else if (ix >= IC(0x7ff00000U))
    return x - x; /* NaN */

  /* argument reduction needed */
  n = __ieee754_rem_pio2(x, y);
  return __kernel_tan(y[0], y[1], (int)(1 - ((n & 1) << 1)));	/*   1 -- n even -1 -- n odd */
}

static double __ieee754_asin(double x)
{
  double t, w, p, q, c, r, s;
  int32_t hx, ix;

  GET_HIGH_WORD(hx, x);
  ix = hx & IC(0x7fffffffU);
  if (ix >= IC(0x3ff00000U)) { /* |x|>= 1 */
    uint32_t lx;
    GET_LOW_WORD(lx, x);
    if (((ix - IC(0x3ff00000U)) | lx) == 0)
      /* asin(1)=+-pi/2 with inexact */
      return x * pio2_hi + x * pio2_lo;
    return (x - x) / (x - x);		/* asin(|x|>1) is NaN */
  } else if (ix < IC(0x3fe00000U)) { /* |x|<0.5 */
    if (ix < IC(0x3e400000U)) { /* if |x| < 2**-27 */
      if (hugeval + x > one)
        return x; /* return x with inexact if x!=0 */
    } else {
      t = x * x;
      p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
      q = one + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
      w = p / q;
      return x + x * w;
    }
  }
  /* 1> |x|>= 0.5 */
  w = one - __ieee754_fabs(x);
  t = w * 0.5;
  p = t * (pS0 + t * (pS1 + t * (pS2 + t * (pS3 + t * (pS4 + t * pS5)))));
  q = one + t * (qS1 + t * (qS2 + t * (qS3 + t * qS4)));
  s = __ieee754_sqrt(t);
  if (ix >= IC(0x3FEF3333U)) { /* if |x| > 0.975 */
    w = p / q;
    t = pio2_hi - (2.0 * (s + s * w) - pio2_lo);
  } else {
    w = s;
    SET_LOW_WORD(w, 0);
    c = (t - w * w) / (s + w);
    r = p / q;
    p = 2.0 * s * r - (pio2_lo - 2.0 * c);
    q = pio4_hi - 2.0 * w;
    t = pio4_hi - (p - q);
  }
  if (hx > 0) return t;
  return -t;
}

static double __ieee754_acos(double x)
{
  double z, p, q, r, w, s, c, df;
  int32_t hx, ix;
  
  GET_HIGH_WORD(hx, x);
  ix = hx & IC(0x7fffffffU);
  if (ix >= IC(0x3ff00000U)) { /* |x| >= 1 */
    uint32_t lx;
    GET_LOW_WORD(lx, x);
    if (((ix - IC(0x3ff00000U)) | lx) == 0) { /* |x|==1 */
      if (hx > 0)
        return 0.0; /* acos(1) = 0  */
      else
        return pi + 2.0 * pio2_lo; /* acos(-1)= pi */
    }
    return (x - x) / (x - x); /* acos(|x|>1) is NaN */
  }

  if (ix < IC(0x3fe00000U)) { /* |x| < 0.5 */
    if (ix <= IC(0x3c600000U))
      return pio2_hi + pio2_lo; /* if |x| < 2**-57 */
    z = x * x;
    p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
    q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
    r = p / q;
    return pio2_hi - (x - (pio2_lo - x * r));
  } else if (hx < 0) { /* x < -0.5 */
    z = (one + x) * 0.5;
    p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
    q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
    s = __ieee754_sqrt(z);
    r = p / q;
    w = r * s - pio2_lo;
    return pi - 2.0 * (s + w);
  } else { /* x > 0.5 */
    z = (one - x) * 0.5;
    s = __ieee754_sqrt(z);
    df = s;
    SET_LOW_WORD(df, 0);
    c = (z - df * df) / (s + df);
    p = z * (pS0 + z * (pS1 + z * (pS2 + z * (pS3 + z * (pS4 + z * pS5)))));
    q = one + z * (qS1 + z * (qS2 + z * (qS3 + z * qS4)));
    r = p / q;
    w = r * s + c;
    return 2.0 * (df + w);
  }
  return __builtin_nan(""); /* to silence WCPL */
}

static double __ieee754_atan(double x)
{
  double w, s1, s2, z;
  int32_t ix, hx, id;

  GET_HIGH_WORD(hx, x);
  ix = hx & IC(0x7fffffffU);
  if (ix >= IC(0x44100000U)) { /* if |x| >= 2^66 */
    uint32_t low;
    GET_LOW_WORD(low, x);
    if (ix > IC(0x7ff00000) || (ix == IC(0x7ff00000) && (low != 0)))
      return x + x; /* NaN */
    if (hx > 0)
      return atanhi[3] + atanlo[3];
    else
      return -atanhi[3] - atanlo[3];
  }
  if (ix < IC(0x3fdc0000)) { /* |x| < 0.4375 */
    if (ix < IC(0x3e200000)) { /* |x| < 2^-29 */
      if (hugeval + x > one)
        return x; /* raise inexact */
    }
    id = -1;
  } else {
    x = __ieee754_fabs(x);
    if (ix < IC(0x3ff30000)) { /* |x| < 1.1875 */
      if (ix < IC(0x3fe60000)) {/* 7/16 <=|x|<11/16 */
        id = 0;
        x = (2.0 * x - one) / (2.0 + x);
      } else { /* 11/16<=|x|< 19/16 */
        id = 1;
        x = (x - one) / (x + one);
      }
    } else {
      if (ix < IC(0x40038000)) { /* |x| < 2.4375 */
        id = 2;
        x = (x - 1.5) / (one + 1.5 * x);
      } else { /* 2.4375 <= |x| < 2^66 */
        id = 3;
        x = -1.0 / x;
      }
    }
  }
  /* end of argument reduction */
  z = x * x;
  w = z * z;
  /* break sum from i=0 to 10 aT[i]z**(i+1) into odd and even poly */
  s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
  s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
  if (id < 0)  return x - x * (s1 + s2);
  z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
  return (hx < 0) ? -z : z;
}

static double __ieee754_atan2(double y, double x)
{
  double z;
  int32_t k, m, hx, hy, ix, iy;
  uint32_t lx, ly;

  GET_DOUBLE_WORDS(hx, lx, x);
  ix = hx & IC(0x7fffffff);
  GET_DOUBLE_WORDS(hy, ly, y);
  iy = hy & IC(0x7fffffff);
  if (((ix | ((lx | -lx) >> 31)) > IC(0x7ff00000)) || ((iy | ((ly | -ly) >> 31)) > IC(0x7ff00000))) /* x or y is NaN */
    return x + y;
  if (((hx - IC(0x3ff00000)) | lx) == 0) return __ieee754_atan(y); /* x=1.0 */
  m = ((hy >> 31) & 1) | ((hx >> 30) & 2); /* 2*sign(x)+sign(y) */

  /* when y = 0 */
  if ((iy | ly) == 0) {
    switch ((int)m) {
      case 0:
      case 1:
        return y; /* atan(+-0,+anything)=+-0 */
      case 2:
        return pi + tiny; /* atan(+0,-anything) = pi */
      case 3:
        return -pi - tiny; /* atan(-0,-anything) =-pi */
    }
  }
  /* when x = 0 */
  if ((ix | lx) == 0)
    return (hy < 0) ? -pi_o_2 - tiny : pi_o_2 + tiny;

  /* when x is INF */
  if (ix == IC(0x7ff00000U)) {
    if (iy == IC(0x7ff00000U)) {
      switch ((int)m) {
        case 0:
          return pi_o_4 + tiny;	/* atan(+INF,+INF) */
        case 1:
          return -pi_o_4 - tiny;	/* atan(-INF,+INF) */
        case 2:
          return 3.0 * pi_o_4 + tiny;	/*atan(+INF,-INF) */
        case 3:
          return -3.0 * pi_o_4 - tiny;	/*atan(-INF,-INF) */
      }
    } else {
      switch ((int)m) {
        case 0:
          return zero;			/* atan(+...,+INF) */
        case 1:
          return -zero;			/* atan(-...,+INF) */
        case 2:
          return pi + tiny;		/* atan(+...,-INF) */
        case 3:
          return -pi - tiny;		/* atan(-...,-INF) */
      }
    }
  }
  /* when y is INF */
  if (iy == IC(0x7ff00000U))
    return (hy < 0) ? -pi_o_2 - tiny : pi_o_2 + tiny;

  /* compute y/x */
  k = (iy - ix) >> 20;
  if (k > 60)
    z = pi_o_2 + 0.5 * pi_lo; /* |y/x| >  2**60 */
  else if (hx < 0 && k < -60)
    z = 0.0; /* |y|/x < -2**60 */
  else
    z = __ieee754_atan(__ieee754_fabs(y / x)); /* safe to do y/x */
  switch ((int)m) {
    case 0:
      return z; /* atan(+,+) */
    case 1: {
        uint32_t zh;
        GET_HIGH_WORD(zh, z);
        SET_HIGH_WORD(z, zh ^ UC(0x80000000U));
      }
      return z; /* atan(-,+) */
    case 2:
      return pi - (z - pi_lo); /* atan(+,-) */
  }
  /* case 3 */
  return (z - pi_lo) - pi; /* atan(-,-) */
}

static double __ieee754_sinh(double x)
{
  double t, w, h;
  int32_t ix, jx;
  uint32_t lx;

  /* High word of |x|. */
  GET_HIGH_WORD(jx, x);
  ix = jx & IC(0x7fffffffU);

  /* x is INF or NaN */
  if (ix >= IC(0x7ff00000U))
    return x + x;

  h = 0.5;
  if (jx < 0) h = -h;
  /* |x| in [0,22], return sign(x)*0.5*(E+E/(E+1))) */
  if (ix < IC(0x40360000U)) { /* |x|<22 */
    if (ix < IC(0x3e300000U)) /* |x|<2**-28 */
      if (shuge + x > one)
        return x; /* sinh(tiny) = tiny with inexact */
    t = __ieee754_expm1(__ieee754_fabs(x));
    if (ix < IC(0x3ff00000U))
      return h * (2.0 * t - t * t / (t + one));
    return h * (t + t / (t + one));
  }

  /* |x| in [22, log(maxdouble)] return 0.5*exp(|x|) */
  if (ix < IC(0x40862e42U))
    return h * __ieee754_exp(__ieee754_fabs(x));

  /* |x| in [log(maxdouble), overflowthresold] */
  GET_LOW_WORD(lx, x);
  if (ix < IC(0x408633ceU) || (ix == IC(0x408633ceU) && lx <= UC(0x8fb9f87dU))) {
    w = __ieee754_exp(0.5 * __ieee754_fabs(x));
    t = h * w;
    return t * w;
  }

  /* |x| > overflowthresold, sinh(x) overflow */
  return x * shuge;
}

static double __ieee754_cosh(double x)
{
  double t, w;
  int32_t ix;
  uint32_t lx;

  /* High word of |x|. */
  GET_HIGH_WORD(ix, x);
  ix &= IC(0x7fffffffU);

  /* x is INF or NaN */
  if (ix >= IC(0x7ff00000U))
    return x * x;

  /* |x| in [0,22] */
  if (ix < IC(0x40360000U)) {
    /* |x| in [0,0.5*ln2], return 1+expm1(|x|)^2/(2*exp(|x|)) */
    if (ix < IC(0x3fd62e43U)) {
      t = __ieee754_expm1(__ieee754_fabs(x));
      w = one + t;
      if (ix < IC(0x3c800000))
        return w; /* cosh(tiny) = 1 */
      return one + (t * t) / (w + w);
    }

    /* |x| in [0.5*ln2,22], return (exp(|x|)+1/exp(|x|)/2; */
    t = __ieee754_exp(__ieee754_fabs(x));
    return half * t + half / t;
  }

  /* |x| in [22, log(maxdouble)] return half*exp(|x|) */
  if (ix < IC(0x40862E42U))
    return half * __ieee754_exp(__ieee754_fabs(x));

  /* |x| in [log(maxdouble), overflowthresold] */
  GET_LOW_WORD(lx, x);
  if (ix < IC(0x408633ceU) || (ix == IC(0x408633ceU) && lx <= UC(0x8fb9f87dU))) {
    w = __ieee754_exp(half * __ieee754_fabs(x));
    t = half * w;
    return t * w;
  }

  /* |x| > overflowthresold, cosh(x) overflow */
  return hugeval * hugeval;
}

static double __ieee754_tanh(double x)
{
  double t, z;
  int32_t jx, ix, lx;

  /* High word of |x|. */
  GET_DOUBLE_WORDS(jx, lx, x);
  ix = jx & IC(0x7fffffffU);

  /* x is INF or NaN */
  if (ix >= IC(0x7ff00000)) {
    if (jx >= 0)
      return one / x + one; /* tanh(+-inf)=+-1 */
    else
      return one / x - one; /* tanh(NaN) = NaN */
  }

  /* |x| < 22 */
  if (ix < IC(0x40360000)) { /* |x|<22 */
    if ((ix | lx) == 0)
      return x; /* x == +-0 */
    if (ix < IC(0x3c800000)) /* |x|<2**-55 */
      return x * (one + x); /* tanh(small) = small */
    if (ix >= IC(0x3ff00000)) { /* |x|>=1  */
      t = __ieee754_expm1(two * __ieee754_fabs(x));
      z = one - two / (t + two);
    } else {
      t = __ieee754_expm1(-two * __ieee754_fabs(x));
      z = -t / (t + two);
    }
    /* |x| > 22, return +-1 */
  } else {
    z = one - tiny; /* raised inexact flag */
  }
  return (jx >= 0) ? z : -z;
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

double expm1(double x)
{ 
  return __ieee754_expm1(x);
}

double fmax(double x, double y)
{
  return (isgreaterequal(x, y) || isnan(y)) ? x : y;
}

double fmin(double x, double y)
{
  return (islessequal(x, y) || isnan(y)) ? x : y;
}

double sin(double x)
{
  return __ieee754_sin(x);
}

double cos(double x)
{
  return __ieee754_cos(x);
}

double tan(double x)
{
  return __ieee754_tan(x);
}

double asin(double x)
{
  return __ieee754_asin(x);
}

double acos(double x)
{
  return __ieee754_acos(x);
}

double atan(double x)
{
  return __ieee754_atan(x);
}

double atan2(double y, double x)
{
  return __ieee754_atan2(y, x);
}

double sinh(double x)
{ 
  return __ieee754_sinh(x);
}

double cosh(double x)
{ 
  return __ieee754_cosh(x);
}

double tanh(double x)
{ 
  return __ieee754_tanh(x);
}

