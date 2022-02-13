/* String handling */

#pragma once

extern void *memcpy(void *dst, const void *src, size_t n);
extern void *memmove(void *, const void *, size_t);
extern char *strcpy(char *dst, const char *src);
extern char *strncpy(char *, const char *, size_t);
extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, size_t);
extern int memcmp(const void *s1, const void *s2, size_t n);
extern int strcmp(const char *, const char *);
extern int strcoll(const char *s1, const char *s2);
extern int strncmp(const char *, const char *, size_t);
extern size_t strxfrm(char *dest, const char *src, size_t n);
extern void *memchr(const void *s, int c, size_t n);
extern char *strchr(const char *s, int c);
extern size_t strcspn(const char *, const char *);
extern char *strpbrk(const char *, const char *);
extern char *strrchr(const char *, int);
extern size_t strspn(const char *, const char *);
extern char *strstr(const char *, const char *);
extern char *strtok(char *, const char *);
extern char *strtok_r(char *, const char *, char **);
extern void *memset(void *dst, int c, size_t n);
/* todo: add char *strerror(int errno); */
extern size_t strlen(const char *s);
/* nonstandard but common */
extern char *strdup(const char *s);
extern char *strndup(const char *s, size_t n);
extern void *memccpy(void *d, const void *s, int c, size_t n);
extern void *memmem(const void *, size_t, const void *, size_t);
extern size_t strnlen(const char *s, size_t maxlen);
extern void memswap(void *s1, void *s2, size_t n);
extern void bzero(void *s, size_t n);
extern int strcasecmp(const char *s1, const char *s2); // POSIX
extern int strncasecmp(const char *s1, const char *s2, size_t n); // POSIX


