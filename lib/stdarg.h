/* Variable arguments */

#pragma once

/* va_arg_t is built-in */
/* va_etc is built-in */
/* va_arg is built-in */

typedef va_arg_t* va_list;
#define va_start(ap, v) (ap = (va_list)va_etc())
#define va_end(ap) ((void)(ap = (va_list)NULL))
#define va_copy(dst, src) ((dst) = (src))

