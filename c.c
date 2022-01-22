/* c.c (wcpl compiler) -- esl */

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
#include "p.h"
#include "c.h"
#include "w.h"

/* globals */
static buf_t g_bases;
static chbuf_t g_dseg;
static buf_t g_dsmap; /* of dsmelt_t, sorted by cb */
static node_t g_dsty; /* NT_TYPE TS_STRUCT */

/* g_dsmap element */
typedef struct dsmelt_tag {
  chbuf_t cb;
  size_t addr;
  size_t id;
} dsmelt_t;

/* initialization */
void init_compiler(dsbuf_t *plibv)
{
  size_t i;
  bufinit(&g_bases, sizeof(sym_t));
  for (i = 0; i < dsblen(plibv); ++i) {
    dstr_t *pds = dsbref(plibv, i);
    *(sym_t*)bufnewbk(&g_bases) = intern(*pds);
  } 
  chbinit(&g_dseg);
  ndinit(&g_dsty); ndset(&g_dsty, NT_TYPE, -1, -1); g_dsty.ts = TS_STRUCT; 
  bufinit(&g_dsmap, sizeof(dsmelt_t));
  init_workspaces();
  init_symbols();
}

void fini_compiler(void)
{
  dsmelt_t *pde; size_t i;
  buffini(&g_bases);
  chbfini(&g_dseg);
  ndfini(&g_dsty);
  for (pde = (dsmelt_t*)(g_dsmap.buf), i = 0; i < g_dsmap.fill; ++i) chbfini(&pde[i].cb);
  buffini(&g_dsmap);
  fini_workspaces();
  fini_symbols();
}


/* data segment operations */

/* intern the string literal, convert node to &($dp)->$foo */
node_t *convert_strlit_to_dsegref(node_t *pn)
{
  dsmelt_t *pe; node_t *psn; sym_t fs;
  assert(pn->nt == NT_LITERAL && (pn->ts == TS_STRING || pn->ts == TS_LSTRING));
  pe = bufbsearch(&g_dsmap, &pn->data, chbuf_cmp);
  if (!pe) {
    /* add data to g_dseg */
    size_t align = pn->ts == TS_STRING ? 1 : 4;
    size_t addr = chblen(&g_dseg), n = addr % align;
    if (n > 0) bufresize(&g_dseg, (addr = addr+(align-n)));
    chbcat(&g_dseg, &pn->data);
    pe = bufnewbk(&g_dsmap); 
    chbicpy(&pe->cb, &pn->data); pe->addr = addr; 
    pe->id = buflen(&g_dsmap);
    fs = internf("$%s%d", pn->ts == TS_STRING ? "str " : "lstr", (int)pe->id);
    bufqsort(&g_dsmap, chbuf_cmp);
    /* ann new field to g_dsty */
    psn = ndinsbk(&g_dsty, NT_VARDECL); psn->name = fs;
    psn = ndinsbk(psn, NT_TYPE); psn->ts = (pn->ts == TS_STRING) ? TS_CHAR : TS_INT;
    psn = ndinsbk(psn, NT_LITERAL); psn->ts = TS_UINT; psn->val.u = addr; 
  } else {
    fs = internf("$%s%d", pn->ts == TS_STRING ? "str " : "lstr", (int)pe->id);
  }
  ndset(pn, NT_PREFIX, pn->pwsid, pn->startpos); pn->op = TT_AND;
  psn = ndinsbk(pn, NT_POSTFIX); psn->op = TT_ARROW; psn->name = fs;
  psn = ndinsbk(psn, NT_IDENTIFIER); psn->name = intern("$dp");
  return pn;
}


/* type predicates and operations */

static bool ts_numerical(ts_t ts) 
{
  return TS_CHAR <= ts && ts <= TS_DOUBLE;
}

static bool ts_unsigned(ts_t ts) 
{
  switch (ts) {
    case TS_UCHAR: case TS_USHORT: case TS_UINT:
    case TS_ULONG: case TS_ULLONG: 
      return true;
  }
  return false;
}

/* returns TS_INT/TS_UINT/TS_LLONG/TS_ULLONG/TS_FLOAT/TS_DOUBLE */
static ts_t ts_integral_promote(ts_t ts)
{
  switch (ts) {
    case TS_CHAR: case TS_UCHAR: 
    case TS_SHORT: case TS_USHORT:
    case TS_INT:                    return TS_INT;
    case TS_UINT:                   return TS_UINT;
    /* fixme: should depend on wasm32/wasm64 model */
    case TS_LONG:                   return TS_INT;
    case TS_ULONG:                  return TS_UINT;
    case TS_LLONG: case TS_ULLONG:  
    case TS_FLOAT: case TS_DOUBLE:  return ts;
    default: assert(false);
  }
  return ts; /* shouldn't happen */
} 

/* common arith type for ts1 and ts2 (can be promoted one) */
static ts_t ts_arith_common(ts_t ts1, ts_t ts2)
{
  assert(ts_numerical(ts1) && ts_numerical(ts2));
  if (ts1 == TS_DOUBLE || ts2 == TS_DOUBLE) return TS_DOUBLE;
  if (ts1 == TS_FLOAT || ts2 == TS_FLOAT) return TS_FLOAT;
  ts1 = ts_integral_promote(ts1), ts2 = ts_integral_promote(ts2);
  if (ts1 == ts2) return ts1;
  if (ts1 == TS_ULLONG || ts2 == TS_ULLONG) return TS_ULLONG;
  if (ts1 == TS_LLONG || ts2 == TS_LLONG) return TS_LLONG;
  if (ts1 == TS_UINT || ts2 == TS_UINT) return TS_UINT;
  return TS_INT;
}

unsigned long long integral_mask(ts_t ts, unsigned long long u)
{
  switch (ts) {
    case TS_CHAR: case TS_UCHAR:    
      return u & 0x00000000000000FFull; 
    case TS_SHORT: case TS_USHORT:  
      return u & 0x000000000000FFFFull; 
    case TS_INT: case TS_UINT:                   
      return u & 0x00000000FFFFFFFFull; 
    /* fixme: should depend on wasm32/wasm64 model */
    case TS_LONG: case TS_ULONG:                  
      return u & 0x00000000FFFFFFFFull; 
    case TS_LLONG: case TS_ULLONG:  
      return u;
    default: assert(false);
  }
  return u;
}

long long integral_sext(ts_t ts, long long i)
{
  switch (ts) {
    case TS_CHAR: case TS_UCHAR:
      return (long long)((((unsigned long long)i & 0x00000000000000FFull) 
                        ^ 0x0000000000000080ull) - 0x0000000000000080ull); 
    case TS_SHORT: case TS_USHORT:
      return (long long)((((unsigned long long)i & 0x000000000000FFFFull) 
                        ^ 0x0000000000008000ull) - 0x0000000000008000ull); 
    case TS_INT: case TS_UINT:                   
      return (long long)((((unsigned long long)i & 0x00000000FFFFFFFFull) 
                        ^ 0x0000000080000000ull) - 0x0000000080000000ull); 
    /* fixme: should depend on wasm32/wasm64 model */
    case TS_LONG: case TS_ULONG:                  
      return (long long)((((unsigned long long)i & 0x00000000FFFFFFFFull) 
                        ^ 0x0000000080000000ull) - 0x0000000080000000ull); 
    case TS_LLONG: case TS_ULLONG:  
      return i;
    default: assert(false);
  }
  return i;
}

void numval_convert(ts_t tsto, ts_t tsfrom, numval_t *pnv)
{
  assert(pnv);
  assert(ts_numerical(tsto) && ts_numerical(tsfrom));
  if (tsto == tsfrom) return; /* no change */
  else if (tsto == TS_DOUBLE) {
    if (tsfrom == TS_FLOAT) pnv->d = (double)pnv->f;
    else if (ts_unsigned(tsfrom)) pnv->d = (double)pnv->u;
    else pnv->d = (double)pnv->i;
  } else if (tsto == TS_FLOAT) {
    if (tsfrom == TS_DOUBLE) pnv->f = (float)pnv->d;
    else if (ts_unsigned(tsfrom)) pnv->f = (float)pnv->u;
    else pnv->f = (float)pnv->i;
  } else if (ts_unsigned(tsto)) {
    if (tsfrom == TS_DOUBLE) pnv->u = (unsigned long long)pnv->d;
    else if (tsfrom == TS_FLOAT) pnv->u = (unsigned long long)pnv->f;
    else pnv->u = integral_mask(tsto, pnv->u); /* i reinterpreted as u */
  } else { /* signed integer */
    if (tsfrom == TS_DOUBLE) pnv->i = (long long)pnv->d;
    else if (tsfrom == TS_FLOAT) pnv->i = (long long)pnv->f;
    else pnv->i = integral_sext(tsto, pnv->i); /* u reinterpreted as i */
  }
}

/* unary operation with a number */
ts_t numval_unop(tt_t op, ts_t tx, numval_t vx, numval_t *pvz)
{
  ts_t tz = ts_integral_promote(tx);
  numval_convert(tz, tx, &vx);
  switch (tz) {
    case TS_DOUBLE: {
      double x = vx.d;
      switch (op) {
        case TT_PLUS:  pvz->d = x; break;
        case TT_MINUS: pvz->d = -x; break;
        case TT_NOT:   pvz->i = x == 0; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    case TS_FLOAT: {
      float x = vx.f;
      switch (op) {
        case TT_PLUS:  pvz->f = x; break;
        case TT_MINUS: pvz->f = -x; break;
        case TT_NOT:   pvz->i = x == 0; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    case TS_ULLONG: case TS_UINT: {
      unsigned long long x = vx.u;
      switch (op) {
        case TT_PLUS:  pvz->u = integral_mask(tz, x); break;
        case TT_MINUS: pvz->u = integral_mask(tz, (unsigned long long)-(long long)x); break;
        case TT_TILDE: pvz->u = integral_mask(tz, ~x); break;
        case TT_NOT:   pvz->i = x == 0; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    case TS_LLONG: case TS_INT: {
      long long x = vx.i;
      switch (op) {
        case TT_PLUS:  pvz->i = integral_sext(tz, x); break;
        case TT_MINUS: pvz->i = integral_sext(tz, -x); break;
        case TT_TILDE: pvz->i = integral_sext(tz, ~x); break;
        case TT_NOT:   pvz->i = x == 0; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    default: {
      assert(false);
      tz = TS_VOID; break;
    } break;
  }
  return tz;
}  

/* binary operation between two numbers */
ts_t numval_binop(tt_t op, ts_t tx, numval_t vx, ts_t ty, numval_t vy, numval_t *pvz)
{
  ts_t tz = ts_arith_common(tx, ty);
  numval_convert(tz, tx, &vx), numval_convert(tz, ty, &vy);
  switch (tz) {
    case TS_DOUBLE: {
      double x = vx.d, y = vy.d;
      switch (op) {
        case TT_PLUS:  pvz->d = x + y; break;
        case TT_MINUS: pvz->d = x - y; break;
        case TT_STAR:  pvz->d = x * y; break;
        case TT_SLASH: if (y) pvz->d = x / y; else tz = TS_VOID; break;
        case TT_EQ:    pvz->i = x == y; tz = TS_INT; break;
        case TT_NE:    pvz->i = x != y; tz = TS_INT; break;
        case TT_LT:    pvz->i = x < y;  tz = TS_INT; break;
        case TT_GT:    pvz->i = x > y;  tz = TS_INT; break;
        case TT_LE:    pvz->i = x <= y; tz = TS_INT; break;
        case TT_GE:    pvz->i = x >= y; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    case TS_FLOAT: {
      float x = vx.f, y = vy.f;
      switch (op) {
        case TT_PLUS:  pvz->f = x + y; break;
        case TT_MINUS: pvz->f = x - y; break;
        case TT_STAR:  pvz->f = x * y; break;
        case TT_SLASH: if (y) pvz->f = x / y; else tz = TS_VOID; break;
        case TT_EQ:    pvz->i = x == y; tz = TS_INT; break;
        case TT_NE:    pvz->i = x != y; tz = TS_INT; break;
        case TT_LT:    pvz->i = x < y;  tz = TS_INT; break;
        case TT_GT:    pvz->i = x > y;  tz = TS_INT; break;
        case TT_LE:    pvz->i = x <= y; tz = TS_INT; break;
        case TT_GE:    pvz->i = x >= y; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    case TS_ULLONG: case TS_UINT: {
      unsigned long long x = vx.u, y = vy.u;
      switch (op) {
        case TT_PLUS:  pvz->u = integral_mask(tz, x + y); break;
        case TT_MINUS: pvz->u = integral_mask(tz, x - y); break;
        case TT_STAR:  pvz->u = integral_mask(tz, x * y); break;
        case TT_AND:   pvz->u = integral_mask(tz, x & y); break;
        case TT_OR:    pvz->u = integral_mask(tz, x | y); break;
        case TT_XOR:   pvz->u = integral_mask(tz, x ^ y); break;
        case TT_SLASH: if (y) pvz->u = integral_mask(tz, x / y); else tz = TS_VOID; break;
        case TT_REM:   if (y) pvz->u = integral_mask(tz, x % y); else tz = TS_VOID; break;
        case TT_SHL:   if (y >= 0 && y < 64) pvz->u = integral_mask(tz, x << y); break;
        case TT_SHR:   if (y >= 0 && y < 64) pvz->u = integral_mask(tz, x >> y); break;
        case TT_EQ:    pvz->i = x == y; tz = TS_INT; break;
        case TT_NE:    pvz->i = x != y; tz = TS_INT; break;
        case TT_LT:    pvz->i = x < y;  tz = TS_INT; break;
        case TT_GT:    pvz->i = x > y;  tz = TS_INT; break;
        case TT_LE:    pvz->i = x <= y; tz = TS_INT; break;
        case TT_GE:    pvz->i = x >= y; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    case TS_LLONG: case TS_INT: {
      long long x = vx.i, y = vy.i;
      switch (op) {
        case TT_PLUS:  pvz->i = integral_sext(tz, x + y); break;
        case TT_MINUS: pvz->i = integral_sext(tz, x - y); break;
        case TT_STAR:  pvz->i = integral_sext(tz, x * y); break;
        case TT_AND:   pvz->i = integral_sext(tz, x & y); break;
        case TT_OR:    pvz->i = integral_sext(tz, x | y); break;
        case TT_XOR:   pvz->i = integral_sext(tz, x ^ y); break;
        case TT_SLASH: if (y) pvz->i = integral_sext(tz, x / y); else tz = TS_VOID; break;
        case TT_REM:   if (y) pvz->i = integral_sext(tz, x % y); else tz = TS_VOID; break;
        case TT_SHL:   if (y >= 0 && y < 64) pvz->i = integral_sext(tz, x << y); break;
        case TT_SHR:   if (y >= 0 && y < 64) pvz->i = integral_sext(tz, x >> y); break;
        case TT_EQ:    pvz->i = x == y; tz = TS_INT; break;
        case TT_NE:    pvz->i = x != y; tz = TS_INT; break;
        case TT_LT:    pvz->i = x < y;  tz = TS_INT; break;
        case TT_GT:    pvz->i = x > y;  tz = TS_INT; break;
        case TT_LE:    pvz->i = x <= y; tz = TS_INT; break;
        case TT_GE:    pvz->i = x >= y; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    default: {
      assert(false);
      tz = TS_VOID; break;
    } break;
  }
  return tz;
}  

/* calc size/align for ptn; prn is NULL or reference node for errors */
void measure_type(node_t *ptn, node_t *prn, size_t *psize, size_t *palign, int lvl)
{
  assert(ptn && ptn->nt == NT_TYPE);
  if (lvl > 100) n2eprintf(ptn, prn, "type too large (possible infinite nesting)");
  *psize = *palign = 0;
  switch (ptn->ts) {
    case TS_VOID: 
      n2eprintf(ptn, prn, "can't allocate void type");
      break;
    case TS_ETC: 
      n2eprintf(ptn, prn, "can't allocate data for ... yet");
      break;
    case TS_CHAR: case TS_UCHAR: 
      *psize = *palign = 1; 
      break;
    case TS_SHORT: case TS_USHORT: 
      *psize = *palign = 2; 
      break;
    case TS_INT: case TS_UINT: 
    case TS_ENUM: 
    case TS_FLOAT:
      *psize = *palign = 4; 
      break;
    case TS_LONG: case TS_ULONG:
    case TS_STRING: case TS_LSTRING:
    case TS_PTR:
      /* wasm32; should be 8 for wasm64 */
      *psize = *palign = 4;
      break;
    case TS_LLONG: case TS_ULLONG:
    case TS_DOUBLE:
      *psize = *palign = 8; 
      break;
    case TS_UNION: {
      size_t size, align, i, n;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(TS_UNION, ptn->name);
        if (!pftn) n2eprintf(ptn, prn, "can't allocate incomplete union type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        node_t *pni = ndref(ptn, i);
        if (pni->nt == NT_VARDECL && ndlen(pni) == 1) pni = ndref(pni, 0);
        measure_type(pni, prn, &size, &align, lvl+1);
        if (size > *psize) *psize = size;         
        if (align > *palign) *palign = align;         
      }
      if (!*psize || !*palign) 
        n2eprintf(ptn, prn, "can't allocate empty or incomplete union");
      /* round size up so it is multiple of align */
      if ((n = *psize % *palign) > 0) *psize += *palign - n; 
    } break;
    case TS_STRUCT: {
      size_t size, align, i, n;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(TS_STRUCT, ptn->name);
        if (!pftn) n2eprintf(ptn, prn, "can't allocate incomplete struct type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        node_t *pni = ndref(ptn, i);
        if (pni->nt == NT_VARDECL && ndlen(pni) == 1) pni = ndref(pni, 0);
        measure_type(pni, prn, &size, &align, lvl+1);
        /* round size up so it is multiple of pni align */
        if ((n = *psize % align) > 0) *psize += align - n; 
        *psize += size; if (align > *palign) *palign = align;         
      }
      if (!*psize || !*palign) 
        n2eprintf(ptn, prn, "can't allocate empty or incomplete struct");
      /* round size up so it is multiple of align */
      if ((n = *psize % *palign) > 0) *psize += *palign - n; 
    } break;
    case TS_ARRAY: {
      if (ndlen(ptn) == 2) {
        node_t *petn = ndref(ptn, 0), *pcn = ndref(ptn, 1); 
        int count = 0;
        if (!static_eval_to_int(pcn, &count) || count <= 0)
          n2eprintf(pcn, prn, "can't allocate data for arrays of nonconstant/nonpositive size");
        measure_type(petn, prn, psize, palign, lvl+1);
        *psize *= count;
      } else {
        n2eprintf(ptn, prn, "can't allocate data for arrays of unspecified size");
      }
    } break;
    case TS_FUNCTION:
      n2eprintf(ptn, prn, "can't allocate data for function");
      break;
    default:
      assert(false);
  }
  if (!*psize || !*palign) 
    n2eprintf(ptn, prn, "can't allocate type");
}

/* calc offset for ptn.fld; prn is NULL or reference node for errors */
size_t measure_offset(node_t *ptn, node_t *prn, sym_t fld)
{
  assert(ptn && ptn->nt == NT_TYPE);
  switch (ptn->ts) {
    case TS_UNION: {
      size_t i;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(TS_UNION, ptn->name);
        if (!pftn) n2eprintf(ptn, prn, "can't allocate incomplete union type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        node_t *pni = ndref(ptn, i);
        if (pni->nt == NT_VARDECL && pni->name == fld) return 0;
      }
      n2eprintf(ptn, prn, "can't find field %s in union", symname(fld));
    } break;
    case TS_STRUCT: {
      size_t size, align, i, n, cursize = 0;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(TS_STRUCT, ptn->name);
        if (!pftn) n2eprintf(ptn, prn, "can't allocate incomplete struct type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        node_t *pni = ndref(ptn, i); sym_t s = 0;
        if (pni->nt == NT_VARDECL && ndlen(pni) == 1) s = pni->name, pni = ndref(pni, 0);
        measure_type(pni, prn, &size, &align, 0);
        /* round size up so it is multiple of pni align */
        if ((n = cursize % align) > 0) cursize += align - n;
        if (s == fld) return cursize;
        cursize += size;
      }
      n2eprintf(ptn, prn, "can't find field %s in struct", symname(fld));
    } break;
  }
  n2eprintf(ptn, prn, "can't find field %s in type", symname(fld));
  return 0; /* won't happen */
}

/* evaluate integer expression pn statically, putting result into pi */
bool static_eval_to_int(node_t *pn, int *pri)
{
  node_t nd = mknd(); bool ok = false;
  if (static_eval(pn, &nd) && nd.nt == NT_LITERAL && ts_numerical(nd.ts)) {
    numval_t v = nd.val; numval_convert(TS_INT, nd.ts, &v);
    *pri = (int)v.i, ok = true;
  }
  ndfini(&nd);
  return ok;
}

#ifdef _DEBUG
bool static_eval(node_t *pn, node_t *prn)
{
  bool ok; bool __static_eval(node_t *pn, node_t *prn);
  ok = __static_eval(pn, prn);
  fprintf(stderr, "** eval\n"); dump_node(pn, stderr);
  if (ok) { fprintf(stderr, "=>\n"); dump_node(prn, stderr); }
  else fprintf(stderr, "=> failed!\n");
  return ok;
}
#define static_eval(pn, prn) __static_eval(pn, prn)
#endif

/* evaluate pn expression statically, putting result into prn (numbers only) */
bool static_eval(node_t *pn, node_t *prn)
{
  switch (pn->nt) {
    case NT_LITERAL: {
      if (ts_numerical(pn->ts)) {
        ndcpy(prn, pn);
        return true;
      }
    } break;
    case NT_INTRCALL: {
      if (pn->intr == INTR_SIZEOF || pn->intr == INTR_ALIGNOF) {
        size_t size, align; assert(ndlen(pn) == 1);
        measure_type(ndref(pn, 0), pn, &size, &align, 0);
        ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); prn->ts = TS_INT; 
        if (pn->name == intern("sizeof")) prn->val.i = (int)size;
        else prn->val.i = (int)align;
        return true;
      } else if (pn->intr == INTR_OFFSETOF) {
        size_t offset; assert(ndlen(pn) == 2 && ndref(pn, 1)->nt == NT_IDENTIFIER);
        offset = measure_offset(ndref(pn, 0), pn, ndref(pn, 1)->name);
        ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); prn->ts = TS_INT; 
        prn->val.i = (int)offset;
        return true;
      }
    } break;
    case NT_PREFIX: {
      node_t nx = mknd(); bool ok = false; 
      assert(ndlen(pn) == 1); 
      if ((static_eval)(ndref(pn, 0), &nx)) {
        assert(nx.nt == NT_LITERAL);
        if (ts_numerical(nx.ts)) {
          numval_t vz; ts_t tz = numval_unop(pn->op, nx.ts, nx.val, &vz);
          if (tz != TS_VOID) {
            ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
            prn->ts = tz; prn->val = vz; ok = true;
          }
        }
      }
      ndfini(&nx);
      return ok;
    } break;
    case NT_INFIX: {
      node_t nx = mknd(), ny = mknd(); bool ok = false;
      assert(ndlen(pn) == 2); 
      if ((static_eval)(ndref(pn, 0), &nx) && (static_eval)(ndref(pn, 1), &ny)) {
        assert(nx.nt == NT_LITERAL && ny.nt == NT_LITERAL);
        if (ts_numerical(nx.ts) && ts_numerical(ny.ts)) {
          numval_t vz; ts_t tz = numval_binop(pn->op, nx.ts, nx.val, ny.ts, ny.val, &vz);
          if (tz != TS_VOID) {
            ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
            prn->ts = tz; prn->val = vz; ok = true;
          }
        }
      }
      ndfini(&nx), ndfini(&ny);
      return ok;
    } break;
    case NT_COND: {
      node_t nr = mknd(); bool ok = false; int cond;
      assert(ndlen(pn) == 3);
      if (static_eval_to_int(ndref(pn, 0), &cond)) {
        if (cond) ok = (static_eval)(ndref(pn, 1), &nr);
        else ok = (static_eval)(ndref(pn, 2), &nr);
        if (ok && !ts_numerical(nr.ts)) ok = false;
        if (ok) {
          ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
          prn->ts = nr.ts; prn->val = nr.val;
        }
      } 
      ndfini(&nr);
      return ok;
    } break;
    case NT_CAST: {
      node_t nx = mknd(); ts_t tc; bool ok = false;
      assert(ndlen(pn) == 2); assert(ndref(pn, 0)->nt == NT_TYPE);
      tc = ndref(pn, 0)->ts; 
      if (ts_numerical(tc) && (static_eval)(ndref(pn, 1), &nx) && ts_numerical(nx.ts)) {
        numval_t vc = nx.val; numval_convert(tc, nx.ts, &vc);
        ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
        prn->ts = tc; prn->val = vc; ok = true;
      }
      ndfini(&nx);
      return ok;
    } break;
  }
  return false;
}

/* check if node can be evaluated statically (numbers only) */
static bool static_constant(node_t *pn)
{
  switch (pn->nt) {
    case NT_LITERAL: 
      return ts_numerical(pn->ts);
    case NT_INTRCALL:
      switch (pn->intr) {
        case INTR_SIZEOF: case INTR_ALIGNOF: case INTR_OFFSETOF:
          return true;
      }
      return false;
    case NT_PREFIX:
      assert(ndlen(pn) == 1); 
      switch (pn->op) {
        case TT_PLUS:  case TT_MINUS: case TT_TILDE: case TT_NOT:
          return static_constant(ndref(pn, 0));
      }
      return false;
    case NT_INFIX: 
      assert(ndlen(pn) == 2); 
      switch (pn->op) {
        case TT_PLUS:  case TT_MINUS: case TT_STAR:  case TT_AND:
        case TT_OR:    case TT_XOR:   case TT_SLASH: case TT_REM:
        case TT_SHL:   case TT_SHR:   case TT_EQ:    case TT_NE:
        case TT_LT:    case TT_GT:    case TT_LE:    case TT_GE:
          return static_constant(ndref(pn, 0)) && static_constant(ndref(pn, 1));
      } 
      return false;
    case NT_COND:
      assert(ndlen(pn) == 3); 
      return static_constant(ndref(pn, 0)) && 
             static_constant(ndref(pn, 1)) && static_constant(ndref(pn, 2));
    case NT_CAST: 
      assert(ndlen(pn) == 2); assert(ndref(pn, 0)->nt == NT_TYPE);
      return ts_numerical(ndref(pn, 0)->ts) && static_constant(ndref(pn, 1));
  }
  return false;
}


/* post variable declaration for later use */
static void post_vardecl(sym_t mod, node_t *pn)
{
  const node_t *pin;
  assert(pn->nt == NT_VARDECL && pn->name && ndlen(pn) == 1);
  assert(ndref(pn, 0)->nt == NT_TYPE);
  /* post in symbol table under pn->name as NT_IMPORT with modname and type;
     set its sc to NONE to be changed to EXTERN when actually referenced from module */
  pin = post_symbol(mod, pn);
#ifdef _DEBUG
  fprintf(stderr, "%s =>\n", symname(pn->name));
  dump_node(pin, stderr);
#endif
}

/* check if this expression will produce valid const code */
static void check_constnd(sym_t mainmod, node_t *pn, node_t *pvn)
{
  /* valid ins: t.const ref.null ref.func global.get referring to imported const */
  switch (pn->nt) {
    case NT_LITERAL: /* compiles to t.const ref.null */
      return; 
    case NT_IDENTIFIER: { /* may compile to valid global.get */
      const node_t *pin = lookup_global(pn->name);
      /* all our imported globals are const, so we need to check module only */ 
      if (pin && pin->name != mainmod) return; 
    } break;  
  }
  n2eprintf(pn, pvn, "non-constant initializer expression");
}

/* process var declaration in main module */
static void process_vardecl(sym_t mainmod, node_t *pdn, node_t *pin, module_t *pm)
{
#ifdef _DEBUG
  dump_node(pdn, stderr);
  if (pin) dump_node(pin, stderr);
#endif
  /* if initializer is present, check it for constness */  
  if (pin) { 
    assert(pin->nt == NT_ASSIGN && ndlen(pin) == 2);
    assert(ndref(pin, 0)->nt == NT_IDENTIFIER && ndref(pin, 0)->name == pdn->name);
    /* todo: try to const-fold it first! */
    check_constnd(mainmod, ndref(pin, 1), pin);
  }
  /* post it for later use */
  post_vardecl(mainmod, pdn);
  /* todo: compile to global */
}

/* check if this function type is legal in wasm */
static void fundef_check_type(node_t *ptn)
{
  size_t i;
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  for (i = 0; i < ndlen(ptn); ++i) {
    node_t *ptni = ndref(ptn, i); 
    assert(ptni->nt == NT_VARDECL && ndlen(ptni) == 1 || ptni->nt == NT_TYPE);
    if (ptni->nt == NT_VARDECL) ptni = ndref(ptni, 0); assert(ptni->nt == NT_TYPE);
    switch (ptni->ts) {
      case TS_VOID:
        if (i == 0) break; /* return type can be void */
        neprintf(ptn, "unexpected void function argument type");
      case TS_CHAR: case TS_UCHAR:   /* ok but widened to int */
      case TS_SHORT: case TS_USHORT: /* ok but widened to int */
      case TS_INT: case TS_UINT: case TS_ENUM:
      case TS_LONG: case TS_ULONG:  
      case TS_LLONG: case TS_ULLONG:  
      case TS_FLOAT: case TS_DOUBLE:
      case TS_PTR: 
        break;
      case TS_ARRAY: /* passed as pointer */
        if (i != 0) break; 
        neprintf(ptn, "unexpected array function return type");
      case TS_STRUCT: case TS_UNION: /* passed as pointer */
        break;
      default:
        neprintf(ptn, "function arguments of this type are not supported");
    }
  }
}

/* hoist_locals environment data type */ 
typedef struct henv_tag {
  buf_t idmap; /* pairs of symbols <old, new> */
  struct henv_tag *up; 
} henv_t;

/* return new name given oname; 0 if not found */
static sym_t henv_lookup(sym_t oname, henv_t *phe, int *pupc)
{
  if (pupc) *pupc = 0;
  while (phe) {
    sym_t *pon = bufsearch(&phe->idmap, &oname, sym_cmp);
    if (pon) return pon[1];
    phe = phe->up; if (pupc) *pupc += 1;
  }
  return 0;
}

/* hoist definitions of local vars to plb renaming them if needed;
 * also, mark local vars which addresses are taken as 'auto' */
static void expr_hoist_locals(node_t *pn, henv_t *phe, ndbuf_t *plb)
{
  switch (pn->nt) {
    case NT_IDENTIFIER: {
      sym_t nname = henv_lookup(pn->name, phe, NULL);
      if (nname) pn->name = nname; /* else global */
    } break;
    case NT_BLOCK: {
      size_t i;
      henv_t he; he.up = phe; bufinit(&he.idmap, 2*sizeof(sym_t));
      for (i = 0; i < ndlen(pn); ++i) {
        node_t *pni = ndref(pn, i);
        if (pni->nt == NT_VARDECL) {
          sym_t nname = pni->name; int upc;
          /* see if in needs to be renamed */
          if (henv_lookup(pni->name, &he, &upc)) {
            if (upc) { /* normal shadowing */
              sym_t *pon = bufnewbk(&he.idmap);
              nname = internf("%s#%d", symname(pni->name), (int)buflen(plb));
              pon[0] = pni->name, pon[1] = nname; 
            } else {
              n2eprintf(pni, pn, "duplicate variable declaration in the same scope"); 
            }
          } else {
            sym_t *pon = bufnewbk(&he.idmap);
            pon[0] = pon[1] = nname; 
          }
          /* hoist vardecl to plb */
          pni->name = nname;
          ndswap(pni, ndbnewbk(plb));
          ndrem(pn, i); --i; 
        } else {
          expr_hoist_locals(pni, &he, plb);
        }
      }
      buffini(&he.idmap);    
    } break;
    case NT_VARDECL: {
      neprintf(pn, "unexpected variable declaration");
    } break;
    case NT_PREFIX: {
      assert(ndlen(pn) == 1);
      expr_hoist_locals(ndref(pn, 0), phe, plb);
      if (pn->op == TT_AND && ndref(pn, 0)->nt == NT_IDENTIFIER) {
        /* address-of var is taken: mark it as auto */
        /* fixme? parameters and globals are not marked */
        sym_t name = ndref(pn, 0)->name; size_t i;
        for (i = 0; i < ndblen(plb); ++i) {
          node_t *pndi = ndbref(plb, i); 
          assert(pndi->nt == NT_VARDECL);
          if (pndi->name != name) continue;
          if (pndi->sc == SC_REGISTER) 
            n2eprintf(pn, pndi, "cannot take address of register var");
          else pndi->sc = SC_AUTO;
        }
      } 
    } break;
    case NT_TYPE: {
      /* no vars here */
    } break;
    default: {
      size_t i;
      for (i = 0; i < ndlen(pn); ++i) {
        expr_hoist_locals(ndref(pn, i), phe, plb);
      }
    } break;
  }
}

/* hoist all locals to the start of the top block, mark &vars as auto */
static void fundef_hoist_locals(node_t *pdn)
{
  size_t i;
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1);
  ndbuf_t locals; henv_t he;
  ndbinit(&locals); he.up = NULL; bufinit(&he.idmap, 2*sizeof(sym_t));
  /* init env with parameter names mapped to themselves */
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  for (i = 0; i < ndlen(ptn); ++i) {
    node_t *ptni = ndref(ptn, i); 
    sym_t name = (ptni->nt == NT_VARDECL) ? ptni->name : 0;
    if (name) {
      sym_t *pon = bufsearch(&he.idmap, &name, sym_cmp);
      if (pon) n2eprintf(ptni, pdn, "duplicate parameter id: %s", symname(name));
      else pon = bufnewbk(&he.idmap), pon[0] = pon[1] = name; 
    }
  }
  /* go over body looking for blocks w/vardecls and var refs */
  assert(pbn->nt == NT_BLOCK);
  expr_hoist_locals(pbn, &he, &locals);
  /* now re-insert decls at the beginning */
  for (i = 0; i < ndblen(&locals); ++i) {
    ndswap(ndinsnew(pbn, i), ndbref(&locals, i));
  }
  ndbfini(&locals); buffini(&he.idmap);
}

/* local variable info for wasmify */
typedef struct vi_tag {
  sym_t name;  /* var name in source code (possibly after renaming) */
  sc_t sc;     /* SC_REGISTER or SC_AUTO only */
  sym_t reg;   /* REGISTER: actual register name; AUTO: pointer register name */
  sym_t fld;   /* AUTO: 0 or field name (if reg is struct/union pointer) */
  ts_t cast;   /* TS_VOID or TS_CHAR..TS_USHORT for widened vars */
} vi_t;

/* add temporary register ($b $ub $s $us $i $u $l $ul $ll $ull $f $d $p) */
static sym_t add_temp_reg(ts_t ts, node_t *pbn, buf_t *pvib)
{
  sym_t name = 0; const char *s = NULL;
  ts_t cast = TS_VOID;
  switch (ts) {
    case TS_CHAR:   s = "$b";   cast = ts; ts = TS_INT; break;
    case TS_UCHAR:  s = "$ub";  cast = ts; ts = TS_INT; break;
    case TS_SHORT:  s = "$s";   cast = ts; ts = TS_INT; break;
    case TS_USHORT: s = "$us";  cast = ts; ts = TS_INT; break;
    case TS_INT:    s = "$i";   break;
    case TS_UINT:   s = "$u";   break;
    case TS_LONG:   s = "$l";   break;
    case TS_ULONG:  s = "$ul";  break;
    case TS_LLONG:  s = "$ll";  break;
    case TS_ULLONG: s = "$ull"; break;
    case TS_FLOAT:  s = "$i";   break;
    case TS_DOUBLE: s = "$i";   break;
    case TS_PTR:    s = "$p";   break;
    default: assert(false);
  }
  name = intern(s);
  if (bufsearch(pvib, &name, sym_cmp) == NULL) {
    node_t *pin = ndinsnew(pbn, 0), *ptn;
    vi_t *pvi = bufnewbk(pvib); 
    pvi->name = name, pvi->sc = SC_REGISTER;
    pvi->reg = name, pvi->cast = cast;
    ndset(pin, NT_VARDECL, pbn->pwsid, pbn->startpos);
    pin->name = name; ptn = ndinsbk(pin, NT_TYPE); 
    if (ts == TS_PTR) ptn->ts = TS_VOID, wrap_type_pointer(ptn);
    else ptn->ts = ts;
    /* resort to keep using bsearch */
    bufqsort(pvib, sym_cmp);
  }
  return name;
}

/* convert to wasm conventions, simplify (get rid of . and []) */
static void expr_wasmify(node_t *pn, buf_t *pvib)
{
  switch (pn->nt) {
    case NT_IDENTIFIER: {
      vi_t *pvi = bufbsearch(pvib, &pn->name, sym_cmp);
      if (pvi) {
        if (pvi->sc == SC_AUTO && !pvi->fld) { /* indirect */
          pn->name = pvi->reg;
          wrap_unary_operator(pn, pn->startpos, TT_STAR);
        } else if (pvi->sc == SC_AUTO) { /* ->fld */
          pn->name = pvi->reg;
          wrap_postfix_operator(pn, TT_ARROW, pvi->fld);
        } else { /* in register, just fix name */
          pn->name = pvi->reg;
        }
        if (pvi->cast != TS_VOID) { /* add narrowing cast */
          node_t cn = mknd(); ndset(&cn, NT_TYPE, -1, -1); cn.ts = pvi->cast;
          ndswap(&cn, pn); wrap_cast(pn, &cn); ndfini(&cn);
        }
      } /* else global */
    } break;
    case NT_TYPE: {
      /* nothing of interest in here */
    } break;
    case NT_SUBSCRIPT: { /* x[y] => *(x + y) */
      assert(ndlen(pn) == 2);
      expr_wasmify(ndref(pn, 0), pvib);
      expr_wasmify(ndref(pn, 1), pvib);
      pn->nt = NT_INFIX, pn->op = TT_PLUS;
      wrap_unary_operator(pn, pn->startpos, TT_STAR);
    } break;
    case NT_POSTFIX: {
      assert(ndlen(pn) == 1);
      expr_wasmify(ndref(pn, 0), pvib);
      if (pn->op == TT_DOT) { /* (*x).y => x->y; x.y => (&x)->y */
        node_t *psn = ndref(pn, 0);
        if (psn->nt == NT_PREFIX && psn->op == TT_STAR) flatten_lift_arg0(psn);
        else wrap_unary_operator(psn, psn->startpos, TT_AND);
        pn->op = TT_ARROW;
      } 
    } break;
    /* todo: split combo assignments using tmp registers */
    default: {
      size_t i;
      for (i = 0; i < ndlen(pn); ++i) {
        expr_wasmify(ndref(pn, i), pvib);
      }
    } break;
  }
}

/* convert to wasm conventions, simplify */
static void fundef_wasmify(node_t *pdn)
{
  /* special registers (scalar/poiner vars with internal names): 
   * $rp (result pointer), $fp (frame pointer), %ap (... data pointer)?
   * $b $ub $s $us $i $u $l $ul $ll $ull $f $d $p (temporaries) */
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1);
  buf_t vib = mkbuf(sizeof(vi_t)); node_t frame = mknd();
  size_t i; 
  
  /* init frame structure, in case there are auto locals*/
  ndset(&frame, NT_TYPE, pdn->pwsid, pdn->startpos);
  frame.ts = TS_STRUCT;
  
  /* wasmify function signature and collect arg vars */
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  for (i = 0; i < ndlen(ptn); ++i) {
    node_t *pdni = ndref(ptn, i), *ptni = pdni; sym_t name = 0; ts_t cast = TS_VOID; 
    if (pdni->nt == NT_VARDECL) name = ptni->name, ptni = ndref(pdni, 0); 
    switch (ptni->ts) {
      case TS_VOID: /* ok if alone */ 
        assert(i == 0); /* we have checked earlier: this is return type */
        break;
      case TS_CHAR:  case TS_UCHAR:  case TS_SHORT: case TS_USHORT: case TS_ENUM: 
        cast = ptni->ts; ptni->ts = TS_INT;
      case TS_INT:   case TS_UINT:   case TS_LONG:  case TS_ULONG:  
      case TS_LLONG: case TS_ULLONG: case TS_FLOAT: case TS_DOUBLE: case TS_PTR: {
        if (i != 0 && name != 0) {
          vi_t *pvi = bufnewbk(&vib); pvi->name = name, pvi->sc = SC_REGISTER;
          pvi->reg = name, pvi->cast = cast;
        }
      } break;
      case TS_ARRAY: case TS_STRUCT: case TS_UNION: { 
        if (i == 0) { /* return value: insert $rp as first arg */
          node_t *prn = ndinsnew(ptn, 1), *pn; ptni = ndref(ptn, 0); /* re-fetch */
          ndset(prn, NT_VARDECL, ptni->pwsid, ptni->startpos);
          pn = ndinsbk(prn, NT_TYPE), pn->ts = TS_VOID; ndswap(pn, ptni);
          wrap_type_pointer(pn); prn->name = intern("$rp");
        } else { /* parameter: passed as pointer */
          if (name != 0) {
            vi_t *pvi = bufnewbk(&vib); pvi->name = name, pvi->sc = SC_AUTO;
            pvi->reg = internf("$p%s", symname(name));
            assert(pdni->nt == NT_VARDECL); /* patch in place */
            pdni->name = pvi->reg;
            if (ptni->ts == TS_ARRAY) flatten_type_array(ptni);
            else wrap_type_pointer(ptni);
          }
        }
      } break;
      default:
        neprintf(ptn, "function arguments of this type are not supported");
    }
  }

  /* wasmify hoisted vardefs, collect arg vars */
  assert(pbn->nt == NT_BLOCK);
  for (i = 0; i < ndlen(pbn); /* bump i */) {
    node_t *pdni = ndref(pbn, i), *ptni; sym_t name; ts_t cast = TS_VOID;
    if (pdni->nt != NT_VARDECL) break; /* statements may follow */
    name = pdni->name, ptni = ndref(pdni, 0); 
    switch (ptni->ts) {
      case TS_CHAR:  case TS_UCHAR:  case TS_SHORT: case TS_USHORT: case TS_ENUM: 
        cast = ptni->ts;
      case TS_INT:   case TS_UINT:   case TS_LONG:  case TS_ULONG:  
      case TS_LLONG: case TS_ULLONG: case TS_FLOAT: case TS_DOUBLE: case TS_PTR: {
        if (pdni->sc == SC_AUTO) { /* field in $fp frame struct */
          node_t *pvn = ndnewbk(&frame); vi_t *pvi = bufnewbk(&vib);
          ndset(pvn, NT_VARDECL, pdni->pwsid, pdni->startpos);
          pvn->name = name; ndpushbk(pvn, ptni);
          pvi->name = name, pvi->sc = SC_AUTO;
          pvi->reg = intern("$fp"); pvi->fld = name;
          ndrem(pbn, i);
        } else { /* normal register var */
          vi_t *pvi = bufnewbk(&vib); pvi->name = name, pvi->sc = SC_REGISTER;
          if (cast != TS_VOID) ptni->ts = TS_INT;
          pvi->reg = name, pvi->cast = cast;
          ++i;
        }
      } break;
      case TS_ARRAY: case TS_STRUCT: case TS_UNION: { 
        node_t *pvn = ndnewbk(&frame); vi_t *pvi = bufnewbk(&vib);
        ndset(pvn, NT_VARDECL, pdni->pwsid, pdni->startpos);
        pvn->name = name; ndpushbk(pvn, ptni);
        pvi->name = name, pvi->sc = SC_AUTO;
        pvi->reg = intern("$fp"); pvi->fld = name;
        ndrem(pbn, i); 
      } break;
      default: {
        assert(false);
        ++i;
      } break;
    }
  }
  
  /* see if we need to insert $fp init & local */
  if (ndlen(&frame) > 0) {
    node_t *pin = ndinsnew(pbn, i), *psn;
    ndset(pin, NT_ASSIGN, pbn->pwsid, pbn->startpos);
    pin->op = TT_ASN;
    psn = ndinsbk(pin, NT_IDENTIFIER); psn->name = intern("$fp");
    psn = ndinsbk(pin, NT_INTRCALL); psn->name = intern("alloca"), psn->intr = INTR_ALLOCA;
    psn = ndinsbk(psn, NT_INTRCALL); psn->name = intern("sizeof"), psn->intr = INTR_SIZEOF;
    ndpushbk(psn, &frame);
    pin = ndinsnew(pbn, i);
    ndset(pin, NT_VARDECL, pbn->pwsid, pbn->startpos);
    pin->name = intern("$fp"); wrap_type_pointer(ndpushbk(pin, &frame));
    i += 2; /* skip inserted nodes */ 
  }
  
  /* sort vib by name for faster lookup in wasmify_expr */
  bufqsort(&vib, sym_cmp);

  for (/* use current i */; i < ndlen(pbn); ++i) {
    node_t *pni = ndref(pbn, i);
    expr_wasmify(pni, &vib);
  }
  
  ndfini(&frame);
  buffini(&vib);
}


/* process function definition in main module */
static void process_fundef(sym_t mainmod, node_t *pn, module_t *pm)
{
  assert(pn->nt == NT_FUNDEF && pn->name && ndlen(pn) == 2);
  assert(ndref(pn, 0)->nt == NT_TYPE && ndref(pn, 0)->ts == TS_FUNCTION);
#ifdef _DEBUG
  fprintf(stderr, "process_fundef:\n");
  dump_node(pn, stderr);
#endif  
  fundef_check_type(ndref(pn, 0));
  { /* post appropriate vardecl for later use */
    node_t nd = mknd();
    ndset(&nd, NT_VARDECL, pn->pwsid, pn->startpos);
    nd.name = pn->name; ndpushbk(&nd, ndref(pn, 0)); 
    post_vardecl(mainmod, &nd); 
    ndfini(&nd); 
  }
  /* hoist local variables */
  fundef_hoist_locals(pn);
#ifdef _DEBUG
  fprintf(stderr, "fundef_hoist_locals ==>\n");
  dump_node(pn, stderr);
#endif  
  /* convert to wasm conventions, normalize */
  fundef_wasmify(pn);
#ifdef _DEBUG
  fprintf(stderr, "fundef_wasmify ==>\n");
  dump_node(pn, stderr);
#endif  
  /* todo: compile */
}

/* process top-level intrinsic call */
static void process_intrcall(node_t *pn, module_t *pm)
{
#ifdef _DEBUG
  dump_node(pn, stderr);
#endif
  if (pn->intr == INTR_SASSERT) {
    node_t *pen, *psn; int res;
    assert(ndlen(pn) == 1 || ndlen(pn) == 2);
    pen = ndref(pn, 0), psn = ndlen(pn) == 2 ? ndref(pn, 1) : NULL;
    if (psn && (psn->nt != NT_LITERAL || psn->ts != TS_STRING))
      neprintf(pn, "unexpected 2nd arg of static_assert (string literal expected)");
    if (!static_eval_to_int(pen, &res))
      n2eprintf(pen, pn, "unexpected lhs of static_assert comparison (static test expected)");
    if (res == 0) {
      if (!psn) neprintf(pn, "static_assert failed");
      else neprintf(pn, "static_assert failed (%s)", chbdata(&psn->data));
    }
  } else neprintf(pn, "unexpected top-level form");
}

/* process single top node (from module or include) */
static void process_top_node(sym_t mainmod, node_t *pn, module_t *pm)
{
  /* ignore empty blocks left from macros */
  if (pn->nt == NT_BLOCK && ndlen(pn) == 0) return;
  /* ignore typedefs (they are already processed) */  
  if (pn->nt == NT_TYPEDEF) {
#ifdef _DEBUG
    size_t size = 0, align = 0;
    dump_node(pn, stderr); assert(ndlen(pn) == 1);
    measure_type(ndref(pn, 0), pn, &size, &align, 0);
    fprintf(stderr, "size: %d, align: %d\n", (int)size, (int)align);
#endif  
    return;
  }
  /* remaining decls/defs are processed differently */
  if (!mainmod) {
    /* in header: declarations only */
    size_t i;
    if (pn->nt == NT_FUNDEF) neprintf(pn, "function definition in header");
    else if (pn->nt == NT_INTRCALL) process_intrcall(pn, pm); 
    else if (pn->nt != NT_BLOCK) neprintf(pn, "invalid top-level declaration");
    else if (!pn->name) neprintf(pn, "unscoped top-level declaration");
    else for (i = 0; i < ndlen(pn); ++i) {
      node_t *pni = ndref(pn, i);
      if (pni->nt == NT_ASSIGN) {
        neprintf(pni, "initialization in header");
      } else if (pni->nt == NT_VARDECL && pni->sc != SC_EXTERN) {
        neprintf(pni, "non-extern declaration in header");
      } else if (pni->nt == NT_VARDECL && pni->name && ndlen(pni) == 1) {
        post_vardecl(pn->name, pni);
      } else {
        neprintf(pni, "extern declaration expected");
      }
    }
  } else {
    /* in module mainmod: produce code */
    size_t i;
    if (pn->nt == NT_FUNDEF) process_fundef(mainmod, pn, pm);
    else if (pn->nt == NT_INTRCALL) process_intrcall(pn, pm); 
    else if (pn->nt != NT_BLOCK) neprintf(pn, "unexpected top-level declaration");
    else for (i = 0; i < ndlen(pn); ++i) {
      node_t *pni = ndref(pn, i);
      if (pni->nt == NT_VARDECL && pni->sc == SC_EXTERN) {
        neprintf(pni, "extern declaration in module (extern is for use in headers only)");
      } else if (pni->nt == NT_VARDECL) {
        if (i+1 < ndlen(pn) && ndref(pn, i+1)->nt == NT_ASSIGN) {
          process_vardecl(mainmod, pni, ndref(pn, ++i), pm);
        } else {
          process_vardecl(mainmod, pni, NULL, pm);
        }
      } else {
        neprintf(pni, "top-level declaration or definition expected");
      }
    }    
  }
}

/* parse/process include file and its includes */
static void process_include(pws_t *pw, int startpos, sym_t name, module_t *pm)
{
  pws_t *pwi; node_t nd = mknd();
  pwi = pws_from_modname(name, &g_bases);
  if (pwi) {
    /* this should be workspace #1... */
    assert(pwi->id > 0);
    while (parse_top_form(pwi, &nd)) {
      if (nd.nt == NT_INCLUDE) {
        process_include(pwi, nd.startpos, nd.name, pm);
      } else {
        process_top_node(0, &nd, pm);
      }
    }
    closepws(pwi);
  } else {
    reprintf(pw, startpos, 
      "cannot locate module %s: check -L option / WCPL_LIBRARY_PATH", 
      symname(name));
  }
  ndfini(&nd);
}

/* parse/process module file and its includes; return module name on success */
static sym_t process_module(const char *fname, module_t *pm)
{
  pws_t *pw; node_t nd = mknd(); sym_t mod = 0;
  pw = newpws(fname);
  if (pw) {
    /* this should be workspace #0 */
    assert(pw->id == 0);
    assert(pw->curmod);
    while (parse_top_form(pw, &nd)) {
      if (nd.nt == NT_INCLUDE) {
        process_include(pw, nd.startpos, nd.name, pm);
      } else {
        process_top_node(pw->curmod, &nd, pm);
      }
    }
    mod = pw->curmod;
    closepws(pw);
  } else {
    exprintf("cannot read module file: %s", fname);
  }
  ndfini(&nd);
  return mod;
}

/* convert type node to a valtype */
static void tn2vt(node_t *ptn, vtbuf_t *pvtb)
{
  assert(ptn->nt == NT_TYPE);
  switch (ptn->ts) {
    case TS_VOID: /* put nothing */ break;
    case TS_INT: case TS_UINT:  
      *vtbnewbk(pvtb) = NT_I32; break;  
    case TS_LONG: case TS_ULONG:  
    case TS_PTR: case TS_ARRAY:
      /* fixme: should depend on wasm32/wasm64 model */
      *vtbnewbk(pvtb) = NT_I32; break;  
    case TS_LLONG: case TS_ULLONG:  
      *vtbnewbk(pvtb) = NT_I64; break;
    case TS_FLOAT: /* legal in wasm, no auto promotion to double */
      *vtbnewbk(pvtb) = NT_F32; break;
    case TS_DOUBLE:
      *vtbnewbk(pvtb) = NT_F64; break;
    default:
      neprintf(ptn, "function arguments of this type are not supported");
  }
}

/* convert function type to a function signature */
static funcsig_t *ftn2fsig(node_t *ptn, funcsig_t *pfs)
{
  size_t i;
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  bufclear(&pfs->argtypes);
  bufclear(&pfs->rettypes);
  for (i = 0; i < ndlen(ptn); ++i) {
    node_t *ptni = ndref(ptn, i); assert(ptni->nt == NT_TYPE);
    tn2vt(ptni, i ? &pfs->argtypes : &pfs->rettypes);
  }
  return pfs;
}

/* compile module file to .wasm output file (if not NULL) */
void compile_module(const char *ifname, const char *ofname)
{
  module_t m; size_t i; sym_t mod;
  size_t fino = 0, gino = 0; /* insertion indices */
  funcsig_t fs;
  modinit(&m); fsinit(&fs);
  
  mod = process_module(ifname, &m); assert(mod);
  
  /* at this point, function/global tables are filled with mainmod definitions */
  /* walk g_syminfo/g_nodes to insert imports actually referenced in the code */
  for (i = 0; i < buflen(&g_syminfo); ++i) {
    int *pe = bufref(&g_syminfo, i); /* <sym_t, tt_t, info> */
    if (pe[1] == TT_IDENTIFIER && pe[2] >= 0 && pe[2] < (int)buflen(&g_nodes)) {
      sym_t id = pe[0];
      node_t *pn = bufref(&g_nodes, (size_t)pe[2]);
      if (pn->nt == NT_IMPORT && pn->sc == SC_EXTERN) {
        node_t *ptn; assert(pn->name && ndlen(pn) == 1);
        ptn = ndref(pn, 0); assert(ptn->nt == NT_TYPE);
        if (pn->name == mod) continue; /* not an import */
        if (ptn->ts == TS_FUNCTION) {
          /* mod=pn->name id=id ctype=ndref(pn, 0) */
          /* this mod:id needs to be imported as function, will get its idx */
          /* name=>idx info should be added to reloc table as function reloc */
          entry_t *pe; inscode_t *pri;
#ifdef _DEBUG
          fprintf(stderr, "imported function %s:%s =>\n", symname(pn->name), symname(id));
          dump_node(ptn, stderr);
#endif
          pe = entbins(&m.funcdefs, fino, EK_FUNC);
          pe->mod = pn->name, pe->name = id;
          pe->fsi = fsintern(&m.funcsigs, ftn2fsig(ptn, &fs));
          /* add reloc table entry */
          pri = icbnewbk(&m.reloctab); 
          pri->relkey = id;
          pri->in = IN_UNREACHABLE; 
          pri->arg.u = fino;
          ++fino;
        } else {
#ifdef _DEBUG
          fprintf(stderr, "imported global %s:%s =>\n", symname(pn->name), symname(id));
          dump_node(ptn, stderr);
#endif
        } 
      }
    }
  }
  
  /* use contents of g_dseg as passive data segment */
  /* fixme: global var $dp should be inited to the base address of dseg, and dsoff be taken from it! */
  if (chblen(&g_dseg) > 0) { 
    size_t dsoff = 16, dslen = chblen(&g_dseg) - 16;
    dseg_t *pds = dsegbnewbk(&m.datadefs, DS_ACTIVE); /* #0 */
    inscode_t *pic;
    chbset(&pds->data, chbdata(&g_dseg)+dsoff, dslen);
    pic = icbnewbk(&pds->code), pic->in = IN_I32_CONST, pic->arg.u = dsoff;
    pic = icbnewbk(&pds->code), pic->in = IN_END; 
  }
  /* add reloc entries for functions defined in mainmod */
  for (i = fino; i < entblen(&m.funcdefs); ++i) {
    entry_t *pe = entbref(&m.funcdefs, i);
    inscode_t *pri = icbnewbk(&m.reloctab);
    pri->relkey = pe->name; assert(pe->name);
    pri->in = IN_UNREACHABLE; 
    pri->arg.u = i;
  }
  /* add reloc entries for globals defined in mainmod */
  for (i = gino; i < entblen(&m.globdefs); ++i) {
    entry_t *pe = entbref(&m.globdefs, i);
    inscode_t *pri = icbnewbk(&m.reloctab);
    pri->relkey = pe->name; assert(pe->name);
    pri->in = IN_UNREACHABLE; 
    pri->arg.u = i;
  }
  /* sort reloctab by symbol before emitting */
  bufqsort(&m.reloctab, sym_cmp);

  /* emit wasm (use relocations) */
  if (ofname) {
    freopen(ofname, "wb", stdout);
    emit_module(&m);
  }

  /* done */
  fsfini(&fs);
  modfini(&m); 
}

#if 0
void emit_test_module(void)
{
  module_t mod;
  dseg_t *pds; /* eseg_t *pes; */
  entry_t *pe; inscode_t *pic;
  /* init */
  modinit(&mod);
  /* memory imports */
  pe = entbnewbk(&mod.memdefs, EK_MEM); /* only mem #0 is supported */
  pe->name = intern("memory"); /* local, exportable memory */
  pe->lt = LT_MIN, pe->n = 1; /* ask for one 64K page first, no max pages limit */
  /* data segments */
  pds = dsegbnewbk(&mod.datadefs, DS_ACTIVE); /* #0 */
  chbsets(&pds->data, "My first WASI module!\n");
  pic = icbnewbk(&pds->code), pic->in = IN_I32_CONST, pic->arg.u = 8;
  pic = icbnewbk(&pds->code), pic->in = IN_END; 
  /* fd_read (imported) */
  pe = entbnewbk(&mod.funcdefs, EK_FUNC); /* #0 */
  pe->mod = intern("wasi_snapshot_preview1"), pe->name = intern("fd_read");
  pe->fsi = funcsig(&mod.funcsigs, 4, 1, NT_I32, NT_I32, NT_I32, NT_I32, NT_I32);
  /* fd_write (imported) */
  pe = entbnewbk(&mod.funcdefs, EK_FUNC); /* #1 */
  pe->mod = intern("wasi_snapshot_preview1"), pe->name = intern("fd_write");
  pe->fsi = funcsig(&mod.funcsigs, 4, 1, NT_I32, NT_I32, NT_I32, NT_I32, NT_I32);
  /* foo */
  pe = entbnewbk(&mod.funcdefs, EK_FUNC); /* #2 */
  pe->name = intern("foo");
  pe->fsi = funcsig(&mod.funcsigs, 2, 1, NT_I32, NT_I32, NT_I32);
  pic = icbnewbk(&pe->code), pic->in = IN_LOCAL_GET, pic->arg.u = 1;
  pic = icbnewbk(&pe->code), pic->in = IN_LOCAL_GET, pic->arg.u = 0;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_ADD;
  pic = icbnewbk(&pe->code), pic->in = IN_END;
  /* _start */
  pe = entbnewbk(&mod.funcdefs, EK_FUNC); /* #3 */
  pe->name = intern("_start");
  pe->fsi = funcsig(&mod.funcsigs, 0, 0);
  //pe->isstart = true;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_CONST, pic->arg.u = 0;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_CONST, pic->arg.u = 8;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_STORE, pic->arg.u = 2, pic->align = 0;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_CONST, pic->arg.u = 4;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_CONST, pic->arg.u = 22;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_STORE, pic->arg.u = 2, pic->align = 0;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_CONST, pic->arg.u = 1;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_CONST, pic->arg.u = 0;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_CONST, pic->arg.u = 1;
  pic = icbnewbk(&pe->code), pic->in = IN_I32_CONST, pic->arg.u = 0;
  pic = icbnewbk(&pe->code), pic->in = IN_CALL, pic->arg.u = 1; /* fd_write */
  pic = icbnewbk(&pe->code), pic->in = IN_DROP;
  pic = icbnewbk(&pe->code), pic->in = IN_END;
  /* emit wasm */
  emit_module(&mod);
  /* done */
  modfini(&mod); 
}
#endif

int main(int argc, char **argv)
{
  int opt;
  const char *ifile_arg = "-";
  const char *ofile_arg = NULL;
  const char *lpath;
  dsbuf_t libv; 
  
  dsbinit(&libv);
  lpath = ""; dsbpushbk(&libv, (dstr_t*)&lpath);
  if ((lpath = getenv("WCPL_LIBRARY_PATH")) != NULL) dsbpushbk(&libv, (dstr_t*)&lpath);

  setprogname(argv[0]);
  setusage
    ("[OPTIONS] [infile]\n"
     "options are:\n"
     "  -w        Suppress warnings\n"
     "  -v        Increase verbosity\n"
     "  -q        Suppress logging ('quiet')\n"
     "  -i ifile  Input file; defaults to - (stdout)\n"
     "  -o ofile  Output file; defaults to a.wasm\n"
     "  -L path   Add library path\n"
     "  -h        This help");
  while ((opt = egetopt(argc, argv, "wvqi:o:L:h")) != EOF) {
    switch (opt) {
      case 'w':  setwlevel(3); break;
      case 'v':  incverbosity(); break;
      case 'q':  incquietness(); break;
      case 'i':  ifile_arg = eoptarg; break;
      case 'o':  ofile_arg = eoptarg; break;
      case 'L':  dsbpushbk(&libv, &eoptarg); break;
      case 'h':  eusage("WCPL 0.01 built on "__DATE__);
    }
  }

  /* get optional script file and argument list */
  if (eoptind < argc) ifile_arg = argv[eoptind++];
  if (eoptind < argc) eusage("too many input files");

  init_compiler(&libv);
  compile_module(ifile_arg, ofile_arg);
  fini_compiler();
  
  dsbfini(&libv);
  return EXIT_SUCCESS;
}

