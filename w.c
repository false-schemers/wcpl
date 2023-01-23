/* w.c (wasm interface) -- esl */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <wchar.h>
#include <ctype.h>
#include <math.h>
#include "l.h"
#include "w.h"
#include "p.h"
#include "c.h"

/* wasm binary encoding globals */
static FILE *g_wasmout = NULL; /* current binary output stream */
static chbuf_t *g_curbuf = NULL; /* current destination or NULL */
static chbuf_t *g_sectbuf = NULL; /* current section or NULL */
static chbuf_t *g_ssecbuf = NULL; /* current subsection or NULL */
static chbuf_t *g_codebuf = NULL; /* current code or NULL */
static unsigned g_sectcnt = 0; /* section element count */

/* wat text encoding globals */
static FILE *g_watout = NULL; /* current text output stream */
static chbuf_t *g_watbuf = NULL; /* work buffer for wat dump */
static int g_watindent = 0; /* cur line indent for wat dump */
static bool g_watexec = false; /* wat executable is being parsed */


/* binary encoding */

/* forwards */
static void wasm_unsigned(unsigned long long val);
static void wasm_byte(unsigned b);

static void wasm_header(void)
{
  fputc(0x00, g_wasmout); fputc(0x61, g_wasmout); fputc(0x73, g_wasmout); fputc(0x6D, g_wasmout);
  fputc(0x01, g_wasmout); fputc(0x00, g_wasmout); fputc(0x00, g_wasmout); fputc(0x00, g_wasmout);
}

static void wasm_section_start(secid_t si)
{
  assert(g_sectbuf == NULL);
  fputc((int)si & 0xFF, g_wasmout);
  g_sectbuf = newchb(); /* buffer emits */
  g_curbuf = g_sectbuf; /* emits go here */
  g_sectcnt = 0; /* incremented as elements are added */
}

static void wasm_section_bumpc(void)
{
  assert(g_sectbuf != NULL && g_sectbuf == g_curbuf);
  ++g_sectcnt;
}

static void wasm_section_end(void)
{
  chbuf_t *psb = g_sectbuf;
  assert(psb != NULL && psb == g_curbuf);
  g_sectbuf = g_curbuf = NULL; /* emit directly from now on */
  if (g_sectcnt > 0 || chblen(psb) == 0) {
    /* element count needs to be inserted */
    chbuf_t cb; chbinit(&cb);
    g_curbuf = &cb; wasm_unsigned(g_sectcnt);
    g_curbuf = NULL;
    wasm_unsigned(chblen(&cb) + chblen(psb));
    fwrite(chbdata(&cb), 1, chblen(&cb), g_wasmout);
    fwrite(chbdata(psb), 1, chblen(psb), g_wasmout);
    chbfini(&cb); 
  } else {
    /* element count is in psb */
    wasm_unsigned(chblen(psb));
    fwrite(chbdata(psb), 1, chblen(psb), g_wasmout);
  }
  freechb(psb);
}

static void wasm_subsection_start(unsigned ssi)
{
  assert(g_ssecbuf == NULL);
  assert(g_sectbuf != NULL && g_sectbuf == g_curbuf);
  wasm_byte(ssi);
  g_ssecbuf = newchb(); /* buffer emits */
  g_curbuf = g_ssecbuf; /* emits go here */
}

static void wasm_subsection_end(void)
{
  chbuf_t *psb = g_ssecbuf;
  assert(psb != NULL && psb == g_curbuf);
  assert(g_sectbuf != NULL);
  g_ssecbuf = NULL, g_curbuf = g_sectbuf; /* emit to section from now on */
  wasm_unsigned(chblen(psb));
  chbput(chbdata(psb), chblen(psb), g_curbuf);
  freechb(psb);
}

static void wasm_code_start(void)
{
  assert(g_sectbuf != NULL && g_sectbuf == g_curbuf);
  assert(g_codebuf == NULL);
  g_codebuf = newchb(); /* buffer emits */
  g_curbuf = g_codebuf; /* emits go here */
}

static void wasm_code_end(void)
{
  chbuf_t *psb = g_codebuf;
  assert(psb != NULL && psb == g_curbuf);
  assert(g_sectbuf != NULL);
  g_curbuf = g_sectbuf; /* emit to section from now on */
  g_codebuf = NULL;
  wasm_unsigned(chblen(psb));
  chbput(chbdata(psb), chblen(psb), g_curbuf);
  freechb(psb);
}

static void wasm_byte(unsigned b)
{
  if (g_curbuf) chbputc(b & 0xFF, g_curbuf);
  else fputc((int)b & 0xFF, g_wasmout);
}

static void wasm_data(chbuf_t *pcb)
{
  char *s = pcb->buf; size_t n = pcb->fill;
  while (n--) wasm_byte(*s++);
}

static void wasm_signed(long long val)
{
  int more;
  do {
    unsigned b = (unsigned)(val & 0x7f);
    val >>= 7;
    more = !((val == 0 && (b & 0x40) == 0) || (val == -1 && (b & 0x40) != 0));
    wasm_byte(b | (more ? 0x80 : 0));
  } while (more);
}

static void wasm_unsigned(unsigned long long val)
{
  do {
    unsigned b = (unsigned)(val & 0x7f);
    val >>= 7;
    if (val != 0) b |= 0x80;
    wasm_byte(b);
  } while (val != 0);
}

static void wasm_float(float val)
{
  union { float v; unsigned char bb[4]; } u; int i;
  assert(sizeof(float) == 4);
  u.v = val; 
  for (i = 0; i < 4; ++i) wasm_byte(u.bb[i]);
}

static void wasm_double(double val)
{
  union { double v; unsigned char bb[8]; } u; int i;
  assert(sizeof(double) == 8);
  u.v = val; 
  for (i = 0; i < 8; ++i) wasm_byte(u.bb[i]);
}

static void wasm_name(const char *name)
{
  size_t n; assert(name);
  n = strlen(name);
  wasm_unsigned(n);
  while (n--) wasm_byte(*name++);
}

static void wasm_in(instr_t in)
{
  if (((int)in & 0xff) == (int)in) { /* 1-byte */
    wasm_byte((unsigned)in & 0xff);
  } else if (((int)in & 0xffff) == (int)in) { /* 2-byte */
    wasm_byte(((unsigned)in >> 8) & 0xff);
    wasm_byte(((unsigned)in) & 0xff);
  } else if (((int)in & 0xffffff) == (int)in) { /* 3-byte */
    wasm_byte(((unsigned)in >> 16) & 0xff);
    wasm_byte(((unsigned)in >> 8) & 0xff);
    wasm_byte(((unsigned)in) & 0xff);
  } else assert(false);
}


/* mid-level representations */

funcsig_t* fsinit(funcsig_t* pf)
{
  memset(pf, 0, sizeof(funcsig_t));
  bufinit(&pf->partypes, sizeof(valtype_t));
  bufinit(&pf->restypes, sizeof(valtype_t));
  return pf;
}

void fsfini(funcsig_t* pf)
{
  buffini(&pf->partypes);
  buffini(&pf->restypes);
}

void fscpy(funcsig_t* pdf, const funcsig_t* psf)
{
  bufcpy(&pdf->partypes, &psf->partypes);
  bufcpy(&pdf->restypes, &psf->restypes);
}

void fsbfini(fsbuf_t* pb)
{
  funcsig_t *pft; size_t i;
  assert(pb); assert(pb->esz == sizeof(funcsig_t));
  pft = (funcsig_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) fsfini(pft+i);
  buffini(pb);
}

static bool sameft(funcsig_t* pf1, funcsig_t* pf2)
{
  size_t i; /* todo: add hash to speed up comparison */
  if (vtblen(&pf1->partypes) != vtblen(&pf2->partypes)) return false;
  if (vtblen(&pf1->restypes) != vtblen(&pf2->restypes)) return false;
  for (i = 0; i < vtblen(&pf1->partypes); ++i)
    if (*vtbref(&pf1->partypes, i) != *vtbref(&pf2->partypes, i)) return false;
  for (i = 0; i < vtblen(&pf1->restypes); ++i)
    if (*vtbref(&pf1->restypes, i) != *vtbref(&pf2->restypes, i)) return false;
  return true;
}

unsigned fsintern(fsbuf_t* pb, const funcsig_t *pfs)
{
  size_t i;
  for (i = 0; i < fsblen(pb); ++i) if (sameft(fsbref(pb, i), (funcsig_t*)pfs)) break;
  if (i == fsblen(pb)) fscpy(fsbnewbk(pb), pfs);
  return (unsigned)i;
}

unsigned funcsig(fsbuf_t* pb, size_t argc, size_t retc, ...)
{
  size_t i; va_list args; funcsig_t ft;
  fsinit(&ft);
  va_start(args, retc);
  for (i = 0; i < argc; ++i) *vtbnewbk(&ft.partypes) = va_arg(args, valtype_t);
  for (i = 0; i < retc; ++i) *vtbnewbk(&ft.restypes) = va_arg(args, valtype_t);
  va_end(args);
  for (i = 0; i < fsblen(pb); ++i) if (sameft(fsbref(pb, i), &ft)) break;
  if (i == fsblen(pb)) memswap(&ft, fsbnewbk(pb), sizeof(funcsig_t));
  fsfini(&ft);
  return (unsigned)i;
}


entry_t* entinit(entry_t* pe, entkind_t ek)
{
  memset(pe, 0, sizeof(entry_t));
  pe->ek = ek;
  bufinit(&pe->loctypes, sizeof(valtype_t));
  icbinit(&pe->code);
  return pe;
}

void entfini(entry_t* pe)
{
  buffini(&pe->loctypes);
  icbfini(&pe->code);
}

void entbfini(entbuf_t* pb)
{
  entry_t *pe; size_t i;
  assert(pb); assert(pb->esz == sizeof(entry_t));
  pe = (entry_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) entfini(pe+i);
  buffini(pb);
}

dseg_t* dseginit(dseg_t* ps, dsmode_t dsm)
{
  memset(ps, 0, sizeof(dseg_t));
  ps->dsm = dsm;
  chbinit(&ps->data);
  icbinit(&ps->code);
  return ps;
}

void dsegfini(dseg_t* ps)
{
  chbfini(&ps->data);
  icbfini(&ps->code);
}

void dsegbfini(dsegbuf_t* pb)
{
  dseg_t *pds; size_t i;
  assert(pb); assert(pb->esz == sizeof(dseg_t));
  pds = (dseg_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) dsegfini(pds+i);
  buffini(pb);
}

eseg_t* eseginit(eseg_t* ps, esmode_t esm)
{
  memset(ps, 0, sizeof(eseg_t));
  ps->esm = esm;
  bufinit(&ps->fidxs, sizeof(unsigned));
  icbinit(&ps->code);
  icbinit(&ps->codes);
  return ps;
}

void esegfini(eseg_t* ps)
{
  buffini(&ps->fidxs);
  icbfini(&ps->code);
  icbfini(&ps->codes);
}

void esegbfini(esegbuf_t* pb)
{
  eseg_t *pes; size_t i;
  assert(pb); assert(pb->esz == sizeof(eseg_t));
  pes = (eseg_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) esegfini(pes+i);
  buffini(pb);
}


/* mid-level emits */

static void wasm_expr(icbuf_t *pcb)
{
  size_t i;
  for (i = 0; i < icblen(pcb); ++i) {
    inscode_t *pic = icbref(pcb, i);
    assert(!pic->id);
    wasm_in(pic->in);
    switch (instr_sig(pic->in)) {
      case INSIG_NONE:
        switch (pic->in) {
          case IN_MEMORY_SIZE: case IN_MEMORY_GROW: case IN_MEMORY_FILL:
            wasm_unsigned(0); /* one reserved arg */
            break;
          case IN_MEMORY_COPY:
            wasm_unsigned(0); wasm_unsigned(0); /* two reserved args */
            break;
          default:;
        }
        break;
      case INSIG_BT:  case INSIG_L:   
      case INSIG_XL:  case INSIG_XG:
      case INSIG_T:   case INSIG_RF:
        wasm_unsigned(pic->arg.u); 
        break;
      case INSIG_XT:
        wasm_unsigned(pic->arg.u); 
        switch (pic->in) {
          case IN_MEMORY_INIT:
            wasm_unsigned(0); /* one reserved arg */
            break;
          default:;
        }
        break;
      case INSIG_I32: case INSIG_I64:
        wasm_signed(pic->arg.i); 
        break;
      case INSIG_MEMARG:
        wasm_unsigned(pic->arg2.u); wasm_unsigned(pic->arg.u);
        break;
      case INSIG_X_Y: case INSIG_CI:
        wasm_unsigned(pic->arg.u); wasm_unsigned(pic->arg2.u); 
        break;
      case INSIG_F32:
        wasm_float(pic->arg.f); 
        break;
      case INSIG_F64:
        wasm_double(pic->arg.d); 
        break;
      case INSIG_LS_L: { 
        /* takes multiple opcodes */
        size_t d = i + (unsigned)pic->arg.u + 1;
        wasm_unsigned(pic->arg.u);
        for (; i < d; ++i) {
          assert(i+1 < icblen(pcb));
          pic = icbref(pcb, i+1); assert(pic->in == IN_BR);
          wasm_unsigned(pic->arg.u);
        }
      } break;
      default:
        assert(false);     
    }
  }    
}

static void wasm_types(fsbuf_t *pftb)
{
  size_t i, k;
  if (!pftb) return;
  wasm_section_start(SI_TYPE);
  /* no type compression: types may repeat */
  for (i = 0; i < fsblen(pftb); ++i) {
    funcsig_t *pft = fsbref(pftb, i);
    wasm_byte(FT_FUNCTYPE);
    wasm_unsigned(vtblen(&pft->partypes));
    for (k = 0; k < vtblen(&pft->partypes); ++k) {
      wasm_byte(*vtbref(&pft->partypes, k));
    }    
    wasm_unsigned(vtblen(&pft->restypes));
    for (k = 0; k < vtblen(&pft->restypes); ++k) {
      wasm_byte(*vtbref(&pft->restypes, k));
    }
    wasm_section_bumpc();
  }
  wasm_section_end();
}

static void wasm_imports(entbuf_t *pfdb, entbuf_t *ptdb, entbuf_t *pmdb, entbuf_t *pgdb)
{
  size_t i;
  if (!pfdb && !ptdb && !pmdb && !pgdb) return;
  wasm_section_start(SI_IMPORT);
  /* all imported functions are at the front of pfdb */
  if (pfdb) for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (!pe->imported) continue; /* skip non-imported */
    assert(pe->name);
    wasm_name(symname(pe->mod));
    wasm_name(symname(pe->name));
    wasm_byte(EK_FUNC);
    wasm_unsigned(pe->fsi);
    wasm_section_bumpc(); 
  }
  /* imported tables */
  if (ptdb) for (i = 0; i < entblen(ptdb); ++i) {
    entry_t *pe = entbref(ptdb, i);
    assert(pe->ek == EK_TABLE);
    if (!pe->imported) continue; /* skip non-imported */
    assert(pe->name);
    wasm_name(symname(pe->mod));
    wasm_name(symname(pe->name));
    wasm_byte(EK_TABLE);
    wasm_byte(pe->vt); /* RT_FUNCREF/RT_EXTERNREF */        
    wasm_byte(pe->lt);
    wasm_unsigned(pe->n);
    if (pe->lt == LT_MINMAX) wasm_unsigned(pe->m);
    wasm_section_bumpc(); 
  }    
  /* imported mems */
  if (pmdb) for (i = 0; i < entblen(pmdb); ++i) {
    entry_t *pe = entbref(pmdb, i);
    assert(pe->ek == EK_MEM);
    if (!pe->imported) continue; /* skip non-imported */
    assert(pe->name);
    wasm_name(symname(pe->mod));
    wasm_name(symname(pe->name));
    wasm_byte(EK_MEM);
    wasm_byte(pe->lt);
    wasm_unsigned(pe->n);
    if (pe->lt == LT_MINMAX) wasm_unsigned(pe->m);
    wasm_section_bumpc(); 
  }    
  /* imported globals */
  if (pgdb) for (i = 0; i < entblen(pgdb); ++i) {
    entry_t *pe = entbref(pgdb, i);
    assert(pe->ek == EK_GLOBAL);
    if (!pe->imported) continue; /* skip non-imported */
    assert(pe->name);
    wasm_name(symname(pe->mod));
    wasm_name(symname(pe->name));
    wasm_byte(EK_GLOBAL);
    wasm_byte(pe->vt);
    wasm_byte(pe->mut);
    wasm_section_bumpc(); 
  }
  wasm_section_end();
}

static void wasm_funcs(entbuf_t *pfdb)
{
  size_t i;
  if (!pfdb) return;
  wasm_section_start(SI_FUNCTION);
  for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (pe->imported) continue;
    wasm_unsigned(pe->fsi);
    wasm_section_bumpc();
  }
  wasm_section_end();
}

static void wasm_tables(entbuf_t *ptdb)
{
  size_t i;
  if (!ptdb) return;
  wasm_section_start(SI_TABLE);
  for (i = 0; i < entblen(ptdb); ++i) {
    entry_t *pe = entbref(ptdb, i);
    assert(pe->ek == EK_TABLE);
    if (pe->imported) continue;
    wasm_byte(pe->vt);
    wasm_byte(pe->lt);
    wasm_unsigned(pe->n);
    if (pe->lt == LT_MINMAX) wasm_unsigned(pe->m);
    wasm_section_bumpc();
  }
  wasm_section_end();
}

static void wasm_mems(entbuf_t *pmdb)
{
  size_t i;
  if (!pmdb) return;
  wasm_section_start(SI_MEMORY);
  for (i = 0; i < entblen(pmdb); ++i) {
    entry_t *pe = entbref(pmdb, i);
    assert(pe->ek == EK_MEM);
    if (pe->imported) continue;
    wasm_byte(pe->lt);
    wasm_unsigned(pe->n);
    if (pe->lt == LT_MINMAX) wasm_unsigned(pe->m);
    wasm_section_bumpc();
  }
  wasm_section_end();
}

static void wasm_globals(entbuf_t *pgdb)
{
  size_t i;
  if (!pgdb) return;
  wasm_section_start(SI_GLOBAL);
  for (i = 0; i < entblen(pgdb); ++i) {
    entry_t *pe = entbref(pgdb, i);
    assert(pe->ek == EK_GLOBAL);
    if (pe->imported) continue;
    wasm_byte(pe->vt);
    wasm_byte(pe->mut);
    wasm_expr(&pe->code);
    wasm_section_bumpc();
  }
  wasm_section_end();
}

static void wasm_exports(entbuf_t *pfdb, entbuf_t *ptdb, entbuf_t *pmdb, entbuf_t *pgdb)
{
  size_t i;
  if (!pfdb && !ptdb && !pmdb && !pgdb) return;
  wasm_section_start(SI_EXPORT);
  if (ptdb) for (i = 0; i < entblen(ptdb); ++i) {
    entry_t *pe = entbref(ptdb, i);
    assert(pe->ek == EK_TABLE);
    if (pe->exported && pe->name) {
      wasm_name(symname(pe->name));
      wasm_byte(EK_TABLE);
      wasm_unsigned(i);
      wasm_section_bumpc();
    }
  }  
  if (pmdb) for (i = 0; i < entblen(pmdb); ++i) {
    entry_t *pe = entbref(pmdb, i);
    assert(pe->ek == EK_MEM);
    if (pe->exported && pe->name) {
      wasm_name(symname(pe->name));
      wasm_byte(EK_MEM);
      wasm_unsigned(i);
      wasm_section_bumpc();
    }
  }  
  if (pgdb) for (i = 0; i < entblen(pgdb); ++i) {
    entry_t *pe = entbref(pgdb, i);
    assert(pe->ek == EK_GLOBAL);
    if (pe->exported && pe->name) {
      wasm_name(symname(pe->name));
      wasm_byte(EK_GLOBAL);
      wasm_unsigned(i);
      wasm_section_bumpc();
    }
  }
  if (pfdb) for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (pe->exported && pe->name) {
      wasm_name(symname(pe->name));
      wasm_byte(EK_FUNC);
      wasm_unsigned(i);
      wasm_section_bumpc();
    }
  }  
  wasm_section_end();
}

static void wasm_start(entbuf_t *pfdb)
{
  size_t i;
  if (!pfdb) return;
  for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (pe->isstart) {
      wasm_section_start(SI_START);
      wasm_unsigned((unsigned)i);
      wasm_section_end();
      break;
    }
  }
}

static void wasm_elems(esegbuf_t *pesb)
{
  size_t i, k;
  if (!pesb) return;
  wasm_section_start(SI_ELEMENT);
  for (i = 0; i < esegblen(pesb); ++i) {
    eseg_t *ps = esegbref(pesb, i);
    wasm_byte(ps->esm);
    switch (ps->esm) {
       case ES_ACTIVE_EIV: /* e fiv */
         wasm_expr(&ps->code);
         wasm_unsigned(idxblen(&ps->fidxs));
         for (k = 0; k < idxblen(&ps->fidxs); ++k)
           wasm_unsigned(*idxbref(&ps->fidxs, k));
         break;       
       case ES_ACTIVE_TEKIV: /* ti e k fiv */
         wasm_unsigned(ps->idx);
         wasm_expr(&ps->code);
         /* fall thru */
       case ES_DECLVE_KIV: /* k fiv */
       case ES_PASSIVE_KIV: /* k fiv */ 
         wasm_byte(ps->ek); /* must be 0 */
         wasm_unsigned(idxblen(&ps->fidxs));
         for (k = 0; k < idxblen(&ps->fidxs); ++k)
           wasm_unsigned(*idxbref(&ps->fidxs, k));
         break;       
       case ES_ACTIVE_EEV: /* e ev */
         wasm_expr(&ps->code);
         wasm_unsigned(ps->cdc);
         wasm_expr(&ps->codes);
         break;
       case ES_ACTIVE_IETEV: /* ti e rt ev */       
         wasm_unsigned(ps->idx);
         wasm_expr(&ps->code);
         /* fall thru */
       case ES_DECLVE_TEV: /* rt ev */
       case ES_PASSIVE_TEV: /* rt ev */
         wasm_byte(ps->rt); /* must be RT_XXX */       
         wasm_unsigned(ps->cdc);
         wasm_expr(&ps->codes);
         break;
      default: assert(false);
    }  
    wasm_section_bumpc();
  }
  wasm_section_end();
}

static void wasm_datacount(dsegbuf_t *pdsb)
{
  size_t i;
  if (!pdsb) return;
  wasm_section_start(SI_DATACOUNT);
  for (i = 0; i < dsegblen(pdsb); ++i) {
    wasm_section_bumpc();
  }  
  wasm_section_end();
}

static void wasm_codes(entbuf_t *pfdb)
{
  size_t i;
  if (!pfdb) return;
  wasm_section_start(SI_CODE);
  for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (pe->imported) continue;
    wasm_code_start();
    if (buflen(&pe->loctypes) > 0) {
      buf_t tcb = mkbuf(sizeof(unsigned)); size_t k, f;
      for (k = f = 0; k < buflen(&pe->loctypes); ++k) {
        unsigned t = (unsigned)*vtbref(&pe->loctypes, k);
        if (f > 0 && *(unsigned*)bufref(&tcb, 2*f-1) == t) {
          *(unsigned*)bufref(&tcb, 2*f-2) += 1;
        } else {
          *(unsigned*)bufnewbk(&tcb) = 1;
          *(unsigned*)bufnewbk(&tcb) = t;
          ++f;
        }  
      }
      wasm_unsigned(f);
      for (k = 0; k < f; ++k) {
        wasm_unsigned(*(unsigned*)bufref(&tcb, 2*k));
        wasm_byte(*(unsigned*)bufref(&tcb, 2*k+1));
      }  
      buffini(&tcb);
    } else {
      wasm_unsigned(0);
    }
    wasm_expr(&pe->code);
    wasm_code_end();
    wasm_section_bumpc();
  }
  wasm_section_end();
}

static void wasm_datas(dsegbuf_t *pdsb)
{
  size_t i;
  if (!pdsb) return;
  wasm_section_start(SI_DATA);
  for (i = 0; i < dsegblen(pdsb); ++i) {
    dseg_t *ps = dsegbref(pdsb, i);
    wasm_byte(ps->dsm);
    switch (ps->dsm) {
      case DS_ACTIVE_MI: wasm_unsigned(ps->idx); /* fall thru */
      case DS_ACTIVE: wasm_expr(&ps->code); /* fall thru */
      case DS_PASSIVE: wasm_unsigned(chblen(&ps->data)), wasm_data(&ps->data); break;
      default: assert(false);
    }  
    wasm_section_bumpc();
  }
  wasm_section_end();
}


/* top-level representations */

wasm_module_t* wasm_module_init(wasm_module_t* pm)
{
  memset(pm, 0, sizeof(wasm_module_t));
  fsbinit(&pm->funcsigs); 
  entbinit(&pm->funcdefs);
  entbinit(&pm->tabdefs);
  entbinit(&pm->memdefs);
  entbinit(&pm->globdefs);
  esegbinit(&pm->elemdefs);
  dsegbinit(&pm->datadefs);
  return pm;
}

void wasm_module_fini(wasm_module_t* pm)
{
  fsbfini(&pm->funcsigs);
  entbfini(&pm->funcdefs);
  entbfini(&pm->tabdefs);
  entbfini(&pm->memdefs);
  entbfini(&pm->globdefs);
  esegbfini(&pm->elemdefs);
  dsegbfini(&pm->datadefs);
}


/* write final wasm binary module */

void write_wasm_module(wasm_module_t* pm, FILE *pf)
{
  g_wasmout = pf;
  wasm_header();
  wasm_types(&pm->funcsigs);
  wasm_imports(&pm->funcdefs, &pm->tabdefs, &pm->memdefs, &pm->globdefs);
  wasm_funcs(&pm->funcdefs);
  wasm_tables(&pm->tabdefs);
  wasm_mems(&pm->memdefs);
  wasm_globals(&pm->globdefs);
  wasm_exports(&pm->funcdefs, &pm->tabdefs, &pm->memdefs, &pm->globdefs);
  wasm_start(&pm->funcdefs);
  wasm_elems(&pm->elemdefs);
  //wasm_datacount(&pm->datadefs);
  wasm_codes(&pm->funcdefs);
  wasm_datas(&pm->datadefs);
  g_wasmout = NULL;
}


/* introspection */

static const char *g_innames[256] = {
    "unreachable",           /* 0x00 */
    "nop",                   /* 0x01 */
    "block",                 /* 0x02 */
    "loop",                  /* 0x03 */
    "if",                    /* 0x04 */
    "else",                  /* 0x05 */
    NULL,                    /* 0x06 */
    NULL,                    /* 0x07 */
    NULL,                    /* 0x08 */
    NULL,                    /* 0x09 */
    NULL,                    /* 0x0A */
    "end",                   /* 0x0B */
    "br",                    /* 0x0C */
    "br_if",                 /* 0x0D */
    "br_table",              /* 0x0E */
    "return",                /* 0x0F */
    "call",                  /* 0x10 */
    "call_indirect",         /* 0x11 */
    "return_call",           /* 0x12 */ /* not in core-1 */
    "return_call_indirect",  /* 0x13 */ /* not in core-1 */
    NULL,                    /* 0x14 */
    NULL,                    /* 0x15 */
    NULL,                    /* 0x16 */
    NULL,                    /* 0x17 */
    NULL,                    /* 0x18 */
    NULL,                    /* 0x19 */
    "drop",                  /* 0x1A */
    "select",                /* 0x1B */
    "select_t",              /* 0x1C */ /* not in core-1 */
    NULL,                    /* 0x1D */
    NULL,                    /* 0x1E */
    NULL,                    /* 0x1F */
    "local.get",             /* 0x20 */
    "local.set",             /* 0x21 */
    "local.tee",             /* 0x22 */
    "global.get",            /* 0x23 */
    "global.set",            /* 0x24 */
    "table.get",             /* 0x25 */ /* not in core-1 */
    "table.set",             /* 0x26 */ /* not in core-1 */
    NULL,                    /* 0x27 */
    "i32.load",              /* 0x28 */
    "i64.load",              /* 0x29 */
    "f32.load",              /* 0x2A */
    "f64.load",              /* 0x2B */
    "i32.load8_s",           /* 0x2C */
    "i32.load8_u",           /* 0x2D */
    "i32.load16_s",          /* 0x2E */
    "i32.load16_u",          /* 0x2F */
    "i64.load8_s",           /* 0x30 */
    "i64.load8_u",           /* 0x31 */
    "i64.load16_s",          /* 0x32 */
    "i64.load16_u",          /* 0x33 */
    "i64.load32_s",          /* 0x34 */
    "i64.load32_u",          /* 0x35 */
    "i32.store",             /* 0x36 */
    "i64.store",             /* 0x37 */
    "f32.store",             /* 0x38 */
    "f64.store",             /* 0x39 */
    "i32.store8",            /* 0x3A */
    "i32.store16",           /* 0x3B */
    "i64.store8",            /* 0x3C */
    "i64.store16",           /* 0x3D */
    "i64.store32",           /* 0x3E */
    "memory.size",           /* 0x3F */
    "memory.grow",           /* 0x40 */
    "i32.const",             /* 0x41 */
    "i64.const",             /* 0x42 */
    "f32.const",             /* 0x43 */
    "f64.const",             /* 0x44 */
    "i32.eqz",               /* 0x45 */
    "i32.eq",                /* 0x46 */
    "i32.ne",                /* 0x47 */
    "i32.lt_s",              /* 0x48 */
    "i32.lt_u",              /* 0x49 */
    "i32.gt_s",              /* 0x4A */
    "i32.gt_u",              /* 0x4B */
    "i32.le_s",              /* 0x4C */
    "i32.le_u",              /* 0x4D */
    "i32.ge_s",              /* 0x4E */
    "i32.ge_u",              /* 0x4F */
    "i64.eqz",               /* 0x50 */
    "i64.eq",                /* 0x51 */
    "i64.ne",                /* 0x52 */
    "i64.lt_s",              /* 0x53 */
    "i64.lt_u",              /* 0x54 */
    "i64.gt_s",              /* 0x55 */
    "i64.gt_u",              /* 0x56 */
    "i64.le_s",              /* 0x57 */
    "i64.le_u",              /* 0x58 */
    "i64.ge_s",              /* 0x59 */
    "i64.ge_u",              /* 0x5A */
    "f32.eq",                /* 0x5B */
    "f32.ne",                /* 0x5C */
    "f32.lt",                /* 0x5D */
    "f32.gt",                /* 0x5E */
    "f32.le",                /* 0x5F */
    "f32.ge",                /* 0x60 */
    "f64.eq",                /* 0x61 */
    "f64.ne",                /* 0x62 */
    "f64.lt",                /* 0x63 */
    "f64.gt",                /* 0x64 */
    "f64.le",                /* 0x65 */
    "f64.ge",                /* 0x66 */
    "i32.clz",               /* 0x67 */
    "i32.ctz",               /* 0x68 */
    "i32.popcnt",            /* 0x69 */
    "i32.add",               /* 0x6A */
    "i32.sub",               /* 0x6B */
    "i32.mul",               /* 0x6C */
    "i32.div_s",             /* 0x6D */
    "i32.div_u",             /* 0x6E */
    "i32.rem_s",             /* 0x6F */
    "i32.rem_u",             /* 0x70 */
    "i32.and",               /* 0x71 */
    "i32.or",                /* 0x72 */
    "i32.xor",               /* 0x73 */
    "i32.shl",               /* 0x74 */
    "i32.shr_s",             /* 0x75 */
    "i32.shr_u",             /* 0x76 */
    "i32.rotl",              /* 0x77 */
    "i32.rotr",              /* 0x78 */
    "i64.clz",               /* 0x79 */
    "i64.ctz",               /* 0x7A */
    "i64.popcnt",            /* 0x7B */
    "i64.add",               /* 0x7C */
    "i64.sub",               /* 0x7D */
    "i64.mul",               /* 0x7E */
    "i64.div_s",             /* 0x7F */
    "i64.div_u",             /* 0x80 */
    "i64.rem_s",             /* 0x81 */
    "i64.rem_u",             /* 0x82 */
    "i64.and",               /* 0x83 */
    "i64.or",                /* 0x84 */
    "i64.xor",               /* 0x85 */
    "i64.shl",               /* 0x86 */
    "i64.shr_s",             /* 0x87 */
    "i64.shr_u",             /* 0x88 */
    "i64.rotl",              /* 0x89 */
    "i64.rotr",              /* 0x8A */
    "f32.abs",               /* 0x8B */
    "f32.neg",               /* 0x8C */
    "f32.ceil",              /* 0x8D */
    "f32.floor",             /* 0x8E */
    "f32.trunc",             /* 0x8F */
    "f32.nearest",           /* 0x90 */
    "f32.sqrt",              /* 0x91 */
    "f32.add",               /* 0x92 */
    "f32.sub",               /* 0x93 */
    "f32.mul",               /* 0x94 */
    "f32.div",               /* 0x95 */
    "f32.min",               /* 0x96 */
    "f32.max",               /* 0x97 */
    "f32.copysign",          /* 0x98 */
    "f64.abs",               /* 0x99 */
    "f64.neg",               /* 0x9A */
    "f64.ceil",              /* 0x9B */
    "f64.floor",             /* 0x9C */
    "f64.trunc",             /* 0x9D */
    "f64.nearest",           /* 0x9E */
    "f64.sqrt",              /* 0x9F */
    "f64.add",               /* 0xA0 */
    "f64.sub",               /* 0xA1 */
    "f64.mul",               /* 0xA2 */
    "f64.div",               /* 0xA3 */
    "f64.min",               /* 0xA4 */
    "f64.max",               /* 0xA5 */
    "f64.copysign",          /* 0xA6 */
    "i32.wrap_i64",          /* 0xA7 */
    "i32.trunc_f32_s",       /* 0xA8 */
    "i32.trunc_f32_u",       /* 0xA9 */
    "i32.trunc_f64_s",       /* 0xAA */
    "i32.trunc_f64_u",       /* 0xAB */
    "i64.extend_i32_s",      /* 0xAC */
    "i64.extend_i32_u",      /* 0xAD */
    "i64.trunc_f32_s",       /* 0xAE */
    "i64.trunc_f32_u",       /* 0xAF */
    "i64.trunc_f64_s",       /* 0xB0 */
    "i64.trunc_f64_u",       /* 0xB1 */
    "f32.convert_i32_s",     /* 0xB2 */
    "f32.convert_i32_u",     /* 0xB3 */
    "f32.convert_i64_s",     /* 0xB4 */
    "f32.convert_i64_u",     /* 0xB5 */
    "f32.demote_f64",        /* 0xB6 */
    "f64.convert_i32_s",     /* 0xB7 */
    "f64.convert_i32_u",     /* 0xB8 */
    "f64.convert_i64_s",     /* 0xB9 */
    "f64.convert_i64_u",     /* 0xBA */
    "f64.promote_f32",       /* 0xBB */
    "i32.reinterpret_f32",   /* 0xBC */
    "i64.reinterpret_f64",   /* 0xBD */
    "f32.reinterpret_i32",   /* 0xBE */
    "f64.reinterpret_i64",   /* 0xBF */
    "i32.extend8_s",         /* 0xC0 */ /* not in core-1 */
    "i32.extend16_s",        /* 0xC1 */ /* not in core-1 */
    "i64.extend8_s",         /* 0xC2 */ /* not in core-1 */
    "i64.extend16_s",        /* 0xC3 */ /* not in core-1 */
    "i64.extend32_s",        /* 0xC4 */ /* not in core-1 */
    NULL,                    /* 0xC5 */
    NULL,                    /* 0xC6 */
    NULL,                    /* 0xC7 */
    NULL,                    /* 0xC8 */
    NULL,                    /* 0xC9 */
    NULL,                    /* 0xCA */
    NULL,                    /* 0xCB */
    NULL,                    /* 0xCC */
    NULL,                    /* 0xCD */
    NULL,                    /* 0xCE */
    NULL,                    /* 0xCF */
    "ref.null",              /* 0xD0 */ /* not in core-1 */
    "ref.is_null",           /* 0xD1 */ /* not in core-1 */
    "ref.func",              /* 0xD2 */ /* not in core-1 */
    NULL,                    /* 0xD3 */
    NULL,                    /* 0xD4 */
    NULL,                    /* 0xD5 */
    NULL,                    /* 0xD6 */
    NULL,                    /* 0xD7 */
    NULL,                    /* 0xD8 */
    NULL,                    /* 0xD9 */
    NULL,                    /* 0xDA */
    NULL,                    /* 0xDB */
    NULL,                    /* 0xDC */
    NULL,                    /* 0xDD */
    NULL,                    /* 0xDE */
    NULL,                    /* 0xDF */
    NULL,                    /* 0xE0 */
    NULL,                    /* 0xE1 */
    NULL,                    /* 0xE2 */
    NULL,                    /* 0xE3 */
    NULL,                    /* 0xE4 */
    NULL,                    /* 0xE5 */
    NULL,                    /* 0xE6 */
    NULL,                    /* 0xE7 */
    NULL,                    /* 0xE8 */
    NULL,                    /* 0xE9 */
    NULL,                    /* 0xEA */
    NULL,                    /* 0xEB */
    NULL,                    /* 0xEC */
    NULL,                    /* 0xED */
    NULL,                    /* 0xEE */
    NULL,                    /* 0xEF */
    NULL,                    /* 0xF0 */
    NULL,                    /* 0xF1 */
    NULL,                    /* 0xF2 */
    NULL,                    /* 0xF3 */
    NULL,                    /* 0xF4 */
    NULL,                    /* 0xF5 */
    NULL,                    /* 0xF6 */
    NULL,                    /* 0xF7 */
    NULL,                    /* 0xF8 */
    NULL,                    /* 0xF9 */
    NULL,                    /* 0xFA */
    NULL,                    /* 0xFB */
    NULL,                    /* 0xFC */
    NULL,                    /* 0xFD */
    NULL,                    /* 0xFE */
    NULL,                    /* 0xFF */
};

const char *instr_name(instr_t in)
{
  const char *s = "?";
  if (0 <= in && in <= 0xFF) {
    /* single byte, core-1 */
    size_t i = (size_t)in;
    if (in <= 0xff && g_innames[i] != NULL) s = g_innames[i];
  } else switch (in) {
    /* extended multibyte, not in core-1 */
    case IN_I32_TRUNC_SAT_F32_S: s = "i32.trunc_sat_f32_s"; break;
    case IN_I32_TRUNC_SAT_F32_U: s = "i32.trunc_sat_f32_u"; break;
    case IN_I32_TRUNC_SAT_F64_S: s = "i32.trunc_sat_f64_s"; break;
    case IN_I32_TRUNC_SAT_F64_U: s = "i32.trunc_sat_f64_u"; break;
    case IN_I64_TRUNC_SAT_F32_S: s = "i64.trunc_sat_f32_s"; break;
    case IN_I64_TRUNC_SAT_F32_U: s = "i64.trunc_sat_f32_u"; break;
    case IN_I64_TRUNC_SAT_F64_S: s = "i64.trunc_sat_f64_s"; break;
    case IN_I64_TRUNC_SAT_F64_U: s = "i64.trunc_sat_f64_u"; break;
    case IN_MEMORY_INIT: s = "memory.init"; break;
    case IN_DATA_DROP: s = "data.drop"; break;
    case IN_MEMORY_COPY: s = "memory.copy"; break;
    case IN_MEMORY_FILL: s = "memory.fill"; break;
    case IN_TABLE_INIT: s = "table.init"; break;
    case IN_ELEM_DROP: s = "elem.drop"; break;
    case IN_TABLE_COPY: s = "table.copy"; break;
    case IN_TABLE_GROW: s = "table.grow"; break;
    case IN_TABLE_SIZE: s = "table.size"; break;
    case IN_TABLE_FILL: s = "table.fill"; break;
    /* simd-128 multibyte, not in core-1 */
    case IN_V128_LOAD: s = "v128.load"; break;
    case IN_V128_LOAD8X8_S: s = "v128.load8x8_s"; break;
    case IN_V128_LOAD8X8_U: s = "v128.load8x8_u"; break;
    case IN_V128_LOAD16X4_S: s = "v128.load16x4_s"; break;
    case IN_V128_LOAD16X4_U: s = "v128.load16x4_u"; break;
    case IN_V128_LOAD32X2_S: s = "v128.load32x2_s"; break;
    case IN_V128_LOAD32X2_U: s = "v128.load32x2_u"; break;
    case IN_V128_LOAD8_SPLAT: s = "v128.load8_splat"; break;
    case IN_V128_LOAD16_SPLAT: s = "v128.load16_splat"; break;
    case IN_V128_LOAD32_SPLAT: s = "v128.load32_splat"; break;
    case IN_V128_LOAD64_SPLAT: s = "v128.load64_splat"; break;
    case IN_V128_STORE: s = "v128.store"; break;
    case IN_V128_CONST: s = "v128.const"; break;
    case IN_I8X16_SHUFFLE: s = "i8x16.shuffle"; break;
    case IN_I8X16_SWIZZLE: s = "i8x16.swizzle"; break;
    case IN_I8X16_SPLAT: s = "i8x16.splat"; break;
    case IN_I16X8_SPLAT: s = "i16x8.splat"; break;
    case IN_I32X4_SPLAT: s = "i32x4.splat"; break;
    case IN_I64X2_SPLAT: s = "i64x2.splat"; break;
    case IN_F32X4_SPLAT: s = "f32x4.splat"; break;
    case IN_F64X2_SPLAT: s = "f64x2.splat"; break;
    case IN_I8X16_EXTRACT_LANE_S: s = "i8x16.extract_lane_s"; break;
    case IN_I8X16_EXTRACT_LANE_U: s = "i8x16.extract_lane_u"; break;
    case IN_I8X16_REPLACE_LANE: s = "i8x16.replace_lane"; break;
    case IN_I16X8_EXTRACT_LANE_S: s = "i16x8.extract_lane_s"; break;
    case IN_I16X8_EXTRACT_LANE_U: s = "i16x8.extract_lane_u"; break;
    case IN_I16X8_REPLACE_LANE: s = "i16x8.replace_lane"; break;
    case IN_I32X4_EXTRACT_LANE: s = "i32x4.extract_lane"; break;
    case IN_I32X4_REPLACE_LANE: s = "i32x4.replace_lane"; break;
    case IN_I64X2_EXTRACT_LANE: s = "i64x2.extract_lane"; break;
    case IN_I64X2_REPLACE_LANE: s = "i64x2.replace_lane"; break;
    case IN_F32X4_EXTRACT_LANE: s = "f32x4.extract_lane"; break;
    case IN_F32X4_REPLACE_LANE: s = "f32x4.replace_lane"; break;
    case IN_F64X2_EXTRACT_LANE: s = "f64x2.extract_lane"; break;
    case IN_F64X2_REPLACE_LANE: s = "f64x2.replace_lane"; break;
    case IN_I8X16_EQ: s = "i8x16.eq"; break;
    case IN_I8X16_NE: s = "i8x16.ne"; break;
    case IN_I8X16_LT_S: s = "i8x16.lt_s"; break;
    case IN_I8X16_LT_U: s = "i8x16.lt_u"; break;
    case IN_I8X16_GT_S: s = "i8x16.gt_s"; break;
    case IN_I8X16_GT_U: s = "i8x16.gt_u"; break;
    case IN_I8X16_LE_S: s = "i8x16.le_s"; break;
    case IN_I8X16_LE_U: s = "i8x16.le_u"; break;
    case IN_I8X16_GE_S: s = "i8x16.ge_s"; break;
    case IN_I8X16_GE_U: s = "i8x16.ge_u"; break;
    case IN_I16X8_EQ: s = "i16x8.eq"; break;
    case IN_I16X8_NE: s = "i16x8.ne"; break;
    case IN_I16X8_LT_S: s = "i16x8.lt_s"; break;
    case IN_I16X8_LT_U: s = "i16x8.lt_u"; break;
    case IN_I16X8_GT_S: s = "i16x8.gt_s"; break;
    case IN_I16X8_GT_U: s = "i16x8.gt_u"; break;
    case IN_I16X8_LE_S: s = "i16x8.le_s"; break;
    case IN_I16X8_LE_U: s = "i16x8.le_u"; break;
    case IN_I16X8_GE_S: s = "i16x8.ge_s"; break;
    case IN_I16X8_GE_U: s = "i16x8.ge_u"; break;
    case IN_I32X4_EQ: s = "i32x4.eq"; break;
    case IN_I32X4_NE: s = "i32x4.ne"; break;
    case IN_I32X4_LT_S: s = "i32x4.lt_s"; break;
    case IN_I32X4_LT_U: s = "i32x4.lt_u"; break;
    case IN_I32X4_GT_S: s = "i32x4.gt_s"; break;
    case IN_I32X4_GT_U: s = "i32x4.gt_u"; break;
    case IN_I32X4_LE_S: s = "i32x4.le_s"; break;
    case IN_I32X4_LE_U: s = "i32x4.le_u"; break;
    case IN_I32X4_GE_S: s = "i32x4.ge_s"; break;
    case IN_I32X4_GE_U: s = "i32x4.ge_u"; break;
    case IN_F32X4_EQ: s = "f32x4.eq"; break;
    case IN_F32X4_NE: s = "f32x4.ne"; break;
    case IN_F32X4_LT: s = "f32x4.lt"; break;
    case IN_F32X4_GT: s = "f32x4.gt"; break;
    case IN_F32X4_LE: s = "f32x4.le"; break;
    case IN_F32X4_GE: s = "f32x4.ge"; break;
    case IN_F64X2_EQ: s = "f64x2.eq"; break;
    case IN_F64X2_NE: s = "f64x2.ne"; break;
    case IN_F64X2_LT: s = "f64x2.lt"; break;
    case IN_F64X2_GT: s = "f64x2.gt"; break;
    case IN_F64X2_LE: s = "f64x2.le"; break;
    case IN_F64X2_GE: s = "f64x2.ge"; break;
    case IN_V128_NOT: s = "v128.not"; break;
    case IN_V128_AND: s = "v128.and"; break;
    case IN_V128_ANDNOT: s = "v128.andnot"; break;
    case IN_V128_OR: s = "v128.or"; break;
    case IN_V128_XOR: s = "v128.xor"; break;
    case IN_V128_BITSELECT: s = "v128.bitselect"; break;
    case IN_V128_ANY_TRUE: s = "v128.any_true"; break;
    case IN_V128_LOAD8_LANE: s = "v128.load8_lane"; break;
    case IN_V128_LOAD16_LANE: s = "v128.load16_lane"; break;
    case IN_V128_LOAD32_LANE: s = "v128.load32_lane"; break;
    case IN_V128_LOAD64_LANE: s = "v128.load64_lane"; break;
    case IN_V128_STORE8_LANE: s = "v128.store8_lane"; break;
    case IN_V128_STORE16_LANE: s = "v128.store16_lane"; break;
    case IN_V128_STORE32_LANE: s = "v128.store32_lane"; break;
    case IN_V128_STORE64_LANE: s = "v128.store64_lane"; break;
    case IN_V128_LOAD32_ZERO: s = "v128.load32_zero"; break;
    case IN_V128_LOAD64_ZERO: s = "v128.load64_zero"; break;
    case IN_F32X4_DEMOTE_F64X2_ZERO: s = "f32x4.demote_f64x2_zero"; break;
    case IN_F64X2_PROMOTE_LOW_F32X4: s = "f64x2.promote_low_f32x4"; break;
    case IN_I8X16_ABS: s = "i8x16.abs"; break;
    case IN_I8X16_NEG: s = "i8x16.neg"; break;
    case IN_I8X16_POPCNT: s = "i8x16.popcnt"; break;
    case IN_I8X16_ALL_TRUE: s = "i8x16.all_true"; break;
    case IN_I8X16_BITMASK: s = "i8x16.bitmask"; break;
    case IN_I8X16_NARROW_I16X8_S: s = "i8x16.narrow_i16x8_s"; break;
    case IN_I8X16_NARROW_I16X8_U: s = "i8x16.narrow_i16x8_u"; break;
    case IN_F32X4_CEIL: s = "f32x4.ceil"; break;
    case IN_F32X4_FLOOR: s = "f32x4.floor"; break;
    case IN_F32X4_TRUNC: s = "f32x4.trunc"; break;
    case IN_F32X4_NEAREST: s = "f32x4.nearest"; break;
    case IN_I8X16_SHL: s = "i8x16.shl"; break;
    case IN_I8X16_SHR_S: s = "i8x16.shr_s"; break;
    case IN_I8X16_SHR_U: s = "i8x16.shr_u"; break;
    case IN_I8X16_ADD: s = "i8x16.add"; break;
    case IN_I8X16_ADD_SAT_S: s = "i8x16.add_sat_s"; break;
    case IN_I8X16_ADD_SAT_U: s = "i8x16.add_sat_u"; break;
    case IN_I8X16_SUB: s = "i8x16.sub"; break;
    case IN_I8X16_SUB_SAT_S: s = "i8x16.sub_sat_s"; break;
    case IN_I8X16_SUB_SAT_U: s = "i8x16.sub_sat_u"; break;
    case IN_F64X2_CEIL: s = "f64x2.ceil"; break;
    case IN_F64X2_FLOOR: s = "f64x2.floor"; break;
    case IN_I8X16_MIN_S: s = "i8x16.min_s"; break;
    case IN_I8X16_MIN_U: s = "i8x16.min_u"; break;
    case IN_I8X16_MAX_S: s = "i8x16.max_s"; break;
    case IN_I8X16_MAX_U: s = "i8x16.max_u"; break;
    case IN_F64X2_TRUNC: s = "f64x2.trunc"; break;
    case IN_I8X16_AVGR_U: s = "i8x16.avgr_u"; break;
    case IN_I16X8_EXTADD_PAIRWISE_I8X16_S: s = "i16x8.extadd_pairwise_i8x16_s"; break;
    case IN_I16X8_EXTADD_PAIRWISE_I8X16_U: s = "i16x8.extadd_pairwise_i8x16_u"; break;
    case IN_I32X4_EXTADD_PAIRWISE_I16X8_S: s = "i32x4.extadd_pairwise_i16x8_s"; break;
    case IN_I32X4_EXTADD_PAIRWISE_I16X8_U: s = "i32x4.extadd_pairwise_i16x8_u"; break;
    case IN_I16X8_ABS: s = "i16x8.abs"; break;
    case IN_I16X8_NEG: s = "i16x8.neg"; break;
    case IN_I16X8_Q15MULR_SAT_S: s = "i16x8.q15mulr_sat_s"; break;
    case IN_I16X8_ALL_TRUE: s = "i16x8.all_true"; break;
    case IN_I16X8_BITMASK: s = "i16x8.bitmask"; break;
    case IN_I16X8_NARROW_I32X4_S: s = "i16x8.narrow_i32x4_s"; break;
    case IN_I16X8_NARROW_I32X4_U: s = "i16x8.narrow_i32x4_u"; break;
    case IN_I16X8_EXTEND_LOW_I8X16_S: s = "i16x8.extend_low_i8x16_s"; break;
    case IN_I16X8_EXTEND_HIGH_I8X16_S: s = "i16x8.extend_high_i8x16_s"; break;
    case IN_I16X8_EXTEND_LOW_I8X16_U: s = "i16x8.extend_low_i8x16_u"; break;
    case IN_I16X8_EXTEND_HIGH_I8X16_U: s = "i16x8.extend_high_i8x16_u"; break;
    case IN_I16X8_SHL: s = "i16x8.shl"; break;
    case IN_I16X8_SHR_S: s = "i16x8.shr_s"; break;
    case IN_I16X8_SHR_U: s = "i16x8.shr_u"; break;
    case IN_I16X8_ADD: s = "i16x8.add"; break;
    case IN_I16X8_ADD_SAT_S: s = "i16x8.add_sat_s"; break;
    case IN_I16X8_ADD_SAT_U: s = "i16x8.add_sat_u"; break;
    case IN_I16X8_SUB: s = "i16x8.sub"; break;
    case IN_I16X8_SUB_SAT_S: s = "i16x8.sub_sat_s"; break;
    case IN_I16X8_SUB_SAT_U: s = "i16x8.sub_sat_u"; break;
    case IN_F64X2_NEAREST: s = "f64x2.nearest"; break;
    case IN_I16X8_MUL: s = "i16x8.mul"; break;
    case IN_I16X8_MIN_S: s = "i16x8.min_s"; break;
    case IN_I16X8_MIN_U: s = "i16x8.min_u"; break;
    case IN_I16X8_MAX_S: s = "i16x8.max_s"; break;
    case IN_I16X8_MAX_U: s = "i16x8.max_u"; break;
    case IN_I16X8_AVGR_U: s = "i16x8.avgr_u"; break;
    case IN_I16X8_EXTMUL_LOW_I8X16_S: s = "i16x8.extmul_low_i8x16_s"; break;
    case IN_I16X8_EXTMUL_HIGH_I8X16_S: s = "i16x8.extmul_high_i8x16_s"; break;
    case IN_I16X8_EXTMUL_LOW_I8X16_U: s = "i16x8.extmul_low_i8x16_u"; break;
    case IN_I16X8_EXTMUL_HIGH_I8X16_U: s = "i16x8.extmul_high_i8x16_u"; break;
    case IN_I32X4_ABS: s = "i32x4.abs"; break;
    case IN_I32X4_NEG: s = "i32x4.neg"; break;
    case IN_I32X4_ALL_TRUE: s = "i32x4.all_true"; break;
    case IN_I32X4_BITMASK: s = "i32x4.bitmask"; break;
    case IN_I32X4_EXTEND_LOW_I16X8_S: s = "i32x4.extend_low_i16x8_s"; break;
    case IN_I32X4_EXTEND_HIGH_I16X8_S: s = "i32x4.extend_high_i16x8_s"; break;
    case IN_I32X4_EXTEND_LOW_I16X8_U: s = "i32x4.extend_low_i16x8_u"; break;
    case IN_I32X4_EXTEND_HIGH_I16X8_U: s = "i32x4.extend_high_i16x8_u"; break;
    case IN_I32X4_SHL: s = "i32x4.shl"; break;
    case IN_I32X4_SHR_S: s = "i32x4.shr_s"; break;
    case IN_I32X4_SHR_U: s = "i32x4.shr_u"; break;
    case IN_I32X4_ADD: s = "i32x4.add"; break;
    case IN_I32X4_SUB: s = "i32x4.sub"; break;
    case IN_I32X4_MUL: s = "i32x4.mul"; break;
    case IN_I32X4_MIN_S: s = "i32x4.min_s"; break;
    case IN_I32X4_MIN_U: s = "i32x4.min_u"; break;
    case IN_I32X4_MAX_S: s = "i32x4.max_s"; break;
    case IN_I32X4_MAX_U: s = "i32x4.max_u"; break;
    case IN_I32X4_DOT_I16X8_S: s = "i32x4.dot_i16x8_s"; break;
    case IN_I32X4_EXTMUL_LOW_I16X8_S: s = "i32x4.extmul_low_i16x8_s"; break;
    case IN_I32X4_EXTMUL_HIGH_I16X8_S: s = "i32x4.extmul_high_i16x8_s"; break;
    case IN_I32X4_EXTMUL_LOW_I16X8_U: s = "i32x4.extmul_low_i16x8_u"; break;
    case IN_I32X4_EXTMUL_HIGH_I16X8_U: s = "i32x4.extmul_high_i16x8_u"; break;
    case IN_I64X2_ABS: s = "i64x2.abs"; break;
    case IN_I64X2_NEG: s = "i64x2.neg"; break;
    case IN_I64X2_ALL_TRUE: s = "i64x2.all_true"; break;
    case IN_I64X2_BITMASK: s = "i64x2.bitmask"; break;
    case IN_I64X2_EXTEND_LOW_I32X4_S: s = "i64x2.extend_low_i32x4_s"; break;
    case IN_I64X2_EXTEND_HIGH_I32X4_S: s = "i64x2.extend_high_i32x4_s"; break;
    case IN_I64X2_EXTEND_LOW_I32X4_U: s = "i64x2.extend_low_i32x4_u"; break;
    case IN_I64X2_EXTEND_HIGH_I32X4_U: s = "i64x2.extend_high_i32x4_u"; break;
    case IN_I64X2_SHL: s = "i64x2.shl"; break;
    case IN_I64X2_SHR_S: s = "i64x2.shr_s"; break;
    case IN_I64X2_SHR_U: s = "i64x2.shr_u"; break;
    case IN_I64X2_ADD: s = "i64x2.add"; break;
    case IN_I64X2_SUB: s = "i64x2.sub"; break;
    case IN_I64X2_MUL: s = "i64x2.mul"; break;
    case IN_I64X2_EQ: s = "i64x2.eq"; break;
    case IN_I64X2_NE: s = "i64x2.ne"; break;
    case IN_I64X2_LT_S: s = "i64x2.lt_s"; break;
    case IN_I64X2_GT_S: s = "i64x2.gt_s"; break;
    case IN_I64X2_LE_S: s = "i64x2.le_s"; break;
    case IN_I64X2_GE_S: s = "i64x2.ge_s"; break;
    case IN_I64X2_EXTMUL_LOW_I32X4_S: s = "i64x2.extmul_low_i32x4_s"; break;
    case IN_I64X2_EXTMUL_HIGH_I32X4_S: s = "i64x2.extmul_high_i32x4_s"; break;
    case IN_I64X2_EXTMUL_LOW_I32X4_U: s = "i64x2.extmul_low_i32x4_u"; break;
    case IN_I64X2_EXTMUL_HIGH_I32X4_U: s = "i64x2.extmul_high_i32x4_u"; break;
    case IN_F32X4_ABS: s = "f32x4.abs"; break;
    case IN_F32X4_NEG: s = "f32x4.neg"; break;
    case IN_F32X4_SQRT: s = "f32x4.sqrt"; break;
    case IN_F32X4_ADD: s = "f32x4.add"; break;
    case IN_F32X4_SUB: s = "f32x4.sub"; break;
    case IN_F32X4_MUL: s = "f32x4.mul"; break;
    case IN_F32X4_DIV: s = "f32x4.div"; break;
    case IN_F32X4_MIN: s = "f32x4.min"; break;
    case IN_F32X4_MAX: s = "f32x4.max"; break;
    case IN_F32X4_PMIN: s = "f32x4.pmin"; break;
    case IN_F32X4_PMAX: s = "f32x4.pmax"; break;
    case IN_F64X2_ABS: s = "f64x2.abs"; break;
    case IN_F64X2_NEG: s = "f64x2.neg"; break;
    case IN_F64X2_SQRT: s = "f64x2.sqrt"; break;
    case IN_F64X2_ADD: s = "f64x2.add"; break;
    case IN_F64X2_SUB: s = "f64x2.sub"; break;
    case IN_F64X2_MUL: s = "f64x2.mul"; break;
    case IN_F64X2_DIV: s = "f64x2.div"; break;
    case IN_F64X2_MIN: s = "f64x2.min"; break;
    case IN_F64X2_MAX: s = "f64x2.max"; break;
    case IN_F64X2_PMIN: s = "f64x2.pmin"; break;
    case IN_F64X2_PMAX: s = "f64x2.pmax"; break;
    case IN_I32X4_TRUNC_SAT_F32X4_S: s = "i32x4.trunc_sat_f32x4_s"; break;
    case IN_I32X4_TRUNC_SAT_F32X4_U: s = "i32x4.trunc_sat_f32x4_u"; break;
    case IN_F32X4_CONVERT_I32X4_S: s = "f32x4.convert_i32x4_s"; break;
    case IN_F32X4_CONVERT_I32X4_U: s = "f32x4.convert_i32x4_u"; break;
    case IN_I32X4_TRUNC_SAT_F64X2_S_ZERO: s = "i32x4.trunc_sat_f64x2_s_zero"; break;
    case IN_I32X4_TRUNC_SAT_F64X2_U_ZERO: s = "i32x4.trunc_sat_f64x2_u_zero"; break;
    case IN_F64X2_CONVERT_LOW_I32X4_S: s = "f64x2.convert_low_i32x4_s"; break;
    case IN_F64X2_CONVERT_LOW_I32X4_U: s = "f64x2.convert_low_i32x4_u"; break;
    /* internal use */
    case IN_REF_DATA: s = "ref.data"; break;
    case IN_DATA_PUT_REF: s = "data.put_ref"; break;
    default: assert(false);
  }
  return s;
}

const char *valtype_name(valtype_t vt)
{
  const char *s = "?";
  switch (vt) {
    case BT_VOID: s = "void"; break;
    case VT_I32: s = "i32"; break;
    case VT_I64: s = "i64"; break;
    case VT_F32: s = "f32"; break;
    case VT_F64: s = "f64"; break;
    case VT_V128: s = "v128"; break;
    case RT_FUNCREF: s = "funcref"; break;
    case RT_EXTERNREF: s = "externref"; break;
    default:;
  }
  return s;
}

static buf_t g_nimap;

static void nimap_intern(const char *name, instr_t in)
{
  int *pi = bufnewbk(&g_nimap); 
  pi[0] = intern(name); pi[1] = (int)in;
}

instr_t name_instr(const char *name)
{ 
  sym_t sym; int *pi;
  if (!g_nimap.esz) {
    size_t i; bufinit(&g_nimap, sizeof(int)*2);
    /* single byte, core-1 */
    for (i = 0; i <= 0xff; ++i) {
      const char *s = g_innames[i];
      if (s) nimap_intern(s, (instr_t)i);
    }
    /* extended multibyte, not in core-1 */
    nimap_intern("i32.trunc_sat_f32_s", IN_I32_TRUNC_SAT_F32_S);
    nimap_intern("i32.trunc_sat_f32_u", IN_I32_TRUNC_SAT_F32_U);
    nimap_intern("i32.trunc_sat_f64_s", IN_I32_TRUNC_SAT_F64_S);
    nimap_intern("i32.trunc_sat_f64_u", IN_I32_TRUNC_SAT_F64_U);
    nimap_intern("i64.trunc_sat_f32_s", IN_I64_TRUNC_SAT_F32_S);
    nimap_intern("i64.trunc_sat_f32_u", IN_I64_TRUNC_SAT_F32_U);
    nimap_intern("i64.trunc_sat_f64_s", IN_I64_TRUNC_SAT_F64_S);
    nimap_intern("i64.trunc_sat_f64_u", IN_I64_TRUNC_SAT_F64_U);
    nimap_intern("memory.init", IN_MEMORY_INIT);
    nimap_intern("data.drop", IN_DATA_DROP);
    nimap_intern("memory.copy", IN_MEMORY_COPY);
    nimap_intern("memory.fill", IN_MEMORY_FILL);
    nimap_intern("table.init", IN_TABLE_INIT);
    nimap_intern("elem.drop", IN_ELEM_DROP);
    nimap_intern("table.copy", IN_TABLE_COPY);
    nimap_intern("table.grow", IN_TABLE_GROW);
    nimap_intern("table.size", IN_TABLE_SIZE);
    nimap_intern("table.fill", IN_TABLE_FILL);
    /* simd-128 multibyte, not in core-1 */
    nimap_intern("v128.load", IN_V128_LOAD);
    nimap_intern("v128.load8x8_s", IN_V128_LOAD8X8_S);
    nimap_intern("v128.load8x8_u", IN_V128_LOAD8X8_U);
    nimap_intern("v128.load16x4_s", IN_V128_LOAD16X4_S);
    nimap_intern("v128.load16x4_u", IN_V128_LOAD16X4_U);
    nimap_intern("v128.load32x2_s", IN_V128_LOAD32X2_S);
    nimap_intern("v128.load32x2_u", IN_V128_LOAD32X2_U);
    nimap_intern("v128.load8_splat", IN_V128_LOAD8_SPLAT);
    nimap_intern("v128.load16_splat", IN_V128_LOAD16_SPLAT);
    nimap_intern("v128.load32_splat", IN_V128_LOAD32_SPLAT);
    nimap_intern("v128.load64_splat", IN_V128_LOAD64_SPLAT);
    nimap_intern("v128.store", IN_V128_STORE);
    nimap_intern("v128.const", IN_V128_CONST);
    nimap_intern("i8x16.shuffle", IN_I8X16_SHUFFLE);
    nimap_intern("i8x16.swizzle", IN_I8X16_SWIZZLE);
    nimap_intern("i8x16.splat", IN_I8X16_SPLAT);
    nimap_intern("i16x8.splat", IN_I16X8_SPLAT);
    nimap_intern("i32x4.splat", IN_I32X4_SPLAT);
    nimap_intern("i64x2.splat", IN_I64X2_SPLAT);
    nimap_intern("f32x4.splat", IN_F32X4_SPLAT);
    nimap_intern("f64x2.splat", IN_F64X2_SPLAT);
    nimap_intern("i8x16.extract_lane_s", IN_I8X16_EXTRACT_LANE_S);
    nimap_intern("i8x16.extract_lane_u", IN_I8X16_EXTRACT_LANE_U);
    nimap_intern("i8x16.replace_lane", IN_I8X16_REPLACE_LANE);
    nimap_intern("i16x8.extract_lane_s", IN_I16X8_EXTRACT_LANE_S);
    nimap_intern("i16x8.extract_lane_u", IN_I16X8_EXTRACT_LANE_U);
    nimap_intern("i16x8.replace_lane", IN_I16X8_REPLACE_LANE);
    nimap_intern("i32x4.extract_lane", IN_I32X4_EXTRACT_LANE);
    nimap_intern("i32x4.replace_lane", IN_I32X4_REPLACE_LANE);
    nimap_intern("i64x2.extract_lane", IN_I64X2_EXTRACT_LANE);
    nimap_intern("i64x2.replace_lane", IN_I64X2_REPLACE_LANE);
    nimap_intern("f32x4.extract_lane", IN_F32X4_EXTRACT_LANE);
    nimap_intern("f32x4.replace_lane", IN_F32X4_REPLACE_LANE);
    nimap_intern("f64x2.extract_lane", IN_F64X2_EXTRACT_LANE);
    nimap_intern("f64x2.replace_lane", IN_F64X2_REPLACE_LANE);
    nimap_intern("i8x16.eq", IN_I8X16_EQ);
    nimap_intern("i8x16.ne", IN_I8X16_NE);
    nimap_intern("i8x16.lt_s", IN_I8X16_LT_S);
    nimap_intern("i8x16.lt_u", IN_I8X16_LT_U);
    nimap_intern("i8x16.gt_s", IN_I8X16_GT_S);
    nimap_intern("i8x16.gt_u", IN_I8X16_GT_U);
    nimap_intern("i8x16.le_s", IN_I8X16_LE_S);
    nimap_intern("i8x16.le_u", IN_I8X16_LE_U);
    nimap_intern("i8x16.ge_s", IN_I8X16_GE_S);
    nimap_intern("i8x16.ge_u", IN_I8X16_GE_U);
    nimap_intern("i16x8.eq", IN_I16X8_EQ);
    nimap_intern("i16x8.ne", IN_I16X8_NE);
    nimap_intern("i16x8.lt_s", IN_I16X8_LT_S);
    nimap_intern("i16x8.lt_u", IN_I16X8_LT_U);
    nimap_intern("i16x8.gt_s", IN_I16X8_GT_S);
    nimap_intern("i16x8.gt_u", IN_I16X8_GT_U);
    nimap_intern("i16x8.le_s", IN_I16X8_LE_S);
    nimap_intern("i16x8.le_u", IN_I16X8_LE_U);
    nimap_intern("i16x8.ge_s", IN_I16X8_GE_S);
    nimap_intern("i16x8.ge_u", IN_I16X8_GE_U);
    nimap_intern("i32x4.eq", IN_I32X4_EQ);
    nimap_intern("i32x4.ne", IN_I32X4_NE);
    nimap_intern("i32x4.lt_s", IN_I32X4_LT_S);
    nimap_intern("i32x4.lt_u", IN_I32X4_LT_U);
    nimap_intern("i32x4.gt_s", IN_I32X4_GT_S);
    nimap_intern("i32x4.gt_u", IN_I32X4_GT_U);
    nimap_intern("i32x4.le_s", IN_I32X4_LE_S);
    nimap_intern("i32x4.le_u", IN_I32X4_LE_U);
    nimap_intern("i32x4.ge_s", IN_I32X4_GE_S);
    nimap_intern("i32x4.ge_u", IN_I32X4_GE_U);
    nimap_intern("f32x4.eq", IN_F32X4_EQ);
    nimap_intern("f32x4.ne", IN_F32X4_NE);
    nimap_intern("f32x4.lt", IN_F32X4_LT);
    nimap_intern("f32x4.gt", IN_F32X4_GT);
    nimap_intern("f32x4.le", IN_F32X4_LE);
    nimap_intern("f32x4.ge", IN_F32X4_GE);
    nimap_intern("f64x2.eq", IN_F64X2_EQ);
    nimap_intern("f64x2.ne", IN_F64X2_NE);
    nimap_intern("f64x2.lt", IN_F64X2_LT);
    nimap_intern("f64x2.gt", IN_F64X2_GT);
    nimap_intern("f64x2.le", IN_F64X2_LE);
    nimap_intern("f64x2.ge", IN_F64X2_GE);
    nimap_intern("v128.not", IN_V128_NOT);
    nimap_intern("v128.and", IN_V128_AND);
    nimap_intern("v128.andnot", IN_V128_ANDNOT);
    nimap_intern("v128.or", IN_V128_OR);
    nimap_intern("v128.xor", IN_V128_XOR);
    nimap_intern("v128.bitselect", IN_V128_BITSELECT);
    nimap_intern("v128.any_true", IN_V128_ANY_TRUE);
    nimap_intern("v128.load8_lane", IN_V128_LOAD8_LANE);
    nimap_intern("v128.load16_lane", IN_V128_LOAD16_LANE);
    nimap_intern("v128.load32_lane", IN_V128_LOAD32_LANE);
    nimap_intern("v128.load64_lane", IN_V128_LOAD64_LANE);
    nimap_intern("v128.store8_lane", IN_V128_STORE8_LANE);
    nimap_intern("v128.store16_lane", IN_V128_STORE16_LANE);
    nimap_intern("v128.store32_lane", IN_V128_STORE32_LANE);
    nimap_intern("v128.store64_lane", IN_V128_STORE64_LANE);
    nimap_intern("v128.load32_zero", IN_V128_LOAD32_ZERO);
    nimap_intern("v128.load64_zero", IN_V128_LOAD64_ZERO);
    nimap_intern("f32x4.demote_f64x2_zero", IN_F32X4_DEMOTE_F64X2_ZERO);
    nimap_intern("f64x2.promote_low_f32x4", IN_F64X2_PROMOTE_LOW_F32X4);
    nimap_intern("i8x16.abs", IN_I8X16_ABS);
    nimap_intern("i8x16.neg", IN_I8X16_NEG);
    nimap_intern("i8x16.popcnt", IN_I8X16_POPCNT);
    nimap_intern("i8x16.all_true", IN_I8X16_ALL_TRUE);
    nimap_intern("i8x16.bitmask", IN_I8X16_BITMASK);
    nimap_intern("i8x16.narrow_i16x8_s", IN_I8X16_NARROW_I16X8_S);
    nimap_intern("i8x16.narrow_i16x8_u", IN_I8X16_NARROW_I16X8_U);
    nimap_intern("f32x4.ceil", IN_F32X4_CEIL);
    nimap_intern("f32x4.floor", IN_F32X4_FLOOR);
    nimap_intern("f32x4.trunc", IN_F32X4_TRUNC);
    nimap_intern("f32x4.nearest", IN_F32X4_NEAREST);
    nimap_intern("i8x16.shl", IN_I8X16_SHL);
    nimap_intern("i8x16.shr_s", IN_I8X16_SHR_S);
    nimap_intern("i8x16.shr_u", IN_I8X16_SHR_U);
    nimap_intern("i8x16.add", IN_I8X16_ADD);
    nimap_intern("i8x16.add_sat_s", IN_I8X16_ADD_SAT_S);
    nimap_intern("i8x16.add_sat_u", IN_I8X16_ADD_SAT_U);
    nimap_intern("i8x16.sub", IN_I8X16_SUB);
    nimap_intern("i8x16.sub_sat_s", IN_I8X16_SUB_SAT_S);
    nimap_intern("i8x16.sub_sat_u", IN_I8X16_SUB_SAT_U);
    nimap_intern("f64x2.ceil", IN_F64X2_CEIL);
    nimap_intern("f64x2.floor", IN_F64X2_FLOOR);
    nimap_intern("i8x16.min_s", IN_I8X16_MIN_S);
    nimap_intern("i8x16.min_u", IN_I8X16_MIN_U);
    nimap_intern("i8x16.max_s", IN_I8X16_MAX_S);
    nimap_intern("i8x16.max_u", IN_I8X16_MAX_U);
    nimap_intern("f64x2.trunc", IN_F64X2_TRUNC);
    nimap_intern("i8x16.avgr_u", IN_I8X16_AVGR_U);
    nimap_intern("i16x8.extadd_pairwise_i8x16_s", IN_I16X8_EXTADD_PAIRWISE_I8X16_S);
    nimap_intern("i16x8.extadd_pairwise_i8x16_u", IN_I16X8_EXTADD_PAIRWISE_I8X16_U);
    nimap_intern("i32x4.extadd_pairwise_i16x8_s", IN_I32X4_EXTADD_PAIRWISE_I16X8_S);
    nimap_intern("i32x4.extadd_pairwise_i16x8_u", IN_I32X4_EXTADD_PAIRWISE_I16X8_U);
    nimap_intern("i16x8.abs", IN_I16X8_ABS);
    nimap_intern("i16x8.neg", IN_I16X8_NEG);
    nimap_intern("i16x8.q15mulr_sat_s", IN_I16X8_Q15MULR_SAT_S);
    nimap_intern("i16x8.all_true", IN_I16X8_ALL_TRUE);
    nimap_intern("i16x8.bitmask", IN_I16X8_BITMASK);
    nimap_intern("i16x8.narrow_i32x4_s", IN_I16X8_NARROW_I32X4_S);
    nimap_intern("i16x8.narrow_i32x4_u", IN_I16X8_NARROW_I32X4_U);
    nimap_intern("i16x8.extend_low_i8x16_s", IN_I16X8_EXTEND_LOW_I8X16_S);
    nimap_intern("i16x8.extend_high_i8x16_s", IN_I16X8_EXTEND_HIGH_I8X16_S);
    nimap_intern("i16x8.extend_low_i8x16_u", IN_I16X8_EXTEND_LOW_I8X16_U);
    nimap_intern("i16x8.extend_high_i8x16_u", IN_I16X8_EXTEND_HIGH_I8X16_U);
    nimap_intern("i16x8.shl", IN_I16X8_SHL);
    nimap_intern("i16x8.shr_s", IN_I16X8_SHR_S);
    nimap_intern("i16x8.shr_u", IN_I16X8_SHR_U);
    nimap_intern("i16x8.add", IN_I16X8_ADD);
    nimap_intern("i16x8.add_sat_s", IN_I16X8_ADD_SAT_S);
    nimap_intern("i16x8.add_sat_u", IN_I16X8_ADD_SAT_U);
    nimap_intern("i16x8.sub", IN_I16X8_SUB);
    nimap_intern("i16x8.sub_sat_s", IN_I16X8_SUB_SAT_S);
    nimap_intern("i16x8.sub_sat_u", IN_I16X8_SUB_SAT_U);
    nimap_intern("f64x2.nearest", IN_F64X2_NEAREST);
    nimap_intern("i16x8.mul", IN_I16X8_MUL);
    nimap_intern("i16x8.min_s", IN_I16X8_MIN_S);
    nimap_intern("i16x8.min_u", IN_I16X8_MIN_U);
    nimap_intern("i16x8.max_s", IN_I16X8_MAX_S);
    nimap_intern("i16x8.max_u", IN_I16X8_MAX_U);
    nimap_intern("i16x8.avgr_u", IN_I16X8_AVGR_U);
    nimap_intern("i16x8.extmul_low_i8x16_s", IN_I16X8_EXTMUL_LOW_I8X16_S);
    nimap_intern("i16x8.extmul_high_i8x16_s", IN_I16X8_EXTMUL_HIGH_I8X16_S);
    nimap_intern("i16x8.extmul_low_i8x16_u", IN_I16X8_EXTMUL_LOW_I8X16_U);
    nimap_intern("i16x8.extmul_high_i8x16_u", IN_I16X8_EXTMUL_HIGH_I8X16_U);
    nimap_intern("i32x4.abs", IN_I32X4_ABS);
    nimap_intern("i32x4.neg", IN_I32X4_NEG);
    nimap_intern("i32x4.all_true", IN_I32X4_ALL_TRUE);
    nimap_intern("i32x4.bitmask", IN_I32X4_BITMASK);
    nimap_intern("i32x4.extend_low_i16x8_s", IN_I32X4_EXTEND_LOW_I16X8_S);
    nimap_intern("i32x4.extend_high_i16x8_s", IN_I32X4_EXTEND_HIGH_I16X8_S);
    nimap_intern("i32x4.extend_low_i16x8_u", IN_I32X4_EXTEND_LOW_I16X8_U);
    nimap_intern("i32x4.extend_high_i16x8_u", IN_I32X4_EXTEND_HIGH_I16X8_U);
    nimap_intern("i32x4.shl", IN_I32X4_SHL);
    nimap_intern("i32x4.shr_s", IN_I32X4_SHR_S);
    nimap_intern("i32x4.shr_u", IN_I32X4_SHR_U);
    nimap_intern("i32x4.add", IN_I32X4_ADD);
    nimap_intern("i32x4.sub", IN_I32X4_SUB);
    nimap_intern("i32x4.mul", IN_I32X4_MUL);
    nimap_intern("i32x4.min_s", IN_I32X4_MIN_S);
    nimap_intern("i32x4.min_u", IN_I32X4_MIN_U);
    nimap_intern("i32x4.max_s", IN_I32X4_MAX_S);
    nimap_intern("i32x4.max_u", IN_I32X4_MAX_U);
    nimap_intern("i32x4.dot_i16x8_s", IN_I32X4_DOT_I16X8_S);
    nimap_intern("i32x4.extmul_low_i16x8_s", IN_I32X4_EXTMUL_LOW_I16X8_S);
    nimap_intern("i32x4.extmul_high_i16x8_s", IN_I32X4_EXTMUL_HIGH_I16X8_S);
    nimap_intern("i32x4.extmul_low_i16x8_u", IN_I32X4_EXTMUL_LOW_I16X8_U);
    nimap_intern("i32x4.extmul_high_i16x8_u", IN_I32X4_EXTMUL_HIGH_I16X8_U);
    nimap_intern("i64x2.abs", IN_I64X2_ABS);
    nimap_intern("i64x2.neg", IN_I64X2_NEG);
    nimap_intern("i64x2.all_true", IN_I64X2_ALL_TRUE);
    nimap_intern("i64x2.bitmask", IN_I64X2_BITMASK);
    nimap_intern("i64x2.extend_low_i32x4_s", IN_I64X2_EXTEND_LOW_I32X4_S);
    nimap_intern("i64x2.extend_high_i32x4_s", IN_I64X2_EXTEND_HIGH_I32X4_S);
    nimap_intern("i64x2.extend_low_i32x4_u", IN_I64X2_EXTEND_LOW_I32X4_U);
    nimap_intern("i64x2.extend_high_i32x4_u", IN_I64X2_EXTEND_HIGH_I32X4_U);
    nimap_intern("i64x2.shl", IN_I64X2_SHL);
    nimap_intern("i64x2.shr_s", IN_I64X2_SHR_S);
    nimap_intern("i64x2.shr_u", IN_I64X2_SHR_U);
    nimap_intern("i64x2.add", IN_I64X2_ADD);
    nimap_intern("i64x2.sub", IN_I64X2_SUB);
    nimap_intern("i64x2.mul", IN_I64X2_MUL);
    nimap_intern("i64x2.eq", IN_I64X2_EQ);
    nimap_intern("i64x2.ne", IN_I64X2_NE);
    nimap_intern("i64x2.lt_s", IN_I64X2_LT_S);
    nimap_intern("i64x2.gt_s", IN_I64X2_GT_S);
    nimap_intern("i64x2.le_s", IN_I64X2_LE_S);
    nimap_intern("i64x2.ge_s", IN_I64X2_GE_S);
    nimap_intern("i64x2.extmul_low_i32x4_s", IN_I64X2_EXTMUL_LOW_I32X4_S);
    nimap_intern("i64x2.extmul_high_i32x4_s", IN_I64X2_EXTMUL_HIGH_I32X4_S);
    nimap_intern("i64x2.extmul_low_i32x4_u", IN_I64X2_EXTMUL_LOW_I32X4_U);
    nimap_intern("i64x2.extmul_high_i32x4_u", IN_I64X2_EXTMUL_HIGH_I32X4_U);
    nimap_intern("f32x4.abs", IN_F32X4_ABS);
    nimap_intern("f32x4.neg", IN_F32X4_NEG);
    nimap_intern("f32x4.sqrt", IN_F32X4_SQRT);
    nimap_intern("f32x4.add", IN_F32X4_ADD);
    nimap_intern("f32x4.sub", IN_F32X4_SUB);
    nimap_intern("f32x4.mul", IN_F32X4_MUL);
    nimap_intern("f32x4.div", IN_F32X4_DIV);
    nimap_intern("f32x4.min", IN_F32X4_MIN);
    nimap_intern("f32x4.max", IN_F32X4_MAX);
    nimap_intern("f32x4.pmin", IN_F32X4_PMIN);
    nimap_intern("f32x4.pmax", IN_F32X4_PMAX);
    nimap_intern("f64x2.abs", IN_F64X2_ABS);
    nimap_intern("f64x2.neg", IN_F64X2_NEG);
    nimap_intern("f64x2.sqrt", IN_F64X2_SQRT);
    nimap_intern("f64x2.add", IN_F64X2_ADD);
    nimap_intern("f64x2.sub", IN_F64X2_SUB);
    nimap_intern("f64x2.mul", IN_F64X2_MUL);
    nimap_intern("f64x2.div", IN_F64X2_DIV);
    nimap_intern("f64x2.min", IN_F64X2_MIN);
    nimap_intern("f64x2.max", IN_F64X2_MAX);
    nimap_intern("f64x2.pmin", IN_F64X2_PMIN);
    nimap_intern("f64x2.pmax", IN_F64X2_PMAX);
    nimap_intern("i32x4.trunc_sat_f32x4_s", IN_I32X4_TRUNC_SAT_F32X4_S);
    nimap_intern("i32x4.trunc_sat_f32x4_u", IN_I32X4_TRUNC_SAT_F32X4_U);
    nimap_intern("f32x4.convert_i32x4_s", IN_F32X4_CONVERT_I32X4_S);
    nimap_intern("f32x4.convert_i32x4_u", IN_F32X4_CONVERT_I32X4_U);
    nimap_intern("i32x4.trunc_sat_f64x2_s_zero", IN_I32X4_TRUNC_SAT_F64X2_S_ZERO);
    nimap_intern("i32x4.trunc_sat_f64x2_u_zero", IN_I32X4_TRUNC_SAT_F64X2_U_ZERO);
    nimap_intern("f64x2.convert_low_i32x4_s", IN_F64X2_CONVERT_LOW_I32X4_S);
    nimap_intern("f64x2.convert_low_i32x4_u", IN_F64X2_CONVERT_LOW_I32X4_U);
    /* internal use */
    nimap_intern("ref.data", IN_REF_DATA);
    nimap_intern("data.put_ref", IN_DATA_PUT_REF);
    bufqsort(&g_nimap, &sym_cmp);
  }
  sym = intern(name);
  pi = bufbsearch(&g_nimap, &sym, &sym_cmp);
  if (pi) return (instr_t)pi[1];
  return IN_PLACEHOLDER;
}


insig_t instr_sig(instr_t in)
{
  switch (in) {
    case IN_BLOCK:         case IN_LOOP:         case IN_IF:
      return INSIG_BT;
    case IN_BR:            case IN_BR_IF:
      return INSIG_L;
    case IN_BR_TABLE:
      return INSIG_LS_L;
    case IN_CALL:          case IN_RETURN_CALL:
      return INSIG_XG;
    case IN_CALL_INDIRECT: case IN_RETURN_CALL_INDIRECT:
      return INSIG_CI;
    case IN_SELECT_T:
      return INSIG_T;  
    case IN_LOCAL_GET:     case IN_LOCAL_SET:    case IN_LOCAL_TEE: 
      return INSIG_XL;
    case IN_GLOBAL_GET:    case IN_GLOBAL_SET:   
      return INSIG_XG;
    case IN_TABLE_GET:    case IN_TABLE_SET:
      return INSIG_XT;
    case IN_I32_LOAD:      case IN_I64_LOAD:     case IN_F32_LOAD:     case IN_F64_LOAD:
    case IN_I32_LOAD8_S:   case IN_I32_LOAD8_U:  case IN_I32_LOAD16_S: case IN_I32_LOAD16_U:
    case IN_I64_LOAD8_S:   case IN_I64_LOAD8_U:  case IN_I64_LOAD16_S: case IN_I64_LOAD16_U:
    case IN_I64_LOAD32_S:  case IN_I64_LOAD32_U: case IN_I32_STORE:    case IN_I64_STORE:
    case IN_F32_STORE:     case IN_F64_STORE:    case IN_I32_STORE8:   case IN_I32_STORE16:
    case IN_I64_STORE8:    case IN_I64_STORE16:  case IN_I64_STORE32:
      return INSIG_MEMARG;
    case IN_I32_CONST:
      return INSIG_I32;  
    case IN_I64_CONST:
      return INSIG_I64;
    case IN_F32_CONST:
      return INSIG_F32;
    case IN_F64_CONST:
      return INSIG_F64;     
    case IN_REF_NULL:
      return INSIG_T;
    case IN_MEMORY_INIT:   case IN_DATA_DROP:    case IN_ELEM_DROP:
    case IN_TABLE_GROW:    case IN_TABLE_SIZE:   case IN_TABLE_FILL:   
      return INSIG_XT;
    case IN_TABLE_INIT:    case IN_TABLE_COPY:
      return INSIG_X_Y;
    case IN_REF_FUNC:
      return INSIG_RF;
    /* simd-128 */
    case IN_V128_CONST:
      return INSIG_I128;
    case IN_V128_LOAD:  
    case IN_V128_LOAD8X8_S:       case IN_V128_LOAD8X8_U:
    case IN_V128_LOAD16X4_S:      case IN_V128_LOAD16X4_U:
    case IN_V128_LOAD32X2_S:      case IN_V128_LOAD32X2_U:
    case IN_V128_LOAD8_SPLAT:     case IN_V128_LOAD16_SPLAT:
    case IN_V128_LOAD32_SPLAT:    case IN_V128_LOAD64_SPLAT:
    case IN_V128_STORE:      
      return INSIG_MEMARG;
    case IN_I8X16_SHUFFLE:       
      return INSIG_LANEIDX16;
    case IN_I8X16_EXTRACT_LANE_S: case IN_I8X16_EXTRACT_LANE_U:
    case IN_I8X16_REPLACE_LANE:   
    case IN_I16X8_EXTRACT_LANE_S: case IN_I16X8_EXTRACT_LANE_U:
    case IN_I16X8_REPLACE_LANE:   case IN_I32X4_EXTRACT_LANE:
    case IN_I32X4_REPLACE_LANE:   case IN_I64X2_EXTRACT_LANE:
    case IN_I64X2_REPLACE_LANE:   case IN_F32X4_EXTRACT_LANE:
    case IN_F32X4_REPLACE_LANE:   case IN_F64X2_EXTRACT_LANE:
    case IN_F64X2_REPLACE_LANE:      
      return INSIG_LANEIDX;
    case IN_V128_LOAD8_LANE:      case IN_V128_LOAD16_LANE:
    case IN_V128_LOAD32_LANE:     case IN_V128_LOAD64_LANE:
    case IN_V128_STORE8_LANE:     case IN_V128_STORE16_LANE:
    case IN_V128_STORE32_LANE:    case IN_V128_STORE64_LANE:
    case IN_V128_LOAD32_ZERO:     case IN_V128_LOAD64_ZERO:
      return INSIG_MEMARG_LANEIDX;
    /* internal use */
    case IN_REF_DATA:
      return INSIG_RD; 
    case IN_DATA_PUT_REF:
      return INSIG_PR; 
    default:;
  }
  return INSIG_NONE;
}

char *format_inscode(inscode_t *pic, chbuf_t *pcb)
{
  insig_t is;
  chbclear(pcb);
  if (pic->in == IN_REGDECL) {
    const char *ts = valtype_name((valtype_t)pic->arg.u);
    if (!pic->id) chbputf(pcb, "register %s", ts); 
    else chbputf(pcb, "register %s $%s", ts, symname(pic->id)); 
    return chbdata(pcb);
  } else if (pic->in == IN_END) { /* has a label for display purposes */
    if (pic->id) chbputf(pcb, "end $%s", symname(pic->id));
    else chbputs("end", pcb);
    return chbdata(pcb);
  }
  chbputs(instr_name(pic->in), pcb);
  switch (is = instr_sig(pic->in)) {
    case INSIG_NONE:
      break;
    case INSIG_BT: {
      valtype_t vt = pic->arg.u;
      if (pic->id) chbputf(pcb, " $%s", symname(pic->id)); 
      if (vt != BT_VOID) chbputf(pcb, " (result %s)", valtype_name(vt)); 
    } break;
    case INSIG_XL: case INSIG_XG: case INSIG_XT: case INSIG_RF:
      if (pic->id && pic->arg2.mod) 
        chbputf(pcb, " $%s:%s", symname(pic->arg2.mod), symname(pic->id)); 
      else if (pic->id) chbputf(pcb, " $%s", symname(pic->id)); 
      else chbputf(pcb, " %lld", pic->arg.i); 
      break;
    case INSIG_L: case INSIG_T: case INSIG_LANEIDX:
      if (pic->id) chbputf(pcb, " $%s", symname(pic->id)); 
      else chbputf(pcb, " %lld", pic->arg.i); 
      break;
    case INSIG_I32: {
      long long i = (long long)(pic->arg.u << 32) >> 32; /* extend sign to upper 32 bits */
      chbputf(pcb, " %lld", i); /* write negatives (if considered signed) as negatives */
      if (pic->id) chbputf(pcb, " (; $%s:%s ;)", symname(pic->arg2.mod), symname(pic->id)); 
    } break;
    case INSIG_I64:
      chbputf(pcb, " %lld", pic->arg.i); 
      if (pic->id) chbputf(pcb, " (; $%s:%s ;)", symname(pic->arg2.mod), symname(pic->id)); 
      break;
    case INSIG_CI: /* should be dumped fancily if types are available */
      if (pic->id) chbputf(pcb, " (type $%s)", symname(pic->id)); 
      else {
        size_t n = (size_t)pic->arg.u;
        if (n < fsblen(&g_funcsigs)) { /* ref to global fsig */
          chbuf_t cb = mkchb();
          format_funcsig(fsbref(&g_funcsigs, n), &cb);
          chbputf(pcb, " %s", chbdata(&cb));
          chbfini(&cb);  
        } else { /* assume fsigs are dumped separately */
          chbputf(pcb, " (type %u)", (unsigned)pic->arg.u); 
        }
      }
      break;
    case INSIG_X_Y:
      if (pic->id) chbputf(pcb, " $%s %u", symname(pic->id), pic->arg2.u); 
      else chbputf(pcb, " %lld %u", pic->arg.i, pic->arg2.u); 
      break;
    case INSIG_F32: {
      char buf[32]; float f = pic->arg.f;
      char *s = uftohex(as_uint32(f), buf);
      chbputf(pcb, " %s", s);
      if (-HUGE_VAL < f && f < HUGE_VAL) {
        sprintf(buf, "%.9g", (double)f); chbputf(pcb, " (; %s ;)", &buf[0]);
      }
    } break;
    case INSIG_F64: {
      char buf[32]; double d = pic->arg.d;
      char *s = udtohex(as_uint64(d), buf);
      chbputf(pcb, " %s", s);
      if (-HUGE_VAL < d && d < HUGE_VAL) {
        sprintf(buf, "%.9g", d); chbputf(pcb, " (; %s ;)", &buf[0]);
      } 
    } break;
    case INSIG_I128: {
      /* todo: negative pic->id as an element type hint in as set in asm_simd128_const() */
      if (pic->id > 0) chbputf(pcb, " $%s", symname(pic->id)); 
      else {
        chbputf(pcb, " i64x2");
        chbputf(pcb, " %lld", pic->arg.i); 
        chbputf(pcb, " %lld", pic->arg2.i); 
      }
    } break;
    case INSIG_MEMARG:
      if (pic->id) chbputf(pcb, " $%s %u", symname(pic->id), pic->arg2.u); 
      else chbputf(pcb, " offset=%llu align=%u", pic->arg.u, 1<<(int)pic->arg2.u); 
      break;
    case INSIG_MEMARG_LANEIDX: {
      chbputf(pcb, " offset=%llu align=%u", pic->arg2.ux2[0], 1<<(int)pic->arg2.ux2[1]); 
      if (pic->id) chbputf(pcb, " $%s", symname(pic->id)); 
      else chbputf(pcb, " %llu", pic->arg.u); 
    } break;      
    case INSIG_LANEIDX16: {
      int i; unsigned long long lidxv;
      if (pic->id) chbputf(pcb, " $%s", symname(pic->id)); 
      else for (lidxv = pic->arg.u, i = 16-1; i >= 0; --i) {
        unsigned off = i*4, lidx = (unsigned)((lidxv >> off) & 0xFULL);
        chbputf(pcb, " %u", lidx); 
      } 
    } break;      
    case INSIG_LS_L:
      chbputf(pcb, " (; see next %llu+1 branches ;)", pic->arg.u);
      break; /* takes multiple inscodes, should be taken care of by caller */
    case INSIG_RD: /* IN_REF_DATA -- not in WASM! */ 
      chbputf(pcb, " $%s:%s", symname(pic->arg2.mod), symname(pic->id));
      if (pic->arg.i != 0) chbputf(pcb, " offset=%lld", pic->arg.i); 
      break;
    case INSIG_PR: /* IN_DATA_PUT_REF -- not in WASM! */
      assert(!pic->arg2.mod && !pic->id);
      chbputf(pcb, " offset=%llu", pic->arg.u);
      break;
    default:
      assert(false);     
  }
  return chbdata(pcb);
}

char *format_funcsig(funcsig_t *pfs, chbuf_t *pcb)
{
  size_t i;
  chbclear(pcb);
  chbputs("(param", pcb);
  for (i = 0; i < vtblen(&pfs->partypes); ++i)
    chbputf(pcb, " %s", valtype_name(*vtbref(&pfs->partypes, i)));
  chbputs(") (result", pcb);
  for (i = 0; i < vtblen(&pfs->restypes); ++i)
    chbputf(pcb, " %s", valtype_name(*vtbref(&pfs->restypes, i)));
  chbputs(")", pcb);
  return chbdata(pcb);
}

/* wat text representations */

watie_t* watieinit(watie_t* pie, iekind_t iek)
{
  memset(pie, 0, sizeof(watie_t));
  bufinit(&pie->data, sizeof(char));
  fsinit(&pie->fs);
  icbinit(&pie->code);
  bufinit(&pie->table, sizeof(dpme_t));
  pie->iek = iek;
  return pie;
}

void watiefini(watie_t* pie)
{
  buffini(&pie->data);
  fsfini(&pie->fs);
  icbfini(&pie->code);
  buffini(&pie->table);
}

void watiebfini(watiebuf_t* pb)
{
  watie_t *pie; size_t i;
  assert(pb); assert(pb->esz == sizeof(watie_t));
  pie = (watie_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) watiefini(pie+i);
  buffini(pb);
}

void watiebdel(watiebuf_t* pb, iekind_t iek, sym_t mod, sym_t id)
{
  watie_t *pie; size_t i;
  assert(pb); assert(pb->esz == sizeof(watie_t));
  pie = (watie_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) {
    watie_t *piei = pie+i;
    if (piei->iek == iek && piei->mod == mod && piei->id == id) {
      watiefini(piei); bufrem(pb, i);
      return;
    }
  }
}

wat_module_t* wat_module_init(wat_module_t* pm)
{
  memset(pm, 0, sizeof(wat_module_t));
  watiebinit(&pm->imports);
  watiebinit(&pm->exports);
  return pm;
}

void wat_module_fini(wat_module_t* pm)
{
  watiebfini(&pm->imports);
  watiebfini(&pm->exports);
}

void wat_module_clear(wat_module_t* pm)
{
  wat_module_fini(pm);
  wat_module_init(pm);
}

void wat_module_buf_fini(wat_module_buf_t* pb)
{
  wat_module_t *pm; size_t i;
  assert(pb); assert(pb->esz == sizeof(wat_module_t));
  pm = (wat_module_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) wat_module_fini(pm+i);
  buffini(pb);
}


/* write object wat text module */

static void wat_indent(void)
{
  if (g_watindent) fprintf(g_watout, "%*c", g_watindent, ' ');
}

static void wat_writeln(const char *s)
{
  fputs(s, g_watout);
  fputc('\n', g_watout);
}

static void wat_line(const char *s)
{
  wat_indent();
  wat_writeln(s);
}

static void wat_linef(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  wat_indent();
  vfprintf(g_watout, fmt, args);
  fputc('\n', g_watout);
  va_end(args);
}

static void wat_imports(watiebuf_t *pib)
{
  size_t i, j;
  for (i = 0; i < watieblen(pib); ++i) {
    watie_t *pi = watiebref(pib, i);
    const char *smod = symname(pi->mod), *sname = symname(pi->id);
    chbsetf(g_watbuf, "(import \"%s\" \"%s\" ", smod, sname);
    switch (pi->iek) {
      case IEK_FUNC: {
        chbputf(g_watbuf, "(func $%s:%s", smod, sname);
        for (j = 0; j < vtblen(&pi->fs.partypes); ++j) {
          valtype_t *pvt = vtbref(&pi->fs.partypes, j);
          chbputf(g_watbuf, " (param %s)", valtype_name(*pvt));
        }
        for (j = 0; j < vtblen(&pi->fs.restypes); ++j) {
          valtype_t *pvt = vtbref(&pi->fs.restypes, j);
          chbputf(g_watbuf, " (result %s)", valtype_name(*pvt));
        }
        chbputc(')', g_watbuf); 
      } break;
      case IEK_DATA: {
        char *vcs = (pi->mut == MT_VAR) ? "var" : "const";
        chbputf(g_watbuf, "(data $%s:%s %s", smod, sname, vcs);
        chbputf(g_watbuf, " align=%d", pi->align);
        chbputf(g_watbuf, " size=%u", (unsigned)pi->n);
        chbputc(')', g_watbuf); 
      } break;
      case IEK_MEM: {
        chbputf(g_watbuf, "(memory $%s:%s", smod, sname);
        if (pi->lt == LT_MIN) chbputf(g_watbuf, " %u)", pi->n);
        else chbputf(g_watbuf, " %u %u)", pi->n, pi->m);
      } break;
      case IEK_GLOBAL: {
        chbputf(g_watbuf, "(global $%s:%s", smod, sname);
        if (pi->mut == MT_CONST) {
          chbputf(g_watbuf, " %s)", valtype_name(pi->vt));
        } else {
          chbputf(g_watbuf, " (mut %s))", valtype_name(pi->vt));
        }
      } break;
      default:;
    }
    chbputc(')', g_watbuf); 
    wat_line(chbdata(g_watbuf));
  }
}

static void wat_export_mems(watiebuf_t *peb)
{
  size_t i;
  for (i = 0; i < watieblen(peb); ++i) {
    watie_t *pe = watiebref(peb, i);
    if (pe->iek == IEK_MEM) {
      const char *smod = symname(pe->mod), *sname = symname(pe->id);
      chbsetf(g_watbuf, "(memory $%s:%s", smod, sname);
      if (pe->exported) chbputf(g_watbuf, " (export \"%s\")", symname(pe->id));
      if (pe->lt == LT_MIN) chbputf(g_watbuf, " %u", pe->n);
      else chbputf(g_watbuf, " %u %u", pe->n, pe->m);
      chbputc(')', g_watbuf); 
      wat_line(chbdata(g_watbuf));
    }
  }
}

static void wat_export_globals(watiebuf_t *peb)
{
  size_t i;
  for (i = 0; i < watieblen(peb); ++i) {
    watie_t *pe = watiebref(peb, i);
    if (pe->iek == IEK_GLOBAL) {
      const char *smod = symname(pe->mod), *sname = symname(pe->id);
      chbsetf(g_watbuf, "(global $%s:%s", smod, sname);
      if (pe->exported) chbputf(g_watbuf, " (export \"%s\")", symname(pe->id));
      if (pe->mut == MT_CONST) chbputf(g_watbuf, " %s", valtype_name(pe->vt));
      else chbputf(g_watbuf, " (mut %s)", valtype_name(pe->vt));
      if (pe->ic.in != 0) {
        chbuf_t cb = mkchb();
        chbputf(g_watbuf, " (%s)", format_inscode(&pe->ic, &cb));
        chbfini(&cb);
      }
      chbputc(')', g_watbuf); 
      wat_line(chbdata(g_watbuf));
    }
  }
}

static bool data_all_zeroes(const char *pc, size_t n)
{
  while (n-- > 0) if (*pc != 0) return false; else ++pc;
  return true;
}

static void wat_export_datas(watiebuf_t *pdb)
{
  size_t i;
  for (i = 0; i < watieblen(pdb); ++i) {
    watie_t *pd = watiebref(pdb, i);
    if (pd->iek == IEK_DATA) { 
      const char *smod = symname(pd->mod), *sname = symname(pd->id);
      unsigned char *pc = pd->data.buf; size_t i, n = pd->data.fill;
      if (smod != NULL && sname != NULL) { /* symbolic segment (nonfinal) */
        char *vcs = (pd->mut == MT_VAR) ? "var" : "const";
        assert(pd->align); assert(pd->ic.id == 0);
        chbsetf(g_watbuf, "(data $%s:%s", smod, sname);
        if (pd->exported) chbputf(g_watbuf, " (export \"%s\")", symname(pd->id));
        chbputf(g_watbuf, " %s align=%d", vcs, pd->align);
      } else { /* positioned segment (final, WAT-compatible) */
        chbuf_t cb = mkchb();
        assert(!pd->align); assert(pd->ic.in == IN_I32_CONST);
        chbsetf(g_watbuf, "(data (%s)", format_inscode(&pd->ic, &cb));
        chbfini(&cb);
      }
      if (smod != NULL && sname != NULL && data_all_zeroes((char *)pc, n)) {
        chbputf(g_watbuf, " size=%z", n); /* .wo shortcut */
      } else {  
        chbputs(" \"", g_watbuf);
        for (i = 0; i < n; ++i) {
          unsigned c = pc[i]; char ec = 0;
          switch ((int)c) {
            case 0x09: ec = 't';  break; 
            case 0x0A: ec = 'n';  break;
            case 0x0D: ec = 'r';  break;
            case 0x22: ec = '\"'; break;
            case 0x27: ec = '\''; break;
            case 0x5C: ec = '\\'; break;
          }
          if (ec) chbputf(g_watbuf, "\\%c", (int)ec);
          else if (' ' <= c && c < 127) chbputc(c, g_watbuf); 
          else chbputf(g_watbuf, "\\%x%x", (c>>4)&0xF, c&0xF);
        }
        chbputc('\"', g_watbuf); 
      }
      if (icblen(&pd->code) == 0) { /* leaf segment */
        chbputc(')', g_watbuf); 
        wat_line(chbdata(g_watbuf));
      } else { /* WCPL data segment with ref slots */
        size_t j;
        assert(smod != NULL && sname != NULL); /* not in standard WAT! */
        wat_line(chbdata(g_watbuf));
        g_watindent += 2;
        for (j = 0; j < icblen(&pd->code); ++j) {
          inscode_t *pic = icbref(&pd->code, j);
          assert(pic->in == IN_REF_DATA || pic->in == IN_DATA_PUT_REF);
          format_inscode(pic, g_watbuf);
          chbinsc(g_watbuf, 0, j == 0 ? '(' : ' ');
          if (j+1 == icblen(&pd->code)) chbputs("))", g_watbuf);
          wat_line(chbdata(g_watbuf));
        }
        g_watindent -= 2;
      }
    }
  }
}

static void wat_export_tables(watiebuf_t *pfb)
{
  size_t i, j, tc = 0;
  for (i = 0; i < watieblen(pfb); ++i) {
    watie_t *pt = watiebref(pfb, i);
    size_t tlen;
    if (pt->iek != IEK_TABLE) continue;
    tlen = buflen(&pt->table) + 1; /* elem 0 is reserved */
    chbsetf(g_watbuf, "(table %d %d funcref)", (int)tlen, (int)tlen);
    wat_line(chbdata(g_watbuf));
    chbsetf(g_watbuf, "(elem (i32.const 1)");
    for (j = 0; j < buflen(&pt->table); ++j) {
      dpme_t *pe = bufref(&pt->table, j);
      chbputf(g_watbuf, " $%s:%s", symname(pe->mod), symname(pe->id));
    }
    chbputf(g_watbuf, ")");
    wat_line(chbdata(g_watbuf));
  }
}

static void wat_export_funcs(watiebuf_t *pfb)
{
  size_t i, j, k;
  for (i = 0; i < watieblen(pfb); ++i) {
    watie_t *pf = watiebref(pfb, i);
    if (pf->iek != IEK_FUNC) continue;
    chbsetf(g_watbuf, "(func $%s:%s", symname(pf->mod), symname(pf->id));
    if (pf->exported) chbputf(g_watbuf, " (export \"%s\")", symname(pf->id));
    wat_line(chbdata(g_watbuf)); chbclear(g_watbuf);
    g_watindent += 2;
    for (j = 0; j < vtblen(&pf->fs.partypes); ++j) {
      valtype_t *pvt = vtbref(&pf->fs.partypes, j);
      inscode_t *pic = icbref(&pf->code, j);
      assert(pic->in == IN_REGDECL);
      assert(*pvt == (valtype_t)pic->arg.u);
      if (!pic->id) chbputf(g_watbuf, "(param %s) ", valtype_name((valtype_t)pic->arg.u));
      else chbputf(g_watbuf, "(param $%s %s) ", symname(pic->id), valtype_name((valtype_t)pic->arg.u));
    }
    for (k = 0; k < vtblen(&pf->fs.restypes); ++k) {
      valtype_t *pvt = vtbref(&pf->fs.restypes, k);
      chbputf(g_watbuf, "(result %s) ", valtype_name(*pvt));
    }
    if (chblen(g_watbuf) > 0) { wat_line(chbdata(g_watbuf)); chbclear(g_watbuf); }
    for (/* use current j */; j < icblen(&pf->code); ++j) {
      inscode_t *pic = icbref(&pf->code, j);
      if (pic->in != IN_REGDECL) break;
      chbputf(g_watbuf, "(local $%s %s) ", symname(pic->id), valtype_name((valtype_t)pic->arg.u));
    }
    if (chblen(g_watbuf) > 0) wat_line(chbdata(g_watbuf));
    /* code */
    for (/* use current j */; j < icblen(&pf->code); ++j) {
      inscode_t *pic = icbref(&pf->code, j);
      assert(pic->in != IN_REGDECL && pic->in != IN_PLACEHOLDER);
      if (pic->in == IN_BR_TABLE) { /* takes multiple inscodes */
        size_t n = (unsigned)pic->arg.u + 1;
        chbsetf(g_watbuf, "br_table"); 
        for (k = j+1; k < j+1+n; ++k) {
          assert(k < icblen(&pf->code));
          pic = icbref(&pf->code, k); assert(pic->in == IN_BR && pic->id);
          chbputf(g_watbuf, " $%s", symname(pic->id));
        }
        wat_line(chbdata(g_watbuf));
        j += n; /* account for n aditional inscodes */
      } else { /* takes single inscode */
        wat_line(format_inscode(pic, g_watbuf));
      }
    }
    g_watindent -= 2;
    wat_line(")");  
  }
}

void write_wat_module(wat_module_t* pm, FILE *pf)
{
  chbuf_t cb;
  g_watout = pf;
  g_watbuf = chbinit(&cb);
  if (!pm->name) wat_linef("(module");
  else wat_linef("(module $%s", symname(pm->name));
  g_watindent += 2;
  wat_imports(&pm->imports);
  wat_export_mems(&pm->exports);
  wat_export_globals(&pm->exports);
  wat_export_datas(&pm->exports);
  wat_export_tables(&pm->exports);
  wat_export_funcs(&pm->exports);
  wat_writeln(")");
  g_watout = NULL;
  chbfini(&cb);
  g_watbuf = NULL;
}


/* read object wat text module */

/* wasm token types */
typedef enum wt_tag {
  WT_WHITESPACE, WT_LC, WT_BC,
  WT_BCSTART, WT_IDCHARS, /* internal */
  WT_KEYWORD, WT_ID, WT_RESERVED,
  WT_INT, WT_FLOAT, WT_STRING,
  WT_LPAR, WT_RPAR, WT_EOF = -1
} wt_t;

/* scanner workspace */
typedef struct sws {
  dstr_t infile;      /* current input file name or "-" */
  FILE *input;        /* current input stream */
  int lno;            /* current line no. (0-based) */
  bool inateof;       /* current input is exausted */  
  chbuf_t chars;      /* line buffer of chars */
  int curi;           /* current input position in line */
  bool gottk;         /* lookahead token is available */
  wt_t ctk;           /* lookahead token type */
  chbuf_t token;      /* lookahead token char data */
  char *tokstr;       /* lookahead token string */
  int posl;           /* lno of la token start */
  int posi;           /* posi of la token start */
} sws_t;

/* alloc scanner workspace for infile */
sws_t *newsws(const char *infile)
{
  FILE *fp;
  fp = streql(infile, "-") ? stdin : fopen(infile, "r");
  if (fp) {
    sws_t *pw = exmalloc(sizeof(sws_t));
    pw->infile = exstrdup(infile);
    pw->input = fp;
    pw->lno = 0;
    pw->inateof = false;
    bufinit(&pw->chars, sizeof(char));
    fget8bom(pw->input);
    pw->curi = 0; 
    pw->gottk = false;
    pw->ctk = WT_EOF;
    chbinit(&pw->token);
    pw->tokstr = NULL;
    pw->posl = 0;
    pw->posi = 0;
    return pw;
  }
  return NULL;
}

static void freesws(sws_t *pw)
{
  if (pw) {
    free(pw->infile);
    if (pw->input && pw->input != stdin) fclose(pw->input);
    chbfini(&pw->chars);
    chbfini(&pw->token);
    free(pw);
  }
}

/* try to get new line of input so parsing can continue */
static int fetchline(sws_t *pw, char **ptbase, int *pendi)
{
  chbuf_t *pp = &pw->chars; int c = EOF;
  chbclear(pp);
  if (!pw->inateof) {
    char *line = fgetlb(pp, pw->input);
    if (!line) {
      pw->inateof = true;
    } else {
      chbputc('\n', pp);
      c = '\n';
      pw->lno += 1;
      pw->curi = 0;
      *ptbase = chbdata(pp);
      *pendi = (int)chblen(pp);
    }
  }
  return c;  
}

/* lexer: splits input into lexemes delivered via char queue */
#define readchar() (c = *pcuri < endi ? tbase[(*pcuri)++] : fetchline(pw, &tbase, &endi))
#define unreadchar() ((*pcuri)--)
#define state_10d 46
#define state_10dd 47
#define state_10ddd 48
#define state_10dddd 49
#define state_10ddddd 50
static wt_t lex(sws_t *pw, chbuf_t *pcb)
{
  char *tbase = chbdata(&pw->chars);
  int *pcuri = &pw->curi;
  int endi = (int)chblen(&pw->chars);
  int c, state = 0;
  chbclear(pcb);
  while (true) {
    switch (state) {
      case 0: 
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == '\"') {
          chbputc(c, pcb);
          state = 9; continue;
        } else if (c == '0') {
          chbputc(c, pcb);
          state = 8; continue;
        } else if ((c >= '1' && c <= '9')) {
          chbputc(c, pcb);
          state = 7; continue;
        } else if (c == '+' || c == '-') {
          chbputc(c, pcb);
          state = 6; continue;
        } else if (c == '!' || (c >= '#' && c <= '\'') || c == '*' || (c >= '.' && c <= '/') || c == ':' || (c >= '<' && c <= 'Z') 
                || c == '\\' || (c >= '^' && c <= 'z') || c == '|' || c == '~') {
          chbputc(c, pcb);
          state = 5; continue;
        } else if (c == ')') {
          chbputc(c, pcb);
          state = 4; continue;
        } else if (c == '(') {
          chbputc(c, pcb);
          state = 3; continue;
        } else if (c == ';') {
          chbputc(c, pcb);
          state = 2; continue;
        } else if ((c >= '\t' && c <= '\n') || (c >= '\f' && c <= '\r') || c == ' ') {
          chbputc(c, pcb);
          state = 1; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 1:
        readchar();
        if (c == EOF) {
          return WT_WHITESPACE;
        } else if ((c >= '\t' && c <= '\n') || (c >= '\f' && c <= '\r') || c == ' ') {
          chbputc(c, pcb);
          state = 1; continue;
        } else {
          unreadchar();
          return WT_WHITESPACE;
        }
      case 2:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == ';') {
          chbputc(c, pcb);
          state = 43; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 3:
        readchar();
        if (c == EOF) {
          return WT_LPAR;
        } else if (c == ';') {
          chbputc(c, pcb);
          state = 42; continue;
        } else {
          unreadchar();
          return WT_LPAR;
        }
      case 4:
        return WT_RPAR;
      case 5:
        readchar();
        if (c == EOF) {
          return WT_IDCHARS;
        } else if (c == '!' || (c >= '#' && c <= '\'') || (c >= '*' && c <= ':') || (c >= '<' && c <= 'Z') || c == '\\' 
                || (c >= '^' && c <= 'z') || c == '|' || c == '~') {
          chbputc(c, pcb);
          state = 5; continue;
        } else {
          unreadchar();
          return WT_IDCHARS;
        }
      case 6:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == 'i') {
          chbputc(c, pcb);
          state = 38; continue;
        } else if (c == 'n') {
          chbputc(c, pcb);
          state = 37; continue;
        } else if (c == '0') {
          chbputc(c, pcb);
          state = 8; continue;
        } else if ((c >= '1' && c <= '9')) {
          chbputc(c, pcb);
          state = 7; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 7:
        readchar();
        if (c == EOF) {
          return WT_INT;
        } else if (c == '.') {
          chbputc(c, pcb);
          state = 22; continue;
        } else if (c == 'E' || c == 'e') {
          chbputc(c, pcb);
          state = 21; continue;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 7; continue;
        } else {
          unreadchar();
          return WT_INT;
        }
      case 8:
        readchar();
        if (c == EOF) {
          return WT_INT;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 23; continue;
        } else if (c == '.') {
          chbputc(c, pcb);
          state = 22; continue;
        } else if (c == 'E' || c == 'e') {
          chbputc(c, pcb);
          state = 21; continue;
        } else if (c == 'X' || c == 'x') {
          chbputc(c, pcb);
          state = 20; continue;
        } else {
          unreadchar();
          return WT_INT;
        }
      case 9:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == '\"') {
          chbputc(c, pcb);
          state = 13; continue;
        } else if (c == '\\') {
          chbputc(c, pcb);
          state = 12; continue;
        } else if (is8chead(c)) { 
          int u = c & 0xFF;
          if (u < 0xE0) { chbputc(c, pcb); state = state_10d; continue; }
          if (u < 0xF0) { chbputc(c, pcb); state = state_10dd; continue; }
          if (u < 0xF8) { chbputc(c, pcb); state = state_10ddd; continue; }
          if (u < 0xFC) { chbputc(c, pcb); state = state_10dddd; continue; }
          if (u < 0xFE) { chbputc(c, pcb); state = state_10ddddd; continue; }
          unreadchar();
          return WT_EOF;
        } else if (!(c == '\n' || c == '\"' || c == '\\')) {
          chbputc(c, pcb);
          state = 9; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case state_10ddddd:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (is8ctail(c)) {
          chbputc(c, pcb);
          state = state_10dddd; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case state_10dddd:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (is8ctail(c)) {
          chbputc(c, pcb);
          state = state_10ddd; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case state_10ddd:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (is8ctail(c)) {
          chbputc(c, pcb);
          state = state_10dd; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case state_10dd:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (is8ctail(c)) {
          chbputc(c, pcb);
          state = state_10d; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case state_10d:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (is8ctail(c)) {
          chbputc(c, pcb);
          state = 9; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 12:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == 'u') {
          chbputc(c, pcb);
          state = 16; continue;
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          unreadchar();
          state = 15; continue;
        } else if ((c >= '\t' && c <= '\n') || c == '\r' || c == ' ') {
          chbputc(c, pcb);
          state = 14; continue;
        } else if (c == '\"' || c == '\'' || c == '\\' || c == 'n' || c == 'r' || c == 't') {
          chbputc(c, pcb);
          state = 9; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 13:
        return WT_STRING;
      case 14:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '\t' && c <= '\n') || c == '\r' || c == ' ') {
          chbputc(c, pcb);
          state = 14; continue;
        } else if (c == '\\') {
          chbputc(c, pcb);
          state = 9; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 15:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          chbputc(c, pcb);
          state = 19; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 16:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == '{') {
          chbputc(c, pcb);
          state = 17; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 17:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          chbputc(c, pcb);
          state = 18; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 18:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          chbputc(c, pcb);
          state = 18; continue;
        } else if (c == '}') {
          chbputc(c, pcb);
          state = 9; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 19:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          chbputc(c, pcb);
          state = 9; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 20:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          chbputc(c, pcb);
          state = 29; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 21:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 28; continue;
        } else if (c == '+' || c == '-') {
          chbputc(c, pcb);
          state = 27; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 22:
        readchar();
        if (c == EOF) {
          return WT_FLOAT;
        } else if (c == 'E' || c == 'e') {
          chbputc(c, pcb);
          state = 24; continue;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 22; continue;
        } else {
          unreadchar();
          return WT_FLOAT;
        }
      case 23:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 23; continue;
        } else if (c == '.') {
          chbputc(c, pcb);
          state = 22; continue;
        } else if (c == 'E' || c == 'e') {
          chbputc(c, pcb);
          state = 21; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 24:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 26; continue;
        } else if (c == '+' || c == '-') {
          chbputc(c, pcb);
          state = 25; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 25:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 26; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 26:
        readchar();
        if (c == EOF) {
          return WT_FLOAT;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 26; continue;
        } else {
          unreadchar();
          return WT_FLOAT;
        }
      case 27:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 28; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 28:
        readchar();
        if (c == EOF) {
          return WT_FLOAT;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 28; continue;
        } else {
          unreadchar();
          return WT_FLOAT;
        }
      case 29:
        readchar();
        if (c == EOF) {
          return WT_INT;
        } else if (c == 'P' || c == 'p') {
          chbputc(c, pcb);
          state = 31; continue;
        } else if (c == '.') {
          chbputc(c, pcb);
          state = 30; continue;
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          chbputc(c, pcb);
          state = 29; continue;
        } else {
          unreadchar();
          return WT_INT;
        }
      case 30:
        readchar();
        if (c == EOF) {
          return WT_FLOAT;
        } else if (c == 'P' || c == 'p') {
          chbputc(c, pcb);
          state = 34; continue;
        } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
          chbputc(c, pcb);
          state = 30; continue;
        } else {
          unreadchar();
          return WT_FLOAT;
        }
      case 31:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 33; continue;
        } else if (c == '+' || c == '-') {
          chbputc(c, pcb);
          state = 32; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 32:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 33; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 33:
        readchar();
        if (c == EOF) {
          return WT_FLOAT;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 33; continue;
        } else {
          unreadchar();
          return WT_FLOAT;
        }
      case 34:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 36; continue;
        } else if (c == '+' || c == '-') {
          chbputc(c, pcb);
          state = 35; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 35:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 36; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 36:
        readchar();
        if (c == EOF) {
          return WT_FLOAT;
        } else if ((c >= '0' && c <= '9')) {
          chbputc(c, pcb);
          state = 36; continue;
        } else {
          unreadchar();
          return WT_FLOAT;
        }
      case 37:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == 'a') {
          chbputc(c, pcb);
          state = 41; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 38:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == 'n') {
          chbputc(c, pcb);
          state = 39; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 39:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == 'f') {
          chbputc(c, pcb);
          state = 40; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 40:
        return WT_FLOAT;
      case 41:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (c == 'n') {
          chbputc(c, pcb);
          state = 40; continue;
        } else {
          unreadchar();
          return WT_EOF;
        }
      case 42:
        return WT_BCSTART;
      case 43:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (!(c == '\n')) {
          chbputc(c, pcb);
          state = 44; continue;
        } else {
          chbputc(c, pcb);
          state = 45; continue;
        }
      case 44:
        readchar();
        if (c == EOF) {
          return WT_EOF;
        } else if (!(c == '\n')) {
          chbputc(c, pcb);
          state = 44; continue;
        } else {
          chbputc(c, pcb);
          state = 45; continue;
        }
      case 45:
        return WT_LC;
      default:
        assert(false);
    }
  }
  return WT_EOF;
}
#undef state_10d
#undef state_10dd
#undef state_10ddd
#undef state_10dddd
#undef state_10ddddd
#undef readchar
#undef unreadchar


/* report error, possibly printing location information */
static void vseprintf(sws_t *pw, const char *fmt, va_list args)
{
  chbuf_t cb = mkchb(); const char *s;
  int ln = 0, off = 0; assert(fmt);
  if (pw != NULL && pw->infile != NULL) chbputf(&cb, "%s:", pw->infile);
  if (pw != NULL) {
    ln = pw->posl, off = pw->posi;
    if (ln > 0) chbputf(&cb, "%d:%d:", ln, off+1);
  }
  fflush(stdout); 
  if (chblen(&cb) > 0) {
    chbputc(' ', &cb);
    fputs(chbdata(&cb), stderr);
  }
  vfprintf(stderr, fmt, args); 
  fputc('\n', stderr);
  if (pw && ln > 0 && (s = chbdata(&pw->chars)) != NULL) {
    fputs(s, stderr); fputc('\n', stderr);
    while (off-- > 0) fputc(' ', stderr);
    fputs("^\n", stderr);
  }
  fflush(stderr); 
  chbfini(&cb);
}

/* report error, possibly printing location information, and exit */
void seprintf(sws_t *pw, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt); 
  vseprintf(pw, fmt, args); 
  va_end(args); 
  exit(1);
}

#define readchar() (c = *pcuri < endi ? tbase[(*pcuri)++] : fetchline(pw, &tbase, &endi))
static bool lexbc(sws_t *pw, chbuf_t *pcb) 
{
  char *tbase = chbdata(&pw->chars);
  int *pcuri = &pw->curi;
  int endi = (int)chblen(&pw->chars);
  int c;
  while (true) {
    readchar();
    if (c == EOF) break;
    chbputc(c, pcb);
    if (c == '(') {
      readchar();
      if (c == EOF) break;
      chbputc(c, pcb);
      if (c == ';') {
        if (!lexbc(pw, pcb)) return false;
      }      
    } else if (c == ';') {
      readchar();
      if (c == EOF) break;
      chbputc(c, pcb);
      if (c == ')') return true;
    }
  }
  return false;
}
#undef readchar

static wt_t peekt(sws_t *pw) 
{ 
  if (!pw->gottk) {
    do { /* fetch next non-whitespace token */
      pw->posl = pw->lno, pw->posi = pw->curi; 
      pw->ctk = lex(pw, &pw->token);
      if (pw->ctk == WT_EOF && !pw->inateof) {
        seprintf(pw, "illegal token"); 
      }
      switch (pw->ctk) {
        case WT_IDCHARS: {
          char *s = chbdata(&pw->token);
          if (*s == '$') pw->ctk = WT_ID;
          else if ('a' <= *s && *s <= 'z') pw->ctk = WT_KEYWORD; 
          else pw->ctk = WT_RESERVED;
        } break;
        case WT_BCSTART: {
          pw->ctk = WT_EOF;
          if (lexbc(pw, &pw->token)) pw->ctk = WT_BC;
          else seprintf(pw, "invalid block comment");
        };
        default:; 
      }
    } while (WT_WHITESPACE <= pw->ctk && pw->ctk <= WT_BC); 
    pw->gottk = true; 
    pw->tokstr = chbdata(&pw->token); 
  } 
  return pw->ctk; 
}

static bool peeks(sws_t *pw, const char *s)
{
  return strprf(chbdata(&pw->chars)+pw->curi, s) != NULL;
}

static void dropt(sws_t *pw) 
{ 
  pw->gottk = false; 
} 

static bool ahead(sws_t *pw, const char *ts)
{
  if (peekt(pw) == WT_EOF) return false;
  return streql(ts, pw->tokstr);
} 

static void expect(sws_t *pw, const char *ts)
{
  if (ahead(pw, ts)) dropt(pw);
  else seprintf(pw, "expected: %s, got %s", ts, pw->tokstr);
} 


static void scan_integer(sws_t *pw, const char *s, numval_t *pv)
{
  char *e; errno = 0;
  if (*s == '-') pv->i = strtoll(s, &e, 0); /* reads -0x... syntax too */
  else pv->u = strtoull(s, &e, 0); /* reads hex 0x... syntax too */
  if (errno || *e != 0) seprintf(pw, "invalid integer literal");
}

static float scan_float(sws_t *pw, const char *s)
{
  unsigned u = hextouf(s);
  if (u == (unsigned)-1) seprintf(pw, "invalid float literal");
  return as_float(u);
}

static double scan_double(sws_t *pw, const char *s)
{
  unsigned long long u = hextoud(s);
  if (u == (unsigned long long)-1LL) seprintf(pw, "invalid double literal");
  return as_double(u);
}

/* size of buffer large enough to hold char escapes */
#define SBSIZE 32
/* convert a single WAT text escape sequence (-1 on error) */
static unsigned long strtowatec(const char *s, char** ep)
{
  char buf[SBSIZE+1]; int c; unsigned long l;
  assert(s);
  if (*s) {
    c = *s++;
    if (c == '\\') {
      switch (c = *s++) {
        case 't':  l = 0x09; break;
        case 'n':  l = 0x0A; break;
        case 'r':  l = 0x0D; break;
        case '\\': l = 0x5C; break;
        case '\'': l = 0x27; break;
        case '\"': l = 0x22; break;
        case 'x': {
          int i = 0; 
          if (*s++ != '{') goto err;
          while (i < SBSIZE && (c = *s++) && isxdigit(c))
            buf[i++] = c;
          if (i == SBSIZE) goto err;
          buf[i] = 0; 
          l = strtoul(buf, NULL, 16);
          if (c != '}') goto err;
        } break;
        default:
          goto err;
      }
    } else {
      l = c & 0xff;
    }
    if (ep) *ep = (char*)s; 
    return l;
    err:;
  }
  if (ep) *ep = (char*)s;
  errno = EINVAL;
  return (unsigned long)(-1);
}

static char *scan_string(sws_t *pw, const char *s, chbuf_t *pcb)
{
  int c = *s++; assert(c == '"');
  chbclear(pcb);
  if (*s) {
    while (*s != '"') {
      c = *s; errno = 0;
      if (!c || is8ctail(c)) goto err;
      if (is8chead(c)) { 
        char *e; unsigned long ul = strtou8c(s, &e); 
        if (errno || ul > 0x10FFFFUL) goto err;
        chbput(s, e-s, pcb); /* as-is! */
        s = e;
      } else if (c == '\\' && isxdigit(s[1]) && isxdigit(s[2])) {
        unsigned b, h; 
        c = tolower(s[1]); h = ('0' <= c && c <= '9') ? c-'0' : 10+c-'a'; b = h << 4;
        c = tolower(s[2]); h = ('0' <= c && c <= '9') ? c-'0' : 10+c-'a'; b = b | h;
        chbputc(b, pcb);
        s += 3;
      } else if (c == '\\') {
        char *e; unsigned long ul = strtowatec(s, &e);
        if (errno || ul > 0x10FFFFUL) goto err;
        chbputlc(ul, pcb); /* in utf-8 */
        s = e;
      } else {
        chbputc(c, pcb);
        s += 1;
      }
    }
    c = *++s; assert(c == 0);
    return chbdata(pcb);
  err:;
  }
  seprintf(pw, "invalid string literal");
  return NULL;
}

static void parse_mod_id(sws_t *pw, sym_t *pmid, sym_t *pid)
{
  char *s, *sep;
  if (g_watexec && peekt(pw) != WT_ID) {
    *pmid = *pid = 0;
  } else if (peekt(pw) != WT_ID || (sep = strchr((s = pw->tokstr), ':')) == NULL) { 
    seprintf(pw, "expected $mod:id, got %s", pw->tokstr);
  } else {
    chbuf_t cb = mkchb();
    assert(*s == '$'); ++s;
    *pmid = intern(chbset(&cb, s, sep-s));
    *pid = intern(sep+1);
    chbfini(&cb);
    dropt(pw);
  }
} 

static sym_t parse_id(sws_t *pw)
{
  char *s, *sep; 
  if (peekt(pw) != WT_ID || (sep = strchr((s = pw->tokstr), ':')) != NULL) { 
    seprintf(pw, "expected plain id $id, got %s", pw->tokstr);
  } else {
    sym_t id;
    assert(*s == '$');
    id = intern(s+1);
    dropt(pw);
    return id;
  }
  return 0; /* never */
}

static sym_t parse_id_string(sws_t *pw, chbuf_t *pcb, const char *chset)
{
  char *s = NULL; size_t n; bool ok = false;
  ok = (peekt(pw) == WT_STRING);
  if (ok) ok = (s = scan_string(pw, pw->tokstr, pcb)) != NULL;
  if (ok) ok = (n = strlen(s)) == chblen(pcb); /* no \00s inside */
  if (ok) ok = strspn(s, chset) == n;
  if (ok) { dropt(pw); return intern(s); }
  else seprintf(pw, "expected \"id\", got %s", pw->tokstr);
  return 0; /* never */
}

static valtype_t parse_valtype(sws_t *pw)
{
  if (peekt(pw) == WT_KEYWORD) {
    valtype_t vt = VT_UNKN;
    if (streql(pw->tokstr, "f64")) vt = VT_F64;
    else if (streql(pw->tokstr, "f32")) vt = VT_F32;
    else if (streql(pw->tokstr, "i64")) vt = VT_I64;
    else if (streql(pw->tokstr, "i32")) vt = VT_I32;
    /* else if (streql(pw->tokstr, "funcref")) vt = RT_FUNCREF; */
    /* else if (streql(pw->tokstr, "externref")) vt = RT_EXTERNREF; */
    if (vt != VT_UNKN) { dropt(pw); return vt; }
  } 
  seprintf(pw, "unexpected value type");
  return VT_UNKN; /* never */
} 

static valtype_t parse_blocktype(sws_t *pw)
{
  valtype_t vt = BT_VOID;
  if (ahead(pw, "(")) {
    dropt(pw);
    expect(pw, "result");
    vt = parse_valtype(pw);
    expect(pw, ")");
  }
  return vt;
}

static void parse_funcsig(sws_t *pw, funcsig_t *pfs)
{
  expect(pw, "(");
  expect(pw, "param");
  while (!ahead(pw, ")")) *vtbnewbk(&pfs->partypes) = parse_valtype(pw);
  expect(pw, ")");
  expect(pw, "(");
  expect(pw, "result");
  while (!ahead(pw, ")")) *vtbnewbk(&pfs->restypes) = parse_valtype(pw);
  expect(pw, ")");
}

static unsigned parse_uint(sws_t *pw)
{
  numval_t v;
  if (peekt(pw) == WT_INT) {
    scan_integer(pw, pw->tokstr, &v); dropt(pw);
    if (((v.u << 32) >> 32) != v.u)
      seprintf(pw, "u32 arg is out of range %s", pw->tokstr);
  } else {
    seprintf(pw, "invalid unsigned argument %s", pw->tokstr);
  }
  return (unsigned)v.u;
}

static unsigned parse_offset(sws_t *pw)
{
  char *s;
  if (peekt(pw) == WT_KEYWORD && (s = strprf(pw->tokstr, "offset=")) != NULL) {
    char *e; unsigned long offset; errno = 0; offset = strtoul(s, &e, 10); 
    if (errno || *e != 0 || offset > UINT_MAX) 
      seprintf(pw, "invalid offset= argument");
    dropt(pw);
    return (unsigned)offset;
  } else {
    seprintf(pw, "missing offset= argument");
  }
  return 0; /* won't happen */
}

static unsigned parse_size(sws_t *pw)
{
  char *s;
  if (peekt(pw) == WT_KEYWORD && (s = strprf(pw->tokstr, "size=")) != NULL) {
    char *e; unsigned long size; errno = 0; size = strtoul(s, &e, 10); 
    if (errno || *e != 0 || size > UINT_MAX) 
      seprintf(pw, "invalid size= argument");
    dropt(pw);
    return (unsigned)size;
  } else {
    seprintf(pw, "missing size= argument");
  }
  return 0; /* won't happen */
}

static unsigned parse_align(sws_t *pw)
{
  char *s;
  if (peekt(pw) == WT_KEYWORD && (s = strprf(pw->tokstr, "align=")) != NULL) {
    char *e; unsigned long align; errno = 0; align = strtoul(s, &e, 10); 
    if (errno || *e != 0 || (align != 1 && align != 2 && align != 4 && align != 8 && align != 16)) 
      seprintf(pw, "invalid align= argument");
    dropt(pw);
    return (unsigned)align;
  } else {
    seprintf(pw, "missing align= argument");
  }
  return 0; /* won't happen */
}

static unsigned parse_laneidx(sws_t *pw)
{
  unsigned ui = parse_uint(pw);
  if (ui > 16) seprintf(pw, "lineidx arg is out of range: %u", ui);
  return ui;
}

static void parse_ins(sws_t *pw, inscode_t *pic, icbuf_t *pexb)
{
  if (peekt(pw) == WT_KEYWORD) {
    instr_t in = name_instr(pw->tokstr); insig_t is;
    if (in != IN_PLACEHOLDER) dropt(pw);
    else seprintf(pw, "unknown opcode %s", pw->tokstr);
    pic->in = in;
    switch (is = instr_sig(in)) {
      case INSIG_NONE: { /* no code args */
        if (in == IN_END && peekt(pw) == WT_ID) {
          pic->id = parse_id(pw); /* optional label */
        }
      } break;
      case INSIG_BT: { /* if/block/loop */
        if (in == IN_IF) {
          pic->arg.u = parse_blocktype(pw);
        } else { 
          pic->id = parse_id(pw);
          pic->arg.u = BT_VOID;
        }
      } break;
      case INSIG_L: case INSIG_XL: { /* br/brif, local.xxx */
        pic->id = parse_id(pw);
      } break;  
      case INSIG_XG: { /* call/return_call, global.xxx */
        sym_t mod, id; parse_mod_id(pw, &mod, &id);
        pic->id = id; pic->arg2.mod = mod;
      } break;
      /* fixme: case INSIG_XT: table.xxx */
      case INSIG_CI: { /* call_indirect, return_call_indirect */
        funcsig_t fs; fsinit(&fs);
        parse_funcsig(pw, &fs);
        pic->arg.u = fsintern(&g_funcsigs, &fs);
        fsfini(&fs);
      } break;
      case INSIG_T: { /* select */
        pic->arg.u = parse_valtype(pw);
      } break;
      case INSIG_I32: case INSIG_I64: {
        numval_t v;
        if (peekt(pw) == WT_INT) {
          scan_integer(pw, pw->tokstr, &v); dropt(pw);
          *(numval_t*)&pic->arg = v; 
          if (is == INSIG_I32 && (v.i < INT32_MIN || v.i > INT32_MAX))
            seprintf(pw, "i32 literal is out of range %s", pw->tokstr);
        } else {
          seprintf(pw, "invalid integer literal %s", pw->tokstr);
        }
      } break;
      case INSIG_F32: {
        if (peekt(pw) == WT_FLOAT || peekt(pw) == WT_INT) {
          pic->arg.f = scan_float(pw, pw->tokstr);
          dropt(pw);
        } else {
          seprintf(pw, "invalid float literal %s", pw->tokstr);
        } 
      } break;
      case INSIG_F64: {
        if (peekt(pw) == WT_FLOAT || peekt(pw) == WT_INT) {
          pic->arg.d = scan_double(pw, pw->tokstr);
          dropt(pw);
        } else {
          seprintf(pw, "invalid double literal %s", pw->tokstr);
        } 
      } break;
      case INSIG_MEMARG: {
        unsigned offset = parse_offset(pw), align = parse_align(pw);
        unsigned a = 0;
        pic->arg.u = offset;
        switch ((int)align) { /* fixme: use ntz? */
          case 1:  a = 0; break;
          case 2:  a = 1; break;
          case 4:  a = 2; break;
          case 8:  a = 3; break;
          case 16: a = 4; break;
          default: assert(false);
        }
        pic->arg2.u = a;
      } break;
      case INSIG_LANEIDX: {
        pic->arg.u = parse_laneidx(pw);
      } break;
      case INSIG_MEMARG_LANEIDX: {
        unsigned offset = parse_offset(pw), align = parse_align(pw);
        unsigned a = 0; unsigned long long lidxv = 0;
        pic->arg2.ux2[0] = offset;
        switch ((int)align) { /* fixme: use ntz? */
          case 1:  a = 0; break;
          case 2:  a = 1; break;
          case 4:  a = 2; break;
          case 8:  a = 3; break;
          case 16: a = 4; break;
          default: assert(false);
        }
        pic->arg2.ux2[1] = a;
        pic->arg.u = parse_laneidx(pw);
      } break;
      case INSIG_LANEIDX16: {
        size_t i; unsigned long long lidxv = 0ULL;
        for (i = 0; i < 16; ++i) {
          unsigned long long lidx = parse_laneidx(pw);
          lidxv = (lidxv << 4) | (lidx & 0xFULL);
        }
        pic->arg.u = lidxv;
      } break;
      case INSIG_RF: { /* ref.func */
        sym_t mod, id; parse_mod_id(pw, &mod, &id);
        pic->id = id; pic->arg2.mod = mod;
      } break;
      case INSIG_RD: { /* ref.data -- not in WASM! */
        sym_t mod, id; parse_mod_id(pw, &mod, &id);
        pic->id = id; pic->arg2.mod = mod;
        pic->arg.i = 0; /* offset; check if given explicitly */
        if (peekt(pw) == WT_KEYWORD && strprf(pw->tokstr, "offset=") != NULL) {
          pic->arg.i = parse_offset(pw);
        }
      } break;
      case INSIG_PR: { /* data.put_ref -- not in WASM! */
        pic->arg.u = parse_offset(pw);
      } break;
      case INSIG_LS_L: { /* br_table */
        size_t n = 0, coff;
        if (!pexb) seprintf(pw, "unexpected br_table instruction");
        coff = bufoff(pexb, pic);
        while (peekt(pw) == WT_ID) {
          pic = icbnewbk(pexb);
          pic->in = IN_BR;
          pic->id = parse_id(pw);
          ++n;
        }
        if (n < 1) seprintf(pw, "invalid br_table instruction");
        pic = icbref(pexb, coff);
        pic->arg.u = n-1; /* sans default */ 
      } break;
      default:
        assert(false);     
    }        
  } else {
    seprintf(pw, "opcode expected");
  }
}

static void parse_import_name(sws_t *pw, watie_t *pi)
{
  if (peekt(pw) == WT_ID) {
    sym_t mod, id;
    parse_mod_id(pw, &mod, &id);
    if (pi->mod != mod || pi->id != id)
      seprintf(pw, "unconventional name $%s:%s ($%s:%s expected)", 
        symname(mod), symname(id), symname(pi->mod), symname(pi->id));
    dropt(pw);
  }
}

static void parse_limits(sws_t *pw, watie_t *pi)
{
  pi->n = parse_uint(pw);
  if (peekt(pw) == WT_INT) {
    pi->m = parse_uint(pw);
    pi->lt = LT_MINMAX;
  } else {
    pi->lt = LT_MIN;
  }
}

static void parse_import_des(sws_t *pw, watie_t *pi)
{
  chbuf_t cb = mkchb();
  expect(pw, "(");
  if (ahead(pw, "global")) {
    dropt(pw);
    pi->iek = IEK_GLOBAL;
    parse_import_name(pw, pi);
    if (ahead(pw, "(")) {
      expect(pw, "(");
      expect(pw, "mut");
      pi->mut = MT_VAR;
      pi->vt = parse_valtype(pw);
      expect(pw, ")");
    } else {
      pi->mut = MT_CONST;
      pi->vt = parse_valtype(pw);
    }
  } else if (ahead(pw, "memory")) {
    dropt(pw);
    pi->iek = IEK_MEM;
    parse_import_name(pw, pi);
    pi->lt = LT_MIN;
    pi->n = parse_uint(pw);
    if (!ahead(pw, ")")) {
      pi->lt = LT_MINMAX;
      pi->m = parse_uint(pw);  
    }
  } else if (ahead(pw, "data")) {
    dropt(pw);
    pi->iek = IEK_DATA;
    parse_import_name(pw, pi);
    if (peekt(pw) == WT_KEYWORD && streql(pw->tokstr, "var")) {
      pi->mut = MT_VAR; dropt(pw);
    }
    if (peekt(pw) == WT_KEYWORD && streql(pw->tokstr, "const")) {
      pi->mut = MT_CONST; dropt(pw);
    }
    pi->align = (int)parse_align(pw);
    pi->n = (unsigned)parse_size(pw);
  } else if (ahead(pw, "func")) {
    dropt(pw);
    pi->iek = IEK_FUNC;
    parse_mod_id(pw, &pi->mod, &pi->id);
    while (ahead(pw, "(")) {
      dropt(pw);
      if (ahead(pw, "param")) {
        dropt(pw);
        *vtbnewbk(&pi->fs.partypes) = parse_valtype(pw);
      } else if (ahead(pw, "result")) {
        dropt(pw);
        *vtbnewbk(&pi->fs.restypes) = parse_valtype(pw);
      } else seprintf(pw, "unexpected function header field %s", pw->tokstr);
      expect(pw, ")");
    }
  } else {
    seprintf(pw, "unexpected import descriptor %s", pw->tokstr);
  }
  expect(pw, ")");
  chbfini(&cb);
}

static void parse_optional_export(sws_t *pw, watie_t *pe)
{
  if (ahead(pw, "(") && peeks(pw, "export")) {
    sym_t id; chbuf_t cb = mkchb();
    const char *cs = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$";
    dropt(pw); expect(pw, "export");
    id = parse_id_string(pw, &cb, cs);
    if (id != pe->id) seprintf(pw, "unexpected export rename"); 
    pe->exported = true;
    expect(pw, ")");
    chbfini(&cb);
  }
}

static void parse_modulefield(sws_t *pw, wat_module_t* pm)
{
  chbuf_t cb = mkchb();
  expect(pw, "(");
  if (ahead(pw, "import")) {
    watie_t *pi = watiebnewbk(&pm->imports, IEK_UNKN);
    const char *csm = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.";
    const char *csi = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$";
    dropt(pw);
    pi->mod = parse_id_string(pw, &cb, csm);
    pi->id = parse_id_string(pw, &cb, csi);
    parse_import_des(pw, pi);
  } else if (ahead(pw, "data")) {
    watie_t *pd = watiebnewbk(&pm->exports, IEK_DATA);
    dropt(pw);
    parse_mod_id(pw, &pd->mod, &pd->id);
    parse_optional_export(pw, pd);
    if (peekt(pw) == WT_KEYWORD && streql(pw->tokstr, "var")) {
      pd->mut = MT_VAR; dropt(pw);
    }
    if (peekt(pw) == WT_KEYWORD && streql(pw->tokstr, "const")) {
      pd->mut = MT_CONST; dropt(pw);
    }
    if (peekt(pw) == WT_KEYWORD && strprf(pw->tokstr, "align=") != NULL) {
      pd->align = (int)parse_align(pw);
    }
    if (peekt(pw) == WT_STRING) {
      scan_string(pw, pw->tokstr, &pd->data);
      dropt(pw);
    } else if (peekt(pw) == WT_KEYWORD && strprf(pw->tokstr, "size=") != NULL) {
      size_t size = (size_t)parse_size(pw);
      chbclear(&pd->data); bufresize(&pd->data, size); /* zeroes */
    } else if (g_watexec) {
      expect(pw, "(");
      parse_ins(pw, &pd->ic, &pd->code);
      expect(pw, ")");
    } else seprintf(pw, "missing size= or data string");
    if (ahead(pw, "(")) { /* WCPL non-lead data segment */
      dropt(pw);
      while (!ahead(pw, ")")) {
        inscode_t *pic = icbnewbk(&pd->code);
        parse_ins(pw, pic, &pd->code);
        if (!g_watexec && (pic->in != IN_REF_DATA && pic->in != IN_DATA_PUT_REF))
          seprintf(pw, "unexpected instruction in WCPL data segment");
      }
      if (!g_watexec && pd->align < 4) /* wasm32 : alignment of a pointer */
        seprintf(pw, "unexpected alignment of non-leaf data segment");
      expect(pw, ")");
    }
    if (g_watexec && peekt(pw) == WT_STRING) {
      /* string follows instruction */
      scan_string(pw, pw->tokstr, &pd->data);
      dropt(pw);
    } 
  } else if (ahead(pw, "memory")) {
    watie_t *pe = watiebnewbk(&pm->exports, IEK_MEM);
    dropt(pw);
    parse_mod_id(pw, &pe->mod, &pe->id);
    parse_optional_export(pw, pe);
    pe->n = parse_uint(pw);
    pe->lt = LT_MIN;
    if (!ahead(pw, ")")) {
      pe->m = parse_uint(pw);
      pe->lt = LT_MINMAX;
    }
  } else if (ahead(pw, "global")) {
    watie_t *pe = watiebnewbk(&pm->exports, IEK_GLOBAL);
    dropt(pw);
    parse_mod_id(pw, &pe->mod, &pe->id);
    parse_optional_export(pw, pe);
    if (ahead(pw, "(")) {
      expect(pw, "(");
      expect(pw, "mut");
      pe->mut = MT_VAR;
      pe->vt = parse_valtype(pw);
      expect(pw, ")");
    } else {
      pe->mut = MT_CONST;
      pe->vt = parse_valtype(pw);
    }
    if (ahead(pw, "(")) {
      expect(pw, "(");
      parse_ins(pw, &pe->ic, NULL);
      expect(pw, ")");
    }
  } else if (ahead(pw, "func")) {
    watie_t *pe = watiebnewbk(&pm->exports, IEK_FUNC);
    dropt(pw);
    parse_mod_id(pw, &pe->mod, &pe->id);
    parse_optional_export(pw, pe);
    while (ahead(pw, "(")) {
      dropt(pw);
      if (ahead(pw, "param")) {
        inscode_t *pic = icbnewbk(&pe->code); valtype_t vt; 
        dropt(pw);
        pic->in = IN_REGDECL; pic->id = parse_id(pw); 
        vt = parse_valtype(pw); pic->arg.u = vt;
        *vtbnewbk(&pe->fs.partypes) = vt;
      } else if (ahead(pw, "result")) {
        dropt(pw);
        *vtbnewbk(&pe->fs.restypes) = parse_valtype(pw);
      } else if (ahead(pw, "local")) {
        inscode_t *pic = icbnewbk(&pe->code);
        dropt(pw);
        pic->in = IN_REGDECL; pic->id = parse_id(pw); 
        pic->arg.u = parse_valtype(pw);
      } else seprintf(pw, "unexpected function header field %s", pw->tokstr);
      expect(pw, ")");
    }
    if (streql(symname(pe->id), "main")) {
      if (vtblen(&pe->fs.restypes) != 1 || *vtbref(&pe->fs.restypes, 0) != VT_I32)
        seprintf(pw, "invalid main() function return value");
      if (vtblen(&pe->fs.partypes) == 0) 
        pm->main = MAIN_VOID;
      else if (vtblen(&pe->fs.partypes) == 2 && *vtbref(&pe->fs.partypes, 0) == VT_I32
               && *vtbref(&pe->fs.partypes, 1) == VT_I32)
        pm->main = MAIN_ARGC_ARGV;
      else
        seprintf(pw, "invalid main() function argument types");
    }
    while (!ahead(pw, ")")) {
      inscode_t *pic = icbnewbk(&pe->code);
      parse_ins(pw, pic, &pe->code);
    }
  } else if (g_watexec && ahead(pw, "table")) {
    watie_t *pe = watiebnewbk(&pm->exports, IEK_TABLE);
    dropt(pw);
    parse_mod_id(pw, &pe->mod, &pe->id);
    parse_limits(pw, pe);
    expect(pw, "funcref");
    pe->vt = RT_FUNCREF;
    expect(pw, ")");
    expect(pw, "(");
    expect(pw, "elem");
    expect(pw, "(");
    expect(pw, "i32.const");
    expect(pw, "1");
    expect(pw, ")");
    while (!ahead(pw, ")")) {
      dpme_t *pd = bufnewbk(&pe->table);
      parse_mod_id(pw, &pd->mod, &pd->id);
    }
    assert(pe->n == pe->m && pe->lt == LT_MINMAX);
    assert(pe->n == buflen(&pe->table) + 1);
  } else {
    seprintf(pw, "unexpected module field type %s", pw->tokstr);
  }
  expect(pw, ")");
  chbfini(&cb);
}

static void parse_module(sws_t *pw, wat_module_t* pm)
{
  expect(pw, "(");
  expect(pw, "module");
  if (peekt(pw) != WT_ID) seprintf(pw, "missing module name");
  pm->name = parse_id(pw);
  while (!ahead(pw, ")")) parse_modulefield(pw, pm);
  expect(pw, ")");
}

void read_wat_object_module(const char *fname, wat_module_t* pm)
{
  sws_t *pw = newsws(fname);
  if (pw) {
    wat_module_clear(pm);
    pm->main = MAIN_ABSENT;
    g_watexec = false;
    parse_module(pw, pm);
    freesws(pw);
  } else {
    exprintf("cannot read object file: %s", fname);
  }
}

void read_wat_executable_module(const char *fname, wat_module_t* pm)
{
  sws_t *pw = newsws(fname);
  if (pw) {
    wat_module_clear(pm);
    pm->main = MAIN_ABSENT;
    expect(pw, "(");
    expect(pw, "module");
    g_watexec = true;
    while (!ahead(pw, ")")) parse_modulefield(pw, pm);
    expect(pw, ")");
    freesws(pw);
  } else {
    exprintf("cannot read executabe wat file: %s", fname);
  }
}

void load_library_wat_module(sym_t mod, wat_module_t* pm)
{
  chbuf_t cb = mkchb(); size_t i;
  sws_t *pws = NULL;
  for (i = 0; i < buflen(g_lbases); ++i) {
    sym_t *pb = bufref(g_lbases, i);
    pws = newsws(chbsetf(&cb, "%s%s.o", symname(*pb), symname(mod)));
    if (pws) break;
    pws = newsws(chbsetf(&cb, "%s%s.wo", symname(*pb), symname(mod)));
    if (pws) break;
    pws = newsws(chbsetf(&cb, "%s%s.wat", symname(*pb), symname(mod)));
    if (pws) break;
  }
  if (pws) {
    logef("# found '%s' module object in %s\n", symname(mod), chbdata(&cb));
    wat_module_clear(pm);
    pm->main = MAIN_ABSENT;
    parse_module(pws, pm);
    logef("# '%s' module object loaded\n", symname(mod));
    freesws(pws);
  } else {
    exprintf("cannot locate object module: '%s'", symname(mod));
  }
  chbfini(&cb); 
}


/* linker */

/* global dependency list entry */
typedef struct modid {
  sym_t mod, id;
  iekind_t iek;
} modid_t;

int modid_cmp(const void *pv1, const void *pv2)
{
  modid_t *pmi1 = (modid_t *)pv1, *pmi2 = (modid_t *)pv2;
  int cmp = (int)pmi1->mod - (int)pmi2->mod;
  if (cmp < 0) return -1; else if (cmp > 0) return 1;
  cmp = (int)pmi1->id - (int)pmi2->id;
  if (cmp < 0) return -1; else if (cmp > 0) return 1;
  return 0;
}

/* if pmii is subsystem global, it is a leaf and should be moved over as import */
static bool process_subsystem_depglobal(modid_t *pmii, wat_module_t* pmi, wat_module_t* pm)
{
  watie_t *pi;
  /* as of now, we only have one subsystem, WASI */
  if (pmii->mod != g_wasi_mod) return false;
  pi = bufbsearch(&pmi->imports, pmii, &modid_cmp);
  if (!pi) exprintf("cannot locate import '%s:%s' in '%s' module", 
    symname(pmii->mod), symname(pmii->id), symname(pmi->name));
  if (!bufsearch(&pm->imports, pmii, &modid_cmp)) { /* linear! */
    watie_t *newpi = watiebnewbk(&pm->imports, pi->iek); 
    newpi->mod = pi->mod; newpi->id = pi->id;
    memswap(pi, newpi, sizeof(watie_t)); /* mod/id still there so bsearch works */
  }
  return true; 
}  

/* find pmii definition, trace it for dependants amd move over to pm */
static void process_depglobal(modid_t *pmii, wat_module_buf_t *pwb, buf_t *pdg, wat_module_t* pm)
{
  wat_module_t* pmi = bufbsearch(pwb, &pmii->mod, &sym_cmp);
  vvverbosef("process_depglobal: %s:%s\n", symname(pmii->mod), symname(pmii->id)); 
  if (!pmi) exprintf("cannot locate '%s' module (looking for %s)", symname(pmii->mod), symname(pmii->id));  
  switch (pmii->iek) {
    case IEK_MEM: {
      watie_t *pe = bufbsearch(&pmi->exports, pmii, &modid_cmp), *newpe;
      if (!pe || pe->iek != IEK_MEM) 
        exprintf("cannot locate memory '%s' in '%s' module", symname(pmii->id), symname(pmii->mod));  
      newpe = watiebnewbk(&pm->exports, IEK_MEM); newpe->mod = pe->mod; newpe->id = pe->id; 
      memswap(pe, newpe, sizeof(watie_t)); /* mod/id still there so bsearch works */
      newpe->exported = true; /* memory should be exported! */
      vverbosef("new mem entry: %s:%s\n", symname(pe->mod), symname(pe->id)); 
    } break;
    case IEK_DATA: { 
      watie_t *pd = bufbsearch(&pmi->exports, pmii, &modid_cmp), *newpd;
      modid_t mi; size_t i;
      if (!pd || pd->iek != IEK_DATA)
        exprintf("cannot locate data '%s' in '%s' module", symname(pmii->id), symname(pmii->mod));  
      for (i = 0; i < icblen(&pd->code); ++i) {
        inscode_t *pic = icbref(&pd->code, i);
        if (pic->in == IN_REF_DATA) {
          mi.mod = pic->arg2.mod; mi.id = pic->id;
          mi.iek = IEK_DATA;
          assert(mi.mod != 0 && mi.id != 0); /* must be relocatable! */
          if (process_subsystem_depglobal(&mi, pmi, pm)) /* ok */ ;
          else if (!bufsearch(pdg, &mi, &modid_cmp)) *(modid_t*)bufnewbk(pdg) = mi;              
        }
      }
      newpd = watiebnewbk(&pm->exports, IEK_DATA); newpd->mod = pd->mod; newpd->id = pd->id; 
      memswap(pd, newpd, sizeof(watie_t)); /* mod/id still there so bsearch works */
      newpd->exported = false;
      vverbosef("new data entry: %s:%s\n", symname(pd->mod), symname(pd->id)); 
    } break;
    case IEK_GLOBAL: {
      watie_t *pe = bufbsearch(&pmi->exports, pmii, &modid_cmp), *newpe;
      if (!pe || pe->iek != IEK_GLOBAL) 
        exprintf("cannot locate global '%s' in '%s' module", symname(pmii->id), symname(pmii->mod));  
      if (pe->ic.in == IN_GLOBAL_GET || pe->ic.in == IN_REF_DATA) {
        inscode_t *pic = &pe->ic;
        modid_t mi; mi.mod = pic->arg2.mod; mi.id = pic->id; 
        mi.iek = (pic->in == IN_GLOBAL_GET) ? IEK_GLOBAL : IEK_DATA;
        assert(mi.mod != 0 && mi.id != 0); /* must be relocatable! */
        if (process_subsystem_depglobal(&mi, pmi, pm)) /* ok */ ;
        else if (!bufsearch(pdg, &mi, &modid_cmp)) *(modid_t*)bufnewbk(pdg) = mi;              
      } else if (pe->ic.in == IN_REF_FUNC) {
        inscode_t *pic = &pe->ic;
        modid_t mi; mi.mod = pic->arg2.mod; mi.id = pic->id;
        mi.iek = IEK_FUNC;
        assert(mi.mod != 0 && mi.id != 0); /* must be relocatable! */
        if (process_subsystem_depglobal(&mi, pmi, pm)) /* ok */ ;
        else if (!bufsearch(pdg, &mi, &modid_cmp)) *(modid_t*)bufnewbk(pdg) = mi;
      } 
      newpe = watiebnewbk(&pm->exports, IEK_GLOBAL); newpe->mod = pe->mod; newpe->id = pe->id; 
      memswap(pe, newpe, sizeof(watie_t)); /* mod/id still there so bsearch works */
      newpe->exported = false;
      vverbosef("new global entry: %s:%s\n", symname(pe->mod), symname(pe->id)); 
    } break;
    case IEK_FUNC: {
      watie_t *pf = bufbsearch(&pmi->exports, pmii, &modid_cmp), *newpf;
      modid_t mi; size_t i;
      if (!pf || pf->iek != IEK_FUNC) 
        exprintf("cannot locate func '%s' in '%s' module", symname(pmii->id), symname(pmii->mod));  
      for (i = 0; i < icblen(&pf->code); ++i) {
        inscode_t *pic = icbref(&pf->code, i);
        switch (pic->in) {
          case IN_GLOBAL_GET: case IN_GLOBAL_SET:
          case IN_CALL: case IN_RETURN_CALL: 
          case IN_REF_FUNC: {
            mi.mod = pic->arg2.mod; mi.id = pic->id;
            mi.iek = (pic->in == IN_GLOBAL_GET || pic->in == IN_GLOBAL_SET) ? IEK_GLOBAL : IEK_FUNC;
            assert(mi.mod != 0 && mi.id != 0); /* must be relocatable! */
            if (process_subsystem_depglobal(&mi, pmi, pm)) /* ok */ ;
            else if (!bufsearch(pdg, &mi, &modid_cmp)) *(modid_t*)bufnewbk(pdg) = mi;              
          } break;
          case IN_REF_DATA: {
            mi.mod = pic->arg2.mod; mi.id = pic->id;
            mi.iek = IEK_DATA;
            assert(mi.mod != 0 && mi.id != 0); /* must be relocatable! */
            if (process_subsystem_depglobal(&mi, pmi, pm)) /* ok */ ;
            else if (!bufsearch(pdg, &mi, &modid_cmp)) *(modid_t*)bufnewbk(pdg) = mi;              
          } break;
          default:;
        }
      }
      newpf = watiebnewbk(&pm->exports, IEK_FUNC); newpf->mod = pf->mod; newpf->id = pf->id; 
      memswap(pf, newpf, sizeof(watie_t)); /* mod/id still there so bsearch works */
      newpf->exported = false;
      vverbosef("new func entry: %s:%s\n", symname(pf->mod), symname(pf->id)); 
    } break;
    default: assert(false);
  }
}

/* data sharing map */
typedef struct dsme {
  chbuf_t data; /* data segment data */
  int align;    /* alignment in bytes: 1,2,4,8,16 */
  bool write;   /* belongs to wtiteable memory */
  sym_t id;     /* module-local dseg id */
  size_t addr;  /* used by linker */
} dsme_t;

typedef buf_t dsmebuf_t; 
#define dsmebinit(mem) (bufinit(mem, sizeof(dsme_t)))
#define dsmeblen(pb) (buflen(pb))
#define dsmebref(pb, i) ((dsme_t*)bufref(pb, i))
#define dsmebnewbk(pb) (dsmeinit(bufnewbk(pb)))

/* g_dsmap element */
dsme_t* dsmeinit(dsme_t* pe)
{
  memset(pe, 0, sizeof(dsme_t));
  chbinit(&pe->data);
  return pe;
}

void dsmefini(dsme_t* pe)
{
  chbfini(&pe->data);
}

void dsmebfini(dsmebuf_t* pb)
{
  dsme_t *pe; size_t i;
  assert(pb); assert(pb->esz == sizeof(dsme_t));
  pe = (dsme_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) dsmefini(pe+i);
  buffini(pb);
}

int dsme_cmp(const void *p1, const void *p2)
{
  dsme_t *pe1 = (dsme_t*)p1, *pe2 = (dsme_t*)p2; 
  int cmp = chbuf_cmp(&pe1->data, &pe2->data);
  if (cmp) return cmp;
  cmp = pe1->align - pe2->align;
  if (cmp < 0) return -1; if (cmp > 0) return 1;
  return 0;
}

/* remove dependence on non-standard WAT features and instructions */
size_t watify_wat_module(wat_module_t* pm)
{
  size_t curaddr = g_sdbaddr;
  chbuf_t dseg = mkchb(); size_t i;
  buf_t dpmap = mkbuf(sizeof(dpme_t));
  buf_t table = mkbuf(sizeof(dpme_t));
  dsmebuf_t dsmap; dsmebinit(&dsmap);
  
  /* reverse exports so leafs are processed before non-leafs */
  bufrev(&pm->exports);
  
  /* concatenate dss into one big data chunk */
  for (i = 0; i < watieblen(&pm->exports); /* del or bumpi */) {
    watie_t *pd = watiebref(&pm->exports, i);
    if (pd->iek == IEK_DATA) {
      dpme_t *pme; size_t addr;
      vverbosef("converting dseg: %s:%s\n", symname(pd->mod), symname(pd->id));
      if (pd->mut == MT_CONST && icblen(&pd->code) == 0) {
        /* leaf, read-only: try to merge it with equals */
        dsme_t e, *pe; dsmeinit(&e); 
        chbcpy(&e.data, &pd->data);
        e.align = pd->align; assert(pd->align);
        pe = bufbsearch(&dsmap, &e, &dsme_cmp);
        if (!pe) { /* new data elt */
          size_t align, n;
          addr = curaddr;
          pe = bufnewbk(&dsmap);
          memswap(pe, &e, sizeof(dsme_t)); 
          align = pd->align, n = addr % align;
          if (n > 0) addr += align - n;
          if (n > 0) bufresize(&dseg, buflen(&dseg) + (align-n));
          chbcat(&dseg, &pe->data);
          pe->addr = addr;
          curaddr = addr + buflen(&pe->data);
          bufqsort(&dsmap, &dsme_cmp);
        } else {
          addr = pe->addr;
        }
        dsmefini(&e);
      } else {
        /* non-leaf or read-write -- can't share */
        size_t align, n;
        addr = curaddr;
        align = pd->align, n = addr % align;
        if (n > 0) addr += align - n;
        if (n > 0) bufresize(&dseg, buflen(&dseg) + (align-n));
        if (icblen(&pd->code) > 0) { /* patch pd->data */
          size_t i; chbuf_t cb = mkchb(); bool gotarg = false;
          for (i = 0; i < icblen(&pd->code); ++i) {
            modid_t mi; dpme_t *pdpme; size_t off; 
            inscode_t *pic = icbref(&pd->code, i);
            if (!gotarg && pic->in == IN_REF_DATA) {
              unsigned address;
              mi.mod = pic->arg2.mod; mi.id = pic->id;
              pdpme = bufsearch(&dpmap, &mi, &modid_cmp); /* linear, still adding to it */
              if (!pdpme) exprintf("internal error: undefined ref in data: $%s:%s", 
                mi.mod ? symname(mi.mod) : "?", mi.id ? symname(mi.id) : "?");
              address = (unsigned)((long long)pdpme->address + pic->arg.i);
              bufclear(&cb); binuint(address, &cb); /* wasm32 */
              gotarg = true;
            } else if (gotarg && pic->in == IN_DATA_PUT_REF) {
              off = (size_t)pic->arg.u; assert(pic->id == 0);
              if (off % 4 != 0 || off + 4 > chblen(&pd->data)) /* wasm32 */
                exprintf("internal error: cannot patch data: bad offset %lu", (unsigned long)off);
              memcpy(chbdata(&pd->data) + off, chbdata(&cb), 4); /* wasm32 */
              gotarg = false;
            } else {
              exprintf("internal error in data patch");
            }
          }
          chbfini(&cb);
        }
        chbcat(&dseg, &pd->data);
        curaddr = addr + buflen(&pd->data);
      }
      pme = bufnewbk(&dpmap);
      pme->mod = pd->mod; pme->id = pd->id;
      pme->address = addr;
      vverbosef("  addr = %u\n", (unsigned)addr);
      watiefini(pd); bufrem(&pm->exports, i);
    } else {
      ++i;
    }
  }
  
  /* prepare dpmap for binary search */
  bufqsort(&dpmap, &modid_cmp);
  
  /* patch IN_REF_{DATA,FUNC}s, memory export, zero globals */
  for (i = 0; i < watieblen(&pm->exports); ++i) {
    watie_t *pe = watiebref(&pm->exports, i);
    if (pe->iek == IEK_FUNC) {
      watie_t *pf = pe; size_t j;
      for (j = 0; j < icblen(&pf->code); ++j) {
        inscode_t *pic = icbref(&pf->code, j);
        if (pic->in == IN_REF_DATA) {
          modid_t mi; dpme_t *pdpme; 
          mi.mod = pic->arg2.mod; mi.id = pic->id;
          pdpme = bufbsearch(&dpmap, &mi, &modid_cmp);
          if (!pdpme) exprintf("internal error: cannot patch ref.data $%s:%s", 
            symname(mi.mod), symname(mi.id));
          pic->in = IN_I32_CONST;
          pic->arg.i = (long long)pdpme->address + pic->arg.i;
        } else if (pic->in == IN_REF_FUNC) {
          dpme_t e, *pdpme;
          e.mod = pic->arg2.mod; e.id = pic->id;
          pdpme = bufsearch(&table, &e, &modid_cmp);
          if (!pdpme) { pdpme = bufnewbk(&table); *pdpme = e; }
          pic->in = IN_I32_CONST;
          pic->arg.i = (long long)bufoff(&table, pdpme) + 1; /* elem #0 reserved */
        }
      }
    } else if (pe->iek == IEK_GLOBAL) {
      if (pe->ic.in == 0) {
        pe->ic.arg.u = 0; /* whole union gets zeroed */
        switch (pe->vt) {
          case VT_F64: pe->ic.in = IN_F64_CONST; break;
          case VT_F32: pe->ic.in = IN_F32_CONST; break;
          case VT_I64: pe->ic.in = IN_I64_CONST; break;
          case VT_I32: pe->ic.in = IN_I32_CONST; break;
          default:;
        }
      } else if (pe->ic.in == IN_REF_DATA) {
        modid_t mi; dpme_t *pdpme; 
        mi.mod = pe->ic.arg2.mod; mi.id = pe->ic.id;
        pdpme = bufbsearch(&dpmap, &mi, &modid_cmp);
        if (!pdpme) exprintf("internal error: cannot patch ref.data $%s:%s", 
          symname(mi.mod), symname(mi.id));
        pe->ic.in = IN_I32_CONST;
        pe->ic.arg.i = (long long)pdpme->address + pe->ic.arg.i;
      } else if (pe->ic.in == IN_REF_FUNC) {
        dpme_t e, *pdpme;
        e.mod = pe->ic.arg2.mod; e.id = pe->ic.id;
        pdpme = bufsearch(&table, &e, &modid_cmp);
        if (!pdpme) { pdpme = bufnewbk(&table); *pdpme = e; }
        pe->ic.in = IN_I32_CONST;
        pe->ic.arg.i = (long long)bufoff(&table, pdpme) + 1; /* elem #0 reserved */
      }
    }
  }
    
  /* add combined data segment */
  if (buflen(&dpmap) > 0) {
    watie_t *pd = watiebnewbk(&pm->exports, IEK_DATA);
    memswap(&pd->data, &dseg, sizeof(buf_t));
    pd->align = 0; /* means ds is final */
    pd->ic.in = IN_I32_CONST; /* wasm32 address */
    pd->ic.arg.u = g_sdbaddr;
  }
  
  /* always add table, even if it is empty! */
  if (true) {
    watie_t *pt = watiebnewbk(&pm->exports, IEK_TABLE);
    memswap(&pt->table, &table, sizeof(buf_t));
    pt->n = pt->m = (unsigned)buflen(&pt->table) + 1;
    pt->lt = LT_MINMAX;
  }
  
  chbfini(&dseg);
  dsmebfini(&dsmap);
  buffini(&dpmap);
  buffini(&table);
  
  /* return absolute address of dseg's end */
  return curaddr;
}

/* patch initial stack pointer and memory size */
static void finalize_wat_module(wat_module_t* pm, size_t dsegend)
{
  size_t i, n, align, spaddr, hpaddr, ememaddr, mempages;
  spaddr = dsegend + g_stacksz; align = 16; 
  n = spaddr % align; if (n > 0) spaddr += align - n;
  hpaddr = spaddr + g_argvbsz; align = 16; 
  n = hpaddr % align; if (n > 0) hpaddr += align - n;
  ememaddr = hpaddr; align = 64*1024; /* page = 64K */
  n = ememaddr % align; if (n > 0) ememaddr += align - n;
  mempages = ememaddr/align;
  for (i = 0; i < watieblen(&pm->exports); ++i) {
    watie_t *pe = watiebref(&pm->exports, i);
    if (pe->iek == IEK_GLOBAL) {
      if (pe->mod == g_crt_mod && pe->id == g_sp_id) {
        if (pe->ic.in == IN_I32_CONST) { /* wasm32 */
          pe->ic.arg.u = spaddr;
        }
      } else if (pe->mod == g_crt_mod && pe->id == g_sb_id) {
        if (pe->ic.in == IN_I32_CONST) { /* wasm32 */
          pe->ic.arg.u = spaddr;
        }
      } else if (pe->mod == g_crt_mod && pe->id == g_hb_id) {
        if (pe->ic.in == IN_I32_CONST) { /* wasm32 */
          pe->ic.arg.u = hpaddr;
        }
      }
    } else if (pe->iek == IEK_MEM) {
      if (pe->mod == g_crt_mod && pe->id == g_lm_id) {
        pe->n = (unsigned)mempages;
        /* it seems to be required that mem is exported as "memory" */
        pe->id = intern("memory"); pe->exported = true;
      }
    }
  }  
}

void link_wat_modules(wat_module_buf_t *pwb, wat_module_t* pm)
{
  size_t i, j; main_t mt = MAIN_ABSENT;
  sym_t mainid = intern("main"), startid = intern("_start"); 
  sym_t mainmod = 0, envmod = 0, startmod = 0; 
  buf_t curmodnames = mkbuf(sizeof(sym_t)); /* in pwb */
  buf_t extmodnames = mkbuf(sizeof(sym_t)); /* not in pwb */
  buf_t depglobals = mkbuf(sizeof(modid_t));
  modid_t *pmodid; size_t dsegend;
  
  /* find main() function, collect dependencies */
  for (i = 0; i < wat_module_buf_len(pwb); ++i) {
    wat_module_t* pmi = wat_module_buf_ref(pwb, i);
    size_t mainj = (size_t)-1;
    /* check for main */
    if (pmi->main != MAIN_ABSENT) {
      if (mainmod) 
        exprintf("duplicate main() in %s and %s modules", 
          symname(mainmod), symname(pmi->name));
      mainmod = pmi->name; mt = pmi->main;
      for (j = 0; j < watieblen(&pmi->exports); ++j) {
        watie_t *pe = watiebref(&pmi->exports, j);
        if (pe->iek != IEK_FUNC) continue;
        if (pe->id == mainid) {
          assert(pe->mod == mainmod);
          if (mainj != (size_t)-1)
            exprintf("duplicate main() in %s module", 
              symname(mainmod));
          mainj = j;
        } 
      }
    }
    /* trace dependencies */
    for (j = 0; j < watieblen(&pmi->imports); ++j) {
      watie_t *pi = watiebref(&pmi->imports, j);
      if (bufsearch(&extmodnames, &pi->mod, &sym_cmp) == NULL)
        *(sym_t*)bufnewbk(&extmodnames) = pi->mod;
    }
    if (bufsearch(&curmodnames, &pmi->name, &sym_cmp) != NULL)
      exprintf("duplicate module: %s", symname(pmi->name));
    else *(sym_t*)bufnewbk(&curmodnames) = pmi->name;
  }
  if (!mainmod) logef("error: main() function not found\n"); /* fixme: should be exprintf */
  else logef("# main() found in '%s' module\n", symname(mainmod));

  /* add missing library modules */
  for (i = 0; i < buflen(&extmodnames); ++i) {
    sym_t *pmn = bufref(&extmodnames, i);
    if (bufsearch(&curmodnames, pmn, &sym_cmp) == NULL) {
      if (*pmn == g_wasi_mod) {
        logef("# found WASI subsystem dependence: '%s' module\n", symname(g_wasi_mod));
        /* nothing to load  */
      } else {
        sym_t rtn = *pmn; wat_module_t* pnewm;
        if (rtn == g_crt_mod) {
          if (mt == MAIN_VOID) rtn = internf("%s.void", symname(rtn));
          else if (mt == MAIN_ARGC_ARGV) {
            if (g_argvbsz != 0) rtn = internf("%s.args", symname(rtn));
            else rtn = internf("%s.argv", symname(rtn));
          }
          envmod = rtn;
        }
        logef("# found library dependence: '%s' module\n", symname(rtn));
        load_library_wat_module(rtn, (pnewm = wat_module_buf_newbk(pwb)));
        if (pnewm->main != MAIN_ABSENT)
          exprintf("unexpected main() in library module '%s'", symname(rtn)); 
        /* trace sub-dependencies */
        for (j = 0; j < watieblen(&pnewm->imports); ++j) {
          watie_t *pi = watiebref(&pnewm->imports, j);
          if (bufsearch(&extmodnames, &pi->mod, &sym_cmp) == NULL)
            *(sym_t*)bufnewbk(&extmodnames) = pi->mod;
        }
      }
    }
  }
  
  /* first, sort modules by module name for bsearch */
  bufqsort(pwb, &sym_cmp);
  /* now sort module's field lists by mod-name pair */
  for (i = 0; i < wat_module_buf_len(pwb); ++i) {
    wat_module_t* pmi = wat_module_buf_ref(pwb, i);
    bufqsort(&pmi->imports, &modid_cmp);    
    bufqsort(&pmi->exports, &modid_cmp);    
  }

  /* now seed depglobals and use it to move globals to pm */
  pmodid = bufnewbk(&depglobals); 
  pmodid->mod = mainmod; pmodid->id = mainid; pmodid->iek = IEK_FUNC;
  pmodid = bufnewbk(&depglobals); 
  pmodid->mod = mainmod; pmodid->id = startid; pmodid->iek = IEK_FUNC;
  if (envmod) { /* need to ref linear memory explicitly */
    pmodid = bufnewbk(&depglobals); 
    pmodid->mod = g_crt_mod; pmodid->id = g_lm_id; pmodid->iek = IEK_MEM;  
  }
  
  /* process depglobals until all deps moved to pm */
  for (i = 0; i < buflen(&depglobals); ++i) {
    /* precondition: this global is not moved yet */
    modid_t *pmii = bufref(&depglobals, i);
    process_depglobal(pmii, pwb, &depglobals, pm);
  }

  /* pass over output module functions to check for _start */
  for (i = 0; i < watieblen(&pm->exports); ++i) {
    watie_t *pe = watiebref(&pm->exports, i);
    if (pe->iek != IEK_FUNC) continue;
    if (pe->id == startid) {
      if (startmod) 
        exprintf("duplicate _start() function in module '%s' and '%s'",
          symname(startmod), symname(pe->mod));
      startmod = pe->mod;
      pe->exported = true;
    }
  }
  if (!startmod) exprintf("_start() function not found");
  else logef("# found _start() function in '%s' module\n", symname(startmod));

  if (getverbosity() > 0) {
    fprintf(stderr, "linked module before watify:\n");
    write_wat_module(pm, stderr);
  }
  
  /* remove dependence on non-standard WAT features and instructions */
  dsegend = watify_wat_module(pm);
  
  /* patch initial stack pointer and memory size */
  finalize_wat_module(pm, dsegend);

  /* done */    
  buffini(&curmodnames);
  buffini(&extmodnames);
  buffini(&depglobals);
}


/* wat-to-wasm converter */

static unsigned lookup_func_idx(wasm_module_t *pbm, sym_t mod, sym_t id)
{
  size_t i;
  for (i = 0; i < entblen(&pbm->funcdefs); ++i) {
    entry_t *pe = entbref(&pbm->funcdefs, i);
    if (pe->mod == mod && pe->name == id) return (unsigned)i;
  }
  exprintf("cannot locate function $%s:%s", symname(mod), symname(id));
  return 0;
}

static unsigned lookup_glob_idx(wasm_module_t *pbm, sym_t mod, sym_t id)
{
  size_t i;
  for (i = 0; i < entblen(&pbm->globdefs); ++i) {
    entry_t *pe = entbref(&pbm->globdefs, i);
    if (pe->mod == mod && pe->name == id) return (unsigned)i;
  }
  exprintf("cannot locate global $%s:%s", symname(mod), symname(id));
  return 0;
}

static unsigned lookup_var_idx(icbuf_t *picb, sym_t id)
{
  size_t i;
  for (i = 0; i < icblen(picb); ++i) {
    inscode_t *pic = icbref(picb, i);
    if (pic->in != IN_REGDECL) break;
    if (pic->id == id) return (unsigned)i;
  }
  exprintf("cannot locate local $%s", symname(id));
  return 0;
}

static unsigned lookup_label(buf_t *plbb, sym_t id)
{
  size_t n = buflen(plbb), i;
  for (i = 0; i < n; ++i) {
    sym_t *pid = bufref(plbb, n-i-1);
    if (*pid == id) return (unsigned)i;
  }
  exprintf("cannot locate label $%s", symname(id));
  return 0;
}

static void wat_to_wasm_code(wasm_module_t *pbm, size_t parc, vtbuf_t *pltb, icbuf_t *pdcb, icbuf_t *pscb)
{
  size_t i;
  buf_t lbb = mkbuf(sizeof(sym_t));
  for (i = 0; i < icblen(pscb); ++i) {
    inscode_t *psc = icbref(pscb, i), *pdc;
    if (psc->in == IN_REGDECL) {
      if (i >= parc) *vtbnewbk(pltb) = (valtype_t)psc->arg.u;
      continue;
    }
    /* copy real instruction and patch it */
    pdc = icbnewbk(pdcb); *pdc = *psc; pdc->id = 0;
    switch (instr_sig(psc->in)) {
      case INSIG_NONE: /* end, ... */ {
        if (psc->in == IN_END) {
          sym_t *pl = bufpopbk(&lbb);
          if (psc->id) assert(*pl == psc->id);
        }
      } break;
      case INSIG_BT: /* block, loop, if */ {
        sym_t *pl = bufnewbk(&lbb);
        *pl = psc->id; 
      } break;
      case INSIG_XL: /* local get,set,tee */ {
        if (psc->id) pdc->arg.u = lookup_var_idx(pscb, psc->id);
      } break;
      case INSIG_XT: /* table get, set, mem/data ref/grow ... */ {
        /* nothing to patch here */
      } break; 
      case INSIG_XG: { /* call, global get,set */
        if (psc->in != IN_CALL && psc->in != IN_RETURN_CALL) {
          if (psc->arg2.mod && psc->id) pdc->arg.u = 
            lookup_glob_idx(pbm, psc->arg2.mod, psc->id); 
          break;
        }
      } /* fall thru */ 
      case INSIG_RF: /* ref_func */ {
        if (psc->arg2.mod && psc->id) pdc->arg.u = 
          lookup_func_idx(pbm, psc->arg2.mod, psc->id); 
      } break;
      case INSIG_L: /* br, br_if */ {
        pdc->arg.u = lookup_label(&lbb, psc->id);
      } break;
      case INSIG_T: /* select */ {
        /* nothing to patch here */
      } break;
      case INSIG_I32: /* const */ {
        /* extend sign to upper 32 bits */
        pdc->arg.i = (psc->arg.i << 32) >> 32;
      } break;
      case INSIG_I64: case INSIG_F32: case INSIG_F64: /* const */ {
        /* nothing to patch here */
      } break;
      case INSIG_CI: /* call_indirect */ {
        /* recalc function signature index */
        size_t n = (size_t)psc->arg.u;
        assert(n < fsblen(&g_funcsigs));
        pdc->arg.u = fsintern(&pbm->funcsigs, fsbref(&g_funcsigs, n));
      } break;
      case INSIG_X_Y: /* table init/copy */ {
      } break;
      case INSIG_MEMARG: /* load*, store* */ {
        /* nothing to patch here */
      } break;
      case INSIG_LS_L: /* br_table */ {
        /* next pic->arg.u branches are patched in a normal way */
      } break;
      default:
        assert(false);     
    }
  }
  icbnewbk(pdcb)->in = IN_END;
  buffini(&lbb);
}

static void wat_to_wasm_ie(bool import, watiebuf_t *pieb, wasm_module_t *pbm)
{
  size_t i;
  for (i = 0; i < watieblen(pieb); ++i) {
    watie_t *pie = watiebref(pieb, i);
    switch (pie->iek) {
      case IEK_FUNC: {
        entry_t *pe = entbnewbk(&pbm->funcdefs, EK_FUNC);
        pe->mod = pie->mod; pe->name = pie->id; 
        if (import) pe->imported = true;
        else pe->exported = pie->exported;
        pe->fsi = fsintern(&pbm->funcsigs, &pie->fs);
      } break;
      case IEK_TABLE: {
        entry_t *pe = entbnewbk(&pbm->tabdefs, EK_TABLE);
        pe->mod = pie->mod; pe->name = pie->id; 
        if (import) pe->imported = true;
        else pe->exported = pie->exported;
        pe->vt = RT_FUNCREF; // can it be RT_EXTERNREF? should we use pi->vt?
        pe->lt = pie->lt; pe->n = pie->n; pe->m = pie->m;
      } break;
      case IEK_MEM: {
        entry_t *pe = entbnewbk(&pbm->memdefs, EK_MEM);
        pe->mod = pie->mod; pe->name = pie->id; 
        if (import) pe->imported = true;
        else pe->exported = pie->exported;
        pe->lt = pie->lt; pe->n = pie->n; pe->m = pie->m;
      } break;
      case IEK_GLOBAL: {
        entry_t *pe = entbnewbk(&pbm->globdefs, EK_GLOBAL);
        inscode_t *pdc;
        pe->mod = pie->mod; pe->name = pie->id; 
        if (import) pe->imported = true;
        else pe->exported = pie->exported;
        pe->vt = pie->vt; pe->mut = pie->mut;
        assert(IN_I32_CONST <= pie->ic.in && pie->ic.in <= IN_F64_CONST);
        pdc = icbnewbk(&pe->code); *pdc = pie->ic; pdc->id = 0;
        icbnewbk(&pe->code)->in = IN_END; 
      } break;
      default:;
    }
  }
}

void wat_to_wasm(wat_module_t *ptm, wasm_module_t *pbm)
{
  size_t i, fi;
  /* go over imports/exports to fill {func,tab,mem,glob}defs  */
  wat_to_wasm_ie(true, &ptm->imports, pbm);
  wat_to_wasm_ie(false, &ptm->exports, pbm);
  /* adjust initial function index for imports */
  for (i = fi = 0; i < watieblen(&ptm->imports); ++i) {
    if (watiebref(&ptm->imports, i)->iek == IEK_FUNC) ++fi;
  }
  /* go over exports to fill elemdefs, datadefs, and funcdefs' code */
  for (i = 0; i < watieblen(&ptm->exports); ++i) {
    watie_t *pe = watiebref(&ptm->exports, i);
    switch (pe->iek) {
      case IEK_FUNC: {
        entry_t *pfe = entbref(&pbm->funcdefs, fi);
        size_t parc = vtblen(&pe->fs.partypes);
        assert(pe->mod == pfe->mod && pe->id == pfe->name);
        /* fprintf(stderr, "converting code for %s:%s (fi = %d, length = %d, parc = %d)\n", 
          symname(pfe->mod), symname(pfe->name), (int)fi, (int)icblen(&pe->code), (int)parc); */
        wat_to_wasm_code(pbm, parc, &pfe->loctypes, &pfe->code, &pe->code);
        ++fi;
      } break;
      case IEK_TABLE: {
        eseg_t *ps = esegbnewbk(&pbm->elemdefs, ES_ACTIVE_EIV);
        size_t j; inscode_t *pi = icbnewbk(&ps->code);
        pi->in = IN_I32_CONST, pi->arg.u = 1;
        icbnewbk(&ps->code)->in = IN_END;
        for (j = 0; j < buflen(&pe->table); ++j) {
          dpme_t *de = bufref(&pe->table, j);
          *idxbnewbk(&ps->fidxs) = lookup_func_idx(pbm, de->mod, de->id);
        }
      } break;
      case IEK_DATA: {
        dseg_t *ps = dsegbnewbk(&pbm->datadefs, DS_ACTIVE); 
        inscode_t *pdc;
        assert(!pe->align); assert(pe->ic.in == IN_I32_CONST);
        pdc = icbnewbk(&ps->code); *pdc = pe->ic; pdc->id = 0;
        icbnewbk(&ps->code)->in = IN_END; 
        chbcpy(&ps->data, &pe->data);
      } break;
      default:;
    }
  }
}

