/* w.c (wasm interface) -- esl */

#include <stddef.h>
#include <stdlib.h>
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
  fputc((unsigned)si & 0xFF, g_wasmout);
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
  else fputc(b & 0xFF, g_wasmout);
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
    more = b & 0x40 ? val != -1 : val != 0;
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
  if ((in & 0xff) == in) { /* 1-byte */
    wasm_byte((unsigned)in & 0xff);
  } else if ((in & 0xff) == in) { /* 2-byte */
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

void fsbfini(fsbuf_t* pb)
{
  funcsig_t *pft; size_t i;
  assert(pb); assert(pb->esz = sizeof(funcsig_t));
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

unsigned fsintern(fsbuf_t* pb, funcsig_t *pfs)
{
  size_t i;
  for (i = 0; i < fsblen(pb); ++i) if (sameft(fsbref(pb, i), pfs)) break;
  if (i == fsblen(pb)) memswap(pfs, fsbnewbk(pb), sizeof(funcsig_t));
  return i;
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
  return i;
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
  assert(pb); assert(pb->esz = sizeof(entry_t));
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
  assert(pb); assert(pb->esz = sizeof(dseg_t));
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
  assert(pb); assert(pb->esz = sizeof(eseg_t));
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
        break;
      case INSIG_BT:  case INSIG_L:   
      case INSIG_XL:  case INSIG_XG:
      case INSIG_XT:  case INSIG_T:
      case INSIG_I32: case INSIG_I64:
        wasm_unsigned(pic->arg.u); 
        break;
      case INSIG_X_Y: case INSIG_MEMARG:
        wasm_unsigned(pic->arg.u); wasm_unsigned(pic->arg2.u); 
        break;
      case INSIG_F32:
        wasm_float(pic->arg.f); 
        break;
      case INSIG_F64:
        wasm_double(pic->arg.d); 
        break;
      case INSIG_LS_L:
        assert(false); /* fixme */
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
    if (!pe->mod) continue; /* skip non-imported */
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
    if (!pe->mod) continue; /* skip non-imported */
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
    if (!pe->mod) continue; /* skip non-imported */
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
    if (!pe->mod) continue; /* skip non-imported */
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
    if (pe->mod) continue; /* skip imported functions */
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
    if (pe->mod) continue; /* skip imported tables */
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
    if (pe->mod) continue; /* skip imported mems */
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
    if (pe->mod) continue; /* skip imported globals */
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
  if (pfdb) for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (!pe->mod && pe->name) {
      wasm_name(symname(pe->name));
      wasm_byte(EK_FUNC);
      wasm_unsigned(i);
      wasm_section_bumpc();
    }
  }  
  if (ptdb) for (i = 0; i < entblen(ptdb); ++i) {
    entry_t *pe = entbref(ptdb, i);
    assert(pe->ek == EK_TABLE);
    if (!pe->mod && pe->name) {
      wasm_name(symname(pe->name));
      wasm_byte(EK_TABLE);
      wasm_unsigned(i);
      wasm_section_bumpc();
    }
  }  
  if (pmdb) for (i = 0; i < entblen(pmdb); ++i) {
    entry_t *pe = entbref(pmdb, i);
    assert(pe->ek == EK_MEM);
    if (!pe->mod && pe->name) {
      wasm_name(symname(pe->name));
      wasm_byte(EK_MEM);
      wasm_unsigned(i);
      wasm_section_bumpc();
    }
  }  
  if (pgdb) for (i = 0; i < entblen(pgdb); ++i) {
    entry_t *pe = entbref(pgdb, i);
    assert(pe->ek == EK_GLOBAL);
    if (!pe->mod && pe->name) {
      wasm_name(symname(pe->name));
      wasm_byte(EK_GLOBAL);
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
         goto emitfiv;
       case ES_ACTIVE_TEKIV: /* ti e k fiv */
         wasm_unsigned(ps->idx);
         wasm_expr(&ps->code);
         /* fall thru */
       case ES_DECLVE_KIV: /* k fiv */
       case ES_PASSIVE_KIV: /* k fiv */ 
         wasm_byte(ps->ek); /* must be 0 */
       emitfiv:
         wasm_unsigned(idxblen(&ps->fidxs));
         for (k = 0; k < idxblen(&ps->fidxs); ++k)
           wasm_unsigned(*idxbref(&ps->fidxs, k));
         break;       
       case ES_ACTIVE_EEV: /* e ev */
         wasm_expr(&ps->code);
         goto emitev;
       case ES_ACTIVE_IETEV: /* ti e rt ev */       
         wasm_unsigned(ps->idx);
         wasm_expr(&ps->code);
         /* fall thru */
       case ES_DECLVE_TEV: /* rt ev */
       case ES_PASSIVE_TEV: /* rt ev */
         wasm_byte(ps->rt); /* must be RT_XXX */       
       emitev:
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
  size_t i, k;
  if (!pfdb) return;
  wasm_section_start(SI_CODE);
  for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (pe->mod) continue; /* skip imported functions */
    wasm_code_start();
    wasm_unsigned(buflen(&pe->loctypes));
    for (k = 0; k < buflen(&pe->loctypes); ++k) {
      /* no type compression: types may repeat */
      wasm_unsigned(1);
      wasm_byte(*vtbref(&pe->loctypes, k));
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
  wasm_datacount(&pm->datadefs);
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
  switch (in) {
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
    case IN_REF_DATA: s = "ref.data"; break; /* not in WASM! */
    case IN_DATA_PUT_REF: s = "data.put_ref"; break; /* not in WASM! */
    default: {
      size_t i = (size_t)in;
      if (in <= 0xff && g_innames[i]) s = g_innames[i];
    }
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
  }
  return s;
}

static buf_t g_nimap;

instr_t name_instr(const char *name)
{ 
  sym_t sym; int *pi;
  if (!g_nimap.esz) {
    size_t i; bufinit(&g_nimap, sizeof(int)*2);
    for (i = 0; i <= 0xff; ++i) {
      const char *s = g_innames[i];
      if (s) { pi = bufnewbk(&g_nimap); pi[0] = intern(s), pi[1] = i; }
    }
    pi = bufnewbk(&g_nimap); pi[0] = intern("i32.trunc_sat_f32_s"), pi[1] = IN_I32_TRUNC_SAT_F32_S;
    pi = bufnewbk(&g_nimap); pi[0] = intern("i32.trunc_sat_f32_u"), pi[1] = IN_I32_TRUNC_SAT_F32_U; 
    pi = bufnewbk(&g_nimap); pi[0] = intern("i32.trunc_sat_f64_s"), pi[1] = IN_I32_TRUNC_SAT_F64_S; 
    pi = bufnewbk(&g_nimap); pi[0] = intern("i32.trunc_sat_f64_u"), pi[1] = IN_I32_TRUNC_SAT_F64_U; 
    pi = bufnewbk(&g_nimap); pi[0] = intern("i64.trunc_sat_f32_s"), pi[1] = IN_I64_TRUNC_SAT_F32_S; 
    pi = bufnewbk(&g_nimap); pi[0] = intern("i64.trunc_sat_f32_u"), pi[1] = IN_I64_TRUNC_SAT_F32_U; 
    pi = bufnewbk(&g_nimap); pi[0] = intern("i64.trunc_sat_f64_s"), pi[1] = IN_I64_TRUNC_SAT_F64_S; 
    pi = bufnewbk(&g_nimap); pi[0] = intern("i64.trunc_sat_f64_u"), pi[1] = IN_I64_TRUNC_SAT_F64_U; 
    pi = bufnewbk(&g_nimap); pi[0] = intern("memory.init"), pi[1] = IN_MEMORY_INIT;
    pi = bufnewbk(&g_nimap); pi[0] = intern("data.drop"), pi[1] = IN_DATA_DROP;           
    pi = bufnewbk(&g_nimap); pi[0] = intern("memory.copy"), pi[1] = IN_MEMORY_COPY;         
    pi = bufnewbk(&g_nimap); pi[0] = intern("memory.fill"), pi[1] = IN_MEMORY_FILL;         
    pi = bufnewbk(&g_nimap); pi[0] = intern("table.init"), pi[1] = IN_TABLE_INIT;          
    pi = bufnewbk(&g_nimap); pi[0] = intern("elem.drop"), pi[1] = IN_ELEM_DROP;           
    pi = bufnewbk(&g_nimap); pi[0] = intern("table.copy"), pi[1] = IN_TABLE_COPY;          
    pi = bufnewbk(&g_nimap); pi[0] = intern("table.grow"), pi[1] = IN_TABLE_GROW;          
    pi = bufnewbk(&g_nimap); pi[0] = intern("table.size"), pi[1] = IN_TABLE_SIZE;          
    pi = bufnewbk(&g_nimap); pi[0] = intern("table.fill"), pi[1] = IN_TABLE_FILL;
    pi = bufnewbk(&g_nimap); pi[0] = intern("ref.data"), pi[1] = IN_REF_DATA; /* not in WASM! */
    pi = bufnewbk(&g_nimap); pi[0] = intern("data.put_ref"), pi[1] = IN_DATA_PUT_REF; /* not in WASM! */
    bufqsort(&g_nimap, sym_cmp);
  }
  sym = intern(name);
  pi = bufbsearch(&g_nimap, &sym, sym_cmp);
  if (pi) return (instr_t)pi[1];
  else return IN_PLACEHOLDER;
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
      return INSIG_X_Y;
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
    case IN_REF_FUNC:      case IN_MEMORY_INIT:  case IN_DATA_DROP:    case IN_ELEM_DROP:
    case IN_TABLE_GROW:    case IN_TABLE_SIZE:   case IN_TABLE_FILL:   
      return INSIG_XT;
    case IN_TABLE_INIT:    case IN_TABLE_COPY:
      return INSIG_X_Y;
    case IN_REF_DATA: /* not in WASM! */
      return INSIG_RD; 
    case IN_DATA_PUT_REF: /* not in WASM! */
      return INSIG_PR; 
  }
  return INSIG_NONE;
}

const char *format_inscode(inscode_t *pic, chbuf_t *pcb)
{
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
  switch (instr_sig(pic->in)) {
    case INSIG_NONE:
      break;
    case INSIG_BT: {
      valtype_t vt = pic->arg.u;
      if (pic->id) chbputf(pcb, " $%s", symname(pic->id)); 
      if (vt != BT_VOID) chbputf(pcb, " (result %s)", valtype_name(vt)); 
    } break;
    case INSIG_XL: case INSIG_XG: case INSIG_XT:
      if (pic->id && pic->arg2.mod) 
        chbputf(pcb, " $%s:%s", symname(pic->arg2.mod), symname(pic->id)); 
      else if (pic->id) chbputf(pcb, " $%s", symname(pic->id)); 
      else chbputf(pcb, " %lld", pic->arg.i); 
      break;
    case INSIG_L: case INSIG_T:
      if (pic->id) chbputf(pcb, " $%s", symname(pic->id)); 
      else chbputf(pcb, " %lld", pic->arg.i); 
      break;
    case INSIG_I32:  case INSIG_I64:
      chbputf(pcb, " %lld", pic->arg.i); 
      if (pic->id) chbputf(pcb, " (; $%s:%s ;)", symname(pic->arg2.mod), symname(pic->id)); 
      break;
    case INSIG_X_Y:
      if (pic->id) chbputf(pcb, " $%s %u", symname(pic->id), pic->arg2.u); 
      else chbputf(pcb, " %lld %u", pic->arg.i, pic->arg2.u); 
      break;
    case INSIG_MEMARG:
      if (pic->id) chbputf(pcb, " $%s %u", symname(pic->id), pic->arg2.u); 
      else chbputf(pcb, " offset=%llu align=%u", pic->arg.u, 1<<pic->arg2.u); 
      break;
    case INSIG_F32:
      chbputc(' ', pcb); chbputg((double)pic->arg.f, pcb);
      if (pic->id) chbputf(pcb, " (; $%s:%s ;)", symname(pic->arg2.mod), symname(pic->id)); 
      break;
    case INSIG_F64:
      chbputc(' ', pcb); chbputg(pic->arg.d, pcb);
      if (pic->id) chbputf(pcb, " (; $%s:%s ;)", symname(pic->arg2.mod), symname(pic->id)); 
      break;
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


/* wat text representations */

watie_t* watieinit(watie_t* pie, iekind_t iek)
{
  memset(pie, 0, sizeof(watie_t));
  bufinit(&pie->data, sizeof(char));
  fsinit(&pie->fs);
  icbinit(&pie->code);
  pie->iek = iek;
  return pie;
}

void watiefini(watie_t* pie)
{
  buffini(&pie->data);
  fsfini(&pie->fs);
  icbfini(&pie->code);
}

void watiebfini(watiebuf_t* pb)
{
  watie_t *pie; size_t i;
  assert(pb); assert(pb->esz = sizeof(watie_t));
  pie = (watie_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) watiefini(pie+i);
  buffini(pb);
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
  assert(pb); assert(pb->esz = sizeof(wat_module_t));
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
        assert(false); /* never imported */
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

static void wat_export_datas(watiebuf_t *pdb)
{
  size_t i;
  for (i = 0; i < watieblen(pdb); ++i) {
    watie_t *pd = watiebref(pdb, i);
    if (pd->iek == IEK_DATA) { 
      const char *smod = symname(pd->mod), *sname = symname(pd->id);
      unsigned char *pc = pd->data.buf; size_t i, n = pd->data.fill;
      if (smod && sname) { /* symbolic segment (nonfinal) */
        char *vcs = (pd->mut == MT_VAR) ? "var" : "const";
        assert(pd->align); assert(pd->ic.id == 0);
        chbsetf(g_watbuf, "(data $%s:%s %s align=%d", smod, sname, vcs, pd->align);
      } else { /* positioned segment (final, WAT-compatible) */
        chbuf_t cb = mkchb();
        assert(!pd->align); assert(pd->ic.in == IN_I32_CONST);
        chbsetf(g_watbuf, "(data (%s)", format_inscode(&pd->ic, &cb));
        chbfini(&cb);
      }  
      chbputs(" \"", g_watbuf);
      for (i = 0; i < n; ++i) {
        unsigned c = pc[i]; char ec = 0;
        switch (c) {
          case 0x09: ec = 't';  break; 
          case 0x0A: ec = 'n';  break;
          case 0x0D: ec = 'r';  break;
          case 0x22: ec = '\"'; break;
          case 0x27: ec = '\''; break;
          case 0x5C: ec = '\\'; break;
        }
        if (ec) chbputf(g_watbuf, "\\%c", ec);
        else if (' ' <= c && c <= 127) chbputc(c, g_watbuf); 
        else chbputf(g_watbuf, "\\%x%x", (c>>4)&0xF, c&0xF);
      }
      chbputc('\"', g_watbuf); 
      if (icblen(&pd->code) == 0) { /* leaf segment */
        chbputc(')', g_watbuf); 
        wat_line(chbdata(g_watbuf));
      } else { /* WCPL data segment with ref slots */
        size_t j;
        assert(smod && sname); /* not in standard WAT! */
        chbputs(" (", g_watbuf); 
        wat_line(chbdata(g_watbuf));
        g_watindent += 2;
        for (j = 0; j < icblen(&pd->code); ++j) {
          inscode_t *pic = icbref(&pd->code, j);
          assert(pic->in == IN_REF_DATA || pic->in == IN_DATA_PUT_REF);
          format_inscode(pic, g_watbuf);
          if (j == icblen(&pd->code)) chbputs("))", g_watbuf);
          wat_line(chbdata(g_watbuf));
        }
        g_watindent -= 2;
      }
    }
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
      if (!pic->id) chbputf(g_watbuf, "(param %s) ", valtype_name(pic->arg.u));
      else chbputf(g_watbuf, "(param $%s %s) ", symname(pic->id), valtype_name(pic->arg.u));
    }
    for (k = 0; k < vtblen(&pf->fs.restypes); ++k) {
      valtype_t *pvt = vtbref(&pf->fs.restypes, k);
      chbputf(g_watbuf, "(result %s) ", valtype_name(*pvt));
    }
    if (chblen(g_watbuf) > 0) { wat_line(chbdata(g_watbuf)); chbclear(g_watbuf); }
    for (/* use current j */; j < icblen(&pf->code); ++j) {
      inscode_t *pic = icbref(&pf->code, j);
      if (pic->in != IN_REGDECL) break;
      chbputf(g_watbuf, "(local $%s %s) ", symname(pic->id), valtype_name(pic->arg.u));
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
          chbputf(g_watbuf, " %s", symname(pic->id));
        }
        wat_line(chbdata(g_watbuf));
        j += n; /* account for n aditional inscodes */
      } else { /* taksed single inscode */
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
static wt_t lex(sws_t *pw, chbuf_t *pcb)
{
  char *tbase = chbdata(&pw->chars);
  int *pcuri = &pw->curi;
  int endi = (int)chblen(&pw->chars);
  int c;
#define readchar() c = *pcuri < endi ? tbase[(*pcuri)++] : fetchline(pw, &tbase, &endi)
#define unreadchar() (*pcuri)--
  chbclear(pcb);
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '\"') {
    chbputc(c, pcb);
    goto state_9;
  } else if (c == '0') {
    chbputc(c, pcb);
    goto state_8;
  } else if ((c >= '1' && c <= '9')) {
    chbputc(c, pcb);
    goto state_7;
  } else if (c == '+' || c == '-') {
    chbputc(c, pcb);
    goto state_6;
  } else if (c == '!' || (c >= '#' && c <= '\'') || c == '*' || (c >= '.' && c <= '/') || c == ':' || (c >= '<' && c <= 'Z') || c == '\\' || (c >= '^' && c <= 'z') || c == '|' || c == '~') {
    chbputc(c, pcb);
    goto state_5;
  } else if (c == ')') {
    chbputc(c, pcb);
    goto state_4;
  } else if (c == '(') {
    chbputc(c, pcb);
    goto state_3;
  } else if (c == ';') {
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
    return WT_WHITESPACE;
  } else if ((c >= '\t' && c <= '\n') || (c >= '\f' && c <= '\r') || c == ' ') {
    chbputc(c, pcb);
    goto state_1;
  } else {
    unreadchar();
    return WT_WHITESPACE;
  }
state_2:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == ';') {
    chbputc(c, pcb);
    goto state_34;
  } else {
    unreadchar();
    goto err;
  }
state_3:
  readchar();
  if (c == EOF) {
    return WT_LPAR;
  } else if (c == ';') {
    chbputc(c, pcb);
    goto state_33;
  } else {
    unreadchar();
    return WT_LPAR;
  }
state_4:
  return WT_RPAR;
state_5:
  readchar();
  if (c == EOF) {
    return WT_IDCHARS;
  } else if (c == '!' || (c >= '#' && c <= '\'') || (c >= '*' && c <= ':') || (c >= '<' && c <= 'Z') || c == '\\' || (c >= '^' && c <= 'z') || c == '|' || c == '~') {
    chbputc(c, pcb);
    goto state_5;
  } else {
    unreadchar();
    return WT_IDCHARS;
  }
state_6:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '0') {
    chbputc(c, pcb);
    goto state_31;
  } else if ((c >= '1' && c <= '9')) {
    chbputc(c, pcb);
    goto state_30;
  } else if (c == 'i') {
    chbputc(c, pcb);
    readchar();
    if (c == 'n') {
      chbputc(c, pcb);
      readchar();
      if (c == 'f') {
        chbputc(c, pcb);
        return WT_FLOAT;
      }
    }
    goto err;
  } else if (c == 'n') {
    chbputc(c, pcb);
    readchar();
    if (c == 'a') {
      chbputc(c, pcb);
      readchar();
      if (c == 'n') {
        chbputc(c, pcb);
        return WT_FLOAT;
      }
    }
    goto err;
  } else {
    unreadchar();
    goto err;
  }
state_7:
  readchar();
  if (c == EOF) {
    return WT_INT;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_22;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_21;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_7;
  } else {
    unreadchar();
    return WT_INT;
  }
state_8:
  readchar();
  if (c == EOF) {
    return WT_INT;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_23;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_22;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_21;
  } else if (c == 'x') {
    chbputc(c, pcb);
    goto state_20;
  } else {
    unreadchar();
    return WT_INT;
  }
state_9:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '\"') {
    chbputc(c, pcb);
    goto state_13;
  } else if (c == '\\') {
    chbputc(c, pcb);
    goto state_12;
  } else if (is8chead(c)) { 
    int u = c & 0xFF;
    if (u < 0xE0) { chbputc(c, pcb); goto state_10d; }
    if (u < 0xF0) { chbputc(c, pcb); goto state_10dd; }
    if (u < 0xF8) { chbputc(c, pcb); goto state_10ddd; }
    if (u < 0xFC) { chbputc(c, pcb); goto state_10dddd; }
    if (u < 0xFE) { chbputc(c, pcb); goto state_10ddddd; }
    unreadchar();
    goto err;
  } else if (!(c == '\n' || c == '\"' || c == '\\')) {
    chbputc(c, pcb);
    goto state_9;
  } else {
    unreadchar();
    goto err;
  }
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
    goto state_9;
  } else {
    unreadchar();
    goto err;
  }
state_12:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == 'u') {
    chbputc(c, pcb);
    goto state_16;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    unreadchar();
    goto state_15;
  } else if ((c >= '\t' && c <= '\n') || c == '\r' || c == ' ') {
    chbputc(c, pcb);
    goto state_14;
  } else if (c == '\"' || c == '\'' || c == '\\' || c == 'n' || c == 'r' || c == 't') {
    chbputc(c, pcb);
    goto state_9;
  } else {
    unreadchar();
    goto err;
  }
state_13:
  return WT_STRING;
state_14:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '\t' && c <= '\n') || c == '\r' || c == ' ') {
    chbputc(c, pcb);
    goto state_14;
  } else if (c == '\\') {
    chbputc(c, pcb);
    goto state_9;
  } else {
    unreadchar();
    goto err;
  }
state_15:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_19;
  } else {
    unreadchar();
    goto err;
  }
state_16:
  readchar();
  if (c == EOF) {
    goto err;
  } else if (c == '{') {
    chbputc(c, pcb);
    goto state_17;
  } else {
    unreadchar();
    goto err;
  }
state_17:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_18;
  } else {
    unreadchar();
    goto err;
  }
state_18:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_18;
  } else if (c == '}') {
    chbputc(c, pcb);
    goto state_9;
  } else {
    unreadchar();
    goto err;
  }
state_19:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_9;
  } else {
    unreadchar();
    goto err;
  }
state_20:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_29;
  } else {
    unreadchar();
    goto err;
  }
state_21:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_28;
  } else if (c == '+' || c == '-') {
    chbputc(c, pcb);
    goto state_27;
  } else {
    unreadchar();
    goto err;
  }
state_22:
  readchar();
  if (c == EOF) {
    return WT_FLOAT;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_24;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_22;
  } else {
    unreadchar();
    return WT_FLOAT;
  }
state_23:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_23;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_22;
  } else if (c == 'E' || c == 'e') {
    chbputc(c, pcb);
    goto state_21;
  } else {
    unreadchar();
    goto err;
  }
state_24:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_26;
  } else if (c == '+' || c == '-') {
    chbputc(c, pcb);
    goto state_25;
  } else {
    unreadchar();
    goto err;
  }
state_25:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_26;
  } else {
    unreadchar();
    goto err;
  }
state_26:
  readchar();
  if (c == EOF) {
    return WT_FLOAT;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_26;
  } else {
    unreadchar();
    return WT_FLOAT;
  }
state_27:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_28;
  } else {
    unreadchar();
    goto err;
  }
state_28:
  readchar();
  if (c == EOF) {
    return WT_FLOAT;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_28;
  } else {
    unreadchar();
    return WT_FLOAT;
  }
state_29:
  readchar();
  if (c == EOF) {
    return WT_INT;
  } else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
    chbputc(c, pcb);
    goto state_29;
  } else {
    unreadchar();
    return WT_INT;
  }
state_30:
  readchar();
  if (c == EOF) {
    return WT_INT;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_30;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_22;
  } else {
    unreadchar();
    return WT_INT;
  }
state_31:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_32;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_22;
  } else if (c == 'x') {
    chbputc(c, pcb);
    goto state_20;
  } else {
    unreadchar();
    goto err;
  }
state_32:
  readchar();
  if (c == EOF) {
    goto err;
  } else if ((c >= '0' && c <= '9')) {
    chbputc(c, pcb);
    goto state_32;
  } else if (c == '.') {
    chbputc(c, pcb);
    goto state_22;
  } else {
    unreadchar();
    goto err;
  }
state_33:
  return WT_BCSTART;
state_34:
  readchar();
  if (c == EOF) {
    goto eoferr;
  } else if (!(c == '\n')) {
    chbputc(c, pcb);
    goto state_35;
  } else {
    chbputc(c, pcb);
    goto state_36;
  }
state_35:
  readchar();
  if (c == EOF) {
    goto eoferr;
  } else if (!(c == '\n')) {
    chbputc(c, pcb);
    goto state_35;
  } else {
    chbputc(c, pcb);
    goto state_36;
  }
state_36:
  return WT_LC;

  err:
  eoferr:
    return WT_EOF;
#undef readchar
#undef unreadchar
}

/* report error, possibly printing location information */
static void vseprintf(sws_t *pw, const char *fmt, va_list args)
{
  chbuf_t cb = mkchb(); const char *s;
  int ln = 0, off = 0; assert(fmt);
  if (pw && pw->infile) chbputf(&cb, "%s:", pw->infile);
  if (pw) {
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

static bool lexbc(sws_t *pw, chbuf_t *pcb) 
{
  char *tbase = chbdata(&pw->chars);
  int *pcuri = &pw->curi;
  int endi = (int)chblen(&pw->chars);
  int c;
#define readchar() c = *pcuri < endi ? tbase[(*pcuri)++] : fetchline(pw, &tbase, &endi)
  while (true) {
    readchar();
    if (c == EOF) return false;
    chbputc(c, pcb);
    if (c == '(') {
      readchar();
      if (c == EOF) return false;
      chbputc(c, pcb);
      if (c == ';') {
        if (!lexbc(pw, pcb)) return false;
      }      
    } else if (c == ';') {
      readchar();
      if (c == EOF) return false;
      chbputc(c, pcb);
      if (c == ')') return true;
    }
  }  
#undef readchar
}

static wt_t peekt(sws_t *pw) 
{ 
  if (!pw->gottk) {
    do { /* fetch next non-whitespace token */
      pw->posl = pw->lno, pw->posi = pw->curi; 
      pw->ctk = lex(pw, &pw->token);
      if (pw->ctk == WT_EOF && !pw->inateof) {
        assert(false);
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
      }
    } while (WT_WHITESPACE <= pw->ctk && pw->ctk <= WT_BC); 
    pw->gottk = true; 
    pw->tokstr = chbdata(&pw->token); 
  } 
  return pw->ctk; 
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

static double scan_float(sws_t *pw, const char *s)
{
  double d; char *e; errno = 0; 
  if (streql(s, "+nan")) return HUGE_VAL - HUGE_VAL;
  if (streql(s, "+inf")) return HUGE_VAL;
  if (streql(s, "-inf")) return -HUGE_VAL;
  d = strtod(s, &e);
  if (errno || *e != 0) seprintf(pw, "invalid floating-point literal");
  return d;
}

/* size of buffer large enough to hold char escapes */
#define SBSIZE 32
/* convert a single WAT text escape sequence (-1 on error) */
static unsigned long strtowatec(const char *s, char** ep)
{
  char buf[SBSIZE+1]; int c; unsigned long l;
  assert(s);
  if (!*s) goto err;
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
err:
  if (ep) *ep = (char*)s;
  errno = EINVAL;
  return (unsigned long)(-1);
}

static char *scan_string(sws_t *pw, const char *s, chbuf_t *pcb)
{
  int c = *s++; assert(c == '"');
  chbclear(pcb);
  while (*s != '"') {
    c = *s; errno = 0;
    if (is8ctail(c)) goto err;
    if (is8chead(c)) { 
      char *e; unsigned long ul = strtou8c(s, &e); 
      if (errno || ul > 0x10FFFFUL) goto err;
      chbput(s, e-s, pcb); /* as-is! */
      s = e;
    } else if (c == '\\' && isxdigit(s[1]) && isxdigit(s[2])) {
      int b, h; 
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
err:
  seprintf(pw, "invalid string literal");
  return NULL;
}

static void parse_mod_id(sws_t *pw, sym_t *pmid, sym_t *pid)
{
  char *s, *sep; 
  if (peekt(pw) != WT_ID || (sep = strchr((s = pw->tokstr), ':')) == NULL) { 
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
    return 0; /* never */
  } else {
    sym_t id;
    assert(*s == '$');
    id = intern(s+1);
    dropt(pw);
    return id;
  }
}

static sym_t parse_id_string(sws_t *pw, chbuf_t *pcb, const char *chset)
{
  char *s; size_t n; bool ok = false;
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
      /* fixme: case INSIG_X_Y: call_indirect, return_call_indirect */
      case INSIG_T: { /* select */
        pic->arg.u = parse_valtype(pw);
      } break;
      case INSIG_I32: case INSIG_I64: {
        numval_t v;
        if (peekt(pw) == WT_INT) {
          scan_integer(pw, pw->tokstr, &v); dropt(pw);
          pic->arg = v; 
          if (is == INSIG_I32 && ((v.i << 32) >> 32) != v.i)
            seprintf(pw, "i32 literal is out of range %s", pw->tokstr);
        } else {
          seprintf(pw, "invalid integer literal %s", pw->tokstr);
        }
      } break;
      case INSIG_MEMARG: {
        char *s; unsigned long offset, align;
        if (peekt(pw) == WT_KEYWORD && (s = strprf(pw->tokstr, "offset=")) != NULL) {
          char *e; errno = 0; offset = strtoul(s, &e, 10); 
          if (errno || *e != 0 || offset > UINT_MAX) 
            seprintf(pw, "invalid offset= argument");
          else dropt(pw);
        } else { /* we require it! */
          seprintf(pw, "missing offset= argument"); 
        }
        if (peekt(pw) == WT_KEYWORD && (s = strprf(pw->tokstr, "align=")) != NULL) {
          char *e; errno = 0; align = strtoul(s, &e, 10); 
          if (errno || *e != 0 || (align != 1 && align != 2 && align != 4 && align != 8 && align != 16)) 
            seprintf(pw, "invalid align= argument");
          else dropt(pw);
        } else { /* we require it! */
          seprintf(pw, "missing align= argument"); 
        }
        pic->arg.u = offset;
        switch (align) { /* fixme: use ntz? */
          case 1:  pic->arg2.u = 0; break;
          case 2:  pic->arg2.u = 1; break;
          case 4:  pic->arg2.u = 2; break;
          case 8:  pic->arg2.u = 3; break;
          case 16: pic->arg2.u = 4; break;
          default: assert(false);
        }
      } break;
      case INSIG_F32: case INSIG_F64: {
        if (peekt(pw) == WT_FLOAT || peekt(pw) == WT_INT) {
          double d = scan_float(pw, pw->tokstr);
          if (is == INSIG_F32) pic->arg.f = (float)d;
          else pic->arg.d = d;
          dropt(pw);
        } else {
          seprintf(pw, "invalid floating-point literal %s", pw->tokstr);
        } 
      } break;
      case INSIG_RD: { /* ref.data -- not in WASM! */
        char *s; long offset;
        sym_t mod, id; parse_mod_id(pw, &mod, &id);
        pic->id = id; pic->arg2.mod = mod;
        pic->arg.i = 0; /* offset; check if given explicitly */
        if (peekt(pw) == WT_KEYWORD && (s = strprf(pw->tokstr, "offset=")) != NULL) {
          char *e; errno = 0; offset = strtol(s, &e, 10); 
          if (errno || *e != 0 || offset < INT_MIN || offset > INT_MAX) 
            seprintf(pw, "invalid offset= argument");
          else {
            pic->arg.i = offset;
            dropt(pw);
          }
        }
      } break;
      case INSIG_PR: { /* data.put_ref -- not in WASM! */
        char *s; unsigned long offset;
        if (peekt(pw) == WT_KEYWORD && (s = strprf(pw->tokstr, "offset=")) != NULL) {
          char *e; errno = 0; offset = strtoul(s, &e, 10); 
          if (errno || *e != 0 || offset > UINT_MAX) 
            seprintf(pw, "invalid offset= argument");
          else {
            pic->arg.u = offset;
            dropt(pw);
          }
        } else { /* we require it! */
          seprintf(pw, "missing offset= argument"); 
        }
      } break;
      /* case INSIG_LS_L: fixme; use *pexb */
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

static void parse_modulefield(sws_t *pw, wat_module_t* pm)
{
  chbuf_t cb = mkchb();
  expect(pw, "(");
  if (ahead(pw, "import")) {
    watie_t *pi = watiebnewbk(&pm->imports, IEK_UNKN);
    dropt(pw);
    pi->mod = parse_id_string(pw, &cb, STR_az STR_AZ STR_09 "_.");
    pi->id = parse_id_string(pw, &cb, STR_az STR_AZ STR_09 "_");
    parse_import_des(pw, pi);
  } else if (ahead(pw, "data")) {
    watie_t *pd = watiebnewbk(&pm->exports, IEK_DATA);
    char *s;
    dropt(pw);
    parse_mod_id(pw, &pd->mod, &pd->id);
    if (peekt(pw) == WT_KEYWORD && streql(pw->tokstr, "var")) {
      pd->mut = MT_VAR; dropt(pw);
    }
    if (peekt(pw) == WT_KEYWORD && streql(pw->tokstr, "const")) {
      pd->mut = MT_CONST; dropt(pw);
    }
    if (peekt(pw) == WT_KEYWORD && (s = strprf(pw->tokstr, "align=")) != NULL) {
      char *e; unsigned long align; errno = 0; align = strtoul(s, &e, 10); 
      if (errno || *e != 0 || (align != 1 && align != 2 && align != 4 && align != 8 && align != 16)) 
        seprintf(pw, "invalid align= argument");
      pd->align = (int)align;
      dropt(pw);
    }
    if (peekt(pw) == WT_STRING) scan_string(pw, pw->tokstr, &pd->data); 
    else seprintf(pw, "missing data string");
    dropt(pw);
    if (ahead(pw, "(")) { /* WCPL non-lead data segment */
      dropt(pw);
      while (!ahead(pw, ")")) {
        inscode_t *pic = icbnewbk(&pd->code);
        parse_ins(pw, pic, &pd->code);
        if (pic->in != IN_REF_DATA && pic->in != IN_DATA_PUT_REF)
          seprintf(pw, "unexpected instruction in WCPL data segment");
      }
      if (pd->align < 4) /* wasm32 : alignment of a pointer */
        seprintf(pw, "unexpected alignment of non-leaf data segment");
      expect(pw, ")");
    } 
  } else if (ahead(pw, "memory")) {
    watie_t *pe = watiebnewbk(&pm->exports, IEK_MEM);
    dropt(pw);
    parse_mod_id(pw, &pe->mod, &pe->id);
    while (ahead(pw, "(")) {
      dropt(pw);
      if (ahead(pw, "export")) {
        sym_t id;
        dropt(pw);
        id = parse_id_string(pw, &cb, STR_az STR_AZ STR_09 "_");
        if (id != pe->id) seprintf(pw, "unexpected export rename"); 
        pe->exported = true;
      } else seprintf(pw, "unexpected memory header field %s", pw->tokstr);
      expect(pw, ")");
    }
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
    pe->mut = MT_CONST;
    while (ahead(pw, "(")) {
      dropt(pw);
      if (ahead(pw, "export")) {
        sym_t id;
        dropt(pw);
        id = parse_id_string(pw, &cb, STR_az STR_AZ STR_09 "_");
        if (id != pe->id) seprintf(pw, "unexpected export rename"); 
        pe->exported = true;
      } else if (ahead(pw, "mut")) {
        dropt(pw);
        pe->mut = MT_VAR;
        pe->vt = parse_valtype(pw);
      } else {
        parse_ins(pw, &pe->ic, NULL);
      }
      expect(pw, ")");
    }
    if (pe->mut == MT_CONST) {
      pe->vt = parse_valtype(pw);
    }
    if (pe->ic.in == IN_PLACEHOLDER) {
      expect(pw, "(");
      parse_ins(pw, &pe->ic, NULL);
      expect(pw, ")");
    }
  } else if (ahead(pw, "func")) {
    watie_t *pe = watiebnewbk(&pm->exports, IEK_FUNC);
    dropt(pw);
    parse_mod_id(pw, &pe->mod, &pe->id);
    while (ahead(pw, "(")) {
      dropt(pw);
      if (ahead(pw, "export")) {
        sym_t id;
        dropt(pw);
        id = parse_id_string(pw, &cb, STR_az STR_AZ STR_09 "_");
        if (id != pe->id) seprintf(pw, "unexpected export rename"); 
        pe->exported = true;
      } else if (ahead(pw, "param")) {
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
      else if (vtblen(&pe->fs.partypes) == 2 && *vtbref(&pe->fs.restypes, 0) == VT_I32
               && *vtbref(&pe->fs.restypes, 1) == VT_I32)
        pm->main = MAIN_ARGC_ARGV;
      else
        seprintf(pw, "invalid main() function argument types");
    }
    while (!ahead(pw, ")")) {
      inscode_t *pic = icbnewbk(&pe->code);
      parse_ins(pw, pic, &pe->code);
    }
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

void read_wat_module(const char *fname, wat_module_t* pm)
{
  sws_t *pw = newsws(fname);
  if (pw) {
    wat_module_clear(pm);
    pm->main = MAIN_ABSENT;
    parse_module(pw, pm);
    freesws(pw);
  } else {
    exprintf("cannot read object file: %s", fname);
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
  pi = bufbsearch(&pmi->imports, pmii, modid_cmp);
  if (!pi) exprintf("cannot locate import '%s:%s' in '%s' module", 
    symname(pmii->mod), symname(pmii->id), symname(pmi->name));
  if (!bufsearch(&pm->imports, pmii, modid_cmp)) { /* linear! */
    watie_t *newpi = watiebnewbk(&pm->imports, pi->iek); 
    newpi->mod = pi->mod; newpi->id = pi->id;
    memswap(pi, newpi, sizeof(watie_t)); /* mod/id still there so bsearch works */
  }
  return true; 
}  

/* find pmii definition, trace it for dependants amd move over to pm */
static void process_depglobal(modid_t *pmii, wat_module_buf_t *pwb, buf_t *pdg, wat_module_t* pm)
{
  wat_module_t* pmi = bufbsearch(pwb, &pmii->mod, sym_cmp); 
  if (!pmi) exprintf("cannot locate '%s' module (looking for %s)", symname(pmii->mod), symname(pmii->id));  
  switch (pmii->iek) {
    case IEK_MEM: {
      watie_t *pe = bufbsearch(&pmi->exports, pmii, modid_cmp), *newpe;
      if (!pe || pe->iek != IEK_MEM) 
        exprintf("cannot locate memory '%s' in '%s' module", symname(pmii->id), symname(pmii->mod));  
      newpe = watiebnewbk(&pm->exports, IEK_MEM); newpe->mod = pe->mod; newpe->id = pe->id; 
      memswap(pe, newpe, sizeof(watie_t)); /* mod/id still there so bsearch works */
      newpe->exported = true; /* memory should be exported! */
    } break;
    case IEK_DATA: { 
      watie_t *pd = bufbsearch(&pmi->exports, pmii, modid_cmp), *newpd;
      if (!pd || pd->iek != IEK_DATA) 
        exprintf("cannot locate data '%s' in '%s' module", symname(pmii->id), symname(pmii->mod));  
      newpd = watiebnewbk(&pm->exports, IEK_DATA); newpd->mod = pd->mod; newpd->id = pd->id; 
      memswap(pd, newpd, sizeof(watie_t)); /* mod/id still there so bsearch works */
    } break;
    case IEK_GLOBAL: {
      watie_t *pe = bufbsearch(&pmi->exports, pmii, modid_cmp), *newpe;
      if (!pe || pe->iek != IEK_GLOBAL) 
        exprintf("cannot locate global '%s' in '%s' module", symname(pmii->id), symname(pmii->mod));  
      if (pe->ic.in == IN_GLOBAL_GET) {
        inscode_t *pic = &pe->ic;
        modid_t mi; mi.mod = pic->arg2.mod; mi.id = pic->id; mi.iek = IEK_GLOBAL;
        assert(mi.mod != 0 && mi.id != 0); /* must be relocatable! */
        if (process_subsystem_depglobal(&mi, pmi, pm)) /* ok */ ;
        else if (!bufsearch(pdg, &mi, modid_cmp)) *(modid_t*)bufnewbk(pdg) = mi;              
      }
      newpe = watiebnewbk(&pm->exports, IEK_GLOBAL); newpe->mod = pe->mod; newpe->id = pe->id; 
      memswap(pe, newpe, sizeof(watie_t)); /* mod/id still there so bsearch works */
      newpe->exported = false;
    } break;
    case IEK_FUNC: {
      watie_t *pf = bufbsearch(&pmi->exports, pmii, modid_cmp), *newpf;
      modid_t mi; size_t i;
      if (!pf || pf->iek != IEK_FUNC) 
        exprintf("cannot locate func '%s' in '%s' module", symname(pmii->id), symname(pmii->mod));  
      for (i = 0; i < icblen(&pf->code); ++i) {
        inscode_t *pic = icbref(&pf->code, i);
        switch (pic->in) {
          case IN_GLOBAL_GET: case IN_GLOBAL_SET:
          case IN_CALL: case IN_RETURN_CALL: {
            mi.mod = pic->arg2.mod; mi.id = pic->id;
            mi.iek = (pic->in == IN_GLOBAL_GET || pic->in == IN_GLOBAL_SET) ? IEK_GLOBAL : IEK_FUNC;
            assert(mi.mod != 0 && mi.id != 0); /* must be relocatable! */
            if (process_subsystem_depglobal(&mi, pmi, pm)) /* ok */ ;
            else if (!bufsearch(pdg, &mi, modid_cmp)) *(modid_t*)bufnewbk(pdg) = mi;              
          } break;
          case IN_REF_DATA: {
            mi.mod = pic->arg2.mod; mi.id = pic->id;
            mi.iek = IEK_DATA;
            if (!bufsearch(pdg, &mi, modid_cmp)) *(modid_t*)bufnewbk(pdg) = mi;              
          } break;
        }
      }
      newpf = watiebnewbk(&pm->exports, IEK_FUNC); newpf->mod = pf->mod; newpf->id = pf->id; 
      memswap(pf, newpf, sizeof(watie_t)); /* mod/id still there so bsearch works */
      newpf->exported = false;
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
#define dsmebinit(mem) bufinit(mem, sizeof(dsme_t))
#define dsmeblen(pb) buflen(pb)
#define dsmebref(pb, i) ((dsme_t*)bufref(pb, i))
#define dsmebnewbk(pb) dsmeinit(bufnewbk(pb))

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
  assert(pb); assert(pb->esz = sizeof(dsme_t));
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

/* data placement map */
typedef struct dpmentry {
  sym_t mod, id;  /* mod:id; used for sorting */
  size_t address; /* absolute addr in data segment */  
} dpme_t; 

/* remove dependence on non-standard WAT features and instructions */
size_t watify_wat_module(wat_module_t* pm)
{
  size_t curaddr = g_sdbaddr;
  chbuf_t dseg = mkchb(); size_t i;
  buf_t dpmap = mkbuf(sizeof(dpme_t));
  dsmebuf_t dsmap; dsmebinit(&dsmap);
  
  /* concatenate dss into one big data chunk */
  for (i = 0; i < watieblen(&pm->exports); /* del or bumpi */) {
    watie_t *pd = watiebref(&pm->exports, i);
    if (pd->iek == IEK_DATA) {
      dpme_t *pme; size_t addr;
      if (pd->mut == MT_CONST && icblen(&pd->code) == 0) {
        /* leaf, read-only: try to merge it with equals */
        dsme_t e, *pe; dsmeinit(&e); 
        chbcpy(&e.data, &pd->data);
        e.align = pd->align; assert(pd->align);
        pe = bufbsearch(&dsmap, &e, dsme_cmp);
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
          bufqsort(&dsmap, dsme_cmp);
          curaddr = addr + buflen(&pe->data);
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
              pdpme = bufbsearch(&dpmap, &mi, modid_cmp);
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
      watiefini(pd); bufrem(&pm->exports, i);
    } else {
      ++i;
    }
  }
  
  /* prepare dpmap for binary search */
  bufqsort(&dpmap, modid_cmp);
  
  /* convert our IN_REF_DATA to IN_I32_CONST, patch memory export */
  for (i = 0; i < watieblen(&pm->exports); ++i) {
    watie_t *pe = watiebref(&pm->exports, i);
    if (pe->iek == IEK_FUNC) {
      watie_t *pf = pe; size_t j;
      for (j = 0; j < icblen(&pf->code); ++j) {
        inscode_t *pic = icbref(&pf->code, j);
        if (pic->in == IN_REF_DATA) {
          modid_t mi; dpme_t *pdpme; 
          mi.mod = pic->arg2.mod; mi.id = pic->id;
          pdpme = bufbsearch(&dpmap, &mi, modid_cmp);
          if (!pdpme) exprintf("internal error: cannot patch ref.data $%s:%s", 
            symname(mi.mod), symname(mi.id));
          pic->in = IN_I32_CONST;
          pic->arg.i = (long long)pdpme->address + pic->arg.i;
        }
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
  
  chbfini(&dseg);
  dsmebfini(&dsmap);
  buffini(&dpmap);
  
  /* return absolute address of dseg's end */
  return curaddr;
}

/* patch initial stack pointer and memory size */
static void finalize_wat_module(wat_module_t* pm, size_t dsegend)
{
  size_t i, n, align, spaddr, ememaddr, mempages;
  spaddr = dsegend + g_stacksz; align = 16; 
  n = spaddr % align; if (n > 0) spaddr += align - n;
  ememaddr = spaddr; align = 64*1024; /* page = 64K */
  n = ememaddr % align; if (n > 0) ememaddr += align - n;
  mempages = ememaddr/align;
  for (i = 0; i < watieblen(&pm->exports); ++i) {
    watie_t *pe = watiebref(&pm->exports, i);
    if (pe->iek == IEK_GLOBAL) {
      if (pe->mod == g_env_mod && pe->id == g_sp_id) {
        if (pe->ic.in == IN_I32_CONST) { /* wasm32 */
          pe->ic.arg.u = spaddr;
        }
      }
    } else if (pe->iek == IEK_MEM) {
      if (pe->mod == g_env_mod && pe->id == g_lm_id) {
        pe->n = mempages;
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
    size_t mainj = SIZE_MAX;
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
          if (mainj != SIZE_MAX)
            exprintf("duplicate main() in %s module", 
              symname(mainmod));
          mainj = j;
        } 
      }
    }
    /* trace dependencies */
    for (j = 0; j < watieblen(&pmi->imports); ++j) {
      watie_t *pi = watiebref(&pmi->imports, j);
      if (bufsearch(&extmodnames, &pi->mod, sym_cmp) == NULL)
        *(sym_t*)bufnewbk(&extmodnames) = pi->mod;
    }
    if (bufsearch(&curmodnames, &pmi->name, sym_cmp) != NULL)
      exprintf("duplicate module: %s", symname(pmi->name));
    else *(sym_t*)bufnewbk(&curmodnames) = pmi->name;
  }
  if (!mainmod) logef("error: main() function not found\n"); /* fixme: should be exprintf */
  else logef("# main() found in '%s' module\n", symname(mainmod));

  /* add missing library modules */
  for (i = 0; i < buflen(&extmodnames); ++i) {
    sym_t *pmn = bufref(&extmodnames, i);
    if (bufsearch(&curmodnames, pmn, sym_cmp) == NULL) {
      if (*pmn == g_wasi_mod) {
        logef("# found WASI subsystem dependence: '%s' module\n", symname(g_wasi_mod));
        /* nothing to load  */
      } else {
        sym_t rtn = *pmn; wat_module_t* pnewm;
        if (rtn == g_env_mod) {
          if (mt == MAIN_VOID) rtn = internf("%s.void", symname(rtn));
          else if (mt == MAIN_ARGC_ARGV) rtn = internf("%s.argv", symname(rtn));
          envmod = rtn;
        }
        logef("# found library dependence: '%s' module\n", symname(rtn));
        load_library_wat_module(rtn, (pnewm = wat_module_buf_newbk(pwb)));
        if (pnewm->main != MAIN_ABSENT)
          exprintf("unexpected main() in library module '%s'", symname(rtn)); 
        /* trace sub-dependencies */
        for (j = 0; j < watieblen(&pnewm->imports); ++j) {
          watie_t *pi = watiebref(&pnewm->imports, j);
          if (bufsearch(&extmodnames, &pi->mod, sym_cmp) == NULL)
            *(sym_t*)bufnewbk(&extmodnames) = pi->mod;
        }
      }
    }
  }
  
  /* first, sort modules by module name for bsearch */
  bufqsort(pwb, sym_cmp);
  /* now sort module's field lists by mod-name pair */
  for (i = 0; i < wat_module_buf_len(pwb); ++i) {
    wat_module_t* pmi = wat_module_buf_ref(pwb, i);
    bufqsort(&pmi->imports, modid_cmp);    
    bufqsort(&pmi->exports, modid_cmp);    
  }

  /* now seed depglobals and use it to move globals to pm */
  pmodid = bufnewbk(&depglobals); 
  pmodid->mod = mainmod; pmodid->id = mainid; pmodid->iek = IEK_FUNC;
  pmodid = bufnewbk(&depglobals); 
  pmodid->mod = mainmod; pmodid->id = startid; pmodid->iek = IEK_FUNC;
  if (envmod) { /* need to ref linear memory explicitly */
    pmodid = bufnewbk(&depglobals); 
    pmodid->mod = g_env_mod; pmodid->id = g_lm_id; pmodid->iek = IEK_MEM;  
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
