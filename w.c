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
#include "l.h"
#include "w.h"

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
  bufinit(&pf->argtypes, sizeof(valtype_t));
  bufinit(&pf->rettypes, sizeof(valtype_t));
  return pf;
}

void fsfini(funcsig_t* pf)
{
  buffini(&pf->argtypes);
  buffini(&pf->rettypes);
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
  if (vtblen(&pf1->argtypes) != vtblen(&pf2->argtypes)) return false;
  if (vtblen(&pf1->rettypes) != vtblen(&pf2->rettypes)) return false;
  for (i = 0; i < vtblen(&pf1->argtypes); ++i)
    if (*vtbref(&pf1->argtypes, i) != *vtbref(&pf2->argtypes, i)) return false;
  for (i = 0; i < vtblen(&pf1->rettypes); ++i)
    if (*vtbref(&pf1->rettypes, i) != *vtbref(&pf2->rettypes, i)) return false;
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
  for (i = 0; i < argc; ++i) *vtbnewbk(&ft.argtypes) = va_arg(args, valtype_t);
  for (i = 0; i < retc; ++i) *vtbnewbk(&ft.rettypes) = va_arg(args, valtype_t);
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

static void relocate_ins(inscode_t *pic, icbuf_t *prelb)
{
  sym_t k = pic->relkey; instr_t in = pic->in;
  inscode_t *pri = bufbsearch(prelb, &k, sym_cmp);
  if (!pri) exprintf("internal error: can't relocate symbol %s", symname(k));
  assert(pri->in == IN_UNREACHABLE);
  /* copy over arguments part, keeping the original instruction */
  memcpy(pic, pri, sizeof(inscode_t));
  pic->relkey = 0; pic->in = in;
} 

static void wasm_expr(icbuf_t *pcb, icbuf_t *prelb)
{
  size_t i;
  for (i = 0; i < icblen(pcb); ++i) {
    inscode_t *pic = icbref(pcb, i);
    if (pic->relkey) relocate_ins(pic, prelb); 
    wasm_in(pic->in);
    switch (instr_sig(pic->in)) {
      case INSIG_NONE:
        break;
      case INSIG_BT:   case INSIG_L:   
      case INSIG_X:    case INSIG_T:
      case INSIG_I32:  case INSIG_I64:
        wasm_unsigned(pic->arg.u); 
        break;
      case INSIG_X_Y:  case INSIG_MEMARG:
        wasm_unsigned(pic->arg.u); wasm_unsigned(pic->argu2); 
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
    wasm_unsigned(vtblen(&pft->argtypes));
    for (k = 0; k < vtblen(&pft->argtypes); ++k) {
      wasm_byte(*vtbref(&pft->argtypes, k));
    }    
    wasm_unsigned(vtblen(&pft->rettypes));
    for (k = 0; k < vtblen(&pft->rettypes); ++k) {
      wasm_byte(*vtbref(&pft->rettypes, k));
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

static void wasm_globals(entbuf_t *pgdb, icbuf_t *prelb)
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
    wasm_expr(&pe->code, prelb);
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

static void wasm_elems(esegbuf_t *pesb, icbuf_t *prelb)
{
  size_t i, k;
  if (!pesb) return;
  wasm_section_start(SI_ELEMENT);
  for (i = 0; i < esegblen(pesb); ++i) {
    eseg_t *ps = esegbref(pesb, i);
    wasm_byte(ps->esm);
    switch (ps->esm) {
       case ES_ACTIVE_EIV: /* e fiv */
         wasm_expr(&ps->code, prelb);
         wasm_unsigned(idxblen(&ps->fidxs));
         goto emitfiv;
       case ES_ACTIVE_TEKIV: /* ti e k fiv */
         wasm_unsigned(ps->idx);
         wasm_expr(&ps->code, prelb);
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
         wasm_expr(&ps->code, prelb);
         goto emitev;
       case ES_ACTIVE_IETEV: /* ti e rt ev */       
         wasm_unsigned(ps->idx);
         wasm_expr(&ps->code, prelb);
         /* fall thru */
       case ES_DECLVE_TEV: /* rt ev */
       case ES_PASSIVE_TEV: /* rt ev */
         wasm_byte(ps->rt); /* must be RT_XXX */       
       emitev:
         wasm_unsigned(ps->cdc);
         wasm_expr(&ps->codes, prelb);
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

static void wasm_codes(entbuf_t *pfdb, icbuf_t *prelb)
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
    wasm_expr(&pe->code, prelb);
    wasm_code_end();
    wasm_section_bumpc();
  }
  wasm_section_end();
}

static void wasm_datas(dsegbuf_t *pdsb, icbuf_t *prelb)
{
  size_t i;
  if (!pdsb) return;
  wasm_section_start(SI_DATA);
  for (i = 0; i < dsegblen(pdsb); ++i) {
    dseg_t *ps = dsegbref(pdsb, i);
    wasm_byte(ps->dsm);
    switch (ps->dsm) {
      case DS_ACTIVE_MI: wasm_unsigned(ps->idx); /* fall thru */
      case DS_ACTIVE: wasm_expr(&ps->code, prelb); /* fall thru */
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
  icbinit(&pm->reloctab);
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
  icbfini(&pm->reloctab);
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
  wasm_globals(&pm->globdefs, &pm->reloctab);
  wasm_exports(&pm->funcdefs, &pm->tabdefs, &pm->memdefs, &pm->globdefs);
  wasm_start(&pm->funcdefs);
  wasm_elems(&pm->elemdefs, &pm->reloctab);
  wasm_datacount(&pm->datadefs);
  wasm_codes(&pm->funcdefs, &pm->reloctab);
  wasm_datas(&pm->datadefs, &pm->reloctab);
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
    case IN_CALL:          case IN_RETURN_CALL:
      return INSIG_X;
    case IN_CALL_INDIRECT: case IN_RETURN_CALL_INDIRECT:
      return INSIG_X_Y;
    case IN_SELECT_T:
      return INSIG_T;  
    case IN_LOCAL_GET:     case IN_LOCAL_SET:    case IN_LOCAL_TEE: 
    case IN_GLOBAL_GET:    case IN_GLOBAL_SET:   case IN_TABLE_GET:    case IN_TABLE_SET:
      return INSIG_X;
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
      return INSIG_X;
    case IN_TABLE_INIT:    case IN_TABLE_COPY:
      return INSIG_X_Y;
  }
  return INSIG_NONE;
}

const char *format_inscode(inscode_t *pic, chbuf_t *pcb)
{
  chbclear(pcb);
  if (pic->in == IN_REGDECL) {
    const char *ts = valtype_name((valtype_t)pic->arg.u);
    chbputf(pcb, "register %s %s", ts, symname(pic->relkey)); 
    return chbdata(pcb);
  }
  chbputs(instr_name(pic->in), pcb);
  switch (instr_sig(pic->in)) {
    case INSIG_NONE:
      break;
    case INSIG_BT: {
      const char *s = valtype_name((valtype_t)pic->arg.u);
      if (*s != '?') chbputf(pcb, " %s", s); 
      else chbputf(pcb, " %llu", pic->arg.u); 
    } break;
    case INSIG_L:   
    case INSIG_X:    case INSIG_T:
    case INSIG_I32:  case INSIG_I64:
      if (pic->relkey) chbputf(pcb, " $%s", symname(pic->relkey)); 
      else chbputf(pcb, " %llu", pic->arg.u); 
      break;
    case INSIG_X_Y:  case INSIG_MEMARG:
      if (pic->relkey) chbputf(pcb, " $%s %u", symname(pic->relkey), pic->argu2); 
      else chbputf(pcb, " %llu %u", pic->arg.u, pic->argu2); 
      break;
    case INSIG_F32:
      if (pic->relkey) chbputf(pcb, " $%s", symname(pic->relkey)); 
      else chbputf(pcb, " %g", (double)pic->arg.f); 
      break;
    case INSIG_F64:
      if (pic->relkey) chbputf(pcb, " $%s", symname(pic->relkey)); 
      else chbputf(pcb, " %g", pic->arg.d); 
      break;
    case INSIG_LS_L:
      assert(false); /* fixme */
    default:
      assert(false);     
  }
  return chbdata(pcb);
}


/* wat text representations */

wati_t* watiinit(wati_t* pi, entkind_t ek)
{
  memset(pi, 0, sizeof(wati_t));
  pi->ek = ek;
  fsinit(&pi->fs);
  return pi;
}

void watifini(wati_t* pi)
{
  fsfini(&pi->fs);
}

void watibfini(watibuf_t* pb)
{
  wati_t *pf; size_t i;
  assert(pb); assert(pb->esz = sizeof(wati_t));
  pf = (wati_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) watifini(pf+i);
  buffini(pb);
}

watf_t* watfinit(watf_t* pf)
{
  memset(pf, 0, sizeof(watf_t));
  fsinit(&pf->fs);
  icbinit(&pf->code);
  return pf;
}

void watffini(watf_t* pf)
{
  fsfini(&pf->fs);
  icbfini(&pf->code);
}

void watfbfini(watfbuf_t* pb)
{
  watf_t *pf; size_t i;
  assert(pb); assert(pb->esz = sizeof(watf_t));
  pf = (watf_t*)(pb->buf);
  for (i = 0; i < pb->fill; ++i) watffini(pf+i);
  buffini(pb);
}

wat_module_t* wat_module_init(wat_module_t* pm)
{
  memset(pm, 0, sizeof(wat_module_t));
  watibinit(&pm->imports);
  watfbinit(&pm->funcs);
  return pm;
}

void wat_module_fini(wat_module_t* pm)
{
  watibfini(&pm->imports);
  watfbfini(&pm->funcs);
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

static void wat_imports(watibuf_t *pib)
{
  size_t i, j;
  for (i = 0; i < watiblen(pib); ++i) {
    wati_t *pi = watibref(pib, i);
    chbsetf(g_watbuf, "(import \"%s\" \"%s\" ", symname(pi->mod), symname(pi->name));
    switch (pi->ek) {
      case EK_FUNC: {
        chbputf(g_watbuf, "(func");
        for (j = 0; j < vtblen(&pi->fs.argtypes); ++j) {
          valtype_t *pvt = vtbref(&pi->fs.argtypes, j);
          chbputf(g_watbuf, " (param %s)", valtype_name(*pvt));
        }
        for (j = 0; j < vtblen(&pi->fs.rettypes); ++j) {
          valtype_t *pvt = vtbref(&pi->fs.rettypes, j);
          chbputf(g_watbuf, " (result %s)", valtype_name(*pvt));
        }
        chbputc(')', g_watbuf); 
      } break;
      case EK_TABLE: {
      } break;
      case EK_MEM: {
      } break;
      case EK_GLOBAL: {
      } break;
    }
    wat_line(chbdata(g_watbuf));
  }
}

static void wat_funcs(watfbuf_t *pfb)
{
  size_t i, j, k;
  for (i = 0; i < watfblen(pfb); ++i) {
    watf_t *pf = watfbref(pfb, i);
    chbsetf(g_watbuf, "(func $%s", symname(pf->name));
    for (j = 0; j < vtblen(&pf->fs.argtypes); ++j) {
      valtype_t *pvt = vtbref(&pf->fs.argtypes, j);
      inscode_t *pic = icbref(&pf->code, j);
      assert(pic->in == IN_REGDECL && pic->relkey);
      assert(*pvt == (valtype_t)pic->arg.u);
      chbputf(g_watbuf, " (param $%s %s)", symname(pic->relkey), valtype_name(pic->arg.u));
    }
    for (k = 0; k < vtblen(&pf->fs.rettypes); ++k) {
      valtype_t *pvt = vtbref(&pf->fs.rettypes, k);
      chbputf(g_watbuf, " (result %s)", valtype_name(*pvt));
    }
    wat_line(chbdata(g_watbuf)); chbclear(g_watbuf);
    g_watindent += 2;
    for (/* use current j */; j < icblen(&pf->code); ++j) {
      inscode_t *pic = icbref(&pf->code, j);
      if (pic->in != IN_REGDECL) break;
      chbputf(g_watbuf, "(local $%s %s) ", symname(pic->relkey), valtype_name(pic->arg.u));
    }
    if (chblen(g_watbuf) > 0) wat_line(chbdata(g_watbuf));
    /* code */
    for (/* use current j */; j < icblen(&pf->code); ++j) {
      inscode_t *pic = icbref(&pf->code, j);
      assert(pic->in != IN_REGDECL);
      wat_line(format_inscode(pic, g_watbuf));
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
  wat_linef("(module $%s", symname(pm->name));
  g_watindent += 2;
  wat_imports(&pm->imports);
  wat_funcs(&pm->funcs);
  wat_writeln(")");
  g_watout = NULL;
  chbfini(&cb);
  g_watbuf = NULL;
}
