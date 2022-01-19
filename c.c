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

/* g_dsmap element */
typedef struct dsmelt_tag {
  chbuf_t cb;
  size_t addr;
} dsmelt_t;

/* initialization */
void init_compiler(const char *larg, const char *lenv)
{
  bufinit(&g_bases, sizeof(sym_t));
  *(sym_t*)bufnewbk(&g_bases) = intern(""); 
  if (larg) *(sym_t*)bufnewbk(&g_bases) = intern(larg); 
  if (lenv) *(sym_t*)bufnewbk(&g_bases) = intern(lenv);
  chbinit(&g_dseg); bufresize(&g_dseg, 16); /* reserve first 16 bytes */
  bufinit(&g_dsmap, sizeof(dsmelt_t));
  init_workspaces();
  init_symbols();
}

void fini_compiler(void)
{
  dsmelt_t *pde; size_t i;
  buffini(&g_bases);
  chbfini(&g_dseg);
  for (pde = (dsmelt_t*)(g_dsmap.buf), i = 0; i < g_dsmap.fill; ++i) chbfini(&pde[i].cb);
  buffini(&g_dsmap);
  fini_workspaces();
  fini_symbols();
}

/* data segment operations */

/* never returns 0 (first 16 bytes are reserved) */
size_t dseg_intern(size_t align, chbuf_t *pcb)
{
  dsmelt_t *pe = bufbsearch(&g_dsmap, pcb, chbuf_cmp);
  if (pe) {
    return pe->addr;
  } else {
    size_t addr = chblen(&g_dseg), n = addr % align;
    if (n > 0) bufresize(&g_dseg, (addr = addr+(align-n)));
    chbcat(&g_dseg, pcb);
    pe = bufnewbk(&g_dsmap); 
    chbicpy(&pe->cb, pcb); pe->addr = addr; 
    bufqsort(&g_dsmap, chbuf_cmp);
    return addr;
  } 
}



/* variable info */
typedef struct vi_tag {
  sc_t sc;      /* SC_REGISTER or SC_AUTO only */
  node_t *ptn;  /* pointer to type node (stored elsewhere) */
  unsigned rno; /* REGISTER: LOCAL_GET/LOCAL_SET offset */
} vi_t;

/* environment */

/* environment (frame) data type */ 
typedef struct env_tag {
  dsbuf_t frame;
  void *pdata; /* pass-specific */ 
  struct env_tag *up; 
} env_t;

static env_t mkenv(env_t *up)
{
  env_t e; 
  memset(&e, 0, sizeof(env_t)); 
  dsbinit(&e.frame);
  e.up = up;
  return e;
}

static void envfini(env_t *penv)
{
  assert(penv);
  dsbfini(&penv->frame);
}

static void envpushbk(env_t *penv, const char *v)
{
  dsbpushbk(&penv->frame, (dstr_t*)&v); /* allocated at back */
}

static bool envlookup(env_t *penv, const char *v, size_t *pdepth)
{
  size_t depth = 0;
  if (!v) return false;
  while (penv) {
    size_t i;
    for (i = 0; i < dsblen(&penv->frame); ++i) {
      dstr_t *pds = dsbref(&penv->frame, i);
      if (streql(*pds, v)) {
        if (pdepth) *pdepth = depth + i;
        return true;
      }
    }
    depth += i;
    penv = penv->up;
  }
  return false;
}

/* type predicates and operations */

static bool ts_spointer(ts_t ts) 
{
  return TS_PTR < ts && ts <= TS_SPTR_MAX;
}

static bool ts_numerical(ts_t ts) 
{
  return TS_CHAR <= ts && ts <= TS_DOUBLE;
}

static bool ts_ptrdiff(ts_t ts) 
{
  return TS_CHAR <= ts && ts <= TS_ULONG; /* can be TS_ULLONG in wasm32? */
}

static bool ts_difforsptr(ts_t ts) 
{
  return ts_spointer(ts) || ts_ptrdiff(ts);
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

/* unary operation with a sized pointer */
ts_t spval_unop(tt_t op, ts_t tx, numval_t vx, numval_t *pvz)
{
  assert(ts_spointer(tx));
  switch (op) {
    case TT_PLUS:  pvz->u = vx.u; return tx;
    case TT_NOT:   pvz->i = vx.u == 0; return TS_INT;
    default:       return TS_VOID;
  }
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

/* binary operation between two pointer/ptrdiff arguments (at least one is a pointer) */
ts_t dspval_binop(tt_t op, ts_t tx, numval_t vx, ts_t ty, numval_t vy, numval_t *pvz)
{
  ts_t tz = TS_VOID;
  if (op == TT_PLUS && ts_spointer(tx) && ts_ptrdiff(ty)) {
    numval_convert(TS_LONG, ty, &vy);
    tz = tx; pvz->u = vx.u + vy.i * (tx-TS_PTR); 
  } else if (op == TT_PLUS && ts_ptrdiff(tx) && ts_spointer(ty)) {
    numval_convert(TS_LONG, tx, &vx);
    tz = ty; pvz->u = vy.u + vx.i * (ty-TS_PTR); 
  } else if (op == TT_MINUS && ts_spointer(tx) && ts_ptrdiff(ty)) {
    numval_convert(TS_LONG, ty, &vy);
    tz = tx; pvz->u = vx.u - vy.i * (tx-TS_PTR); 
  } else if (op == TT_MINUS && ts_spointer(tx) && ts_spointer(ty) && tx == ty) {
    tz = TS_LONG; pvz->i = (vx.u - vy.u)/(tx-TS_PTR); 
  } else if (ts_spointer(tx) && ts_spointer(ty) && tx == ty) {
    unsigned long long x = vx.u, y = vy.u;
    switch (op) {
      case TT_EQ:    pvz->i = x == y; tz = TS_INT; break;
      case TT_NE:    pvz->i = x != y; tz = TS_INT; break;
      case TT_LT:    pvz->i = x < y;  tz = TS_INT; break;
      case TT_GT:    pvz->i = x > y;  tz = TS_INT; break;
      case TT_LE:    pvz->i = x <= y; tz = TS_INT; break;
      case TT_GE:    pvz->i = x >= y; tz = TS_INT; break;
      default:       tz = TS_VOID; break;
    }
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

/* get ts from type node; measure the pointers to return TS_SPTR... */
static ts_t type_extended_ts(node_t *ptn)
{
  assert(ptn && ptn->nt == NT_TYPE);
  if (ptn->ts == TS_PTR) {
    size_t size = 0, align = 0; 
    assert(ndlen(ptn) == 1);
    measure_type(ndref(ptn, 0), ptn, &size, &align, 0);
    if (size > 0 && size <= TS_SPTR_MAX-TS_PTR)
      return (ts_t)(TS_PTR+size);
  }
  return ptn->ts;
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

/* evaluate pn expression statically, putting result into prn (VERY limited) */
bool static_eval(node_t *pn, node_t *prn)
{
  /* for now, deal with ints only */
  switch (pn->nt) {
    case NT_LITERAL: {
      if (ts_numerical(pn->ts)) {
        ndcpy(prn, pn);
        return true;
      } else if (pn->ts == TS_STRING || pn->ts == TS_LSTRING) {
        size_t eltsz = (pn->ts == TS_STRING ? 1 : 4), align = eltsz;
        ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
        prn->ts = TS_PTR + eltsz; /* 'sized' pointer */
        prn->val.u = dseg_intern(align, &pn->data);
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
        } else if (ts_spointer(nx.ts)) {
          numval_t vz; ts_t tz = spval_unop(pn->op, nx.ts, nx.val, &vz);
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
        } else if (ts_difforsptr(nx.ts) && ts_difforsptr(ny.ts)) {
          numval_t vz; ts_t tz = dspval_binop(pn->op, nx.ts, nx.val, ny.ts, ny.val, &vz);
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
        if (ok && !ts_numerical(nr.ts) && !ts_spointer(nr.ts)) ok = false;
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
      tc = type_extended_ts(ndref(pn, 0)); 
      if ((ts_numerical(tc) || ts_spointer(tc)) && (static_eval)(ndref(pn, 1), &nx)) {
        if (ts_numerical(tc) && ts_numerical(nx.ts)) {
          numval_t vc = nx.val; numval_convert(tc, nx.ts, &vc);
          ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
          prn->ts = tc; prn->val = vc; ok = true;
        } else if (ts_spointer(tc) && ts_numerical(nx.ts)) {
          numval_t vc = nx.val; numval_convert(TS_ULONG, nx.ts, &vc);
          ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
          prn->ts = tc; prn->val = vc; ok = true;
        } else if (ts_numerical(tc) && ts_spointer(nx.ts)) {
          numval_t vc = nx.val; numval_convert(tc, TS_ULONG, &vc);
          ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
          prn->ts = tc; prn->val = vc; ok = true;
        } else if (ts_spointer(tc) && ts_spointer(nx.ts)) {
          prn->ts = tc; prn->val = nx.val; ok = true;
        }
      }
      ndfini(&nx);
      return ok;
    } break;
  }
  return false;
}

/* expression compiler; returns type node (either made in ptn or taken from some other place)
 * compilation adds instructions to code buffer pcb; instructions should grow stack by exactly
 * n positions (1 or 0 -- use drop) */
static node_t *compile_expr(const node_t *pn, env_t *penv, icbuf_t *pcb, size_t retc, node_t *ptn)
{
  switch (pn->nt) {
    case NT_LITERAL: {
    } break; 
    case NT_IDENTIFIER: {
    } break; 
    case NT_SUBSCRIPT: {
    } break; 
    case NT_CALL: {
    } break; 
    case NT_INTRCALL: {
    } break; 
    case NT_CAST: {
    } break; 
    case NT_PREFIX: {
    } break; 
    case NT_INFIX: {
    } break; 
    case NT_COND: {
    } break; 
    case NT_ASSIGN: {
    } break; 
    case NT_COMMA: {
    } break;
    default: {
      //neprintf(pn, "can't compile as an expression");
    } break; 
  }
  return NULL; /* won't happen */
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

/* check if this function type is legal in wasm */
static void check_ftnd(node_t *ptn)
{
  size_t i;
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  for (i = 0; i < ndlen(ptn); ++i) {
    node_t *ptni = ndref(ptn, i); 
    assert(ptni->nt == NT_VARDECL && ndlen(ptni) == 1 || ptni->nt == NT_TYPE);
    if (ptni->nt == NT_VARDECL) ptni = ndref(ptni, 0); assert(ptni->nt == NT_TYPE);
    /* todo: allow sizeable structures/unions as immediate args/return values;
     * they will be passed as pointers (extra arg for return value) */
    switch (ptni->ts) {
      case TS_VOID: /* ok if alone */ 
        if (i == 0) break; /* return type can be void */
        neprintf(ptn, "unexpected void function argument type");
      case TS_INT: case TS_UINT: case TS_ENUM:
      case TS_LONG: case TS_ULONG:  
      case TS_PTR: case TS_ARRAY:
      case TS_LLONG: case TS_ULLONG:  
      case TS_FLOAT: case TS_DOUBLE:
        break;
      default:
        neprintf(ptn, "function arguments of this type are not supported");
    }
  }
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

/* process function definition in main module */
static void process_fundef(sym_t mainmod, node_t *pn, module_t *pm)
{
  assert(pn->nt == NT_FUNDEF && pn->name && ndlen(pn) == 2);
  assert(ndref(pn, 0)->nt == NT_TYPE && ndref(pn, 0)->ts == TS_FUNCTION);
#ifdef _DEBUG
  dump_node(pn, stderr);
#endif  
  check_ftnd(ndref(pn, 0));
  { /* post appropriate vardecl for later use */
    node_t nd = mknd();
    ndset(&nd, NT_VARDECL, pn->pwsid, pn->startpos);
    nd.name = pn->name; ndcpy(ndnewbk(&nd), ndref(pn, 0)); 
    post_vardecl(mainmod, &nd); 
    ndfini(&nd); 
  }
  { /* todo: compile to function */
    //unsigned fno = entblen(&pm->funcdefs);
    /* ... */
  }
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

/* compile module file */
void compile_module(const char *fname)
{
  module_t m; size_t i; sym_t mod;
  size_t fino = 0, gino = 0; /* insertion indices */
  funcsig_t fs;
  modinit(&m); fsinit(&fs);
  mod = process_module(fname, &m); assert(mod);
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
  //emit_module(&mod);
  /* done */
  fsfini(&fs);
  modfini(&m); 
}


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

int main(int argc, char **argv)
{
  if (argc == 1 || argc == 3 && streql(argv[1], "-L")) {
    const char *larg = argc >= 3 ? argv[2] : NULL; 
    const char *lenv = getenv("WCPL_LIBRARY_PATH");
    init_compiler(larg, lenv);
    compile_module("-");
    fini_compiler();
  } else if (argc == 2) {
    freopen(argv[1], "wb", stdout);
    emit_test_module();
  } else {
    exprintf("missing output file argument");
  }
  return EXIT_SUCCESS;
}

