Examples
========

Selected examples of C code ported to WCPL. Please note that all examples remain valid C programs.

## Advent

The first Adventure program developed by Willie Crowtherand Don Woods,
translated from Fortran to CWEB by Don Knuth, and then from CWEB to ANSI C 
by Arthur O'Dwyer*. Porting it to WCPL involved the following changes:

- `goto`-based state machine in the main loop changed to `switch`-based one (main change)
- some implicit conversions needed to be made explicit via casts
- implicit conversions of arrays to pointers are made explicit as `&buf[0]`

\* available at https://github.com/Quuxplusone/Advent

## Pi

Fabrice Bellard's little program for calculation of decimal digits of Pi
that runs in O(n^2) time, but uses very little memory.
Ported by inserting explicit casts from `long long`s to `int`s.

## Calendar

Simple calendar-printing program of unknown pedigree. Ported by moving
static variable out of function scope.

## Sudoku

Sudoku solver, a popular benchmark. Needs to be called with the
accompanying `sudoku.txt` file as standard input. Ported by removing
`inline` keyword, changing `!= 0` pointer comparison to `!= NULL` and 
replacing array arguments such as `int8_t sr[729]` with pointers.


## Binarytrees, Fannkuchredux, Nbody, Spectralnorm

Benchmark programs from the Benchmarks Game site**. Ported by adding
few explicit casts and moving some static variable out of function scope.

\*\* see https://benchmarksgame-team.pages.debian.net/benchmarksgame/

