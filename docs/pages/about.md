---
title: About
permalink: /about/
---

![/assets/img/splash.png](/assets/img/splash.png)

WCPL (Webassembly Combined Programming Language) is a standalone self-hosted compiler/linker/libc 
for a subset of C targeting Webassembly and WASI. It supports most traditional pre-C99 C features 
except for full C preprocessor and K&R-style function parameter declarations (modern prototypes 
are fully supported). Features listed below are the ones that are either borrowed from modern 
C dialects, or not implemented in the way described in C90 standard.

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

## Support

If you need help, please don't hesitate to [open an issue](https://www.github.com/{{ site.github_repo }}/{{ site.github_user }}).

