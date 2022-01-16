/* l.h (wcpl library) -- esl */

#ifndef _L_H_INCLUDED
#define _L_H_INCLUDED

#if defined(_MSC_VER) && (_MSC_VER > 1300)
#pragma warning(disable: 4996)
#define strtoull _strtoui64 
#endif

#if !defined(true) && !defined(__cplusplus) && !defined(inline) /* cutil hack */ 
typedef enum { false = 0, true = 1 } bool;
#endif
#if !defined(NELEMS) /* cutil hack */
#define NELEMS(a) (sizeof(a) / sizeof(a[0]))
#endif
#if !defined(MIN) /* cutil hack */
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#if !defined(MAX) /* cutil hack */
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

/* globals */
extern int g_verbosity;

/* common utility functions */
extern void exprintf(const char *fmt, ...);
extern void *exmalloc(size_t n);
extern void *excalloc(size_t n, size_t s);
extern void *exrealloc(void *m, size_t n);
extern char *exstrdup(const char *s);
extern char *strprf(const char *str, const char *prefix);
extern char *strsuf(const char *str, const char *suffix);
extern bool streql(const char *s1, const char *s2);
extern bool strieql(const char *s1, const char *s2);
extern void memswap(void *mem1, void *mem2, size_t sz);
extern unsigned char *utf8(unsigned long c, unsigned char *s);
extern unsigned long unutf8(unsigned char **ps);
extern unsigned long strtou8c(const char *s, char **ep);
extern unsigned long strtocc32(const char *s, char **ep);
extern bool fget8bom(FILE *fp);
#define is8cbyte(c) (((c) & 0x80) != 0)
#define is8chead(c) (((c) & 0xC0) == 0xC0)
#define is8ctail(c) (((c) & 0xC0) == 0x80)
extern int int_cmp(const void *pi1, const void *pi2);

/* dynamic (heap-allocated) 0-terminated strings */
typedef char* dstr_t;
#define dsinit(pds) (*(dstr_t*)(pds) = NULL)
extern void dsicpy(dstr_t* mem, const dstr_t* pds);
#define dsfini(pds) (free(*(dstr_t*)(pds)))
extern void (dsfini)(dstr_t* pds); 
extern void dscpy(dstr_t* pds, const dstr_t* pdss);
extern void dssets(dstr_t* pds, const char *s);
#define dsclear(pds) dssets(pds, NULL)
extern int dstr_cmp(const void *pds1, const void *pds2);

/* simple dynamic memory buffers */
typedef struct buf_tag {
  size_t esz; /* element size in bytes */
  void*  buf; /* data (never NULL) */
  size_t fill; /* # of elements used */
  size_t end; /* # of elements allocated */
} buf_t;
extern buf_t* bufinit(buf_t* pb, size_t esz);
extern buf_t* buficpy(buf_t* mem, const buf_t* pb);
extern void *buffini(buf_t* pb);
extern buf_t mkbuf(size_t esz);
extern buf_t* newbuf(size_t esz);
extern void freebuf(buf_t* pb);
extern size_t buflen(const buf_t* pb);
extern void* bufdata(buf_t* pb);
extern bool bufempty(const buf_t* pb);
extern void bufclear(buf_t* pb);
extern void bufgrow(buf_t* pb, size_t n);
extern void bufresize(buf_t* pb, size_t n);
extern void* bufref(buf_t* pb, size_t i);
extern void bufrem(buf_t* pb, size_t i);
extern void bufnrem(buf_t* pb, size_t i, size_t n);
extern void* bufins(buf_t* pb, size_t i);
extern void *bufbk(buf_t* pb);
extern void *bufnewbk(buf_t* pb);
extern void *bufpopbk(buf_t* pb);
extern void *bufnewfr(buf_t* pb);
extern void *bufalloc(buf_t* pb, size_t n);
extern void bufqsort(buf_t* pb, int (*cmp)(const void *, const void *)); /* unstable */
extern void bufremdups(buf_t* pb, int (*cmp)(const void *, const void *), void (*fini)(void *)); /* adjacent */
extern void* bufsearch(const buf_t* pb, const void *pe, int (*cmp)(const void *, const void *)); /* linear */
extern void* bufbsearch(const buf_t* pb, const void *pe, int (*cmp)(const void *, const void *)); /* binary */
extern size_t bufoff(const buf_t* pb, const void *pe); /* element offset of non-NULL pe inside pb: [0..len] */

/* dstr_t buffers */
#define dsbuf_t buf_t
#define dsbinit(mem) bufinit(mem, sizeof(dstr_t))
extern void dsbicpy(dsbuf_t* mem, const dsbuf_t* pb);
extern void dsbfini(dsbuf_t* pb);
#define dsblen(pb) buflen(pb)
#define dsbref(pb, i) ((dstr_t*)bufref(pb, i))
#define dsbpushbk(pb, pds) dsicpy(bufnewbk(pb), pds)
#define dsbrem(pb, i) do { dsbuf_t *_pb = pb; size_t _i = i; dsfini(bufref(_pb, _i)); bufrem(_pb, _i); } while(0)
#define dsbqsort(pb) bufqsort(pb, dstr_cmp)
#define dsbremdups(pb) bufremdups(pb, dstr_cmp, (dsfini))
#define dsbsearch(pb, pe) bufsearch(pb, pe, dstr_cmp)
#define dsbbsearch(pb, pe) bufbsearch(pb, pe, dstr_cmp)

/* unicode charsets */
#define ucset_t buf_t
#define ucsinit(mem) bufinit(mem, sizeof(unsigned)*2)
#define ucsfini(ps) buffini(ps)
#define mkucs() mkbuf(sizeof(unsigned)*2)
#define ucsempty(ps) bufempty(ps)
extern bool ucsin(unsigned uc, const ucset_t *ps);
extern void ucspushi(ucset_t *ps, unsigned fc, unsigned lc);

/* regular char buffers */
#define chbuf_t buf_t
#define chbinit(mem) bufinit(mem, sizeof(char))
#define chbicpy(mem, pb) buficpy(mem, pb)
#define chbfini(pb) buffini(pb)
#define mkchb() mkbuf(sizeof(char))
#define newchb() newbuf(sizeof(char))
#define freechb(pb) freebuf(pb)
#define chblen(pb) buflen(pb)
#define chbclear(pb) bufclear(pb)
#define chbputc(c, pb) (*(char*)bufnewbk(pb) = (c))
extern void chbput(const char *s, size_t n, chbuf_t* pb);
extern void chbputs(const char *s, chbuf_t* pb);
extern void chbputlc(unsigned long uc, chbuf_t* pb);
extern void chbputwc(wchar_t wc, chbuf_t* pb);
extern void chbputd(int v, chbuf_t* pb);
extern void chbputld(long v, chbuf_t* pb);
extern void chbputt(ptrdiff_t v, chbuf_t* pb);
extern void chbputu(unsigned v, chbuf_t* pb);
extern void chbputlu(unsigned long v, chbuf_t* pb);
extern void chbputllu(unsigned long long v, chbuf_t* pb);
extern void chbputz(size_t v, chbuf_t* pb);
extern void chbputg(double v, chbuf_t* pb);
extern void chbputvf(chbuf_t* pb, const char *fmt, va_list ap);
extern void chbputf(chbuf_t* pb, const char *fmt, ...);
extern char* chbset(chbuf_t* pb, const char *s, size_t n);
extern char* chbsets(chbuf_t* pb, const char *s);
extern char* chbsetf(chbuf_t* pb, const char *fmt, ...);
extern char* chbdata(chbuf_t* pb);
extern dstr_t chbclose(chbuf_t* pb);
extern char* fgetlb(chbuf_t *pcb, FILE *fp);
extern char *wcsto8cb(const wchar_t *wstr, int rc, chbuf_t *pcb);

/* wide char buffers */
extern wchar_t *s8ctowcb(const char *str, wchar_t rc, buf_t *pb);

/* unicode char (unsigned long) buffers */
extern unsigned long *s8ctoucb(const char *str, unsigned long rc, buf_t *pb);

/* symbols */
typedef int sym_t;
#define sym_cmp int_cmp
/* NULL => 0, otherwise sym is positive */
extern sym_t intern(const char *name);
/* 0 => NULL, otherwise returns stored string */
extern const char *symname(sym_t s);
/* uses limited formatter (chbputvf) internally */
extern sym_t internf(const char *fmt, ...);
/* reset symbol table */
extern void clearsyms(void);

#endif /* ndef _L_H_INCLUDED */
