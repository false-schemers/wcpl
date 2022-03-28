/* Date and time */

#pragma once
/* NULL is built-in */
/* size_t is built-in */
#include <sys/types.h>
#include <sys/defs.h>

#define CLOCKS_PER_SEC (1000000000LL)

struct tm {
  int tm_sec;   /* seconds */
  int tm_min;   /* minutes */
  int tm_hour;  /* hours */
  int tm_mday;  /* day of the month */
  int tm_mon;   /* month */
  int tm_year;  /* year */
  int tm_wday;  /* day of the week */
  int tm_yday;  /* day in the year */
  int tm_isdst; /* daylight saving time */
};

extern clock_t clock(void);
extern double difftime(time_t t1, time_t t0);
extern time_t mktime(struct tm *ptm);
extern time_t time(time_t *pt);
extern char *asctime(const struct tm *ptm);
extern char *ctime(const time_t *pt);
extern struct tm *gmtime(const time_t *pt);
extern struct tm *localtime(const time_t *pt);
extern size_t strftime(char *s, size_t maxsz, const char *fmt, const struct tm *ptm);
/* selected POSIX-like additions */
extern void tzset(void); /* relies on TZ env variable */
extern char *tzname[2];
extern long timezone;
extern struct tm *gmtime_r(const time_t *pt, struct tm *ptm);
extern char *ctime_r(const time_t *pt, char *buf);
extern char *asctime_r(const struct tm *ptm, char *buf);
extern struct tm *localtime_r(const time_t *pt, struct tm *ptm);
