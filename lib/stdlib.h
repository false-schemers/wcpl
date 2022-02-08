/* General utilities */

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0
#define MB_CUR_MAX 6 /* utf-8 */
/* NULL is built-in */
#define RAND_MAX 0x7fffffff
typedef struct { int quot, rem; } div_t;
typedef struct { long quot, rem; } ldiv_t;
typedef struct { long long quot, rem; } lldiv_t;
/* size_t is built-in */
/* wchar_t is built-in */

extern double atof(const char *s);
extern int atoi(const char *s);
extern long atol(const char *s);
extern long long atoll(const char *s);
extern double strtod(const char *s, char **end);
extern long strtol(const char *s, char **end, int base);
extern long long strtoll(const char *s, char **end, int base);
extern unsigned long strtoul(const char *s, char **end, int base);
extern unsigned long long strtoull(const char *, char **, int base);
extern int rand(void);
extern void srand(unsigned seed);
extern void *calloc(size_t n, size_t sz);
extern void free(void *ptr);
extern void *malloc(size_t sz);
extern void *realloc(void *ptr, size_t sz);
extern void abort(void);
extern int atexit(void (*func)(void));
extern void exit(int status);
extern char *getenv(const char *name);
// system() is not in WASI
extern void *bsearch(const void *key, const void *base, 
 size_t n, size_t sz, int (*) (const void *, const void *));
extern void qsort(const void *base, 
 size_t n, size_t sz, int (*) (const void *, const void *));
extern int abs(int n);
extern long labs(long n);
extern long long llabs(long long n);
extern div_t div(int n, int d);
extern ldiv_t ldiv(long n, long d);
extern lldiv_t lldiv(long long n, long long d);
extern int mblen(const char *s, size_t n);
extern int mbtowc(wchar_t *pwc, const char *s, size_t n);
extern int wctomb(char *s, wchar_t wc);
extern size_t mbstowcs(wchar_t *pwcs, const char *s, size_t n);
extern size_t wcstombs(char *s, const wchar_t *pwcs, size_t n);
