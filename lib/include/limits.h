/* Implementation limits */

#pragma once

#define CHAR_BIT 8
#define CHAR_MAX 127
#define CHAR_MIN (-128)
#define MB_LEN_MAX 4 /* utf-8 */
#define INT_MAX  0x7fffffff
#define INT_MIN  (-1-0x7fffffff)
#define LONG_MAX INT_MAX /* wasm32 */
#define LONG_MIN INT_MIN /* wasm32 */
#define LLONG_MAX  0x7fffffffffffffffLL
#define LLONG_MIN (-LLONG_MAX-1LL)
#define SCHAR_MAX 127
#define SCHAR_MIN (-128)
#define SHRT_MAX  0x7fff
#define SHRT_MIN  (-1-0x7fff)
#define UCHAR_MAX 255
#define USHRT_MAX 0xffff
#define UINT_MAX 0xffffffffU
#define ULONG_MAX UINT_MAX /* wasm32 */
#define ULLONG_MAX  0xffffffffffffffffULL
