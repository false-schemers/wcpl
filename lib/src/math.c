#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <math.h>

/* to be continued.. */

double fabs(double x)
{
	union { double d; uint64_t i; } u;
	u.d = x; u.i &= 0x7FFFFFFFFFFFFFFFULL;
	return u.d;
}
