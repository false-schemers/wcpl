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
extern char *strdup(const char *);
extern char *strndup(const char *, size_t);
extern void *memccpy(void *, const void *, int, size_t);
extern void *memmem(const void *, size_t, const void *, size_t);
/* the rest is nonstandard but common */
extern size_t strnlen(const char *s, size_t maxlen);
extern void memswap(void *, void *, size_t);
extern void bzero(void *, size_t);
extern int strcasecmp(const char *, const char *); // POSIX
extern int strncasecmp(const char *, const char *, size_t); // POSIX


