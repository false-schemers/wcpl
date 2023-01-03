/* l.h (wcpl library) -- esl */

#pragma once

#define STR_az "abcdefghijklmnopqrstuvwxyz"
#define STR_AZ "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define STR_09 "0123456789"

/* argument parsing and usage */
extern  void  setprogname(const char *s);
extern  const char *progname(void);
extern  void  setusage(const char *s);
extern  const char *usage(void);
extern  void  eusage(const char *fmt, ...);
extern  const char *cutillit(void);
/* warning levels */
extern  void  setwlevel(int l);
extern  int   getwlevel(void);
/* logging */
extern  void  setquietness(int q);
extern  int   getquietness(void);
extern  void  incquietness(void);
extern  void  logenf(int n, const char *fmt, ...);
extern  void  logef(const char *fmt, ...);
extern  void  llogef(const char *fmt, ...);
extern  void  lllogef(const char *fmt, ...);
/* verbosity (debug-style logging) */
extern  void  setverbosity(int n);
extern  int   getverbosity(void);
extern  void  incverbosity(void);
extern  void  verbosenf(int n, const char *fmt, ...);
extern  void  verbosef(const char *fmt, ...);
extern  void  vverbosef(const char *fmt, ...);
extern  void  vvverbosef(const char *fmt, ...);
/* AT&T-like option parser */
extern  int   eoptind, eopterr, eoptopt, eoptich;
extern  char  *eoptarg;
extern  int   egetopt(int argc, char* argv[], const char* opts);
extern  void  eoptreset(void); /* reset getopt globals */

/* common utility functions */
extern void exprintf(const char *fmt, ...);
extern void *exmalloc(size_t n);
extern void *excalloc(size_t n, size_t s);
extern void *exrealloc(void *m, size_t n);
extern char *exstrdup(const char *s);
extern char *exstrndup(const char* s, size_t n);
extern char *strtrc(char *str, int c, int toc);
extern char *strprf(const char *str, const char *prefix);
extern char *strsuf(const char *str, const char *suffix);
extern bool streql(const char *s1, const char *s2);
extern bool strieql(const char *s1, const char *s2);
extern void memswap(void *mem1, void *mem2, size_t sz);
extern unsigned char *utf8(unsigned long c, unsigned char *s);
extern unsigned long unutf8(unsigned char **ps);
extern unsigned long strtou8c(const char *s, char **ep);
extern unsigned long strtocc32(const char *s, char **ep, bool *rp);
extern unsigned long strtou8cc32(const char *s, char **ep, bool *rp);
extern bool fget8bom(FILE *fp);
#define is8cbyte(c) (((c) & 0x80) != 0)
#define is8chead(c) (((c) & 0xC0) == 0xC0)
#define is8ctail(c) (((c) & 0xC0) == 0x80)
extern int int_cmp(const void *pi1, const void *pi2);

/* floating-point reinterpret casts and exact hex i/o */
extern unsigned long long as_uint64(double f); /* NB: asuint64 is WCPL intrinsic */
extern double as_double(unsigned long long u); /* NB: asdouble is WCPL intrinsic */
extern unsigned as_uint32(float f); /* NB: asuint32 is WCPL intrinsic */
extern float as_float(unsigned u); /* NB: asfloat is WCPL intrinsic */
extern char *udtohex(unsigned long long uval, char *buf); /* buf needs 32 chars */
extern unsigned long long hextoud(const char *buf); /* -1 on error */
extern char *uftohex(unsigned uval, char *buf); /* buf needs 32 chars */
extern unsigned hextouf(const char *buf); /* -1 on error */

/* dynamic (heap-allocated) 0-terminated strings */
typedef char* dstr_t;
#define dsinit(pds) (*(dstr_t*)(pds) = NULL)
extern void dsicpy(dstr_t* mem, const dstr_t* pds);
extern void dsfini(dstr_t* pds); 
extern void dscpy(dstr_t* pds, const dstr_t* pdss);
extern void dssets(dstr_t* pds, const char *s);
#define dsclear(pds) (dssets(pds, NULL))
extern int dstr_cmp(const void *pds1, const void *pds2);

/* simple dynamic memory buffers */
typedef struct buf {
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
extern void  bufpopfr(buf_t* pb);
extern void *bufalloc(buf_t* pb, size_t n);
extern void bufrev(buf_t* pb);
extern void bufcpy(buf_t* pb, const buf_t* pab);
extern void bufcat(buf_t* pb, const buf_t* pab);
extern void bufqsort(buf_t* pb, int (*cmp)(const void *, const void *)); /* unstable */
extern void bufremdups(buf_t* pb, int (*cmp)(const void *, const void *), void (*fini)(void *)); /* adjacent */
extern void* bufsearch(const buf_t* pb, const void *pe, int (*cmp)(const void *, const void *)); /* linear */
extern void* bufbsearch(const buf_t* pb, const void *pe, int (*cmp)(const void *, const void *)); /* binary */
extern size_t bufoff(const buf_t* pb, const void *pe); /* element offset of non-NULL pe inside pb: [0..len] */
extern void bufswap(buf_t* pb1, buf_t* pb2);

/* dstr_t buffers */
typedef buf_t dsbuf_t;
#define dsbinit(mem) (bufinit(mem, sizeof(dstr_t)))
extern void dsbicpy(dsbuf_t* mem, const dsbuf_t* pb);
extern void dsbfini(dsbuf_t* pb);
#define dsblen(pb) (buflen(pb))
#define dsbref(pb, i) ((dstr_t*)bufref(pb, i))
#define dsbpushbk(pb, pds) (dsicpy(bufnewbk(pb), pds))
#define dsbrem(pb, i) do { dsbuf_t *_pb = pb; size_t _i = i; dsfini(bufref(_pb, _i)); bufrem(_pb, _i); } while(0)
#define dsbqsort(pb) (bufqsort(pb, dstr_cmp))
#define dsbremdups(pb) (bufremdups(pb, dstr_cmp, (dsfini)))
#define dsbsearch(pb, pe) (bufsearch(pb, pe, dstr_cmp))
#define dsbbsearch(pb, pe) (bufbsearch(pb, pe, dstr_cmp))

/* unicode charsets */
typedef buf_t ucset_t;
#define ucsinit(mem) (bufinit(mem, sizeof(unsigned)*2))
#define ucsfini(ps) (buffini(ps))
#define mkucs() (mkbuf(sizeof(unsigned)*2))
#define ucsempty(ps) (bufempty(ps))
extern bool ucsin(unsigned uc, const ucset_t *ps);
extern void ucspushi(ucset_t *ps, unsigned fc, unsigned lc);

/* regular char buffers */
typedef buf_t chbuf_t;
#define chbinit(mem) (bufinit(mem, sizeof(char)))
#define chbicpy(mem, pb) (buficpy(mem, pb))
#define chbfini(pb) (buffini(pb))
#define mkchb() (mkbuf(sizeof(char)))
#define newchb() (newbuf(sizeof(char)))
#define freechb(pb) (freebuf(pb))
#define chblen(pb) (buflen(pb))
#define chbclear(pb) (bufclear(pb))
#define chbputc(c, pb) (*(char*)bufnewbk(pb) = (char)(c))
extern void chbput(const char *s, size_t n, chbuf_t* pcb);
extern void chbputs(const char *s, chbuf_t* pcb);
extern void chbputlc(unsigned long uc, chbuf_t* pcb);
extern void chbputwc(wchar_t wc, chbuf_t* pcb);
extern void chbputd(int v, chbuf_t* pcb);
extern void chbputld(long v, chbuf_t* pcb);
extern void chbputt(ptrdiff_t v, chbuf_t* pcb);
extern void chbputu(unsigned v, chbuf_t* pcb);
extern void chbputx(unsigned v, chbuf_t* pcb);
extern void chbputlu(unsigned long v, chbuf_t* pcb);
extern void chbputllu(unsigned long long v, chbuf_t* pcb);
extern void chbputll(long long v, chbuf_t* pcb);
extern void chbputz(size_t v, chbuf_t* pcb);
extern void chbputg(double v, chbuf_t* pcb);
extern void chbputvf(chbuf_t* pcb, const char *fmt, va_list ap);
extern void chbputf(chbuf_t* pcb, const char *fmt, ...);
extern void chbput4le(unsigned v, chbuf_t* pcb);
extern void chbputtime(const char *fmt, const struct tm *tp, chbuf_t* pcb);
extern void chbinsc(chbuf_t* pcb, size_t n, int c);
extern void chbinss(chbuf_t* pcb, size_t n, const char *s);
extern char* chbset(chbuf_t* pcb, const char *s, size_t n);
extern char* chbsets(chbuf_t* pcb, const char *s);
extern char* chbsetf(chbuf_t* pcb, const char *fmt, ...);
extern char* chbdata(chbuf_t* pcb);
extern void chbcpy(chbuf_t* pdcb, const chbuf_t* pscb);
extern void chbcat(chbuf_t* pdcb, const chbuf_t* pscb);
extern dstr_t chbclose(chbuf_t* pcb);
extern int chbuf_cmp(const void *p1, const void *p2);
extern char* fgetlb(chbuf_t *pcb, FILE *fp);
extern char *wcsto8cb(const wchar_t *wstr, int rc, chbuf_t *pcb);

/* wide char buffers */
extern wchar_t *s8ctowcb(const char *str, wchar_t rc, buf_t *pb);

/* unicode char (unsigned long) buffers */
extern unsigned long *s8ctoucb(const char *str, unsigned long rc, buf_t *pb);


/* grow pcb to get to the required alignment */
extern void binalign(chbuf_t* pcb, size_t align);
/* lay out numbers as little-endian binary into cbuf */
extern void binchar(int c, chbuf_t* pcb);  /* align=1 */
extern void binshort(int s, chbuf_t* pcb); /* align=2 */
extern void binint(int i, chbuf_t* pcb);   /* align=4 */
extern void binllong(long long ll, chbuf_t* pcb); /* align=8 */
extern void binuchar(unsigned uc, chbuf_t* pcb);  /* align=1 */
extern void binushort(unsigned us, chbuf_t* pcb); /* align=2 */
extern void binuint(unsigned ui, chbuf_t* pcb);   /* align=4 */
extern void binullong(unsigned long long ull, chbuf_t* pcb); /* align=8 */
extern void binfloat(float f, chbuf_t* pcb);   /* align=4 */
extern void bindouble(double d, chbuf_t* pcb); /* align=8 */


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

/* path name components */
/* returns trailing file name */
extern char *getfname(const char *path);
/* returns file base (up to, but not including last .) */
extern size_t spanfbase(const char* path);
/* returns trailing file extension ("" or ".foo") */
extern char* getfext(const char* path);
