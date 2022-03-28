#include <wasi/api.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#define TZ_STRLEN_MAX 255
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')
/* 2000-03-01 (mod 400 year, immediately after feb29 */
#define LEAPOCH (946684800LL + 86400*(31+29))
#define DAYS_PER_400Y (365*400 + 97)
#define DAYS_PER_100Y (365*100 + 24)
#define DAYS_PER_4Y   (365*4   + 1)
#define DAYS_PER_WEEK 7
/* year range is from 1901 to 2038, so no century can occur which is not a leap year
 * #define is_leap(y)  ((y) % 4 == 0 && ((y) % 100 != 0 || (y) % 400 == 0)) */
#define is_leap(y)  ((y) % 4 == 0)

static int days_in_month[] = {
  31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 31, 29
};

static const int mon_days[] = {
  31,
  31 + 28,
  31 + 28 + 31,
  31 + 28 + 31 + 30,
  31 + 28 + 31 + 30 + 31,
  31 + 28 + 31 + 30 + 31 + 30,
  31 + 28 + 31 + 30 + 31 + 30 + 31,
  31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
  31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
  31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
  31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
  31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30 + 31
};

static const char *wday_name[7] = {
  "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char *wday_full_name[] = { 
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" 
};

static const char *mon_name[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static const char *mon_full_name[] = { 
  "January", "February", "March", "April", "May", "June", 
  "July", "August", "Sepember", "October", "November", "December" 
};

/* hidden state for non-reentrant functions */
static char asctime_buf[26];
static struct tm gmtime_buf;

/* timezone internals */
static char _tzname[2][TZ_STRLEN_MAX + 1];
static char	_tzval[TZ_STRLEN_MAX + 1];
static int	_tzset = 0;
static char _tzdflt[] = {(char)' ', (char)' ', (char)' ', (char)0};
/* public globals */
long  timezone = 0;
char* tzname[2] = { &_tzdflt[0], &_tzdflt[0] };


time_t time(time_t *pt) 
{
  timestamp_t ts = 0;
  errno_t error = clock_time_get(CLOCKID_REALTIME, CLOCKS_PER_SEC, &ts);
  if (!error) ts /= CLOCKS_PER_SEC; else ts = -1LL;
  if (pt != NULL) *pt = (time_t)ts; 
  return (time_t)ts;
}

clock_t clock(void) 
{
  timestamp_t ts = 0;
  errno_t error = clock_time_get(CLOCKID_PROCESS_CPUTIME_ID, 1LL, &ts);
  if (error) return (clock_t)-1LL;
  return (clock_t)ts;
}

double difftime(time_t t1, time_t t0)
{
  return t1-t0;
}

static int secs_to_tm(long long t, struct tm *ptm)
{
  long long days, secs, years;
  int remdays, remsecs, remyears;
  int qc_cycles, c_cycles, q_cycles;
  int months;
  int wday, yday, leap;

  /* reject time_t values whose year would overflow int */
  if (t < INT_MIN * 31622400LL || t > INT_MAX * 31622400LL)
    return -1;

  secs = t - LEAPOCH;
  days = secs / 86400;
  remsecs = (int)(secs % 86400);
  if (remsecs < 0) {
    remsecs += 86400;
    --days;
  }

  wday = (int)((3+days) % 7);
  if (wday < 0) wday += 7;

  qc_cycles = (int)(days / DAYS_PER_400Y);
  remdays = (int)(days % DAYS_PER_400Y);
  if (remdays < 0) {
    remdays += DAYS_PER_400Y;
    --qc_cycles;
  }

  c_cycles = remdays / DAYS_PER_100Y;
  if (c_cycles == 4) --c_cycles;
  remdays -= c_cycles * DAYS_PER_100Y;

  q_cycles = remdays / DAYS_PER_4Y;
  if (q_cycles == 25) --q_cycles;
  remdays -= q_cycles * DAYS_PER_4Y;

  remyears = remdays / 365;
  if (remyears == 4) --remyears;
  remdays -= remyears * 365;

  leap = !remyears && (q_cycles || !c_cycles);
  yday = remdays + 31 + 28 + leap;
  if (yday >= 365+leap) yday -= 365+leap;

  years = remyears + 4*q_cycles + 100*c_cycles + 400LL*qc_cycles;

  for (months=0; days_in_month[months] <= remdays; months++)
    remdays -= days_in_month[months];

  if (months >= 10) {
    months -= 12;
    ++years;
  }

  if (years+100 > INT_MAX || years+100 < INT_MIN)
    return -1;

  ptm->tm_year = (int)years + 100;
  ptm->tm_mon = months + 2;
  ptm->tm_mday = remdays + 1;
  ptm->tm_wday = wday;
  ptm->tm_yday = yday;

  ptm->tm_hour = remsecs / 3600;
  ptm->tm_min = remsecs / 60 % 60;
  ptm->tm_sec = remsecs % 60;

  return 0;
}

struct tm *gmtime_r(const time_t *pt, struct tm *ptm)
{
  if (secs_to_tm(*pt, ptm) < 0) {
    errno = EOVERFLOW;
    return NULL;
  }
  ptm->tm_isdst = 0;
  return ptm;
}

static const char *get_tz_name(const char *src, char *dest)
{
  int tzlen = 0;
  if (*src != ':') {
    while (*src != '\0' && *src != ',' && *src != '+' && *src != '-' 
        && !ISDIGIT(*src) && tzlen < TZ_STRLEN_MAX) {
      *dest++ = *src++;
      ++tzlen;
    }
  }
  if (tzlen == 0) {
    strcpy(dest, _tzdflt);
  } else {
    *dest = '\0';
  }
  return src;
}

static const char *get_tz_offset(const char *src, int *error)
{
  if (*src == '\0') {
    timezone = 0;
    *error   = 0;
  } else {
    enum { Hours, Minutes, Seconds };
    int tim[3]; tim[0] = tim[1] = tim[2] = 0; /* = { 0, 0, 0 }; */
    long mul = 1L; int i;
    if (*src == '-') {
      mul = -1L;
      ++src;
    } else if (*src == '+') {
      ++src;
    }
    for (i = 0; i < 3; ++i) {  /* sizeof(tim) / sizeof(tim[0]) == 3 */
      if (*src == '\0') { /* invalid */
        mul = 0L;
      } else if (ISDIGIT(*src)) {
        tim[i] = *src++ - '0';
        if (ISDIGIT(*src)) {
          tim[i] = 10 * tim[i] + *src++ - '0';
          if (ISDIGIT(*src)) { /* not more than two digits allowed */
            mul = 0L;
          }
        }
      }
      if (mul == 0L) { /* invalid offset format */
        break;
      } else if (*src == ':') { /* next field */
        ++src;
      } else { /* end of offset information */
        break;
      }
    }
    if (tim[Minutes] > 59 || tim[Seconds] > 59) { /* invalid minutes or seconds */
      mul = 0L;
    }
    timezone = mul * ((long)tim[Seconds] + 60L * (long)tim[Minutes] + 3600L * (long)tim[Hours]);
    *error = (mul == 0L);
  }
  return src;
}

/* tzset() relies on TZ env variable */
void tzset(void)
{
  const char* tzval = getenv("TZ");
  if (tzval == NULL) tzval = "";
  if (!_tzset || strcmp(_tzval, tzval) != 0) {
    int error;
    _tzset = (strlen(tzval) <= TZ_STRLEN_MAX);
    if (_tzset) strcpy(_tzval, tzval);
    tzval = get_tz_name(tzval, _tzname[0]);
    tzval = get_tz_offset(tzval, &error);
    if (error) {
      tzname[0] = _tzdflt;
      tzname[1] = _tzdflt;
    } else {
      tzval = get_tz_name(tzval, _tzname[1]);
      tzname[0] = _tzname[0];
      tzname[1] = _tzname[1];
    }
  }
}

time_t mktime(struct tm *tm)
{
  long tyears, tdays, leaps, utc_hrs;
  int leap_year;

  tzset();
  leap_year = is_leap(tm->tm_year + 1900);
  tyears = tm->tm_year - 70;
  leaps = (tyears + 2) / 4;
  if (tyears < 0) {
    leaps = (tyears - 2) / 4;
    if (leap_year && tm->tm_mon > 1) ++leaps;
  } else {
    leaps = (tyears + 2) / 4; /* no of next two lines until year 2100. */
    if (leap_year && (tm->tm_mon == 0 || tm->tm_mon == 1)) --leaps;
  }
  tdays = (tm->tm_mon > 0) ? mon_days[tm->tm_mon - 1] : 0;
  tdays += tm->tm_mday - 1; /* days of month passed. */
  tdays += (tyears * 365) + leaps;
  utc_hrs = tm->tm_hour;

  return (tdays * 86400L) + (utc_hrs * 3600L) + ((long)tm->tm_min * 60L) + (long)tm->tm_sec + timezone;
}

char *asctime_r(const struct tm *ptm, char *buf)
{
  snprintf(buf, 26, 
    "%s %s%3d %.2d:%.2d:%.2d %d\n",
    wday_name[ptm->tm_wday], mon_name[ptm->tm_mon],
    ptm->tm_mday, ptm->tm_hour,
    ptm->tm_min, ptm->tm_sec,
    1900 + ptm->tm_year);
  return buf;
}

struct tm *localtime_r(const time_t *pt, struct tm *ptm)
{
  time_t t = *pt;
  tzset();
  t -= (time_t)timezone;
  return gmtime_r(&t, ptm);
}

char *ctime_r(const time_t *pt, char *buf)
{
  struct tm tm, *ptm = localtime_r(pt, &tm);
  return ptm ? asctime_r(ptm, buf) : (char*)NULL;
}


/* non-reentrant time utils */

struct tm *gmtime(const time_t *pt)
{
  return gmtime_r(pt, &gmtime_buf);
}

char *asctime(const struct tm *ptm)
{
  return asctime_r(ptm, &asctime_buf[0]);
}

struct tm *localtime(const time_t* pt)
{
  return localtime_r(pt, &gmtime_buf);
}

char *ctime(const time_t *pt)
{
  struct tm *ptm = localtime_r(pt, &gmtime_buf);
  if (!ptm) return NULL;
  return asctime_r(ptm, &asctime_buf[0]);
}


/* formatting */

static int get_week(const struct tm *tp, int start_monday)
{
  int week = tp->tm_yday + DAYS_PER_WEEK;
  if (start_monday) {
    if (tp->tm_wday == 0) week -= DAYS_PER_WEEK - 1;
    else week -= tp->tm_wday - 1;
  } else {
    week -= tp->tm_wday;
  }
  return week / DAYS_PER_WEEK;
}

size_t strftime(char *s, size_t smax, const char *fmt, const struct tm *tp)
{
  char *ptr = s; size_t count = 0;

  tzset();
  do {
    if (*fmt == '%') {
      const char *addstr = NULL;
      int addlen = -1;
      char addval[80];
      int week;
      switch (*++fmt) {
        case 'a':
          addlen = 3; /* abbrevation = first three characters */
          /* fall thru */
        case 'A':
          if (tp->tm_wday < 0 || tp->tm_wday > 6) {
            addstr = "?";
            addlen = -1;
          } else {
            addstr = wday_full_name[tp->tm_wday];
          }
          break;
        case 'b':
          addlen = 3; /* abbrevation = first three characters */
          /* fall thru */
        case 'B':
          if (tp->tm_mon < 0 || tp->tm_mon > 11) {
            addstr = "?";
            addlen = -1;
          } else {
            addstr = mon_full_name[tp->tm_mon];
          }
          break;
        case 'c':
          strftime(addval, 80, "%a %b %d %x %Y", tp);
          break;
        case 'd':
          addval[0] = '0' + (tp->tm_mday / 10);
          addval[1] = '0' + (tp->tm_mday % 10);
          addval[2] = '\0';
          break;
        case 'H':
          addval[0] = '0' + (tp->tm_hour / 10);
          addval[1] = '0' + (tp->tm_hour % 10);
          addval[2] = '\0';
          break;
        case 'I': {
            int hour12 = tp->tm_hour % 12;
            if (hour12 == 0) hour12 = 12;
            addval[0] = '0' + (hour12 / 10);
            addval[1] = '0' + (hour12 % 10);
            addval[2] = '\0';
          }
          break;
        case 'j':
          addval[0] = '0' + ((tp->tm_yday + 1) / 100);
          addval[1] = '0' + (((tp->tm_yday + 1) % 100) / 10);
          addval[2] = '0' + ((tp->tm_yday + 1) % 10);
          addval[3] = '\0';
          break;
        case 'm':
          addval[0] = '0' + ((tp->tm_mon + 1) / 10);
          addval[1] = '0' + ((tp->tm_mon + 1) % 10);
          addval[2] = '\0';
          break;
        case 'M':
          addval[0] = '0' + (tp->tm_min / 10);
          addval[1] = '0' + (tp->tm_min % 10);
          addval[2] = '\0';
          break;
        case 'p':
          if (tp->tm_hour > 0 && tp->tm_hour < 13) addstr = "AM";
          else addstr = "PM";
          break;
        case 'S':
          addval[0] = '0' + (tp->tm_sec / 10);
          addval[1] = '0' + (tp->tm_sec % 10);
          addval[2] = '\0';
          break;
        case 'U':
        case 'W':
          week = get_week(tp, *fmt == 'U' ? 0 : 1);
          addval[0] = '0' + (week / 10);
          addval[1] = '0' + (week % 10);
          addval[2] = '\0';
          break;
        case 'w':
          addval[0] = '0' + (tp->tm_wday / 10);
          addval[1] = '0' + (tp->tm_wday % 10);
          addval[2] = '\0';
          break;
        case 'X':
          strftime(addval, 80, "%d.%m.%Y", tp);
          break;
        case 'x':
          strftime(addval, 80, "%H:%M:%S", tp);
          break;
        case 'y':
          addval[0] = '0' + ((tp->tm_year % 100) / 10);
          addval[1] = '0' + ((tp->tm_year % 100) % 10);
          addval[2] = '\0';
          break;
        case 'Y':
          addval[0] = '0' + ((tp->tm_year + 1900) / 1000);
          addval[1] = '0' + (((tp->tm_year + 1900) % 1000) / 100);
          addval[2] = '0' + (((tp->tm_year + 1900) % 100) / 10);
          addval[3] = '0' + ((tp->tm_year + 1900) % 10);
          addval[4] = '\0';
          break;
        case 'Z':
          addstr = tzname[tp->tm_isdst > 0];
          break;
        case '%':
          addstr = "%";
          break;
        default:
          addval[0] = '%';
          addval[1] = *fmt;
          addval[2] = '\0';
          break;
      }
      if (addstr == NULL) addstr = addval;
      if (*addstr != '\0') {
        size_t len = (addlen < 0) ? strlen(addstr) : addlen;
        if (count + len > smax) len = smax - count;
        strncpy(ptr, addstr, len);
        ptr += len;
        count += len;
      }
      ++fmt;
    } else {
      if (count <= smax) *ptr++ = *fmt;
      ++fmt;
    }
  } while (fmt[-1] != '\0');

  if (count > smax) count = 0;
  return count;
}
