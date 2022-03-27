#include <wasi/api.h>
#include <time.h>

time_t time(time_t *pt) 
{
  timestamp_t ts = 0;
  uint64_t nsps = 1000000000LL; /* NSEC_PER_SEC */ 
  errno_t error = clock_time_get(CLOCKID_REALTIME, nsps, &ts);
  if (!error) ts /= nsps; else ts = -1LL;
  if (pt != NULL) *pt = (time_t)ts; 
  return (time_t)ts;
}
