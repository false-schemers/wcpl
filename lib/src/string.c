#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

void bzero(void *dst, size_t n)
{
  memset(dst, 0, n);
}

void *memccpy(void *dst, const void *src, int c, size_t n)
{
  char *q = dst; const char *p = src;
  char ch;
  while (n--) {
    *q++ = ch = *p++;
    if (ch == (char)c) return q;
  }
  return NULL;
}

void *memchr(const void *s, int c, size_t n)
{
  const char *sp = s;
  while (n--) {
    if (*sp == (char)c) return (void *)sp;
    sp++;
  }
  return NULL;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
  const char *c1 = s1, *c2 = s2;
  int d = 0;
  while (n--) {
    d = (unsigned char)*c1++ - (unsigned char)*c2++;
    if (d) break;
  }
  return d;
}

void *memcpy(void *dst, const void *src, size_t n)
{
  const char *p = src; char *q = dst;
  while (n--) *q++ = *p++;
  return dst;
}

/* See http://www-igm.univ-mlv.fr/~lecroq/string/  */
void *memmem(const void *haystack, size_t n, const void *needle, size_t m)
{
  const unsigned char *y = (const unsigned char *)haystack;
  const unsigned char *x = (const unsigned char *)needle;
  size_t j, k, l;
  if (m > n || !m || !n)
    return NULL;
  if (1 != m) {
    if (x[0] == x[1]) { 
      k = 2; l = 1;
    } else {
      k = 1; l = 2;
    }
    j = 0;
    while (j <= n - m) {
      if (x[1] != y[j + 1]) {
        j += k;
      } else {
        if (!memcmp(x + 2, y + j + 2, m - 2) && x[0] == y[j])
          return (void *)&y[j];
        j += l;
      }
    }
  } else {
    do {
      if (*y == *x) return (void *)y;
      y++;
    } while (--n);
  }
  return NULL;
}

void *memmove(void *dst, const void *src, size_t n)
{
  const char *p = src;
  char *q = dst;
  if (q < p) {
    while (n--) *q++ = *p++;
  } else {
    p += n; q += n;
    while (n--) *--q = *--p;
  }
  return dst;
}

void *memset(void *dst, int c, size_t n)
{
  char *q = dst;
  while (n--) *q++ = c;
  return dst;
}

void memswap(void *m1, void *m2, size_t n)
{
  char *p = m1, *q = m2;
  while (n--) {
    int tmp = *p;
    *p = *q; *q = tmp;
    p++; q++;
  }
}

int strcasecmp(const char *s1, const char *s2)
{
  const unsigned char *c1 = (const unsigned char *)s1;
  const unsigned char *c2 = (const unsigned char *)s2;
  unsigned char ch; int d = 0;
  while (1) {
    d = tolower(ch = *c1++) - tolower(*c2++);
    if (d || !ch) break;
  }
  return d;
}

char *strcat(char *dst, const char *src)
{
  strcpy(strchr(dst, '\0'), src);
  return dst;
}

char *strchr(const char *s, int c)
{
  while (*s != (char)c) {
    if (!*s) return NULL;
    s++;
  }
  return (char *)s;
}

int strcmp(const char *s1, const char *s2)
{
  const unsigned char *c1 = (const unsigned char *)s1;
  const unsigned char *c2 = (const unsigned char *)s2;
  unsigned char ch; int d = 0;
  while (1) {
    d = (int)(ch = *c1++) - (int)*c2++;
    if (d || !ch) break;
  }
  return d;
}

char *strcpy(char *dst, const char *src)
{
  char *q = dst, ch; const char *p = src;
  do { *q++ = ch = *p++; } while (ch);
  return dst;
}

size_t strcspn(const char *s1, const char *s2)
{
  const char *s = s1, *c;
  while (*s1) {
    for (c = s2; *c; c++) {
      if (*s1 == *c) break;
    }
    if (*c) break;
    s1++;
  }
  return s1 - s;
}

char *strdup(const char *s)
{
  size_t l = strlen(s) + 1;
  char *d = malloc(l);
  if (d) memcpy(d, s, l);
  return d;
}

size_t strlen(const char *s)
{
  const char *ss = s;
  while (*ss) ss++;
  return ss - s;
}

int strncasecmp(const char *s1, const char *s2, size_t n)
{
  const unsigned char *c1 = (const unsigned char *)s1;
  const unsigned char *c2 = (const unsigned char *)s2;
  unsigned char ch; int d = 0;
  while (n--) {
    d = toupper(ch = *c1++) - toupper(*c2++);
    if (d || !ch) break;
  }
  return d;
}

char *strncat(char *dst, const char *src, size_t n)
{
  char *q = strchr(dst, '\0');
  const char *p = src;
  char ch;
  while (n--) {
    *q++ = ch = *p++;
    if (!ch)
      return dst;
  }
  *q = '\0';
  return dst;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
  const unsigned char *c1 = (const unsigned char *)s1;
  const unsigned char *c2 = (const unsigned char *)s2;
  unsigned char ch; int d = 0;
  while (n--) {
    d = (int)(ch = *c1++) - (int)*c2++;
    if (d || !ch) break;
  }
  return d;
}

char *strncpy(char *dst, const char *src, size_t n)
{
  char ch, *q = dst; const char *p = src;
  while (n) {
    n--; *q++ = ch = *p++;
    if (!ch) break;
  }
  /* The specs say strncpy() fills the entire buffer with 0 */
  memset(q, 0, n);
  return dst;
}

char *strndup(const char *s, size_t n)
{
  size_t sl = strlen(s), l = (n > sl ? sl : n) + 1;
  char *d = malloc(l);
  if (!d) return NULL;
  memcpy(d, s, l);
  d[n] = '\0';
  return d;
}

size_t strnlen(const char *s, size_t maxlen)
{
  const char *ss = s;
  while ((maxlen > 0) && *ss) { ss++; maxlen--; }
  return ss - s;
}

char *strpbrk(const char *s1, const char *s2)
{
  const char *c = s2;
  if (!*s1) return (char *)NULL;
  while (*s1) {
    for (c = s2; *c; c++) {
      if (*s1 == *c) break;
    }
    if (*c) break;
    s1++;
  }
  if (*c == '\0') s1 = NULL;
  return (char *)s1;
}

char *strrchr(const char *s, int c)
{
  const char *found = NULL;
  while (*s) {
    if (*s == (char)c) found = s;
    s++;
  }
  return (char *)found;
}

size_t strspn(const char *s1, const char *s2)
{
  const char *s = s1, *c;
  while (*s1) {
    for (c = s2; *c; c++) {
      if (*s1 == *c) break;
    }
    if (*c == '\0') break;
    s1++;
  }
  return s1 - s;
}

char *strstr(const char *haystack, const char *needle)
{
  return (char *)memmem(haystack, strlen(haystack), needle,
                        strlen(needle));
}

static char *strsep(char **stringp, const char *delim)
{
  char *s = *stringp, *e;
  if (!s) return NULL;
  e = strpbrk(s, delim);
  if (e) *e++ = '\0';
  *stringp = e;
  return s;
}

char *strtok_r(char *s, const char *delim, char **holder)
{
  if (s) *holder = s;
  do s = strsep(holder, delim); while (s && !*s);
  return s;
}

static char *strtok_holder;

char *strtok(char *s, const char *delim)
{
  char *holder = s ? s : strtok_holder; 
  do s = strsep(&holder, delim); while (s && !*s);
  strtok_holder = holder;
  return s;
}

int strcoll(const char *s1, const char *s2)
{
  /* fake */
  return strcmp(s1, s2);
}

size_t strxfrm(char *dest, const char *src, size_t n)
{
  /* fake */
  strncpy(dest, src, n);
  return strlen(src);
}

const char *strerror(int errno)
{
  if (errno < 0 || errno > ERRNO_MAXERR) return "?unknown error";
  return _emsg[errno];
}