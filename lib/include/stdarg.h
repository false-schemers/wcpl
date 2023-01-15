/* Variable arguments */

#pragma once

/* va_arg is built-in */
/* va_arg_t is built-in as
 * union va_arg { 
 *   int va_int; 
 *   unsigned int va_uint;
 *   long va_long; 
 *   unsigned long va_ulong;
 *   long long va_llong; 
 *   unsigned long long va_ullong;
 *   size_t va_size; 
 *   double va_double;
 *   v128_t va_v128;
 *   void *va_voidptr;
 * } 
 */

typedef va_arg_t* va_list;
#define va_count() (countof(...)) /* WCPL */
#define va_start(ap, v) ((ap) = ...)
#define va_end(ap) ((void)(ap = NULL))
#define va_copy(dst, src) ((dst) = (src))

