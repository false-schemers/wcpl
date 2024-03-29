/* p.c (wcpl parser) -- esl */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <wchar.h>
#include <time.h>
#include <math.h>
#include "l.h"
#include "w.h"
#include "p.h"
#include "c.h"


/* module names and files */

sym_t base_from_path(const char *path)
{
  cbuf_t cb = mkcb(); char *fn = getfname(path);
  sym_t bp = intern(cbset(&cb, path, fn-path));
  cbfini(&cb);
  return bp;
}

sym_t modname_from_path(const char *path)
{
  cbuf_t cb = mkcb(); char *fn = getfname(path);
  sym_t mod = intern(cbset(&cb, fn, spanfbase(fn)));  
  cbfini(&cb);
  return mod;
}

pws_t *pws_from_modname(bool sys, sym_t mod)
{
  pws_t *pws = NULL;
  if (mod) {
    size_t i; cbuf_t cb = mkcb();
    if (!sys) { /* start with current directory */
      pws = newpws(cbsetf(&cb, "%s", symname(mod)));
      if (!pws) pws = newpws(cbsetf(&cb, "%s.h", symname(mod)));
      if (!pws) pws = newpws(cbsetf(&cb, "%s.wh", symname(mod)));
    }
    /* search include base directories */
    if (!pws) for (i = 0; i < buflen(g_ibases); ++i) {
      sym_t *pb = bufref(g_ibases, i);
      pws = newpws(cbsetf(&cb, "%s%s", symname(*pb), symname(mod)));
      if (pws) break;
      pws = newpws(cbsetf(&cb, "%s%s.h", symname(*pb), symname(mod)));
      if (pws) break;
      pws = newpws(cbsetf(&cb, "%s%s.wh", symname(*pb), symname(mod)));
      if (pws) break;
    }
    if (pws) logef("# found '%s' module interface in %s (pws #%d)\n", symname(mod), cbdata(&cb), pwsid(pws));
    cbfini(&cb);
  }
  return pws;
}

/* parser workspaces */
struct pws {
  int id;             /* sequential id of this pws or -1 */
  dstr_t infile;      /* current input file name or "-" */
  sym_t curmod;       /* current module name or 0 */
  void *input;        /* current input stream */
  fgetlb_t getlb;     /* function to read from it */
  fclose_t close;     /* and close it afterwards */
  bool inateof;       /* current input is exausted */  
  cbuf_t incb;        /* input buffer for parser */  
  buf_t lsposs;       /* line start positions */  
  size_t discarded;   /* count of discarded chars */  
  cbuf_t chars;       /* line buffer of chars */
  int curi;           /* current input position in buf */
  bool gottk;         /* lookahead token is available */
  tt_t ctk;           /* lookahead token type */
  cbuf_t token;       /* lookahead token char data */
  char *tokstr;       /* lookahead token string */
  int pos;            /* absolute pos of la token start */
  int cclevel;        /* nesting of active cc blocks */
}; /* pws_t */

int pwsid(pws_t *pw)
{
  return pw->id;
}

sym_t pwscurmod(pws_t *pw)
{
  return pw->curmod;
}

static buf_t g_pwsbuf;

/* alloc parser workspace for infile */
pws_t *newpws(const char *infile)
{
  FILE *fp = NULL; MEM *mp = NULL;
  const char *mpath; pws_t *pw = NULL;
  if (streql(infile, "-")) fp = stdin;
  else if ((mpath = strprf(infile, "res://")) != NULL) mp = mopen(mpath);
  else fp = fopen(infile, "r");
  if (fp) {
    pw = emalloc(sizeof(pws_t));
    pw->input = fp;
    pw->getlb = (fgetlb_t)&fgetlb;
    pw->close = (fclose_t)&fclose;
    fget8bom(pw->input);
  } else if (mp) {
    pw = emalloc(sizeof(pws_t));
    pw->input = mp;
    pw->getlb = (fgetlb_t)&mgetlb;
    pw->close = (fclose_t)&mclose;
  }
  /* init all remaining fields */
  if (pw) {
    pw->id = (int)buflen(&g_pwsbuf);
    pw->infile = estrdup(infile);
    pw->curmod = streql(infile, "-") ? intern("_") : modname_from_path(infile);
    pw->inateof = false;
    cbinit(&pw->incb);
    bufinit(&pw->lsposs, sizeof(size_t));
    pw->discarded = 0;
    bufinit(&pw->chars, sizeof(char));
    pw->curi = 0; 
    pw->gottk = false;
    pw->ctk = TT_EOF;
    cbinit(&pw->token);
    pw->tokstr = NULL;
    pw->pos = 0;
    pw->cclevel = 0;
    *(pws_t**)bufnewbk(&g_pwsbuf) = pw;
  }
  return pw;
}

/* close existing workspace, leaving only data used for error reporting */
void closepws(pws_t *pw)
{
  if (pw) {
    if (pw->input != NULL) {
      if (pw->close != (fclose_t)&fclose || pw->input != stdin) (*pw->close)(pw->input);
      pw->input = NULL;
    }
    cbclear(&pw->incb);
    cbclear(&pw->token);
  }
}

static void freepws(pws_t *pw)
{
  if (pw) {
    closepws(pw);
    free(pw->infile);
    cbfini(&pw->incb);
    buffini(&pw->lsposs);
    cbfini(&pw->chars);
    cbfini(&pw->token);
    free(pw);
  }
}

void init_workspaces(void)
{
  bufinit(&g_pwsbuf, sizeof(pws_t*));
}

void fini_workspaces(void)
{
  size_t i;
  for (i = 0; i < buflen(&g_pwsbuf); ++i) {
    freepws(*(pws_t**)bufref(&g_pwsbuf, i));
  }
  buffini(&g_pwsbuf);
}

/* convert global position into 1-based line + 0-based offset */
static void pos2lnoff(pws_t *pw, int gci, int *pln1, int *poff0)
{
  /* todo: use binary search */
  size_t i; int curlc = 0, curci = 0;
  gci += (int)pw->discarded; /* in case we are in a repl */
  for (i = 0; i < buflen(&pw->lsposs); ++i) {
    size_t *plsp = bufref(&pw->lsposs, i);
    int ici = (int)*plsp;
    if (ici > gci) break;
    curlc = (int)i; 
    curci = ici;
  }
  *pln1 = curlc + 1;
  *poff0 = gci - curci;
}

/* recall line #n; return NULL if it is not available */
static char *recallline(pws_t *pw, int lno, cbuf_t *pcb)
{
  size_t gci, ppi; cbuf_t *pp = &pw->chars; 
  char *pchars = cbdata(pp);
  assert(lno > 0);
  /* check if this line is already comitted in full */
  if (lno >= (int)(1+buflen(&pw->lsposs))) return NULL;
  /* ok, let's get its global initial offset as gci */
  gci = *(size_t *)bufref(&pw->lsposs, (size_t)(lno-1));
  /* see if this part is still available */
  if (gci < pw->discarded) return NULL; 
  gci -= pw->discarded; /* now gci is relative to pp */
  assert(gci <= buflen(pp));
  /* collect the characters */
  cbclear(pcb);
  for (ppi = gci; ppi <= cblen(pp); ++ppi) {
    int c = pchars[ppi];
    if (c == '\n') break;
    cbputc(c, pcb);
  }
  return cbdata(pcb);
}

/* try to get new line of input so parsing can continue */
static int fetchline(pws_t *pw, char **ptbase, int *pendi)
{
  cbuf_t *pp = &pw->chars; int c = EOF;
  if (!pw->inateof) {
    char *line = (*pw->getlb)(&pw->incb, pw->input);
    if (!line) {
      pw->inateof = true;
    } else {
      size_t pos = buflen(pp) + pw->discarded;
      unsigned long ch = 0; char *ln = line; 
      while (*ln) {
        ch = unutf8((unsigned char **)&ln);
        if (ch == 0 || ch == (unsigned long)-1) {
          int lno = (int)buflen(&pw->lsposs)+1;
          eprintf("%s:%d: utf-8 encoding error", pw->infile, lno);
        }
      }
      cbputs(line, pp); 
      if (ch == '\\') pp->fill -= 1; /* c preprocessor-like behavior */
      else cbputc('\n', pp); /* normal line not ending in backslash */
      if (c == EOF) c = '\n';
      *(size_t*)bufnewbk(&pw->lsposs) = pos;
      *ptbase = cbdata(pp);
      *pendi = (int)cblen(pp);
    }
  }
  return c;  
}

#if 0
/* readjust input after previous read; return true unless at EOF */
static bool clearinput(pws_t *pw)
{
  /* readjust and repeat unless at eof */
  if (!pw->inateof) {
    /* continuing after successful parse, its associated info is no longer needed */
    /* throw away chars before curi -- but not after the beginning of the line where curi
     * is located as we don't want to lose the ability to show current line as error line;
     * also we still need to keep track of discarded chars for position calculations */
    cbuf_t *pp = &pw->chars; 
    size_t nl = buflen(&pw->lsposs);
    size_t lspos = nl ? *(size_t*)bufref(&pw->lsposs, nl-1): 0;  /* abs pos of last line's start */
    if (lspos < pw->discarded) {
      /* pp contains partial line; no reason to keep it */
      pw->discarded += pw->curi;
      bufnrem(pp, 0, pw->curi);
      pw->curi = 0;
    } else {
      /* pp contains full last line; throw away chars before its start only */
      size_t rc = lspos - pw->discarded;
      pw->discarded += rc;
      bufnrem(pp, 0, rc); assert(pw->curi >= (int)rc);
      pw->curi -= (int)rc;
    }
    return true;
  }
  return false;
}
#endif

/* report error, possibly printing location information */
static void vrprintf(pws_t *pw, int startpos, const char *fmt, va_list args)
{
  cbuf_t cb = mkcb(); const char *s;
  int ln = 0, off = 0; assert(fmt);
  if (pw != NULL && pw->infile != NULL) cbputf(&cb, "%s:", pw->infile);
  if (pw && startpos >= 0) {
    pos2lnoff(pw, startpos, &ln, &off);
    if (ln > 0) cbputf(&cb, "%d:%d:", ln, off+1);
  }
  fflush(stdout); 
  if (cblen(&cb) > 0) {
    cbputc(' ', &cb);
    fputs(cbdata(&cb), stderr);
  }
  vfprintf(stderr, fmt, args); 
  fputc('\n', stderr);
  if (startpos >= 0 && ln > 0 && (s = recallline(pw, ln, &cb)) != NULL) {
    fputs(s, stderr); fputc('\n', stderr);
    while (off-- > 0) fputc(' ', stderr);
    fputs("^\n", stderr);
  }
  fflush(stderr); 
  cbfini(&cb);
}

/* report error, possibly printing location information, and exit */
void reprintf(pws_t *pw, int startpos, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt); 
  vrprintf(pw, startpos, fmt, args); 
  va_end(args); 
  exit(1);
}


/* symbol table */

/* triples of <sym_t, tt_t, info> sorted by sym */
buf_t g_syminfo;
/* nodes referred to by some syminfos */
ndbuf_t g_nodes;

/* NB: intern_symbol does not check for duplicates! */
static void intern_symbol(const char *name, tt_t tt, int info)
{
  int *pt = bufnewbk(&g_syminfo);
  pt[0] = intern(name), pt[1] = tt, pt[2] = info;
  bufqsort(&g_syminfo, &int_cmp);
}

/* NB: unintern_symbol does not touch g_nodes! */
static void unintern_symbol(const char *name)
{
  sym_t s = intern(name);
  int *pi = bufbsearch(&g_syminfo, &s, &int_cmp);
  if (pi) bufrem(&g_syminfo, bufoff(&g_syminfo, pi));
}

size_t sizeof_vararg_union = 16;
void make_vararg_union_type(node_t *ptn)
{
  node_t *pfn;
  assert(ptn->nt == NT_NULL);
  ptn->nt = NT_TYPE; ptn->ts = TS_UNION; ptn->name = intern("va_arg");
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_int"); 
  ndsettype(ndnewbk(pfn), TS_INT);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_uint"); 
  ndsettype(ndnewbk(pfn), TS_UINT);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_long"); 
  ndsettype(ndnewbk(pfn), TS_LONG);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_ulong"); 
  ndsettype(ndnewbk(pfn), TS_ULONG);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_llong"); 
  ndsettype(ndnewbk(pfn), TS_LLONG);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_ullong"); 
  ndsettype(ndnewbk(pfn), TS_ULLONG);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_double"); 
  ndsettype(ndnewbk(pfn), TS_DOUBLE);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_size"); 
  ndsettype(ndnewbk(pfn), TS_ULONG);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_v128"); 
  ndsettype(ndnewbk(pfn), TS_V128);
  ndset(pfn = ndnewbk(ptn), NT_VARDECL, -1, -1)->name = intern("va_voidptr"); 
  ndsettype(ndnewbk(ndsettype(ndnewbk(pfn), TS_PTR)), TS_VOID);
}

void init_symbols(void)
{
  node_t *pn, *psn; time_t now;
  bufinit(&g_syminfo, sizeof(int)*3);
  ndbinit(&g_nodes); time(&now);
  intern_symbol("asm", TT_ASM_KW, -1); /* WCPL */
  intern_symbol("auto", TT_AUTO_KW, -1);
  intern_symbol("break", TT_BREAK_KW, -1);
  intern_symbol("case", TT_CASE_KW, -1);
  intern_symbol("char", TT_CHAR_KW, -1);
  intern_symbol("const", TT_CONST_KW, -1);
  intern_symbol("constexpr", TT_CONSTEXPR_KW, -1);
  intern_symbol("continue", TT_CONTINUE_KW, -1);
  intern_symbol("default", TT_DEFAULT_KW, -1);
  intern_symbol("do"	, TT_DO_KW, -1);
  intern_symbol("double", TT_DOUBLE_KW, -1);
  intern_symbol("else"	, TT_ELSE_KW, -1);
  intern_symbol("enum"	, TT_ENUM_KW, -1);
  intern_symbol("extern", TT_EXTERN_KW, -1);
  intern_symbol("float", TT_FLOAT_KW, -1);
  intern_symbol("for", TT_FOR_KW, -1);
  intern_symbol("goto", TT_GOTO_KW, -1);
  intern_symbol("if", TT_IF_KW, -1);
  intern_symbol("inline", TT_INLINE_KW, -1);
  intern_symbol("int", TT_INT_KW, -1);
  intern_symbol("long", TT_LONG_KW, -1);
  intern_symbol("register", TT_REGISTER_KW, -1);
  intern_symbol("restrict", TT_RESTRICT_KW, -1);
  intern_symbol("return", TT_RETURN_KW, -1);
  intern_symbol("short", TT_SHORT_KW, -1);
  intern_symbol("signed", TT_SIGNED_KW, -1);
  intern_symbol("simd", TT_SIMD_KW, -1); /* WCPL */
  intern_symbol("static", TT_STATIC_KW, -1);
  intern_symbol("struct", TT_STRUCT_KW, -1);
  intern_symbol("switch", TT_SWITCH_KW, -1);
  intern_symbol("typedef", TT_TYPEDEF_KW, -1);
  intern_symbol("union", TT_UNION_KW, -1);
  intern_symbol("unsigned", TT_UNSIGNED_KW, -1);
  intern_symbol("void", TT_VOID_KW, -1);
  intern_symbol("volatile", TT_VOLATILE_KW, -1);
  intern_symbol("while", TT_WHILE_KW, -1);
  intern_symbol("bool", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_BOOL);
  intern_symbol("true", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_LITERAL, -1, -1);
  pn->ts = TS_BOOL; pn->val.i = 1; /* NB: int in C99 */
  wrap_node(pn, NT_MACRODEF); pn->name = intern("true");
  intern_symbol("false", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_LITERAL, -1, -1);
  pn->ts = TS_BOOL; pn->val.i = 0; /* NB: int in C99 */
  wrap_node(pn, NT_MACRODEF); pn->name = intern("false");
  intern_symbol("wchar_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_INT);
  intern_symbol("wint_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_INT);
  intern_symbol("int8_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_CHAR);
  intern_symbol("uint8_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_UCHAR);
  intern_symbol("int16_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_SHORT);
  intern_symbol("uint16_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_USHORT);
  intern_symbol("int32_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_INT);
  intern_symbol("uint32_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_UINT);
  intern_symbol("int64_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_LLONG);
  intern_symbol("uint64_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_ULLONG);
  intern_symbol("size_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_ULONG);
  intern_symbol("ssize_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_LONG);
  intern_symbol("uintptr_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_ULONG);
  intern_symbol("ptrdiff_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_LONG);
  intern_symbol("intptr_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_LONG);
  intern_symbol("float32_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_FLOAT);
  intern_symbol("float64_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_DOUBLE);
  intern_symbol("v128_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  ndsettype(ndbnewbk(&g_nodes), TS_V128);
  intern_symbol("va_arg_t", TT_TYPE_NAME, (int)ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); make_vararg_union_type(pn);
  intern_symbol("INTPTR_MIN", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndsetilit(ndbnewbk(&g_nodes), TS_LONG, INT_MIN); /* wasm32 */
  wrap_node(pn, NT_MACRODEF); pn->name = intern("INTPTR_MIN");
  intern_symbol("INTPTR_MAX", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndsetilit(ndbnewbk(&g_nodes), TS_LONG, INT_MAX); /* wasm32 */
  wrap_node(pn, NT_MACRODEF); pn->name = intern("INTPTR_MAX");
  intern_symbol("UINTPTR_MAX", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndsetulit(ndbnewbk(&g_nodes), TS_ULONG, UINT_MAX); /* wasm32 */
  wrap_node(pn, NT_MACRODEF); pn->name = intern("UINTPTR_MAX");
  intern_symbol("NULL", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_CAST, -1, -1);
  psn = ndnewbk(pn); ndsettype(psn, TS_PTR);
  psn = ndnewbk(psn); ndsettype(psn, TS_VOID);
  psn = ndnewbk(pn); ndset(psn, NT_LITERAL, -1, -1);
  psn->ts = TS_ULONG; psn->val.i = 0;
  wrap_node(pn, NT_MACRODEF); pn->name = intern("NULL");
  intern_symbol("HUGE_VAL", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_LITERAL, -1, -1);
  pn->ts = TS_DOUBLE; pn->val.d = HUGE_VAL;
  wrap_node(pn, NT_MACRODEF); pn->name = intern("HUGE_VAL");
  intern_symbol("sizeof", TT_INTR_NAME, INTR_SIZEOF);
  intern_symbol("alignof", TT_INTR_NAME, INTR_ALIGNOF);
  intern_symbol("offsetof", TT_INTR_NAME, INTR_OFFSETOF);
  intern_symbol("countof", TT_INTR_NAME, INTR_COUNTOF);
  intern_symbol("alloca", TT_INTR_NAME, INTR_ALLOCA);
  intern_symbol("_Generic", TT_INTR_NAME, INTR_GENERIC); /* C11 */
  intern_symbol("generic", TT_INTR_NAME, INTR_GENERIC); /* WCPL */
  intern_symbol("va_etc", TT_INTR_NAME, INTR_VAETC);
  intern_symbol("va_arg", TT_INTR_NAME, INTR_VAARG);
  intern_symbol("static_assert", TT_INTR_NAME, INTR_SASSERT);
  intern_symbol("defined", TT_INTR_NAME, INTR_DEFINED);
  intern_symbol("__VA_ARGS__", TT_MACRO_NAME, (int)ndblen(&g_nodes)); /* C99 */
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_INTRCALL, -1, -1); pn->intr = INTR_VAETC;
  wrap_node(pn, NT_MACRODEF); pn->name = intern("__VA_ARGS__");
  intern_symbol("__DATE__", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_LITERAL, -1, -1);
  pn->ts = TS_STRING; cbputtime("%b %d %Y", gmtime(&now), &pn->data);
  cbputc(0, &pn->data); wrap_node(pn, NT_MACRODEF); pn->name = intern("__DATE__");
  intern_symbol("__TIME__", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_LITERAL, -1, -1);
  pn->ts = TS_STRING; cbputtime("%X", gmtime(&now), &pn->data);
  cbputc(0, &pn->data); wrap_node(pn, NT_MACRODEF); pn->name = intern("__TIME__");
  intern_symbol("__WCPL__", TT_MACRO_NAME, (int)ndblen(&g_nodes));
  pn = ndsetilit(ndbnewbk(&g_nodes), TS_INT, 1); /* major version */
  wrap_node(pn, NT_MACRODEF); pn->name = intern("__WCPL__");
}

/* simple comparison of numerical NT_LITERAL nodes for equivalence */
bool same_numlit(const node_t *pcn1, const node_t *pcn2)
{
  node_t *pn1 = (node_t*)pcn1, *pn2 = (node_t*)pcn2;
  if (pn1->nt == NT_NULL || pn2->nt == NT_NULL) return true;
  if (pn1->nt != NT_LITERAL || pn2->nt != NT_LITERAL) return false;
  if (pn1->ts != pn2->ts) return false;
  switch (pn1->ts) { 
    case TS_INT: case TS_LONG: case TS_LLONG:
      return pn1->val.i == pn2->val.i;
    case TS_UINT: case TS_ULONG: case TS_ULLONG:
      return pn1->val.u == pn2->val.u;
    default:;
  }
  return false;
}

/* simple comparison of NT_TYPE nodes for equivalence */
bool same_type(const node_t *pctn1, const node_t *pctn2)
{
  node_t *ptn1 = (node_t*)pctn1, *ptn2 = (node_t*)pctn2;
  assert(ptn1->nt == NT_TYPE && ptn2->nt == NT_TYPE);
  if (ptn1->ts == TS_ENUM && ptn2->ts == TS_INT) return true;
  if (ptn1->ts == TS_INT && ptn2->ts == TS_ENUM) return true;
  if (ptn1->ts == TS_V128 && ptn2->ts == TS_V128) {
    assert(ndlen(ptn1) <= 2 && ndlen(ptn2) <= 2);
    /* make sure specialized vectors are specialized in the same way */
    if (!ndlen(ptn1) || !ndlen(ptn2) || same_type(ndcref(ptn1, 0), ndcref(ptn2, 0))) return true;
    if (ndlen(ptn1) < 2 || ndlen(ptn2) < 2) return true;
    return same_numlit(ndcref(ptn1, 1), ndcref(ptn2, 1));
  }
  if (ptn1->ts == TS_ARRAY && ptn2->ts == TS_PTR) { 
    assert(ndlen(ptn1) == 2 && ndlen(ptn2) == 1);
    return ndref(ptn1, 1)->nt == NT_NULL && same_type(ndref(ptn1, 0), ndref(ptn2, 0));
  } else if (ptn1->ts == TS_PTR && ptn2->ts == TS_ARRAY) { 
    assert(ndlen(ptn1) == 1 && ndlen(ptn2) == 2);
    return ndref(ptn2, 1)->nt == NT_NULL && same_type(ndref(ptn1, 0), ndref(ptn2, 0));
  }
  if (ptn1->ts != ptn2->ts) return false;
  if (ptn1->ts == TS_ENUM && (ptn1->name && ptn2->name && ptn1->name != ptn2->name)) return false;
  if (ptn1->ts == TS_STRUCT || ptn1->ts == TS_UNION || 
      ptn1->ts == TS_FUNCTION || ptn1->ts == TS_PTR) {
    size_t i;
    if ((ptn1->ts == TS_STRUCT || ptn1->ts == TS_UNION) &&
        (ptn1->name != ptn2->name)) return false;
    /* allow incomplete struct/union to be equivalent to a complete one */
    if ((ptn1->ts == TS_STRUCT || ptn1->ts == TS_UNION) &&
        (!ndlen(ptn1) || !ndlen(ptn2))) return true;
    /* the rest are types/bindings that should match recursively */
    if (ndlen(ptn1) != ndlen(ptn2)) return false;
    for (i = 0; i < ndlen(ptn1); ++i) {
      node_t *pn1 = ndref(ptn1, i), *pn2 = ndref(ptn2, i);
      if ((ptn1->ts == TS_STRUCT || ptn1->ts == TS_UNION) &&
          (pn1->nt != NT_VARDECL || pn2->nt != NT_VARDECL || 
           pn1->name != pn2->name)) return false;
      if (pn1->nt == NT_VARDECL && ndlen(pn1) == 1) pn1 = ndref(pn1, 0); 
      if (pn2->nt == NT_VARDECL && ndlen(pn2) == 1) pn2 = ndref(pn2, 0); 
      if (pn1->nt != NT_TYPE || pn2->nt != NT_TYPE || 
          !same_type(pn1, pn2)) return false;
    }
  } else if (ptn1->ts == TS_ARRAY) {
    /* allow incomplete array to be equivalent to a complete one */
    assert(ndlen(ptn1) == 2 && ndlen(ptn2) == 2);
    return same_type(ndref(ptn1, 0), ndref(ptn2, 0))
        && same_numlit(ndref(ptn1, 1), ndref(ptn2, 1));
  }
  return true;
}

/* post imported/forward symbol to symbol table; forward unless final */
const node_t *post_symbol(sym_t mod, node_t *pvn, bool final, bool hide)
{
  /* kluge: symbol's status is encoded in pin->ts as follows:  
   * not final: not hide: NONE/EXTERN(if referenced) | hide: STATIC
   *     final: not hide: REGISTER                   | hide: AUTO
   * (not final means 'just a declaration', final means 'definition')
   * (not hide means external linkage, hide means internal linkage) */
  node_t *pin = NULL; int *pi, info;
  assert(pvn->nt == NT_VARDECL && pvn->name && ndlen(pvn) == 1);
  assert(ndref(pvn, 0)->nt == NT_TYPE);
  if ((pi = bufbsearch(&g_syminfo, &pvn->name, &int_cmp)) != NULL) {
    /* see if what's there is an older one with the same name */
    if (pi[1] == TT_IDENTIFIER && pi[2] >= 0) {
      assert(pi[2] < (int)buflen(&g_nodes));
      pin = bufref(&g_nodes, (size_t)pi[2]);
      if (pin->nt == NT_IMPORT && ndlen(pin) == 1) {
        bool curfinal = (pin->sc == SC_REGISTER || pin->sc == SC_AUTO); 
        if (final && curfinal) { /* redefinition attempt */
          n2eprintf(pvn, pin, "symbol already defined: %s", symname(pvn->name));
        } else { /* redeclaration attempt: allow if no change */
          bool curhide = (pin->sc == SC_STATIC || pin->sc == SC_AUTO);
          node_t *ptn = ndref(pin, 0); assert(ptn->nt == NT_TYPE);
          if (curhide == hide && same_type(ptn, ndref(pvn, 0))) {
            if (final) {
              pin->pwsid = pvn->pwsid; pin->startpos = pvn->startpos;
              pin->sc = (hide ? SC_AUTO : SC_REGISTER);
            }
            return pin;
          }
        }
      }
    }
    n2eprintf(pvn, pin, "symbol already defined differently: %s", symname(pvn->name));
  }
  info = (int)ndblen(&g_nodes); pin = ndbnewbk(&g_nodes);
  ndset(pin, NT_IMPORT, pvn->pwsid, pvn->startpos);
  /* changed to SC_EXTERN on reference; SC_AUTO is for private, SC_REGISTER for public */
  pin->name = mod; assert(mod);
  pin->sc = final ? (hide ? SC_AUTO : SC_REGISTER) : (hide ? SC_STATIC : SC_NONE); 
  ndpushbk(pin, ndref(pvn, 0));
  intern_symbol(symname(pvn->name), TT_IDENTIFIER, info);
  return pin;
} 

void fini_symbols(void)
{
  buffini(&g_syminfo);
  ndbfini(&g_nodes);
}

static tt_t lookup_symbol(const char *name, int *pinfo)
{
  sym_t s = intern(name);
  int *pi = bufbsearch(&g_syminfo, &s, &int_cmp);
  if (pinfo) *pinfo = pi ? pi[2] : -1;
  return pi != NULL ? (tt_t)pi[1] : TT_IDENTIFIER;  
}

static bool check_once(const char *fname)
{
  sym_t o = internf("once %s", fname);
  int *pi = bufbsearch(&g_syminfo, &o, &int_cmp);
  if (pi != NULL) return false;
  intern_symbol(symname(o), TT_EOF, -1);
  return true;   
}

const node_t *lookup_global(sym_t name)
{
  const node_t *pn = NULL;
  int *pi = bufbsearch(&g_syminfo, &name, &int_cmp);
  if (pi && pi[1] == TT_IDENTIFIER && pi[2] >= 0) {
    assert(pi[2] < (int)buflen(&g_nodes));
    pn = bufref(&g_nodes, (size_t)pi[2]);
  }
  return pn;
}

const node_t *lookup_macro(sym_t name)
{
  const node_t *pn = NULL;
  int *pi = bufbsearch(&g_syminfo, &name, &int_cmp);
  if (pi && pi[1] == TT_MACRO_NAME && pi[2] >= 0) {
    assert(pi[2] < (int)buflen(&g_nodes));
    pn = bufref(&g_nodes, (size_t)pi[2]);
  }
  return pn;
}

void mark_global_referenced(const node_t *pgn)
{
  assert(pgn && pgn->nt == NT_IMPORT);
  if (pgn->sc == SC_NONE) ((node_t*)pgn)->sc = SC_EXTERN;
}


const node_t *lookup_eus_type(ts_t ts, sym_t name)
{
  const char *tag = NULL; tt_t tt = TT_EOF;
  sym_t s; int *pi; const node_t *pn = NULL;
  switch (ts) {
    case TS_ENUM: tag = "enum"; tt = TT_ENUM_KW; break;
    case TS_UNION: tag = "union"; tt = TT_UNION_KW; break;
    case TS_STRUCT: tag = "struct"; tt = TT_STRUCT_KW; break;
    default: assert(false);
  }
  if (!name || !tag || tt == TT_EOF) return NULL;
  s = internf("%s %s", tag, symname(name));
  pi = bufbsearch(&g_syminfo, &s, &int_cmp);
  if (pi) {
    assert(pi[1] == tt);
    assert(pi[2] < (int)buflen(&g_nodes));
    pn = bufref(&g_nodes, (size_t)pi[2]);
    assert(pn->nt == NT_TYPE && pn->ts == ts); 
  }
  return pn;
}


/* tokenizer */

/* split input into tokens */
#define readchar() (c = *pcuri < endi ? tbase[(*pcuri)++] : fetchline(pw, &tbase, &endi))
#define unreadchar() ((*pcuri)--)
#define state_10d 78
#define state_10dd 79
#define state_10ddd 80
#define state_10dddd 81
#define state_10ddddd 82
#define state_4L 83
#define state_73LL 84
static tt_t lex(pws_t *pw, cbuf_t *pcb)
{
  char *tbase = cbdata(&pw->chars);
  int *pcuri = &pw->curi;
  int endi = (int)cblen(&pw->chars);
  int c, state = 0; bool longprefix = false;
  cbclear(pcb);
  while (true) {
    switch (state) {
    case 0: 
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '^') {
        cbputc(c, pcb);
        state = 29; continue;
      } else if (c == '%') {
        cbputc(c, pcb);
        state = 28; continue;
      } else if (c == '|') {
        cbputc(c, pcb);
        state = 27; continue;
      } else if (c == '&') {
        cbputc(c, pcb);
        state = 26; continue;
      } else if (c == '!') {
        cbputc(c, pcb);
        state = 25; continue;
      } else if (c == '=') {
        cbputc(c, pcb);
        state = 24; continue;
      } else if (c == '<') {
        cbputc(c, pcb);
        state = 23; continue;
      } else if (c == '>') {
        cbputc(c, pcb);
        state = 22; continue;
      } else if (c == '#') {
        cbputc(c, pcb);
        state = 21; continue;
      } else if (c == ';') {
        cbputc(c, pcb);
        return TT_SEMICOLON;
      } else if (c == ',') {
        cbputc(c, pcb);
        return TT_COMMA;
      } else if (c == '}') {
        cbputc(c, pcb);
        return TT_RBRC;
      } else if (c == '{') {
        cbputc(c, pcb);
        return TT_LBRC;
      } else if (c == ']') {
        cbputc(c, pcb);
        return TT_RBRK;
      } else if (c == '[') {
        cbputc(c, pcb);
        return TT_LBRK;
      } else if (c == ')') {
        cbputc(c, pcb);
        return TT_RPAR;
      } else if (c == '(') {
        cbputc(c, pcb);
        return TT_LPAR;
      } else if (c == '?') {
        cbputc(c, pcb);
        return TT_QMARK;
      } else if (c == ':') {
        cbputc(c, pcb);
        return TT_COLON;
      } else if (c == '~') {
        cbputc(c, pcb);
        return TT_TILDE;
      } else if (c == '\"') {
        state = 11; continue;
      } else if (c == '\'') {
        state = 10; continue;
      } else if (c == '-') {
        cbputc(c, pcb);
        state = 9; continue;
      } else if (c == '+') {
        cbputc(c, pcb);
        state = 8; continue;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 7; continue;
      } else if (c == '0') {
        cbputc(c, pcb);
        state = 6; continue;
      } else if ((c >= '1' && c <= '9')) {
        cbputc(c, pcb);
        state = 5; continue;
      } else if (c == 'L') {
        state = state_4L; continue;
      } else if ((c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z')) {
        cbputc(c, pcb);
        state = 4; continue;
      } else if (c == '*') {
        cbputc(c, pcb);
        state = 3; continue;
      } else if (c == '/') {
        cbputc(c, pcb);
        state = 2; continue;
      } else if (c == '\t' || c == '\f' || c == ' ') {
        cbputc(c, pcb);
        state = 1; continue;
      } else if (c == '\r') {
        readchar();
        if (c == '\n') {
          cbputc(c, pcb);
          return TT_WHITESPACE; /* single-char token */
        } else {
          unreadchar();
          return TT_EOF;
        }
      } else if (c == '\n') {
        cbputc(c, pcb);
        return TT_WHITESPACE; /* single-char token */
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 1:
      readchar();
      if (c == EOF) {
        return TT_WHITESPACE;
      } else if (c == '\t' || c == '\f' || c == ' ') {
        cbputc(c, pcb);
        state = 1; continue;
      } else {
        unreadchar();
        return TT_WHITESPACE;
      }
    case 2:
      readchar();
      if (c == EOF) {
        return TT_SLASH;
      } else if (c == '*') {
        cbputc(c, pcb);
        state = 76; continue;
      } else if (c == '/') {
        cbputc(c, pcb);
        state = 75; continue;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_SLASH_ASN;
      } else {
        unreadchar();
        return TT_SLASH;
      }
    case 3:
      readchar();
      if (c == EOF) {
        return TT_STAR;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_STAR_ASN;
      } else {
        unreadchar();
        return TT_STAR;
      }
    case state_4L:
      readchar();
      if (c == EOF) {
        cbputc('L', pcb);
        return lookup_symbol(cbdata(pcb), NULL);
      } else if (c == '\"') {
        longprefix = true;
        state = 11; continue;
      } else if (c == '\'') {
        longprefix = true;
        state = 10; continue;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z')) {
        cbputc('L', pcb);
        cbputc(c, pcb);
        state = 4; continue;
      } else {
        unreadchar();
        cbputc('L', pcb);
        return lookup_symbol(cbdata(pcb), NULL);
      }
    case 4:
      readchar();
      if (c == EOF) {
        return lookup_symbol(cbdata(pcb), NULL);
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z')) {
        cbputc(c, pcb);
        state = 4; continue;
      } else {
        unreadchar();
        return lookup_symbol(cbdata(pcb), NULL);
      }
    case 5:
      readchar();
      if (c == EOF) {
        return TT_INT;
      } else if (c == 'U' || c == 'u') {
        state = 74; continue;
      } else if (c == 'L' || c == 'l') {
        readchar();
        if (c == 'L' || c == 'l') { 
          state = state_73LL; continue; 
        }
        if (c != EOF) unreadchar();
        state = 73; continue;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 66; continue;
      } else if (c == 'E' || c == 'e') {
        cbputc(c, pcb);
        state = 62; continue;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 5; continue;
      } else {
        unreadchar();
        return TT_INT;
      }
    case 6:
      readchar();
      if (c == EOF) {
        return TT_INT;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 66; continue;
      } else if ((c >= '8' && c <= '9')) {
        cbputc(c, pcb);
        state = 65; continue;
      } else if ((c >= '0' && c <= '7')) {
        cbputc(c, pcb);
        state = 64; continue;
      } else if (c == 'U' || c == 'u') {
        state = 74; continue;
      } else if (c == 'E' || c == 'e') {
        cbputc(c, pcb);
        state = 62; continue;
      } else if (c == 'X' || c == 'x') {
        cbputc(c, pcb);
        state = 61; continue;
      } else if (c == 'L' || c == 'l') {
        readchar();
        if (c == 'L' || c == 'l') { 
          state = state_73LL; continue;
        }
        if (c != EOF) unreadchar();
        state = 73; continue;
      } else {
        unreadchar();
        return TT_INT;
      }
    case 7:
      readchar();
      if (c == EOF) {
        return TT_DOT;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 55; continue;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 54; continue;
      } else {
        unreadchar();
        return TT_DOT;
      }
    case 8:
      readchar();
      if (c == EOF) {
        return TT_PLUS;
      } else if (c == '+') {
        cbputc(c, pcb);
        return TT_PLUS_PLUS;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_PLUS_ASN;
      } else {
        unreadchar();
        return TT_PLUS;
      }
    case 9:
      readchar();
      if (c == EOF) {
        return TT_MINUS;
      } else if (c == '-') {
        cbputc(c, pcb);
        return TT_MINUS_MINUS;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_MINUS_ASN;
      } else if (c == '>') {
        cbputc(c, pcb);
        return TT_ARROW;
      } else {
        unreadchar();
        return TT_MINUS;
      }
    case 10:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '\\') {
        cbputc(c, pcb);
        state = 44; continue;
      } else if (is8chead(c)) { 
        int u = c & 0xFF;
        if (u < 0xE0) { cbputc(c, pcb); state = state_10d; continue; }
        if (u < 0xF0) { cbputc(c, pcb); state = state_10dd; continue; }
        if (u < 0xF8) { cbputc(c, pcb); state = state_10ddd; continue; }
        if (u < 0xFC) { cbputc(c, pcb); state = state_10dddd; continue; }
        if (u < 0xFE) { cbputc(c, pcb); state = state_10ddddd; continue; }
        unreadchar();
        return TT_EOF;
      } else if (!(c == '\n' || c == '\'' || c == '\\')) {
        cbputc(c, pcb);
        state = 43; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case state_10ddddd:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (is8ctail(c)) {
        cbputc(c, pcb);
        state = state_10dddd; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case state_10dddd:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (is8ctail(c)) {
        cbputc(c, pcb);
        state = state_10ddd; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case state_10ddd:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (is8ctail(c)) {
        cbputc(c, pcb);
        state = state_10dd; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case state_10dd:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (is8ctail(c)) {
        cbputc(c, pcb);
        state = state_10d; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case state_10d:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (is8ctail(c)) {
        cbputc(c, pcb);
        state = 43; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 11:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '\\') {
        cbputc(c, pcb);
        state = 34; continue;
      } else if (c == '\"') {
        return longprefix ? TT_LSTRING : TT_STRING;
      } else if (!(c == '\n' || c == '\"' || c == '\\')) {
        cbputc(c, pcb);
        state = 11; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 21:
      readchar();
      if (c == EOF) {
        return TT_HASH;
      } else if (c == '#') {
        cbputc(c, pcb);
        state = 32; continue;
      } else {
        unreadchar();
        return TT_HASH;
      }
    case 22:
      readchar();
      if (c == EOF) {
        return TT_GT;
      } else if (c == '>') {
        cbputc(c, pcb);
        state = 31; continue;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_GE;
      } else {
        unreadchar();
        return TT_GT;
      }
    case 23:
      readchar();
      if (c == EOF) {
        return TT_LT;
      } else if (c == '<') {
        cbputc(c, pcb);
        state = 30; continue;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_LE;
      } else {
        unreadchar();
        return TT_LT;
      }
    case 24:
      readchar();
      if (c == EOF) {
        return TT_ASN;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_EQ;
      } else {
        unreadchar();
        return TT_ASN;
      }
    case 25:
      readchar();
      if (c == EOF) {
        return TT_NOT;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_NE;
      } else {
        unreadchar();
        return TT_NOT;
      }
    case 26:
      readchar();
      if (c == EOF) {
        return TT_AND;
      } else if (c == '&') {
        cbputc(c, pcb);
        return TT_AND_AND;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_AND_ASN;
      } else {
        unreadchar();
        return TT_AND;
      }
    case 27:
      readchar();
      if (c == EOF) {
        return TT_OR;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_OR_ASN;
      } else if (c == '|') {
        cbputc(c, pcb);
        return TT_OR_OR;
      } else {
        unreadchar();
        return TT_OR;
      }
    case 28:
      readchar();
      if (c == EOF) {
        return TT_REM;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_REM_ASN;
      } else {
        unreadchar();
        return TT_REM;
      }
    case 29:
      readchar();
      if (c == EOF) {
        return TT_XOR;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_XOR_ASN;
      } else {
        unreadchar();
        return TT_XOR;
      }
    case 30:
      readchar();
      if (c == EOF) {
        return TT_SHL;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_SHL_ASN;
      } else {
        unreadchar();
        return TT_SHL;
      }
    case 31:
      readchar();
      if (c == EOF) {
        return TT_SHR;
      } else if (c == '=') {
        cbputc(c, pcb);
        return TT_SHR_ASN;
      } else {
        unreadchar();
        return TT_SHR;
      }
    case 32:
      return TT_HASHHASH;
    case 34:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '7')) {
        cbputc(c, pcb);
        state = 37; continue;
      } else if (c == 'u') {
        cbputc(c, pcb);
        state = 36; continue;
      } else if (c == 'x') {
        cbputc(c, pcb);
        state = 35; continue;
      } else if (c == '\"' || c == '\'' || c == '?' || c == '\\' || (c >= 'a' && c <= 'b') || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v') {
        cbputc(c, pcb);
        state = 11; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 35:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 42; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 36:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 39; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 37:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '7')) {
        cbputc(c, pcb);
        state = 38; continue;
      } else if (c == '\\') {
        cbputc(c, pcb);
        state = 34; continue;
      } else if (c == '\"') {
        return longprefix ? TT_LSTRING : TT_STRING;
      } else if (!(c == '\n' || c == '\"' || (c >= '0' && c <= '7') || c == '\\')) {
        cbputc(c, pcb);
        state = 11; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 38:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '\\') {
        cbputc(c, pcb);
        state = 34; continue;
      } else if (c == '\"') {
        return longprefix ? TT_LSTRING : TT_STRING;
      } else if (!(c == '\n' || c == '\"' || c == '\\')) {
        cbputc(c, pcb);
        state = 11; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 39:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 40; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 40:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 41; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 41:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 11; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 42:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 42; continue;
      } else if (c == '\\') {
        cbputc(c, pcb);
        state = 34; continue;
      } else if (c == '\"') {
        return longprefix ? TT_LSTRING : TT_STRING;
      } else if (!(c == '\n' || c == '\"' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == '\\' || (c >= 'a' && c <= 'f'))) {
        cbputc(c, pcb);
        state = 11; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 43:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '\'') {
        return longprefix ? TT_LCHAR : TT_CHAR;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 44:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '7')) {
        cbputc(c, pcb);
        state = 47; continue;
      } else if (c == 'u') {
        cbputc(c, pcb);
        state = 46; continue;
      } else if (c == 'x') {
        cbputc(c, pcb);
        state = 45; continue;
      } else if (c == '\"' || c == '\'' || c == '?' || c == '\\' || (c >= 'a' && c <= 'b') || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v') {
        cbputc(c, pcb);
        state = 43; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 45:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 53; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 46:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 50; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 47:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '\'') {
        return longprefix ? TT_LCHAR : TT_CHAR;
      } else if ((c >= '0' && c <= '7')) {
        cbputc(c, pcb);
        state = 48; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 48:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '\'') {
        return longprefix ? TT_LCHAR : TT_CHAR;
      } else if ((c >= '0' && c <= '7')) {
        cbputc(c, pcb);
        state = 43; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 50:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 51; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 51:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 52; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 52:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 43; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 53:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 53; continue;
      } else if (c == '\'') {
        return longprefix ? TT_LCHAR : TT_CHAR;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 54:
      readchar();
      if (c == EOF) {
        return TT_DOUBLE;
      } else if (c == 'F' || c == 'f') {
        return TT_FLOAT;
      } else if (c == 'E' || c == 'e') {
        cbputc(c, pcb);
        state = 57; continue;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 54; continue;
      } else {
        unreadchar();
        return TT_DOUBLE;
      }
    case 55:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 56; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 56:
      return TT_ELLIPSIS;
    case 57:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '+' || c == '-') {
        cbputc(c, pcb);
        state = 59; continue;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 58; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 58:
      readchar();
      if (c == EOF) {
        return TT_DOUBLE;
      } else if (c == 'F' || c == 'f') {
        return TT_FLOAT;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 58; continue;
      } else {
        unreadchar();
        return TT_DOUBLE;
      }
    case 59:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 58; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 61:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 90; continue;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 70; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 62:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '+' || c == '-') {
        cbputc(c, pcb);
        state = 69; continue;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 68; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 64:
      readchar();
      if (c == EOF) {
        return TT_INT;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 66; continue;
      } else if ((c >= '8' && c <= '9')) {
        cbputc(c, pcb);
        state = 65; continue;
      } else if ((c >= '0' && c <= '7')) {
        cbputc(c, pcb);
        state = 64; continue;
      } else if (c == 'U' || c == 'u') {
        state = 74; continue;
      } else if (c == 'E' || c == 'e') {
        cbputc(c, pcb);
        state = 62; continue;
      } else if (c == 'L' || c == 'l') {
        readchar();
        if (c == 'L' || c == 'l') {
          state = state_73LL; continue;
        }
        if (c != EOF) unreadchar();
        state = 73; continue;
      } else {
        unreadchar();
        return TT_INT;
      }
    case 65:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 66; continue;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 65; continue;
      } else if (c == 'E' || c == 'e') {
        cbputc(c, pcb);
        state = 62; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 66:
      readchar();
      if (c == EOF) {
        return TT_DOUBLE;
      } else if (c == 'F' || c == 'f') {
        return TT_FLOAT;
      } else if (c == 'E' || c == 'e') {
        cbputc(c, pcb);
        state = 57; continue;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 54; continue;
      } else {
        unreadchar();
        return TT_DOUBLE;
      }
    case 68:
      readchar();
      if (c == EOF) {
        return TT_DOUBLE;
      } else if (c == 'F' || c == 'f') {
        return TT_FLOAT;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 68; continue;
      } else {
        unreadchar();
        return TT_DOUBLE;
      }
    case 69:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9')) {
        cbputc(c, pcb);
        state = 68; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 70:
      readchar();
      if (c == EOF) {
        return TT_INT;
      } else if (c == '.') {
        cbputc(c, pcb);
        state = 91; continue;
      } else if (c == 'P' || c == 'p') {
        cbputc(c, pcb);
        state = 62; continue; 
      } else if (c == 'U' || c == 'u') {
        state = 74; continue;
      } else if (c == 'L' || c == 'l') {
        readchar();
        if (c == 'L' || c == 'l') {
          state = state_73LL;
          continue;
        }
        if (c != EOF) unreadchar();
        state = 73; continue;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 70; continue;
      } else {
        unreadchar();
        return TT_INT;
      }
    case 73:
      readchar();
      if (c == EOF) {
        return TT_LONG;
      } else if (c == 'U' || c == 'u') {
        return TT_ULONG;
      } else {
        unreadchar();
        return TT_LONG;
      }
    case state_73LL:
      readchar();
      if (c == EOF) {
        return TT_LLONG;
      } else if (c == 'U' || c == 'u') {
        return TT_ULLONG;
      } else {
        unreadchar();
        return TT_LLONG;
      }
    case 74:
      readchar();
      if (c == EOF) {
        return TT_UINT;
      } else if (c == 'L' || c == 'l') {
        readchar();
        if (c == 'L' || c == 'l') return TT_ULLONG;
        if (c != EOF) unreadchar();
        return TT_ULONG;
      } else {
        unreadchar();
        return TT_UINT;
      }
    case 75:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (!(c == '\n')) {
        cbputc(c, pcb);
        state = 75; continue;
      } else {
        unreadchar(); /* \n will be in a separate whitespace token */
        return TT_WHITESPACE;
      }
    case 76:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (!(c == '*')) {
        cbputc(c, pcb);
        state = 76; continue;
      } else {
        cbputc(c, pcb);
        state = 77; continue;
      }
    case 77:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == '*') {
        cbputc(c, pcb);
        state = 77; continue;
      } else if (!(c == '*' || c == '/')) {
        cbputc(c, pcb);
        state = 76; continue;
      } else {
        cbputc(c, pcb);
        return TT_WHITESPACE;
      }
    case 90:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 91; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    case 91:
      readchar();
      if (c == EOF) {
        return TT_EOF;
      } else if (c == 'P' || c == 'p') {
        cbputc(c, pcb);
        state = 62; continue;
      } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
        cbputc(c, pcb);
        state = 91; continue;
      } else {
        unreadchar();
        return TT_EOF;
      }
    }
  }
  return TT_EOF;
}
#undef state_10d
#undef state_10dd
#undef state_10ddd
#undef state_10dddd
#undef state_10ddddd
#undef state_4L
#undef state_73LL
#undef readchar
#undef unreadchar

/* fetch next token, as-is */ 
static tt_t peekt_ws(pws_t *pw)
{
  if (!pw->gottk) {
    pw->pos = (int)pw->discarded + pw->curi; 
    pw->ctk = lex(pw, &pw->token); 
    pw->gottk = true; 
    pw->tokstr = cbdata(&pw->token); 
  }
  return pw->ctk; 
}

/* fetch next non-whitespace token */ 
static tt_t peekt(pws_t *pw) 
{ 
  if (!pw->gottk) {
    do {
      pw->pos = (int)pw->discarded + pw->curi; 
      pw->ctk = lex(pw, &pw->token); 
      if (pw->ctk == TT_EOF && !pw->inateof) 
        reprintf(pw, pw->pos, "illegal token"); 
    } while (pw->ctk == TT_WHITESPACE); 
    pw->gottk = true; 
    pw->tokstr = cbdata(&pw->token); 
  } 
  return pw->ctk; 
}

static int peekc(pws_t *pw)
{
  return cbdata(&pw->chars)[pw->curi];
} 

static int peekpos(pws_t *pw)
{
  peekt(pw);
  return pw->pos;
}
 
static void dropt(pws_t *pw) 
{ 
  pw->gottk = false; 
} 

/* expect and read non-quoted-literal token tk */ 
static void expect(pws_t *pw, tt_t tt, const char *ts) 
{ 
  tt_t ctk = peekt(pw); 
  if (ctk == TT_EOF)
    reprintf(pw, pw->pos, "expecting %s (got EOF instead)", ts); 
  else if (ctk != tt)
    reprintf(pw, pw->pos, "expecting %s (got %s instead)", ts, pw->tokstr); 
  else pw->gottk = false; 
} 
 
static bool ahead(pws_t *pw, const char *tk) 
{ 
  peekt(pw); 
  assert(tk);
  /* do not match tk with content of quoted literals */
  if (pw->ctk == TT_CHAR || pw->ctk == TT_LCHAR) return false; 
  if (pw->ctk == TT_STRING || pw->ctk == TT_LSTRING) return false; 
  return streql(tk, pw->tokstr); 
} 


/* node_t grammar nodes */

node_t mknd(void)
{
  node_t nd; ndinit(&nd);
  return nd;
}

node_t* ndinit(node_t* pn)
{
  memset(pn, 0, sizeof(node_t));
  pn->pwsid = pn->startpos = -1;
  bufinit(&pn->data, sizeof(char)); /* cbuf by default */
  ndbinit(&pn->body);
  return pn;
}

node_t *ndicpy(node_t* mem, const node_t* pn)
{
  memcpy(mem, pn, sizeof(node_t));
  buficpy(&mem->data, &pn->data);
  ndbicpy(&mem->body, &pn->body);
  return mem;
}

void ndfini(node_t* pn)
{
  buffini(&pn->data);
  ndbfini(&pn->body);
}

node_t *ndcpy(node_t* pn, const node_t* pr)
{
  ndfini(pn);
  ndicpy(pn, pr);
  return pn;
}

node_t *ndset(node_t *dst, nt_t nt, int pwsid, int startpos)
{
  size_t esz = (nt == NT_ACODE) ? sizeof(inscode_t) : sizeof(char);
  dst->nt = nt;
  dst->pwsid = pwsid;
  dst->startpos = startpos;
  dst->name = 0;
  if (esz == dst->data.esz) bufclear(&dst->data);
  else { buffini(&dst->data); bufinit(&dst->data, esz); }
  dst->val.u = 0;
  dst->op = TT_EOF;
  dst->ts = TS_VOID;
  dst->sc = SC_NONE;
  ndbclear(&dst->body);
  return dst;
}

node_t *ndsettype(node_t *dst, ts_t ts)
{
  ndset(dst, NT_TYPE, -1, -1);
  dst->ts = ts;
  return dst;
}

node_t *ndsetilit(node_t *dst, ts_t ts, long long i)
{
  ndset(dst, NT_LITERAL, -1, -1);
  dst->ts = ts; dst->val.i = i;
  return dst;
}

node_t *ndsetulit(node_t *dst, ts_t ts, unsigned long long u)
{
  ndset(dst, NT_LITERAL, -1, -1);
  dst->ts = ts; dst->val.u = u;
  return dst;
}

void ndclear(node_t* pn)
{
  ndfini(pn);
  ndinit(pn);
}

void ndrem(node_t* pn, size_t i)
{
  ndfini(bufref(&pn->body, i));
  bufrem(&pn->body, i);
}

node_t *ndinsfr(node_t *pn, nt_t nt)
{
  return ndset(ndinsnew(pn, 0), nt, pn->pwsid, pn->startpos);
}

node_t *ndinsbk(node_t *pn, nt_t nt)
{
  return ndset(ndnewbk(pn), nt, pn->pwsid, pn->startpos);
}

void ndbicpy(ndbuf_t* mem, const ndbuf_t* pb)
{
  node_t *pndm, *pnd; size_t i;
  assert(mem); assert(pb); assert(pb->esz == sizeof(node_t));
  buficpy(mem, pb);
  pndm = (node_t*)(mem->buf), pnd = (node_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) ndicpy(pndm+i, pnd+i);
}

void ndbfini(ndbuf_t* pb)
{
  node_t *pnd; size_t i;
  assert(pb); assert(pb->esz == sizeof(node_t));
  pnd = (node_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) ndfini(pnd+i);
  buffini(pb);
}

void ndbclear(ndbuf_t* pb)
{
  node_t *pnd; size_t i;
  assert(pb); assert(pb->esz == sizeof(node_t));
  pnd = (node_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) ndfini(pnd+i);
  bufclear(pb);
}


/* node pool */

static buf_t g_npbuf;

void init_nodepool(void)
{
  bufinit(&g_npbuf, sizeof(node_t*));
}

void fini_nodepool(void)
{
  size_t i;
  for (i = 0; i < buflen(&g_npbuf); ++i) {
    node_t **ppn = bufref(&g_npbuf, i);
    ndfini(*ppn); free(*ppn);
  }
  buffini(&g_npbuf);
}

void clear_nodepool(void)
{
  fini_nodepool();
  init_nodepool();
}

node_t *npalloc(void)
{
  node_t *pn = emalloc(sizeof(node_t)), **ppn;
  ndinit(pn);
  ppn = bufnewbk(&g_npbuf); *ppn = pn;
  return pn;
}

node_t *npnew(nt_t nt, int pwsid, int startpos)
{
  return ndset(npalloc(), nt, pwsid, startpos);
}

node_t *npnewcode(const node_t *psn)
{
  return npnew(NT_ACODE, psn ? psn->pwsid : -1, psn ? psn->startpos : -1);
}

node_t *npdup(const node_t *pr)
{
  return ndcpy(npalloc(), pr); 
}


/* unique register name pool */

static int g_rnum_f64 = 0;
static int g_rnum_f32 = 0;
static int g_rnum_i64 = 0;
static int g_rnum_i32 = 0;
static int g_rnum_lab = 0;

void init_regpool(void)
{
  g_rnum_f64 = 0;
  g_rnum_f32 = 0;
  g_rnum_i64 = 0;
  g_rnum_i32 = 0;
  g_rnum_lab = 0;
}

void fini_regpool(void)
{
  init_regpool();
}

void clear_regpool(void)
{
  init_regpool();
}

sym_t rpalloc(valtype_t vt)
{
  switch (vt) {
    case VT_F64: return internf("d%d$",  ++g_rnum_f64); 
    case VT_F32: return internf("d%d$",  ++g_rnum_f32); 
    case VT_I64: return internf("ll%d$", ++g_rnum_i64); 
    case VT_I32: return internf("i%d$",  ++g_rnum_i32); 
    default: assert(false);
  }
  return 0;
}

sym_t rpalloc_label(void)
{
  return internf("%d$", ++g_rnum_lab);
}

/* node builders */

/* wrap node into nt node as a subnode */
node_t *wrap_node(node_t *pn, nt_t nt)
{
  node_t nd = mknd();
  ndset(&nd, nt, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap type node into TS_PTR type node */
node_t *wrap_type_pointer(node_t *pn)
{
  node_t nd = mknd();
  ndset(&nd, NT_TYPE, pn->pwsid, pn->startpos);
  nd.ts = TS_PTR;
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap type node and expr node into TS_ARRAY type node */
node_t *wrap_type_array(node_t *pn, node_t *pi)
{
  node_t nd = mknd();
  ndset(&nd, NT_TYPE, pn->pwsid, pn->startpos);
  nd.ts = TS_ARRAY;
  ndswap(pn, ndnewbk(&nd));
  ndswap(pi, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* flatten TS_ARRAY type node into TS_PTR type node */
node_t *flatten_type_array(node_t *pn)
{
  size_t len = ndlen(pn);
  assert(pn->ts == TS_ARRAY && len == 1 || len == 2);
  if (len == 2) ndbrem(&pn->body, 1);
  pn->ts = TS_PTR;
  return pn;
}

/* wrap type node and vec of type nodes into TS_FUNCTION type node */
node_t *wrap_type_function(node_t *pn, ndbuf_t *pnb)
{
  size_t i; node_t nd = mknd();
  ndset(&nd, NT_TYPE, pn->pwsid, pn->startpos);
  nd.ts = TS_FUNCTION;
  ndswap(pn, ndnewbk(&nd));
  if (pnb) for (i = 0; i < ndblen(pnb); ++i) {
    node_t *pni = ndbref(pnb, i);
    /* treat (void) as () */
    if (i+1 == ndblen(pnb) && pni->nt == NT_VARDECL && 
        ndlen(pni) == 1 && ndref(pni, 0)->nt == NT_TYPE && 
        ndref(pni, 0)->ts == TS_VOID) break;
    ndswap(pni, ndnewbk(&nd));
  }
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap node into NT_SUBSCRIPT node */
node_t *wrap_subscript(node_t *pn, node_t *psn)
{
  node_t nd = mknd();
  ndset(&nd, NT_SUBSCRIPT, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(psn, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap expr node into NT_POSTFIX type node */
node_t *wrap_postfix_operator(node_t *pn, tt_t op, sym_t id)
{
  node_t nd = mknd();
  ndset(&nd, NT_POSTFIX, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  nd.op = op; nd.name = id;
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap expr node into NT_PREFIX type node */
node_t *wrap_unary_operator(node_t *pn, int startpos, tt_t op)
{
  node_t nd = mknd();
  ndset(&nd, NT_PREFIX, pn->pwsid, startpos);
  ndswap(pn, ndnewbk(&nd));
  nd.op = op;
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* flatten node into its argument #0 */
node_t *lift_arg0(node_t *pn)
{
  node_t nd = mknd();
  assert(ndlen(pn) >= 1);
  ndswap(&nd, ndref(pn, 0));
  ndswap(&nd, pn);  
  ndfini(&nd);
  return pn;
}

/* flatten node into its argument #1 */
node_t *lift_arg1(node_t *pn)
{
  node_t nd = mknd();
  assert(ndlen(pn) >= 2);
  ndswap(&nd, ndref(pn, 1));
  ndswap(&nd, pn);  
  ndfini(&nd);
  return pn;
}

/* replace NT_NULL content with int literal n */
node_t *init_to_int(node_t *pn, int n)
{
  assert(pn->nt == NT_NULL);
  pn->nt = NT_LITERAL;
  pn->ts = TS_INT;
  pn->val.i = n;
  return pn;
}

/* replace content with int literal n, keep posinfo */
extern node_t *set_to_int(node_t *pn, int n)
{
  node_t nd = mknd();
  ndset(&nd, NT_LITERAL, pn->pwsid, pn->startpos);
  nd.ts = TS_INT, nd.val.i = n;
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap expr node into NT_CAST type node */
node_t *wrap_cast_expr(node_t *pcn, node_t *pn)
{
  node_t nd = mknd();
  ndset(&nd, NT_CAST, pcn->pwsid, pcn->startpos);
  ndswap(pcn, ndnewbk(&nd));
  ndswap(pn, ndnewbk(&nd));
  ndswap(pcn, &nd);
  ndfini(&nd);
  return pcn;
}

/* wrap expr node into NT_CAST type node */
node_t *wrap_expr_cast(node_t *pn, node_t *pcn)
{
  node_t nd = mknd();
  ndset(&nd, NT_CAST, pcn->pwsid, pcn->startpos);
  ndswap(pcn, ndnewbk(&nd));
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap expr node into NT_INFIX with second expr */
node_t *wrap_binary(node_t *pn, tt_t op, node_t *pn2)
{
  node_t nd = mknd();
  ndset(&nd, NT_INFIX, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn2, ndnewbk(&nd));
  nd.op = op;
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap expr node into NT_COND with second/third exprs */
node_t *wrap_conditional(node_t *pn, node_t *pn2, node_t *pn3)
{
  node_t nd = mknd();
  ndset(&nd, NT_COND, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn2, ndnewbk(&nd));
  ndswap(pn3, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap expr node into NT_ASSIGN with second expr */
node_t *wrap_assignment(node_t *pn, tt_t op, node_t *pn2)
{
  node_t nd = mknd();
  ndset(&nd, NT_ASSIGN, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn2, ndnewbk(&nd));
  nd.op = op;
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}

/* wrap expr node into NT_COMMA with second expr */
node_t *wrap_comma(node_t *pn, node_t *pn2)
{
  node_t nd = mknd();
  ndset(&nd, NT_COMMA, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn2, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
  return pn;
}


/* top-down parser */

/* get an identifier */ 
static sym_t getid(pws_t *pw) 
{ 
  sym_t id = 0;
  if (peekt(pw) == TT_IDENTIFIER) {
    id = intern(pw->tokstr);
    dropt(pw);
  } else reprintf(pw, pw->pos, "identifier expected");  
  return id; 
}

/* get an identifier or any id-like name, even reserved one */ 
static sym_t gettag(pws_t *pw) 
{ 
  sym_t id = 0; tt_t tt = peekt(pw);
  if (tt == TT_IDENTIFIER || 
      (TT_AUTO_KW <= tt && tt <= TT_WHILE_KW) ||
      (TT_TYPE_NAME <= tt && tt <= TT_INTR_NAME)) {
    id = intern(pw->tokstr);
    dropt(pw);
  } else reprintf(pw, pw->pos, "tag or name expected");  
  return id; 
}

static bool type_specifier_ahead(pws_t *pw)
{
  switch (peekt(pw)) {
    case TT_CHAR_KW:     case TT_CONST_KW:    case TT_DOUBLE_KW:
    case TT_ENUM_KW:     case TT_FLOAT_KW:    case TT_INT_KW:
    case TT_LONG_KW:     case TT_RESTRICT_KW: case TT_SHORT_KW:
    case TT_SIGNED_KW:   case TT_SIMD_KW:     case TT_STRUCT_KW: 
    case TT_UNION_KW:    case TT_UNSIGNED_KW: case TT_VOID_KW:   
    case TT_VOLATILE_KW: case TT_TYPE_NAME:
      return true;
    default:;
  }
  return false;
}

static bool storage_class_specifier_ahead(pws_t *pw)
{
  switch (peekt(pw)) {
    case TT_EXTERN_KW: case TT_STATIC_KW:   
    case TT_AUTO_KW:   case TT_REGISTER_KW:
      return true;
    default:;
  }
  return false;
}

/* defined below */
static void parse_expr(pws_t *pw, node_t *pn);
static void parse_asm_code(pws_t *pw, node_t *pn, node_t *ptn);
static void parse_cast_expr(pws_t *pw, node_t *pn);
static void parse_unary_expr(pws_t *pw, node_t *pn);
static void parse_assignment_expr(pws_t *pw, node_t *pn);
static void parse_initializer(pws_t *pw, node_t *pn);
static void parse_base_type(pws_t *pw, node_t *pn);
static void parse_unary_declarator(pws_t *pw, node_t *pn);
static void parse_enum_body(pws_t *pw, node_t *pn);
static void parse_sru_body(pws_t *pw, ts_t sru, node_t *pn);
static sym_t parse_declarator(pws_t *pw, node_t *ptn);
static sc_t parse_decl(pws_t *pw, ndbuf_t *pnb);
static void parse_stmt(pws_t *pw, node_t *pn);

bool node_is_etc(const node_t *pn)
{
  return pn->nt == NT_INTRCALL && pn->intr == INTR_VAETC && ndlen(pn) == 0;
}

bool node_is_countofetc(const node_t *pn)
{
  if (pn && pn->nt == NT_INTRCALL && pn->intr == INTR_COUNTOF && ndlen(pn) == 1) {
    return node_is_etc(ndcref(pn, 0));
  }
  return false;
}

bool node_is_generic(const node_t *pn)
{
  return pn->nt == NT_INTRCALL && pn->intr == INTR_GENERIC && ndlen(pn) >= 1;
}

static bool no_etcpars(ndbuf_t *ppars)
{
  size_t i; 
  for (i = 0; i < ndblen(ppars); ++i) {
    if (node_is_etc(ndbref(ppars, i))) return false; 
  }
  return true;
}

void patch_macro_template(buf_t *pids, ndbuf_t *ppars, node_t *pn)
{
  size_t n = buflen(pids), i; sym_t *pi = bufdata(pids); 
  bool etcbound = n > 0 && pi[n-1] == 0; /* ... is macro-bound */
  if (pn->nt == NT_IDENTIFIER) {
    for (i = 0; i < n; ++i) { /* linear search is ok here */
      if (i+1 == n && !pi[i]) break; /* ... */
      if (pi[i] != pn->name) continue;
      ndcpy(pn, ndbref(ppars, i)); break; 
    }
  } else if (node_is_etc(pn) && no_etcpars(ppars)) {
    if (etcbound) neprintf(pn, "... or __VA_ARGS__ outside of function call argument list");
  } else if (node_is_countofetc(pn) && no_etcpars(ppars)) {
    size_t pcnt = ndblen(ppars), ecnt = pcnt-(n-1); 
    node_t nd = mknd(); assert(pcnt >= n-1);
    ndset(&nd, NT_LITERAL, pn->pwsid, pn->startpos);
    nd.ts = TS_INT; nd.val.i = (int)ecnt;
    ndswap(pn, &nd); ndfini(&nd);
  } else if (node_is_generic(pn)) {
    if (etcbound && node_is_countofetc(ndcref(pn, 0)) && no_etcpars(ppars)) {
      size_t i, pcnt = ndblen(ppars), ecnt = pcnt-(n-1); node_t *presn = NULL;
      assert(pcnt >= n-1);
      for (i = 1; i+1 < ndlen(pn); i += 2) {
        node_t *pcn = ndref(pn, i), *pen = ndref(pn, i+1);
        if (pcn->nt == NT_NULL || (pcn->nt == NT_LITERAL && pcn->ts == TS_INT && pcn->val.i == (int)pcnt)) {
          patch_macro_template(pids, ppars, pen);
          presn = pen; break;
        }
      }
      if (presn) { node_t nd = mknd(); ndswap(&nd, presn);  ndswap(&nd, pn); ndfini(&nd); } 
      else neprintf(pn, "generic dispatch on countof(...): no case for %d params", (int)ecnt);
    } else { /* can't be reduced */
      size_t i;
      patch_macro_template(pids, ppars, ndref(pn, 0));
      for (i = 1; i+1 < ndlen(pn); i += 2) {
        patch_macro_template(pids, ppars, ndref(pn, i+1));
      }
    }
  } else if (pn->nt == NT_ACODE) {
    size_t insi; 
    assert(pn->data.esz == sizeof(inscode_t));
    for (insi = 0; insi < buflen(&pn->data); ++insi) {
      inscode_t *pic = bufref(&pn->data, insi);
      if (pic->id) {
        for (i = 0; i < n; ++i) { /* linear search is ok here */
          if (i+1 == n && !pi[i]) break; /* ... */
          if (pi[i] != pic->id) continue;
          else {
            node_t *prn = ndbref(ppars, i); seval_t r;
            if (!static_eval(prn, NULL, &r) || r.id != 0)
              n2eprintf(prn, pn, "parameter not a numerical constant");
            switch (pic->in) {
              case IN_F32_CONST:
                if (r.ts == TS_FLOAT) pic->arg.f = r.val.f;
                else n2eprintf(prn, pn, "parameter not a float constant");
                break;
              case IN_F64_CONST:
                if (r.ts == TS_DOUBLE) pic->arg.d = r.val.d;
                else n2eprintf(prn, pn, "parameter not a float constant");
                break;
              default:
                pic->arg.i = r.val.i;
                break;
            }
            pic->id = 0;
          }
        }
      }
    }
  } else {
    size_t i, j;
    for (i = 0; i < ndlen(pn); /* ins or bump */) {
      node_t *pni = ndref(pn, i);
      if (etcbound && node_is_etc(pni) && pn->nt == NT_CALL) {
        assert(ndblen(ppars) >= n-1);
        ndrem(pn, i); /* remove va_etc() */
        for (j = n-1; j < ndblen(ppars); ++j) {
          ndins(pn, i, ndbref(ppars, j));
          ++i;
        }
      } else {
        patch_macro_template(pids, ppars, pni);
        ++i;
      }
    }
  } 
}

static void parse_primary_expr(pws_t *pw, node_t *pn)
{
  tt_t tk = peekt(pw); int startpos = peekpos(pw);
  switch (tk) {
    case TT_LPAR: {     
      dropt(pw);
      if (type_specifier_ahead(pw)) {
        sym_t id;  
        parse_base_type(pw, pn);
        id = parse_declarator(pw, pn); 
        if (id) reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
      } else {
        parse_expr(pw, pn);
      }
      expect(pw, TT_RPAR, ")");
    } break;
    case TT_LONG: /* longs are the same size as pointers; wasm32 */
    case TT_INT: { /* 0 .. 2147483647 (INT32_MIN is written as = -2147483647-1) */
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ul = strtoul(ns, NULL, 0), !errno && ul <= 0x7FFFFFFFUL) {
        pn->val.i = (long)ul; /* no need for for sext: too small? */
      } else reprintf(pw, startpos, "signed int literal overflow"); 
      pn->ts = TS_INT;
      dropt(pw);
    } break;
    case TT_ULONG: /* ulongs are the same size as pointers; wasm32 */
    case TT_UINT: { /* 0 .. 4294967295 */
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ul = strtoul(ns, NULL, 0), !errno && ul <= 0xFFFFFFFFUL) {
        pn->val.u = ul;
      } else reprintf(pw, startpos, "unsigned int literal overflow"); 
      pn->ts = TS_UINT;
      dropt(pw);
    } break;
    case TT_LLONG: { /* 0 .. 9223372036854775807 (INT64_MIN is written as = -9223372036854775807-1) */
      char *ns = pw->tokstr; unsigned long long ull;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ull = strtoull(ns, NULL, 0), !errno && ull <= 0x7FFFFFFFFFFFFFFFULL) {
        pn->val.i = (long long)ull; /* no need for for sext */
      } else reprintf(pw, startpos, "signed long literal overflow"); 
      pn->ts = TS_LLONG;
      dropt(pw);
    } break;
    case TT_ULLONG: { /* 0 .. 18446744073709551615 */
      char *ns = pw->tokstr; unsigned long long ull;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ull = strtoull(ns, NULL, 0), !errno && ull <= 0xFFFFFFFFFFFFFFFFULL) {
        pn->val.u = ull;
      } else reprintf(pw, startpos, "unsigned long literal overflow"); 
      pn->ts = TS_ULLONG;
      dropt(pw);
    } break;
    case TT_FLOAT: {
      char *ns = pw->tokstr; double d;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, d = strtod(ns, NULL), !errno) {
        pn->val.f = (float)d;
      } else reprintf(pw, startpos, "invalid float literal"); 
      pn->ts = TS_FLOAT;
      dropt(pw);
    } break;
    case TT_DOUBLE: {
      char *ns = pw->tokstr; double d;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, d = strtod(ns, NULL), !errno) {
        pn->val.d = d;
      } else reprintf(pw, startpos, "invalid double literal"); 
      pn->ts = TS_DOUBLE;
      dropt(pw);
    } break;
    case TT_CHAR: {
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ul = strtou8cc32(ns, NULL, NULL), !errno && ul <= 0x10FFFFUL) {
        pn->val.i = ul; /* no sext: too small */
      } else reprintf(pw, startpos, "char literal overflow");
      pn->ts = TS_INT;
      dropt(pw);
    } break;
    case TT_LCHAR: {
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ul = strtou8cc32(ns, NULL, NULL), !errno && ul <= 0x10FFFFUL) {
        pn->val.i = ul; /* no sext: too small */
      } else reprintf(pw, startpos, "long char literal overflow");
      pn->ts = TS_LONG;
      dropt(pw);
    } break;
    case TT_STRING: {
      char *ns = pw->tokstr; unsigned long ul; bool raw;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      while (*ns && (errno = 0, ul = strtou8cc32(ns, &ns, &raw), !errno && ul <= 0x10FFFFUL)) {
        if (raw) { assert(ul <= 0xFFL); cbputc(ul, &pn->data); }
        else cbputlc(ul, &pn->data); /* in utf-8 */
      }
      cbputc(0, &pn->data); /* terminating zero char, C-style */
      if (*ns) 
        reprintf(pw, startpos+(int)(ns-pw->tokstr), "string literal char overflow");
      pn->ts = TS_STRING;
      dropt(pw);
    } break;
    case TT_LSTRING: {
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      while (*ns && (errno = 0, ul = strtou8cc32(ns, &ns, NULL), !errno && ul <= 0x10FFFFUL)) {
        cbput4le((unsigned)ul, &pn->data); /* 4 bytes, in LE order */
      }
      cbput4le(0, &pn->data); /* terminating zero wchar, C-style */
      if (*ns) reprintf(pw, startpos+(int)(ns-pw->tokstr), "long string literal char overflow");
      pn->ts = TS_LSTRING;
      dropt(pw);
    } break;
    case TT_IDENTIFIER: {
      ndset(pn, NT_IDENTIFIER, pw->id, startpos); 
      pn->name = getid(pw);
    } break;
    case TT_ELLIPSIS: { /* same as va_etc() */
      dropt(pw);
      ndset(pn, NT_INTRCALL, pw->id, startpos);
      pn->intr = INTR_VAETC;
    } break;
    case TT_ENUM_NAME: {
      /* todo: cast to enum type for extra type checking */
      int info = 42; lookup_symbol(pw->tokstr, &info);
      ndset(pn, NT_LITERAL, pw->id, startpos);
      pn->ts = TS_INT; pn->val.i = info;
      dropt(pw);
    } break;
    case TT_MACRO_NAME: {
      node_t *pdn; int info = -1; 
      lookup_symbol(pw->tokstr, &info);
      if (info < 0) reprintf(pw, startpos, "use of undefined macro??");
      assert(info >= 0 && info < (int)ndblen(&g_nodes));
      pdn = ndbref(&g_nodes, (size_t)info);
      assert(pdn->nt == NT_MACRODEF);
      dropt(pw);
      if (ndlen(pdn) == 1) {
        /* parameterless macro */
        ndcpy(pn, ndref(pdn, 0));
      } else if (ndlen(pdn) == 2) {
        /* parameterized macro */
        node_t *pfn = ndref(pdn, 0); 
        buf_t ids; ndbuf_t pars; size_t i;
        bool variadic = false;
        bufinit(&ids, sizeof(sym_t)); ndbinit(&pars); 
        assert(pfn->nt == NT_TYPE && pfn->ts == TS_FUNCTION); 
        expect(pw, TT_LPAR, "(");
        while (peekt(pw) != TT_RPAR) {
          parse_assignment_expr(pw, ndbnewbk(&pars));
          if (peekt(pw) == TT_COMMA) dropt(pw);
          else break;
        }
        expect(pw, TT_RPAR, ")");
        if (ndlen(pfn) > 1 && ndref(pfn, ndlen(pfn)-1)->name == 0) 
          variadic = true; 
        if (!variadic && ndlen(pfn) != ndblen(&pars)+1)
          reprintf(pw, startpos, "invalid number of parameters to macro");
        if (variadic && ndlen(pfn) > ndblen(&pars)+2) 
          reprintf(pw, startpos, "not enough parameters to variadic macro");
        for (i = 1/* skip null type */; i < ndlen(pfn); ++i) {
          node_t *pin = ndref(pfn, i);
          assert(pin->nt == NT_VARDECL);
          assert(pin->name != 0 || i+1 == ndlen(pfn)); /* ... is last */
          *(sym_t*)bufnewbk(&ids) = pin->name;
        }
        ndcpy(pn, ndref(pdn, 1));
        patch_macro_template(&ids, &pars, pn);
        buffini(&ids); ndbfini(&pars); 
      } else assert(false);
      /* use current loc info on outer level */
      pn->pwsid = pw->id; pn->startpos = startpos;
    } break;
    case TT_INTR_NAME: {
      /* intrinsic application */
      int intr = INTR_NONE; lookup_symbol(pw->tokstr, &intr);
      if (intr == INTR_NONE) 
        reprintf(pw, startpos, "use of undefined intrinsic??");
      ndset(pn, NT_INTRCALL, pw->id, startpos);
      dropt(pw);
      pn->intr = (intr_t)intr;
      switch (intr) {
        case INTR_SIZEOF: case INTR_ALIGNOF: case INTR_COUNTOF: { /* (type/expr) */
          node_t *ptn = ndnewbk(pn);
          expect(pw, TT_LPAR, "(");
          if (type_specifier_ahead(pw)) {
            parse_base_type(pw, ptn);
            if (parse_declarator(pw, ptn)) 
              reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
          } else {
            parse_expr(pw, ptn);
          }
          expect(pw, TT_RPAR, ")"); 
        } break;
        case INTR_VAETC: { /* () */
          expect(pw, TT_LPAR, "(");
          expect(pw, TT_RPAR, ")"); 
        } break;
        case INTR_VAARG: { /* (id, type) */
          node_t *ptn = ndnewbk(pn);
          expect(pw, TT_LPAR, "(");
          ndset(ptn, NT_IDENTIFIER, pw->id, peekpos(pw));
          ptn->name = getid(pw); 
          expect(pw, TT_COMMA, ",");
          ptn = ndnewbk(pn);
          parse_base_type(pw, ptn);
          if (parse_declarator(pw, ptn)) 
            reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
          if (ptn->ts != TS_PTR && ptn->ts != TS_ENUM && !(TS_INT <= ptn->ts && ptn->ts <= TS_DOUBLE))
            reprintf(pw, startpos, "unsuported vararg type");
          expect(pw, TT_RPAR, ")"); 
        } break;
        case INTR_OFFSETOF: { /* (type, id) */
          node_t *ptn = ndnewbk(pn);
          expect(pw, TT_LPAR, "(");
          if (type_specifier_ahead(pw)) {
            parse_base_type(pw, ptn);
            if (parse_declarator(pw, ptn)) 
              reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
          } else {
            parse_assignment_expr(pw, ptn);
          }
          expect(pw, TT_COMMA, ",");
          ptn = ndnewbk(pn);
          ndset(ptn, NT_IDENTIFIER, pw->id, peekpos(pw));
          ptn->name = getid(pw); 
          expect(pw, TT_RPAR, ")"); 
        } break;
        case INTR_GENERIC: { /* (expr/type, type: expr, ..., default: expr) */
          node_t *pgn = ndnewbk(pn); bool bytype = true;
          tt_t vse = TT_RPAR;
          expect(pw, TT_LPAR, "(");
          if (type_specifier_ahead(pw)) {
            parse_base_type(pw, pgn);
            if (parse_declarator(pw, pgn)) 
              reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
          } else {
            parse_assignment_expr(pw, pgn);
          }
          if (node_is_countofetc(pgn)) bytype = false;
          if (peekt(pw) == TT_RPAR) {
            dropt(pw); expect(pw, TT_LBRC, "{");
            vse = TT_RBRC;
          }
          while (peekt(pw) != vse) {
            node_t *ptn = ndnewbk(pn);
            int defpos = 0;
            if (vse == TT_RPAR) expect(pw, TT_COMMA, ",");
            if (peekt(pw) == TT_DEFAULT_KW) {
              defpos = peekpos(pw);
              dropt(pw); ptn->nt = NT_NULL;
            } else {
              if (vse != TT_RPAR) expect(pw, TT_CASE_KW, "case");
              if (bytype) {
                parse_base_type(pw, ptn);
                if (parse_declarator(pw, ptn)) 
                  reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
              } else {
                long n; errno = 0;
                if (peekt(pw) == TT_INT && (n = atol(pw->tokstr)) >= 0 && (n <= INT_MAX) && !errno) {
                  ptn->nt = NT_LITERAL; ptn->ts = TS_INT; ptn->val.i = n;
                  dropt(pw);
                } else expect(pw, TT_INT, "nonnegative integer literal");
              } 
            }
            expect(pw, TT_COLON, ":");
            parse_assignment_expr(pw, ndnewbk(pn));
            if (vse != TT_RPAR) expect(pw, TT_SEMICOLON, ";");
            if (defpos && peekt(pw) != vse)
              reprintf(pw, defpos, "generic default: variant should be last");
          }          
          expect(pw, vse, vse == TT_RPAR ? ")" : "}"); 
        } break;
        case INTR_ALLOCA: case INTR_SASSERT: { /* (expr ...) */
          size_t n = 0;
          expect(pw, TT_LPAR, "(");
          while (peekt(pw) != TT_RPAR) {
            parse_assignment_expr(pw, ndnewbk(pn));
            ++n; 
            if (peekt(pw) != TT_COMMA) break;
            dropt(pw); 
          }
          expect(pw, TT_RPAR, ")"); 
          if ((intr == INTR_ALLOCA && n != 1) || (intr == INTR_SASSERT && !(n == 1 || n == 2)))  
           reprintf(pw, startpos, "unexpected arguments for %s", intr_name(pn->intr));
        } break;
        case INTR_DEFINED: { /* (expr ...) */
          tt_t idt; int i = 0;
          expect(pw, TT_LPAR, "(");
          idt = peekt(pw);
          if (idt == TT_MACRO_NAME) i = 1;
          else if (idt == TT_IDENTIFIER) i = 0;
          else reprintf(pw, startpos, "macro name or identifier expected");
          dropt(pw);
          expect(pw, TT_RPAR, ")");
          pn->nt = NT_LITERAL; pn->intr = INTR_NONE;
          pn->ts = TS_INT; pn->val.i = i;
        } break;
        default: assert(false);
      }
    } break;
    default:
      reprintf(pw, startpos, "valid expression expected");
  }
}

static bool concat_strlit(node_t *ptn, node_t *psn)
{
  if (ptn->ts != psn->ts) return false; /* todo: cast "foo" to L"foo" and concat */
  if (ptn->ts == TS_LSTRING) { bufpopbk(&ptn->data); bufpopbk(&ptn->data); bufpopbk(&ptn->data); }
  bufpopbk(&ptn->data); /* drop one or four zero bytes at end */
  bufcat(&ptn->data, &psn->data); /* include terminating zero in psn */
  return true;
}

static void parse_concat_expr(pws_t *pw, node_t *pn)
{
  parse_primary_expr(pw, pn);
  if (pn->ts != TS_STRING && pn->ts != TS_LSTRING) return;
  while (true) {
    tt_t tk = peekt(pw); int startpos = peekpos(pw);
    if (tk == TT_STRING || tk == TT_LSTRING || tk == TT_MACRO_NAME) {
      node_t nd = mknd();
      parse_primary_expr(pw, &nd);
      if (!concat_strlit(pn, &nd)) reprintf(pw, startpos, "invalid string literal continuation");
      ndfini(&nd);
    } else break;
  }          
}

static bool postfix_operator_ahead(pws_t *pw)
{
  switch (peekt(pw)) {
    case TT_LBRK:      case TT_LPAR:        case TT_DOT:
    case TT_ARROW:     case TT_PLUS_PLUS:   case TT_MINUS_MINUS:
      return true;
    default:;
  }
  return false;
}

static void parse_postfix_expr(pws_t *pw, node_t *pn)
{
  parse_concat_expr(pw, pn);
  /* cast type (t) can't be postfixed */
  if (pn->nt == NT_TYPE) return;
  /* wrap pn into postfix operators */
  while (postfix_operator_ahead(pw)) {
    switch (pw->ctk) {
      case TT_LBRK: {
        node_t nd = mknd();
        dropt(pw);
        parse_expr(pw, &nd);
        expect(pw, TT_RBRK, "]");
        wrap_subscript(pn, &nd);
        ndfini(&nd);
      } break;      
      case TT_LPAR: {
        node_t nd = mknd();
        dropt(pw);
        ndset(&nd, NT_CALL, pn->pwsid, pn->startpos);
        ndswap(pn, ndnewbk(&nd));
        while (peekt(pw) != TT_RPAR) {
          parse_assignment_expr(pw, ndnewbk(&nd));
          if (peekt(pw) != TT_COMMA) break;
          dropt(pw); 
        }
        expect(pw, TT_RPAR, ")");
        ndswap(pn, &nd);
        ndfini(&nd);
      } break;        
      case TT_DOT: case TT_ARROW: {
        tt_t op = pw->ctk;
        dropt(pw);
        wrap_postfix_operator(pn, op, gettag(pw));
      } break;
      case TT_PLUS_PLUS: case TT_MINUS_MINUS: {
        dropt(pw);
        wrap_postfix_operator(pn, pw->ctk, 0);
      } break;
      default: assert(false);
    }  
  }
}
 
static void parse_unary_expr(pws_t *pw, node_t *pn)
{
  int startpos = peekpos(pw);
  switch (peekt(pw)) {
    case TT_PLUS_PLUS: case TT_MINUS_MINUS:
    case TT_STAR: case TT_PLUS: case TT_MINUS:
    case TT_AND: case TT_TILDE: case TT_NOT: {
      tt_t tk = pw->ctk; dropt(pw);
      parse_cast_expr(pw, pn);
      if (pn->nt == NT_TYPE) reprintf(pw, startpos, "unexpected type cast");
      wrap_unary_operator(pn, startpos, tk);
    } break;
    default: {
      parse_postfix_expr(pw, pn);
    } break;  
  }
}  

static long long parse_asm_signed(pws_t *pw)
{
  node_t xnd = mknd(), lnd = mknd();
  int startpos = peekpos(pw);
  parse_primary_expr(pw, &xnd);
  if (arithmetic_eval(&xnd, NULL, &lnd) && lnd.nt == NT_LITERAL) {
    switch (lnd.ts) {
      case TS_INT: case TS_LONG: case TS_LLONG: {
        long long ll = lnd.val.i;
        ndfini(&xnd), ndfini(&lnd);
        return ll;
      }
      default:;
    }
  }
  reprintf(pw, startpos, "signed argument expected");  
  ndfini(&xnd), ndfini(&lnd);
  return 0;
}

static unsigned long long parse_asm_unsigned(pws_t *pw, unsigned long long maxval)
{
  node_t xnd = mknd(), lnd = mknd();
  int startpos = peekpos(pw);
  parse_primary_expr(pw, &xnd);
  if (arithmetic_eval(&xnd, NULL, &lnd) && lnd.nt == NT_LITERAL) {
    switch (lnd.ts) {
      case TS_INT: case TS_LONG: case TS_LLONG:
        if (lnd.val.i < 0) 
          reprintf(pw, startpos, "unsigned argument is negative");
        /* else fall thru */
      case TS_UINT: case TS_ULONG: case TS_ULLONG: {
        unsigned long long ull = lnd.val.u;
        ndfini(&xnd), ndfini(&lnd);
        if (ull <= maxval) return ull;
        reprintf(pw, startpos, "unsigned argument is too big");  
      }
      default:;
    }
  }
  reprintf(pw, startpos, "unsigned argument expected");  
  ndfini(&xnd), ndfini(&lnd);
  return 0;
}

static double parse_asm_double(pws_t *pw)
{
  node_t xnd = mknd(), lnd = mknd();
  int startpos = peekpos(pw);
  parse_primary_expr(pw, &xnd);
  if (arithmetic_eval(&xnd, NULL, &lnd) && lnd.nt == NT_LITERAL) {
    switch (lnd.ts) {
      case TS_FLOAT: case TS_DOUBLE: {
        double d = lnd.ts == TS_FLOAT ? lnd.val.f : lnd.val.d;
        ndfini(&xnd), ndfini(&lnd);
        return d;
      }
      default:;
    }
  }
  reprintf(pw, startpos, "floating-point argument expected");  
  ndfini(&xnd), ndfini(&lnd);
  return 0;
}

static void parse_asm_instr(pws_t *pw, node_t *pan)
{
  if (peekt(pw) == TT_LPAR) {
    /* nested asm */
    node_t nd = mknd(), *psn = ndnewbk(pan);
    inscode_t *pic = bufnewbk(&pan->data);
    sym_t id; int startpos = peekpos(pw);
    pic->in = IN_PLACEHOLDER;
    expect(pw, TT_LPAR, "(");
    parse_base_type(pw, &nd);
    id = parse_declarator(pw, &nd); 
    if (id) reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
    expect(pw, TT_RPAR, ")");
    parse_asm_code(pw, psn, &nd);
    ndfini(&nd);
  } else {
    /* declaration or regular instr */
    inscode_t *pic = bufnewbk(&pan->data);
    cbuf_t cb = mkcb(); tt_t tt; 
    instr_t in; insig_t is;
    tt = peekt(pw);
    if (tt == TT_IDENTIFIER || 
      (TT_AUTO_KW <= tt && tt <= TT_WHILE_KW) ||
      (TT_TYPE_NAME <= tt && tt <= TT_INTR_NAME)) {
      bool dot = peekc(pw) == '.';
      cbputs(pw->tokstr, &cb); dropt(pw);
      if (dot) {
        expect(pw, TT_DOT, "."); cbputc('.', &cb); 
        tt = peekt(pw);
        if (tt == TT_IDENTIFIER || 
          (TT_AUTO_KW <= tt && tt <= TT_WHILE_KW) ||
          (TT_TYPE_NAME <= tt && tt <= TT_INTR_NAME)) {
          cbputs(pw->tokstr, &cb); dropt(pw);
        }
      }
    }
    if (streql(cbdata(&cb), "register")) {
      valtype_t vt = 0;
      tt = peekt(pw);
      if (streql(pw->tokstr, "f64")) vt = VT_F64;
      else if (streql(pw->tokstr, "f32")) vt = VT_F32;
      else if (streql(pw->tokstr, "i64")) vt = VT_I64;
      else if (streql(pw->tokstr, "i32")) vt = VT_I32;
      else if (streql(pw->tokstr, "funcref")) vt = RT_FUNCREF;
      else if (streql(pw->tokstr, "externref")) vt = RT_EXTERNREF;
      else reprintf(pw, pw->pos, "unexpected register type");
      dropt(pw);
      pic->in = IN_REGDECL;
      pic->id = getid(pw);
      pic->arg.u = vt;  
      cbfini(&cb);
      return;
    }
    if (!cblen(&cb)) reprintf(pw, pw->pos, "instruction name expected");
    in = name_instr(cbdata(&cb));
    if (in == IN_PLACEHOLDER) reprintf(pw, pw->pos, "unknown instruction");
    pic->in = in;
    switch (is = instr_sig(in)) {
      case INSIG_NONE:
        break;
      case INSIG_BT:   case INSIG_L:    case INSIG_RD:
      case INSIG_XL:   case INSIG_XG:   case INSIG_XT:
      case INSIG_T:    case INSIG_I32:  case INSIG_I64:
      case INSIG_RF:
        if (peekt(pw) == TT_IDENTIFIER) {
          if (peekc(pw) == ':') { /* mod:id */
            pic->arg2.mod = getid(pw);
            expect(pw, TT_COLON, ":");
          } 
          pic->id = getid(pw);
        } else {
          pic->arg.i = parse_asm_signed(pw);
        }
        break;
      case INSIG_X_Y:
        if (peekt(pw) == TT_IDENTIFIER) pic->id = getid(pw);
        else pic->arg.u = parse_asm_unsigned(pw, INT32_MAX);
        pic->arg2.u = (unsigned)parse_asm_unsigned(pw, INT32_MAX);
        break;
      case INSIG_F32: case INSIG_F64:
        if (peekt(pw) == TT_IDENTIFIER) {
          if (peekc(pw) == ':') { /* mod:id */
            pic->arg2.mod = getid(pw);
            expect(pw, TT_COLON, ":");
          } 
          pic->id = getid(pw);
        } else if (is == INSIG_F32) {
          pic->arg.f = (float)parse_asm_double(pw);
        } else {
          pic->arg.d = parse_asm_double(pw);
        }
        break;
      case INSIG_MEMARG: {
        if (ahead(pw, "offset")) {
          dropt(pw); expect(pw, TT_ASN, "=");
          pic->arg.u = parse_asm_unsigned(pw, INT32_MAX);
        } else pic->arg.u = 0;
        if (ahead(pw, "align")) {
          unsigned long long align; unsigned a = 0;
          dropt(pw); expect(pw, TT_ASN, "=");
          align = parse_asm_unsigned(pw, 16);
          switch ((int)align) { /* fixme: use ntz? */
            case 1:  a = 0; break;
            case 2:  a = 1; break;
            case 4:  a = 2; break;
            case 8:  a = 3; break;
            case 16: a = 4; break;
          }
          pic->arg2.u = a;
        } else pic->arg2.u = 0;
      } break;
      case INSIG_LANEIDX:
        if (peekt(pw) == TT_IDENTIFIER) pic->id = getid(pw);
        else pic->arg.u = parse_asm_unsigned(pw, 16);
        break;
      case INSIG_MEMARG_LANEIDX: {
        unsigned a = 0;
        if (ahead(pw, "offset")) {
          dropt(pw); expect(pw, TT_ASN, "=");
          pic->arg2.ux2[0] = (unsigned)parse_asm_unsigned(pw, INT32_MAX);
        } else pic->arg2.ux2[0] = 0;
        if (ahead(pw, "align")) {
          unsigned long long align; 
          dropt(pw); expect(pw, TT_ASN, "=");
          align = parse_asm_unsigned(pw, 16);
          switch ((int)align) { /* fixme: use ntz? */
            case 1:  a = 0; break;
            case 2:  a = 1; break;
            case 4:  a = 2; break;
            case 8:  a = 3; break;
            case 16: a = 4; break;
          }
          pic->arg2.ux2[1] = a;
        } else pic->arg2.ux2[1] = 0;
        if (peekt(pw) == TT_IDENTIFIER) pic->id = getid(pw);
        else pic->arg.u = parse_asm_unsigned(pw, 16);
      } break;
      case INSIG_LANEIDX16: {
        size_t i; unsigned long long lidxv = 0;
        if (peekt(pw) == TT_IDENTIFIER) { 
          pic->id = getid(pw); /* packed */
        } else for (i = 0; i < 16; ++i) {
          unsigned long long lidx = parse_asm_unsigned(pw, i == 0 ? UINT32_MAX : 16);
          lidxv = (lidxv << 4) | lidx;
          if (peekt(pw) == TT_COMMA || peekt(pw) == TT_RPAR) break;
        }
        pic->arg.u = lidxv;
      } break;
      case INSIG_LS_L: case INSIG_PR:
        /* fixme? */
      default:
        reprintf(pw, pw->pos, "inline assembly: instruction not supported");     
    } 
    cbfini(&cb);
  }
}

static void parse_asm_code(pws_t *pw, node_t *pn, node_t *ptn)
{
  assert(ptn->nt == NT_TYPE);
  ndset(pn, NT_ACODE, pw->id, pw->pos);
  ndswap(ptn, ndnewbk(pn));
  expect(pw, TT_ASM_KW, "asm");
  expect(pw, TT_LPAR, "(");
  while (peekt(pw) != TT_RPAR) {
    parse_asm_instr(pw, pn);
    if (peekt(pw) == TT_COMMA) dropt(pw);
    else break;
  }
  expect(pw, TT_RPAR, ")");
  if (peekt(pw) == TT_LPAR) {
    node_t nd = mknd();
    dropt(pw);
    ndset(&nd, NT_INTRCALL, pn->pwsid, pn->startpos);
    nd.intr = INTR_ACODE;
    ndswap(pn, ndnewbk(&nd));
    while (peekt(pw) != TT_RPAR) {
      parse_assignment_expr(pw, ndnewbk(&nd));
      if (peekt(pw) != TT_COMMA) break;
      dropt(pw); 
    }
    expect(pw, TT_RPAR, ")");
    ndswap(pn, &nd);
    ndfini(&nd);
  }
}

static void patch_type_array_sizes(const node_t *psn, node_t *ptn)
{
  assert(ptn->nt == NT_TYPE);
  if (ptn->ts == TS_ARRAY) {
    if (psn->nt == NT_DISPLAY && ndcref(psn, 0)->ts == TS_ARRAY) {
      const node_t *pasn = ndcref(ndcref(psn, 0), 1);
      node_t *petn = ndref(ptn, 0), *pvsn = ndref(ptn, 1);
      if (pvsn->nt == NT_NULL && pasn->nt != NT_NULL) ndcpy(pvsn, pasn);
      /* try to use first nested display to patch nested size */
      if (petn->ts == TS_ARRAY && ndlen(psn) > 1)
        patch_type_array_sizes(ndcref(psn, 1), petn);
    } else if (psn->nt == NT_LITERAL && (psn->ts == TS_STRING || psn->ts == TS_LSTRING)) {
      node_t *pvsn = ndref(ptn, 1); 
      if (pvsn->nt == NT_NULL) {
        size_t icnt = buflen(&psn->data); 
        if (psn->ts == TS_LSTRING) icnt /= 4;
        pvsn->nt = NT_LITERAL;
        pvsn->ts = TS_INT;
        pvsn->val.i = (long)icnt; /* with 0 terminator char */
      }
    }
  }
}

void patch_display_array_sizes(node_t *psn, const node_t *ptn)
{
  if (psn->nt == NT_DISPLAY) {
    assert(ndlen(psn) > 0 && ndref(psn, 0)->nt == NT_TYPE);
    if (ptn->ts == TS_ARRAY) {
      node_t tn, *pasn, *petn;
      ndicpy(&tn, ptn);
      assert(ndlen(&tn) == 2);
      pasn = ndcref(&tn, 1); 
      if (pasn->nt == NT_NULL) {
        pasn->nt = NT_LITERAL;
        pasn->ts = TS_INT;
        pasn->val.i = (long)ndlen(psn)-1; /* sans type */
      }
      petn = ndcref(&tn, 0);
      if (petn->ts == TS_ARRAY) {
        size_t i, n = ndlen(psn);
        for (i = 1; i < n; ++i) {
          node_t *psni = ndref(psn, i);
          patch_display_array_sizes(psni, petn); 
          if (i == 1) patch_type_array_sizes(psni, petn);
        }
      }
      /* replace placeholder type */
      ndswap(ndref(psn, 0), &tn);
      ndfini(&tn);
    } else {
      /* just replace placeholder type */
      node_t *pstn = ndref(psn, 0);
      if (pstn->ts == TS_VOID) ndcpy(pstn, ptn);
    }
  }
}

static void parse_cast_expr(pws_t *pw, node_t *pn)
{
  parse_unary_expr(pw, pn);
  if (pn->nt == NT_TYPE) {
    node_t nd = mknd();
    switch (peekt(pw)) {
      case TT_LBRC: {
        parse_initializer(pw, &nd);
        assert(nd.nt == NT_DISPLAY);
        /* patch display's array size if needed */
        patch_display_array_sizes(&nd, pn);
        ndswap(pn, &nd);
      } break;
      case TT_ASM_KW: {
        ndswap(pn, &nd);
        parse_asm_code(pw, pn, &nd);
      } break;
      default: {
        parse_cast_expr(pw, &nd);
        wrap_cast_expr(pn, &nd);
      } break;
    }
    ndfini(&nd);
  }
}

static tt_t getop(pws_t *pw, int deft)
{
  int dt = 0;
  switch (peekt(pw)) {
    case TT_STAR: case TT_SLASH: case TT_REM:        dt = 'm'; break;
    case TT_PLUS: case TT_MINUS:                     dt = 'a'; break;
    case TT_SHL: case TT_SHR:                        dt = 's'; break;
    case TT_LT: case TT_LE: case TT_GT: case TT_GE:  dt = 'r'; break;
    case TT_EQ: case TT_NE:                          dt = 'e'; break;
    case TT_AND:                                     dt = '&'; break;
    case TT_XOR:                                     dt = '^'; break;
    case TT_OR:                                      dt = '|'; break;
    case TT_AND_AND:                                 dt = 'c'; break;      
    case TT_OR_OR:                                   dt = 'd'; break;      
    default:;
  }
  return deft == dt ? (dropt(pw), pw->ctk) : TT_EOF;
} 

static void parse_binary_expr(pws_t *pw, node_t *pn, int deft)
{
  tt_t op;
  switch (deft) {
    case 'm': parse_cast_expr(pw, pn); break;
    case 'a': parse_binary_expr(pw, pn, 'm'); break;
    case 's': parse_binary_expr(pw, pn, 'a'); break;
    case 'r': parse_binary_expr(pw, pn, 's'); break;
    case 'e': parse_binary_expr(pw, pn, 'r'); break;
    case '&': parse_binary_expr(pw, pn, 'e'); break;
    case '^': parse_binary_expr(pw, pn, '&'); break;
    case '|': parse_binary_expr(pw, pn, '^'); break;
    case 'c': parse_binary_expr(pw, pn, '|'); break;
    case 'd': parse_binary_expr(pw, pn, 'c'); break;
    default: assert(false);
  }
  while ((op = getop(pw, deft)) != TT_EOF) {
    node_t nd = mknd();
    switch (deft) {
      case 'm': parse_cast_expr(pw, &nd); break;
      case 'a': parse_binary_expr(pw, &nd, 'm'); break;
      case 's': parse_binary_expr(pw, &nd, 'a'); break;
      case 'r': parse_binary_expr(pw, &nd, 's'); break;
      case 'e': parse_binary_expr(pw, &nd, 'r'); break;
      case '&': parse_binary_expr(pw, &nd, 'e'); break;
      case '^': parse_binary_expr(pw, &nd, '&'); break;
      case '|': parse_binary_expr(pw, &nd, '^'); break;
      case 'c': parse_binary_expr(pw, &nd, '|'); break;
      case 'd': parse_binary_expr(pw, &nd, 'c'); break;
      default: assert(false);
    }
    wrap_binary(pn, op, &nd);
    ndfini(&nd);
  }
}

static void parse_conditional_expr(pws_t *pw, node_t *pn)
{
  parse_binary_expr(pw, pn, 'd');
  if (peekt(pw) == TT_QMARK) {
    node_t ndt = mknd(), ndf = mknd();
    dropt(pw);
    parse_expr(pw, &ndt);
    expect(pw, TT_COLON, ":");
    parse_conditional_expr(pw, &ndf);
    wrap_conditional(pn, &ndt, &ndf);
    ndfini(&ndt), ndfini(&ndf);
  }
}

static void parse_assignment_expr(pws_t *pw, node_t *pn)
{
  parse_conditional_expr(pw, pn);
  switch (peekt(pw)) {
    case TT_ASN:      case TT_AND_ASN:  case TT_OR_ASN:
    case TT_XOR_ASN:  case TT_REM_ASN:  case TT_SLASH_ASN:
    case TT_STAR_ASN: case TT_PLUS_ASN: case TT_MINUS_ASN:
    case TT_SHL_ASN:  case TT_SHR_ASN: {
      tt_t op; node_t nd = mknd(); 
      op = pw->ctk; dropt(pw);
      parse_assignment_expr(pw, &nd);
      wrap_assignment(pn, op, &nd);
      ndfini(&nd);
    }
    default:;
  }
}

static void parse_expr(pws_t *pw, node_t *pn)
{
  parse_assignment_expr(pw, pn);
  while (peekt(pw) == TT_COMMA) {
    node_t nd = mknd(); 
    dropt(pw);
    parse_assignment_expr(pw, &nd);
    wrap_comma(pn, &nd);
    ndfini(&nd);
  }
}

/* fill pn with definition of typedef'd type tn */
static void load_typedef_type(pws_t *pw, node_t *pn, sym_t tn)
{
  int info = -1, pwsid = pn->pwsid, startpos = pn->startpos; 
  lookup_symbol(symname(tn), &info);
  if (info < 0) reprintf(pw, pw->pos, "can't find definition of type: %s", symname(tn));
  assert(info >= 0 && info < (int)ndblen(&g_nodes));
  ndcpy(pn, ndbref(&g_nodes, (size_t)info));
  pn->pwsid = pwsid, pn->startpos = startpos; 
  assert(pn->nt == NT_TYPE);
}

/* parse enum body (name? + enumlist?) */
static void parse_enum_body(pws_t *pw, node_t *pn)
{
  ndset(pn, NT_TYPE, pw->id, peekpos(pw));
  pn->ts = TS_ENUM;
  if (peekt(pw) == TT_IDENTIFIER) pn->name = getid(pw);
  if (peekt(pw) == TT_LBRC) {
    int curval = 0;
    dropt(pw);
    while (peekt(pw) != TT_RBRC) {
      node_t *pni = ndnewbk(pn), *pnv; int *pi;
      ndset(pni, NT_VARDECL, pw->id, peekpos(pw));
      pni->name = getid(pw);
      pi = bufbsearch(&g_syminfo, &pni->name, &int_cmp);
      if (pi) neprintf(pni, "enum constant name is already in use");
      pnv = ndnewbk(pni);
      if (peekt(pw) == TT_ASN) {
        dropt(pw);
        parse_conditional_expr(pw, pnv);
        if (!arithmetic_eval_to_int(pnv, NULL, &curval))
          neprintf(pnv, "invalid enum initializer (int constant expected)");
      }
      ndset(pnv, NT_LITERAL, pw->id, pni->startpos);
      pnv->ts = TS_INT; pnv->val.i = curval;
      intern_symbol(symname(pni->name), TT_ENUM_NAME, curval);  
      if (peekt(pw) == TT_COMMA) dropt(pw);
      curval += 1; 
    }
    expect(pw, TT_RBRC, "}");
  }  
  if (!pn->name && !ndlen(pn)) {
    reprintf(pw, pn->startpos, "incomplete enum");
  } else if (pn->name && ndlen(pn)) {
    /* register this enum type for lazy fetch */
    const char *tag = "enum"; tt_t tt = TT_ENUM_KW;
    sym_t s = internf("%s %s", tag, symname(pn->name));
    int *pinfo = bufbsearch(&g_syminfo, &s, &int_cmp);
    if (pinfo) {
      reprintf(pw, pn->startpos, "redefinition of %s", symname(s));
    } else {
      intern_symbol(symname(s), tt, (int)ndblen(&g_nodes));
      ndcpy(ndbnewbk(&g_nodes), pn);
    }
  }
}

/* parse simd body <T, N> */
static void parse_simd_body(pws_t *pw, node_t *pn)
{
  node_t *ptn; int startpos; 
  ndset(pn, NT_TYPE, pw->id, peekpos(pw));
  pn->ts = TS_V128;
  expect(pw, TT_LT, "<");
  ptn = ndnewbk(pn); startpos = peekpos(pw);
  parse_base_type(pw, ptn);
  if (ptn->ts < TS_BOOL || ptn->ts > TS_DOUBLE)
    reprintf(pw, startpos, "unexpected base type in simd<T, N> type specifier");
  expect(pw, TT_COMMA, ","); startpos = peekpos(pw);
  if (ahead(pw, "2") || ahead(pw, "4") || ahead(pw, "8") || ahead(pw, "16")) {
    node_t *pvn = ndnewbk(pn); long n = atol(pw->tokstr); dropt(pw);
    ndset(pvn, NT_LITERAL, pw->id, startpos); pvn->ts = TS_INT; pvn->val.i = n; 
    switch (ptn->ts) {
      case TS_BOOL:                  n = 128; break; /* all sizes work */
      case TS_CHAR:  case TS_UCHAR:  n *=  8; break;
      case TS_SHORT: case TS_USHORT: n *= 16; break;
      case TS_INT:   case TS_UINT:   n *= 32; break;
      case TS_LONG:  case TS_ULONG:  n *= 32; break; /* wasm32 */
      case TS_LLONG: case TS_ULLONG: n *= 64; break;
      case TS_FLOAT:                 n *= 32; break; 
      case TS_DOUBLE:                n *= 64; break;
      default: assert(false); 
    }
    if (n != 128) reprintf(pw, startpos, "unexpected type/count in simd<T, N> type specifier");
  } else reprintf(pw, startpos, "unexpected count in simd<T, N> type specifier");
  expect(pw, TT_GT, ">");
}

/* parse struct or union body (name? + declist?) */
static void parse_sru_body(pws_t *pw, ts_t sru, node_t *pn)
{
  size_t i;
  ndset(pn, NT_TYPE, pw->id, peekpos(pw));
  pn->ts = sru;
  if (peekt(pw) == TT_IDENTIFIER) pn->name = getid(pw);
  if (peekt(pw) == TT_LBRC) {
    dropt(pw);
    while (peekt(pw) != TT_RBRC) {
      sc_t sc = parse_decl(pw, &pn->body);
      if (sc != SC_NONE)
        reprintf(pw, pn->startpos, "unexpected storage declarations in struct/union"); 
      expect(pw, TT_SEMICOLON, ";"); /* no block-level function definitions */
    }
    expect(pw, TT_RBRC, "}");
  }  
  for (i = 0; i < ndlen(pn); ++i) {
    node_t *pni = ndref(pn, i);
    if (pni->nt == NT_VARDECL && ndlen(pni) > 1) 
      reprintf(pw, pni->startpos, "unexpected initializer in struct/union"); 
  }
  if (!pn->name && !ndlen(pn)) {
    reprintf(pw, pn->startpos, "incomplete struct/union");
  } else if (pn->name && ndlen(pn)) {
    /* register this struct/union type for lazy fetch */
    const char *tag = (sru == TS_STRUCT) ? "struct" : "union";
    tt_t tt = (sru == TS_STRUCT) ? TT_STRUCT_KW : TT_UNION_KW;
    sym_t s = internf("%s %s", tag, symname(pn->name));
    int *pinfo = bufbsearch(&g_syminfo, &s, &int_cmp);
    if (pinfo) {
      reprintf(pw, pn->startpos, "redefinition of %s", symname(s));
    } else {
      intern_symbol(symname(s), tt, (int)ndblen(&g_nodes));
      ndcpy(ndbnewbk(&g_nodes), pn);
    }
  }
}

/* base types are limited to scalars/named types */
static void parse_base_type(pws_t *pw, node_t *pn)
{
  int v = 0, c = 0, s = 0, i = 0, l = 0, f = 0, d = 0;
  int sg = 0, ug = 0, en = 0, si = 0, st = 0, un = 0, ty = 0;   
  int pos = peekpos(pw); sym_t tn = 0;
  while (type_specifier_ahead(pw)) {
    switch (pw->ctk) {
      case TT_VOID_KW:     v = 1; break;
      case TT_CHAR_KW:     c = 1; break;
      case TT_SHORT_KW:    s = 1; break;
      case TT_INT_KW:      i = 1; break;
      case TT_LONG_KW:     ++l; break;
      case TT_SIGNED_KW:   sg = 1; break;
      case TT_UNSIGNED_KW: ug = 1; break;
      case TT_FLOAT_KW:    f = 1; break;
      case TT_DOUBLE_KW:   d = 1; break;
      case TT_ENUM_KW:     en = 1; break;
      case TT_SIMD_KW:     si = 1; break;
      case TT_STRUCT_KW:   st = 1; break;
      case TT_UNION_KW:    un = 1; break;
      case TT_TYPE_NAME:   ty = 1; tn = intern(pw->tokstr); break;
      case TT_CONST_KW:    /* ignored */ break;  
      case TT_VOLATILE_KW: /* ignored */ break;
      case TT_RESTRICT_KW: /* ignored */ break;
      default: assert(false);
    }
    dropt(pw);
  }
  if (c + s + l > 0 && i > 0) i = 0;
  if (v + c + s + i + l + f + d + en + si + st + un + ty == 0 && sg + ug != 0) i = 1;
  if (!!l + f + d > 1)
    reprintf(pw, pw->pos, "invalid type specifier: unsupported type"); 
  if (v + c + s + i + !!l + f + d + en + si + st + un + ty != 1 || sg + ug > 1)
    reprintf(pw, pw->pos, "invalid type specifier: missing or conflicting keywords"); 
  if (f + d + en + si + st + un + ty > 0 && sg + ug > 0)
    reprintf(pw, pw->pos, "invalid type specifier: unexpected signedness keyword"); 
  ndset(pn, NT_TYPE, pw->id, pos);
  if (v) pn->ts = TS_VOID;
  else if (c && !ug) pn->ts = TS_CHAR; 
  else if (c && ug) pn->ts = TS_UCHAR; 
  else if (s && !ug) pn->ts = TS_SHORT; 
  else if (s && ug) pn->ts = TS_USHORT; 
  else if (i && !ug) pn->ts = TS_INT; 
  else if (i && ug) pn->ts = TS_UINT; 
  else if (l > 1 && !ug) pn->ts = TS_LLONG; 
  else if (l > 1 && ug) pn->ts = TS_ULLONG; 
  else if (l && !ug) pn->ts = TS_LONG; 
  else if (l && ug) pn->ts = TS_ULONG; 
  else if (f) pn->ts = TS_FLOAT; 
  else if (d) pn->ts = TS_DOUBLE; 
  else if (en) parse_enum_body(pw, pn);
  else if (si) parse_simd_body(pw, pn);
  else if (st) parse_sru_body(pw, TS_STRUCT, pn);
  else if (un) parse_sru_body(pw, TS_UNION, pn);
  else if (ty) load_typedef_type(pw, pn, tn);
  else assert(false);
}

static void parse_primary_declarator(pws_t *pw, node_t *pn)
{
  switch (peekt(pw)) {
    case TT_IDENTIFIER: {
      ndset(pn, NT_IDENTIFIER, pw->id, peekpos(pw)); 
      pn->name = getid(pw);
    } break;
    case TT_COMMA: case TT_LBRK: case TT_RPAR: case TT_COLON: {
      /* identifier is missing: allow here, but check in the caller */
      ndset(pn, NT_NULL, pw->id, peekpos(pw)); 
    } break;
    case TT_LPAR: {
      dropt(pw);
      /* NB: empty pair of parens () could only be a parameter list! */
      if (peekt(pw) == TT_RPAR) {
        /* we've got a missing identifier followed by (); handle it here */
        ndset(pn, NT_NULL, pw->id, peekpos(pw));
        wrap_type_function(pn, NULL);
      } else {
        /* nested declarator */
        parse_unary_declarator(pw, pn);
      }
      expect(pw, TT_RPAR, ")");
    } break;
    default: {
      reprintf(pw, pw->pos, "valid declarator expected");
    }
  }
}

static void parse_postfix_declarator(pws_t *pw, node_t *pn)
{
  parse_primary_declarator(pw, pn);
  while (postfix_operator_ahead(pw)) {
    switch (pw->ctk) {
      case TT_LBRK: {
        node_t nd = mknd();
        dropt(pw);
        if (peekt(pw) != TT_RBRK) parse_expr(pw, &nd); 
        else /* NT_NULL */;
        expect(pw, TT_RBRK, "]");
        wrap_type_array(pn, &nd);
        ndfini(&nd);
      } break;      
      case TT_LPAR: {
        ndbuf_t ndb; ndbinit(&ndb); 
        dropt(pw);
        while (peekt(pw) != TT_RPAR) {
          node_t *pdn = ndset(ndbnewbk(&ndb), NT_VARDECL, pw->id, peekpos(pw));
          node_t *pti = ndset(ndnewbk(pdn), NT_TYPE, pw->id, peekpos(pw)); 
          if (peekt(pw) == TT_ELLIPSIS) { /* starting with ellipsis is ok */
            pti->ts = TS_ETC;
            dropt(pw);
            break;
          } else {
            parse_base_type(pw, pti);
            pdn->name = parse_declarator(pw, pti); /* 0 if id is missing */
            if (peekt(pw) != TT_COMMA) break;
            dropt(pw);
          }
        }
        expect(pw, TT_RPAR, ")");
        wrap_type_function(pn, &ndb);
        ndbfini(&ndb); 
      } break;
      default:
        reprintf(pw, pw->pos, "valid declarator postfix expected");    
    }
  }
}

static void parse_unary_declarator(pws_t *pw, node_t *pn)
{
  { retry: switch (peekt(pw)) {
      case TT_CONST_KW: case TT_VOLATILE_KW: {
        dropt(pw); /* allowed but ignored */
        goto retry;
      } break;
      case TT_STAR: {
        dropt(pw);
        parse_unary_declarator(pw, pn);
        wrap_type_pointer(pn);
      } break;
      default: {
        parse_postfix_declarator(pw, pn);
      } break;
    }
  }
}

static sym_t invert_declarator(node_t *prn, node_t *ptn)
{
  sym_t id = 0;
  switch (prn->nt) {
    case NT_NULL: {
      /* no id */;
    } break;
    case NT_IDENTIFIER: {
      id = prn->name;
    } break;
    case NT_TYPE: {
      switch (prn->ts) {
        case TS_PTR: {
          assert(ndlen(prn) == 1);
          wrap_type_pointer(ptn);
          id = invert_declarator(ndref(prn, 0), ptn);
        } break;
        case TS_ARRAY: {
          assert(ndlen(prn) == 2);
          wrap_type_array(ptn, ndref(prn, 1));
          id = invert_declarator(ndref(prn, 0), ptn);
        } break;
        case TS_FUNCTION: {
          node_t nd = mknd();
          assert(ndlen(prn) >= 1);
          ndswap(&nd, ndref(prn, 0));
          ndbrem(&prn->body, 0);
          wrap_type_function(ptn, &prn->body);
          id = invert_declarator(&nd, ptn);
          ndfini(&nd);
        } break;
        default: assert(false);
      }
    } break;
    default: assert(false);
  }
  return id;
}

static sym_t parse_declarator(pws_t *pw, node_t *ptn)
{
  sym_t id; node_t nd = mknd();
  parse_unary_declarator(pw, &nd);
  /* fixme: invert nd using ptn as a 'seed' type */
  id = invert_declarator(&nd, ptn);
  ndfini(&nd); 
  return id;
}

static void parse_initializer(pws_t *pw, node_t *pn)
{
  if (peekt(pw) == TT_LBRC) {
    ndset(pn, NT_DISPLAY, pw->id, peekpos(pw));
    ndinsbk(pn, NT_TYPE); /* placeholder TS_VOID type */
    dropt(pw);
    while (peekt(pw) != TT_RBRC) {
      parse_initializer(pw, ndnewbk(pn));
      if (peekt(pw) == TT_COMMA) dropt(pw); 
    }
    expect(pw, TT_RBRC, "}");
  } else {
    parse_assignment_expr(pw, pn);
  }
}

static void parse_init_declarator(pws_t *pw, sc_t sc, const node_t *ptn, ndbuf_t *pnb)
{
  /* declarator (= expr/{display})? */
  node_t *pn = ndbnewbk(pnb), *pni, *psn, tn; sym_t id;
  ndicpy(&tn, ptn);
  ndset(pn, NT_VARDECL, pw->id, pw->pos);
  pn->name = id = parse_declarator(pw, &tn);
  if (id == 0) reprintf(pw, pw->pos, "declared identifier is missing");
  pn->name = id; pn->sc = sc;
  ndcpy(ndnewbk(pn), &tn); 
  if (peekt(pw) != TT_ASN) {
    ndfini(&tn);
    return;
  }
  dropt(pw);
  pn = ndbnewbk(pnb);
  ndset(pn, NT_ASSIGN, pw->id, pw->pos);
  pn->op = TT_ASN;
  pni = ndnewbk(pn);
  ndset(pni, NT_IDENTIFIER, pw->id, pw->pos); 
  pni->name = id;
  psn = ndnewbk(pn);
  parse_initializer(pw, psn);
  /* patch display's array size if needed */
  patch_display_array_sizes(psn, &tn);
  /* patch declarator's array size in decl we just added above */
  pn = ndbref(pnb, ndblen(pnb)-2); assert(pn->nt == NT_VARDECL && ndlen(pn) == 1);
  ptn = ndcref(pn, 0); assert(ptn->nt == NT_TYPE);
  patch_type_array_sizes(psn, (node_t*)ptn);
  /* wrap string literals intitializing arrays in casts */
  if (psn->nt == NT_LITERAL && ptn->ts == TS_ARRAY) {
    wrap_expr_cast(psn, ndcpy(&tn, ptn)); 
  }
  ndfini(&tn);
}

static sc_t parse_decl(pws_t *pw, ndbuf_t *pnb)
{
  sc_t sc = SC_NONE;
  while (storage_class_specifier_ahead(pw)) {
    switch (peekt(pw)) {
      case TT_EXTERN_KW:   sc = SC_EXTERN; break;
      case TT_STATIC_KW:   sc = SC_STATIC; break;
      case TT_AUTO_KW:     sc = SC_AUTO; break;
      case TT_REGISTER_KW: sc = SC_REGISTER; break;
      default: assert(false);
    }
    dropt(pw);
  }
  if (type_specifier_ahead(pw)) {
    node_t tnd = mknd();
    parse_base_type(pw, &tnd);
    while (peekt(pw) != TT_SEMICOLON && peekt(pw) != TT_LBRC) {
      parse_init_declarator(pw, sc, &tnd, pnb);
      if (peekt(pw) != TT_COMMA) break;
      else dropt(pw);
    }
    ndfini(&tnd);
  } else {
    reprintf(pw, pw->pos, "type specifier expected"); 
  }
  return sc;
}

static void parse_switch_clause(pws_t *pw, node_t *pn)
{
  tt_t ctk = peekt(pw);
  if (ctk == TT_CASE_KW || ctk == TT_DEFAULT_KW) {
    ndset(pn, ctk == TT_CASE_KW ? NT_CASE : NT_DEFAULT, pw->id, pw->pos);
    dropt(pw);
    if (ctk == TT_CASE_KW) parse_expr(pw, ndnewbk(pn));
    expect(pw, TT_COLON, ":");
    while ((ctk = peekt(pw)) != TT_RBRC && ctk != TT_CASE_KW && ctk != TT_DEFAULT_KW)
      parse_stmt(pw, ndnewbk(pn));
  } else {
    reprintf(pw, pw->pos, "case or default clause expected");
  }
}

static void parse_stmt(pws_t *pw, node_t *pn)
{
  switch (peekt(pw)) {
    case TT_SEMICOLON: {
      ndset(pn, NT_NULL, pw->id, pw->pos);
      dropt(pw);
    } break;
    case TT_LBRC: {
      ndset(pn, NT_BLOCK, pw->id, pw->pos);
      dropt(pw);
      while (peekt(pw) != TT_RBRC) {
        if (peekt(pw) == TT_TYPEDEF_KW) {
          reprintf(pw, pw->pos, "block-level typedef declarations are not supported");  
        } else if (peekt(pw) == TT_IDENTIFIER && peekc(pw) == ':') {
          sym_t l; int startpos = peekpos(pw);
          l = getid(pw); expect(pw, TT_COLON, ":");
          if (peekt(pw) == TT_SEMICOLON) dropt(pw); /* optional, as in C23 */
          pn->name = l; 
          if (ndlen(pn) == 0 && peekt(pw) != TT_RBRC) pn->op = TT_CONTINUE_KW;
          else if (ndlen(pn) > 0 && peekt(pw) == TT_RBRC) pn->op = TT_BREAK_KW; 
          else reprintf(pw, startpos, "unexpected block label position");
        } else if (storage_class_specifier_ahead(pw) || type_specifier_ahead(pw)) {
          sc_t sc = parse_decl(pw, &pn->body);
          if (sc == SC_EXTERN || sc == SC_STATIC)
            reprintf(pw, pw->pos, "block-level extern/static declarations are not supported"); 
          /* auto and register are accepted and ignored */
          expect(pw, TT_SEMICOLON, ";"); /* no block-level function definitions */
        } else {
          parse_stmt(pw, ndnewbk(pn)); 
        }
      }
      expect(pw, TT_RBRC, "}");
    } break;
    case TT_IF_KW: {
      ndset(pn, NT_IF, pw->id, pw->pos);
      dropt(pw);
      expect(pw, TT_LPAR, "(");
      parse_expr(pw, ndnewbk(pn));
      expect(pw, TT_RPAR, ")");
      parse_stmt(pw, ndnewbk(pn));
      if (peekt(pw) == TT_ELSE_KW) {
        dropt(pw);
        parse_stmt(pw, ndnewbk(pn));  
      }
    } break;
    case TT_SWITCH_KW: {
      ndset(pn, NT_SWITCH, pw->id, pw->pos);
      dropt(pw);
      expect(pw, TT_LPAR, "(");
      parse_expr(pw, ndnewbk(pn));
      expect(pw, TT_RPAR, ")");
      expect(pw, TT_LBRC, "{");
      while (peekt(pw) != TT_RBRC) parse_switch_clause(pw, ndnewbk(pn));
      expect(pw, TT_RBRC, "}");
    } break;
    case TT_WHILE_KW: {
      ndset(pn, NT_WHILE, pw->id, pw->pos);
      dropt(pw);
      expect(pw, TT_LPAR, "(");
      parse_expr(pw, ndnewbk(pn));
      expect(pw, TT_RPAR, ")");
      parse_stmt(pw, ndnewbk(pn));
    } break;
    case TT_DO_KW: {
      ndset(pn, NT_DO, pw->id, pw->pos);
      dropt(pw);
      parse_stmt(pw, ndnewbk(pn));
      expect(pw, TT_WHILE_KW, "while");
      expect(pw, TT_LPAR, "(");
      parse_expr(pw, ndnewbk(pn));
      expect(pw, TT_RPAR, ")");
      expect(pw, TT_SEMICOLON, ";");
    } break;
    case TT_FOR_KW: {
      ndset(pn, NT_FOR, pw->id, pw->pos);
      dropt(pw);
      expect(pw, TT_LPAR, "(");
      if (peekt(pw) != TT_SEMICOLON) {
        if (storage_class_specifier_ahead(pw) || type_specifier_ahead(pw)) {
          node_t *psn = ndnewbk(pn); sc_t sc;
          ndset(psn, NT_BLOCK, pw->id, pw->pos);
          if ((sc = parse_decl(pw, &psn->body)) == SC_EXTERN || sc == SC_STATIC)
            reprintf(pw, pw->pos, "block-level extern/static declarations are not supported"); 
        } else {
          parse_expr(pw, ndnewbk(pn));
        }
      } else ndset(ndnewbk(pn), NT_NULL, pw->id, pw->pos); 
      expect(pw, TT_SEMICOLON, ";");
      if (peekt(pw) != TT_SEMICOLON) parse_expr(pw, ndnewbk(pn));
      else ndset(ndnewbk(pn), NT_NULL, pw->id, pw->pos); 
      expect(pw, TT_SEMICOLON, ";");
      if (peekt(pw) != TT_RPAR) parse_expr(pw, ndnewbk(pn));
      else ndset(ndnewbk(pn), NT_NULL, pw->id, pw->pos); 
      expect(pw, TT_RPAR, ")");
      parse_stmt(pw, ndnewbk(pn));
      if (ndref(pn, 0)->nt == NT_BLOCK) {
        node_t nd; ndinit(&nd); ndswap(&nd, ndref(pn, 0));
        ndswap(pn, ndnewbk(&nd)); ndswap(pn, &nd);
        ndfini(&nd);
      }
    } break;
    case TT_BREAK_KW: {
      ndset(pn, NT_BREAK, pw->id, pw->pos);
      dropt(pw);
      expect(pw, TT_SEMICOLON, ";");
    } break;
    case TT_CONTINUE_KW: { 
      ndset(pn, NT_CONTINUE, pw->id, pw->pos);
      dropt(pw);
      expect(pw, TT_SEMICOLON, ";");
    } break;
    case TT_RETURN_KW: {
      ndset(pn, NT_RETURN, pw->id, pw->pos);
      dropt(pw);
      if (peekt(pw) != TT_SEMICOLON) parse_expr(pw, ndnewbk(pn));
      expect(pw, TT_SEMICOLON, ";");
    } break;
    case TT_GOTO_KW: {
      ndset(pn, NT_GOTO, pw->id, pw->pos);
      dropt(pw);
      pn->name = getid(pw);
      expect(pw, TT_SEMICOLON, ";");
    } break;
    default: {
      parse_expr(pw, pn);
      if (pn->nt == NT_IDENTIFIER && peekt(pw) == TT_COLON) 
        reprintf(pw, pw->pos, "arbitrary placed labels are not supported"); 
      expect(pw, TT_SEMICOLON, ";");
    } break;
  }
}

static void parse_top_decl(pws_t *pw, node_t *pn)
{
  if (storage_class_specifier_ahead(pw) || type_specifier_ahead(pw)) {
    sc_t sc;
    ndset(pn, NT_BLOCK, pw->id, pw->pos);
    pn->name = pw->curmod;
    sc = parse_decl(pw, &pn->body);
    if (sc == SC_AUTO || sc == SC_REGISTER)
      reprintf(pw, pw->pos, "top-level auto/register declarations are not supported"); 
    if (peekt(pw) == TT_LBRC) {
      /* must be single function declaration */
      if (ndlen(pn) == 1 && ndref(pn, 0)->nt == NT_VARDECL) { 
        node_t nd = mknd(), *pbn;
        ndswap(ndref(pn, 0), &nd);
        nd.nt = NT_FUNDEF;
        parse_stmt(pw, (pbn = ndnewbk(&nd)));
        if (pbn->name) neprintf(pbn, "unexpected label on top function block");   
        ndswap(pn, &nd);
        ndfini(&nd);
      } else {
        reprintf(pw, pw->pos, "malformed top-level function definition"); 
      }     
    } else {
      /* variable(s) declaration, should end in ; */
      expect(pw, TT_SEMICOLON, ";");
    }
  } else {
    reprintf(pw, pw->pos, "top-level declaration/definition expected"); 
  }
}

static void parse_top_typedef(pws_t *pw, node_t *pn)
{
  int startpos = peekpos(pw);
  expect(pw, TT_TYPEDEF_KW, "typedef");
  ndset(pn, NT_TYPEDEF, pw->id, startpos);
  if (type_specifier_ahead(pw)) {
    node_t *ptn = ndnewbk(pn);
    parse_base_type(pw, ptn);
    pn->name = parse_declarator(pw, ptn);
    expect(pw, TT_SEMICOLON, ";");
    if (!pn->name) reprintf(pw, startpos, "missing new type name in typedef");
    intern_symbol(symname(pn->name), TT_TYPE_NAME, (int)ndblen(&g_nodes));
    ndcpy(ndbnewbk(&g_nodes), ptn);    
  } else {
    reprintf(pw, pw->pos, "type specifier expected");
  }
}

static bool parse_pragma_directive(pws_t *pw, int startpos)
{
  if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "pragma")) {
    dropt(pw);
    if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "once")) {
      dropt(pw);
      if (!check_once(pw->infile)) {
        logef("# repeated #include of '%s' ignored\n", pw->infile);
        return false;
      }
    } else if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "module")) {
      dropt(pw);
      if (peekt(pw) == TT_STRING) {
        sym_t name = intern(pw->tokstr);
        dropt(pw);
        logef("# module name set to '%s'\n", symname(name));
        pw->curmod = name;
      } else reprintf(pw, peekpos(pw), "missing module name string after #pragma module");
    }
  }
  return true; 
}

static void parse_define_directive(pws_t *pw, int startpos)
{
  if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "define")) {
    int *pi, info = (int)ndblen(&g_nodes); node_t *pn = ndbnewbk(&g_nodes);
    dropt(pw);
    ndset(pn, NT_MACRODEF, pw->id, startpos);
    if (peekt(pw) == TT_MACRO_NAME)
      reprintf(pw, peekpos(pw), "macro already defined; use #undef before redefinition");
    pn->name = getid(pw);
    if ((pi = bufbsearch(&g_syminfo, &pn->name, &int_cmp)) != NULL)
      reprintf(pw, peekpos(pw), "macro can't redefine globally defined symbol");
    /* do manual char-level lookahead */
    if (peekc(pw) == '(') {
      /* macro with parameters */
      node_t *ptn = ndnewbk(pn);
      ndset(ptn, NT_TYPE, pw->id, peekpos(pw));
      ptn->ts = TS_FUNCTION;
      ndnewbk(ptn); /* return value type is not set */
      expect(pw, TT_LPAR, "(");
      while (peekt(pw) != TT_RPAR) {
        node_t *pti = ndnewbk(ptn);
        ndset(pti, NT_VARDECL, pw->id, peekpos(pw));
        if (peekt(pw) == TT_ELLIPSIS) {
          pti->name = 0;
          dropt(pw);
          break;
        } else {
          pti->name = getid(pw);
          if (peekt(pw) == TT_COMMA) dropt(pw);
          else break;
        }
      }
      expect(pw, TT_RPAR, ")");
    }  
    /* righ-hand-side expression or do-block-while-0 */    
    if (peekt(pw) == TT_DO_KW) {
      dropt(pw); parse_stmt(pw, ndnewbk(pn));
      expect(pw, TT_WHILE_KW, "while");
      expect(pw, TT_LPAR, "(");
      if (peekt(pw) == TT_INT && streql(pw->tokstr, "0")) dropt(pw);
      else reprintf(pw, pw->pos, "0 is expected");
      expect(pw, TT_RPAR, ")");
    } else { 
      parse_primary_expr(pw, ndnewbk(pn));
    }
    /* save macro definition for future use */
    if (lookup_symbol(symname(pn->name), NULL) != TT_IDENTIFIER) {
      reprintf(pw, startpos, "macro name is already in use");
    } else { 
      intern_symbol(symname(pn->name), TT_MACRO_NAME, info);
    }
    if (getverbosity() > 0) {
      dump_node(pn, stderr);
    }
  } else if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "undef")) {  
    dropt(pw);
    if (peekt(pw) != TT_MACRO_NAME) {
      reprintf(pw, peekpos(pw), "undef name is not a macro");
    } else if (streql(pw->tokstr, "__WCPL__") || streql(pw->tokstr, "__DATE__")
            || streql(pw->tokstr, "__TIME__") || streql(pw->tokstr, "NULL")
            || streql(pw->tokstr, "true")     || streql(pw->tokstr, "false")
            || streql(pw->tokstr, "__VA_ARGS__")) {
      reprintf(pw, peekpos(pw), "%s macro cannot be undefined or redefined", pw->tokstr);
    } else {
      /* definition will still remain in g_nodes, but name won't */ 
      unintern_symbol(pw->tokstr);
      dropt(pw);
    }
  }
}

static void parse_include_directive(pws_t *pw, node_t *pn, int startpos)
{
  cbuf_t cb; bool gotdot = false;
  cbinit(&cb);
  dropt(pw);
  if (peekt(pw) == TT_STRING) {
    char *sep, *s;
    const char *cs = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_/";
    if ((sep = strsuf(pw->tokstr, ".h")) != NULL) cbset(&cb, pw->tokstr, sep-pw->tokstr);
    else if ((sep = strsuf(pw->tokstr, ".wh")) != NULL) cbset(&cb, pw->tokstr, sep-pw->tokstr);
    else cbsets(&cb, pw->tokstr);
    s = cbdata(&cb);
    if (strspn(s, cs) != strlen(s))
      reprintf(pw, peekpos(pw), "unsupported module name in include directive");
    dropt(pw);
    strtrc(s, '/', '.');
    ndset(pn, NT_INCLUDE, pw->id, startpos);
    pn->name = intern(cbdata(&cb));
    pn->op = TT_STRING;
  } else {
    tt_t tt;
    expect(pw, TT_LT, "<");
    while (peekt(pw) != TT_GT) {
      if (peekt(pw) == TT_DOT) {
        dropt(pw); gotdot = true;
      } else if (!gotdot && ((tt = peekt(pw)) == TT_IDENTIFIER || tt >= TT_ASM_KW)) {
        cbputs(pw->tokstr, &cb); dropt(pw);
      } else if (!gotdot && peekt(pw) == TT_SLASH) {
        cbputc('.', &cb); dropt(pw);
      } else if (gotdot && peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "h")) {
        dropt(pw); break;
      } else if (gotdot && peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "wh")) {
        dropt(pw); break;
      } else if (gotdot) {
        reprintf(pw, peekpos(pw), "unsupported file extension in include directive");
        break;
      } else {
        reprintf(pw, peekpos(pw), "invalid include directive");
        break;
      }
    }
    expect(pw, TT_GT, ">");
    ndset(pn, NT_INCLUDE, pw->id, startpos);
    pn->name = intern(cbdata(&cb));
    pn->op = TT_IDENTIFIER;
  }
  cbfini(&cb);
}


/* the only cc test macro is __WCPL__ */
static void parse_cc_test_macro(pws_t *pw)
{
  if (ahead(pw, "__WCPL__")) dropt(pw);
  else reprintf(pw, peekpos(pw), "__WCPL__ expected in condition"); 
}

/* returns true if cc condition is true, false otherwise; keeps \n token */
static bool parse_cc_cond(pws_t *pw)
{
  { bool res = true, gotlf = false;
    if (ahead(pw, "0")) { 
      dropt(pw); res = false; 
    } else if (ahead(pw, "1")) {
      dropt(pw);
    } else {
      if (ahead(pw, "!")) { dropt(pw); res = !res; }
      if (ahead(pw, "defined")) dropt(pw); else goto err;
      expect(pw, TT_LPAR, "(");
      parse_cc_test_macro(pw);
      expect(pw, TT_RPAR, ")");
    }
    while (!gotlf && peekt_ws(pw) == TT_WHITESPACE) {
      if (pw->tokstr[0] == '\n') gotlf = true;
      else dropt(pw); /* don't eat \n token! */
    }
    if (gotlf) return res;
    err:;
  }
  reprintf(pw, peekpos(pw), "invalid or unsupported #if condition"); 
  return false;
}

/* returns true if cc 'if' test is true, false otherwise */
static bool parse_cc_if_directive(pws_t *pw)
{
  bool cnd;
  if (ahead(pw, "ifdef") || ahead(pw, "ifndef")) {
    cnd = ahead(pw, "ifdef"); 
    dropt(pw); parse_cc_test_macro(pw);
  } else {
    expect(pw, TT_IF_KW, "if");
    cnd = parse_cc_cond(pw);
    dropt(pw); /* eat \n token! */
  }
  return cnd;
}

/* returns false if stopped on #endif, true otherwise (all consumed) */
static bool parse_cc_inactive(pws_t *pw, bool gotactive) 
{
  bool bol = false; int level = 0;
  while (true) {
    switch (peekt_ws(pw)) { /* raw peek */
      case TT_EOF:
        if (pw->inateof) reprintf(pw, pw->pos, "missing #endif"); /* real eof */
        else if (pw->curi < (int)cblen(&pw->chars)) pw->curi += 1; /* try to step over it */
        else reprintf(pw, pw->pos, "can't skip token inside inactive cc block");
        bol = false; dropt(pw); continue;
      case TT_WHITESPACE:
        bol = (pw->tokstr[0] == '\n'); dropt(pw); continue;
      case TT_HASH:
        if (bol) {
          int startpos = pw->pos;
          dropt(pw);
          if (ahead(pw, "if") || ahead(pw, "ifdef") || ahead(pw, "ifndef")) {
            ++level; 
          } else if (ahead(pw, "elif")) {
            if (!level && !gotactive) { dropt(pw); if (parse_cc_cond(pw)) { dropt(pw); return true; } }
          } else if (ahead(pw, "else")) {
            if (!level && !gotactive) { dropt(pw); return true; }
          } else if (ahead(pw, "endif")) {
            if (!level) { dropt(pw); return false; } else --level;
          }
        } 
        /* fall thru */
      default: 
        bol = false; dropt(pw); continue;
    }
    break;
  }
  return false;
}

/* parse single top-level declaration/definition/directive */
bool parse_top_form(pws_t *pw, node_t *pn)
{
  ndclear(pn);
  if (peekt(pw) != TT_EOF) {
    if (peekt(pw) == TT_HASH) {
      int startpos = peekpos(pw);
      dropt(pw);
      if (ahead(pw, "pragma")) {
        bool cont = parse_pragma_directive(pw, startpos);
        if (!cont) return false; /* bail out on '#pragma once' repeat */
        ndset(pn, NT_BLOCK, pw->id, startpos);
      } else if (ahead(pw, "define")) {
        parse_define_directive(pw, startpos);
        ndset(pn, NT_BLOCK, pw->id, startpos);
      } else if (ahead(pw, "undef")) {
        parse_define_directive(pw, startpos);
        ndset(pn, NT_BLOCK, pw->id, startpos);
      } else if (ahead(pw, "include")) {
        parse_include_directive(pw, pn, startpos); 
      } else if (ahead(pw, "ifdef") || ahead(pw, "ifndef") || ahead(pw, "if")) {
        if (parse_cc_if_directive(pw) || parse_cc_inactive(pw, false)) pw->cclevel++;
      } else if (ahead(pw, "elif") || ahead(pw, "else")) { 
        if (!pw->cclevel) reprintf(pw, startpos, "unexpected #%s", pw->tokstr);
        dropt(pw); if (!parse_cc_inactive(pw, true)) pw->cclevel--; 
      } else if (ahead(pw, "endif")) {
        dropt(pw); if (!pw->cclevel--) reprintf(pw, startpos, "unexpected #endif");
      } else {
        reprintf(pw, startpos, "invalid or unsupported preprocessor directive");
      }
    } else if (peekt(pw) == TT_TYPEDEF_KW) {
      parse_top_typedef(pw, pn);
    } else if (peekt(pw) == TT_INTR_NAME) {
      parse_primary_expr(pw, pn);
      expect(pw, TT_SEMICOLON, ";");
    } else {
      parse_top_decl(pw, pn); 
    } 
    return true; 
  }
  return false;
}

/* parse and collect top-level declarations/definitions */
void parse_translation_unit(pws_t *pw, ndbuf_t *pnb)
{
  while (true) {
    node_t *pn = ndbnewbk(pnb);
    bool ok = parse_top_form(pw, pn);
    if (!ok || pn->nt == NT_NULL) ndbrem(pnb, ndblen(pnb)-1);
    if (!ok) return;
  } 
}

/* report node error, possibly printing location information, and exit */
void neprintf(const node_t *pn, const char *fmt, ...)
{
  pws_t *pw = NULL; int startpos = -1;
  va_list args;
  va_start(args, fmt); 
  if (pn && pn->pwsid >= 0 && pn->pwsid < (int)buflen(&g_pwsbuf)) {
    pw = *(pws_t**)bufref(&g_pwsbuf, (size_t)pn->pwsid);
    startpos = pn->startpos;
  }
  vrprintf(pw, startpos, fmt, args); 
  va_end(args); 
  exit(1);
}

void n2eprintf(const node_t *pn, const node_t *pn2, const char *fmt, ...)
{
  pws_t *pw = NULL; int startpos = -1;
  va_list args;
  va_start(args, fmt); 
  if (pn && pn->pwsid >= 0 && pn->pwsid < (int)buflen(&g_pwsbuf)) {
    pw = *(pws_t**)bufref(&g_pwsbuf, (size_t)pn->pwsid);
    startpos = pn->startpos;
  }
  vrprintf(pw, startpos, fmt, args); 
  va_end(args);
  if (pn2) neprintf(pn2, "(original location)");
  else exit(1);
}

/* report node warning, possibly printing location information */
void nwprintf(const node_t *pn, const char *fmt, ...)
{
  if (getwlevel() < 1) {
    pws_t *pw = NULL; int startpos = -1;
    va_list args;
    va_start(args, fmt); 
    if (pn && pn->pwsid >= 0 && pn->pwsid < (int)buflen(&g_pwsbuf)) {
      pw = *(pws_t**)bufref(&g_pwsbuf, (size_t)pn->pwsid);
      startpos = pn->startpos;
    }
    vrprintf(pw, startpos, fmt, args); 
    va_end(args); 
  }
}


/* dump nodes in s-expression format */

static void fdumpss(const char *str, size_t n, FILE* fp)
{
  cbuf_t cb = mkcb();
  cbputc('\"', &cb);
  while (n > 0) {
    int c = *str & 0xFF;
    /* check for control chars manually */
    if (c <= 0x1F || c >= 0x7F) {
      switch (c) {
        case '\a' : cbputs("\\a", &cb); break;
        case '\b' : cbputs("\\b", &cb); break;
        case '\t' : cbputs("\\t", &cb); break;
        case '\n' : cbputs("\\n", &cb); break;
        case '\r' : cbputs("\\r", &cb); break;
        default   : {
          char buf[10]; sprintf(buf, "\\%.2X", c); /* WAT-style; R7RS: "\\x%.2X;" */
          cbputs(buf, &cb);
        } break;
      }
      ++str; --n;
      continue;
    } else if (c == '\"' || c == '\\') {
      switch (c) {
        case '\"' : cbputs("\\\"", &cb); break;
        case '\\' : cbputs("\\\\", &cb); break;
      }
      ++str; --n;
      continue;
    } else { 
      /* put ascii chars as-is */
      cbputc(c, &cb);
      ++str; --n;
    }
  }
  cbputc('\"', &cb);
  fputs(cbdata(&cb), fp);
  cbfini(&cb);
}

static void fdumpv32(const char *str, size_t n, FILE* fp)
{
  cbuf_t cb = mkcb(); size_t i;
  cbputs("#(", &cb); /* #u32? */
  for (i = 0; i+4 <= n; i += 4) {
    /* stored in le order */
    unsigned u = str[i] | (str[i+1] << 8) | (str[i+2] << 16) | (str[i+3] << 24);
    if (i) cbputc(' ', &cb);
    cbputu(u, &cb);
  }
  cbputc(')', &cb);
  fputs(cbdata(&cb), fp);
  cbfini(&cb);
}

const char *ts_name(ts_t ts)
{
  const char *s = "?";
  switch (ts) {
    case TS_VOID: s = "void"; break;
    case TS_ETC: s = "etc"; break;  
    case TS_V128: s = "v128"; break; 
    case TS_BOOL: s = "bool"; break; 
    case TS_CHAR: s = "char"; break; 
    case TS_UCHAR: s = "uchar"; break; 
    case TS_SHORT: s = "short"; break; 
    case TS_USHORT: s = "ushort"; break; 
    case TS_INT: s = "int"; break; 
    case TS_UINT: s = "uint"; break; 
    case TS_LONG: s = "long"; break; 
    case TS_ULONG: s = "ulong"; break; 
    case TS_LLONG: s = "llong"; break; 
    case TS_ULLONG: s = "ullong"; break; 
    case TS_FLOAT: s = "float"; break; 
    case TS_DOUBLE: s = "double"; break; 
    case TS_STRING: s = "string"; break; 
    case TS_LSTRING: s = "lstring"; break; 
    case TS_ENUM: s = "enum"; break; 
    case TS_ARRAY: s = "array"; break; 
    case TS_STRUCT: s = "struct"; break; 
    case TS_UNION: s = "union"; break; 
    case TS_FUNCTION: s = "function"; break; 
    case TS_PTR: s = "ptr"; break; 
    default: assert(false);
  }
  return s;
}

const char *sc_name(sc_t sc)
{
  const char *s = "?";
  switch (sc) {
    case SC_NONE: s = ""; break; 
    case SC_EXTERN: s = "extern"; break; 
    case SC_STATIC: s = "static"; break; 
    case SC_AUTO: s = "auto"; break; 
    case SC_REGISTER: s = "register"; break; 
    default: assert(false);
  }
  return s;
}

const char *intr_name(intr_t intr)
{
  const char *s = "?";
  switch (intr) {
    case INTR_NONE: s = ""; break; 
    case INTR_ALLOCA: s = "alloca"; break; 
    case INTR_ACODE: s = "acode"; break; 
    case INTR_SIZEOF: s = "sizeof"; break; 
    case INTR_ALIGNOF: s = "alignof"; break; 
    case INTR_OFFSETOF: s = "offsetof"; break;
    case INTR_COUNTOF: s = "countof"; break; 
    case INTR_GENERIC: s = "generic"; break; 
    case INTR_VAETC: s = "va_etc"; break; 
    case INTR_VAARG: s = "va_arg"; break; 
    case INTR_SASSERT: s = "static_assert"; break; 
    case INTR_DEFINED: s = "defined"; break; 
    default: assert(false);
  }
  return s;
}

const char *op_name(tt_t op)
{
  const char *s = "?";
  switch (op) {
    case TT_PLUS: s = "+"; break; 
    case TT_MINUS: s = "-"; break; 
    case TT_AND_AND: s = "&&"; break; 
    case TT_OR_OR: s = "||"; break; 
    case TT_NOT: s = "!"; break; 
    case TT_AND: s = "&"; break; 
    case TT_AND_ASN: s = "&="; break; 
    case TT_OR: s = "|"; break; 
    case TT_OR_ASN: s = "|="; break; 
    case TT_XOR: s = "^"; break; 
    case TT_XOR_ASN: s = "^="; break; 
    case TT_REM: s = "%"; break; 
    case TT_REM_ASN: s = "%="; break; 
    case TT_SLASH: s = "/"; break; 
    case TT_SLASH_ASN: s = "/="; break; 
    case TT_STAR: s = "*"; break; 
    case TT_STAR_ASN: s = "*="; break; 
    case TT_PLUS_PLUS: s = "++"; break; 
    case TT_PLUS_ASN: s = "+="; break; 
    case TT_MINUS_MINUS: s = "--"; break; 
    case TT_MINUS_ASN: s = "-="; break; 
    case TT_SHL: s = "<<"; break; 
    case TT_SHL_ASN: s = "<<="; break; 
    case TT_SHR: s = ">>"; break; 
    case TT_SHR_ASN: s = ">>="; break; 
    case TT_LT: s = "<"; break; 
    case TT_LE: s = "<="; break; 
    case TT_GT: s = ">"; break; 
    case TT_GE: s = ">="; break; 
    case TT_EQ: s = "=="; break; 
    case TT_NE: s = "!="; break; 
    case TT_DOT: s = "."; break; 
    case TT_ARROW: s = "->"; break; 
    case TT_ASN: s = "="; break; 
    case TT_TILDE: s = "~"; break;
    case TT_BREAK_KW: s = "break"; break;  
    case TT_CONTINUE_KW: s = "continue"; break;  
    default:;
  }
  return s;
}

static void dump(node_t *pn, FILE* fp, int indent)
{
  size_t i;
  if (pn->pwsid >= 0 && pn->startpos >= 0) {
    fprintf(fp, "#|%10d%c|# ", pn->startpos, 'a'+pn->pwsid);
  } else {
    fprintf(fp, "#|           |# ");
  }
  if (indent) fprintf(fp, "%*c", indent, ' ');
  switch (pn->nt) {
    case NT_NULL: {
      fprintf(fp, "(null");
    } break;
    case NT_LITERAL: {
      cbuf_t cb; cbinit(&cb);
      fprintf(fp, "(literal %s ", ts_name(pn->ts));
      switch (pn->ts) {
        case TS_STRING:
          fdumpss(cbdata(&pn->data), cblen(&pn->data), fp);
          break;
        case TS_LSTRING:
          fdumpv32(cbdata(&pn->data), cblen(&pn->data), fp);
          break;
        case TS_FLOAT: 
          fprintf(fp, "%.9g", (double)pn->val.f);
          break;
        case TS_DOUBLE:
          fprintf(fp, "%.17g", pn->val.d);
          break;
        case TS_CHAR: case TS_SHORT: case TS_INT: case TS_LONG: case TS_LLONG:
          cbputll(pn->val.i, &cb);
          fputs(cbdata(&cb), fp);
          break;          
        case TS_BOOL: case TS_UCHAR: case TS_USHORT: case TS_UINT: case TS_ULONG: case TS_ULLONG:
          cbputllu(pn->val.u, &cb);
          fputs(cbdata(&cb), fp);
          break;
        case TS_V128:
          if (ndlen(pn) == 2) fprintf(fp, "%s %d ", ts_name(ndref(pn, 0)->ts), (int)ndref(pn, 0)->val.i);
          fdumpss(cbdata(&pn->data), cblen(&pn->data), fp);
          break;
        default: assert(false); 
      }    
      cbfini(&cb);
    } break;
    case NT_IDENTIFIER: {
      fputs(symname(pn->name), fp);
      if (!indent) fputc('\n', fp);
      return;
    } break;
    case NT_SUBSCRIPT: {
      fprintf(fp, "(subscript");
    } break;
    case NT_CALL: {
      fprintf(fp, "(call");
    } break;
    case NT_INTRCALL: {
      if (pn->intr == INTR_VAETC) {
        fputs("...", fp);
        if (!indent) fputc('\n', fp);
        return;
      } else {
        fprintf(fp, "(intrcall %s", intr_name(pn->intr));
      }
    } break;
    case NT_CAST: {
      fprintf(fp, "(cast");
    } break;
    case NT_POSTFIX: {
      fprintf(fp, "(postfix \"%s\"", op_name(pn->op));
      if (pn->op == TT_DOT || pn->op == TT_ARROW)
        fprintf(fp, " %s", symname(pn->name));
    } break;
    case NT_PREFIX: {
      fprintf(fp, "(prefix \"%s\"", op_name(pn->op));
    } break;
    case NT_INFIX: {
      fprintf(fp, "(infix \"%s\"", op_name(pn->op));
    } break;
    case NT_COND: {
      fprintf(fp, "(cond");
    } break;
    case NT_ASSIGN: {
      fprintf(fp, "(assign \"%s\"", op_name(pn->op));
    } break;
    case NT_COMMA: {
      fprintf(fp, "(comma");
    } break;
    case NT_ACODE: {
      size_t i, ri = 1; cbuf_t cb = mkcb();
      node_t *ptn = ndref(pn, 0);
      if (ptn->ts == TS_PTR && ndlen(ndref(ptn, 0)) == 0) {
        ptn = ndref(ptn, 0);
        if (!ptn->name) fprintf(fp, "(acode (type ptr (type %s))", ts_name(ptn->ts));
        else fprintf(fp, "(acode (type ptr (type %s %s))", ts_name(ptn->ts), symname(ptn->name));
      } else if (ndlen(ptn) == 0) {
        if (!ptn->name) fprintf(fp, "(acode (type %s)", ts_name(ptn->ts));
        else fprintf(fp, "(acode (type %s %s)", ts_name(ptn->ts), symname(ptn->name));
      } else {
        fprintf(fp, "(acode\n");
        dump(ptn, fp, indent+2);
      }
      assert(pn->data.esz == sizeof(inscode_t));
      for (i = 0; i < buflen(&pn->data); ++i) {
        inscode_t *pic = bufref(&pn->data, i);
        if (pic->in == IN_PLACEHOLDER) {
          assert(ri < ndlen(pn));
          fputc('\n', fp);
          dump(ndref(pn, ri), fp, indent+2);
          ++ri;
        } else {
          const char *s = format_inscode(pic, &cb);
          fprintf(fp, "\n#|           |# %*c(%s)", indent+2, ' ', s);
        }
      }  
      fputs(")", fp);
      cbfini(&cb);
      if (!indent) fputc('\n', fp);
      return;
    }
    case NT_BLOCK: {
      if (!pn->name) fprintf(fp, "(block");
      else if (pn->op == 0) fprintf(fp, "(block %s", symname(pn->name)); /* top-level special */
      else fprintf(fp, "(block %s %s", op_name(pn->op), symname(pn->name)); /* labeled */
    } break;
    case NT_IF: {
      fprintf(fp, "(if");
    } break;
    case NT_SWITCH: {
      fprintf(fp, "(switch");
    } break;
    case NT_CASE: {
      fprintf(fp, "(case");
    } break;
    case NT_DEFAULT: {
      fprintf(fp, "(default");
    } break;
    case NT_WHILE: {
      fprintf(fp, "(while");
    } break;
    case NT_DO: {
      fprintf(fp, "(do");
    } break;
    case NT_FOR: {
      fprintf(fp, "(for");
    } break;
    case NT_GOTO: {
      fprintf(fp, "(goto %s", symname(pn->name));
    } break;
    case NT_RETURN: {
      if (!pn->name) fprintf(fp, "(return");
      else fprintf(fp, "(return %s", symname(pn->name)); /* w/stack restore */
    } break;
    case NT_BREAK: {
      fprintf(fp, "(break");
    } break;
    case NT_CONTINUE: {
      fprintf(fp, "(continue");
    } break;
    case NT_TYPE: {
      if (!pn->name) fprintf(fp, "(type %s", ts_name(pn->ts));
      else fprintf(fp, "(type %s %s", ts_name(pn->ts), symname(pn->name));
    } break;
    case NT_VARDECL: {
      const char *s = sc_name(pn->sc);
      if (!pn->name) fprintf(fp, "(vardecl %s", s);
      else fprintf(fp, "(vardecl %s %s", symname(pn->name), s);
    } break;
    case NT_DISPLAY: {
      if (pn->sc == SC_STATIC) fprintf(fp, "(display static");
      else fprintf(fp, "(display");
    } break;
    case NT_FUNDEF: {
      const char *s = sc_name(pn->sc);
      fprintf(fp, "(fundef %s %s", symname(pn->name), s);
    } break;
    case NT_TYPEDEF: {
      fprintf(fp, "(typedef %s", symname(pn->name));
    } break;
    case NT_MACRODEF: {
      fprintf(fp, "(macrodef %s", symname(pn->name));
      if (ndlen(pn) == 2 && ndref(pn, 0)->nt == NT_TYPE) {
        node_t *pfn = ndref(pn, 0), *pvn = ndref(pn, 1);
        assert(pfn->ts == TS_FUNCTION && ndlen(pfn) >= 1);
        fputs(" (", fp);
        for (i = 1; i < ndlen(pfn); ++i) {
          node_t *pin = ndref(pfn, i);
          assert(pin->nt == NT_VARDECL);
          if (i > 1) fputc(' ', fp);
          fputs(pin->name ? symname(pin->name) : "...", fp);
        }
        fputs(")\n", fp);
        dump(pvn, fp, indent+2);
        fputc(')', fp);
        if (!indent) fputc('\n', fp);
        return;
      }
    } break;
    case NT_INCLUDE: {
      char *it = (pn->op == TT_STRING) ? "user" : "system";
      fprintf(fp, "(include %s %s", it, symname(pn->name));
    } break;
    case NT_IMPORT: {
      bool final = (pn->sc == SC_REGISTER || pn->sc == SC_AUTO);  
      bool referenced = (pn->sc == SC_EXTERN); /* nonfinals only */
      bool hide = (pn->sc == SC_STATIC || pn->sc == SC_AUTO);
      const char *fs = final ? " final" : "";
      const char *rs = referenced ? " referenced" : "";
      const char *hs = hide ? " hide" : "";
      assert(pn->name);
      fprintf(fp, "(import %s %s%s%s", symname(pn->name), fs, rs, hs);
    } break;
    default: assert(false);
  }
  if (ndlen(pn) > 0) {
    for (i = 0; i < ndlen(pn); i += 1) {
      fputc('\n', fp);
      dump(ndref(pn, i), fp, indent+2);
    }
    fputc(')', fp);
  } else {
    fputc(')', fp);
  }
  if (!indent) fputc('\n', fp);
}


/* dump node in s-expression format */
void dump_node(const node_t *pn, FILE *fp)
{
  dump((node_t*)pn, fp, 0);
}
