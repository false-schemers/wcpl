WCPL
====

Standalone compiler/linker/libc for a subset of C targeting Webassembly and WASI

# C features supported

- `#pragma once` in headers
- `static_assert(expr);`, `static_assert(expr, "message");` on top level in headers and module files
- `sizeof(type)`, `alignof(type)`, and `offsetof(type, field)` are supported
- static evaluation of expressions (numerics only in general, numerics+static pointers in top-level initializers)
- `asuint32`, `asfloat`, `asuint64`, `asdouble` reinterpret-cast-style intrinsics
- top-level macros: `#define FOO 1234`, `#define FOO (expr)`, `#define FOO do {...} while (0)` as well as 
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

# C and library features not yet supported or supported partially

- adjacent string literals concatenation
- `sizeof(expr)`, `alignof(expr)`, and `offsetof(expr, field)`
- implicit conversions of arrays to element pointers; explicit `&arr[0]` required for now
- implicit promotions of char and short types as vararg ... arguments (for now it's reported as an error)
- implicit conversion of `0` to `NULL` (also currently reported as an error)
- implicit conversion of pointer to boolean (now requires explicit `!= NULL`)
- implicit conversion of function pointer to function (reported as an error)
- taking address of a global scalar var (for now, `&` works for global arrays/structs/unions only)
- `{}` initializers for locals in function scope
- `static` variables in function scope
- structures/unions/arrays as parameters
- static inline functions in header files
- built-in `__DATE__` and `__TIME__` macros (use `gmtime` for now) 

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

- `<assert.h>` (C90, header only)
- `<ctype.h>`  (C90)
- `<dirent.h>` (POSIX-like, abridged)
- `<errno.h>` (C90 + full WASI error list)
- `<fcntl.h>` (POSIX-like, abridged)
- `<fenv.h>` (with WASM limitations)
- `<float.h>` (C90, header only)
- `<inttypes.h>` (C99, header only)
- `<limits.h>` (C90, header only)
- `<stdarg.h>` (C90, header only)
- `<stdbool.h>` (C99, header only)
- `<stddef.h>` (C90, header only)
- `<stdint.h>` (C99, header only)
- `<stdio.h>` (C90, abridged: no `gets`, `tmpfile`, `tmpnam`)
- `<stdlib.h>` (C90, abridged: no `system`)
- `<string.h>` (C90 + some POSIX-like extras)
- `<sys/types.h>` (header only, internal)
- `<sys/cdefs.h>` (header only, internal)
- `<sys/stat.h>` (POSIX-like, abridged)
- `<unistd.h>` (POSIX-like, abridged)
- `<wasi/api.h>` (header only, implemented by host)
- `<math.h>` (C90 + some C99 extras)
- `<time.h>` (C90 + some POSIX-like extras)
 
# Libaries that won't be supported

- `<locale.h>`
- `<setjmp.h>`
- `<signal.h>`

# Installation

Here's how you can compile WCPL on a Unix box; instructions for other
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
wcpl -c -o infile.wo infile.c 
```

If `-o` option is not specified, output goes to standard output.
WCPL uses extended WAT format with symbolic identifiers in place of indices as
object file format which uses symbolic names for relocatable constants, so no 
custom sections or relocation tables are needed.


## Compilation and linking

```
wcpl -o out.wat infile1.c infile2.c infile3.wo ...
```

Any mix of source and object files can be given; one of the input files should
contain implementation of the `main()` procedure. Library dependences are automatically
loaded and used. Linker's output is regular WAT format with no extensions; 
WASM format will be an option in the future.


## Running executables

Currently, WCPL produces output executables in WAT format. Some WASM runtimes
such as `wasmtime`* allow running WAT files directly (and provide better disgnostics
this way, e.g. symbolic stack traces); for others, WAT files should be first converted 
to WASM format with `wat2wasm`** or similar tools.

Please read the documentation on your WASM runtime for details on directory
mapping and passing command line arguments.

\* available at https://github.com/bytecodealliance/wasmtime/releases

\*\* available at https://github.com/WebAssembly/wabt/releases



