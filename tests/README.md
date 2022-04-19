Tests
=====

Few tests of WCPL compiler/library.

## stdio-tests

Tests of `printf` and `scanf` implementation from the `stdio` module.
Please note that in order to keep library code reasonably small, floating-point
decimal conversions provide only 9 correct digits of precision, and the
tests reflect that.

## math-tests

Tests of numerical routines from the `math` module.
Please note that tests depend on limited-precision decimal conversions, so
test failures do not necessarily mean that math routines are incorrect;
in almost all cases they are correct up to 16th decimal digit.
