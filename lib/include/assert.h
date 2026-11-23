/* Diagnostics */

#pragma once

/* this is a regular header */
/* static_assert is built-in */
/* compile with -DNDEBUG to get the NDEBUG version, where assert is a no-op */

#define NDEBUG 0

/* writes "file:line: assertion failed" to stderr, then traps */
extern void __assert_fail(const char *file, int line);
/* set by stdio (if used) to flush its streams before the message */
extern void (*__assert_flush)(void);

#define assert(e) ((e) ? (void)0 : __assert_fail(__FILE__, __LINE__))
