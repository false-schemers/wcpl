Tests
=====

Few tests of WCPL compiler/library.

## stdio-tests

Tests of `printf` and `scanf` implementation from the `stdio` module.
Decimal floating-point conversions are correctly rounded to up to 17
significant digits (positions past the 17th significant digit print as
0), so the expected strings are the ones the C standard and glibc give,
except where glibc prints more than 17 significant digits.

## math-tests

Tests of numerical routines from the `math` module.  Expected values are
exact or correctly rounded; results are accepted within 1 ulp of them
(2 ulp for `tanh`, whose error reaches ~1.5 ulp, as in glibc).  A failure
prints both values and their distance in ulps.

## regress-tests

Regression tests for fixed compiler and library defects: long vs
unsigned comparisons, constant-expression array sizes, pointer casts in
static initializers, typedefs with several declarators, `__LINE__` and
`__FILE__`, `assert`, subnormal literals, `printf` `%e`/`%f`/`%g`,
`strto*` overflow, and `strtod`.  Defects that show up as compile-time
errors (negative array size, overflowing literals, taking the address of
a scalar global in an initializer) cannot be checked by a running test.
