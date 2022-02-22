WCPL
====

Compiler for a subset of C targeting Webassembly (not yet functional)

# C features supported

- `#pragma once` in headers
- `static_assert(expr);`, `static_assert(expr, "message");` on top level in headers and module files
- `sizeof(type)`, `alignof(type)`, and `offsetof(type, field)` are supported
- static evaluation of expressions (numerics only in general, numerics+static pointers in top-level initializers)
- limited macros: `#define FOO 1234`, `#define FOO (expr)`, `#define FOO do {...} while (0)` as well as 
  corresponding parameterized forms (`#define FOO(a, b) ...`)   
- `const` and `volatile` specifiers are allowed but ignored
- labels can only label starts/ends of sub-blocks; `goto` can only target labels in the same block or its parent blocks
- assignment of structures/unions (and same-type arrays as well)
- vararg macros and stdarg.h
- object modules can have the following extensions: `.o`, `.wo`
- system object modules are looked up in library directories given via `-L` option and `WCPL_LIBRARY_PATH` environment variable
- system headers included as `#include <foo>` can have the following extensions: (none), `.h`, `.wh`
- system headers are looked up in directories given via `-I` option and `WCPL_INCLUDE_PATH` environment variable
- also, system headers are looked up in `include` sub-directories of library directories as specified above
- user headers included as `#include "foo"` can have the following extensions: (none), `.h`, `.wh`
- user headers are looked up first in current directory, then in system directories as stated above
- user object modules should be provided explicitly as command line arguments
- lookup directories should end in separator char (`/` on Un*x, `\` on Windows), file name is just appended to it

# C features not yet supported

- taking address of a global scalar var (?)
- `{}` initializers for locals
- `static` variables in function scope
- taking address of a function, indirect calls

# C features that won't be supported

- features beyond C90/ANSI-C other than the ones explicitly mentioned above
- `#if`-category directives for conditional compilation
- bit fields
- structures/unions as parameters and return values
- free-form `switch`: nothing but cases in curly braces after test will be supported
- free-form labels and `goto`
- `setjmp`/`longjmp` (not in WASM model)

# Additional WCPL-specific features

- `#pragma module "foo"` in headers

# Libraries already implemented

- `<wasi/api.h>` (header only, implemented by host)
- `<ctype.h>`
- `<string.h>`
- `<errno.h>`
- `<stdarg.h>` (header only)
- `<stdbool.h>` (header only)
- `<stddef.h>` (header only)
- `<stdint.h>` (header only)
- `<inttypes.h>` (header only)
- `<limits.h>` (header only)
 
# Libaries to be implemented

- `<stdlib.h>`
- `<stdio.h>`
- `<time.h>`
- `<math.h>`

# Libaries that won't be supported

- `<locale.h>`
- `<setjmp.h>`
- `<signal.h>`


# Linking

WCPL uses extended WAT format with symbolic identifiers in place of indices as
object file format which uses symbolic names for relocatable constants, so no 
custom sections or relocation tables are needed. Linker's output is regular WAT 
format with no extensions; WASM format will be an option in the future.
 

# Installation

Here's how you can compile WCPL on a unix box; instructions for other
systems/compilers are similar:

```
gcc -o wcpl [wcpl].c 
```


# Use

There are two modes of operation: compiling a single file to object file and
compiling and linking a set of source and/or object files into an executable
file.

## Single-file compilation

```
wcpl -c infile.c -o infile.wo
```

If `-o` option is not specified, output goes to standard output.

## Compilation and Linking

```
wcpl -o out.wat infile1.c infile2.c infile3.wo ...
```

A mix of source and object files can be given; one of the input files should
contain implementation of main() procedure. Library dependences are automatically
loaded and used.






