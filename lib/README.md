WCPL Library
============

Simplified version of LibC suitable for supporting WCPL code.
Relies on WASI for system interface functionality, but can
be used without it if none of the WASI-dependent functions 
are invoked by the program.

The library is represented as a collection of traditional header
files and (for functionality that can't be implemented directly
in the header file) object modules. Files and modules are coordinated
by module name which serves as the base of the file names for header, 
object, and source files. For traditional header include names that 
have internal slashes, module names contain dots in place of slashes.

Please note that currently WCPL cannot link multiple modules with the
same module name, so please make sure that your source files do not
have names like `stdio.c`. This limitation may be lifted in the future. 


# Modules already implemented

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
- `<locale.h>` (stub to allow setting utf8 locale)
 
# Modules that won't be supported

- `<setjmp.h>`
- `<signal.h>`






