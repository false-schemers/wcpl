/* Wide character Input and Output */

#pragma once

/* wchar_t is built-in */
/* wint_t is built-in */

#define WCHAR_MIN (-1-0x7fffffff) /* utf-32 */
#define WCHAR_MAX (0x7fffffff) /* utf-32 */
#define WINT_MIN WCHAR_MIN
#define WINT_MAX WCHAR_MAX

/* no wide-character i/o */
