WCPL
====

Compiler for a subset of C targeting Webassembly (not yet functional)

# C features supported

- `#pragma once` in headers
- `#pragma module "foo"` in headers
- `static_assert(expr);`, `static_assert(expr, "message");` on top level in headers and module files
- `sizeof(type)`, `alignof(type)`, and `offsetof(type, field)` are supported
- limited static evaluation of expressions (int only for now)
- limited macros: `#define FOO 1234`, `#define FOO (expr)`, `#define FOO do {...} while (0)` as well as 
  corresponding parameterized forms (`#define FOO(a, b) ...`)   
- headers included as `#include <foo>` can have the following extensions: (none), `.h`, `.wh`
- headers are looked up in directories given via `-L` option and `WCPL_LIBRARY_PATH` environment variable
- lookup directory should end in separator char (`/` on Un*x, `\` on Windows), file name is just appended to it

# C features not yet supported

- vararg functions


# C features that won't be supported

- features beyond C99 other than the ones explicitly mentioned above
- `#if`-category directives for conditional compilation
- bit fields
- free-form `switch`: nothing but cases in curly braces after test will be supported
- free-form labels and `goto`
- `setjmp`/`longjmp` (not in WASM model)


# Installation

Here's how you can compile WCPL on a unix box; instructions for other
systems/compilers are similar:

```
gcc -o wcpl [wcpl].c 
```






