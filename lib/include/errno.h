/* Errors */

#pragma once

#define EDOM   18 /* = WASI ERRNO_DOM */
#define ERANGE 68 /* = WASI ERRNO_RANGE */
#define EILSEQ 25 /* = WASI ERRNO_ILSEQ */

extern int errno;
