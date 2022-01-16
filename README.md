# wcpl
Compiler for a subset of C targeting Webassembly

(Not functional yet)

Features supported:

- `#pragma once` in headers
- `#pragma module "foo"` in headers
- `static_assert(expr, "message");` on top level in headers and module files
- `sizeof(type)`, `alignof(type)`, and `offsetof(type, field)` are supported
- limited static evaluation of expressions (int only for now)
- limited macros: `#define FOO 1234`, `#define FOO (expr)` and parameterized forms (`#define FOO(a, b) ...`)   
- headers included as `#include <foo>` can have the following extensions: (none), `.h`, `.wh`
- headers are looked up in directories given via `-L` option and `WCPL_LIBRARY_PATH` environment variable
- lookup directory should end in separator char (`/` on Un*x, `\` on Windows)




