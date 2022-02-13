/* Diagnostics */

#pragma once

/* this is a regular header */

#define NDEBUG 0
#define assert(e) ((e) ? (void)0 : (void)asm(unreachable))
