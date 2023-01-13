/* Variable arguments */

#pragma once

/* va_arg_t is built-in */
/* va_arg is built-in */

typedef va_arg_t* va_list;
#define va_count() (countof(...)) /* WCPL */
#define va_start(ap, v) ((ap) = ...)
#define va_end(ap) ((void)(ap = NULL))
#define va_copy(dst, src) ((dst) = (src))

