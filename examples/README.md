Examples
========

Selected examples of C code ported to WCPL. Please note that all examples remain valid C programs.

# Advent

The first Adventure program developed by Willie Crowtherand Don Woods,
translated from Fortran to CWEB by Don Knuth, and then from CWEB to ANSI C 
by Arthur O'Dwyer*. Porting it to WCPL involved the following changes:

- `goto`-based state machine in the main loop changed to `switch`-based one (main change)
- some implicit conversions needed to be made explicit via casts
- implicit conversions of arrays to pointers are made explicit as `&buf[0]`

\* available at https://github.com/Quuxplusone/Advent


