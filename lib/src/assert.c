#include <wasi/api.h>
#include <assert.h>

/* Run-time support for assert(); kept free of stdio so that a program
 * using assert does not have to link printf and friends.  If the program
 * does use stdio, stdio sets __assert_flush so that output written before
 * the failure is not lost. */

void (*__assert_flush)(void) = NULL;

static void writeall(const char *s, size_t len)
{
  ciovec_t iov; size_t written;
  while (len > 0) {
    iov.buf = (const uint8_t *)s; iov.buf_len = len;
    if (fd_write(2, &iov, 1, &written) != 0 || written == 0) break;
    s += written; len -= written;
  }
}

void __assert_fail(const char *file, int line)
{
  char num[16]; char *pn = &num[16]; size_t len; unsigned u;
  if (__assert_flush != NULL) (*__assert_flush)();
  for (len = 0; file[len]; ++len);
  writeall(file, len);
  u = line < 0 ? 0 : (unsigned)line;
  do { *--pn = (char)('0' + u % 10); u /= 10; } while (u > 0);
  *--pn = ':';
  writeall(pn, (size_t)(&num[16] - pn));
  writeall(": assertion failed\n", 19);
  (void)asm(unreachable); /* trap, so that the runtime can show a backtrace */
}
