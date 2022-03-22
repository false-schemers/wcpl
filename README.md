WCPL
====

Standalone compiler/linker/libc for a subset of C targeting Webassembly (mostly functional)

# C features supported

- `#pragma once` in headers
- `static_assert(expr);`, `static_assert(expr, "message");` on top level in headers and module files
- `sizeof(type)`, `alignof(type)`, and `offsetof(type, field)` are supported
- static evaluation of expressions (numerics only in general, numerics+static pointers in top-level initializers)
- `asuint32`, `asfloat`, `asuint64`, `asdouble` reinterpret-cast-style intrinsics
- limited macros: `#define FOO 1234`, `#define FOO (expr)`, `#define FOO do {...} while (0)` as well as 
  corresponding parameterized forms (`#define FOO(a, b) ...`)   
- `const` and `volatile` specifiers are allowed but ignored
- variables can be declared at any top-level position in a block
- labels can only label starts/ends of sub-blocks; `goto` can only target labels in the same block or its parent blocks
- assignment of structures/unions (and same-type arrays as well)
- vararg macros and `stdarg.h`
- object modules can have the following extensions: `.o`, `.wo`
- system object modules are looked up in library directories given via `-L` option and `WCPL_LIBRARY_PATH` environment variable
- system headers included as `#include <foo>` can have the following extensions: (none), `.h`, `.wh`
- system headers are looked up in directories given via `-I` option and `WCPL_INCLUDE_PATH` environment variable
- also, system headers are looked up in `include` sub-directories of library directories as specified above
- user headers included as `#include "foo"` can have the following extensions: (none), `.h`, `.wh`
- user headers are looked up first in current directory, then in system directories as stated above
- user object modules should be provided explicitly as command line arguments
- lookup directories should end in separator char (`/` on Un*x, `\` on Windows), file name is just appended to it

# C and library features not yet supported

- implicit conversions of arrays to element pointers; explicit `&arr[0]` required
- taking address of a global scalar var (?)
- `{}` initializers for locals
- `static` variables in function scope
- structures/unions as parameters
- instrumenting implicit return paths from a non-void function
- hex format of doubles in WAT object and output files
- fixme: non-NULL address expr is not a constant for global var init
- fixme: stdout/stderr are not line-buffered and don't autoflush on exit

# C features that won't be supported

- features beyond C90/ANSI-C other than the ones explicitly listed as supported
- `#if`-category directives for conditional compilation
- token-based macros (expression-based macros work as described above)
- bit fields
- free-form `switch`: nothing but cases in curly braces after test will be supported
- free-form labels and `goto`
- `setjmp`/`longjmp` (not in WASM model)

# Additional WCPL-specific features

- `#pragma module "foo"` in headers

# Libraries already implemented

- `<assert.h>` (header only)
- `<ctype.h>`
- `<dirent.h>` (POSIX-like, abridged)
- `<errno.h>`
- `<fcntl.h>` (POSIX-like, abridged)
- `<fenv.h>` (with WASM limitations)
- `<float.h>` (header only)
- `<inttypes.h>` (header only)
- `<limits.h>` (header only)
- `<stdarg.h>` (header only)
- `<stdbool.h>` (header only)
- `<stddef.h>` (header only)
- `<stdint.h>` (header only)
- `<stdio.h>` (abridged: no `gets`, `tmpfile`, `tmpnam`)
- `<stdlib.h>` (abridged: no `system`)
- `<string.h>`
- `<sys/types.h>` (header only)
- `<sys/defs.h>` (header only, internal)
- `<sys/stat.h>` (POSIX-like, abridged)
- `<unistd.h>` (POSIX-like, abridged)
- `<wasi/api.h>` (header only, implemented by host)
 
# Libaries yet to be implemented

- `<time.h>`
- `<math.h>` (just a few functions for now)

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






