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
#include "w.h"
#include "p.h"
#include "c.h"

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
  init_nodepool();
  init_regpool();
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
  fini_nodepool();
  fini_regpool();
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

valtype_t ts_to_blocktype(ts_t ts)
{
  switch (ts) {
    case TS_VOID:                   return BT_VOID;
    case TS_LONG:  case TS_ULONG:   /* fall thru (assumes wasm32) */
    case TS_PTR:                    /* fall thru (assumes wasm32) */
    case TS_INT:   case TS_UINT:    return VT_I32;
    case TS_LLONG: case TS_ULLONG:  return VT_I64;
    case TS_FLOAT:                  return VT_F32;
    case TS_DOUBLE:                 return VT_F64;
  }
  return VT_UNKN;
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
    case TS_LONG:                   return TS_LONG;
    case TS_ULONG:                  return TS_ULONG;
    case TS_LLONG: case TS_ULLONG:  
    case TS_FLOAT: case TS_DOUBLE:  return ts;
    default: assert(false);
  }
  return ts; /* shouldn't happen */
}

/* check if ts value can work as test in boolean context */
static bool ts_booltest(ts_t ts) 
{
  return ts == TS_PTR || ts_numerical(ts);
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
  if (ts1 == TS_ULONG || ts2 == TS_ULONG) return TS_ULONG;
  if (ts1 == TS_LONG || ts2 == TS_LONG) return TS_LONG;
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
size_t measure_offset(node_t *ptn, node_t *prn, sym_t fld, node_t **ppftn)
{
  assert(ptn && ptn->nt == NT_TYPE);
  switch (ptn->ts) {
    case TS_UNION: {
      size_t i;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(TS_UNION, ptn->name);
        if (!pftn) n2eprintf(ptn, prn, "can't measure incomplete union type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        node_t *pni = ndref(ptn, i);
        if (pni->nt == NT_VARDECL && pni->name == fld) {
          if (ppftn) *ppftn = ndref(pni, 0);
          return 0;
        }
      }
      n2eprintf(ptn, prn, "can't find field %s in union", symname(fld));
    } break;
    case TS_STRUCT: {
      size_t size, align, i, n, cursize = 0;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(TS_STRUCT, ptn->name);
        if (!pftn) n2eprintf(ptn, prn, "can't measure incomplete struct type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        node_t *pni = ndref(ptn, i); sym_t s = 0;
        if (pni->nt == NT_VARDECL && ndlen(pni) == 1) s = pni->name, pni = ndref(pni, 0);
        measure_type(pni, prn, &size, &align, 0);
        /* round size up so it is multiple of pni align */
        if ((n = cursize % align) > 0) cursize += align - n;
        if (s == fld) {
          if (ppftn) *ppftn = pni;
          return cursize;
        }
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
        if (pn->intr == INTR_SIZEOF) prn->val.i = (int)size;
        else prn->val.i = (int)align;
        return true;
      } else if (pn->intr == INTR_OFFSETOF) {
        size_t offset; assert(ndlen(pn) == 2 && ndref(pn, 1)->nt == NT_IDENTIFIER);
        offset = measure_offset(ndref(pn, 0), pn, ndref(pn, 1)->name, NULL);
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

static void check_static_assert(node_t *pn)
{
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
  }
}

/* check if node can be evaluated statically (numbers only)
 * NB: positive answer does not guarantee successful or error-free evaluation
 * (but eval errors will also happen at run time if original expr is compiled);
 * negative answer saves time on attempting actual evaluation */
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
      case TS_ENUM: /* ok, treated as int */
      case TS_CHAR: case TS_UCHAR:   /* ok but widened to int */
      case TS_SHORT: case TS_USHORT: /* ok but widened to int */
      case TS_INT: case TS_UINT: 
      case TS_LONG: case TS_ULONG:  
      case TS_LLONG: case TS_ULLONG:  
      case TS_FLOAT: case TS_DOUBLE:
      case TS_PTR: 
        break;
      case TS_ARRAY: 
        neprintf(ptn, "not supported: array as function parameter/return type");
        break;
      case TS_STRUCT:
        neprintf(ptn, "not supported: struct as function parameter/return type");
        break;
       case TS_UNION:
        neprintf(ptn, "not supported: union as function parameter/return type");
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
static void expr_hoist_locals(node_t *ptn, node_t *pn, henv_t *phe, ndbuf_t *plb)
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
          expr_hoist_locals(ptn, pni, &he, plb);
        }
      }
      buffini(&he.idmap);    
    } break;
    case NT_VARDECL: {
      neprintf(pn, "unexpected variable declaration");
    } break;
    case NT_PREFIX: {
      assert(ndlen(pn) == 1);
      expr_hoist_locals(ptn, ndref(pn, 0), phe, plb);
      if (pn->op == TT_AND && ndref(pn, 0)->nt == NT_IDENTIFIER) {
        /* address-of var is taken: mark it as auto */
        sym_t name = ndref(pn, 0)->name; size_t i;
        for (i = 0; i < ndblen(plb); ++i) {
          node_t *pndi = ndbref(plb, i); 
          assert(pndi->nt == NT_VARDECL);
          if (pndi->name != name) continue;
          if (pndi->sc == SC_REGISTER) 
            n2eprintf(pn, pndi, "cannot take address of register var");
          else pndi->sc = SC_AUTO;
          break;
        }
        /* if this is an arg var, mark it as auto too */
        if (i == ndblen(plb)) { /* not a local */
          for (i = 1 /* skip ret type */; i < ndlen(ptn); ++i) {
            node_t *pndi = ndref(ptn, i);
            if (pndi->nt != NT_VARDECL) continue; 
            if (pndi->name != name) continue;
            if (pndi->sc == SC_NONE) pndi->sc = SC_AUTO;
            break; 
          }
        }
      } 
    } break;
    case NT_TYPE: {
      /* no vars here */
    } break;
    default: {
      size_t i;
      for (i = 0; i < ndlen(pn); ++i) {
        expr_hoist_locals(ptn, ndref(pn, i), phe, plb);
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
  expr_hoist_locals(ptn, pbn, &he, &locals);
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

/* convert node wasm conventions, simplify (get rid of -> and []) */
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
          wrap_postfix_operator
            (wrap_unary_operator(pn, pn->startpos, TT_STAR), 
             TT_DOT, pvi->fld);
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
      if (pn->op == TT_ARROW) { /* (&x)->y => x.y; x->y => (*x).y */
        node_t *psn = ndref(pn, 0);
        if (psn->nt == NT_PREFIX && psn->op == TT_AND) lift_arg0(psn);
        else wrap_unary_operator(psn, psn->startpos, TT_STAR);
        pn->op = TT_DOT;
      } 
    } break;
    case NT_RETURN: {
      sym_t fp = intern("fp$"); node_t *psn;
      vi_t *pvi = bufbsearch(pvib, &fp, sym_cmp);
      if (ndlen(pn) > 0) {
        assert(ndlen(pn) == 1);
        expr_wasmify(ndref(pn, 0), pvib);
      }
      if (pvi) { /* return ... => freea(fp$), return ... */
        wrap_node(pn, NT_COMMA);
        psn = ndinsfr(pn, NT_INTRCALL); psn->intr = INTR_FREEA;
        psn = ndinsbk(psn, NT_IDENTIFIER); psn->name = fp;
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

/* convert function to wasm conventions, simplify */
static void fundef_wasmify(node_t *pdn)
{
  /* special registers (scalar/poiner vars with internal names): 
   * fp$ (frame pointer), ap$ (... data pointer)? */
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1);
  buf_t vib = mkbuf(sizeof(vi_t)); node_t frame = mknd();
  ndbuf_t asb; size_t i; ndbinit(&asb);
  
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
          vi_t *pvi = bufnewbk(&vib);
          if (pdni->nt == NT_VARDECL && pdni->sc == SC_AUTO) {
            sym_t hname = internf("%s#", symname(name));
            node_t *pni = ndbnewbk(&asb), *psn;
            ndset(pni, NT_ASSIGN, pdni->pwsid, pdni->startpos); pni->op = TT_ASN;
            psn = ndinsbk(pni, NT_IDENTIFIER); psn->name = name;
            psn = ndinsbk(pni, NT_IDENTIFIER); psn->name = hname;
            pni = ndinsfr(pbn, NT_VARDECL); pni->sc = SC_AUTO;
            pni->name = name; ndcpy(ndnewbk(pni), ptni);
            pdni->name = pvi->name = pvi->reg = hname;
            pdni->sc = SC_NONE;
          } else {
            pvi->name = pvi->reg = name;
          }
          pvi->sc = SC_REGISTER; pvi->cast = cast;
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
        if (pdni->sc == SC_AUTO) { /* field in fp$ frame struct */
          node_t *pvn = ndnewbk(&frame); vi_t *pvi = bufnewbk(&vib);
          ndset(pvn, NT_VARDECL, pdni->pwsid, pdni->startpos);
          pvn->name = name; ndpushbk(pvn, ptni);
          pvi->name = name, pvi->sc = SC_AUTO;
          pvi->reg = intern("fp$"); pvi->fld = name;
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
        pvi->reg = intern("fp$"); pvi->fld = name;
        ndrem(pbn, i); 
      } break;
      default: {
        assert(false);
        ++i;
      } break;
    }
  }
  
  /* see if we need to insert assigns for shadowed vars */
  while (ndblen(&asb) > 0) {
    ndswap(ndinsnew(pbn, i), ndbref(&asb, 0));
    ndbrem(&asb, 0);
  }
  
  /* see if we need to insert fp$ init & local */
  if (ndlen(&frame) > 0) {
    node_t *pin = ndinsnew(pbn, i), *psn; vi_t *pvi;
    ndset(pin, NT_ASSIGN, pbn->pwsid, pbn->startpos);
    pin->op = TT_ASN;
    psn = ndinsbk(pin, NT_IDENTIFIER); psn->name = intern("fp$");
    psn = ndinsbk(pin, NT_INTRCALL); psn->intr = INTR_ALLOCA;
    psn = ndinsbk(psn, NT_INTRCALL); psn->intr = INTR_SIZEOF;
    ndpushbk(psn, &frame);
    pin = ndinsnew(pbn, i);
    ndset(pin, NT_VARDECL, pbn->pwsid, pbn->startpos);
    pin->name = intern("fp$"); wrap_type_pointer(ndpushbk(pin, &frame));
    pvi = bufnewbk(&vib); pvi->sc = SC_REGISTER;
    pvi->name = pvi->reg = pin->name;
    i += 2; /* skip inserted nodes */ 
  }
  
  /* sort vib by name for faster lookup in wasmify_expr */
  bufqsort(&vib, sym_cmp);

  for (/* use current i */; i < ndlen(pbn); ++i) {
    node_t *pni = ndref(pbn, i);
    expr_wasmify(pni, &vib);
  }

  /* check if falling off is a valid way to return */
  if (ndref(ptn, 0)->ts == TS_VOID) {
    /* if fp$ is present, insert freea(fp$); */
    sym_t fp = intern("fp$"); 
    vi_t *pvi = bufbsearch(&vib, &fp, sym_cmp);
    if (pvi) {
      node_t *psn = ndinsbk(pbn, NT_INTRCALL); psn->intr = INTR_FREEA;
      psn = ndinsbk(psn, NT_IDENTIFIER); psn->name = fp;
    }
  }

  ndbfini(&asb);
  ndfini(&frame);
  buffini(&vib);
}

/* 'register' (wasm local var) info for typecheck */
typedef struct ri_tag {
  sym_t name; /* register or global var name */
  const node_t *ptn; /* NT_TYPE (not owned) */
} ri_t;

static const node_t *lookup_var_type(sym_t name, buf_t *prib, bool *pbgl)
{
  ri_t *pri; const node_t *pgn;
  if ((pri = bufbsearch(prib, &name, sym_cmp)) != NULL) {
    *pbgl = false;
    return pri->ptn;
  } else if ((pgn = lookup_global(name)) != NULL) {
    *pbgl = true;
    if (pgn->nt == NT_IMPORT && ndlen(pgn) == 1)
      return ndcref(pgn, 0);
  }
  return NULL;
}

/* check if tan type can be assigned to parameter/lval tpn type */
static bool assign_compatible(const node_t *ptpn, const node_t *ptan)
{
  if (same_type(ptan, ptpn)) return true;
  if (ts_numerical(ptan->ts) && ts_numerical(ptpn->ts)) 
    return ts_arith_common(ptan->ts, ptpn->ts) == ptpn->ts; /* can be promoted up */
  if (ptan->ts == TS_ARRAY && ptpn->ts == TS_PTR)
    return same_type(ndcref(ptan, 0), ndcref(ptpn, 0));
  if (ptan->ts == TS_PTR && ptpn->ts == TS_PTR)
    return ndcref(ptan, 0)->ts == TS_VOID || ndcref(ptpn, 0)->ts == TS_VOID;
  return false;
}

/* check if tan type can be cast to tpn type */
static bool cast_compatible(const node_t *ptpn, const node_t *ptan)
{
  /* any two number types can be explicitly cast to each other */
  if (ts_numerical(ptpn->ts) && ts_numerical(ptan->ts)) return true; 
  /* any two pointer types can be explicitly cast to each other */
  if (ptpn->ts == TS_PTR && ptan->ts == TS_PTR) return true;
  /* allow long->ptr and ptr->long casts */
  if (ptpn->ts == TS_PTR && ts_numerical(ptan->ts))
    return ts_arith_common(ptan->ts, TS_ULONG) == TS_ULONG; /* can be promoted up to ULONG */
  if (ts_numerical(ptpn->ts) && ptan->ts == TS_PTR)
    return ts_arith_common(ptpn->ts, TS_ULONG) == ptpn->ts; /* ULONG and up */
  /* the rest must be compatible */
  return assign_compatible(ptpn, ptan);
}


/* procedures producing assembly instructions */

/* produce cast instructions to convert arithmetical types */
static void asm_numerical_cast(ts_t tsto, ts_t tsfrom, icbuf_t *pdata)
{
  instr_t in0 = 0, in = 0; 
  assert(ts_numerical(tsto) && ts_numerical(tsfrom));
  if (tsto == tsfrom) return; /* no change */
  switch (tsto) {
    case TS_DOUBLE:
      switch (tsfrom) {
        case TS_FLOAT:                           in = IN_F64_PROMOTE_F32;   break;
        case TS_CHAR:   in0 = IN_I32_EXTEND8_S,  in = IN_F64_CONVERT_I32_S; break; 
        case TS_SHORT:  in0 = IN_I32_EXTEND16_S, in = IN_F64_CONVERT_I32_S; break; 
        case TS_INT: case TS_LONG:/* wasm32 */   in = IN_F64_CONVERT_I32_S; break;
        case TS_UCHAR:                           in = IN_F64_CONVERT_I32_U; break;
        case TS_USHORT:                          in = IN_F64_CONVERT_I32_U; break;
        case TS_UINT: case TS_ULONG:/* wasm32 */ in = IN_F64_CONVERT_I32_U; break;
        case TS_LLONG:                           in = IN_F64_CONVERT_I64_S; break;
        case TS_ULLONG:                          in = IN_F64_CONVERT_I64_U; break;
      } break;
    case TS_FLOAT:
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_F32_DEMOTE_F64;    break;
        case TS_CHAR:   in0 = IN_I32_EXTEND8_S,  in = IN_F32_CONVERT_I32_S; break; 
        case TS_SHORT:  in0 = IN_I32_EXTEND16_S, in = IN_F32_CONVERT_I32_S; break; 
        case TS_INT: case TS_LONG:/* wasm32 */   in = IN_F32_CONVERT_I32_S; break;
        case TS_UCHAR:                           in = IN_F32_CONVERT_I32_U; break;
        case TS_USHORT:                          in = IN_F32_CONVERT_I32_U; break;
        case TS_UINT: case TS_ULONG:/* wasm32 */ in = IN_F32_CONVERT_I32_U; break;
        case TS_LLONG:                           in = IN_F32_CONVERT_I64_S; break;
        case TS_ULLONG:                          in = IN_F32_CONVERT_I64_U; break;
      } break;
    case TS_CHAR: case TS_SHORT: { /* replace upper bits w/sign bit */
        inscode_t *pic; unsigned sh = (tsto == TS_CHAR) ? 24 : 16;
        asm_numerical_cast(TS_INT, tsfrom, pdata); 
        pic = icbnewbk(pdata); pic->in = IN_I32_CONST; pic->arg.u = sh;
        icbnewbk(pdata)->in = IN_I32_SHL;
        pic = icbnewbk(pdata); pic->in = IN_I32_CONST; pic->arg.u = sh;
        icbnewbk(pdata)->in = IN_I32_SHR_S;
        return;
      } break;  
    case TS_UCHAR: case TS_USHORT: { /* mask upper bits */
        inscode_t *pic; unsigned m = (tsto == TS_CHAR) ? 0xFF : 0xFFFF;
        asm_numerical_cast(TS_UINT, tsfrom, pdata);
        pic = icbnewbk(pdata); pic->in = IN_I32_CONST; pic->arg.u = m;
        icbnewbk(pdata)->in = IN_I32_AND;
        return;
      } break;  
    case TS_INT: case TS_LONG: /* LONG: wasm32 assumed */
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_I32_TRUNC_F64_S;   break;
        case TS_FLOAT:                           in = IN_I32_TRUNC_F32_S;   break;
        case TS_CHAR:                            in = IN_I32_EXTEND8_S;     break; 
        case TS_SHORT:                           in = IN_I32_EXTEND16_S;    break; 
        case TS_INT: case TS_LONG:/* wasm32 */   /* nop */                  break;
        case TS_UCHAR:                           /* nop */                  break;
        case TS_USHORT:                          /* nop */                  break;
        case TS_UINT: case TS_ULONG:/* wasm32 */ /* nop */                  break;
        case TS_LLONG:                           in = IN_I32_WRAP_I64;      break;
        case TS_ULLONG:                          in = IN_I32_WRAP_I64;      break;
      } break;
    case TS_UINT: case TS_ULONG: /* ULONG: wasm32 assumed */
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_I32_TRUNC_F64_U;   break;
        case TS_FLOAT:                           in = IN_I32_TRUNC_F32_U;   break;
        case TS_CHAR:                            in = IN_I32_EXTEND8_S;     break; 
        case TS_SHORT:                           in = IN_I32_EXTEND16_S;    break; 
        case TS_INT: case TS_LONG:/* wasm32 */   /* nop */                  break;
        case TS_UCHAR:                           /* nop */                  break;
        case TS_USHORT:                          /* nop */                  break;
        case TS_UINT: case TS_ULONG:/* wasm32 */ /* nop */                  break;
        case TS_LLONG:                           in = IN_I32_WRAP_I64;      break;
        case TS_ULLONG:                          in = IN_I32_WRAP_I64;      break;
      } break;
    case TS_LLONG:
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_I64_TRUNC_F64_S;   break;
        case TS_FLOAT:                           in = IN_I64_TRUNC_F32_S;   break;
        case TS_CHAR:  in0 = IN_I32_EXTEND8_S;   in = IN_I64_EXTEND_I32_S;  break; 
        case TS_SHORT: in0 = IN_I32_EXTEND16_S;  in = IN_I64_EXTEND_I32_S;  break; 
        case TS_INT: case TS_LONG:/* wasm32 */   in = IN_I64_EXTEND_I32_S;  break;
        case TS_UCHAR: case TS_USHORT:
        case TS_UINT: case TS_ULONG:/* wasm32 */ in = IN_I64_EXTEND_I32_U;  break;
        case TS_LLONG:                           /* nop */                  break;
        case TS_ULLONG:                          /* nop */                  break;
      } break;
    case TS_ULLONG:
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_I64_TRUNC_F64_U;   break;
        case TS_FLOAT:                           in = IN_I64_TRUNC_F32_U;   break;
        case TS_CHAR:  in0 = IN_I32_EXTEND8_S;   in = IN_I64_EXTEND_I32_S;  break; 
        case TS_SHORT: in0 = IN_I32_EXTEND16_S;  in = IN_I64_EXTEND_I32_S;  break; 
        case TS_INT: case TS_LONG:/* wasm32 */   in = IN_I64_EXTEND_I32_S;  break;
        case TS_UCHAR: case TS_USHORT:
        case TS_UINT: case TS_ULONG:/* wasm32 */ in = IN_I64_EXTEND_I32_U;  break;
        case TS_LLONG:                           /* nop */                  break;
        case TS_ULLONG:                          /* nop */                  break;
      } break;
  }
  if (in0) icbnewbk(pdata)->in = in0;
  if (in) icbnewbk(pdata)->in = in;
}

/* produce load instruction to fetch from a scalar pointer */
static void asm_load(ts_t tsto, ts_t tsfrom, unsigned off, icbuf_t *pdata)
{
  instr_t in = 0; unsigned align = 0; inscode_t *pin;
  switch (tsto) { /* TS_PTR or result of ts_integral_promote(tsfrom) */
    case TS_PTR: case TS_LONG: case TS_ULONG: /* wasm32 */
    case TS_INT: case TS_UINT:
      switch (tsfrom) {
        case TS_CHAR:   in = IN_I32_LOAD8_S;  align = 0; break;
        case TS_UCHAR:  in = IN_I32_LOAD8_U;  align = 0; break;
        case TS_SHORT:  in = IN_I32_LOAD16_S; align = 1; break;
        case TS_USHORT: in = IN_I32_LOAD16_U; align = 1; break;
        case TS_PTR: case TS_LONG: case TS_ULONG: /* wasm32 assumed */
        case TS_INT: case TS_UINT: in = IN_I32_LOAD; align = 2; break;
      } break;
    case TS_LLONG: case TS_ULLONG:
      switch (tsfrom) {
        case TS_CHAR:   in = IN_I64_LOAD8_S;  align = 0; break;
        case TS_UCHAR:  in = IN_I64_LOAD8_U;  align = 0; break;
        case TS_SHORT:  in = IN_I64_LOAD16_S; align = 1; break;
        case TS_USHORT: in = IN_I64_LOAD16_U; align = 1; break;
        case TS_PTR: case TS_ULONG: /* wasm32 assumed */
        case TS_UINT:   in = IN_I64_LOAD32_U; align = 2; break; 
        case TS_LONG: /* wasm32 assumed */
        case TS_INT :   in = IN_I64_LOAD32_S; align = 2; break; 
        case TS_LLONG: case TS_ULLONG: in = IN_I64_LOAD; align = 3; break;
      } break;
    case TS_FLOAT:
      switch (tsfrom) {
        case TS_FLOAT:  in = IN_F32_LOAD; align = 2; break;      
      } break;
    case TS_DOUBLE:
      switch (tsfrom) {
        case TS_DOUBLE: in = IN_F64_LOAD; align = 3; break;      
      } break;
  }
  assert(in);
  pin = icbnewbk(pdata); 
  pin->in = in; pin->arg.u = off; pin->argu2 = align;
}

/* produce store instruction from a load instruction */
static void asm_instr_load_to_store(const inscode_t *plic, inscode_t *psic)
{
  instr_t in = 0; 
  switch (plic->in) {
    case IN_LOCAL_GET: in = IN_LOCAL_SET; break;
    case IN_GLOBAL_GET: in = IN_GLOBAL_SET; break;
    case IN_I32_LOAD: in = IN_I32_STORE; break;
    case IN_I64_LOAD: in = IN_I64_STORE; break;
    case IN_F32_LOAD: in = IN_F32_STORE; break;
    case IN_F64_LOAD: in = IN_F64_STORE; break;
    case IN_I32_LOAD8_S:  case IN_I32_LOAD8_U:  in = IN_I32_STORE8; break;
    case IN_I32_LOAD16_S: case IN_I32_LOAD16_U: in = IN_I32_STORE16; break;
    case IN_I64_LOAD8_S:  case IN_I64_LOAD8_U:  in = IN_I64_STORE8; break;
    case IN_I64_LOAD16_S: case IN_I64_LOAD16_U: in = IN_I64_STORE16; break;
    case IN_I64_LOAD32_S: case IN_I64_LOAD32_U: in = IN_I64_STORE32; break;
  }
  assert(in);
  *psic = *plic; psic->in = in;
}

/* produce cast instructions as needed to convert tsn to cast_compatible ttn type */
static void asm_cast(const node_t *ptpn, const node_t *ptan, icbuf_t *pdata)
{
  if (ts_numerical(ptpn->ts) && ts_numerical(ptan->ts)) {
    asm_numerical_cast(ptpn->ts, ptan->ts, pdata);
  } else if (ptpn->ts == TS_PTR && ptan->ts == TS_PTR) {
    /* no representation change */
  } else if (ptpn->ts == TS_PTR && ts_numerical(ptan->ts)) {
    asm_numerical_cast(TS_ULONG, ptan->ts, pdata); /* wasm32 assumed */
  } else if (ts_numerical(ptpn->ts) && ptan->ts == TS_PTR) {
    asm_numerical_cast(ptpn->ts, TS_ULONG, pdata); /* wasm32 assumed */
  } else {
    /* same type: no representation change */
  }
}

/* encode unary assembly operator; return false if not applicable */
static bool asm_unary(ts_t ts, tt_t op, icbuf_t *pdata)
{
  switch (ts) {
    case TS_INT: case TS_UINT:
    case TS_LONG: case TS_ULONG: /* wasm32 model (will be different under wasm64) */
      switch (op) {
        case TT_PLUS: return true;
        case TT_NOT: icbnewbk(pdata)->in = IN_I32_EQZ; return true;
      } break;
    case TS_LLONG: case TS_ULLONG: 
      switch (op) {
        case TT_PLUS: return true;
        case TT_NOT: icbnewbk(pdata)->in = IN_I64_EQZ; return true;
      } break;
    case TS_FLOAT:
      switch (op) {
        case TT_PLUS: return true;
        case TT_MINUS: icbnewbk(pdata)->in = IN_F32_NEG; return true;
      } break;
    case TS_DOUBLE: 
      switch (op) {
        case TT_PLUS: return true;
        case TT_MINUS: icbnewbk(pdata)->in = IN_F64_NEG; return true;
      } break;
    default: assert(false);
  }
  return false;
}

/* encode binary assembly operator; return false if not applicable */
static bool asm_binary(ts_t ts, tt_t op, icbuf_t *pdata)
{
  instr_t in = 0;
  switch (ts) {
    case TS_INT: case TS_LONG: /* wasm32 model (will be different under wasm64) */
      switch (op) {
        case TT_PLUS:  in = IN_I32_ADD; break;
        case TT_MINUS: in = IN_I32_SUB; break;
        case TT_STAR:  in = IN_I32_MUL; break;
        case TT_AND:   in = IN_I32_AND; break;
        case TT_OR:    in = IN_I32_OR; break;
        case TT_XOR:   in = IN_I32_XOR; break;
        case TT_SLASH: in = IN_I32_DIV_S; break;
        case TT_REM:   in = IN_I32_REM_S; break;
        case TT_SHL:   in = IN_I32_SHL; break;
        case TT_SHR:   in = IN_I32_SHR_S; break;
        case TT_EQ:    in = IN_I32_EQ; break;
        case TT_NE:    in = IN_I32_NE; break;
        case TT_LT:    in = IN_I32_LT_S; break;
        case TT_GT:    in = IN_I32_GT_S; break;
        case TT_LE:    in = IN_I32_LE_S; break;
        case TT_GE:    in = IN_I32_GE_S; break;
      } break;
    case TS_UINT: case TS_ULONG: /* wasm32 model (will be different under wasm64) */
      switch (op) {
        case TT_PLUS:  in = IN_I32_ADD; break;
        case TT_MINUS: in = IN_I32_SUB; break;
        case TT_STAR:  in = IN_I32_MUL; break;
        case TT_AND:   in = IN_I32_AND; break;
        case TT_OR:    in = IN_I32_OR; break;
        case TT_XOR:   in = IN_I32_XOR; break;
        case TT_SLASH: in = IN_I32_DIV_U; break;
        case TT_REM:   in = IN_I32_REM_U; break;
        case TT_SHL:   in = IN_I32_SHL; break;
        case TT_SHR:   in = IN_I32_SHR_U; break;
        case TT_EQ:    in = IN_I32_EQ; break;
        case TT_NE:    in = IN_I32_NE; break;
        case TT_LT:    in = IN_I32_LT_U; break;
        case TT_GT:    in = IN_I32_GT_U; break;
        case TT_LE:    in = IN_I32_LE_U; break;
        case TT_GE:    in = IN_I32_GE_U; break;
      } break;
    case TS_LLONG: 
      switch (op) {
        case TT_PLUS:  in = IN_I64_ADD; break;
        case TT_MINUS: in = IN_I64_SUB; break;
        case TT_STAR:  in = IN_I64_MUL; break;
        case TT_AND:   in = IN_I64_AND; break;
        case TT_OR:    in = IN_I64_OR; break;
        case TT_XOR:   in = IN_I64_XOR; break;
        case TT_SLASH: in = IN_I64_DIV_S; break;
        case TT_REM:   in = IN_I64_REM_S; break;
        case TT_SHL:   in = IN_I64_SHL; break;
        case TT_SHR:   in = IN_I64_SHR_S; break;
        case TT_EQ:    in = IN_I64_EQ; break;
        case TT_NE:    in = IN_I64_NE; break;
        case TT_LT:    in = IN_I64_LT_S; break;
        case TT_GT:    in = IN_I64_GT_S; break;
        case TT_LE:    in = IN_I64_LE_S; break;
        case TT_GE:    in = IN_I64_GE_S; break;
      } break;
    case TS_ULLONG: 
      switch (op) {
        case TT_PLUS:  in = IN_I64_ADD; break;
        case TT_MINUS: in = IN_I64_SUB; break;
        case TT_STAR:  in = IN_I64_MUL; break;
        case TT_AND:   in = IN_I64_AND; break;
        case TT_OR:    in = IN_I64_OR; break;
        case TT_XOR:   in = IN_I64_XOR; break;
        case TT_SLASH: in = IN_I64_DIV_U; break;
        case TT_REM:   in = IN_I64_REM_U; break;
        case TT_SHL:   in = IN_I64_SHL; break;
        case TT_SHR:   in = IN_I64_SHR_U; break;
        case TT_EQ:    in = IN_I64_EQ; break;
        case TT_NE:    in = IN_I64_NE; break;
        case TT_LT:    in = IN_I64_LT_U; break;
        case TT_GT:    in = IN_I64_GT_U; break;
        case TT_LE:    in = IN_I64_LE_U; break;
        case TT_GE:    in = IN_I64_GE_U; break;
      } break;
    case TS_FLOAT: 
      switch (op) {
        case TT_PLUS:  in = IN_F32_ADD; break;
        case TT_MINUS: in = IN_F32_SUB; break;
        case TT_STAR:  in = IN_F32_MUL; break;
        case TT_SLASH: in = IN_F32_DIV; break;
        case TT_EQ:    in = IN_F32_EQ; break;
        case TT_NE:    in = IN_F32_NE; break;
        case TT_LT:    in = IN_F32_LT; break;
        case TT_GT:    in = IN_F32_GT; break;
        case TT_LE:    in = IN_F32_LE; break;
        case TT_GE:    in = IN_F32_GE; break;
      } break;
    case TS_DOUBLE: 
      switch (op) {
        case TT_PLUS:  in = IN_F64_ADD; break;
        case TT_MINUS: in = IN_F64_SUB; break;
        case TT_STAR:  in = IN_F64_MUL; break;
        case TT_SLASH: in = IN_F64_DIV; break;
        case TT_EQ:    in = IN_F64_EQ; break;
        case TT_NE:    in = IN_F64_NE; break;
        case TT_LT:    in = IN_F64_LT; break;
        case TT_GT:    in = IN_F64_GT; break;
        case TT_LE:    in = IN_F64_LE; break;
        case TT_GE:    in = IN_F64_GE; break;
      } break;
    default: assert(false);
  }
  if (!in) return false;
  icbnewbk(pdata)->in = in; 
  return true;
}

/* produce numerical constant instruction from literal */
static void asm_numerical_constant(ts_t ts, numval_t *pval, icbuf_t *pdata)
{
  inscode_t *pic = icbnewbk(pdata);
  switch (ts) {
    case TS_INT: case TS_UINT:
    case TS_LONG: case TS_ULONG: /* wasm32 model (will be different under wasm64) */
      pic->in = IN_I32_CONST; pic->arg.u = pval->u;
      break;
    case TS_LLONG: case TS_ULLONG: 
      pic->in = IN_I64_CONST; pic->arg.u = pval->u;
      break;
    case TS_FLOAT:
      pic->in = IN_F32_CONST; pic->arg.f = pval->f;
      break;
    case TS_DOUBLE: 
      pic->in = IN_F64_CONST; pic->arg.d = pval->d;
      break;
    default: assert(false);
  }
}

/* push instruction to pdata */
static void asm_pushbk(icbuf_t *pdata, inscode_t *pic)
{
  *icbnewbk(pdata) = *pic;
}

/* fetch last instruction from pdata into pic */
static void asm_getbk(icbuf_t *pdata, inscode_t *pic)
{
  size_t n = icblen(pdata); assert(n > 0);
  *pic = *icbref(pdata, n-1);
}

/* pop last instruction from pdata into pic */
static void asm_popbk(icbuf_t *pdata, inscode_t *pic)
{
  size_t n = icblen(pdata); assert(n > 0);
  if (pic) *pic = *icbref(pdata, n-1);
  icbpopbk(pdata); 
}


/* acode node predicates */

static node_t *acode_type(node_t *pcn)
{
  node_t *ptn; assert(pcn);
  if (pcn->nt != NT_ACODE) neprintf(pcn, "compile: asm code expected");
  assert(ndlen(pcn) > 0);
  ptn = ndref(pcn, 0); assert(ptn->nt == NT_TYPE);
  return ptn;
}

/* check if code is simple and side-effect-free */
static bool acode_simple_noeff(node_t *pan)
{
  if (pan->nt == NT_ACODE && ndlen(pan) == 1 && icblen(&pan->data) == 1) {
    instr_t in = icbref(&pan->data, 0)->in;
    if (in >= IN_I32_CONST && in <= IN_F64_CONST) return true;
    if (in == IN_GLOBAL_GET || in == IN_LOCAL_GET) return true;
    if (in >= IN_I32_LOAD && in <= IN_I64_LOAD32_U) return true;
  }  
  return false;
}

/* check if this node is a const; returns 0 or ts_t/nv */
static ts_t acode_const(node_t *pan, numval_t **ppnv)
{
  if (pan->nt == NT_ACODE && ndlen(pan) == 1 && icblen(&pan->data) == 1) {
    inscode_t *pic = icbref(&pan->data, 0); instr_t in = pic->in;
    switch (in) {
      case IN_I32_CONST: if (ppnv) *ppnv = &pic->arg; return TS_INT;
      case IN_I64_CONST: if (ppnv) *ppnv = &pic->arg; return TS_LLONG;
      case IN_F32_CONST: if (ppnv) *ppnv = &pic->arg; return TS_FLOAT;
      case IN_F64_CONST: if (ppnv) *ppnv = &pic->arg; return TS_DOUBLE;
    }
  }  
  return TS_VOID; /* 0 */
}

/* check if this node is a var get; returns 0 or instruction code */
static int acode_rval_get(node_t *pan)
{
  if (pan->nt == NT_ACODE && icblen(&pan->data) == 1) {
    instr_t in = icbref(&pan->data, 0)->in;
    return (in == IN_GLOBAL_GET || in == IN_LOCAL_GET) ? in : 0;
  } else return 0;
}

/* check if this node is memory load; returns 0 or instruction code */
static int acode_rval_load(node_t *pan)
{
  if (pan->nt == NT_ACODE && icblen(&pan->data) >= 1) {
    instr_t in = icbref(&pan->data, icblen(&pan->data)-1)->in;
    return (in >= IN_I32_LOAD && in <= IN_I64_LOAD32_U) ? in : 0;
  } else return 0;
}


/* compiler helpers; prn is original reference node for errors */

/* compile a cast expression (can produce empty code sequence) */
static node_t *compile_cast(node_t *prn, node_t *ptn, node_t *pan)
{
  node_t *pcn, *patn = acode_type(pan);
  if (ptn->ts == TS_VOID) ; /* ok, any value can be dropped */
  else if (!cast_compatible(ptn, patn))
    n2eprintf(ndref(prn, 1), prn, "compile: impossible cast");
  pcn = npnewcode(prn); ndcpy(ndnewbk(pcn), ptn);
  ndswap(ndnewbk(pcn), pan); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
  if (ptn->ts == TS_VOID) icbnewbk(&pcn->data)->in = IN_DROP;
  else asm_cast(ptn, patn, &pcn->data);
  return pcn;
}

/* compile a numerical literal expression */
static node_t *compile_numlit(node_t *prn, ts_t ts, numval_t *pnv)
{
  node_t *pcn = npnewcode(prn); ndsettype(ndnewbk(pcn), ts); 
  asm_numerical_constant(ts, pnv, &pcn->data);
  return pcn;
}

/* compile identifier reference (as rval); ptn is type node (copied) */
static node_t *compile_idref(node_t *prn, bool global, sym_t name, const node_t *ptn)
{
  inscode_t *pic; node_t *pcn = npnewcode(prn); ndpushbk(pcn, ptn);
  pic = icbnewbk(&pcn->data); pic->relkey = name;
  pic->in = global ? IN_GLOBAL_GET : IN_LOCAL_GET;
  return pcn;
}

/* compile regular binary operator expressions (+ - * / % << >> = != < <= > >=) */
static node_t *compile_binary(node_t *prn, node_t *pan1, tt_t op, node_t *pan2)
{
  node_t *patn1 = acode_type(pan1), *patn2 = acode_type(pan2); 
  ts_t ts1 = patn1->ts, ts2 = patn2->ts;
  if (ts_numerical(ts1) && ts_numerical(ts2)) {
    ts_t rts = ts_arith_common(ts1, ts2); 
    node_t rtn = mknd(), *prtn, *pcn = npnewcode(prn); ndsettype(&rtn, rts);
    if (rts != ts1) pan1 = compile_cast(prn, &rtn, pan1); /* fixme: constant fold oportunity */
    if (rts != ts2) pan2 = compile_cast(prn, &rtn, pan2); /* fixme: constant fold oportunity */
    prtn = ndcpy(ndnewbk(pcn), &rtn); if (op >= TT_LT && op <= TT_NE) prtn->ts = TS_INT;
    ndswap(ndnewbk(pcn), pan1); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
    ndswap(ndnewbk(pcn), pan2); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
    ndfini(&rtn);
    if (asm_binary(rts, op, &pcn->data)) return pcn;
  } else if (ts1 == TS_PTR && ts_numerical(ts2) && (op == TT_PLUS || op == TT_MINUS)) {
    size_t size, align; numval_t v; node_t *psn;
    measure_type(ndref(patn1, 0), prn, &size, &align, 0);
    if (size > 1) {
      node_t *pmn = compile_numlit(prn, TS_ULONG, (v.u = size, &v));
      node_t *pcn = compile_cast(prn, acode_type(pmn), pan1);
      node_t *pnn = compile_binary(prn, pan2, TT_STAR, pmn); /* fixme: constant fold oportunity */
      psn = compile_binary(prn, pcn, op, pnn);
    } else { /* byte pointer, no need to scale */
      node_t *pcn = compile_cast(prn, ndsettype(npalloc(), TS_ULONG), pan1);
      psn = compile_binary(prn, pcn, op, pan2); 
    }
    return compile_cast(prn, patn1, psn); 
  } else if (ts_numerical(ts1) && ts2 == TS_PTR && op == TT_PLUS) {
    return compile_binary(prn, pan2, op, pan1);
  } else if (ts1 == TS_PTR && ts2 == TS_PTR && op == TT_MINUS) {
    if (same_type(patn1, patn2)) {
      size_t size, align; numval_t v;
      measure_type(ndref(patn1, 0), prn, &size, &align, 0);
      if (size > 1) { 
        node_t *pmn = compile_numlit(prn, TS_LONG, (v.i = size, &v));
        node_t *pdn = compile_binary(prn, compile_cast(prn, acode_type(pmn), pan1), TT_MINUS, 
                                          compile_cast(prn, acode_type(pmn), pan2));
        return compile_binary(prn, pdn, TT_SLASH, pmn); 
      } else { /* byte pointer, no need to scale */
        node_t *ptn = ndsettype(npalloc(), TS_LONG);
        return compile_binary(prn, compile_cast(prn, ptn, pan1), TT_MINUS, 
                                   compile_cast(prn, ptn, pan2));
      }
    } 
    n2eprintf(ndref(prn, 0), prn, "incompatible pointer types for subtraction operation");
  } else if (ts1 == TS_PTR && ts2 == TS_PTR && op >= TT_LT && op <= TT_NE) {
    node_t tn = mknd(), *ptn = ndsettype(&tn, TS_ULONG), *pan;
    pan = compile_binary(prn, compile_cast(prn, ptn, pan1), op, compile_cast(prn, ptn, pan2));
    ptn->ts = TS_INT; pan = compile_cast(prn, ptn, pan);
    ndfini(&tn); return pan;
  } 
  n2eprintf(ndref(prn, 0), prn, "invalid types for binary %s operation", op_name(op));
  return NULL; /* won't happen */
}

/* compile regular unary operator expressions (! ~ + -) */
static node_t *compile_unary(node_t *prn, tt_t op, node_t *pan)
{
  node_t *pcn = npnewcode(prn), *patn = acode_type(pan); 
  ts_t ts = patn->ts; numval_t v;
  if (ts_numerical(ts)) {
    ts_t rts = ts_integral_promote(ts); 
    ndsettype(ndnewbk(pcn), (op == TT_NOT) ? TS_INT : rts);
    if (op == TT_MINUS && rts != TS_FLOAT && rts != TS_DOUBLE) {
      /* no NEG instructions for integers; -x => 0-x */
      asm_numerical_constant(rts, (v.i = 0, &v), &pcn->data); 
      ndswap(ndnewbk(pcn), pan); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
      if (rts != ts) asm_numerical_cast(rts, ts, &pcn->data);
      if (asm_binary(rts, TT_MINUS, &pcn->data)) return pcn;
    } else if (op == TT_TILDE && rts != TS_FLOAT && rts != TS_DOUBLE) {
      /* no INV instructions for integers; ~x => x^-1 */
      asm_numerical_constant(rts, (v.i = -1, &v), &pcn->data); 
      ndswap(ndnewbk(pcn), pan); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
      if (rts != ts) asm_numerical_cast(rts, ts, &pcn->data);
      if (asm_binary(rts, TT_XOR, &pcn->data)) return pcn;
    } else if (op == TT_NOT && (rts == TS_FLOAT || rts == TS_DOUBLE)) {
      /* no EQZ instructions for floats; !x => x==0.0 */
      if (rts == TS_FLOAT) v.f = 0.0; else v.d = 0.0;
      asm_numerical_constant(rts, &v, &pcn->data); 
      ndswap(ndnewbk(pcn), pan); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
      if (rts != ts) asm_numerical_cast(rts, ts, &pcn->data);
      if (asm_binary(rts, TT_EQ, &pcn->data)) return pcn;
    } else { /* regular case */
      ndswap(ndnewbk(pcn), pan); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
      if (rts != ts) asm_numerical_cast(rts, ts, &pcn->data);
      if (asm_unary(rts, op, &pcn->data)) return pcn;
    }
  }
  n2eprintf(ndref(prn, 0), prn, "invalid type for unary %s operation", op_name(op));
  return NULL; /* won't happen */
}

/* compile x to serve in a boolean test position */
node_t *compile_booltest(node_t *prn, node_t *pan)
{
  node_t *patn = acode_type(pan);
  if (patn->ts < TS_INT || patn->ts > TS_ULONG) {
    numval_t v; node_t *pzn = compile_numlit(prn, TS_INT, (v.i = 0, &v));
    return compile_binary(prn, pan, TT_NE, pzn);
  } else { /* good as-is */
    return pan;
  }
}

/* compile x ? y : z operator */
static node_t *compile_cond(node_t *prn, node_t *pan1, node_t *pan2, node_t *pan3)
{
  node_t *patn1 = acode_type(pan1), *patn2 = acode_type(pan2), *patn3 = acode_type(pan3); 
  node_t nt = mknd(), *pcn = npnewcode(prn), *pctn = NULL; 
  if (ts_numerical(patn2->ts) && ts_numerical(patn3->ts)) 
    pctn = ndsettype(&nt, ts_arith_common(patn2->ts, patn3->ts));
  else if (same_type(patn2, patn3)) pctn = patn2;
  else neprintf(prn, "incompatible types of ?: operator branches");
  pan1 = compile_booltest(prn, pan1); /* add !=0 if not i32 */
  ndcpy(ndnewbk(pcn), pctn);
  if (acode_simple_noeff(pan2) && acode_simple_noeff(pan3)) {
    /* both branches can be computed cheaply with no effects; use SELECT */
    ndswap(ndnewbk(pcn), pan1); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;  
    ndswap(ndnewbk(pcn), pan2); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;  
    ndswap(ndnewbk(pcn), pan3); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
    icbnewbk(&pcn->data)->in = IN_SELECT;
  } else { 
    /* fixme: 'if'-'else' construct is needed */
    inscode_t *pic; unsigned bt = ts_to_blocktype(pctn->ts); 
    assert(bt >= VT_F64 && bt <= VT_I32);
    ndswap(ndnewbk(pcn), pan1); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;  
    pic = icbnewbk(&pcn->data); pic->in = IN_IF; pic->arg.u = bt;
    ndswap(ndnewbk(pcn), pan2); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;  
    icbnewbk(&pcn->data)->in = IN_ELSE;
    ndswap(ndnewbk(pcn), pan3); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;  
    icbnewbk(&pcn->data)->in = IN_END;
  }
  ndfini(&nt); 
  return pcn;
}

/* compile non-reduceable *x{.f...} returning scalar value (load) or pointer (!load) */
static node_t *compile_deref(node_t *prn, node_t *pan, buf_t *psymb, bool load)
{
  node_t *patn = acode_type(pan), *petn = NULL; 
  size_t off = 0, i;
  if (patn->ts == TS_PTR) petn = ndref(patn, 0);
  else neprintf(prn, "cannot dereference non-pointer expression");
  if (psymb) for (i = 0; i < buflen(psymb); ++i) {
    sym_t fld = *(sym_t*)bufref(psymb, i);
    off += measure_offset(petn, prn, fld, &petn);
  }
  if (load) { /* code for scalar value */
    node_t *pcn = npnewcode(prn), *ptn = ndpushbk(pcn, petn);
    if (petn->ts != TS_PTR && !ts_numerical(petn->ts))
     neprintf(prn, "cannot dereference pointer to non-scalar expression");
    if (petn->ts != TS_PTR) ptn->ts = ts_integral_promote(petn->ts);
    ndswap(ndnewbk(pcn), pan); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
    asm_load(ptn->ts, petn->ts, off, &pcn->data);
    return pcn;
  } else {
    node_t *pcn = npnewcode(prn); 
    ndpushbk(ndsettype(ndnewbk(pcn), TS_PTR), petn);
    if (off > 0) {
      numval_t v; node_t *pon = compile_numlit(prn, TS_ULONG, (v.u = off, &v));
      pan = compile_binary(prn, compile_cast(prn, acode_type(pon), pan), TT_PLUS, pon);
    }
    ndswap(ndnewbk(pcn), pan); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
    return pcn;
  } 
}

/* compile ++x --x x++ x-- and assignments; ptn is cast type or NULL */
static node_t *compile_asncombo(node_t *prn, node_t *pan, node_t *pctn, tt_t op, node_t *pvn, bool post)
{
  /* convert pan in place and return it */
  inscode_t lic, sic, *pic;
  if (acode_rval_get(pan)) {
    /* pan is a single IN_GLOBAL_GET/IN_LOCAL_GET instruction */
    node_t *pdn = op ? npdup(pan) : NULL; /* needed for combo */
    if (post) asm_getbk(&pan->data, &lic); /* old val stays on stack */
    else asm_popbk(&pan->data, &lic); /* old val is no longer on stack */
    if (op) pvn = compile_binary(prn, pdn, op, pvn); /* new val on stack */
    if (!assign_compatible(acode_type(pan), pctn ? pctn : acode_type(pvn)))
      neprintf(prn, "can't modify lval: unexpected type");
    if (pctn && !cast_compatible(pctn, acode_type(pvn)))
      neprintf(prn, "can't modify lval: impossible narrowing cast");
    if (pctn && !same_type(pctn, acode_type(pvn))) 
      pvn = compile_cast(prn, pctn, pvn); 
    if (!same_type(acode_type(pan), acode_type(pvn))) 
      pvn = compile_cast(prn, acode_type(pan), pvn); 
    ndswap(ndnewbk(pan), pvn); icbnewbk(&pan->data)->in = IN_PLACEHOLDER;
    asm_instr_load_to_store(&lic, &sic);
    if (post) { /* new val removed, old val remains */
      asm_pushbk(&pan->data, &sic); 
    } else if (sic.in == IN_LOCAL_SET) { /* short way to keep new val */
      sic.in = IN_LOCAL_TEE;
      asm_pushbk(&pan->data, &sic); 
    } else { /* re-fetch to get new val */
      asm_pushbk(&pan->data, &sic);
      asm_pushbk(&pan->data, &lic);
    }
    return pan;    
  } else if (acode_rval_load(pan)) {
    /* pan code ends in one of IN_I32_LOAD..IN_I64_LOAD32_U instructions */
    sym_t pname = rpalloc(VT_I32); /* function-unique register name (wasm32) */
    node_t *ptn, *pdn;
    asm_popbk(&pan->data, &lic); /* now pointer is on stack, save it as p$ */
    pic = icbnewbk(&pan->data); pic->relkey = pname;
    if (post) pic->in = IN_LOCAL_TEE; /* ptr val stays on stack */
    else pic->in = IN_LOCAL_SET;  /* ptr val is no longer on stack */
    if (post) asm_pushbk(&pan->data, &lic); /* old val on stack */
    ptn = wrap_type_pointer(npdup(acode_type(pan))); 
    pdn = compile_idref(prn, false, pname, ptn);
    ndcpy(ndnewbk(pan), pdn); icbnewbk(&pan->data)->in = IN_PLACEHOLDER;
    if (op) {
      node_t *pln = npdup(pdn); 
      asm_pushbk(&pln->data, &lic); lift_arg0(acode_type(pln));
      pvn = compile_binary(prn, pln, op, pvn); /* new val on stack */
    }
    if (!assign_compatible(acode_type(pan), pctn ? pctn : acode_type(pvn)))
      neprintf(prn, "can't modify lval: unexpected type");
    if (pctn && !cast_compatible(pctn, acode_type(pvn)))
      neprintf(prn, "can't modify lval: impossible narrowing cast");
    if (pctn && !same_type(pctn, acode_type(pvn)))
      pvn = compile_cast(prn, pctn, pvn); 
    if (!same_type(acode_type(pan), acode_type(pvn))) 
      pvn = compile_cast(prn, acode_type(pan), pvn); 
    ndswap(ndnewbk(pan), pvn); icbnewbk(&pan->data)->in = IN_PLACEHOLDER;
    asm_instr_load_to_store(&lic, &sic);
    if (post) { /* new val removed, old val remains */
      asm_pushbk(&pan->data, &sic);
    } else { /* re-fetch to get new val */
      asm_pushbk(&pan->data, &sic);
      ndcpy(ndnewbk(pan), pdn); icbnewbk(&pan->data)->in = IN_PLACEHOLDER;
      asm_pushbk(&pan->data, &lic);
    }
    pic = icbnewfr(&pan->data); pic->in = IN_REGDECL; 
    pic->relkey = pname; pic->arg.u = VT_I32; /* wasm32 */ 
    return pan;    
  } else {
    neprintf(prn, "not a valid lvalue");
    return NULL;
  }
} 

/* compile call expression */
static node_t *compile_call(node_t *prn, node_t *pfn, buf_t *pab)
{
  node_t *pcn = npnewcode(prn), *pftn = acode_type(pfn); size_t i;
  inscode_t cic; 
  if (pftn->ts != TS_FUNCTION) 
    n2eprintf(ndref(prn, 0), prn, "can't call non-function type");
  if (ndlen(pftn) != buflen(pab)+1)
    n2eprintf(ndref(prn, 0), prn, "%d-parameter function called with %d arguments",
      (int)ndlen(pftn)-1, (int)buflen(pab));
  ndpushbk(pcn, ndref(pftn, 0));      
  if (acode_rval_get(pfn)) {
    inscode_t ic; asm_getbk(&pfn->data, &ic);
    if (ic.in != IN_GLOBAL_GET) 
      n2eprintf(ndref(prn, 0), prn, "not a function: %s", symname(ic.relkey));
    cic.in = IN_CALL; cic.relkey = ic.relkey;
  } else {
    n2eprintf(ndref(prn, 0), prn, "NYI: non-identifier functions are not yet supported");
  }
  for (i = 0; i < buflen(pab); ++i) {
    node_t **ppani = bufref(pab, i), *pani = *ppani;
    node_t *ptni = acode_type(pani), *pftni = ndref(pftn, i+1);
    if (pftni->nt == NT_VARDECL) pftni = ndref(pftni, 0);
    if (!assign_compatible(pftni, ptni))
      n2eprintf(pani, prn, "can't pass argument[%d]: unexpected type", i);
    if (!same_type(pftni, ptni)) pani = compile_cast(prn, pftni, pani);
    ndswap(ndnewbk(pcn), pani); icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
  }
  asm_pushbk(&pcn->data, &cic);
  return pcn;
}

/* compile expression -- that is, convert it to asm tree */
static node_t *expr_compile(node_t *pn, buf_t *prib, const node_t *ret)
{
  node_t *pcn = NULL; numval_t v;
  if (static_constant(pn)) {
    node_t nd = mknd(); 
    if (static_eval(pn, &nd)) ndswap(&nd, pn);
    ndfini(&nd);
  }
retry:
  switch (pn->nt) {
    case NT_LITERAL: {
      assert(ts_numerical(pn->ts));
      pcn = npnewcode(pn); ndsettype(ndnewbk(pcn), pn->ts);
      asm_numerical_constant(pn->ts, &pn->val, &pcn->data);
    } break; 
    case NT_IDENTIFIER: {
      bool bgl; const node_t *ptn = lookup_var_type(pn->name, prib, &bgl);
      if (!ptn) neprintf(pn, "compile: undefined identifier");
      pcn = compile_idref(pn, bgl, pn->name, ptn);
    } break;
    case NT_SUBSCRIPT: assert(false); break; /* dewasmified */
    case NT_CALL: {
      size_t i; buf_t apb = mkbuf(sizeof(node_t*));
      node_t *pfn = expr_compile(ndref(pn, 0), prib, ret);
      for (i = 1; i < ndlen(pn); ++i)
        *(node_t **)bufnewbk(&apb) = expr_compile(ndref(pn, i), prib, ret);
      pcn = compile_call(pn, pfn, &apb);
      buffini(&apb);
    } break;
    case NT_INTRCALL: {
      switch (pn->intr) {
        case INTR_ALLOCA: {
          if (ndlen(pn) == 1) {
            node_t *pan = expr_compile(ndref(pn, 0), prib, ret);
            node_t *ptni = acode_type(pan); inscode_t *pic; numval_t *pnv;
            ts_t ts = acode_const(pan, &pnv);
            if (ts == TS_INT) { /* sizeof?: calc frame size statically */
              pnv->i = (pnv->i + 15) & ~0xFLL;
              pic = icbnewfr(&pan->data); pic->in = IN_GLOBAL_GET; pic->relkey = intern("env_stack_pointer");
              pic = icbnewfr(&pan->data); pic->in = IN_GLOBAL_GET; pic->relkey = intern("env_stack_pointer");
              pic = icbnewbk(&pan->data); pic->in = IN_I32_SUB; 
              pic = icbnewbk(&pan->data); pic->in = IN_GLOBAL_SET; pic->relkey = intern("env_stack_pointer");
            } else { /* calc frame size dynamically */
              sym_t sname = rpalloc(VT_I32), pname = rpalloc(VT_I32); /* wasm32 */
              pic = icbnewfr(&pan->data); pic->in = IN_REGDECL; pic->relkey = sname; pic->arg.u = VT_I32;
              if (!ts_numerical(ptni->ts) || ts_arith_common(ptni->ts, TS_ULONG) != TS_ULONG)
                neprintf(pn, "invalid alloca() intrinsic's argument");
              if (ptni->ts != TS_ULONG) asm_numerical_cast(TS_ULONG, ptni->ts, &pan->data);
              pic = icbnewbk(&pan->data); pic->in = IN_I32_CONST; pic->arg.u = 15;  
              pic = icbnewbk(&pan->data); pic->in = IN_I32_ADD; 
              pic = icbnewbk(&pan->data); pic->in = IN_I32_CONST; pic->arg.u = 0xFFFFFFF0;
              pic = icbnewbk(&pan->data); pic->in = IN_I32_AND;   
              pic = icbnewbk(&pan->data); pic->in = IN_LOCAL_SET; pic->relkey = sname;
              pic = icbnewbk(&pan->data); pic->in = IN_GLOBAL_GET; pic->relkey = intern("env_stack_pointer");
              pic = icbnewbk(&pan->data); pic->in = IN_LOCAL_TEE; pic->relkey = pname;
              pic = icbnewbk(&pan->data); pic->in = IN_LOCAL_GET; pic->relkey = sname;
              pic = icbnewfr(&pan->data); pic->in = IN_REGDECL; pic->relkey = pname; pic->arg.u = VT_I32;
              pic = icbnewbk(&pan->data); pic->in = IN_I32_SUB;
              pic = icbnewbk(&pan->data); pic->in = IN_GLOBAL_SET; pic->relkey = intern("env_stack_pointer");
              pic = icbnewbk(&pan->data); pic->in = IN_LOCAL_GET; pic->relkey = pname;
            }
            wrap_type_pointer(ndsettype(acode_type(pan), TS_VOID));
            pcn = pan;
          } else {
            neprintf(pn, "alloca() intrinsic expects one argument");
          }
        } break;
        case INTR_FREEA: {
          if (ndlen(pn) == 1) {
            node_t *pan = expr_compile(ndref(pn, 0), prib, ret);
            node_t *ptni = acode_type(pan); inscode_t *pic;
            if (ptni->ts != TS_PTR)
              neprintf(pn, "invalid freea() intrinsic's argument");
            pic = icbnewbk(&pan->data); pic->in = IN_GLOBAL_SET; pic->relkey = intern("env_stack_pointer");
            ndsettype(acode_type(pan), TS_VOID);
            pcn = pan;
          } else {
            neprintf(pn, "freea() intrinsic expects one argument");
          }
        } break;
        case INTR_SASSERT: {
          check_static_assert(pn);
          pcn = ndsettype(npnewcode(pn), TS_VOID);
          icbnewbk(&pcn->data)->in = IN_NOP;
        } break;
      }
    } break;
    case NT_CAST: {
      pcn = compile_cast(pn, ndref(pn, 0), expr_compile(ndref(pn, 1), prib, ret));
    } break;
    case NT_POSTFIX: {
      switch (pn->op) {
        case TT_DOT: {
          node_t *pni = pn; buf_t fb; bufinit(&fb, sizeof(sym_t));
          while (pni->nt == NT_POSTFIX && pni->op == TT_DOT) {
            *(sym_t*)bufnewfr(&fb) = pni->name;
            pni = ndref(pni, 0); 
          }
          if (pni->nt == NT_PREFIX && pni->op == TT_STAR)
            pcn = compile_deref(pn, expr_compile(ndref(pni, 0), prib, ret), &fb, true);
          buffini(&fb);
        } break;
        case TT_PLUS_PLUS: case TT_MINUS_MINUS: {
          node_t *pn0 = ndref(pn, 0), *pln = (pn0->nt == NT_CAST) ? ndref(pn0, 1) : pn0;
          node_t *ptn = (pn0->nt == NT_CAST) ? ndref(pn0, 0) : NULL;
          node_t *pan = expr_compile(pln, prib, ret);
          numval_t v; tt_t op = pn->op == TT_PLUS_PLUS ? TT_PLUS : TT_MINUS;
          pcn = compile_asncombo(pn, pan, ptn, op, compile_numlit(pn, TS_INT, (v.i = 1, &v)), true); 
        } break;
        case TT_ARROW: /* TT_ARROW was dewasmified into TT_DOT */
        default: assert(false); 
      }    
    } break;
    case NT_PREFIX: {
      node_t *pn0 = ndref(pn, 0);
      switch (pn->op) {
        case TT_AND: {
          if (pn0->nt == NT_PREFIX && pn0->op == TT_STAR) {
            pn = ndref(pn0, 0); goto retry;
          } else {
            node_t *pni = pn0; buf_t fb; bufinit(&fb, sizeof(sym_t));
            while (pni->nt == NT_POSTFIX && pni->op == TT_DOT) {
              *(sym_t*)bufnewfr(&fb) = pni->name;
              pni = ndref(pni, 0); 
            }
            if (pni->nt == NT_PREFIX && pni->op == TT_STAR)
              pcn = compile_deref(pn, expr_compile(ndref(pni, 0), prib, ret), &fb, false);
            else neprintf(pn, "cannot get address of expression");
            buffini(&fb);
          }
        } break;
        case TT_STAR: {
          if (pn0->nt == NT_PREFIX && pn0->op == TT_AND) {
            pn = ndref(pn0, 0); goto retry;
          } else { /* nb: struct/union pointers handled in TT_DOT */
            node_t *pan = expr_compile(pn0, prib, ret); /* scalar ptr */
            pcn = compile_deref(pn, pan, NULL, true); /* no extra (sub)field offset */ 
          }
        } break;
        case TT_PLUS_PLUS: case TT_MINUS_MINUS: {
          node_t *pln = (pn0->nt == NT_CAST) ? ndref(pn0, 1) : pn0;
          node_t *ptn = (pn0->nt == NT_CAST) ? ndref(pn0, 0) : NULL;
          node_t *pan = expr_compile(pln, prib, ret);
          tt_t op = pn->op == TT_PLUS_PLUS ? TT_PLUS : TT_MINUS; 
          pcn = compile_asncombo(pn, pan, ptn, op, compile_numlit(pn, TS_INT, (v.i = 1, &v)), false); 
        } break;
        default: {
          node_t *pan = expr_compile(pn0, prib, ret);
          pcn = compile_unary(pn, pn->op, pan);
        } break;
      }    
    } break;
    case NT_INFIX: {
      node_t *pan1 = expr_compile(ndcref(pn, 0), prib, ret);
      node_t *pan2 = expr_compile(ndcref(pn, 1), prib, ret);
      switch (pn->op) {
        case TT_AND_AND: {
          pcn = compile_cond(pn, pan1, pan2, compile_numlit(pn, TS_INT, (v.i = 0, &v)));
        } break;
        case TT_OR_OR: {
          pcn = compile_cond(pn, pan1, compile_numlit(pn, TS_INT, (v.i = 1, &v)), pan2);
        } break;
        default: {
          pcn = compile_binary(pn, pan1, pn->op, pan2);
        } break;
      }
    } break;
    case NT_COND: {
      node_t *pan1 = expr_compile(ndcref(pn, 0), prib, ret);
      node_t *pan2 = expr_compile(ndcref(pn, 1), prib, ret);
      node_t *pan3 = expr_compile(ndcref(pn, 2), prib, ret);
      pcn = compile_cond(pn, pan1, pan2, pan3);
    } break;
    case NT_ASSIGN: {
      node_t *pn0 = ndref(pn, 0), *pln = (pn0->nt == NT_CAST) ? ndref(pn0, 1) : pn0;
      node_t *ptn = (pn0->nt == NT_CAST) ? ndref(pn0, 0) : NULL;
      node_t *pan = expr_compile(pln, prib, ret);
      node_t *pan2 = expr_compile(ndcref(pn, 1), prib, ret);
      tt_t op = (pn->op >= TT_PLUS_ASN && pn->op <= TT_SHR_ASN) ? pn->op - 1 : 0;  
      pcn = compile_asncombo(pn, pan, ptn, op, pan2, false); 
    } break;
    case NT_COMMA: {
      size_t i; pcn = npnewcode(pn); assert(ndlen(pn) > 0);
      for (i = 0; i < ndlen(pn); ++i) {
        node_t *pcni = expr_compile(ndref(pn, i), prib, ret);
        node_t *ptni = acode_type(pcni); bool drop = false;
        if (i == ndlen(pn) - 1) ndcpy(ndinsnew(pcn, 0), ptni);
        else drop = ptni->ts != TS_VOID;
        ndswap(pcni, ndnewbk(pcn));
        icbnewbk(&pcn->data)->in = IN_PLACEHOLDER;
        if (drop) icbnewbk(&pcn->data)->in = IN_DROP;
      }
    } break;
    case NT_ACODE: {
      pcn = pn;
    } break;
    case NT_TYPE: assert(false); break; /* inside cast only */  
    /* leave statement layer as-is for now (fixme!) */
    default: {
      /* for now, dup outside structure and compile nodes */
      size_t i;
      pcn = npnew(pn->nt, pn->pwsid, pn->startpos); 
      pcn->intr = pn->intr; pcn->op = pn->op;
      pcn->name = pn->name; /* shouldn't be there */
      for (i = 0; i < ndlen(pn); ++i) {
        node_t *pcni = expr_compile(ndref(pn, i), prib, ret);
        ndswap(pcni, ndnewbk(pcn));
      }
    } break;
  }
  if (!pcn) neprintf(pn, "failed to compile expression");
  //assert(pcn->nt == NT_ACODE);
  return pcn;  
}

/* compile function -- that is, convert it to asm tree */
static node_t *fundef_compile(node_t *pdn)
{
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1);
  buf_t rib = mkbuf(sizeof(ri_t)); node_t *ret; size_t i; 
  /* output is created anew from pieces of input and pool nodes */
  node_t *prn = npnew(NT_FUNDEF, pdn->pwsid, pdn->startpos), *prtn, *prbn; 
  prn->name = pdn->name; prn->sc = pdn->sc;
  ndnewbk(prn); ndnewbk(prn); prtn = ndref(prn, 0), prbn = ndref(prn, 1); 
  ndset(prtn, NT_TYPE, ptn->pwsid, ptn->startpos); prtn->ts = TS_FUNCTION;
  ndset(prbn, NT_BLOCK, pbn->pwsid, pbn->startpos);
  
  /* collect types for args and local vars; sort for bsearch */
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  for (i = 0; i < ndlen(ptn); ++i) {
    node_t *pdni = ndref(ptn, i), *ptni = pdni; sym_t name = 0;
    if (i == 0) { ret = pdni; assert(pdni->nt == NT_TYPE); } 
    if (pdni->nt == NT_VARDECL) name = ptni->name, ptni = ndref(pdni, 0); 
    if (name) { ri_t *pri = bufnewbk(&rib); pri->name = name; pri->ptn = ptni; }
    ndpushbk(prtn, pdni); /* as-is, entry already converted */
  }
  assert(pbn->nt == NT_BLOCK);
  for (i = 0; i < ndlen(pbn); ++i) {
    node_t *pdni = ndref(pbn, i), *ptni; sym_t name;
    if (pdni->nt != NT_VARDECL) break; /* statements may follow */
    name = pdni->name, ptni = ndref(pdni, 0); 
    if (name) { ri_t *pri = bufnewbk(&rib); pri->name = name; pri->ptn = ptni; }
    ndpushbk(prbn, pdni); /* as-is, entry already converted */
  }
  bufqsort(&rib, sym_cmp);

  /* go over body statements */
  for (/* use current i */; i < ndlen(pbn); ++i) {
    node_t *pni = ndref(pbn, i);
    node_t *pcn = expr_compile(pni, &rib, ret);
    ndswap(ndnewbk(prbn), pcn);
  }

  buffini(&rib);
  return prn;
}

/* process function definition in main module */
static void process_fundef(sym_t mainmod, node_t *pn, module_t *pm)
{
  node_t *pcn;
  assert(pn->nt == NT_FUNDEF && pn->name && ndlen(pn) == 2);
  assert(ndref(pn, 0)->nt == NT_TYPE && ndref(pn, 0)->ts == TS_FUNCTION);
  clear_regpool(); /* reset reg name generator */
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
  /* convert entry to wasm conventions, normalize a bit */
  fundef_wasmify(pn);
#ifdef _DEBUG
  fprintf(stderr, "fundef_wasmify ==>\n");
  dump_node(pn, stderr);
#endif  
#if 1
  /* compile to asm tree */
  pcn = fundef_compile(pn);
#ifdef _DEBUG
  fprintf(stderr, "fundef_compile ==>\n");
  dump_node(pcn, stderr);
#endif  
#endif
  /* free all nodes allocated with np-functions at once */
  clear_nodepool();
}

/* process top-level intrinsic call */
static void process_top_intrcall(node_t *pn, module_t *pm)
{
#ifdef _DEBUG
  dump_node(pn, stderr);
#endif
  if (pn->intr == INTR_SASSERT) check_static_assert(pn);
  else neprintf(pn, "unexpected top-level form");
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
    else if (pn->nt == NT_INTRCALL) process_top_intrcall(pn, pm); 
    else if (pn->nt != NT_BLOCK) neprintf(pn, "invalid top-level declaration");
    else if (!pn->name) neprintf(pn, "unscoped top-level declaration");
    else for (i = 0; i < ndlen(pn); ++i) {
      node_t *pni = ndref(pn, i);
      if (pni->nt == NT_ASSIGN) {
        neprintf(pni, "initialization in header");
      } else if (pni->nt == NT_VARDECL && pni->sc != SC_EXTERN) {
        neprintf(pni, "non-extern declaration in header");
      } else if (pni->nt == NT_VARDECL && pni->name && ndlen(pni) == 1) {
        node_t *ptn = ndref(pni, 0); assert(ptn->nt == NT_TYPE);
        if (ptn->ts == TS_FUNCTION) fundef_check_type(ptn);
        post_vardecl(pn->name, pni);
      } else {
        neprintf(pni, "extern declaration expected");
      }
    }
  } else {
    /* in module mainmod: produce code */
    size_t i;
    if (pn->nt == NT_FUNDEF) process_fundef(mainmod, pn, pm);
    else if (pn->nt == NT_INTRCALL) process_top_intrcall(pn, pm); 
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
      *vtbnewbk(pvtb) = VT_I32; break;  
    case TS_LONG: case TS_ULONG:  
    case TS_PTR: case TS_ARRAY:
      /* fixme: should depend on wasm32/wasm64 model */
      *vtbnewbk(pvtb) = VT_I32; break;  
    case TS_LLONG: case TS_ULLONG:  
      *vtbnewbk(pvtb) = VT_I64; break;
    case TS_FLOAT: /* legal in wasm, no auto promotion to double */
      *vtbnewbk(pvtb) = VT_F32; break;
    case TS_DOUBLE:
      *vtbnewbk(pvtb) = VT_F64; break;
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

