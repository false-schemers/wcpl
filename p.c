/* p.c (wcpl parser) -- esl */

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
#include "l.h"
#include "p.h"
#include "c.h"


/* module names and files */

char *path_filename(const char *path)
{
  char *s1, *s2, *s3, *s = (char*)path;
  s1 = strrchr(path, '\\'), s2 = strrchr(path, '/'), s3 = strrchr(path, ':');
  if (s1 && s < s1+1) s = s1+1; 
  if (s2 && s < s2+1) s = s2+1; 
  if (s3 && s < s3+1) s = s3+1;
  return s;
}

sym_t base_from_path(const char *path)
{
  char *f = path_filename(path), *z = (char*)path + strlen(path); 
  sym_t bp = 0;
  if (f < z) {
    chbuf_t cb; chbinit(&cb); 
    bp = intern(chbset(&cb, path, f-path));
    chbfini(&cb);
  }
  return bp;
}

sym_t modname_from_path(const char *path)
{
  char *e, *s = path_filename(path), *z = (char*)path + strlen(path); 
  sym_t mod = 0;
  if (!(e = strrchr(path, '.')) || e < s) e = z;
  assert(s <= e);
  if (s < e) {
    chbuf_t cb; chbinit(&cb); 
    mod = intern(chbset(&cb, s, e-s));
    chbfini(&cb);
  }
  return mod;
}

pws_t *pws_from_modname(sym_t mod, buf_t *pbases)
{
  pws_t *pws = NULL;
  if (mod) {
    size_t i; chbuf_t cb; chbinit(&cb);
    for (i = 0; i < buflen(pbases); ++i) {
      sym_t *pb = bufref(pbases, i);
      pws = newpws(chbsetf(&cb, "%s%s", symname(*pb), symname(mod)));
      if (pws) break;
      pws = newpws(chbsetf(&cb, "%s%s.h", symname(*pb), symname(mod)));
      if (pws) break;
      pws = newpws(chbsetf(&cb, "%s%s.wh", symname(*pb), symname(mod)));
      if (pws) break;
    }
    if (pws) fprintf(stderr, "# found module %s in %s (pws #%d)\n", symname(mod), chbdata(&cb), pws->id);
    chbfini(&cb);
  }
  return pws;
}


/* parser workspaces */

static buf_t g_pwsbuf;

/* alloc parser workspace for infile */
pws_t *newpws(const char *infile)
{
  FILE *fp;
  fp = streql(infile, "-") ? stdin : fopen(infile, "r");
  if (fp) {
    pws_t *pw = exmalloc(sizeof(pws_t));
    pw->id = (int)buflen(&g_pwsbuf);
    pw->infile = exstrdup(infile);
    pw->curmod = streql(infile, "-") ? intern("_") : modname_from_path(infile);
    pw->input = fp;
    pw->inateof = false;
    chbinit(&pw->inchb);
    bufinit(&pw->lsposs, sizeof(size_t));
    pw->discarded = 0;
    bufinit(&pw->chars, sizeof(char));
    fget8bom(pw->input);
    pw->curi = 0; 
    pw->gottk = false;
    pw->ctk = TT_EOF;
    chbinit(&pw->token);
    pw->tokstr = NULL;
    pw->pos = 0;
    *(pws_t**)bufnewbk(&g_pwsbuf) = pw;
    return pw;
  }
  return NULL;
}

/* close existing workspace, leaving only data used for error reporting */
void closepws(pws_t *pw)
{
  if (pw) {
    if (pw->input && pw->input != stdin) {
      fclose(pw->input); pw->input = NULL;
    }
    chbclear(&pw->inchb);
    chbclear(&pw->token);
  }
}


static void freepws(pws_t *pw)
{
  if (pw) {
    free(pw->infile);
    if (pw->input && pw->input != stdin) fclose(pw->input);
    chbfini(&pw->inchb);
    buffini(&pw->lsposs);
    chbfini(&pw->chars);
    chbfini(&pw->token);
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
    freepws(*(pws_t**)bufpopbk(&g_pwsbuf));
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
static char *recallline(pws_t *pw, int lno, chbuf_t *pcb)
{
  size_t gci, ppi; chbuf_t *pp = &pw->chars; 
  char *pchars = chbdata(pp);
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
  chbclear(pcb);
  for (ppi = gci; ppi <= chblen(pp); ++ppi) {
    int c = pchars[ppi];
    if (c == '\n') break;
    chbputc(c, pcb);
  }
  return chbdata(pcb);
}

/* try to get new line of input so parsing can continue */
static int fetchline(pws_t *pw, char **ptbase, int *pendi)
{
  chbuf_t *pp = &pw->chars; int c = EOF;
  if (!pw->inateof) {
    char *line = fgetlb(&pw->inchb, pw->input);
    if (!line) {
      pw->inateof = true;
    } else {
      size_t pos = buflen(pp) + pw->discarded;
      unsigned long ch = 0; char *ln = line; 
      while (*ln) {
        ch = unutf8((unsigned char **)&ln);
        if (ch == 0 || ch == -1) {
          int lno = (int)buflen(&pw->lsposs)+1;
          exprintf("%s:%d: utf-8 encoding error", pw->infile, lno);
        }
      }
      chbputs(line, pp); 
      if (ch == '\\') pp->fill -= 1; /* c preprocessor-like behavior */
      else chbputc('\n', pp); /* normal line not ending in backslash */
      if (c == EOF) c = '\n';
      *(size_t*)bufnewbk(&pw->lsposs) = pos;
      *ptbase = chbdata(pp);
      *pendi = (int)chblen(pp);
    }
  }
  return c;  
}

/* readjust input after previous read; return true unless at EOF */
static bool clearinput(pws_t *pw)
{
  /* readjust and repeat unless at eof */
  if (!pw->inateof) {
    /* continuing after successful parse, its associated info is no longer needed */
    /* throw away unichars before curi -- but not after the beginning of the line where curi
     * is located as we don't want to lose the ability to show current line as error line;
     * also we still need to keep track of discarded chars for position calculations */
    chbuf_t *pp = &pw->chars; 
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

/* report error, possibly printing location information */
static void vrprintf(pws_t *pw, int startpos, const char *fmt, va_list args)
{
  chbuf_t cb = mkchb(); const char *s;
  int ln = 0, off = 0; assert(fmt);
  if (pw && pw->infile) chbputf(&cb, "%s:", pw->infile);
  if (pw && startpos >= 0) {
    pos2lnoff(pw, startpos, &ln, &off);
    if (ln > 0) chbputf(&cb, "%d:%d:", ln, off+1);
  }
  fflush(stdout); 
  if (chblen(&cb) > 0) {
    chbputc(' ', &cb);
    fputs(chbdata(&cb), stderr);
  }
  vfprintf(stderr, fmt, args); 
  fputc('\n', stderr);
  if (startpos >= 0 && ln > 0 && (s = recallline(pw, ln, &cb)) != NULL) {
    fputs(s, stderr); fputc('\n', stderr);
    while (off-- > 0) fputc(' ', stderr);
    fputs("^\n", stderr);
  }
  fflush(stderr); 
  chbfini(&cb);
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
  bufqsort(&g_syminfo, int_cmp);	
}

/* NB: unintern_symbol does not touch g_nodes! */
static void unintern_symbol(const char *name)
{
  sym_t s = intern(name);
  int *pi = bufbsearch(&g_syminfo, &s, int_cmp);
  if (pi) bufrem(&g_syminfo, bufoff(&g_syminfo, pi));
}

void init_symbols(void)
{
  node_t *pn;
  bufinit(&g_syminfo, sizeof(int)*3);
  ndbinit(&g_nodes);
  intern_symbol("auto", TT_AUTO_KW, -1);
  intern_symbol("break", TT_BREAK_KW, -1);
  intern_symbol("case", TT_CASE_KW, -1);
  intern_symbol("char", TT_CHAR_KW, -1);
  intern_symbol("const", TT_CONST_KW, -1);
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
  intern_symbol("int", TT_INT_KW, -1);
  intern_symbol("long", TT_LONG_KW, -1);
  intern_symbol("register", TT_REGISTER_KW, -1);
  intern_symbol("return", TT_RETURN_KW, -1);
  intern_symbol("short", TT_SHORT_KW, -1);
  intern_symbol("signed", TT_SIGNED_KW, -1);
  intern_symbol("static", TT_STATIC_KW, -1);
  intern_symbol("struct", TT_STRUCT_KW, -1);
  intern_symbol("switch", TT_SWITCH_KW, -1);
  intern_symbol("typedef", TT_TYPEDEF_KW, -1);
  intern_symbol("union", TT_UNION_KW, -1);
  intern_symbol("unsigned", TT_UNSIGNED_KW, -1);
  intern_symbol("void", TT_VOID_KW, -1);
  intern_symbol("volatile", TT_VOLATILE_KW, -1);
  intern_symbol("while", TT_WHILE_KW, -1);
  intern_symbol("bool", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_INT;
  intern_symbol("true", TT_ENUM_NAME, 1);
  intern_symbol("false", TT_ENUM_NAME, 0);
  intern_symbol("wchar_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_INT;
  intern_symbol("int8_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_CHAR;
  intern_symbol("uint8_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_UCHAR;
  intern_symbol("int16_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_SHORT;
  intern_symbol("uint16_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_USHORT;
  intern_symbol("int32_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_INT;
  intern_symbol("uint32_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_UINT;
  intern_symbol("int64_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_LLONG;
  intern_symbol("uint64_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_ULLONG;
  intern_symbol("size_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_ULONG;
  intern_symbol("ssize_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_LONG;
  intern_symbol("ptrdiff_t", TT_TYPE_NAME, ndblen(&g_nodes));
  pn = ndbnewbk(&g_nodes); ndset(pn, NT_TYPE, -1, -1); pn->ts = TS_LONG;
  intern_symbol("sizeof", TT_INTR_NAME, -1);
  intern_symbol("alignof", TT_INTR_NAME, -1);
  intern_symbol("offsetof", TT_INTR_NAME, -2);
  intern_symbol("alloca", TT_INTR_NAME, 1);
  intern_symbol("static_assert", TT_INTR_NAME, 2);
}

/* simple comparison of NT_TYPE nodes for equivalence */
bool same_type(const node_t *pctn1, const node_t *pctn2)
{
  node_t *ptn1 = (node_t*)pctn1, *ptn2 = (node_t*)pctn2;
  assert(ptn1->nt == NT_TYPE && ptn2->nt == NT_TYPE);
  if (ptn1->ts != ptn2->ts) return false;
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
          (pn1->nt != NT_VARDECL || pn2->nt == NT_VARDECL || 
           pn1->name != pn2->name)) return false;
      if (pn1->nt == NT_VARDECL && ndlen(pn1) == 1) pn1 = ndref(pn1, 0); 
      if (pn2->nt == NT_VARDECL && ndlen(pn2) == 1) pn2 = ndref(pn2, 0); 
      if (pn1->nt != NT_TYPE || pn2->nt != NT_TYPE || 
          !same_type(pn1, pn2)) return false;
    }
  } else if (ptn1->ts == TS_ARRAY || ptn1->ts == TS_ARRAY) {
    /* allow incomplete array to be equivalent to a complete one */
    node_t *pn1, *pn2;
    if (!ndlen(ptn1) || !ndlen(ptn2)) return true;
    if (ndlen(ptn1) != 1 || ndlen(ptn2) != 1) return false;
    pn1 = ndref(ptn1, 0), pn2 = ndref(ptn2, 0);
    if (pn1->nt != NT_LITERAL || pn2->nt != NT_LITERAL) return false;
    if (pn1->ts != pn2->ts) return false;
    switch (pn1->ts) { 
      case TS_INT: case TS_UINT: case TS_LONG: case TS_ULONG: break;
      default: return false;
    }
    if (!streql(chbdata(&pn1->data), chbdata(&pn2->data))) return false;
  }
  return true;
}

/* post imported/forward symbol to symbol table */
const node_t *post_symbol(sym_t mod, node_t *pvn)
{
  node_t *pin = NULL; int *pi, info;
  assert(pvn->nt == NT_VARDECL && pvn->name && ndlen(pvn) == 1);
  assert(ndref(pvn, 0)->nt == NT_TYPE);
  if ((pi = bufbsearch(&g_syminfo, &pvn->name, int_cmp)) != NULL) {
    /* see if what's there is equivalent to what we want */
    if (pi[1] == TT_IDENTIFIER && pi[2] >= 0) {
      assert(pi[2] < (int)buflen(&g_nodes));
      pin = bufref(&g_nodes, (size_t)pi[2]);
      if (pin->nt == NT_IMPORT && ndlen(pin) == 1) {
        node_t *ptn = ndref(pin, 0); assert(ptn->nt == NT_TYPE);
        if (same_type(ptn, ndref(pvn, 0))) return pin;
      }
    }
    //neprintf(pvn, "symbol already defined differently: %s", symname(pvn->name));
    n2eprintf(pvn, pin, "symbol already defined differently: %s", symname(pvn->name));
  }
  info = (int)ndblen(&g_nodes); pin = ndbnewbk(&g_nodes);
  ndset(pin, NT_IMPORT, pvn->pwsid, pvn->startpos);
  pin->name = mod; pin->sc = SC_NONE; /* changed to SC_EXTERN on reference */
  ndcpy(ndnewbk(pin), ndref(pvn, 0));
  intern_symbol(symname(pvn->name), TT_IDENTIFIER, info);
  return pin;
} 

void fini_symbols(void)
{
  buffini(&g_syminfo);
  ndbfini(&g_nodes);
  clearsyms();
}

static tt_t lookup_symbol(const char *name, int *pinfo)
{
  sym_t s = intern(name);
  int *pi = bufbsearch(&g_syminfo, &s, int_cmp);
  if (pinfo) *pinfo = pi ? pi[2] : -1;
  return pi ? (tt_t)pi[1] : TT_IDENTIFIER;  
}

static bool check_once(const char *fname)
{
  sym_t o = internf("once %s", fname);
  int *pi = bufbsearch(&g_syminfo, &o, int_cmp);
  if (pi != NULL) return false;
  intern_symbol(symname(o), TT_EOF, -1);
  return true;   
}

const node_t *lookup_global(sym_t name)
{
  const node_t *pn = NULL;
  int *pi = bufbsearch(&g_syminfo, &name, int_cmp);
  if (pi && pi[1] == TT_IDENTIFIER && pi[2] >= 0) {
    assert(pi[2] < (int)buflen(&g_nodes));
    pn = bufref(&g_nodes, (size_t)pi[2]);
  }
  return pn;
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
  pi = bufbsearch(&g_syminfo, &s, int_cmp);
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
tt_t lex(pws_t *pw, chbuf_t *pcb)
{
  char *tbase = chbdata(&pw->chars);
  int *pcuri = &pw->curi;
  int endi = (int)chblen(&pw->chars);
  int c; bool longprefix = false;
#define readchar() c = *pcuri < endi ? tbase[(*pcuri)++] : fetchline(pw, &tbase, &endi)
#define unreadchar() (*pcuri)--
  chbclear(pcb);
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '^') {
    chbputc(c, pcb);
    goto state_29;
  } else if (c == '%') {
    chbputc(c, pcb);
    goto state_28;
  } else if (c == '|') {
    chbputc(c, pcb);
    goto state_27;
  } else if (c == '&') {
    chbputc(c, pcb);
    goto state_26;
  } else if (c == '!') {
    chbputc(c, pcb);
    goto state_25;
  } else if (c == '=') {
    chbputc(c, pcb);
    goto state_24;
  } else if (c == '<') {
    chbputc(c, pcb);
    goto state_23;
  } else if (c == '>') {
    chbputc(c, pcb);
    goto state_22;
  } else if (c == '#') {
    chbputc(c, pcb);
    goto state_21;
  } else if (c == ';') {
    chbputc(c, pcb);
    return TT_SEMICOLON;
  } else if (c == ',') {
    chbputc(c, pcb);
    return TT_COMMA;
  } else if (c == '}') {
    chbputc(c, pcb);
    return TT_RBRC;
  } else if (c == '{') {
    chbputc(c, pcb);
    return TT_LBRC;
  } else if (c == ']') {
    chbputc(c, pcb);
    return TT_RBRK;
  } else if (c == '[') {
    chbputc(c, pcb);
    return TT_LBRK;
  } else if (c == ')') {
    chbputc(c, pcb);
    return TT_RPAR;
  } else if (c == '(') {
    chbputc(c, pcb);
    return TT_LPAR;
  } else if (c == '?') {
    chbputc(c, pcb);
    return TT_QMARK;
  } else if (c == ':') {
    chbputc(c, pcb);
    return TT_COLON;
  } else if (c == '~') {
    chbputc(c, pcb);
    return TT_TILDE;
  } else if (c == '\"') {
    goto state_11;
  } else if (c == '\'') {
    goto state_10;
  } else if (c == '-') {
    chbputc(c, pcb);
    goto state_9;
  } else if (c == '+') {
    chbputc(c, pcb);
    goto state_8;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_7;
  } else if (c == '0') {
    chbputc(c, pcb);
    goto state_6;
  } else if ((c >= '1' && c <= '9')) {
    chbputc(c, pcb);
    goto state_5;
  } else if ((c == 'L')) {
    goto state_4L;
  } else if ((c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z')) {
    chbputc(c, pcb);
    goto state_4;
  } else if (c == '*') {
    chbputc(c, pcb);
    goto state_3;
  } else if (c == '/') {
    chbputc(c, pcb);
    goto state_2;
  } else if ((c >= '\t' && c <= '\n') || (c >= '\f' && c <= '\r') || c == ' ') {
    chbputc(c, pcb);
    goto state_1;
  } else {
    unreadchar();
    goto err;
  }
state_1:
  readchar();
  if (c == EOF) {
    return TT_WHITESPACE;
  } else if ((c >= '\t' && c <= '\n') || (c >= '\f' && c <= '\r') || c == ' ') {
    chbputc(c, pcb);
    goto state_1;
  } else {
    unreadchar();
    return TT_WHITESPACE;
  }
state_2:
  readchar();
  if (c == EOF) {
    return TT_SLASH;
  } else if (c == '*') {
    chbputc(c, pcb);
    goto state_76;
  } else if (c == '/') {
    chbputc(c, pcb);
    goto state_75;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_SLASH_ASN;
  } else {
    unreadchar();
    return TT_SLASH;
  }
state_3:
  readchar();
  if (c == EOF) {
    return TT_STAR;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_STAR_ASN;
  } else {
    unreadchar();
    return TT_STAR;
  }
state_4L:
  readchar();
  if (c == EOF) {
    chbputc('L', pcb);
    return lookup_symbol(chbdata(pcb), NULL);
  } else if (c == '\"') {
    longprefix = true;
    goto state_11;
  } else if (c == '\'') {
    longprefix = true;
    goto state_10;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z')) {
    chbputc('L', pcb);
    chbputc(c, pcb);
    goto state_4;
  } else {
    unreadchar();
    chbputc('L', pcb);
    return lookup_symbol(chbdata(pcb), NULL);
  }
state_4:
  readchar();
  if (c == EOF) {
    return lookup_symbol(chbdata(pcb), NULL);
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= 'a' && c <= 'z')) {
    chbputc(c, pcb);
    goto state_4;
  } else {
    unreadchar();
    return lookup_symbol(chbdata(pcb), NULL);
  }
state_5:
  readchar();
  if (c == EOF) {
    return TT_INT;
  } else if (c == 'U' || c == 'u') {
    goto state_74;
  } else if (c == 'L' || c == 'l') {
    readchar();
    if (c == 'L' || c == 'l') goto state_73LL;
    if (c != EOF) unreadchar();
    goto state_73;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_66;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_62;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_5;
  } else {
    unreadchar();
    return TT_INT;
  }
state_6:
  readchar();
  if (c == EOF) {
    return TT_INT;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_66;
  } else if ((c >= '8' && c <= '9')) {
    chbputc(c, pcb);
    goto state_65;
  } else if ((c >= '0' && c <= '7')) {
    chbputc(c, pcb);
    goto state_64;
  } else if (c == 'U' || c == 'u') {
    goto state_74;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_62;
  } else if (c == 'X' || c == 'x') {
    chbputc(c, pcb);
    goto state_61;
  } else if (c == 'L' || c == 'l') {
    readchar();
    if (c == 'L' || c == 'l') goto state_73LL;
    if (c != EOF) unreadchar();
    goto state_73;
  } else {
    unreadchar();
    return TT_INT;
  }
state_7:
  readchar();
  if (c == EOF) {
    return TT_DOT;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_55;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_54;
  } else {
    unreadchar();
    return TT_DOT;
  }
state_8:
  readchar();
  if (c == EOF) {
    return TT_PLUS;
  } else if (c == '+') {
    chbputc(c, pcb);
    return TT_PLUS_PLUS;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_PLUS_ASN;
  } else {
    unreadchar();
    return TT_PLUS;
  }
state_9:
  readchar();
  if (c == EOF) {
    return TT_MINUS;
  } else if (c == '-') {
    chbputc(c, pcb);
    return TT_MINUS_MINUS;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_MINUS_ASN;
  } else if (c == '>') {
    chbputc(c, pcb);
    return TT_ARROW;
  } else {
    unreadchar();
    return TT_MINUS;
  }
state_10:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '\\') {
    chbputc(c, pcb);
    goto state_44;
  } else if (is8chead(c)) { 
    /* esl: this whole branch is added manually! */
    int u = c & 0xFF;
    if (u < 0xE0) { chbputc(c, pcb); goto state_10d; }
    if (u < 0xF0) { chbputc(c, pcb); goto state_10dd; }
    if (u < 0xF8) { chbputc(c, pcb); goto state_10ddd; }
    if (u < 0xFC) { chbputc(c, pcb); goto state_10dddd; }
    if (u < 0xFE) { chbputc(c, pcb); goto state_10ddddd; }
    unreadchar();
    goto err;
  } else if (!(c == '\n' || c == '\'' || c == '\\')) {
    chbputc(c, pcb);
    goto state_43;
  } else {
    unreadchar();
    goto err;
  }
/* esl: all 'd' states were added manually */
state_10ddddd:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (is8ctail(c)) {
    chbputc(c, pcb);
    goto state_10dddd;
  } else {
    unreadchar();
    goto err;
  }
state_10dddd:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (is8ctail(c)) {
    chbputc(c, pcb);
    goto state_10ddd;
  } else {
    unreadchar();
    goto err;
  }
state_10ddd:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (is8ctail(c)) {
    chbputc(c, pcb);
    goto state_10dd;
  } else {
    unreadchar();
    goto err;
  }
state_10dd:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (is8ctail(c)) {
    chbputc(c, pcb);
    goto state_10d;
  } else {
    unreadchar();
    goto err;
  }
state_10d:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (is8ctail(c)) {
    chbputc(c, pcb);
    goto state_43;
  } else {
    unreadchar();
    goto err;
  }
state_11:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '\\') {
    chbputc(c, pcb);
    goto state_34;
  } else if (c == '\"') {
    return longprefix ? TT_LSTRING : TT_STRING;
  } else if (!(c == '\n' || c == '\"' || c == '\\')) {
    chbputc(c, pcb);
    goto state_11;
  } else {
    unreadchar();
    goto err;
  }
state_21:
  readchar();
  if (c == EOF) {
    return TT_HASH;
  } else if (c == '#') {
    chbputc(c, pcb);
    goto state_32;
  } else {
    unreadchar();
    return TT_HASH;
  }
state_22:
  readchar();
  if (c == EOF) {
    return TT_GT;
  } else if (c == '>') {
    chbputc(c, pcb);
    goto state_31;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_GE;
  } else {
    unreadchar();
    return TT_GT;
  }
state_23:
  readchar();
  if (c == EOF) {
    return TT_LT;
  } else if (c == '<') {
    chbputc(c, pcb);
    goto state_30;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_LE;
  } else {
    unreadchar();
    return TT_LT;
  }
state_24:
  readchar();
  if (c == EOF) {
    return TT_ASN;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_EQ;
  } else {
    unreadchar();
    return TT_ASN;
  }
state_25:
  readchar();
  if (c == EOF) {
    return TT_NOT;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_NE;
  } else {
    unreadchar();
    return TT_NOT;
  }
state_26:
  readchar();
  if (c == EOF) {
    return TT_AND;
  } else if (c == '&') {
    chbputc(c, pcb);
    return TT_AND_AND;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_AND_ASN;
  } else {
    unreadchar();
    return TT_AND;
  }
state_27:
  readchar();
  if (c == EOF) {
    return TT_OR;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_OR_ASN;
  } else if (c == '|') {
    chbputc(c, pcb);
    return TT_OR_OR;
  } else {
    unreadchar();
    return TT_OR;
  }
state_28:
  readchar();
  if (c == EOF) {
    return TT_REM;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_REM_ASN;
  } else {
    unreadchar();
    return TT_REM;
  }
state_29:
  readchar();
  if (c == EOF) {
    return TT_XOR;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_XOR_ASN;
  } else {
    unreadchar();
    return TT_XOR;
  }
state_30:
  readchar();
  if (c == EOF) {
    return TT_SHL;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_SHL_ASN;
  } else {
    unreadchar();
    return TT_SHL;
  }
state_31:
  readchar();
  if (c == EOF) {
    return TT_SHR;
  } else if (c == '=') {
    chbputc(c, pcb);
    return TT_SHR_ASN;
  } else {
    unreadchar();
    return TT_SHR;
  }
state_32:
  return TT_HASHHASH;
state_34:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '7')) {
    chbputc(c, pcb);
    goto state_37;
  } else if (c == 'u') {
    chbputc(c, pcb);
    goto state_36;
  } else if (c == 'x') {
    chbputc(c, pcb);
    goto state_35;
  } else if (c == '\"' || c == '\'' || c == '?' || c == '\\' || (c >= 'a' && c <= 'b') || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v') {
    chbputc(c, pcb);
    goto state_11;
  } else {
    unreadchar();
    goto err;
  }
state_35:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_42;
  } else {
    unreadchar();
    goto err;
  }
state_36:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_39;
  } else {
    unreadchar();
    goto err;
  }
state_37:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '7')) {
    chbputc(c, pcb);
    goto state_38;
  } else if (c == '\\') {
    chbputc(c, pcb);
    goto state_34;
  } else if (c == '\"') {
    return longprefix ? TT_LSTRING : TT_STRING;
  } else if (!(c == '\n' || c == '\"' || (c >= '0' && c <= '7') || c == '\\')) {
    chbputc(c, pcb);
    goto state_11;
  } else {
    unreadchar();
    goto err;
  }
state_38:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '\\') {
    chbputc(c, pcb);
    goto state_34;
  } else if (c == '\"') {
    return longprefix ? TT_LSTRING : TT_STRING;
  } else if (!(c == '\n' || c == '\"' || c == '\\')) {
    chbputc(c, pcb);
    goto state_11;
  } else {
    unreadchar();
    goto err;
  }
state_39:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_40;
  } else {
    unreadchar();
    goto err;
  }
state_40:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_41;
  } else {
    unreadchar();
    goto err;
  }
state_41:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_11;
  } else {
    unreadchar();
    goto err;
  }
state_42:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_42;
  } else if (c == '\\') {
    chbputc(c, pcb);
    goto state_34;
  } else if (c == '\"') {
    return longprefix ? TT_LSTRING : TT_STRING;
  } else if (!(c == '\n' || c == '\"' || (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == '\\' || (c >= 'a' && c <= 'f'))) {
    chbputc(c, pcb);
    goto state_11;
  } else {
    unreadchar();
    goto err;
  }
state_43:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '\'') {
    return longprefix ? TT_LCHAR : TT_CHAR;
  } else {
    unreadchar();
    goto err;
  }
state_44:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '7')) {
    chbputc(c, pcb);
    goto state_47;
  } else if (c == 'u') {
    chbputc(c, pcb);
    goto state_46;
  } else if (c == 'x') {
    chbputc(c, pcb);
    goto state_45;
  } else if (c == '\"' || c == '\'' || c == '?' || c == '\\' || (c >= 'a' && c <= 'b') || c == 'f' || c == 'n' || c == 'r' || c == 't' || c == 'v') {
    chbputc(c, pcb);
    goto state_43;
  } else {
    unreadchar();
    goto err;
  }
state_45:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_53;
  } else {
    unreadchar();
    goto err;
  }
state_46:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_50;
  } else {
    unreadchar();
    goto err;
  }
state_47:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '\'') {
    return longprefix ? TT_LCHAR : TT_CHAR;
  } else if ((c >= '0' && c <= '7')) {
    chbputc(c, pcb);
    goto state_48;
  } else {
    unreadchar();
    goto err;
  }
state_48:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '\'') {
    return longprefix ? TT_LCHAR : TT_CHAR;
  } else if ((c >= '0' && c <= '7')) {
    chbputc(c, pcb);
    goto state_43;
  } else {
    unreadchar();
    goto err;
  }
state_50:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_51;
  } else {
    unreadchar();
    goto err;
  }
state_51:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_52;
  } else {
    unreadchar();
    goto err;
  }
state_52:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_43;
  } else {
    unreadchar();
    goto err;
  }
state_53:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_53;
  } else if (c == '\'') {
    return longprefix ? TT_LCHAR : TT_CHAR;
  } else {
    unreadchar();
    goto err;
  }
state_54:
  readchar();
  if (c == EOF) {
    return TT_DOUBLE;
  } else if (c == 'F' || c == 'f') {
    return TT_FLOAT;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_57;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_54;
  } else {
    unreadchar();
    return TT_DOUBLE;
  }
state_55:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_56;
  } else {
    unreadchar();
    goto err;
  }
state_56:
  return TT_ELLIPSIS;
state_57:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '+' || c == '-') {
    chbputc(c, pcb);
    goto state_59;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_58;
  } else {
    unreadchar();
    goto err;
  }
state_58:
  readchar();
  if (c == EOF) {
    return TT_DOUBLE;
  } else if (c == 'F' || c == 'f') {
    return TT_FLOAT;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_58;
  } else {
    unreadchar();
    return TT_DOUBLE;
  }
state_59:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_58;
  } else {
    unreadchar();
    goto err;
  }
/* state_60 merged with state_73 */
state_61:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_70;
  } else {
    unreadchar();
    goto err;
  }
state_62:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '+' || c == '-') {
    chbputc(c, pcb);
    goto state_69;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_68;
  } else {
    unreadchar();
    goto err;
  }
/* state_63 merged with state_74 */
state_64:
  readchar();
  if (c == EOF) {
    return TT_INT;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_66;
  } else if ((c >= '8' && c <= '9')) {
    chbputc(c, pcb);
    goto state_65;
  } else if ((c >= '0' && c <= '7')) {
    chbputc(c, pcb);
    goto state_64;
  } else if (c == 'U' || c == 'u') {
    goto state_74;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_62;
  } else if (c == 'L' || c == 'l') {
    readchar();
    if (c == 'L' || c == 'l') goto state_73LL;
    if (c != EOF) unreadchar();
    goto state_73;
  } else {
    unreadchar();
    return TT_INT;
  }
state_65:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_66;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_65;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_62;
  } else {
    unreadchar();
    goto err;
  }
state_66:
  readchar();
  if (c == EOF) {
    return TT_DOUBLE;
  } else if (c == 'F' || c == 'f') {
    return TT_FLOAT;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_57;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_54;
  } else {
    unreadchar();
    return TT_DOUBLE;
  }
state_68:
  readchar();
  if (c == EOF) {
    return TT_DOUBLE;
  } else if (c == 'F' || c == 'f') {
    return TT_FLOAT;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_68;
  } else {
    unreadchar();
    return TT_DOUBLE;
  }
state_69:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_68;
  } else {
    unreadchar();
    goto err;
  }
state_70:
  readchar();
  if (c == EOF) {
    return TT_INT;
  } else if (c == 'U' || c == 'u') {
    goto state_74;
  } else if (c == 'L' || c == 'l') {
    readchar();
    if (c == 'L' || c == 'l') goto state_73LL;
    if (c != EOF) unreadchar();
    goto state_73;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_70;
  } else {
    unreadchar();
    return TT_INT;
  }
/* state_71 merged with state_73 */
/* state_72 merged with state_74 */
state_73:
  readchar();
  if (c == EOF) {
    return TT_LONG;
  } else if (c == 'U' || c == 'u') {
    return TT_ULONG;
  } else {
    unreadchar();
    return TT_LONG;
  }
state_73LL:
  readchar();
  if (c == EOF) {
    return TT_LLONG;
  } else if (c == 'U' || c == 'u') {
    return TT_ULLONG;
  } else {
    unreadchar();
    return TT_LLONG;
  }
state_74:
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
state_75:
  readchar();
  if (c == EOF) {
    goto eoferr;
  } else if (!(c == '\n')) {
    chbputc(c, pcb);
    goto state_75;
  } else {
    chbputc(c, pcb);
    return TT_WHITESPACE;
  }
state_76:
  readchar();
  if (c == EOF) {
    goto eoferr;
  } else if (!(c == '*')) {
    chbputc(c, pcb);
    goto state_76;
  } else {
    chbputc(c, pcb);
    goto state_77;
  }
state_77:
  readchar();
  if (c == EOF) {
    goto eoferr;
  } else if (c == '*') {
    chbputc(c, pcb);
    goto state_77;
  } else if (!(c == '*' || c == '/')) {
    chbputc(c, pcb);
    goto state_76;
  } else {
    chbputc(c, pcb);
    return TT_WHITESPACE;
  }

err:
eoferr:
  return TT_EOF;
#undef readchar
#undef unreadchar
}

static tt_t peekt(pws_t *pw) 
{ 
  if (!pw->gottk) {
    do { /* fetch next non-whitespace token */ 
      pw->pos = (int)pw->discarded + pw->curi; 
      pw->ctk = lex(pw, &pw->token); 
      if (pw->ctk == TT_EOF && !pw->inateof) 
        reprintf(pw, pw->pos, "illegal token"); 
    } while (pw->ctk == TT_WHITESPACE); 
    pw->gottk = true; 
    pw->tokstr = chbdata(&pw->token); 
  } 
  return pw->ctk; 
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

/* expect and rean non-quoted-literal token tk */ 
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

node_t* ndinit(node_t* pn)
{
  memset(pn, 0, sizeof(node_t));
  chbinit(&pn->data);
  ndbinit(&pn->body);
  return pn;
}

void ndicpy(node_t* mem, const node_t* pn)
{
  memcpy(mem, pn, sizeof(node_t));
  chbicpy(&mem->data, &pn->data);
  ndbicpy(&mem->body, &pn->body);
}

void ndfini(node_t* pn)
{
  chbfini(&pn->data);
  ndbfini(&pn->body);
}

void ndcpy(node_t* pn, const node_t* pr)
{
  ndfini(pn);
  ndicpy(pn, pr);
}

void ndset(node_t *dst, nt_t nt, int pwsid, int startpos)
{
  dst->nt = nt;
  dst->pwsid = pwsid;
  dst->startpos = startpos;
  dst->name = 0;
  chbclear(&dst->data);
  dst->op = TT_EOF;
  dst->ts = TS_VOID;
  dst->sc = SC_NONE;
  ndbclear(&dst->body);
}

void ndclear(node_t* pn)
{
  ndfini(pn);
  ndinit(pn);
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
  assert(pb); assert(pb->esz = sizeof(node_t));
  pnd = (node_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) ndfini(pnd+i);
  buffini(pb);
}

void ndbclear(ndbuf_t* pb)
{
  node_t *pnd; size_t i;
  assert(pb); assert(pb->esz = sizeof(node_t));
  pnd = (node_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) ndfini(pnd+i);
  bufclear(pb);
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

static bool type_specifier_ahead(pws_t *pw)
{
  switch (peekt(pw)) {
    case TT_CHAR_KW:   case TT_CONST_KW:    case TT_DOUBLE_KW:
    case TT_ENUM_KW:   case TT_FLOAT_KW:    case TT_INT_KW:
    case TT_LONG_KW:   case TT_SHORT_KW:    case TT_SIGNED_KW:
    case TT_STRUCT_KW: case TT_UNION_KW:    case TT_UNSIGNED_KW:
    case TT_VOID_KW:   case TT_VOLATILE_KW: case TT_TYPE_NAME:
      return true;
  }
  return false;
}

static bool storage_class_specifier_ahead(pws_t *pw)
{
  switch (peekt(pw)) {
    case TT_EXTERN_KW: case TT_STATIC_KW:   
    case TT_AUTO_KW:   case TT_REGISTER_KW:
      return true;
  }
  return false;
}

/* defined below */
static void parse_expr(pws_t *pw, node_t *pn);
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

static void patch_macro_template(buf_t *pids, ndbuf_t *ppars, node_t *pn)
{
  if (pn->nt == NT_IDENTIFIER) {
    size_t n = buflen(pids), i; sym_t *pi = bufdata(pids); 
    for (i = 0; i < n; ++i) { /* linear search is ok here */
      if (pi[i] != pn->name) continue;
      ndcpy(pn, ndbref(ppars, i)); break; 
    }
  } else {
    size_t i;
    for (i = 0; i < ndlen(pn); ++i) 
      patch_macro_template(pids, ppars, ndref(pn, i));
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
    case TT_INT: case_i32: { /* 0 .. 2147483647 (INT32_MIN is written as = -2147483647-1) */
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ul = strtoul(ns, NULL, 0), !errno && ul <= 0x7FFFFFFFUL) {
        chbputlu(ul, &pn->data);
      } else reprintf(pw, startpos, "signed int literal overflow"); 
      pn->ts = TS_INT;
      dropt(pw);
    } break;
    case TT_UINT: case_u32: { /* 0 .. 4294967295 */
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ul = strtoul(ns, NULL, 0), !errno && ul <= 0xFFFFFFFFUL) {
        chbputlu(ul, &pn->data);
      } else reprintf(pw, startpos, "unsigned int literal overflow"); 
      pn->ts = TS_UINT;
      dropt(pw);
    } break;
    case TT_LONG: /* longs are the same size as pointers */
      if (1) goto case_i32; /* wasm32 model */
      else goto case_i64; /* wasm64 model */
    case TT_ULONG: /* ulongs are the same size as pointers */
      if (1) goto case_u32; /* wasm32 model */
      else goto case_u64; /* wasm64 model */
    case TT_LLONG: case_i64: { /* 0 .. 9223372036854775807 (INT64_MIN is written as = -9223372036854775807-1) */
      char *ns = pw->tokstr; unsigned long long ull;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ull = strtoull(ns, NULL, 0), !errno && ull <= 0x7FFFFFFFFFFFFFFFULL) {
        chbputllu(ull, &pn->data);
      } else reprintf(pw, startpos, "signed long literal overflow"); 
      pn->ts = TS_LLONG;
      dropt(pw);
    } break;
    case TT_ULLONG: case_u64: { /* 0 .. 18446744073709551615 */
      char *ns = pw->tokstr; unsigned long long ull;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ull = strtoull(ns, NULL, 0), !errno && ull <= 0xFFFFFFFFFFFFFFFFULL) {
        chbputllu(ull, &pn->data);
      } else reprintf(pw, startpos, "unsigned long literal overflow"); 
      pn->ts = TS_ULLONG;
      dropt(pw);
    } break;
    case TT_FLOAT: {
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      chbsets(&pn->data, pw->tokstr);
      pn->ts = TS_FLOAT;
      dropt(pw);
    } break;
    case TT_DOUBLE: {
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      chbsets(&pn->data, pw->tokstr);
      pn->ts = TS_DOUBLE;
      dropt(pw);
    } break;
    case TT_CHAR: {
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ul = strtocc32(ns, NULL), !errno && ul <= 0x10FFFFUL) {
        chbputlu(ul, &pn->data);
      } else reprintf(pw, startpos, "char literal overflow");
      pn->ts = TS_INT;
      dropt(pw);
    } break;
    case TT_LCHAR: {
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      if (errno = 0, ul = strtocc32(ns, NULL), !errno && ul <= 0x10FFFFUL) {
        chbputlu(ul, &pn->data);
      } else reprintf(pw, startpos, "long char literal overflow");
      pn->ts = TS_LONG;
      dropt(pw);
    } break;
    case TT_STRING: {
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      while (*ns && (errno = 0, ul = strtocc32(ns, &ns), !errno && ul <= 0x10FFFFUL)) {
        chbputlc(ul, &pn->data); /* in utf-8 */
      } 
      if (*ns) reprintf(pw, startpos+(ns-pw->tokstr), "string literal char overflow");
      pn->ts = TS_STRING;
      dropt(pw);
    } break;
    case TT_LSTRING: {
      char *ns = pw->tokstr; unsigned long ul;
      ndset(pn, NT_LITERAL, pw->id, startpos); 
      while (*ns && (errno = 0, ul = strtocc32(ns, &ns), !errno && ul <= 0x10FFFFUL)) {
        chbputlu(ul, &pn->data); if (*ns) chbputc(' ', &pn->data); /* in decimal, space-separated */
      } 
      if (*ns) reprintf(pw, startpos+(ns-pw->tokstr), "long string literal char overflow");
      pn->ts = TS_LSTRING;
      dropt(pw);
    } break;
    case TT_IDENTIFIER: {
      ndset(pn, NT_IDENTIFIER, pw->id, startpos); 
      pn->name = getid(pw);
    } break;
    case TT_ENUM_NAME: {
      int info = 42; lookup_symbol(pw->tokstr, &info);
      ndset(pn, NT_LITERAL, pw->id, startpos);
      pn->ts = TS_INT; chbputd(info, &pn->data);
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
        bufinit(&ids, sizeof(sym_t)); ndbinit(&pars); 
        assert(pfn->nt == NT_TYPE && pfn->ts == TS_FUNCTION); 
        expect(pw, TT_LPAR, "(");
        while (peekt(pw) != TT_RPAR) {
          parse_assignment_expr(pw, ndbnewbk(&pars));
          if (peekt(pw) == TT_COMMA) dropt(pw);
          else break;
        }
        expect(pw, TT_RPAR, ")"); 
        if (ndlen(pfn) != ndblen(&pars)+1)
          reprintf(pw, startpos, "invalid number of parameters to macro");
        for (i = 1/* skip null type */; i < ndlen(pfn); ++i) {
          node_t *pin = ndref(pfn, i);
          assert(pin->nt == NT_VARDECL && pin->name != 0);
          *(sym_t*)bufnewbk(&ids) = pin->name;
        }
        ndcpy(pn, ndref(pdn, 1));
        patch_macro_template(&ids, &pars, pn);
        buffini(&ids); ndbfini(&pars); 
      } else assert(false);
    } break;
    case TT_INTR_NAME: {
      /* intrinsic application */
      int argc = INT_MIN; lookup_symbol(pw->tokstr, &argc);
      if (argc == INT_MIN) 
        reprintf(pw, startpos, "use of undefined intrinsic??");
      ndset(pn, NT_INTRCALL, pw->id, startpos);
      pn->name = intern(pw->tokstr); dropt(pw);
      if (argc == -1) { /* (type) */
        node_t *ptn = ndnewbk(pn);
        expect(pw, TT_LPAR, "(");
        parse_base_type(pw, ptn);
        if (parse_declarator(pw, ptn)) 
          reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
        expect(pw, TT_RPAR, ")"); 
      } else if (argc == -2) { /* (type, id) */
        node_t *ptn = ndnewbk(pn);
        expect(pw, TT_LPAR, "(");
        parse_base_type(pw, ptn);
        if (parse_declarator(pw, ptn)) 
          reprintf(pw, startpos, "unexpected identifier in abstract type specifier");
        expect(pw, TT_COMMA, ",");
        ptn = ndnewbk(pn);
        ndset(ptn, NT_IDENTIFIER, pw->id, peekpos(pw));
        ptn->name = getid(pw); 
        expect(pw, TT_RPAR, ")"); 
      } else { /* (expression, ...) */
        expect(pw, TT_LPAR, "(");
        while (peekt(pw) != TT_RPAR) {
          parse_assignment_expr(pw, ndnewbk(pn));
          if (peekt(pw) != TT_COMMA) break;
          dropt(pw); 
        }
        expect(pw, TT_RPAR, ")"); 
      }
      if (argc >= 0 && (size_t)argc != ndlen(pn))
        reprintf(pw, startpos, "%s intrinsic: %d argument(s) expected", 
                 symname(pn->name), argc);
    } break;
    default:
      reprintf(pw, startpos, "valid expression expected");
  }
}

static bool postfix_operator_ahead(pws_t *pw)
{
  switch (peekt(pw)) {
    case TT_LBRK:      case TT_LPAR:        case TT_DOT:
    case TT_ARROW:     case TT_PLUS_PLUS:   case TT_MINUS_MINUS:
      return true;
  }
  return false;
}

/* wrap node into NT_SUBSCRIPT node */
void wrap_subscript(node_t *pn, node_t *psn)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_SUBSCRIPT, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(psn, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
}

/* wrap expr node into NT_POSTFIX type node */
void wrap_postfix_operator(node_t *pn, tt_t op, sym_t id)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_POSTFIX, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  nd.op = op; nd.name = id;
  ndswap(pn, &nd);
  ndfini(&nd);
}

static void parse_postfix_expr(pws_t *pw, node_t *pn)
{
  int startpos = peekpos(pw);
  parse_primary_expr(pw, pn);
  /* cast type (t) can't be postfixed */
  if (pn->nt == NT_TYPE) return;
  /* wrap pn into postfix operators */
  while (postfix_operator_ahead(pw)) {
    switch (pw->ctk) {
      case TT_LBRK: {
        node_t nd; ndinit(&nd);
        dropt(pw);
        parse_expr(pw, &nd);
        expect(pw, TT_RBRK, "]");
        wrap_subscript(pn, &nd);
        ndfini(&nd);
      } break;      
      case TT_LPAR: {
        node_t nd; ndinit(&nd);
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
        dropt(pw);
        wrap_postfix_operator(pn, pw->ctk, getid(pw));
      } break;
      case TT_PLUS_PLUS: case TT_MINUS_MINUS: {
        dropt(pw);
        wrap_postfix_operator(pn, pw->ctk, 0);
      } break;
      default: assert(false);
    }  
  }
}

/* wrap expr node into NT_PREFIX type node */
void wrap_unary_operator(node_t *pn, int startpos, tt_t op)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_PREFIX, pn->pwsid, startpos);
  ndswap(pn, ndnewbk(&nd));
  nd.op = op;
  ndswap(pn, &nd);
  ndfini(&nd);
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

/* wrap expr node into NT_CAST type node */
void wrap_cast(node_t *pcn, node_t *pn)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_CAST, pcn->pwsid, pcn->startpos);
  ndswap(pcn, ndnewbk(&nd));
  ndswap(pn, ndnewbk(&nd));
  ndswap(pcn, &nd);
  ndfini(&nd);
}

static void parse_cast_expr(pws_t *pw, node_t *pn)
{
  int startpos = peekpos(pw);
  parse_unary_expr(pw, pn);
  if (pn->nt == NT_TYPE) {
    node_t nd; ndinit(&nd);
    if (peekt(pw) == TT_LBRC) parse_initializer(pw, &nd);
    else parse_cast_expr(pw, &nd);
    wrap_cast(pn, &nd);
    ndfini(&nd);
  }
}

/* wrap expr node into NT_INFIX with second expr */
void wrap_binary(node_t *pn, tt_t op, node_t *pn2)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_INFIX, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn2, ndnewbk(&nd));
  nd.op = op;
  ndswap(pn, &nd);
  ndfini(&nd);
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
    node_t nd; ndinit(&nd);
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

/* wrap expr node into NT_COND with second/third exprs */
void wrap_conditional(node_t *pn, node_t *pn2, node_t *pn3)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_COND, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn2, ndnewbk(&nd));
  ndswap(pn3, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
}

static void parse_conditional_expr(pws_t *pw, node_t *pn)
{
  parse_binary_expr(pw, pn, 'd');
  if (peekt(pw) == TT_QMARK) {
    node_t ndt, ndf; ndinit(&ndt); ndinit(&ndf);
    dropt(pw);
    parse_expr(pw, &ndt);
    expect(pw, TT_COLON, ":");
    parse_conditional_expr(pw, &ndf);
    wrap_conditional(pn, &ndt, &ndf);
    ndfini(&ndt); ndfini(&ndf);
  }
}

/* wrap expr node into NT_ASSIGN with second expr */
void wrap_assignment(node_t *pn, tt_t op, node_t *pn2)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_ASSIGN, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn2, ndnewbk(&nd));
  nd.op = op;
  ndswap(pn, &nd);
  ndfini(&nd);
}

static void parse_assignment_expr(pws_t *pw, node_t *pn)
{
  parse_conditional_expr(pw, pn);
  switch (pn->nt) {
    case NT_IDENTIFIER: case NT_SUBSCRIPT: case NT_PREFIX: {
      switch (peekt(pw)) {
        case TT_ASN:      case TT_AND_ASN:  case TT_OR_ASN:
        case TT_XOR_ASN:  case TT_REM_ASN:  case TT_SLASH_ASN:
        case TT_STAR_ASN: case TT_PLUS_ASN: case TT_MINUS_ASN:
        case TT_SHL_ASN:  case TT_SHR_ASN: {
          tt_t op; node_t nd; ndinit(&nd); 
          op = pw->ctk; dropt(pw);
          parse_assignment_expr(pw, &nd);
          wrap_assignment(pn, op, &nd);
          ndfini(&nd);
        }
      }
    }
  }
}

/* wrap expr node into NT_COMMA with second expr */
void wrap_comma(node_t *pn, node_t *pn2)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_COMMA, pn->pwsid, pn->startpos);
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn2, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
}

static void parse_expr(pws_t *pw, node_t *pn)
{
  parse_assignment_expr(pw, pn);
  while (peekt(pw) == TT_COMMA) {
    node_t nd; ndinit(&nd); 
    dropt(pw);
    parse_assignment_expr(pw, &nd);
    wrap_comma(pn, &nd);
    ndfini(&nd);
  }
}

/* fill pn with definition of typedef'd type tn */
static void load_typedef_type(pws_t *pw, node_t *pn, sym_t tn)
{
  int info = -1; 
  lookup_symbol(symname(tn), &info);
  if (info < 0) reprintf(pw, pw->pos, "can't find definition of type: %s", symname(tn));
  assert(info >= 0 && info < (int)ndblen(&g_nodes));
  ndcpy(pn, ndbref(&g_nodes, (size_t)info));
  assert(pn->nt == NT_TYPE);
}

/* parse enum body (name? + enumlist?) */
void parse_enum_body(pws_t *pw, node_t *pn)
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
      pi = bufbsearch(&g_syminfo, &pni->name, int_cmp);
      if (pi) neprintf(pni, "enum constant name is already in use");
      pnv = ndnewbk(pni);
      if (peekt(pw) == TT_ASN) {
        dropt(pw);
        parse_conditional_expr(pw, pnv);
        if (!static_eval_to_int(pnv, &curval))
          neprintf(pnv, "invalid enum initializer (int constant expected)");
      }
      ndset(pnv, NT_LITERAL, pw->id, pni->startpos);
      pnv->ts = TS_INT; chbputd(curval, &pnv->data);
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
    int *pinfo = bufbsearch(&g_syminfo, &s, int_cmp);
    if (pinfo) {
      reprintf(pw, pn->startpos, "redefinition of %s", symname(s));
    } else {
      intern_symbol(symname(s), tt, ndblen(&g_nodes));
      ndcpy(ndbnewbk(&g_nodes), pn);
    }
  }
}

/* parse struct or union body (name? + declist?) */
void parse_sru_body(pws_t *pw, ts_t sru, node_t *pn)
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
    if (pni->nt != NT_VARDECL) 
      reprintf(pw, pni->startpos, "unexpected initializer in struct/union"); 
  }
  if (!pn->name && !ndlen(pn)) {
    reprintf(pw, pn->startpos, "incomplete struct/union");
  } else if (pn->name && ndlen(pn)) {
    /* register this struct/union type for lazy fetch */
    const char *tag = (sru == TS_STRUCT) ? "struct" : "union";
    tt_t tt = (sru == TS_STRUCT) ? TT_STRUCT_KW : TT_UNION_KW;
    sym_t s = internf("%s %s", tag, symname(pn->name));
    int *pinfo = bufbsearch(&g_syminfo, &s, int_cmp);
    if (pinfo) {
      reprintf(pw, pn->startpos, "redefinition of %s", symname(s));
    } else {
      intern_symbol(symname(s), tt, ndblen(&g_nodes));
      ndcpy(ndbnewbk(&g_nodes), pn);
    }
  }
}

/* base types are limited to scalars/named types */
static void parse_base_type(pws_t *pw, node_t *pn)
{
  int v = 0, c = 0, s = 0, i = 0, l = 0, f = 0, d = 0;
  int sg = 0, ug = 0, en = 0, st = 0, un = 0, ty = 0;   
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
      case TT_STRUCT_KW:   st = 1; break;
      case TT_UNION_KW:    un = 1; break;
      case TT_TYPE_NAME:   ty = 1; tn = intern(pw->tokstr); break;
      case TT_CONST_KW:    /* ignored */ break;  
      case TT_VOLATILE_KW: /* ignored */ break;
      default: assert(false);
    }
    dropt(pw);
  }
  if (c + s + l > 0 && i > 0) i = 0;
  if (v + c + s + i + l + f + d + en + st + un + ty == 0 && sg + ug != 0) i = 1;
  if (!!l + f + d > 1)
    reprintf(pw, pw->pos, "invalid type specifier: unsupported type"); 
  if (v + c + s + i + !!l + f + d + en + st + un + ty != 1 || sg + ug > 1)
    reprintf(pw, pw->pos, "invalid type specifier: missing or conflicting keywords"); 
  if (f + d + en + st + un + ty > 0 && sg + ug > 0)
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
  else if (st) parse_sru_body(pw, TS_STRUCT, pn);
  else if (un) parse_sru_body(pw, TS_UNION, pn);
  else if (ty) load_typedef_type(pw, pn, tn);
  else assert(false);
}

/* wrap type node into TS_PTR type node */
void wrap_type_pointer(node_t *pn)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_TYPE, pn->pwsid, pn->startpos);
  nd.ts = TS_PTR;
  ndswap(pn, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
}

/* wrap type node and expr node into TS_ARRAY type node */
void wrap_type_array(node_t *pn, node_t *pi)
{
  node_t nd; ndinit(&nd);
  ndset(&nd, NT_TYPE, pn->pwsid, pn->startpos);
  nd.ts = TS_ARRAY;
  ndswap(pn, ndnewbk(&nd));
  ndswap(pi, ndnewbk(&nd));
  ndswap(pn, &nd);
  ndfini(&nd);
}

/* wrap type node and vec of type nodes into TS_FUNCTION type node */
void wrap_type_function(node_t *pn, ndbuf_t *pnb)
{
  size_t i; node_t nd; ndinit(&nd);
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
}

static void parse_primary_declarator(pws_t *pw, node_t *pn)
{
  switch (peekt(pw)) {
    case TT_IDENTIFIER: {
      ndset(pn, NT_IDENTIFIER, pw->id, peekpos(pw)); 
      pn->name = getid(pw);
    } break;
    case TT_COMMA: case TT_LBRK: case TT_RPAR: {
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
        node_t nd; ndinit(&nd);
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
          node_t *pdn = ndbnewbk(&ndb), *pti;
          ndset(pdn, NT_VARDECL, pw->id, peekpos(pw));
          pti = ndnewbk(pdn);
          parse_base_type(pw, pti);
          pdn->name = parse_declarator(pw, pti); /* 0 if id is missing */
          if (peekt(pw) != TT_COMMA) break;
          dropt(pw);
          if (peekt(pw) == TT_ELLIPSIS) {
            pdn = ndbnewbk(&ndb);
            ndset(pdn, NT_VARDECL, pw->id, peekpos(pw));
            pti = ndnewbk(pdn);
            ndset(pti, NT_TYPE, pw->id, peekpos(pw));
            pti->ts = TS_ETC;  
            dropt(pw);
            break;
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
  retry: switch (peekt(pw)) {
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
          node_t nd; ndinit(&nd);
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
  sym_t id; node_t nd; ndinit(&nd);
  parse_unary_declarator(pw, &nd);
  /* fixme: invert nd using ptn as a 'seed' type */
  id = invert_declarator(&nd, ptn);
  ndfini(&nd); 
  return id; // ? id : intern("?");
}

static void parse_initializer(pws_t *pw, node_t *pn)
{
  if (peekt(pw) == TT_LBRC) {
    ndset(pn, NT_DISPLAY, pw->id, peekpos(pw));
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
  /* declarator (= expr)? */
  node_t *pn = ndbnewbk(pnb), *pni, tn; sym_t id;
  ndicpy(&tn, ptn);
  ndset(pn, NT_VARDECL, pw->id, pw->pos);
  pn->name = id = parse_declarator(pw, &tn);
  if (id == 0) reprintf(pw, pw->pos, "declared identifier is missing");
  pn->name = id; pn->sc = sc;
  ndswap(&tn, ndnewbk(pn)); 
  if (peekt(pw) != TT_ASN) return;
  dropt(pw);
  pn = ndbnewbk(pnb);
  ndset(pn, NT_ASSIGN, pw->id, pw->pos);
  pn->op = TT_ASN;
  pni = ndnewbk(pn);
  ndset(pni, NT_IDENTIFIER, pw->id, pw->pos); 
  pni->name = id;
  parse_initializer(pw, ndnewbk(pn)); 
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
    node_t tnd; ndinit(&tnd); 
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
  if (ctk == TT_CASE_KW || ctk == NT_DEFAULT) {
    ndset(pn, ctk == TT_CASE_KW ? NT_CASE : NT_DEFAULT, pw->id, pw->pos);
    dropt(pw);
    if (ctk == TT_CASE_KW) {
      parse_expr(pw, ndnewbk(pn));
      expect(pw, TT_COLON, ":");
    }
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
      while (storage_class_specifier_ahead(pw) || type_specifier_ahead(pw)) {
        sc_t sc = parse_decl(pw, &pn->body);
        if (sc == SC_EXTERN || sc == SC_STATIC)
          reprintf(pw, pw->pos, "block-level extern/static declarations are not supported"); 
        /* auto and register are accepted and ignored */
        expect(pw, TT_SEMICOLON, ";"); /* no block-level function definitions */
      }
      while (peekt(pw) != TT_RBRC) {
        if (peekt(pw) == TT_TYPEDEF_KW)
          reprintf(pw, pw->pos, "block-level typedef declarations are not supported");  
        parse_stmt(pw, ndnewbk(pn)); 
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
      if (peekt(pw) != TT_SEMICOLON) parse_expr(pw, ndnewbk(pn));
      else ndset(ndnewbk(pn), NT_NULL, pw->id, pw->pos); 
      expect(pw, TT_SEMICOLON, ";");
      if (peekt(pw) != TT_SEMICOLON) parse_expr(pw, ndnewbk(pn));
      else ndset(ndnewbk(pn), NT_NULL, pw->id, pw->pos); 
      expect(pw, TT_SEMICOLON, ";");
      if (peekt(pw) != TT_RPAR) parse_expr(pw, ndnewbk(pn));
      else ndset(ndnewbk(pn), NT_NULL, pw->id, pw->pos); 
      expect(pw, TT_RPAR, ")");
      parse_stmt(pw, ndnewbk(pn));
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
      reprintf(pw, pw->pos, "goto statements are not supported"); 
    } break;
    default: {
      parse_expr(pw, pn);
      if (peekt(pw) == TT_COLON) reprintf(pw, pw->pos, "labels are not supported"); 
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
        node_t nd; ndinit(&nd);
        ndswap(ndref(pn, 0), &nd);
        nd.nt = NT_FUNDEF;
        parse_stmt(pw, ndnewbk(&nd));
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
        fprintf(stderr, "# repeated #include of '%s' ignored\n", pw->infile);
        return false;
      }
    } else if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "module")) {
      dropt(pw);
      if (peekt(pw) == TT_STRING) {
        sym_t name = intern(pw->tokstr);
        dropt(pw);
        fprintf(stderr, "# module name set to '%s'\n", symname(name));
        pw->curmod = name;
      } else reprintf(pw, peekpos(pw), "missing module name string after #pragma module");
    }
  }
  return true; 
}

static void parse_define_directive(pws_t *pw, int startpos)
{
  if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "define")) {
    int info = ndblen(&g_nodes); node_t *pn = ndbnewbk(&g_nodes);
    dropt(pw);
    ndset(pn, NT_MACRODEF, pw->id, startpos);
    if (peekt(pw) == TT_MACRO_NAME)
      reprintf(pw, peekpos(pw), "macro already defined; use #undef before redefinition");
    pn->name = getid(pw);
    /* do manual char-level lookahead */
    if (chbdata(&pw->chars)[pw->curi] == '(') {
      /* macro with parameters */
      node_t *ptn = ndnewbk(pn);
      ndset(ptn, NT_TYPE, pw->id, peekpos(pw));
      ptn->ts = TS_FUNCTION;
      ndnewbk(ptn); /* return value type is not set */
      expect(pw, TT_LPAR, "(");
      while (peekt(pw) != TT_RPAR) {
        node_t *pti = ndnewbk(ptn);
        ndset(pti, NT_VARDECL, pw->id, peekpos(pw));
        pti->name = getid(pw);
        if (peekt(pw) == TT_COMMA) dropt(pw);
        else break;
      }
      expect(pw, TT_RPAR, ")");
    }  
    /* righ-hand-side expression or block */    
    if (peekt(pw) == TT_LBRC) parse_stmt(pw, ndnewbk(pn));
    else parse_primary_expr(pw, ndnewbk(pn));
    /* save macro definition for future use */
    if (lookup_symbol(symname(pn->name), NULL) != TT_IDENTIFIER) {
      reprintf(pw, startpos, "macro name is already in use");
    } else { 
      intern_symbol(symname(pn->name), TT_MACRO_NAME, info);
    }
  } else if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "undef")) {  
    dropt(pw);
    if (peekt(pw) != TT_MACRO_NAME) {
      reprintf(pw, peekpos(pw), "undef name is not a macro");
    } else {
      /* definition will still remain in g_nodes, but name won't */ 
      unintern_symbol(pw->tokstr);
      dropt(pw);
    }
  }
}

/* parse single top-level declaration/definition */
bool parse_top_form(pws_t *pw, node_t *pn)
{
  ndclear(pn);
  if (peekt(pw) != TT_EOF) {
    if (peekt(pw) == TT_HASH) {
      int startpos = peekpos(pw);
      dropt(pw);
      if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "pragma")) {
        bool cont = parse_pragma_directive(pw, startpos);
        if (!cont) return false; /* bail out on '#pragma once' repeat */
        ndset(pn, NT_BLOCK, pw->id, startpos);
      } else if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "define")) {
        parse_define_directive(pw, startpos);
        ndset(pn, NT_BLOCK, pw->id, startpos);
      } else if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "undef")) {
        parse_define_directive(pw, startpos);
        ndset(pn, NT_BLOCK, pw->id, startpos);
      } else if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "include")) { 
        dropt(pw);
        ndset(pn, NT_INCLUDE, pw->id, startpos);
        expect(pw, TT_LT, "<");
        pn->name = getid(pw);
        if (peekt(pw) == TT_DOT) {
          dropt(pw);
          if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "h")) dropt(pw);
          else if (peekt(pw) == TT_IDENTIFIER && streql(pw->tokstr, "wh")) dropt(pw);
          else reprintf(pw, pw->pos, "invalid module name");
        }
        expect(pw, TT_GT, ">");
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
    if (!parse_top_form(pw, pn)) {
      ndbrem(pnb, ndblen(pnb)-1);
      return;
    } 
  } 
}

/* report node error, possibly printing location information, and exit */
void neprintf(node_t *pn, const char *fmt, ...)
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

void n2eprintf(node_t *pn, node_t *pn2, const char *fmt, ...)
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
  if (pn2) neprintf(pn2, "(original definition)");
  else exit(1);
}



/* dump nodes in s-expression format */

static void fdumpss(const char *str, FILE* fp)
{
  chbuf_t cb = mkchb();
  chbputc('\"', &cb);
  while (*str) {
    int c = *str & 0xFF;
    unsigned long uc;
    /* check for control chars manually */
    if (c <= 0x1F) {
      switch (c) {
        case '\a' : chbputs("\\a", &cb); break;
        case '\b' : chbputs("\\b", &cb); break;
        case '\t' : chbputs("\\t", &cb); break;
        case '\n' : chbputs("\\n", &cb); break;
        case '\r' : chbputs("\\r", &cb); break;
        default   : {
          char buf[10]; sprintf(buf, "\\x%.2X;", c);
          chbputs(buf, &cb);
        } break;
      }
      ++str;
      continue;
    } else if (c == '\"' || c == '\\') {
      switch (c) {
        case '\"' : chbputs("\\\"", &cb); break;
        case '\\' : chbputs("\\\\", &cb); break;
      }
      ++str;
      continue;
    }
    /* get next unicode char */
    uc = unutf8((unsigned char **)&str);
    if (uc <= 0x1F) {
      /* should have been taken care of above! */
      assert(false);
    } else if (uc <= 0x80) {
      /* put ascii chars as-is */
      assert(uc == c);
      chbputc(c, &cb);
    } else {
      chbputlc(uc, &cb);
    }
  }
  chbputc('\"', &cb);
  fputs(chbdata(&cb), fp);
  chbfini(&cb);
}

static char *ts_name(ts_t ts)
{
  char *s = "?";
  switch (ts) {
    case TS_VOID: s = "void"; break;
    case TS_ETC: s = "etc"; break;  
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
    case TS_STRUCT: s = "struct"; break; 
    case TS_UNION: s = "union"; break; 
    case TS_PTR: s = "ptr"; break; 
    case TS_ARRAY: s = "array"; break; 
    case TS_FUNCTION: s = "function"; break; 
    default: assert(false);
  }
  return s;
}

static char *sc_name(sc_t sc)
{
  char *s = "?";
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

static char *op_name(tt_t op)
{
  char *s = "?";
  switch (op) {
    case TT_PLUS: s = "+"; break; 
    case TT_MINUS: s = "-"; break; 
    case TT_AND_AND: s = "&&"; break; 
    case TT_OR_OR: s = "||"; break; 
    case TT_NOT: s = "not"; break; 
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
      fprintf(fp, "(literal %s ", ts_name(pn->ts));
      fdumpss(chbdata(&pn->data), fp); /* fixme: stops at \0 */
    } break;
    case NT_IDENTIFIER: {
      fputs(symname(pn->name), fp);
      goto out;
    } break;
    case NT_SUBSCRIPT: {
      fprintf(fp, "(subscript");
    } break;
    case NT_CALL: {
      fprintf(fp, "(call");
    } break;
    case NT_INTRCALL: {
      fprintf(fp, "(intrcall %s", symname(pn->name));
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
    case NT_BLOCK: {
      if (!pn->name) fprintf(fp, "(block");
      else fprintf(fp, "(block %s", symname(pn->name));
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
    case NT_RETURN: {
      fprintf(fp, "(return");
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
      char *s = sc_name(pn->sc);
      if (!pn->name) fprintf(fp, "(vardecl %s", s);
      else fprintf(fp, "(vardecl %s %s", symname(pn->name), s);
    } break;
    case NT_DISPLAY: {
      fprintf(fp, "(display");
    } break;
    case NT_FUNDEF: {
      char *s = sc_name(pn->sc);
      fprintf(fp, "(fundef %s %s", symname(pn->name), s);
    } break;
    case NT_TYPEDEF: {
      fprintf(fp, "(typedef %s", symname(pn->name));
    } break;
    case NT_MACRODEF: {
      fprintf(fp, "(macrodef %s", symname(pn->name));
    } break;
    case NT_INCLUDE: {
      fprintf(fp, "(include %s", symname(pn->name));
    } break;
    case NT_IMPORT: {
      char *s = sc_name(pn->sc);
      if (!pn->name) fprintf(fp, "(import %s", s);
      else fprintf(fp, "(import %s %s", symname(pn->name), s);
    } break;
    default: assert(false);
  }
  if (ndlen(pn) > 0) {
    for (i = 0; i < ndlen(pn); i += 1) {
      fputs("\n", fp);
      dump(ndref(pn, i), fp, indent+2);
    }
    fputs(")", fp);
  } else {
    fputs(")", fp);
  }
  out: if (!indent) fputs("\n", fp);
}


/* dump node in s-expression format */
void dump_node(const node_t *pn, FILE *fp)
{
  dump((node_t*)pn, fp, 0);
}
