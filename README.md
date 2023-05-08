WCPL
====

Standalone self-hosted compiler/linker/libc for a subset of C targeting Webassembly and WASI

# C features

WCPL supports most traditional pre-C99 C features except for full C preprocessor and K&R-style function parameter declarations (modern prototypes are fully supported). 
Features listed below are the ones that are either borrowed from modern C dialects, or not implemented in the way described in C90 standard.

## Notable C features supported

- `#pragma once` in headers
- `static_assert(expr);`, `static_assert(expr, "message");` on top level in headers and module files
- `sizeof`, `alignof`, and `offsetof` are supported for types and limited expressions
- static evaluation of expressions (numerics only in general, numerics+static pointers in top-level initializers)
- top-level macros: `#define FOO 1234`, `#define FOO (expr)`, `#define FOO do {...} while (0)` as well as 
  corresponding parameterized forms (`#define FOO(a, b) ...`)   
- top-level conditional compilation blocks formed with `#ifdef __WCPL__`, `#ifndef __WCPL__`, `#if 0`, `#if 1`, `#else`, `#endif`
  are allowed and treated as fancy comments; inactive parts can contain any C99 code and properly nested conditional compilation blocks 
- `const` and `volatile` specifiers are allowed but ignored
- variables can be declared at any top-level position in a block
- variables can be declared in the first clause of the `for` statement
- labels can only label starts/ends of sub-blocks; `goto` can only target labels in the same block or its parent blocks
- assignment of structures/unions (and same-type arrays as well)
- generic type diapatch via C11-like `_Generic`/`generic` forms
- vararg macros and `stdarg.h`
- vararg `...` short integer and float arguments implicitly promoted to `int`/`double` respectively; arrays converted to pointers

## C features not yet supported or supported partially

- adjacent mixed-char-size string literals concatenation (works for same-char-size)
- implicit conversions of arrays to element pointers; explicit `&arr[0]` required for now
- implicit conversion of `0` to `NULL` (also currently reported as an error)
- implicit conversion of pointer to boolean in `||`, `&&` and `?:` expressions (now requires explicit `!= NULL`)
- implicit conversion of function pointer to function (reported as an error)
- implicit conversion of longer to shorter integral types (use explicit casts)
- implicit conversion of same-length unsigned to signed integer parameters (use explicit casts)
- taking address of a global scalar var (for now, `&` works for global arrays/structs/unions only)
- non-constant `{}` initializers for locals in function scope
- `static` variables in function scope
- structures/unions/arrays as parameters
- static inline functions in header files
- built-in `__DATE__` and `__TIME__` macros use `gmtime`, not `localtime` 

## C features that won't be supported

- features beyond C90/ANSI-C other than the ones explicitly listed as supported
- full-scale conditional compilation blocks, conditional directives inside WCPL functions, and top-level expressions
- token-based macros (expression-based macros work as described above)
- bit fields
- free-form `switch`: nothing but cases in curly braces after test will be supported
- free-form labels and `goto`
- `setjmp`/`longjmp` (not in WASM model)

## Additional WCPL-specific features

- `#pragma module "foo"` in headers
- `asm` form for inline webassembly

# Libraries

## Libraries already implemented

- `<assert.h>` (C90, header only)
- `<ctype.h>`  (C90)
- `<errno.h>` (C90 + full WASI error list)
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
- `<math.h>` (C90 + some C99 extras)
- `<time.h>` (C90 + some POSIX-like extras)
- `<locale.h>` (stub to allow setting utf8 locale)
- `<sys/types.h>` (header only, internal)
- `<sys/cdefs.h>` (header only, internal)
- `<sys/intrs.h>` (header only, internal -- WASM intrinsics)
- `<sys/stat.h>` (POSIX-like, abridged)
- `<fcntl.h>` (POSIX-like, abridged)
- `<dirent.h>` (POSIX-like, abridged)
- `<unistd.h>` (POSIX-like, abridged)
- `<wasi/api.h>` (header only, implemented by host)
 
## Libaries that won't be supported

- `<setjmp.h>` (no support in WASM)
- `<signal.h>` (no support in WASI)

# Installation

Here's how you can compile WCPL on a Unix box; instructions for other
systems/compilers are similar:

```
cc -o wcpl [wcpl].c 
```

# Modules and compilation environment

- object modules can have the following extensions: `.o`, `.wo`
- system object modules are looked up in library directories specified via `-L` option and `WCPL_LIBRARY_PATH` environment variable
- system headers included as `#include <foo>` can have the following extensions: (none), `.h`, `.wh`
- system headers are looked up in directories given via `-I` option and `WCPL_INCLUDE_PATH` environment variable
- also, system headers are looked up in `include` sub-directories of library directories as specified above
- all bundled libraries (see `lib` folder) are embedded inside the executable, so they need no `-L`/`-I` options
- embedded library files are logged as belonging to `res://` pseudo-directory (a compressed archive inside `l.c`)
- user headers included as `#include "foo"` can have the following extensions: (none), `.h`, `.wh`
- user headers are looked up first in current directory, then in system directories as stated above
- user object modules should be provided explicitly as command line arguments
- lookup directories should end in separator char (`/` on Un*x, `\` on Windows), file name is just appended to it

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
object file format using symbolic names for relocatable constants. This way,
no custom sections or relocation tables are needed.


## Compilation and linking

```
wcpl -o out.wasm infile1.c infile2.c infile3.wo ...
```

Any mix of source and object files can be given; one of the input files should
contain implementation of the `main()` procedure. Library dependences are automatically
loaded and used. If -o file name argument ends in `.wasm`, linker's output will be
a WASM binary; otherwise, the output is in a regular WAT format with no extensions.


## Running executables

WCPL can produce executables in both WASM and WAT format. Some WASM runtimes
such as `wasmtime`* allow running WAT files directly and provide better disgnostics
this way, e.g. symbolic stack traces; for others, WASM format should be used. Please
note that WAT files may be easily converted to WASM format with `wat2wasm`** or similar
tools.

Please read the documentation on your WASM runtime for details on directory/environment
mapping and passing command line arguments.

## Profiling executables

WCPL's executables in WAT format can be profiled via Intel's `vtune` profiler while
running under `wasmtime`* runtime with `--vtune` option. Runtime statistics is dispayed
in terms of the original WCPL functions, so hotspots in WCPL code can be identified.

## Additional optimizations

WASM executables produced by WCPL are quite small but not as fast as ones produced
by industry-scale optimizing compilers such as `clang`. As a rule, executables
produced by WCPL are about as fast as the ones produced by `clang`'s `-O0` mode. 
Fortunately, some advanced optimizations can be applied to WASM output post-factum.
One tool that can be used for this purpose is `wasm-opt` from `bynaryen`*** project:

```
$ wcpl -o foo.wasm foo.c
$ wasm-opt -o foo-opt.wasm -O3 foo.wasm
```

## Self-hosting

Starting with version 1.0, WCPL can compile its own source code and the resulting WASM
executable produces the same results as the original. Example session using `wasmtime`*
runtime may look something like this:

```
$ cc -o wcpl [wcpl].c
$ ./wcpl -q -o wcpl.wasm [wcpl].c
$ wasmtime --dir=. -- wcpl.wasm -q -o wcpl1.wasm [wcpl].c
$ diff -s wcpl.wasm wcpl1.wasm
Files wcpl.wasm and wcpl1.wasm are identical
```

\* available at https://github.com/bytecodealliance/wasmtime/releases

\*\* available at https://github.com/WebAssembly/wabt/releases

\*\*\* available at https://github.com/WebAssembly/binaryen


# Future directions

We plan to add more C features such as local static variables and macros with variable number
of arguments, as well as some popular libraries such as POSIX-compatible regular expressions.

