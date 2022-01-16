/* w.c (wasm writer) -- esl */

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

/* binary encoding */

static chbuf_t *g_curbuf = NULL; /* current destination or NULL */
static chbuf_t *g_sectbuf = NULL; /* current section or NULL */
static chbuf_t *g_ssecbuf = NULL; /* current subsection or NULL */
static chbuf_t *g_codebuf = NULL; /* current code or NULL */
static unsigned g_sectcnt = 0; /* section element count */
void emit_header(void)
{
  fputc(0x00, stdout); fputc(0x61, stdout); fputc(0x73, stdout); fputc(0x6D, stdout);
  fputc(0x01, stdout); fputc(0x00, stdout); fputc(0x00, stdout); fputc(0x00, stdout);
}

void emit_section_start(secid_t si)
{
  assert(g_sectbuf == NULL);
  fputc((unsigned)si & 0xFF, stdout);
  g_sectbuf = newchb(); /* buffer emits */
  g_curbuf = g_sectbuf; /* emits go here */
  g_sectcnt = 0; /* incremented as elements are added */
}

void emit_section_bumpc(void)
{
  assert(g_sectbuf != NULL && g_sectbuf == g_curbuf);
  ++g_sectcnt;
}

void emit_section_end(void)
{
  chbuf_t *psb = g_sectbuf;
  assert(psb != NULL && psb == g_curbuf);
  g_sectbuf = g_curbuf = NULL; /* emit directly from now on */
  if (g_sectcnt > 0 || chblen(psb) == 0) {
    /* element count needs to be inserted */
    chbuf_t cb; chbinit(&cb);
    g_curbuf = &cb; emit_unsigned(g_sectcnt);
    g_curbuf = NULL;
    emit_unsigned(chblen(&cb) + chblen(psb));
    fwrite(chbdata(&cb), 1, chblen(&cb), stdout);
    fwrite(chbdata(psb), 1, chblen(psb), stdout);
    chbfini(&cb); 
  } else {
    /* element count is in psb */
    emit_unsigned(chblen(psb));
    fwrite(chbdata(psb), 1, chblen(psb), stdout);
  }
  freechb(psb);
}

void emit_subsection_start(unsigned ssi)
{
  assert(g_ssecbuf == NULL);
  assert(g_sectbuf != NULL && g_sectbuf == g_curbuf);
  emit_byte(ssi);
  g_ssecbuf = newchb(); /* buffer emits */
  g_curbuf = g_ssecbuf; /* emits go here */
}

void emit_subsection_end(void)
{
  chbuf_t *psb = g_ssecbuf;
  assert(psb != NULL && psb == g_curbuf);
  assert(g_sectbuf != NULL);
  g_ssecbuf = NULL, g_curbuf = g_sectbuf; /* emit to section from now on */
  emit_unsigned(chblen(psb));
  chbput(chbdata(psb), chblen(psb), g_curbuf);
  freechb(psb);
}

void emit_code_start(void)
{
  assert(g_sectbuf != NULL && g_sectbuf == g_curbuf);
  assert(g_codebuf == NULL);
  g_codebuf = newchb(); /* buffer emits */
  g_curbuf = g_codebuf; /* emits go here */
}

void emit_code_end(void)
{
  chbuf_t *psb = g_codebuf;
  assert(psb != NULL && psb == g_curbuf);
  assert(g_sectbuf != NULL);
  g_curbuf = g_sectbuf; /* emit to section from now on */
  g_codebuf = NULL;
  emit_unsigned(chblen(psb));
  chbput(chbdata(psb), chblen(psb), g_curbuf);
  freechb(psb);
}

void emit_byte(unsigned b)
{
  if (g_curbuf) chbputc(b & 0xFF, g_curbuf);
  else fputc(b & 0xFF, stdout);
}

void emit_data(chbuf_t *pcb)
{
  char *s = pcb->buf; size_t n = pcb->fill;
  while (n--) emit_byte(*s++);
}

void emit_signed(long long val)
{
  int more;
  do {
    unsigned b = (unsigned)(val & 0x7f);
    val >>= 7;
    more = b & 0x40 ? val != -1 : val != 0;
    emit_byte(b | (more ? 0x80 : 0));
  } while (more);
}

void emit_unsigned(unsigned long long val)
{
  do {
    unsigned b = (unsigned)(val & 0x7f);
    val >>= 7;
	  if (val != 0) b |= 0x80;
	  emit_byte(b);
  } while (val != 0);
}

void emit_float(float val)
{
  union { float v; unsigned char bb[4]; } u; int i;
  assert(sizeof(float) == 4);
  u.v = val; 
  for (i = 0; i < 4; ++i) emit_byte(u.bb[i]);
}

void emit_double(double val)
{
  union { double v; unsigned char bb[8]; } u; int i;
  assert(sizeof(double) == 8);
  u.v = val; 
  for (i = 0; i < 8; ++i) emit_byte(u.bb[i]);
}

void emit_name(const char *name)
{
  size_t n; assert(name);
  n = strlen(name);
  emit_unsigned(n);
  while (n--) emit_byte(*name++);
}

void emit_in(instr_t in)
{
  emit_byte((unsigned)in);
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

static void emit_expr(icbuf_t *pcb, icbuf_t *prelb)
{
  size_t i;
  for (i = 0; i < icblen(pcb); ++i) {
    inscode_t *pic = icbref(pcb, i);
    if (pic->relkey) relocate_ins(pic, prelb); 
    emit_in(pic->in);
    switch (pic->in) {
      case IN_BLOCK:        case IN_LOOP:         case IN_IF:
      case IN_BR:           case IN_BR_IF:
      case IN_CALL:         case IN_CALL_INDIRECT:
      case IN_RETURN_CALL:  case IN_RETURN_CALL_INDIRECT:
      case IN_LOCAL_GET:    case IN_LOCAL_SET:    case IN_LOCAL_TEE: 
      case IN_GLOBAL_GET:   case IN_GLOBAL_SET:   case IN_TABLE_GET:    case IN_TABLE_SET:
      case IN_I32_CONST:    case IN_I64_CONST:    case IN_REF_NULL:     case IN_REF_FUNC:
        emit_unsigned(pic->arg.u); 
        break;
      case IN_F32_CONST:
        emit_float(pic->arg.f); 
        break;
      case IN_F64_CONST:
        emit_double(pic->arg.d); 
        break;
      case IN_I32_LOAD:     case IN_I64_LOAD:     case IN_F32_LOAD:     case IN_F64_LOAD:
      case IN_I32_LOAD8_S:  case IN_I32_LOAD8_U:  case IN_I32_LOAD16_S: case IN_I32_LOAD16_U:
      case IN_I64_LOAD8_S:  case IN_I64_LOAD8_U:  case IN_I64_LOAD16_S: case IN_I64_LOAD16_U:
      case IN_I64_LOAD32_S: case IN_I64_LOAD32_U: case IN_I32_STORE:    case IN_I64_STORE:
      case IN_F32_STORE:    case IN_F64_STORE:    case IN_I32_STORE8:   case IN_I32_STORE16:
      case IN_I64_STORE8:   case IN_I64_STORE16:  case IN_I64_STORE32:
        emit_unsigned(pic->arg.u); emit_unsigned(pic->align); 
        break;
      case IN_BR_TABLE: /* TODO */
      case 0x06: case 0x07: case 0x08:
      case 0x09: case 0x0A:
      case 0x14: case 0x15: case 0x16:
      case 0x17: case 0x18: case 0x19:
      case 0x1D: case 0x1E: case 0x1F:
      case 0x27:
      case 0xC5: case 0xC6: case 0xC7: case 0xC8:
      case 0xC9: case 0xCA: case 0xCB: case 0xCC:
      case 0xCD: case 0xCE: case 0xCF:
      case IN_EXTENDED_1: case IN_EXTENDED_2:
      case 0xFE: case 0xFF:
        assert(false);     
    }
  }    
}

void emit_types(fsbuf_t *pftb)
{
  size_t i, k;
  if (!pftb) return;
  emit_section_start(SI_TYPE);
  /* no type compression: types may repeat */
  for (i = 0; i < fsblen(pftb); ++i) {
    funcsig_t *pft = fsbref(pftb, i);
    emit_byte(FT_FUNCTYPE);
    emit_unsigned(vtblen(&pft->argtypes));
    for (k = 0; k < vtblen(&pft->argtypes); ++k) {
      emit_byte(*vtbref(&pft->argtypes, k));
    }    
    emit_unsigned(vtblen(&pft->rettypes));
    for (k = 0; k < vtblen(&pft->rettypes); ++k) {
      emit_byte(*vtbref(&pft->rettypes, k));
    }
    emit_section_bumpc();
  }
  emit_section_end();
}

void emit_imports(entbuf_t *pfdb, entbuf_t *ptdb, entbuf_t *pmdb, entbuf_t *pgdb)
{
  size_t i;
  if (!pfdb && !ptdb && !pmdb && !pgdb) return;
  emit_section_start(SI_IMPORT);
  /* all imported functions are at the front of pfdb */
  if (pfdb) for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (!pe->mod) continue; /* skip non-imported */
    assert(pe->name);
    emit_name(symname(pe->mod));
    emit_name(symname(pe->name));
    emit_byte(EK_FUNC);
    emit_unsigned(pe->fsi);
    emit_section_bumpc(); 
  }
  /* imported tables */
  if (ptdb) for (i = 0; i < entblen(ptdb); ++i) {
    entry_t *pe = entbref(ptdb, i);
    assert(pe->ek == EK_TABLE);
    if (!pe->mod) continue; /* skip non-imported */
    assert(pe->name);
    emit_name(symname(pe->mod));
    emit_name(symname(pe->name));
    emit_byte(EK_TABLE);
    emit_byte(pe->vt); /* RT_FUNCREF/RT_EXTERNREF */        
    emit_byte(pe->lt);
    emit_unsigned(pe->n);
    if (pe->lt == LT_MINMAX) emit_unsigned(pe->m);
    emit_section_bumpc(); 
  }    
  /* imported mems */
  if (pmdb) for (i = 0; i < entblen(pmdb); ++i) {
    entry_t *pe = entbref(pmdb, i);
    assert(pe->ek == EK_MEM);
    if (!pe->mod) continue; /* skip non-imported */
    assert(pe->name);
    emit_name(symname(pe->mod));
    emit_name(symname(pe->name));
    emit_byte(EK_MEM);
    emit_byte(pe->lt);
    emit_unsigned(pe->n);
    if (pe->lt == LT_MINMAX) emit_unsigned(pe->m);
    emit_section_bumpc(); 
  }    
  /* imported globals */
  if (pgdb) for (i = 0; i < entblen(pgdb); ++i) {
    entry_t *pe = entbref(pgdb, i);
    assert(pe->ek == EK_GLOBAL);
    if (!pe->mod) continue; /* skip non-imported */
    assert(pe->name);
    emit_name(symname(pe->mod));
    emit_name(symname(pe->name));
    emit_byte(EK_GLOBAL);
    emit_byte(pe->vt);
    emit_byte(pe->mut);
    emit_section_bumpc(); 
  }
  emit_section_end();
}

void emit_funcs(entbuf_t *pfdb)
{
  size_t i;
  if (!pfdb) return;
  emit_section_start(SI_FUNCTION);
  for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (pe->mod) continue; /* skip imported functions */
    emit_unsigned(pe->fsi);
    emit_section_bumpc();
  }
  emit_section_end();
}

void emit_tables(entbuf_t *ptdb)
{
  size_t i;
  if (!ptdb) return;
  emit_section_start(SI_TABLE);
  for (i = 0; i < entblen(ptdb); ++i) {
    entry_t *pe = entbref(ptdb, i);
    assert(pe->ek == EK_TABLE);
    if (pe->mod) continue; /* skip imported tables */
    emit_byte(pe->vt);
    emit_byte(pe->lt);
    emit_unsigned(pe->n);
    if (pe->lt == LT_MINMAX) emit_unsigned(pe->m);
    emit_section_bumpc();
  }
  emit_section_end();
}

void emit_mems(entbuf_t *pmdb)
{
  size_t i;
  if (!pmdb) return;
  emit_section_start(SI_MEMORY);
  for (i = 0; i < entblen(pmdb); ++i) {
    entry_t *pe = entbref(pmdb, i);
    assert(pe->ek == EK_MEM);
    if (pe->mod) continue; /* skip imported mems */
    emit_byte(pe->lt);
    emit_unsigned(pe->n);
    if (pe->lt == LT_MINMAX) emit_unsigned(pe->m);
    emit_section_bumpc();
  }
  emit_section_end();
}

void emit_globals(entbuf_t *pgdb, icbuf_t *prelb)
{
  size_t i;
  if (!pgdb) return;
  emit_section_start(SI_GLOBAL);
  for (i = 0; i < entblen(pgdb); ++i) {
    entry_t *pe = entbref(pgdb, i);
    assert(pe->ek == EK_GLOBAL);
    if (pe->mod) continue; /* skip imported globals */
    emit_byte(pe->vt);
    emit_byte(pe->mut);
    emit_expr(&pe->code, prelb);
    emit_section_bumpc();
  }
  emit_section_end();
}

void emit_exports(entbuf_t *pfdb, entbuf_t *ptdb, entbuf_t *pmdb, entbuf_t *pgdb)
{
  size_t i;
  if (!pfdb && !ptdb && !pmdb && !pgdb) return;
  emit_section_start(SI_EXPORT);
  if (pfdb) for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (!pe->mod && pe->name) {
      emit_name(symname(pe->name));
      emit_byte(EK_FUNC);
      emit_unsigned(i);
      emit_section_bumpc();
    }
  }  
  if (ptdb) for (i = 0; i < entblen(ptdb); ++i) {
    entry_t *pe = entbref(ptdb, i);
    assert(pe->ek == EK_TABLE);
    if (!pe->mod && pe->name) {
      emit_name(symname(pe->name));
      emit_byte(EK_TABLE);
      emit_unsigned(i);
      emit_section_bumpc();
    }
  }  
  if (pmdb) for (i = 0; i < entblen(pmdb); ++i) {
    entry_t *pe = entbref(pmdb, i);
    assert(pe->ek == EK_MEM);
    if (!pe->mod && pe->name) {
      emit_name(symname(pe->name));
      emit_byte(EK_MEM);
      emit_unsigned(i);
      emit_section_bumpc();
    }
  }  
  if (pgdb) for (i = 0; i < entblen(pgdb); ++i) {
    entry_t *pe = entbref(pgdb, i);
    assert(pe->ek == EK_GLOBAL);
    if (!pe->mod && pe->name) {
      emit_name(symname(pe->name));
      emit_byte(EK_GLOBAL);
      emit_unsigned(i);
      emit_section_bumpc();
    }
  }
  emit_section_end();
}

void emit_start(entbuf_t *pfdb)
{
  size_t i;
  if (!pfdb) return;
  for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (pe->isstart) {
      emit_section_start(SI_START);
      emit_unsigned((unsigned)i);
      emit_section_end();
      break;
    }
  }
}

void emit_elems(esegbuf_t *pesb, icbuf_t *prelb)
{
  size_t i, k;
  if (!pesb) return;
  emit_section_start(SI_ELEMENT);
  for (i = 0; i < esegblen(pesb); ++i) {
    eseg_t *ps = esegbref(pesb, i);
    emit_byte(ps->esm);
    switch (ps->esm) {
       case ES_ACTIVE_EIV: /* e fiv */
         emit_expr(&ps->code, prelb);
         emit_unsigned(idxblen(&ps->fidxs));
         goto emitfiv;
       case ES_ACTIVE_TEKIV: /* ti e k fiv */
         emit_unsigned(ps->idx);
         emit_expr(&ps->code, prelb);
         /* fall thru */
       case ES_DECLVE_KIV: /* k fiv */
       case ES_PASSIVE_KIV: /* k fiv */ 
         emit_byte(ps->ek); /* must be 0 */
       emitfiv:
         emit_unsigned(idxblen(&ps->fidxs));
         for (k = 0; k < idxblen(&ps->fidxs); ++k)
           emit_unsigned(*idxbref(&ps->fidxs, k));
         break;       
       case ES_ACTIVE_EEV: /* e ev */
         emit_expr(&ps->code, prelb);
         goto emitev;
       case ES_ACTIVE_IETEV: /* ti e rt ev */       
         emit_unsigned(ps->idx);
         emit_expr(&ps->code, prelb);
         /* fall thru */
       case ES_DECLVE_TEV: /* rt ev */
       case ES_PASSIVE_TEV: /* rt ev */
         emit_byte(ps->rt); /* must be RT_XXX */       
       emitev:
         emit_unsigned(ps->cdc);
         emit_expr(&ps->codes, prelb);
         break;
      default: assert(false);
    }  
    emit_section_bumpc();
  }
  emit_section_end();
}

void emit_datacount(dsegbuf_t *pdsb)
{
  size_t i;
  if (!pdsb) return;
  emit_section_start(SI_DATACOUNT);
  for (i = 0; i < dsegblen(pdsb); ++i) {
    emit_section_bumpc();
  }  
  emit_section_end();
}

void emit_codes(entbuf_t *pfdb, icbuf_t *prelb)
{
  size_t i, k;
  if (!pfdb) return;
  emit_section_start(SI_CODE);
  for (i = 0; i < entblen(pfdb); ++i) {
    entry_t *pe = entbref(pfdb, i);
    assert(pe->ek == EK_FUNC);
    if (pe->mod) continue; /* skip imported functions */
    emit_code_start();
    emit_unsigned(buflen(&pe->loctypes));
    for (k = 0; k < buflen(&pe->loctypes); ++k) {
      /* no type compression: types may repeat */
      emit_unsigned(1);
      emit_byte(*vtbref(&pe->loctypes, k));
    }
    emit_expr(&pe->code, prelb);
    emit_code_end();
    emit_section_bumpc();
  }
  emit_section_end();
}

void emit_datas(dsegbuf_t *pdsb, icbuf_t *prelb)
{
  size_t i;
  if (!pdsb) return;
  emit_section_start(SI_DATA);
  for (i = 0; i < dsegblen(pdsb); ++i) {
    dseg_t *ps = dsegbref(pdsb, i);
    emit_byte(ps->dsm);
    switch (ps->dsm) {
      case DS_ACTIVE_MI: emit_unsigned(ps->idx); /* fall thru */
      case DS_ACTIVE: emit_expr(&ps->code, prelb); /* fall thru */
      case DS_PASSIVE: emit_unsigned(chblen(&ps->data)), emit_data(&ps->data); break;
      default: assert(false);
    }  
    emit_section_bumpc();
  }
  emit_section_end();
}


/* top-level representations */

module_t* modinit(module_t* pm)
{
  memset(pm, 0, sizeof(module_t));
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

void modfini(module_t* pm)
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


/* top-level emits */

void emit_module(module_t* pm)
{
  emit_header();
  emit_types(&pm->funcsigs);
  emit_imports(&pm->funcdefs, &pm->tabdefs, &pm->memdefs, &pm->globdefs);
  emit_funcs(&pm->funcdefs);
  emit_tables(&pm->tabdefs);
  emit_mems(&pm->memdefs);
  emit_globals(&pm->globdefs, &pm->reloctab);
  emit_exports(&pm->funcdefs, &pm->tabdefs, &pm->memdefs, &pm->globdefs);
  emit_start(&pm->funcdefs);
  emit_elems(&pm->elemdefs, &pm->reloctab);
  emit_datacount(&pm->datadefs);
  emit_codes(&pm->funcdefs, &pm->reloctab);
  emit_datas(&pm->datadefs, &pm->reloctab);
}
