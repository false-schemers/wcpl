---
title: Getting Started
tags: 
 - github
---

# Getting Started

## Installation

There are no prerequisites or dependencies. All you need is a C compiler.
Here's how you can compile WCPL on a Unix box; instructions for other
systems/compilers are similar:

```
cc -o wcpl [wcpl].c 
```

## Libraries

### Libraries already implemented

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
 
### Libaries that won't be supported

- `<setjmp.h>` (no support in WASM)
- `<signal.h>` (no support in WASI)


## Modules

WCPL treats each source file and header file as a module with a separate namespace. The name of the module is derived
from the name of the file, so if a source file is named `foo.c`, it's header file should be named `foo.h`; both
will contribute to module named `foo`. If needed, the name of the module can be specified directly via `#pragma module`
directive. Please note that module names only affect internal naming scheme used by the linker; `#include` directives
have to use actual header file names (slashes in include file names are replaced by dots, so `#include <sys/etc.h>`
will look for a file named `sys.etc.h`). In separate compilation scenario, each source module is compiled to a
single object module as described below.

### Compilation environment

- object modules can have the following extensions: `.o`, `.wo`
- system object modules are looked up in library directories specified via `-L` option and `WCPL_LIBRARY_PATH` environment variable
- system headers included as `#include <foo>` can have the following extensions: (none), `.h`, `.wh`
- system headers are looked up in directories given via `-I` option and `WCPL_INCLUDE_PATH` environment variable
- also, system headers are looked up in `include` sub-directories of library directories as specified above
- user headers included as `#include "foo"` can have the following extensions: (none), `.h`, `.wh`
- user headers are looked up first in current directory, then in system directories as stated above
- user object modules should be provided explicitly as command line arguments
- lookup directories should end in separator char (`/` on Un*x, `\` on Windows), file name is just appended to it


## Use

There are two modes of operation: compiling a single file to object file and
compiling and linking a set of source and/or object files into an executable
file.

### Single-file compilation

```
wcpl -c -o infile.wo infile.c 
```

If `-o` option is not specified, output goes to standard output.
WCPL uses extended WAT format with symbolic identifiers in place of indices as
object file format using symbolic names for relocatable constants. This way,
no custom sections or relocation tables are needed.


### Compilation and linking

```
wcpl -o out.wasm infile1.c infile2.c infile3.wo ...
```

Any mix of source and object files can be given; one of the input files should
contain implementation of the `main()` procedure. Library dependences are automatically
loaded and used. If -o file name argument ends in `.wasm`, linker's output will be
a WASM binary; otherwise, the output is in a regular WAT format with no extensions.


### Running executables

WCPL can produce executables in both WASM and WAT format. Some WASM runtimes
such as `wasmtime`* allow running WAT files directly and provide better disgnostics
this way, e.g. symbolic stack traces; for others, WASM format should be used. Please
note that WAT files may be easily converted to WASM format with `wat2wasm`** or similar
tools.

Please read the documentation on your WASM runtime for details on directory/environment
mapping and passing command line arguments.

### Profiling executables

WCPL's executables in WAT format can be profiled via Intel's `vtune` profiler while
running under `wasmtime`* runtime with `--vtune` option. Runtime statistics is dispayed
in terms of the original WCPL functions, so hotspots in WCPL code can be identified.

### Additional optimizations

WASM executables produced by WCPL are quite small but not as fast as ones produced
by industry-scale optimizing compilers such as `clang`. As a rule, executables
produced by WCPL are about as fast as the ones produced by `clang`'s `-O0` mode. 
Fortunately, some advanced optimizations can be applied to WASM output post-factum.
One tool that can be used for this purpose is `wasm-opt` from `bynaryen`*** project:

```
$ wcpl -L lib/ -o foo.wasm foo.c
$ wasm-opt -o foo-opt.wasm -O3 foo.wasm
```

### Self-hosting

Starting with version 1.0, WCPL can compile its own source code and the resulting WASM
executable produces the same results as the original. Example session using `wasmtime`*
runtime may look something like this:

```
$ cc -o wcpl [wcpl].c
$ ./wcpl -q -L lib/ -o wcpl.wasm [wcpl].c
$ wasmtime --dir=. -- wcpl.wasm -q -L lib/ -o wcpl1.wasm [wcpl].c
$ diff -s wcpl.wasm wcpl1.wasm
Files wcpl.wasm and wcpl1.wasm are identical
```

\* available at https://github.com/bytecodealliance/wasmtime/releases

\*\* available at https://github.com/WebAssembly/wabt/releases

\*\*\* available at https://github.com/WebAssembly/binaryen
