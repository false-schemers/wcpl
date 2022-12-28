/* c.c (wcpl compiler) -- esl */

#include <stdbool.h>
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

/* wcpl globals */
long    g_optlvl;   /* -O arg */
buf_t  *g_ibases;   /* module include search bases */
buf_t  *g_lbases;   /* module object module search bases */
fsbuf_t g_funcsigs; /* unique function signatures */
sym_t   g_crt_mod;  /* C runtime module */
sym_t   g_wasi_mod; /* WASI module */
sym_t   g_lm_id;    /* id for linear memory */
sym_t   g_sp_id;    /* id for stack pointer global */
sym_t   g_sb_id;    /* id for stack base global */
sym_t   g_hb_id;    /* id for heap base global */
size_t  g_sdbaddr;  /* static data allocation start */
size_t  g_stacksz;  /* stack size in bytes */
size_t  g_argvbsz;  /* argv buf size in bytes */


void init_wcpl(dsbuf_t *pincv, dsbuf_t *plibv, long optlvl, size_t sarg, size_t aarg)
{
  size_t i;
  g_optlvl = optlvl; 
  g_ibases = newbuf(sizeof(sym_t));
  for (i = 0; i < dsblen(pincv); ++i) {
    dstr_t *pds = dsbref(pincv, i);
    *(sym_t*)bufnewbk(g_ibases) = intern(*pds);
  }
  g_lbases = newbuf(sizeof(sym_t));
  for (i = 0; i < dsblen(plibv); ++i) {
    dstr_t *pds = dsbref(plibv, i);
    int sepchar = strsuf(*pds, "\\") ? '\\' : '/';
    *(sym_t*)bufnewbk(g_lbases) = intern(*pds);
    *(sym_t*)bufnewbk(g_ibases) = internf("%sinclude%c", *pds, sepchar);
  }
  fsbinit(&g_funcsigs);
  g_wasi_mod = intern("wasi_snapshot_preview1");
  g_crt_mod = intern("crt");
  g_lm_id = intern("memory");
  g_sp_id = intern("sp$");
  g_sb_id = intern("stack_base");
  g_hb_id = intern("heap_base");
  g_sdbaddr = 1024; /* >0, 16-aligned: address 0 reserved for NULL */
  g_stacksz = sarg; /* 64K default */
  g_argvbsz = aarg; /* 4K default */
} 

void fini_wcpl(void)
{
  freebuf(g_ibases);
  freebuf(g_lbases);
  fsbfini(&g_funcsigs);
  g_wasi_mod = g_crt_mod = 0;
  g_lm_id = g_sp_id = 0;
}


/* compiler globals */
static sym_t g_curmod; 
static wat_module_t *g_curpwm;

void init_compiler(void)
{
  init_workspaces();
  init_nodepool();
  init_regpool();
  init_symbols();
  g_curmod = 0;
  g_curpwm = NULL;
}

void fini_compiler(void)
{
  fini_workspaces();
  fini_nodepool();
  fini_regpool();
  fini_symbols();
  g_curmod = 0;
  g_curpwm = NULL;
}


/* convert WCPL to a WASM type or VT_UNKN */
static valtype_t ts2vt(ts_t ts)
{
  switch (ts) {
    case TS_BOOL:
    case TS_CHAR:  case TS_UCHAR:
    case TS_SHORT: case TS_USHORT:
    case TS_INT:   case TS_UINT:
    case TS_ENUM:
      return VT_I32;
    case TS_LONG:  case TS_ULONG:  
    case TS_PTR: 
      /* depends on wasm32/wasm64 model */
      return VT_I32;
    case TS_LLONG: case TS_ULLONG:  
      return VT_I64;
    case TS_FLOAT: 
      return VT_F32;
    case TS_DOUBLE:
      return VT_F64; 
    default:; 
  }
  return VT_UNKN;
}


/* type predicates and operations */

static bool ts_numerical(ts_t ts) 
{
  return TS_BOOL <= ts && ts <= TS_DOUBLE;
}

static bool ts_numerical_or_enum(ts_t ts) 
{
  return TS_BOOL <= ts && ts <= TS_DOUBLE || ts == TS_ENUM;
}

static bool ts_bulk(ts_t ts) 
{
  return ts == TS_ARRAY || ts == TS_STRUCT || ts == TS_UNION;
}

static bool ts_unsigned(ts_t ts) 
{
  switch (ts) {
    case TS_BOOL:   case TS_UCHAR: 
    case TS_USHORT: case TS_UINT:
    case TS_ULONG:  case TS_ULLONG: 
      return true;
    default:; 
  }
  return false;
}

static valtype_t ts_to_blocktype(ts_t ts)
{
  switch (ts) {
    case TS_VOID:                   return BT_VOID;
    case TS_LONG:  case TS_ULONG:   /* fall thru (assumes wasm32) */
    case TS_PTR:                    /* fall thru (assumes wasm32) */
    case TS_ENUM:
    case TS_INT:   case TS_UINT:    return VT_I32;
    case TS_LLONG: case TS_ULLONG:  return VT_I64;
    case TS_FLOAT:                  return VT_F32;
    case TS_DOUBLE:                 return VT_F64;
    default:; 
  }
  return VT_UNKN;
}

/* returns TS_INT/TS_UINT/TS_LLONG/TS_ULLONG/TS_FLOAT/TS_DOUBLE */
static ts_t ts_integral_promote(ts_t ts)
{
  switch (ts) {
    case TS_BOOL:
    case TS_CHAR:  case TS_UCHAR: 
    case TS_SHORT: case TS_USHORT:
    case TS_INT:   case TS_ENUM:   
      return TS_INT;
    case TS_UINT:  
    case TS_LONG:  case TS_ULONG:
    case TS_LLONG: case TS_ULLONG:  
    case TS_FLOAT: case TS_DOUBLE: 
      return ts;
    default: assert(false);
  }
  return ts; /* shouldn't happen */
}

/* common arith type for ts1 and ts2 (can be promoted one) */
static ts_t ts_arith_common(ts_t ts1, ts_t ts2)
{
  assert(ts_numerical_or_enum(ts1) && ts_numerical_or_enum(ts2));
  if (ts1 == TS_DOUBLE || ts2 == TS_DOUBLE) return TS_DOUBLE;
  if (ts1 == TS_FLOAT  || ts2 == TS_FLOAT)  return TS_FLOAT;
  ts1 = ts_integral_promote(ts1), ts2 = ts_integral_promote(ts2);
  if (ts1 == ts2) return ts1;
  if (ts1 == TS_ULLONG || ts2 == TS_ULLONG) return TS_ULLONG;
  if (ts1 == TS_LLONG  || ts2 == TS_LLONG)  return TS_LLONG;
  if (ts1 == TS_ULONG  || ts2 == TS_ULONG)  return TS_ULONG;
  if (ts1 == TS_LONG   || ts2 == TS_LONG)   return TS_LONG;
  if (ts1 == TS_UINT   || ts2 == TS_UINT)   return TS_UINT;
  return TS_INT;
}

/* ts2 can be promoted up to ts1 with implicit cast */
static bool ts_arith_assign_compatible(ts_t ts1, ts_t ts2)
{
  assert(ts_numerical_or_enum(ts1) && ts_numerical_or_enum(ts2));
  if (ts1 == ts2) return true;
  return ts_arith_common(ts1, ts2) == ts1;
}


static unsigned long long integral_mask(ts_t ts, unsigned long long u)
{
  switch (ts) {
    case TS_CHAR: case TS_UCHAR:    
      return u & 0x00000000000000FFull; 
    case TS_SHORT: case TS_USHORT:  
      return u & 0x000000000000FFFFull; 
    case TS_INT: case TS_UINT: case TS_ENUM:                  
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

static long long integral_sext(ts_t ts, long long i)
{
  switch (ts) {
    case TS_CHAR: case TS_UCHAR:
      return (long long)((((unsigned long long)i & 0x00000000000000FFull) 
                        ^ 0x0000000000000080ull) - 0x0000000000000080ull); 
    case TS_SHORT: case TS_USHORT:
      return (long long)((((unsigned long long)i & 0x000000000000FFFFull) 
                        ^ 0x0000000000008000ull) - 0x0000000000008000ull); 
    case TS_INT: case TS_UINT: case TS_ENUM:                  
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

static void numval_convert(ts_t tsto, ts_t tsfrom, numval_t *pnv)
{
  assert(pnv);
  assert(ts_numerical_or_enum(tsto) && ts_numerical_or_enum(tsfrom));
  if (tsto == tsfrom) return; /* no change */
  else if (tsto == TS_DOUBLE) {
    if (tsfrom == TS_FLOAT) pnv->d = (double)pnv->f;
    else if (ts_unsigned(tsfrom)) pnv->d = (double)pnv->u;
    else pnv->d = (double)pnv->i;
  } else if (tsto == TS_FLOAT) {
    if (tsfrom == TS_DOUBLE) pnv->f = (float)pnv->d;
    else if (ts_unsigned(tsfrom)) pnv->f = (float)pnv->u;
    else pnv->f = (float)pnv->i;
  } else if (tsto == TS_BOOL) {
    if (tsfrom == TS_DOUBLE) pnv->u = (pnv->d != 0);
    else if (tsfrom == TS_FLOAT) pnv->u = (pnv->f != 0);
    else if (ts_unsigned(tsfrom)) pnv->u = (pnv->u != 0);
    else pnv->u = (pnv->i != 0);
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

static bool numval_eq(ts_t ts, numval_t *pnv1, numval_t *pnv2)
{
  switch (ts) {
    case TS_DOUBLE:
      return pnv1->d == pnv2->d;
    case TS_FLOAT:
      return pnv1->f == pnv2->f;
    case TS_BOOL:   case TS_UCHAR: 
    case TS_USHORT: case TS_UINT:
    case TS_ULONG:  case TS_ULLONG: 
      return pnv1->u == pnv2->u;
    default:;
  }    
  return pnv1->i == pnv2->i;
} 

/* ts/pnv can be used to init ts1 while preserving value */
static bool ts_arith_init_compatible(ts_t ts1, ts_t ts, numval_t *pnv)
{
  assert(ts_numerical_or_enum(ts1) && ts_numerical_or_enum(ts));
  if (ts1 != ts) { 
    numval_t nv = *pnv;
    numval_convert(ts1, ts, &nv);
    numval_convert(ts, ts1, &nv);  
    return numval_eq(ts, &nv, pnv); /* round-trip preserves value */
  }  
  return true;
}

/* ts/pnv can be used up to init pointer type */
static bool ts_init_ptr_compatible(ts_t ts, numval_t *pnv)
{
  if (ts == TS_PTR) return true;
  if (ts == TS_INT && pnv->i == 0) return true; /* wasm32 */
  return false;
}

/* unary operation with a number */
static ts_t numval_unop(tt_t op, ts_t tx, const numval_t *pvx, numval_t *pvz)
{
  ts_t tz = ts_integral_promote(tx);
  numval_t vx = *pvx; numval_convert(tz, tx, &vx);
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
    case TS_ULLONG: case TS_ULONG: case TS_UINT: {
      unsigned long long x = vx.u;
      switch (op) {
        case TT_PLUS:  pvz->u = integral_mask(tz, x); break;
        case TT_MINUS: pvz->u = integral_mask(tz, (unsigned long long)-(long long)x); break;
        case TT_TILDE: pvz->u = integral_mask(tz, ~x); break;
        case TT_NOT:   pvz->i = x == 0; tz = TS_INT; break;
        default:       tz = TS_VOID; break;
      }
    } break;
    case TS_LLONG: case TS_LONG: case TS_INT: {
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
static ts_t numval_binop(tt_t op, ts_t tx, const numval_t *pvx, ts_t ty, const numval_t *pvy, numval_t *pvz)
{
  ts_t tz = ts_arith_common(tx, ty);
  numval_t vx = *pvx, vy = *pvy; 
  numval_convert(tz, tx, &vx); numval_convert(tz, ty, &vy);
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
    case TS_ULLONG: case TS_ULONG: case TS_UINT: {
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
    case TS_LLONG: case TS_LONG: case TS_INT: {
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

/* produce numerical constant instruction from literal */
static void asm_numerical_const(ts_t ts, numval_t *pval, inscode_t *pic)
{
  switch (ts) {
    case TS_BOOL:
      pic->in = IN_I32_CONST; pic->arg.u = !!pval->u;
      break;
    case TS_CHAR:  case TS_UCHAR:
    case TS_SHORT: case TS_USHORT:
    case TS_INT:   case TS_UINT:   case TS_ENUM:
    case TS_LONG:  case TS_ULONG: /* wasm32 */
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


/* 'register' (wasm local var) info for typecheck */
typedef struct ri {
  sym_t name; /* register or global var name */
  const node_t *ptn; /* NT_TYPE (not owned) */
} ri_t;

/* look up var in prib (sorted by name; can be NULL) or globally */
static const node_t *lookup_var_type(sym_t name, buf_t *prib, sym_t *pmod)
{
  ri_t *pri; const node_t *pgn;
  if (prib != NULL && (pri = bufbsearch(prib, &name, &sym_cmp)) != NULL) {
    *pmod = 0; /* local symbol */
    return pri->ptn;
  } else if ((pgn = lookup_global(name)) != NULL) {
    if (pgn->nt == NT_IMPORT && ndlen(pgn) == 1) {
      *pmod = pgn->name; /* global symbol's module name */
      assert(*pmod);
      /* mark this import as actually referenced! */
      mark_global_referenced(pgn);
      return ndcref(pgn, 0);
    }
  }
  return NULL;
}

/* limited type eval for const expressions; returns NULL on failures */
static const node_t *type_eval(node_t *pn, buf_t *prib)
{
  switch (pn->nt) {
    case NT_LITERAL: {
      if (ts_numerical_or_enum(pn->ts)) {
        return ndsettype(npalloc(), pn->ts);
      } else if (pn->ts == TS_STRING || pn->ts == TS_LSTRING) {
        ts_t ets = (pn->ts == TS_STRING) ? TS_CHAR : TS_INT;
        return wrap_type_pointer(ndsettype(npalloc(), ets));
      }
    } break;
    case NT_IDENTIFIER: {
      sym_t mod = 0; 
      return lookup_var_type(pn->name, prib, &mod);
    } break;
    case NT_SUBSCRIPT: {
      if (ndlen(pn) == 2) {
        const node_t *ptn = type_eval(ndref(pn, 0), prib);
        if (ptn && (ptn->ts == TS_ARRAY || ptn->ts == TS_PTR)) {
          if (ndlen(ptn) > 0) return ndcref(ptn, 0); 
        }
      }
    } break;
    case NT_POSTFIX: {
      if (pn->op == TT_DOT && pn->name && ndlen(pn) == 1) {
        const node_t *ptn = type_eval(ndref(pn, 0), prib);
        if (ptn && (ptn->ts == TS_STRUCT || ptn->ts == TS_UNION)) {
          size_t i;
          if (ptn->name && !ndlen(ptn)) {
            node_t *pftn = (node_t *)lookup_eus_type(ptn->ts, ptn->name);
            if (!pftn) n2eprintf(ptn, pn, "can't allocate/measure incomplete union type");
            else ptn = pftn;
          }
          for (i = 0; i < ndlen(ptn); ++i) {
            const node_t *pni = ndcref(ptn, i);
            if (pni->nt == NT_VARDECL && pni->name == pn->name) {
              return ndcref(pni, 0);
            }
          }
        }
      } else if (pn->op == TT_ARROW && pn->name && ndlen(pn) == 1) {
        const node_t *ptn = type_eval(ndref(pn, 0), prib);
        if (ptn && ptn->ts == TS_PTR && ndlen(ptn) == 1) {
          ptn = ndcref(ptn, 0); 
          if (ptn->ts == TS_STRUCT || ptn->ts == TS_UNION) {
            size_t i;
            if (ptn->name && !ndlen(ptn)) {
              node_t *pftn = (node_t *)lookup_eus_type(ptn->ts, ptn->name);
              if (!pftn) n2eprintf(ptn, pn, "can't allocate/measure incomplete union type");
              else ptn = pftn;
            }
            for (i = 0; i < ndlen(ptn); ++i) {
              const node_t *pni = ndcref(ptn, i);
              if (pni->nt == NT_VARDECL && pni->name == pn->name) {
                return ndcref(pni, 0);
              }
            }
          }
        }
      }
    } break;
    case NT_PREFIX: {
      if (pn->op == TT_STAR && ndlen(pn) == 1) {
        const node_t *ptn = type_eval(ndref(pn, 0), prib);
        if (ptn && ptn->ts == TS_PTR && ndlen(ptn) == 1) {
          if (ndlen(ptn) == 1) return ndcref(ptn, 0); 
        }
      }
    } break;
    default:
      break;
  }
  return NULL;
}

/* calc size/align for ptn; prn is NULL or reference node for errors */
void measure_type(const node_t *ptn, node_t *prn, size_t *psize, size_t *palign, int lvl)
{
  assert(ptn && ptn->nt == NT_TYPE);
  if (lvl > 100) n2eprintf(ptn, prn, "type too large (possible infinite nesting)");
  *psize = *palign = 0;
  switch (ptn->ts) {
    case TS_VOID: 
      n2eprintf(ptn, prn, "can't allocate/measure void type");
      break;
    case TS_ETC: 
      n2eprintf(ptn, prn, "can't allocate data for ... yet");
      break;
    case TS_BOOL:
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
        if (!pftn) n2eprintf(ptn, prn, "can't allocate/measure incomplete union type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        const node_t *pni = ndcref(ptn, i);
        if (pni->nt == NT_VARDECL && ndlen(pni) == 1) pni = ndcref(pni, 0);
        measure_type(pni, prn, &size, &align, lvl+1);
        if (size > *psize) *psize = size;         
        if (align > *palign) *palign = align;         
      }
      if (!*psize || !*palign) 
        n2eprintf(ptn, prn, "can't allocate/measure empty or incomplete union");
      /* round size up so it is multiple of align */
      if ((n = *psize % *palign) > 0) *psize += *palign - n; 
    } break;
    case TS_STRUCT: {
      size_t size, align, i, n;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(TS_STRUCT, ptn->name);
        if (!pftn) n2eprintf(ptn, prn, "can't allocate/measure incomplete struct type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        const node_t *pni = ndcref(ptn, i);
        if (pni->nt == NT_VARDECL && ndlen(pni) == 1) pni = ndcref(pni, 0);
        measure_type(pni, prn, &size, &align, lvl+1);
        /* round size up so it is multiple of pni align */
        if ((n = *psize % align) > 0) *psize += align - n; 
        *psize += size; if (align > *palign) *palign = align;         
      }
      if (!*psize || !*palign) 
        n2eprintf(ptn, prn, "can't allocate/measure empty or incomplete struct");
      /* round size up so it is multiple of align */
      if ((n = *psize % *palign) > 0) *psize += *palign - n; 
    } break;
    case TS_ARRAY: {
      if (ndlen(ptn) == 2) {
        const node_t *petn = ndcref(ptn, 0), *pcn = ndcref(ptn, 1); 
        int count = 0;
        /* fixme: eval may need to see local vars, so prib is needed */
        if (!arithmetic_eval_to_int((node_t*)pcn, NULL, &count) || count <= 0)
          n2eprintf(pcn, prn, "can't allocate/measure data for arrays of nonconstant/nonpositive size");
        measure_type(petn, prn, psize, palign, lvl+1);
        *psize *= count;
      } else {
        n2eprintf(ptn, prn, "can't allocate/measure data for arrays of unspecified size");
      }
    } break;
    case TS_FUNCTION:
      n2eprintf(ptn, prn, "can't allocate/measure data for function");
      break;
    default:
      assert(false);
  }
  if (!*psize || !*palign) 
    n2eprintf(ptn, prn, "can't allocate/measure type");
}

/* calc offset for ptn.fld; prn is NULL or reference node for errors */
size_t measure_offset(const node_t *ptn, node_t *prn, sym_t fld, node_t **ppftn)
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
        const node_t *pni = ndcref(ptn, i);
        if (pni->nt == NT_VARDECL && pni->name == fld) {
          if (ppftn) *ppftn = (node_t*)ndcref(pni, 0);
          return 0;
        }
      }
      n2eprintf(ptn, prn, "can't find field .%s in union", symname(fld));
    } break;
    case TS_STRUCT: {
      size_t size, align, i, n, cursize = 0;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(TS_STRUCT, ptn->name);
        if (!pftn) n2eprintf(ptn, prn, "can't measure incomplete struct type");
        else ptn = pftn;
      }
      for (i = 0; i < ndlen(ptn); ++i) {
        const node_t *pni = ndcref(ptn, i); sym_t s = 0;
        if (pni->nt == NT_VARDECL && ndlen(pni) == 1) s = pni->name, pni = ndcref(pni, 0);
        measure_type(pni, prn, &size, &align, 0);
        /* round size up so it is multiple of pni align */
        if ((n = cursize % align) > 0) cursize += align - n;
        if (s == fld) {
          if (ppftn) *ppftn = (node_t*)pni;
          return cursize;
        }
        cursize += size;
      }
      n2eprintf(ptn, prn, "can't find field .%s in struct", symname(fld));
    } break;
    default:; 
  }
  n2eprintf(ptn, prn, "can't find field .%s in type", symname(fld));
  return 0; /* won't happen */
}

/* intern the string literal, return fresh data id */
static sym_t intern_strlit(node_t *pn)
{
  if (!pn->name) { /* first attempt */
    size_t n = watieblen(&g_curpwm->exports);
    watie_t *pd = watiebnewbk(&g_curpwm->exports, IEK_DATA);
    assert(pn->ts == TS_STRING || pn->ts == TS_LSTRING);
    pd->mod = g_curmod;
    pd->id = internf("ds%d$", (int)n);
    chbcpy(&pd->data, &pn->data); 
    pd->align = (pn->ts == TS_LSTRING) ? 4 /* wchar_t */ : 1 /* char */;
    pd->mut = MT_CONST;
    pn->name = pd->id;
  }
  return pn->name;
}


/* static eval value */
typedef struct seval {
  ts_t ts;      /* TS_PTR or ts_numerical */
  numval_t val; /* ts_numerical */
  sym_t id;     /* TS_PTR (can be 0 if val is 0) */
} seval_t; 

/* evaluate pn expression statically, putting result into pr */
bool static_eval(node_t *pn, buf_t *prib, seval_t *pr)
{
  switch (pn->nt) {
    case NT_LITERAL: {
      if (ts_numerical(pn->ts)) {
        pr->ts = pn->ts;
        pr->val = pn->val;
        pr->id = 0;
        return true;
      } else if (pn->ts == TS_STRING || pn->ts == TS_LSTRING) {
        sym_t id = pn->name;
        if (!id) {
          node_t vn = mknd();
          ts_t ets = (pn->ts == TS_STRING) ? TS_CHAR : TS_INT;
          id = intern_strlit(pn); /* sets pn->name */
          ndset(&vn, NT_VARDECL, pn->pwsid, pn->startpos);
          vn.name = id;
          wrap_type_pointer(ndsettype(ndnewbk(&vn), ets));
          /* post id so initialize_bulk_data can find it */
          post_symbol(g_curmod, &vn, true/*final*/, true/*hide*/);
          ndfini(&vn);
        }
        pr->ts = TS_PTR;
        pr->id = id;
        pr->val.i = 0; /* no offset */
        return true;
      }
    } break;
    case NT_INTRCALL: {
      if (pn->intr == INTR_SIZEOF || pn->intr == INTR_ALIGNOF) {
        const node_t *ptn = NULL;
        size_t size, align; assert(ndlen(pn) == 1);
        if (ndref(pn, 0)->nt == NT_TYPE) ptn = ndref(pn, 0);
        else ptn = type_eval(ndref(pn, 0), prib);
        if (!ptn) return false; /* can't get type */
        measure_type(ptn, pn, &size, &align, 0);
        pr->ts = TS_INT; 
        if (pn->intr == INTR_SIZEOF) pr->val.i = (int)size;
        else pr->val.i = (int)align;
        return true;
      } else if (pn->intr == INTR_OFFSETOF) {
        const node_t *ptn = NULL;
        size_t offset; assert(ndlen(pn) == 2 && ndref(pn, 1)->nt == NT_IDENTIFIER);
        if (ndref(pn, 0)->nt == NT_TYPE) ptn = ndref(pn, 0);
        else ptn = type_eval(ndref(pn, 0), prib);
        if (!ptn) return false; /* can't get type */
        offset = measure_offset(ptn, pn, ndref(pn, 1)->name, NULL);
        pr->ts = TS_INT; 
        pr->val.i = (int)offset;
        return true;
      } else if (pn->intr == INTR_ASU32 || pn->intr == INTR_ASFLT) {
        ts_t tc = pn->intr == INTR_ASU32 ? TS_FLOAT : TS_UINT; 
        seval_t rx; bool ok = false; assert(ndlen(pn) == 1);
        if (static_eval(ndref(pn, 0), prib, &rx) && rx.ts == tc) {
          union { unsigned u; float f; } uf;
          if (pn->intr == INTR_ASU32) { uf.f = rx.val.f; pr->ts = TS_UINT; pr->val.u = uf.u; }
          else { uf.u = (unsigned)rx.val.u; pr->ts = TS_FLOAT; pr->val.f = uf.f; }
          ok = true;
        }
        return ok;
      } else if (pn->intr == INTR_ASU64 || pn->intr == INTR_ASDBL) {
        ts_t tc = pn->intr == INTR_ASU64 ? TS_DOUBLE : TS_ULLONG; 
        seval_t rx; bool ok = false; assert(ndlen(pn) == 1);
        if (static_eval(ndref(pn, 0), prib, &rx) && rx.ts == tc) {
          pr->ts = (pn->intr == INTR_ASU64) ? TS_ULLONG : TS_DOUBLE;
          pr->val = rx.val; ok = true;
        }
        return ok;
      }
    } break;
    case NT_PREFIX: {
      seval_t rx; node_t *psn; bool ok = false; 
      assert(ndlen(pn) == 1);
      if (pn->op == TT_AND && ndref(pn, 0)->nt == NT_IDENTIFIER) {
        pr->ts = TS_PTR; pr->val.i = 0; pr->id = ndref(pn, 0)->name;
        ok = true;
      } else if (pn->op == TT_AND && (psn = ndref(pn, 0))->nt == NT_SUBSCRIPT
        && ndref(psn, 0)->nt == NT_IDENTIFIER) {
        assert(ndlen(psn) == 2);
        if (static_eval(ndref(psn, 1), prib, &rx) && rx.ts == TS_INT) {
          pr->ts = TS_PTR; pr->val = rx.val; pr->id = ndref(psn, 0)->name;
          ok = true;
        }
      } else if (static_eval(ndref(pn, 0), prib, &rx)) {
        if (ts_numerical(rx.ts)) {
          numval_t vz; ts_t tz = numval_unop(pn->op, rx.ts, &rx.val, &vz);
          if (tz != TS_VOID) { pr->ts = tz; pr->val = vz; ok = true; }
        } else if (rx.ts == TS_PTR && pn->op == TT_NOT) {
          pr->ts = TS_INT; pr->val.i = 0; ok = true; /* dseg ptr != NULL */
        }
      }
      return ok;
    } break;
    case NT_INFIX: {
      seval_t rx, ry; bool ok = false;
      assert(ndlen(pn) == 2); 
      if (static_eval(ndref(pn, 0), prib, &rx) && static_eval(ndref(pn, 1), prib, &ry)) {
        if (ts_numerical(rx.ts) && ts_numerical(ry.ts)) {
          numval_t vz; ts_t tz = numval_binop(pn->op, rx.ts, &rx.val, ry.ts, &ry.val, &vz);
          if (tz != TS_VOID) { pr->ts = tz; pr->val = vz; ok = true; }
        } else if (rx.ts == TS_PTR && ry.ts == TS_INT && (pn->op == TT_MINUS || pn->op == TT_PLUS)) {
          rx.val.i = (pn->op == TT_MINUS) ? rx.val.i - ry.val.i : rx.val.i + ry.val.i;
          *pr = rx; ok = true;
        } else if (rx.ts == TS_INT && ry.ts == TS_PTR && pn->op == TT_PLUS) {
          ry.val.i = ry.val.i + rx.val.i;
          *pr = ry; ok = true;
        } else if (rx.ts == TS_PTR && ry.ts == TS_PTR && rx.id == ry.id) {
          pr->ts = TS_INT; ok = true;
          switch (pn->op) { 
            case TT_MINUS: pr->val.i = rx.val.i - ry.val.i; break;
            case TT_EQ:    pr->val.i = rx.val.i == ry.val.i; break;
            case TT_NE:    pr->val.i = rx.val.i != ry.val.i; break;
            case TT_LT:    pr->val.i = rx.val.i < ry.val.i; break;
            case TT_GT:    pr->val.i = rx.val.i > ry.val.i; break;
            case TT_LE:    pr->val.i = rx.val.i <= ry.val.i; break;
            case TT_GE:    pr->val.i = rx.val.i >= ry.val.i; break;
            default:       ok = false; break;
          }
        }
      }
      return ok;
    } break;
    case NT_COND: {
      seval_t r; bool ok = false; int cond;
      assert(ndlen(pn) == 3);
      if (arithmetic_eval_to_int(ndref(pn, 0), prib, &cond)) {
        if (cond) ok = static_eval(ndref(pn, 1), prib, &r);
        else ok = static_eval(ndref(pn, 2), prib, &r);
        if (ok) *pr = r; /* can be num or ptr! */
      } 
      return ok;
    } break;
    case NT_CAST: {
      seval_t rx; ts_t tc; bool ok = false;
      assert(ndlen(pn) == 2); assert(ndref(pn, 0)->nt == NT_TYPE);
      tc = ndref(pn, 0)->ts; 
      if (ts_numerical(tc) && static_eval(ndref(pn, 1), prib, &rx) && ts_numerical(rx.ts)) {
        numval_t vc = rx.val; numval_convert(tc, rx.ts, &vc);
        pr->ts = tc; pr->val = vc; ok = true;
      } else if (tc == TS_PTR && ndlen(ndref(pn, 0)) == 1 && ndref(ndref(pn, 0), 0)->ts == TS_VOID) {
        int ri; /* see if this is NULL pointer, i.e. (void*)0 */ 
        if (arithmetic_eval_to_int(ndref(pn, 1), prib, &ri) && ri == 0) {
          pr->ts = TS_PTR; pr->val.i = 0; pr->id = 0; ok = true;
        }
      } 
      return ok;
    } break;
    default:; 
  }
  return false;
}

/* evaluate integer expression pn statically, putting result into pi */
bool arithmetic_eval_to_int(node_t *pn, buf_t *prib, int *pri)
{
  seval_t r; bool ok = false;
  if (static_eval(pn, prib, &r) && ts_numerical(r.ts)) {
    numval_convert(TS_INT, r.ts, &r.val);
    *pri = (int)r.val.i;
    ok = true;
  }
  return ok;
}

/* evaluate pn expression statically, putting result into prn (numbers only) */
bool arithmetic_eval(node_t *pn, buf_t *prib, node_t *prn)
{
  seval_t r; bool ok = false;
  if (static_eval(pn, prib, &r) && ts_numerical(r.ts)) {
    ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
    prn->ts = r.ts; prn->val = r.val; 
    ok = true;
  }
  return ok;
}

static void check_static_assert(node_t *pn, buf_t *prib)
{
  if (pn->intr == INTR_SASSERT) {
    node_t *pen, *psn; int res;
    assert(ndlen(pn) == 1 || ndlen(pn) == 2);
    pen = ndref(pn, 0), psn = ndlen(pn) == 2 ? ndref(pn, 1) : NULL;
    if (psn && (psn->nt != NT_LITERAL || psn->ts != TS_STRING))
      neprintf(pn, "unexpected 2nd arg of static_assert (string literal expected)");
    if (!arithmetic_eval_to_int(pen, prib, &res))
      n2eprintf(pen, pn, "unexpected lhs of static_assert comparison (static test expected)");
    if (res == 0) {
      if (!psn) neprintf(pn, "static_assert failed");
      else neprintf(pn, "static_assert failed (%s)", chbdata(&psn->data));
    }
  }
}


/* post variable declaration for later use */
static void post_vardecl(sym_t mod, node_t *pn, bool final, bool hide)
{
  const node_t *pin;
  assert(pn->nt == NT_VARDECL && pn->name && ndlen(pn) == 1);
  assert(ndref(pn, 0)->nt == NT_TYPE);
  /* post in symbol table under pn->name as NT_IMPORT with modname and type;
   * set its sc to final?(hide?AUTO:REGISTER):NONE
   * NONE is to be changed to EXTERN when actually referenced from module */
  pin = post_symbol(mod, pn, final, hide);
  if (getverbosity() > 0) {
    fprintf(stderr, "%s final=%d, hide=%d =>\n", symname(pn->name), (int)final, (int)hide);
    dump_node(pin, stderr);
  }
}

/* check if this global variable type is legal in WCPL; evals array sizes */
static node_t *vardef_check_type(node_t *ptn)
{
  assert(ptn->nt == NT_TYPE);
  switch (ptn->ts) {
    case TS_ENUM: /* ok, treated as int */
    case TS_BOOL: /* ok but widened to int */
    case TS_CHAR: case TS_UCHAR:   /* ok but widened to int */
    case TS_SHORT: case TS_USHORT: /* ok but widened to int */
    case TS_INT: case TS_UINT: 
    case TS_LONG: case TS_ULONG:  
    case TS_LLONG: case TS_ULLONG:  
    case TS_FLOAT: case TS_DOUBLE:
    case TS_PTR: 
      break;
    case TS_ARRAY: {
      node_t *pcn; assert(ndlen(ptn) == 2);
      pcn = ndref(ptn, 1);
      if (pcn->nt == NT_NULL) {
        neprintf(ptn, "not supported: flexible array as global variable type");
      } else if (pcn->nt != NT_LITERAL || pcn->ts != TS_INT) {
        int count = 0;
        if (!arithmetic_eval_to_int(pcn, NULL, &count) || count <= 0)
          n2eprintf(pcn, ptn, "array size should be positive constant");
        ndset(pcn, NT_LITERAL, pcn->pwsid, pcn->startpos);
        pcn->ts = TS_INT; pcn->val.i = count;
      }
    } break;
    case TS_STRUCT:
    case TS_UNION:
      break;
    default:
      neprintf(ptn, "global variables of this type are not supported");
  }
  return ptn;
}

/* check if this function type is legal in WCPL */
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
        neprintf(ptni, "unexpected void function argument type");
      case TS_ENUM: /* ok, treated as int */
      case TS_BOOL: /* ok but widened to int */
      case TS_CHAR: case TS_UCHAR:   /* ok but widened to int */
      case TS_SHORT: case TS_USHORT: /* ok but widened to int */
      case TS_INT: case TS_UINT: 
      case TS_LONG: case TS_ULONG:  
      case TS_LLONG: case TS_ULLONG:  
      case TS_FLOAT: case TS_DOUBLE:
      case TS_PTR: 
        break;
      case TS_ARRAY: {
        node_t *pcn; assert(ndlen(ptni) == 2);
        pcn = ndref(ptni, 1);
        if (pcn->nt == NT_NULL) { /* t foo[] == t* foo */
          ndbrem(&ptni->body, 1); ptni->ts = TS_PTR;
        } /* else neprintf(ptni, "not supported: array as function parameter/return type"); */
      } break;
      case TS_STRUCT:
        if (i > 0) neprintf(ptni, "not supported: struct as function parameter type");
        break;
      case TS_UNION:
        if (i > 0) neprintf(ptni, "not supported: union as function parameter type");
        break;
      case TS_ETC:
        if (i+1 != ndlen(ptn)) neprintf(ptni, "nonfinal ... in function parameter list");
        break; 
      default:
        neprintf(ptni, "function arguments of this type are not supported: %s", ts_name(ptni->ts));
    }
  }
}

/* initialize preallocated bulk data with display initializer */
static watie_t *initialize_bulk_data(size_t pdidx, watie_t *pd, size_t off, node_t *ptn, node_t *pdn)
{
  if (ts_bulk(ptn->ts) && pdn->nt == NT_CAST) {
    /* supported if cast has the same type */
    assert(ndlen(pdn) == 2 && ndref(pdn, 0)->nt == NT_TYPE);
    if (same_type(ptn, ndref(pdn, 0))) return initialize_bulk_data(pdidx, pd, off, ptn, ndref(pdn, 1));
    else neprintf(pdn, "initializer cast is not compatible with lval type");
  } if (ts_bulk(ptn->ts) && pdn->nt == NT_DISPLAY) {
    /* element-by-element initialization */
    if (ndref(pdn, 0)->ts != TS_VOID && !same_type(ptn, vardef_check_type(ndref(pdn, 0))))
      neprintf(pdn, "initializer has incompatible type");
    /* start at off and go from element to element updating off */
    if (ptn->ts == TS_ARRAY) {
      size_t j, asize, size, align, offi; node_t *petn, *pecn;
      assert(ndlen(ptn) == 2);
      measure_type(ptn, ptn, &asize, &align, 0);
      petn = ndref(ptn, 0), pecn = ndref(ptn, 1);
      measure_type(petn, ptn, &size, &align, 0);
      for (offi = 0, j = 1; j < ndlen(pdn); ++j) {
        if (offi >= asize) n2eprintf(ndref(pdn, j), pdn, "too many initializers for array");
        else pd = initialize_bulk_data(pdidx, pd, off+offi, petn, ndref(pdn, j));
        offi += size;
      }
      /* if (offi < asize) nwprintf(pdn, "warning: too few initializers for array"); */
    } else { /* struct or union */
      size_t i, j, offi;
      if (ptn->name && !ndlen(ptn)) {
        node_t *pftn = (node_t *)lookup_eus_type(ptn->ts, ptn->name);
        if (!pftn) n2eprintf(ptn, pdn, "can't initialize incomplete type");
        else ptn = pftn;
      }
      for (i = 0, j = 1; i < ndlen(ptn); ++i, ++j) {
        node_t *ptni = ndref(ptn, i), *pfni;
        assert(ptni->nt == NT_VARDECL && ndlen(ptni) == 1);
        offi = measure_offset(ptn, ptni, ptni->name, &pfni);
        if (j < ndlen(pdn)) {
          pd = initialize_bulk_data(pdidx, pd, off+offi, pfni, ndref(pdn, j));
        } else {
          neprintf(pdn, "initializer is too short");
        }
        if (ptn->ts == TS_UNION) break; /* first member only! */
      }
      if (j != ndlen(pdn)) 
        neprintf(pdn, "initializer is too long");
    }
  } else if (ptn->ts == TS_ARRAY && pdn->nt == NT_LITERAL) {
    node_t *petn, *pecn; size_t asize, size, align, acnt, icnt;
    assert(ndlen(ptn) == 2);
    measure_type(ptn, ptn, &asize, &align, 0);
    petn = ndref(ptn, 0), pecn = ndref(ptn, 1);
    measure_type(petn, ptn, &size, &align, 0);
    if (size == 1 && pdn->ts == TS_STRING) icnt = buflen(&pdn->data); 
    else if (size == 4 && pdn->ts == TS_LSTRING) icnt = buflen(&pdn->data) / 4;
    else neprintf(pdn, "unexpected array type for literal string initializer");
    assert(icnt >= 1); /* literal's data always ends in zero character */
    acnt = asize/size;
    if (icnt-1 == acnt) --icnt; /* legal init: no final \0 */
    else if (icnt > acnt) neprintf(pdn, "too many characters for array");
    memcpy(chbdata(&pd->data) + off, chbdata(&pdn->data), icnt*size);
  } else if (ts_numerical_or_enum(ptn->ts)) {
    seval_t r; buf_t cb = mkchb();
    if (!static_eval(pdn, NULL, &r) || !ts_numerical(r.ts)) 
      neprintf(pdn, "non-numerical-constant initializer");
    if (ts_arith_assign_compatible(ptn->ts, r.ts) || /* r.ts can be promoted up (no value check) */
        ts_arith_init_compatible(ptn->ts, r.ts, &r.val)) { /* r.ts can be converted saving value */
      if (ptn->ts != r.ts) { numval_convert(ptn->ts, r.ts, &r.val); r.ts = ptn->ts; }
    } else neprintf(pdn, "constant initializer cannot be implicitly converted to expected type");
    pd = watiebref(&g_curpwm->exports, pdidx); /* re-fetch after static_eval */
    switch (r.ts) {
      case TS_CHAR:   binchar((int)r.val.i, &cb); break;
      case TS_BOOL:   /* as uchar, takes 1 byte of storage */
      case TS_UCHAR:  binuchar((unsigned)r.val.u, &cb); break;
      case TS_SHORT:  binshort((int)r.val.i, &cb); break;
      case TS_USHORT: binushort((unsigned)r.val.u, &cb); break;
      case TS_LONG:   /* wasm32 */
      case TS_ENUM:   /* same as int */
      case TS_INT:    binint((int)r.val.i, &cb); break;
      case TS_ULONG:  /* wasm32 */
      case TS_UINT:   binuint((unsigned)r.val.u, &cb); break;
      case TS_LLONG:  binllong(r.val.i, &cb); break;
      case TS_ULLONG: binullong(r.val.u, &cb); break;
      case TS_FLOAT:  binfloat(r.val.f, &cb); break;
      case TS_DOUBLE: bindouble(r.val.d, &cb); break; 
      default: assert(false);
    }
    memcpy(chbdata(&pd->data) + off, chbdata(&cb), chblen(&cb));
    chbfini(&cb);
  } else if (ptn->ts == TS_PTR) {
    seval_t r;
    if (!static_eval(pdn, NULL, &r) || !ts_init_ptr_compatible(r.ts, &r.val)) {
      neprintf(pdn, "constant address initializer expected");
    }
    pd = watiebref(&g_curpwm->exports, pdidx); /* re-fetch after static_eval */
    if (!r.id) { /* NULL */
      buf_t cb = mkchb();
      if (r.val.i != 0) neprintf(pdn, "NULL initializer expected");
      binint(0, &cb); /* wasm32 */
      memcpy(chbdata(&pd->data) + off, chbdata(&cb), chblen(&cb));
      chbfini(&cb);
    } else {
      const node_t *pitn, *pin = lookup_global(r.id);
      if (!pin) neprintf(pdn, "unknown name in address initializer");
      assert(pin->nt == NT_IMPORT && ndlen(pin) == 1 && ndcref(pin, 0)->nt == NT_TYPE);
      pitn = ndcref(pin, 0);
      if (ts_bulk(pitn->ts) || pitn->ts == TS_PTR) {
        inscode_t *pic = icbnewbk(&pd->code);
        pic->in = IN_REF_DATA; pic->id = r.id; pic->arg2.mod = pin->name;
        pic->arg.i = r.val.i; /* 0 or numerical offset in 'elements' */
        if (r.val.i != 0) { /* have to use scale factor to get byte offset */
          /* got to make sure that static_eval allows things like &intarray[4] */
          neprintf(pdn, "NYI: offset in address initializer");
        }
        pic = icbnewbk(&pd->code);
        pic->in = IN_DATA_PUT_REF;
        pic->arg.u = off;      
      } else {
        neprintf(pdn, "address initializer should be a reference to a nonscalar type");
      }
    }
  } else {
    assert(false);
    neprintf(pdn, "valid initializer expected");
  }
  return pd;
}

/* process var declaration in main module */
static void process_vardecl(sym_t mmod, node_t *pdn, node_t *pin)
{
  bool final, hide;
  node_t *ptn;
  if (getverbosity() > 0) {
    dump_node(pdn, stderr);
    if (pin) dump_node(pin, stderr);
  }
  /* check variable type for legality */
  assert(pdn->nt == NT_VARDECL); assert(ndlen(pdn) == 1);
  ptn = ndref(pdn, 0); assert(ptn->nt == NT_TYPE);
  if (ptn->ts == TS_FUNCTION) fundef_check_type(ptn); /* param: x[] => *x */
  else vardef_check_type(ptn); /* evals array sizes */
  /* post it for later use */
  final = true; hide = (pdn->sc == SC_STATIC);
  /* if it is just function declared, allow definition to come later */
  if (ptn->ts == TS_FUNCTION) final = false;
  post_vardecl(mmod, pdn, final, hide); /* final */
  /* compile to global */
  if (final) { 
    assert(ptn->ts != TS_FUNCTION);
    if (ts_bulk(ptn->ts)) { /* compiles to a var data segment */
      size_t size, align;
      size_t pdidx = watieblen(&g_curpwm->exports);
      watie_t *pd = watiebnewbk(&g_curpwm->exports, IEK_DATA);
      pd->mod = mmod; pd->id = pdn->name;
      pd->exported = !hide;
      measure_type(ptn, pdn, &size, &align, 0);
      bufresize(&pd->data, size); /* fills with zeroes */
      pd->align = (int)align;
      if (pin) {
        assert(pin->nt == NT_ASSIGN && ndlen(pin) == 2);
        assert(ndref(pin, 0)->nt == NT_IDENTIFIER && ndref(pin, 0)->name == pdn->name);
        pd = initialize_bulk_data(pdidx, pd, 0, ptn, ndref(pin, 1));
      }
      pd->mut = MT_VAR;
    } else { /* compiles to a global */
      size_t pgidx = watieblen(&g_curpwm->exports);
      watie_t *pg = watiebnewbk(&g_curpwm->exports, IEK_GLOBAL);
      pg->mod = mmod; pg->id = pdn->name;
      pg->exported = !hide;
      if (pin) {
        seval_t r; assert(pin->nt == NT_ASSIGN && ndlen(pin) == 2);
        assert(ndref(pin, 0)->nt == NT_IDENTIFIER && ndref(pin, 0)->name == pdn->name);
        if (!static_eval(ndref(pin, 1), NULL, &r)) {
          neprintf(pin, "non-constant initializer");
        } else if (!(ts_numerical(r.ts) || (r.ts == TS_PTR && r.val.i == 0))) {
          neprintf(pin, "unsupported type of global initializer"); /* NYI: non-0 offset (fixme) */
        } else {
          pg = watiebref(&g_curpwm->exports, pgidx); /* re-fetch after static_eval */
          if (r.ts != TS_PTR || r.id == 0) {
            if (!ts_numerical_or_enum(r.ts) || !ts_numerical_or_enum(ptn->ts)) {
              if (ptn->ts != TS_PTR) 
                neprintf(pdn, "unexpected initializer for a non-pointer type");
              if (!ts_init_ptr_compatible(r.ts, &r.val))
                neprintf(pdn, "unexpected initializer for a pointer type");
            } else if (ts_arith_assign_compatible(ptn->ts, r.ts) || /* r.ts can be promoted up (no value check) */
              ts_arith_init_compatible(ptn->ts, r.ts, &r.val)) { /* r.ts can be converted saving value */
              if (ptn->ts != r.ts) { numval_convert(ptn->ts, r.ts, &r.val); r.ts = ptn->ts; }
            } else neprintf(pdn, "constant initializer cannot be implicitly converted to expected type");
            asm_numerical_const(r.ts == TS_PTR ? TS_UINT : r.ts, &r.val, &pg->ic); /* wasm32 */
          } else {
            const node_t *pimn = lookup_global(r.id); inscode_t *pic = &pg->ic;
            if (!pimn) neprintf(pdn, "unknown identifier in address initializer");
            assert(pimn->nt == NT_IMPORT && pimn->name != 0 && ndlen(pimn) == 1);
            pic->in = ndcref(pimn, 0)->ts == TS_FUNCTION ? IN_REF_FUNC : IN_REF_DATA; 
            pic->id = r.id; pic->arg2.mod = pimn->name;
          }
        }
      }  
      pg->mut = MT_VAR;
      pg->vt = ts2vt(ptn->ts);
      if (pg->vt == VT_UNKN) neprintf(pin, "invalid type for a global");
    }
  }
}


/* hoist_locals environment data type */ 
typedef struct henv {
  buf_t idmap; /* pairs of symbols <old, new> */
  struct henv *up; 
} henv_t;

/* return new name given oname; 0 if not found */
static sym_t henv_lookup(sym_t oname, henv_t *phe, int *pupc)
{
  if (pupc) *pupc = 0;
  while (phe) {
    sym_t *pon = bufsearch(&phe->idmap, &oname, &sym_cmp);
    if (pon) return pon[1];
    phe = phe->up; if (pupc) *pupc += 1;
  }
  return 0;
}

/* see if this local name is already in use */
static bool locals_lookup(sym_t name, ndbuf_t *plb)
{
  size_t i;
  for (i = 0; i < ndblen(plb); ++i) {
    node_t *pn = ndbref(plb, i); 
    if (pn->name == name) return true;
  }
  return false;
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
          } else if (locals_lookup(nname, plb)) {
            sym_t *pon = bufnewbk(&he.idmap);
            nname = internf("%s#%d", symname(pni->name), (int)buflen(plb));
            pon[0] = pni->name, pon[1] = nname;
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
      sym_t *pon = bufsearch(&he.idmap, &name, &sym_cmp);
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

/* check if node can be evaluated statically as an arithmetic expression
 * NB: positive answer does not guarantee successful or error-free evaluation
 * (but eval errors will also happen at run time if original expr is compiled);
 * negative answer saves time on attempting actual evaluation */
static bool arithmetic_constant_expr(node_t *pn)
{
  switch (pn->nt) {
    case NT_LITERAL: 
      return ts_numerical(pn->ts);
    case NT_INTRCALL:
      switch (pn->intr) {
        case INTR_SIZEOF: case INTR_ALIGNOF: case INTR_OFFSETOF:
          return true;
        case INTR_ASU32: case INTR_ASFLT: 
        case INTR_ASU64: case INTR_ASDBL:
          assert(ndlen(pn) == 1); 
          return arithmetic_constant_expr(ndref(pn, 0));
        default:;
      }
      return false;
    case NT_PREFIX:
      assert(ndlen(pn) == 1); 
      switch (pn->op) {
        case TT_PLUS:  case TT_MINUS: case TT_TILDE: case TT_NOT:
          return arithmetic_constant_expr(ndref(pn, 0));
        default:;
      }
      return false;
    case NT_INFIX: 
      assert(ndlen(pn) == 2); 
      switch (pn->op) {
        case TT_PLUS:  case TT_MINUS: case TT_STAR:  case TT_AND:
        case TT_OR:    case TT_XOR:   case TT_SLASH: case TT_REM:
        case TT_SHL:   case TT_SHR:   case TT_EQ:    case TT_NE:
        case TT_LT:    case TT_GT:    case TT_LE:    case TT_GE:
          return arithmetic_constant_expr(ndref(pn, 0)) && arithmetic_constant_expr(ndref(pn, 1));
        default:;
      } 
      return false;
    case NT_COND:
      assert(ndlen(pn) == 3); 
      return arithmetic_constant_expr(ndref(pn, 0)) && 
             arithmetic_constant_expr(ndref(pn, 1)) && arithmetic_constant_expr(ndref(pn, 2));
    case NT_CAST: 
      assert(ndlen(pn) == 2); assert(ndref(pn, 0)->nt == NT_TYPE);
      return ts_numerical(ndref(pn, 0)->ts) && arithmetic_constant_expr(ndref(pn, 1));
    default:; 
  }
  return false;
}

/* check if display can be calculated statically */
static bool static_display(node_t *pn, buf_t *prib)
{
  switch (pn->nt) {
    case NT_LITERAL: {
      /* both numeric and string literals are ok */
      return true;
    } break;
    case NT_DISPLAY: {
      size_t n = ndlen(pn), i;
      assert(n > 0 && ndref(pn, 0)->nt == NT_TYPE);
      for (i = 1; i < n; ++i) {
        if (!static_display(ndref(pn, i), prib)) return false;
      }
      return true;
    } break;
    case NT_PREFIX: {
      assert(ndlen(pn) == 1);
      if (pn->op == TT_AND && ndref(pn, 0)->nt == NT_IDENTIFIER) {
        pn = ndref(pn, 0);
        if (bufbsearch(prib, &pn->name, &sym_cmp) == NULL) { /* global var */
          sym_t mod; const node_t *ptn = lookup_var_type(pn->name, prib, &mod);
          return ptn && ts_bulk(ptn->ts); /* ok if bulk */
        }
      }
      return false;
    } break;
    case NT_CAST: {
      ts_t tc;
      assert(ndlen(pn) == 2); assert(ndref(pn, 0)->nt == NT_TYPE);
      if (ts_numerical(tc = ndref(pn, 0)->ts))
        return arithmetic_constant_expr(ndref(pn, 1));
      if (tc == TS_PTR && ndlen(ndref(pn, 0)) == 1 && ndref(ndref(pn, 0), 0)->ts == TS_VOID)
        return arithmetic_constant_expr(ndref(pn, 1));
      if (tc == TS_ARRAY)
        return static_display(ndref(pn, 1), prib);
      return false;
    } break;
    default: {
      return arithmetic_constant_expr(pn);
    } break;
  }
  return false;
}

/* replace arithmetic constant expressions in pn with literals */
static void expr_fold_constants(node_t *pn, buf_t *prib)
{
  switch (pn->nt) {
    case NT_NULL:
    case NT_LITERAL: 
    case NT_IDENTIFIER:
      /* can't be folded */
      break;
    case NT_TYPE:
      /* no expressions inside */
      break;
    case NT_VARDECL:
    case NT_FUNDEF:
    case NT_TYPEDEF:
    case NT_MACRODEF:
    case NT_INCLUDE:
    case NT_IMPORT:
      /* can't be part of an expression */
      assert(false);
      break;
    case NT_DISPLAY:
      if (static_display(pn, prib)) pn->sc = SC_STATIC;
      /* fall thru (i.e. fold arith constants inside) */
    default: {
      node_t *pcn = NULL;
      if (arithmetic_constant_expr(pn)) {
        node_t nd = mknd();
        if (arithmetic_eval(pn, prib, &nd)) ndswap(&nd, pn);
        ndfini(&nd);
      } else {
        size_t i;
        for (i = 0; i < ndlen(pn); ++i) {
          expr_fold_constants(ndref(pn, i), prib);
        }   
      }
    } break;
  }
}

/* fold arithmetic constant expressions inside function with literals */
static void fundef_fold_constants(node_t *pdn)
{
  size_t i; buf_t rib = mkbuf(sizeof(ri_t));
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1);
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION && ndlen(ptn) > 0);
  /* go over parameter definitions */
  for (i = 1 /* skip result type */; i < ndlen(ptn); ++i) {
    node_t *pdni = ndref(ptn, i), *ptni = pdni; sym_t name = 0;
    if (pdni->nt == NT_VARDECL) name = ptni->name, ptni = ndref(pdni, 0); 
    if (name) { ri_t *pri = bufnewbk(&rib); pri->name = name; pri->ptn = ptni; }
  }
  assert(pbn->nt == NT_BLOCK);
  /* go over hoisted local var definitions */
  for (i = 0; i < ndlen(pbn); ++i) {
    node_t *pdni = ndref(pbn, i), *ptni; sym_t name;
    if (pdni->nt != NT_VARDECL) break; /* statements may follow */
    name = pdni->name, ptni = ndref(pdni, 0); 
    if (name) { ri_t *pri = bufnewbk(&rib); pri->name = name; pri->ptn = ptni; }
  }    
  bufqsort(&rib, &sym_cmp);
  /* fold expressions inside body statements */
  for (/* use current i */; i < ndlen(pbn); ++i) {
    expr_fold_constants(ndref(pbn, i), &rib);
  }
  buffini(&rib);
}


/* local variable info for wasmify */
typedef struct vi {
  sym_t name;  /* var name in source code (possibly after renaming) */
  sc_t sc;     /* SC_REGISTER or SC_AUTO only */
  sym_t reg;   /* REGISTER: actual register name; AUTO: pointer register name */
  sym_t fld;   /* AUTO: 0 or field name (if reg is struct/union pointer) */
  ts_t cast;   /* TS_VOID or TS_CHAR..TS_USHORT for widened vars */
} vi_t;

/* case sorting aid: sorts cases numerically, default is first; dupcheck */
int case_cmp(const void *p1, const void *p2)
{
  const node_t *pn1 = p1, *pn2 = p2; int i1, i2;
  if (pn1->nt == NT_DEFAULT && pn2->nt == NT_DEFAULT)
    n2eprintf(pn1, pn2, "duplicate default cases"); 
  if (pn1->nt == NT_DEFAULT) return -1;
  if (pn2->nt == NT_DEFAULT) return 1;
  assert(pn1->nt == NT_CASE && pn2->nt == NT_CASE);
  i1 = (int)ndcref(pn1, 0)->val.i, i2 = (int)ndcref(pn2, 0)->val.i;
  if (i1 == i2) n2eprintf(pn1, pn2, "duplicate cases"); 
  return i1 < i2 ? -1 : 1;
}

///* convert init display to a bunch of assignments */
//static void init_wasmify(node_t *pvn, node_t *ptn, node_t *pdn, size_t i, node_t *pbn)
//{
//  /* input assertion: elements pdn[i]... fill bulk type ptn */
//  if (ptn->ts == TS_ARRAY) {
//    int count = 0; node_t *petn, *pcn; assert(ndlen(ptn) == 2);
//    petn = ndref(ptn, 0); pcn = ndref(ptn, 1); 
//    if (!arithmetic_eval_to_int(pcn, &count) || count <= 0)
//       n2eprintf(pcn, ptn, "can't init arrays of nonconstant/nonpositive size");
//    while (count > 0) {
//      if (i < 
//    }
//    
//  } else if (ptn->ts == TS_STRUCT) {
//  } else if (ptn->ts == TS_UNION) {
//  } else {
//    n2eprintf(pvn, pdn, "invalid type for {} initializer");
//  }
//}

/* convert node wasm conventions, simplify operators and control */
static void expr_wasmify(node_t *pn, buf_t *pvib, buf_t *plib)
{
  switch (pn->nt) {
    case NT_IDENTIFIER: {
      vi_t *pvi = bufbsearch(pvib, &pn->name, &sym_cmp);
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
      } else { /* global */
        const node_t *ptn = lookup_global(pn->name);
        if (ptn && ptn->nt == NT_IMPORT && ndlen(ptn) == 1) ptn = ndcref(ptn, 0);
        if (ptn->nt == NT_TYPE && ptn->ts >= TS_BOOL && ptn->ts < TS_INT) { /* add narrowing cast */
          node_t cn = mknd(); ndset(&cn, NT_TYPE, -1, -1); cn.ts = ptn->ts;
          ndswap(&cn, pn); wrap_cast(pn, &cn); ndfini(&cn);
        }
      }
    } break;
    case NT_INTRCALL: {
      switch (pn->intr) {
        case INTR_VAETC: { /* () => ap$ */
          sym_t ap = intern("ap$");
          vi_t *pvi = bufbsearch(pvib, &ap, &sym_cmp);
          if (!pvi) neprintf(pn, "access to ... in non-vararg function");
          assert(pvi->sc == SC_REGISTER && !pvi->fld);
          pn->nt = NT_IDENTIFIER; pn->name = pvi->reg;
        } break;
        case INTR_VAARG: { /* (id, type) => *(type*)id++ */
          node_t *pin, *ptn; assert(ndlen(pn) == 2);
          pin = ndref(pn, 0), ptn = ndref(pn, 1);
          assert(pin->nt == NT_IDENTIFIER && ptn->nt == NT_TYPE);
          wrap_postfix_operator(pin, TT_PLUS_PLUS, 0);
          wrap_type_pointer(ptn);
          ndswap(pin, ptn); pn->nt = NT_CAST;
          wrap_unary_operator(pn, pn->startpos, TT_STAR);
        } break;
        case INTR_SIZEOF: case INTR_ALIGNOF: case INTR_OFFSETOF: {
          node_t *pan; assert(ndlen(pn) >= 1);
          pan = ndref(pn, 0);
          if (pan->nt != NT_TYPE) expr_wasmify(pan, pvib, plib);
        } break;
        case INTR_ALLOCA: 
        case INTR_ASU32:  case INTR_ASFLT:
        case INTR_ASU64:  case INTR_ASDBL: {
          size_t i;
          for (i = 0; i < ndlen(pn); ++i) expr_wasmify(ndref(pn, i), pvib, plib);
        } break;
        default:;
      }
    } break;
    case NT_POSTFIX: {
      assert(ndlen(pn) == 1);
      expr_wasmify(ndref(pn, 0), pvib, plib);
      if (pn->op == TT_ARROW) { /* (&x)->y => x.y; x->y => (*x).y */
        node_t *psn = ndref(pn, 0);
        if (psn->nt == NT_PREFIX && psn->op == TT_AND) lift_arg0(psn);
        else wrap_unary_operator(psn, psn->startpos, TT_STAR);
        pn->op = TT_DOT;
      } 
    } break;
    //case NT_ASSIGN: {
    //  node_t *pln, *prn;
    //  assert(ndlen(pn) == 2);
    //  pln = ndref(pn, 0), prn = ndref(pn, 1);
    //  if (pn->op == TT_ASN && pln->nt == NT_IDENTIFIER && prn->nt == NT_DISPLAY) {
    //    node_t nd = mknd(); ndset(&nd, NT_BLOCK, pn->pwsid, pn->startpos);
    //    assert(ndlen(prn) >= 1 && ndref(prn, 0)->nt == NT_TYPE);
    //    init_wasmify(pln, ndref(prn, 0), prn, 1, &nd);
    //    ndswap(pn, &nd); ndfini(&nd);
    //    expr_wasmify(pn, pvib, plib);
    //  } else {
    //    expr_wasmify(pln, pvib, plib);
    //    expr_wasmify(prn, pvib, plib);
    //  }
    //} break;
    case NT_WHILE: {
      /* while (x) s => {{cl: if (x) {s; goto cl;}} bl:} */
      sym_t bl = rpalloc_label(), cl = rpalloc_label();
      int *pil; node_t *psn; assert(ndlen(pn) == 2);
      expr_wasmify(ndref(pn, 0), pvib, plib);
      pil = bufnewbk(plib); pil[0] = bl, pil[1] = NT_BREAK;  
      pil = bufnewbk(plib); pil[0] = cl, pil[1] = NT_CONTINUE;  
      expr_wasmify(ndref(pn, 1), pvib, plib);
      psn = wrap_node(ndref(pn, 1), NT_BLOCK);
      psn = ndinsbk(psn, NT_GOTO); psn->name = cl;
      pn->nt = NT_IF;
      wrap_node(pn, NT_BLOCK); pn->name = cl; pn->op = TT_CONTINUE_KW;
      wrap_node(pn, NT_BLOCK); pn->name = bl; pn->op = TT_BREAK_KW;
      bufpopbk(plib);
      bufpopbk(plib);
    } break;
    case NT_DO: {
      /* do s while (x) => {{l: {s; cl:} if (x) goto l;} bl:} */
      sym_t l = rpalloc_label(), bl = rpalloc_label(), cl = rpalloc_label();
      int *pil; node_t *psn; assert(ndlen(pn) == 2);
      expr_wasmify(ndref(pn, 1), pvib, plib); /* x */
      pil = bufnewbk(plib); pil[0] = bl, pil[1] = NT_BREAK;  
      pil = bufnewbk(plib); pil[0] = cl, pil[1] = NT_CONTINUE;  
      expr_wasmify(ndref(pn, 0), pvib, plib); /* s */
      psn = wrap_node(ndref(pn, 1), NT_IF); psn = ndinsbk(psn, NT_GOTO); psn->name = l;
      psn = wrap_node(ndref(pn, 0), NT_BLOCK); psn->name = cl; psn->op = TT_BREAK_KW;
      pn->nt = NT_BLOCK; pn->name = l; pn->op = TT_CONTINUE_KW;
      wrap_node(pn, NT_BLOCK); pn->name = bl; pn->op = TT_BREAK_KW;
      bufpopbk(plib);
      bufpopbk(plib);
    } break;
    case NT_FOR: {
      /* for (x; y; z) s => {x; {l: if (!y) goto bl; {s; cl:} z; goto l;} bl:} */
      sym_t l = rpalloc_label(), bl = rpalloc_label(), cl = rpalloc_label();
      int *pil; node_t nd = mknd(), *psn; assert(ndlen(pn) == 4);
      psn = ndref(pn, 0); assert(psn->nt != NT_NULL || !ndlen(psn)); /* x */ 
      if (psn->nt != NT_NULL) expr_wasmify(psn, pvib, plib); else psn->nt = NT_BLOCK; /* {} */
      psn = ndref(pn, 1); assert(psn->nt != NT_NULL || !ndlen(psn)); /* y */ 
      if (psn->nt != NT_NULL) expr_wasmify(psn, pvib, plib); else set_to_int(psn, 1); /* 1 */
      psn = ndref(pn, 2); assert(psn->nt != NT_NULL || !ndlen(psn)); /* z */ 
      if (psn->nt != NT_NULL) expr_wasmify(psn, pvib, plib); else psn->nt = NT_BLOCK; /* {} */
      pil = bufnewbk(plib); pil[0] = bl, pil[1] = NT_BREAK;  
      pil = bufnewbk(plib); pil[0] = cl, pil[1] = NT_CONTINUE;  
      expr_wasmify(ndref(pn, 3), pvib, plib); /* s */
      psn = wrap_node(ndref(pn, 3), NT_BLOCK); psn->name = cl; psn->op = TT_BREAK_KW;
      psn = wrap_node(ndref(pn, 1), NT_PREFIX); psn->op = TT_NOT;
      psn = wrap_node(psn, NT_IF); psn = ndinsbk(psn, NT_GOTO); psn->name = bl;
      /* for x [if (!y) goto bl;] z {s; cl:} */
      ndswap(ndref(pn, 2), ndref(pn, 3));
      /* for x [if (!y) goto bl;] {s; cl:} z; */
      psn = ndinsbk(pn, NT_GOTO); psn->name = l;
      /* for x [if (!y) goto bl;] {s; cl:} z; goto l; */
      ndswap(ndref(pn, 0), &nd); ndrem(pn, 0);
      pn->nt = NT_BLOCK; pn->name = l; pn->op = TT_CONTINUE_KW; 
      /* x  {l: if (!y) goto bl; {s; cl:} z; goto l;} */
      wrap_node(pn, NT_BLOCK); pn->name = bl; pn->op = TT_BREAK_KW;
      ndswap(ndnewfr(pn), &nd);
      bufpopbk(plib);
      bufpopbk(plib);
      ndfini(&nd);
    } break;
    case NT_SWITCH: {
      /* switch (x) case c1: s1 ... => 
       * {{..{switch (x) case c1: goto l1; ...; goto bl; l1:} s1 l2:} s2 ...} bl:} */
      sym_t bl = rpalloc_label(); buf_t clb; ndbuf_t ndb;
      int *pil; size_t i; assert(ndlen(pn) >= 1);
      bufinit(&clb, sizeof(sym_t)); ndbinit(&ndb);
      expr_wasmify(ndref(pn, 0), pvib, plib); /* x */
      pil = bufnewbk(plib); pil[0] = bl, pil[1] = NT_BREAK;  
      for (i = 1; i < ndlen(pn); ++i) { /* move clause stms to ndb, convert to gotos */
        sym_t l = rpalloc_label(), *pl = bufnewbk(&clb);
        node_t *psn = ndref(pn, i), *pni = ndset(ndbnewbk(&ndb), psn->nt, psn->pwsid, psn->startpos);
        expr_wasmify(psn, pvib, plib); ndswap(psn, pni); pni->nt = NT_BLOCK;
        if (psn->nt == NT_CASE) { ndswap(ndnewbk(psn), ndref(pni, 0)); ndrem(pni, 0); }
        ndinsbk(psn, NT_GOTO)->name = l; *pl = l; 
      }
      if (ndlen(pn) > 2) qsort(ndref(pn, 1), ndlen(pn)-1, sizeof(node_t), &case_cmp);
      if (ndlen(pn) == 1 || ndref(pn, 1)->nt != NT_DEFAULT) {
        node_t *psn = ndset(ndinsnew(pn, 1), NT_DEFAULT, pn->pwsid, pn->startpos);
        ndinsbk(psn, NT_GOTO)->name = bl;  
      } 
      wrap_node(pn, NT_BLOCK);
      for (i = 0; i < ndblen(&ndb); ++i) { /* nest labeled blocks */
        sym_t *pl = bufref(&clb, i); node_t *pni = ndbref(&ndb, i);
        pn->name = *pl; pn->op = TT_BREAK_KW;
        ndswap(pn, ndnewfr(pni)); ndswap(pn, pni); 
      } 
      pn->name = bl; pn->op = TT_BREAK_KW;
      bufpopbk(plib);
      buffini(&clb), ndbfini(&ndb); 
    } break;
    case NT_CASE: {
      int x; size_t i; assert(ndlen(pn) >= 1);
      /* fixme: correct eval environment prib (ri_t) is not yet available here;
       * todo: need to somehow make use of pvib or unify it w/prib  */
      if (arithmetic_eval_to_int(ndref(pn, 0), NULL, &x)) set_to_int(ndref(pn, 0), x);
      else neprintf(pn, "noninteger case label"); 
      for (i = 1; i < ndlen(pn); ++i) expr_wasmify(ndref(pn, i), pvib, plib);
    } break;
    case NT_DEFAULT: {
      size_t i;
      for (i = 0; i < ndlen(pn); ++i) expr_wasmify(ndref(pn, i), pvib, plib);
    } break;
    case NT_GOTO: {
      int *pil = bufsearch(plib, &pn->name, &sym_cmp);
      if (!pil) neprintf(pn, "goto to unknown or mispositioned label");
      else assert(pil[1] == 0); /* not a break/continue target */
    } break;
    case NT_RETURN: {
      sym_t rp = intern("rp$"), bp = intern("bp$"); 
      vi_t *prvi, *pbvi; node_t *psn;
      if (ndlen(pn) > 0) {
        assert(ndlen(pn) == 1);
        expr_wasmify(ndref(pn, 0), pvib, plib);
      }
      prvi = bufbsearch(pvib, &rp, &sym_cmp);
      pbvi = bufbsearch(pvib, &bp, &sym_cmp);
      if (prvi) { /* return x => { *rp$ = x; return; } */
        if (ndlen(pn) != 1) neprintf(pn, "return value expected"); 
        wrap_node(lift_arg0(pn), NT_ASSIGN); pn->op = TT_ASN;
        psn = ndinsfr(pn, NT_PREFIX); psn->op = TT_STAR;
        psn = ndinsbk(psn, NT_IDENTIFIER); psn->name = rp;
        wrap_node(pn, NT_BLOCK); 
        psn = ndinsbk(pn, NT_RETURN);
        if (pbvi) psn->name = bp;
      } else {
        if (pbvi) pn->name = bp;
      }
    } break;
    case NT_BREAK: case NT_CONTINUE: {
      size_t i; 
      for (i = buflen(plib); i > 0; --i) {
        int *pil = bufref(plib, i-1);
        if (pil[1] == pn->nt) {
          pn->name = (sym_t)pil[0];
          pn->nt = NT_GOTO;
          return;
        }
      }
      neprintf(pn, "unexpected %s operator", (pn->nt == NT_BREAK) ? "break" : "continue");
    } break;
    case NT_BLOCK: {
      size_t i;
      if (pn->name) {
        int *pil = bufnewbk(plib);
        assert(pn->op == TT_BREAK_KW || pn->op == TT_CONTINUE_KW);
        pil[0] = pn->name; pil[1] = 0; /* not a break/continue target */ 
      } 
      for (i = 0; i < ndlen(pn); ++i) expr_wasmify(ndref(pn, i), pvib, plib);
      if (pn->name) {
        bufpopbk(plib);
      }
    } break;
    case NT_TYPE: {
      /* nothing of interest in here */
    } break;
    default: {
      size_t i;
      for (i = 0; i < ndlen(pn); ++i) expr_wasmify(ndref(pn, i), pvib, plib);
    } break;
  }
}

/* check if node ends in NT_RETURN */
static bool ends_in_return(node_t *pn)
{
  return (pn && pn->nt == NT_RETURN);
}

/* check if body contains ALLOCA calls */
static bool contains_alloca(node_t *pn)
{
  switch (pn->nt) {
    case NT_TYPE: {
      /* nothing of interest in here */
    } break;
    case NT_INTRCALL: 
      if (pn->intr == INTR_ALLOCA) return true;
      /* else fall thru */
    default: {
      size_t i;
      for (i = 0; i < ndlen(pn); ++i) 
        if (contains_alloca(ndref(pn, i))) return true;
    } break;
  }
  return false;
}

/* convert function to wasm conventions, simplify */
static void fundef_wasmify(node_t *pdn)
{
  /* special registers (scalar/poiner vars with internal names): 
   * bp$ (base pointer), fp$ (frame pointer), ap$ (... data pointer) */
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1);
  buf_t vib = mkbuf(sizeof(vi_t)); node_t frame = mknd();
  ndbuf_t asb; buf_t lib; size_t i; bool explicit_tail_ret = false;
  ndbinit(&asb); bufinit(&lib, sizeof(int)*2); /* <tt, lbsym> */
  
  /* init frame structure, in case there are auto locals */
  ndset(&frame, NT_TYPE, pdn->pwsid, pdn->startpos);
  frame.ts = TS_STRUCT;
  
  /* wasmify function signature and collect arg vars */
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  for (i = 0; i < ndlen(ptn); ++i) {
    node_t *pdni = ndref(ptn, i), *ptni = pdni; sym_t name = 0; ts_t cast = TS_VOID; 
    if (pdni->nt == NT_VARDECL) name = ptni->name, ptni = ndref(pdni, 0); 
    switch (ptni->ts) {
      case TS_ETC: { /* last */
        vi_t *pvi = bufnewbk(&vib);
        assert(i+1 == ndlen(ptn));
        ndset(pdni, NT_VARDECL, pdni->pwsid, pdni->startpos);
        pdni->name = intern("ap$");
        ptni = ndinsbk(pdni, NT_TYPE); ptni->ts = TS_ULLONG; wrap_type_pointer(ptni);
        pvi->name = pvi->reg = pdni->name; pvi->sc = SC_REGISTER;
      } break; 
      case TS_VOID: /* ok if alone */ 
        assert(i == 0); /* we have checked earlier: this is return type */
        break;
      case TS_BOOL:  case TS_ENUM:
      case TS_CHAR:  case TS_UCHAR:  case TS_SHORT: case TS_USHORT: 
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
      case TS_ARRAY: case TS_STRUCT: case TS_UNION: { 
        if (i == 0) { /* return value: insert $rp as first arg */
          node_t *prn = ndinsnew(ptn, 1), *pn; ptni = ndref(ptn, 0); /* re-fetch */
          ndset(prn, NT_VARDECL, ptni->pwsid, ptni->startpos);
          pn = ndinsbk(prn, NT_TYPE), pn->ts = TS_VOID; ndswap(pn, ptni);
          wrap_type_pointer(pn); prn->name = intern("rp$");
          break;
        } /* else fall through */ 
      }
      default:
        neprintf(pdni, "function arguments of type %s are not supported", ts_name(ptni->ts));
    }
  }

  /* wasmify hoisted vardefs, collect arg vars */
  assert(pbn->nt == NT_BLOCK);
  for (i = 0; i < ndlen(pbn); /* bump i */) {
    node_t *pdni = ndref(pbn, i), *ptni; sym_t name; ts_t cast = TS_VOID;
    if (pdni->nt != NT_VARDECL) break; /* statements may follow */
    name = pdni->name, ptni = ndref(pdni, 0); 
    switch (ptni->ts) {
      case TS_BOOL: case TS_ENUM:
      case TS_CHAR:  case TS_UCHAR:  case TS_SHORT: case TS_USHORT: 
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
  
  /* see if we need to insert bp$/fp$ inits & locals */
  if (ndlen(&frame) > 0) {
    node_t *pin, *psn; vi_t *pvi;
    pin = ndinsnew(pbn, i);
    ndset(pin, NT_ASSIGN, pbn->pwsid, pbn->startpos);
    pin->op = TT_ASN;
    psn = ndinsbk(pin, NT_IDENTIFIER); psn->name = intern("fp$");
    psn = ndinsbk(pin, NT_INTRCALL); psn->intr = INTR_ALLOCA;
    psn = ndinsbk(psn, NT_INTRCALL); psn->intr = INTR_SIZEOF;
    ndpushbk(psn, &frame);
    pin = ndinsnew(pbn, i);
    ndset(pin, NT_ASSIGN, pbn->pwsid, pbn->startpos);
    pin->op = TT_ASN;
    psn = ndinsbk(pin, NT_IDENTIFIER); psn->name = intern("bp$");
    psn = ndinsbk(pin, NT_IDENTIFIER); psn->name = g_sp_id;
    pin = ndinsnew(pbn, i);
    ndset(pin, NT_VARDECL, pbn->pwsid, pbn->startpos);
    pin->name = intern("fp$"); wrap_type_pointer(ndpushbk(pin, &frame));
    pvi = bufnewbk(&vib); pvi->sc = SC_REGISTER;
    pvi->name = pvi->reg = pin->name;
    pin = ndinsnew(pbn, i);
    ndset(pin, NT_VARDECL, pbn->pwsid, pbn->startpos);
    pin->name = intern("bp$"); wrap_type_pointer(ndsettype(ndnewbk(pin), TS_VOID));
    pvi = bufnewbk(&vib); pvi->sc = SC_REGISTER;
    pvi->name = pvi->reg = pin->name;
    i += 4; /* skip inserted nodes */ 
  } else if (contains_alloca(pbn)) {
    node_t *pin, *psn; vi_t *pvi;
    pin = ndinsnew(pbn, i);
    ndset(pin, NT_ASSIGN, pbn->pwsid, pbn->startpos);
    pin->op = TT_ASN;
    psn = ndinsbk(pin, NT_IDENTIFIER); psn->name = intern("bp$");
    psn = ndinsbk(pin, NT_IDENTIFIER); psn->name = g_sp_id;
    pin = ndinsnew(pbn, i);
    ndset(pin, NT_VARDECL, pbn->pwsid, pbn->startpos);
    pin->name = intern("bp$"); wrap_type_pointer(ndsettype(ndnewbk(pin), TS_VOID));
    pvi = bufnewbk(&vib); pvi->sc = SC_REGISTER;
    pvi->name = pvi->reg = pin->name;
    i += 2; /* skip inserted nodes */ 
  }
  
  /* sort vib by name for faster lookup in wasmify_expr */
  bufqsort(&vib, &sym_cmp);

  for (/* use current i */; i < ndlen(pbn); ++i) {
    node_t *pni = ndref(pbn, i);
    if (i + 1 == ndlen(pbn)) explicit_tail_ret = ends_in_return(pni);
    expr_wasmify(pni, &vib, &lib);
  }

  /* check if falling off is a valid way to return */
  if (ndref(ptn, 0)->ts == TS_VOID && !explicit_tail_ret) {
    /* insert explicit return */
    sym_t bp = intern("bp$"); 
    vi_t *pvi = bufbsearch(&vib, &bp, &sym_cmp);
    node_t *psn = ndinsbk(pbn, NT_RETURN);
    if (pvi) psn->name = bp;
  }

  buffini(&lib);
  ndbfini(&asb);
  ndfini(&frame);
  buffini(&vib);
}

/* check if tan type can be assigned to parameter/lval tpn type */
static bool assign_compatible(const node_t *ptpn, const node_t *ptan)
{
  if (same_type(ptan, ptpn)) return true;
  /* with numericals, enums behave like ints */
  if (ptpn->ts == TS_ENUM && ts_numerical(ptan->ts)) 
    return ts_arith_assign_compatible(TS_INT, ptan->ts);
  if (ts_numerical(ptpn->ts) && ptan->ts == TS_ENUM) 
    return ts_arith_assign_compatible(ptpn->ts, TS_INT);
  /* numerical rval should be promotable to lval's type  */
  if (ts_numerical(ptpn->ts) && ts_numerical(ptan->ts)) 
    return ts_arith_assign_compatible(ptpn->ts, ptan->ts);
  /* array can be assigned to its element ptr */
  if (ptpn->ts == TS_PTR && ptan->ts == TS_ARRAY)
    return same_type(ndcref(ptan, 0), ndcref(ptpn, 0));
  /* void pointers can be assined to/from other pointers */
  if (ptpn->ts == TS_PTR && ptan->ts == TS_PTR)
    return ndcref(ptpn, 0)->ts == TS_VOID || ndcref(ptan, 0)->ts == TS_VOID;
  return false;
}

/* check if tan type can be cast to tpn type */
static bool cast_compatible(const node_t *ptpn, const node_t *ptan)
{
  /* enum can be cast to a numerical and vice versa */
  if (ptpn->ts == TS_ENUM && ts_numerical(ptan->ts)) return true; 
  if (ts_numerical(ptpn->ts) && ptan->ts == TS_ENUM) return true; 
  /* any two number types can be explicitly cast to each other */
  if (ts_numerical(ptpn->ts) && ts_numerical(ptan->ts)) return true; 
  /* any two pointer types can be explicitly cast to each other */
  if (ptpn->ts == TS_PTR && ptan->ts == TS_PTR) return true;
  /* allow long=>ptr and ptr=>long casts */
  if (ptpn->ts == TS_PTR && ts_numerical(ptan->ts))
    return ts_arith_common(ptan->ts, TS_ULONG) == TS_ULONG; /* can be promoted up to ULONG */
  if (ts_numerical(ptpn->ts) && ptan->ts == TS_PTR)
    return ts_arith_common(ptpn->ts, TS_LONG) == ptpn->ts; /* LONG and up */
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
        case TS_BOOL: /* assume that value is 0 or 1, so treat as unsigned int */
        case TS_UCHAR:                           in = IN_F64_CONVERT_I32_U; break;
        case TS_USHORT:                          in = IN_F64_CONVERT_I32_U; break;
        case TS_UINT: case TS_ULONG:/* wasm32 */ in = IN_F64_CONVERT_I32_U; break;
        case TS_LLONG:                           in = IN_F64_CONVERT_I64_S; break;
        case TS_ULLONG:                          in = IN_F64_CONVERT_I64_U; break;
        default:;
      } break;
    case TS_FLOAT:
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_F32_DEMOTE_F64;    break;
        case TS_CHAR:   in0 = IN_I32_EXTEND8_S,  in = IN_F32_CONVERT_I32_S; break; 
        case TS_SHORT:  in0 = IN_I32_EXTEND16_S, in = IN_F32_CONVERT_I32_S; break; 
        case TS_INT: case TS_LONG:/* wasm32 */   in = IN_F32_CONVERT_I32_S; break;
        case TS_BOOL: /* assume that value is 0 or 1, so treat as unsigned int */
        case TS_UCHAR:                           in = IN_F32_CONVERT_I32_U; break;
        case TS_USHORT:                          in = IN_F32_CONVERT_I32_U; break;
        case TS_UINT: case TS_ULONG:/* wasm32 */ in = IN_F32_CONVERT_I32_U; break;
        case TS_LLONG:                           in = IN_F32_CONVERT_I64_S; break;
        case TS_ULLONG:                          in = IN_F32_CONVERT_I64_U; break;
        default:;
      } break;
    case TS_BOOL:
      switch (tsfrom) {
        case TS_DOUBLE: { 
          inscode_t *pic = icbnewbk(pdata); pic->in = IN_F64_CONST; pic->arg.d = 0.0;
          icbnewbk(pdata)->in = IN_F64_NE; /* i.e. x != 0.0 */
          return;
        } break;
        case TS_CHAR:  case TS_SHORT:  case TS_INT:  case TS_LONG:/* wasm32 */ 
        case TS_UCHAR: case TS_USHORT: case TS_UINT: case TS_ULONG:/* wasm32 */ {
          /* these are all ints, we need to compare to zero and negate */
          in0 = IN_I32_EQZ; in = IN_I32_EQZ; /* i.e. !!x */
        } break;
        case TS_LLONG: case TS_ULLONG: {
          /* these are all ints, we need to compare to zero and negate */
          in0 = IN_I64_EQZ; in = IN_I32_EQZ; /* i.e. !!x, second ! acts on int */
        } break;
        default:;
      } break;
    case TS_CHAR: case TS_SHORT: { 
        /* these are still ints, we need to replace upper bits w/sign bit */
        in = (tsto == TS_CHAR) ? IN_I32_EXTEND8_S : IN_I32_EXTEND16_S;
        asm_numerical_cast(TS_INT, tsfrom, pdata); 
        icbnewbk(pdata)->in = in;
        return;
      } break;  
    case TS_UCHAR: case TS_USHORT: {
        /* these are still ints, we need to mask upper bits */
        inscode_t *pic; unsigned m = (tsto == TS_UCHAR) ? 0xFF : 0xFFFF;
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
        case TS_INT:  case TS_LONG:/* wasm32 */
        case TS_BOOL: case TS_UCHAR: case TS_USHORT:
        case TS_UINT: case TS_ULONG:/* wasm32 */ /* nop */                  break;
        case TS_LLONG:                           in = IN_I32_WRAP_I64;      break;
        case TS_ULLONG:                          in = IN_I32_WRAP_I64;      break;
        default:;
      } break;
    case TS_UINT: case TS_ULONG: /* ULONG: wasm32 assumed */
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_I32_TRUNC_F64_U;   break;
        case TS_FLOAT:                           in = IN_I32_TRUNC_F32_U;   break;
        case TS_CHAR:                            in = IN_I32_EXTEND8_S;     break; 
        case TS_SHORT:                           in = IN_I32_EXTEND16_S;    break; 
        case TS_INT:  case TS_LONG:/* wasm32 */                             break;
        case TS_BOOL: case TS_UCHAR: case TS_USHORT:
        case TS_UINT: case TS_ULONG:/* wasm32 */ /* nop */                  break;
        case TS_LLONG:                           in = IN_I32_WRAP_I64;      break;
        case TS_ULLONG:                          in = IN_I32_WRAP_I64;      break;
        default:;
      } break;
    case TS_LLONG:
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_I64_TRUNC_F64_S;   break;
        case TS_FLOAT:                           in = IN_I64_TRUNC_F32_S;   break;
        case TS_CHAR:  in0 = IN_I32_EXTEND8_S;   in = IN_I64_EXTEND_I32_S;  break; 
        case TS_SHORT: in0 = IN_I32_EXTEND16_S;  in = IN_I64_EXTEND_I32_S;  break; 
        case TS_INT:  case TS_LONG:/* wasm32 */  in = IN_I64_EXTEND_I32_S;  break;
        case TS_BOOL: case TS_UCHAR: case TS_USHORT:
        case TS_UINT: case TS_ULONG:/* wasm32 */ in = IN_I64_EXTEND_I32_U;  break;
        case TS_LLONG:                           /* nop */                  break;
        case TS_ULLONG:                          /* nop */                  break;
        default:;
      } break;
    case TS_ULLONG:
      switch (tsfrom) {
        case TS_DOUBLE:                          in = IN_I64_TRUNC_F64_U;   break;
        case TS_FLOAT:                           in = IN_I64_TRUNC_F32_U;   break;
        case TS_CHAR:  in0 = IN_I32_EXTEND8_S;   in = IN_I64_EXTEND_I32_S;  break; 
        case TS_SHORT: in0 = IN_I32_EXTEND16_S;  in = IN_I64_EXTEND_I32_S;  break; 
        case TS_INT:  case TS_LONG:/* wasm32 */  in = IN_I64_EXTEND_I32_S;  break;
        case TS_BOOL: case TS_UCHAR: case TS_USHORT:
        case TS_UINT: case TS_ULONG:/* wasm32 */ in = IN_I64_EXTEND_I32_U;  break;
        case TS_LLONG:                           /* nop */                  break;
        case TS_ULLONG:                          /* nop */                  break;
        default:;
      } break;
    default:;
  }
  if (in0) icbnewbk(pdata)->in = in0;
  if (in) icbnewbk(pdata)->in = in;
}

/* produce load instruction to fetch from a scalar pointer */
static inscode_t *asm_load(ts_t tsto, ts_t tsfrom, unsigned off, icbuf_t *pdata)
{ /* tsfrom may be narrow, tsto is integral-promoted */
  instr_t in = 0; unsigned align = 0; inscode_t *pin;
  switch (tsto) { /* TS_PTR or result of ts_integral_promote(tsfrom) */
    case TS_PTR: case TS_LONG: case TS_ULONG: /* wasm32 */
    case TS_INT: case TS_UINT: case TS_ENUM:
      switch (tsfrom) {
        case TS_CHAR:   in = IN_I32_LOAD8_S;  align = 0; break;
        case TS_BOOL:   /* stored as single byte; assume 0/1 */
        case TS_UCHAR:  in = IN_I32_LOAD8_U;  align = 0; break;
        case TS_SHORT:  in = IN_I32_LOAD16_S; align = 1; break;
        case TS_USHORT: in = IN_I32_LOAD16_U; align = 1; break;
        case TS_PTR:    case TS_LONG:   case TS_ULONG: /* wasm32 assumed */
        case TS_INT:    case TS_UINT:   case TS_ENUM:
          in = IN_I32_LOAD; align = 2; break;
        default: assert(false);
      } break;
    case TS_LLONG: case TS_ULLONG:
      switch (tsfrom) {
        case TS_CHAR:   in = IN_I64_LOAD8_S;  align = 0; break;
        case TS_BOOL:   /* stored as single byte; assume 0/1 */
        case TS_UCHAR:  in = IN_I64_LOAD8_U;  align = 0; break;
        case TS_SHORT:  in = IN_I64_LOAD16_S; align = 1; break;
        case TS_USHORT: in = IN_I64_LOAD16_U; align = 1; break;
        case TS_PTR:    case TS_ULONG: /* wasm32 assumed */
        case TS_UINT:   in = IN_I64_LOAD32_U; align = 2; break; 
        case TS_LONG:   /* wasm32 assumed */
        case TS_INT:    case TS_ENUM:   in = IN_I64_LOAD32_S; align = 2; break; 
        case TS_LLONG:  case TS_ULLONG: in = IN_I64_LOAD; align = 3; break;
        default: assert(false);
      } break;
    case TS_FLOAT:
      switch (tsfrom) {
        case TS_FLOAT:  in = IN_F32_LOAD; align = 2; break;
        default:;
      } break;
    case TS_DOUBLE:
      switch (tsfrom) {
        case TS_DOUBLE: in = IN_F64_LOAD; align = 3; break;
        default:;
      } break;
    default: assert(false);
  }
  assert(in);
  pin = icbnewbk(pdata); 
  pin->in = in; pin->arg.u = off; pin->arg2.u = align;
  return pin;
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
    default: assert(false);
  }
  assert(in);
  *psic = *plic; psic->in = in;
}

/* produce store instruction to write thru a scalar pointer */
static inscode_t *asm_store(ts_t tsto, ts_t tsfrom, unsigned off, icbuf_t *pdata)
{ /* tsto may be narrow, tsfrom is integral-promoted */
  inscode_t *plic = asm_load(tsfrom, tsto, off, pdata), sic;
  asm_instr_load_to_store(plic, &sic);
  *plic = sic; return plic;
}

/* produce cast instructions as needed to convert tsn to cast_compatible ttn type */
static void asm_cast(const node_t *ptpn, const node_t *ptan, icbuf_t *pdata)
{
  if (ptpn->ts == TS_ENUM && ts_numerical(ptan->ts)) {
    asm_numerical_cast(TS_INT, ptan->ts, pdata);
  } else if (ts_numerical(ptpn->ts) && ptan->ts == TS_ENUM) {
    asm_numerical_cast(ptpn->ts, TS_INT, pdata);
  } else if (ts_numerical(ptpn->ts) && ts_numerical(ptan->ts)) {
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
        default:;
      } break;
    case TS_LLONG: case TS_ULLONG: 
      switch (op) {
        case TT_PLUS: return true;
        case TT_NOT: icbnewbk(pdata)->in = IN_I64_EQZ; return true;
        default:;
      } break;
    case TS_FLOAT:
      switch (op) {
        case TT_PLUS: return true;
        case TT_MINUS: icbnewbk(pdata)->in = IN_F32_NEG; return true;
        default:;
      } break;
    case TS_DOUBLE: 
      switch (op) {
        case TT_PLUS: return true;
        case TT_MINUS: icbnewbk(pdata)->in = IN_F64_NEG; return true;
        default:;
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
        default:;
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
        default:;
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
        default:;
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
        default:;
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
        default:;
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
        default:;
      } break;
    default: assert(false);
  }
  if (in == 0) return false;
  icbnewbk(pdata)->in = in; 
  return true;
}

/* push instruction to pdata's front */
static void asm_pushfr(icbuf_t *pdata, inscode_t *pic)
{
  *icbnewfr(pdata) = *pic;
}

/* push instruction to pdata's end */
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


/* acode node predicates and helpers */

/* type of acode node as NT_TYPE node */
static node_t *acode_type(node_t *pcn)
{
  node_t *ptn; assert(pcn);
  if (pcn->nt != NT_ACODE) {
    neprintf(pcn, "compile: asm code expected");
  }
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
      default:;
    }
  }  
  return TS_VOID; /* 0 */
}

/* check if this node is an empty code */
static bool acode_empty(node_t *pan)
{
  return pan->nt == NT_ACODE && icblen(&pan->data) == 0;
}

/* check if this node is a func ref code */
static bool acode_func_ref(node_t *pan)
{
  if (pan->nt == NT_ACODE && icblen(&pan->data) == 1) {
    instr_t in = icbref(&pan->data, 0)->in;
    return (in == IN_REF_FUNC);
  } 
  return false;
}

/* check if this node is a var get; returns 0 or instruction code */
static int acode_rval_get(node_t *pan)
{
  if (pan->nt == NT_ACODE && icblen(&pan->data) == 1) {
    instr_t in = icbref(&pan->data, 0)->in;
    return (in == IN_GLOBAL_GET || in == IN_LOCAL_GET) ? in : 0;
  } 
  return 0;
}

/* check if this node is memory load; returns 0 or instruction code */
static int acode_rval_load(node_t *pan)
{
  if (pan->nt == NT_ACODE && icblen(&pan->data) >= 1) {
    instr_t in = icbref(&pan->data, icblen(&pan->data)-1)->in;
    return (in >= IN_I32_LOAD && in <= IN_I64_LOAD32_U) ? in : 0;
  } 
  return 0;
}

/* push argument-less instruction in to the end of pcn code */
static node_t *acode_pushin(node_t *pcn, instr_t in)
{
  icbnewbk(&pcn->data)->in = in;
  return pcn;
}

/* push signed-argument instruction in to the end of pcn code */
static node_t *acode_pushin_iarg(node_t *pcn, instr_t in, long long i)
{
  inscode_t *pic = icbnewbk(&pcn->data); pic->in = in; 
  pic->arg.i = i;
  return pcn;
}

/* push unsigned-argument instruction in to the end of pcn code */
static node_t *acode_pushin_uarg(node_t *pcn, instr_t in, unsigned long long u)
{
  inscode_t *pic = icbnewbk(&pcn->data); pic->in = in; 
  pic->arg.u = u;
  return pcn;
}

/* push instruction with id to the end of pcn code */
static node_t *acode_pushin_id(node_t *pcn, instr_t in, sym_t id)
{
  inscode_t *pic = icbnewbk(&pcn->data); pic->in = in; 
  pic->id = id;
  return pcn;
}

/* push instruction with id and mod to the end of pcn code */
static node_t *acode_pushin_id_mod(node_t *pcn, instr_t in, sym_t id, sym_t mod)
{
  inscode_t *pic = icbnewbk(&pcn->data); pic->in = in; 
  pic->id = id; pic->arg2.mod = mod;
  return pcn;
}

/* push instruction with id and uarg to the end of pcn code */
static node_t *acode_pushin_id_uarg(node_t *pcn, instr_t in, sym_t id, unsigned u)
{
  inscode_t *pic = icbnewbk(&pcn->data); pic->in = in; 
  pic->id = id; pic->arg.u = u;
  return pcn;
}

/* push instruction with id and mod to the end of pcn code */
static node_t *acode_pushin_id_mod_iarg(node_t *pcn, instr_t in, sym_t id, sym_t mod, int i)
{
  inscode_t *pic = icbnewbk(&pcn->data); pic->in = in; 
  pic->id = id; pic->arg2.mod = mod; pic->arg.i = i;
  return pcn;
}


/* copy contents of pan code to the end of pcn code */
static node_t *acode_copyin(node_t *pcn, const node_t *pan)
{
  ndcpy(ndnewbk(pcn), pan);
  return acode_pushin(pcn, IN_PLACEHOLDER);
}

/* move contents of pan code to the end of pcn code */
static node_t *acode_swapin(node_t *pcn, node_t *pan)
{
  ndswap(ndnewbk(pcn), pan); 
  return acode_pushin(pcn, IN_PLACEHOLDER);
}


/* compiler helpers; prn is original reference node for errors */

/* compile a cast expression (can produce empty code sequence) */
static node_t *compile_cast(node_t *prn, const node_t *ptn, node_t *pan)
{
  node_t *pcn, *patn = acode_type(pan);
  if (ptn->ts == TS_VOID) ; /* ok, any value can be dropped */
  else if (!cast_compatible(ptn, patn)) {
    n2eprintf(prn, pan, "compile: impossible cast");
  }
  pcn = npnewcode(prn); ndcpy(ndnewbk(pcn), ptn);
  acode_swapin(pcn, pan);
  if (ptn->ts == TS_VOID) acode_pushin(pcn, IN_DROP);
  else asm_cast(ptn, patn, &pcn->data);
  return pcn;
}

/* compile a numerical literal expression */
static node_t *compile_numlit(node_t *prn, ts_t ts, numval_t *pnv)
{
  node_t *pcn = npnewcode(prn); ndsettype(ndnewbk(pcn), ts); 
  asm_numerical_const(ts, pnv, icbnewbk(&pcn->data));
  return pcn;
}

/* compile id reference (as rval); ptn is type node (copied), mod != 0 for globals */
static node_t *compile_idref(node_t *prn, sym_t mod, sym_t name, const node_t *ptn)
{
  inscode_t *pic; node_t *pcn = npnewcode(prn); ndpushbk(pcn, ptn);
  pic = icbnewbk(&pcn->data); pic->id = name;
  if (mod) { 
    pic->arg2.mod = mod; 
    switch (ptn->ts) {
      case TS_FUNCTION: 
        pic->in = IN_REF_FUNC;
        break;
      case TS_ARRAY: case TS_STRUCT: case TS_UNION:
        pic->in = IN_REF_DATA;
        break;
      default:
        pic->in = IN_GLOBAL_GET; 
        break;
    }
  } else {
    pic->in = IN_LOCAL_GET;
  }
  return pcn;
}

/* compile regular binary operator expressions (+ - * / % << >> = != < <= > >=) */
static node_t *compile_binary(node_t *prn, node_t *pan1, tt_t op, node_t *pan2)
{
  node_t *patn1 = acode_type(pan1), *patn2 = acode_type(pan2); 
  ts_t ts1 = patn1->ts, ts2 = patn2->ts;
  if (((ts1 == TS_ENUM && ts2 == TS_ENUM && 
        (patn1->name == 0 || patn2->name == 0 || patn1->name == patn2->name)) ||
       (ts1 == TS_ENUM && ts_numerical(ts2)) || (ts_numerical(ts1) && ts2 == TS_ENUM))
      && op >= TT_LT && op <= TT_NE) {
    if (ts1 == TS_ENUM) ts1 = TS_INT;
    if (ts2 == TS_ENUM) ts2 = TS_INT;
  }
  if (ts_numerical(ts1) && ts_numerical(ts2)) {
    ts_t rts = ts_arith_common(ts1, ts2); 
    node_t rtn = mknd(), *prtn, *pcn = npnewcode(prn); ndsettype(&rtn, rts);
    if (rts != ts1) pan1 = compile_cast(prn, &rtn, pan1); /* fixme: constant fold oportunity */
    if (rts != ts2) pan2 = compile_cast(prn, &rtn, pan2); /* fixme: constant fold oportunity */
    prtn = ndcpy(ndnewbk(pcn), &rtn); if (op >= TT_LT && op <= TT_NE) prtn->ts = TS_INT;
    acode_swapin(pcn, pan1); acode_swapin(pcn, pan2);
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
        node_t *p1n = compile_cast(prn, acode_type(pmn), pan1);
        node_t *p2n = compile_cast(prn, acode_type(pmn), pan2);
        node_t *pdn = compile_binary(prn, p1n, TT_MINUS, p2n);
        return compile_binary(prn, pdn, TT_SLASH, pmn); 
      } else { /* byte pointer, no need to scale */
        node_t *ptn = ndsettype(npalloc(), TS_LONG);
        node_t *p1n = compile_cast(prn, ptn, pan1);
        node_t *p2n = compile_cast(prn, ptn, pan2);
        return compile_binary(prn, p1n, TT_MINUS, p2n);
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
      asm_numerical_const(rts, (v.i = 0, &v), icbnewbk(&pcn->data)); 
      acode_swapin(pcn, pan);
      if (rts != ts) asm_numerical_cast(rts, ts, &pcn->data);
      if (asm_binary(rts, TT_MINUS, &pcn->data)) return pcn;
    } else if (op == TT_TILDE && rts != TS_FLOAT && rts != TS_DOUBLE) {
      /* no INV instructions for integers; ~x => x^-1 */
      asm_numerical_const(rts, (v.i = -1, &v), icbnewbk(&pcn->data)); 
      acode_swapin(pcn, pan);
      if (rts != ts) asm_numerical_cast(rts, ts, &pcn->data);
      if (asm_binary(rts, TT_XOR, &pcn->data)) return pcn;
    } else if (op == TT_NOT && (rts == TS_FLOAT || rts == TS_DOUBLE)) {
      /* no EQZ instructions for floats; !x => x==0.0 */
      if (rts == TS_FLOAT) v.f = 0.0f; else v.d = 0.0;
      asm_numerical_const(rts, &v, icbnewbk(&pcn->data)); 
      acode_swapin(pcn, pan);
      if (rts != ts) asm_numerical_cast(rts, ts, &pcn->data);
      if (asm_binary(rts, TT_EQ, &pcn->data)) return pcn;
    } else { /* regular case */
      acode_swapin(pcn, pan);
      if (rts != ts) asm_numerical_cast(rts, ts, &pcn->data);
      if (asm_unary(rts, op, &pcn->data)) return pcn;
    }
  } else if (ts == TS_PTR && op == TT_NOT) {
    node_t tn = mknd(), *ptn = ndsettype(&tn, TS_ULONG);
    pcn = compile_unary(prn, op, compile_cast(prn, ptn, pan));
    ndfini(&tn); return pcn;
  }
  n2eprintf(ndref(prn, 0), prn, "invalid type for unary %s operation", op_name(op));
  return NULL; /* won't happen */
}

/* compile x to serve in a boolean test position */
node_t *compile_booltest(node_t *prn, node_t *pan)
{
  node_t *patn = acode_type(pan);
  if (patn->ts < TS_INT || patn->ts > TS_ULONG) {
    numval_t v; node_t *pzn = compile_numlit(prn, TS_ULONG, (v.i = 0, &v));
    if (patn->ts == TS_PTR) pan = compile_cast(prn, acode_type(pzn), pan);
    return compile_binary(prn, pan, TT_NE, pzn);
  } 
  /* else good as-is */
  return pan;
}

/* compile x to serve as an integer selector */
node_t *compile_intsel(node_t *prn, node_t *pan)
{
  node_t *patn = acode_type(pan);
  if (patn->ts != TS_INT) {
    node_t tn = mknd(); ndsettype(&tn, TS_INT);
    if (!assign_compatible(&tn, patn)) n2eprintf(pan, prn, "integer selector expression is expected"); 
    if (!same_type(&tn, patn)) pan = compile_cast(prn, &tn, pan);
    ndfini(&tn);
  }
  return pan;
}

/* check for pointer type compatibility; returns common type or NULL */
static node_t *common_ptr_type(node_t *patn1, node_t *patn2)
{
  assert(patn1->ts == TS_PTR && patn2->ts == TS_PTR);
  assert(ndlen(patn1) == 1 && ndlen(patn2) == 1);
  if (ndref(patn1, 0)->ts == TS_VOID) return patn2;
  if (ndref(patn2, 0)->ts == TS_VOID) return patn1;
  if (same_type(ndref(patn1, 0), ndref(patn2, 0))) return patn1;
  return NULL;
}

/* compile x ? y : z operator */
static node_t *compile_cond(node_t *prn, node_t *pan1, node_t *pan2, node_t *pan3)
{
  node_t *patn1 = acode_type(pan1), *patn2 = acode_type(pan2), *patn3 = acode_type(pan3); 
  node_t nt = mknd(), *pcn = npnewcode(prn), *pctn = NULL; bool use_select;
  if (ts_numerical(patn2->ts) && ts_numerical(patn3->ts)) 
    pctn = ndsettype(&nt, ts_arith_common(patn2->ts, patn3->ts));
  else if (patn2->ts == TS_PTR && patn3->ts == TS_PTR && (pctn = common_ptr_type(patn2, patn3)) != NULL) /* ok */; 
  else if (same_type(patn2, patn3)) pctn = patn2;
  else neprintf(prn, "incompatible types of logical operator branches");
  use_select = acode_simple_noeff(pan2) && acode_simple_noeff(pan3);
  if (!same_type(pctn, patn2)) pan2 = compile_cast(prn, pctn, pan2);
  if (!same_type(pctn, patn3)) pan3 = compile_cast(prn, pctn, pan3);
  pan1 = compile_booltest(prn, pan1); /* add !=0 if not i32 */
  ndcpy(ndnewbk(pcn), pctn);
  if (use_select) {
    /* both branches can be computed cheaply with no effects; use SELECT */
    acode_swapin(pcn, pan2); 
    acode_swapin(pcn, pan3);
    acode_swapin(pcn, pan1); 
    acode_pushin(pcn, IN_SELECT);
  } else { 
    /* fixme: 'if'-'else' block is needed */
    unsigned bt = ts_to_blocktype(pctn->ts);
    if (!bt) neprintf(prn, "impossible type of logical operator result"); 
    assert(bt == BT_VOID || (bt >= VT_F64 && bt <= VT_I32));
    acode_swapin(pcn, pan1);
    acode_pushin_uarg(pcn, IN_IF, bt);
    acode_swapin(pcn, pan2);
    acode_pushin(pcn, IN_ELSE);
    acode_swapin(pcn, pan3);
    acode_pushin(pcn, IN_END);
  }
  ndfini(&nt); 
  return pcn;
}

/* compile *x */
static node_t *compile_ataddr(node_t *prn, node_t *pan)
{
  node_t *patn = acode_type(pan), *petn = NULL;
  if (patn->ts == TS_PTR) petn = ndref(patn, 0);
  else neprintf(prn, "cannot dereference non-pointer expression");  
  if (ts_bulk(petn->ts)) {
    /* {bulk_t* | x} => {bulk_t | x} */
    lift_arg0(patn);
    return pan;
  } if (petn->ts == TS_FUNCTION) {
    /* {func_t* | x} => {func_t | x} */
    lift_arg0(patn);
    return pan;
  } else { 
    /* {scalar_t* | x} => {IP(scalar_t) | {scalar_t* | x} fetch} */
    ts_t oets = petn->ts, pets = ts_numerical(oets) ? ts_integral_promote(oets) : oets;
    node_t *pcn = npnewcode(prn), *ptn = ndpushbk(pcn, petn);
    ptn->ts = pets;
    acode_swapin(pcn, pan);
    asm_load(pets, oets, 0, &pcn->data);
    return pcn;
  }
  return NULL; /* never */
} 

/* compile x.fld; expects bulk struct/union type */
static node_t *compile_dot(node_t *prn, node_t *pan, sym_t fld)
{
  node_t *patn = acode_type(pan), *petn = NULL; 
  size_t off; node_t *pcn = npnewcode(prn); 
  if (patn->ts != TS_STRUCT && patn->ts != TS_UNION)
    neprintf(prn, "cannot take field of non-struct/union expression");
  off = measure_offset(patn, prn, fld, &petn); assert(petn);
  ndpushbk(pcn, petn); /* type of field is return type */
  if (ts_bulk(petn->ts)) { /* field type is fbulk_t  */
    /* {bulk_t | x} => {fbulk_t | {bulk_t | x} +off?} */
    acode_swapin(pcn, pan);
    if (off > 0) { acode_pushin_uarg(pcn, IN_I32_CONST, off); acode_pushin(pcn, IN_I32_ADD); }
  } else { /* field type is fscalar_t */
    /* {bulk_t | x} => {IP(fscalar_t) | {fscalar_t* | {bulk_t | x} +off?} fetch} */
    node_t *psn = npnewcode(prn); wrap_type_pointer(ndpushbk(psn, petn));
    acode_swapin(psn, pan);
    if (off > 0) { acode_pushin_uarg(psn, IN_I32_CONST, off); acode_pushin(psn, IN_I32_ADD); }
    acode_swapin(pcn, psn); petn = acode_type(pcn);
    if (ts_numerical(petn->ts)) {
      ts_t ipts = ts_integral_promote(petn->ts);
      asm_load(ipts, petn->ts, 0, &pcn->data);
      petn->ts = ipts;
    } else {
      asm_load(petn->ts, petn->ts, 0, &pcn->data);
    }
  }
  return pcn;
}

/* compile &x */
static node_t *compile_addrof(node_t *prn, node_t *pan)
{
  node_t *patn = acode_type(pan); 
  if (ts_bulk(patn->ts)) { 
    /* {bulk_t | x} => {bulk_t* | x} */
    wrap_type_pointer(patn); 
  } else if (patn->ts == TS_FUNCTION) { 
    /* {func_t | x} => {func_t* | x} */
    wrap_type_pointer(patn);
  } else {
    /* {IP(scalar_t) | {scalar_t* | x} fetch} => {scalar_t* | x} */
    if (ndlen(pan) == 2 && icblen(&pan->data) == 2 && acode_rval_load(pan)) {
      assert(icbref(&pan->data, 1)->arg.u == 0); /* no offset! */
      lift_arg1(pan); /* flatten node into its ndref(1) */
      assert(acode_type(pan)->ts == TS_PTR);
    } else {
      neprintf(prn, "cannot take address of expression");
    }   
  }
  return pan;
} 

/* compile x[y] */
static node_t *compile_subscript(node_t *prn, node_t *pana, node_t *pani)
{
  node_t *pata = acode_type(pana), *pati = acode_type(pani);
  if (pata->ts == TS_ARRAY) {
    /* automatically downcast type to type of element pointer */
    assert(ndlen(pata) == 2); ndrem(pata, 1); /* no static/dynamic range check */
    pata->ts = TS_PTR;
  }
  if (pata->ts == TS_PTR || pati->ts == TS_PTR) { 
    /* x[y] => *(x + y) */
    return compile_ataddr(prn, compile_binary(prn, pana, TT_PLUS, pani));
  } 
  neprintf(prn, "cannot compile subscript expression");
  return NULL; /* never */
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
      neprintf(prn, "can't modify lval: unexpected type on the right (need cast?)");
    if (pctn && !cast_compatible(pctn, acode_type(pvn)))
      neprintf(prn, "can't modify lval: impossible narrowing cast");
    if (pctn && !same_type(pctn, acode_type(pvn))) 
      pvn = compile_cast(prn, pctn, pvn); 
    if (!same_type(acode_type(pan), acode_type(pvn))) 
      pvn = compile_cast(prn, acode_type(pan), pvn); 
    acode_swapin(pan, pvn);
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
    /* if 'post' ptr val stays on stack, dropped otherwise */
    acode_pushin_id(pan, post ? IN_LOCAL_TEE : IN_LOCAL_SET, pname);
    if (post) asm_pushbk(&pan->data, &lic); /* old val on stack */
    ptn = wrap_type_pointer(npdup(acode_type(pan))); 
    pdn = compile_idref(prn, 0/*local*/, pname, ptn);
    acode_copyin(pan, pdn);
    if (op) {
      node_t *pln = npdup(pdn); 
      asm_pushbk(&pln->data, &lic); lift_arg0(acode_type(pln));
      pvn = compile_binary(prn, pln, op, pvn); /* new val on stack */
    }
    if (!assign_compatible(acode_type(pan), pctn ? pctn : acode_type(pvn)))
      neprintf(prn, "can't modify lval: unexpected type on the right (need cast?)");
    if (pctn && !cast_compatible(pctn, acode_type(pvn)))
      neprintf(prn, "can't modify lval: impossible narrowing cast");
    if (pctn && !same_type(pctn, acode_type(pvn)))
      pvn = compile_cast(prn, pctn, pvn); 
    if (!same_type(acode_type(pan), acode_type(pvn))) 
      pvn = compile_cast(prn, acode_type(pan), pvn); 
    acode_swapin(pan, pvn);
    asm_instr_load_to_store(&lic, &sic);
    if (post) { /* new val removed, old val remains */
      asm_pushbk(&pan->data, &sic);
    } else { /* re-fetch to get new val */
      asm_pushbk(&pan->data, &sic);
      acode_copyin(pan, pdn);
      asm_pushbk(&pan->data, &lic);
    }
    pic = icbnewfr(&pan->data); pic->in = IN_REGDECL; 
    pic->id = pname; pic->arg.u = VT_I32; /* wasm32 */ 
    return pan;    
  }
  neprintf(prn, "not a valid lvalue");
  return NULL;
} 

/* compile call expression; pdn != NULL for suspected bulk return call */
static node_t *compile_call(node_t *prn, node_t *pfn, buf_t *pab, node_t *pdn)
{
  node_t *pcn = npnewcode(prn), *pftn = acode_type(pfn), *psn; size_t i;
  inscode_t cic; size_t alen = ndlen(pftn); bool etc = false;
  if (pftn->ts != TS_FUNCTION) 
    n2eprintf(ndref(prn, 0), prn, "can't call non-function type (function pointers need to be dereferenced)");
  if (alen > 1 && (psn = ndref(pftn, alen-1))->nt == NT_VARDECL
    && ndlen(psn) == 1 && ndref(psn, 0)->ts == TS_ETC) { 
    etc = true;
    if (ndlen(pftn) > buflen(pab)+2)
      n2eprintf(ndref(prn, 0), prn, "%d-or-more-parameter function called with %d arguments",
        (int)ndlen(pftn)-2, (int)buflen(pab));
  } else if (ndlen(pftn) != buflen(pab)+1)
    n2eprintf(ndref(prn, 0), prn, "%d-parameter function called with %d arguments",
      (int)ndlen(pftn)-1, (int)buflen(pab));
  if (ts_bulk(ndref(pftn, 0)->ts)) {
    if (!pdn) n2eprintf(ndref(prn, 0), prn, "no lval to accept bulk return value"); 
    /* adjust to bulk return convention */
    pftn = npdup(pftn); wrap_type_pointer(ndref(pftn, 0));
    ndsettype(ndnewfr(pftn), TS_VOID);
    *(node_t **)bufins(pab, 0) = pdn;
    ndsettype(ndnewbk(pcn), TS_VOID);
  } else { 
    assert(!pdn);
    ndpushbk(pcn, ndref(pftn, 0));
  }
  if (acode_func_ref(pfn)) {
    inscode_t ic; asm_getbk(&pfn->data, &ic);
    assert(ic.in == IN_REF_FUNC); 
    cic.in = IN_CALL; cic.id = ic.id; cic.arg2 = ic.arg2; /* mod */
  } else {
    funcsig_t fs; fsinit(&fs);
    cic.in = IN_CALL_INDIRECT;
    cic.arg.u = fsintern(&g_funcsigs, ftn2fsig(pftn, &fs));
    cic.id = 0; cic.arg2.u = 0;
    fsfini(&fs);
  }
  for (i = 0; i < buflen(pab) && (!etc || i < ndlen(pftn)-2); ++i) {
    node_t **ppani = bufref(pab, i), *pani = *ppani;
    node_t *ptni = acode_type(pani), *pftni = ndref(pftn, i+1);
    node_t nd; bool nd_inited = false;
    if (pftni->nt == NT_VARDECL) pftni = ndref(pftni, 0);
    /* see if we need to promote param type */
    if (ts_numerical(pftni->ts) && pftni->ts < TS_INT) {
      ndicpy(&nd, pftni); nd_inited = true;
      pftni = &nd; pftni->ts = ts_integral_promote(pftni->ts);
    }
    if (!assign_compatible(pftni, ptni)) {
      n2eprintf(pani, prn, "can't pass argument[%d]: unexpected type", i);
    }
    if (!same_type(pftni, ptni)) pani = compile_cast(prn, pftni, pani);
    acode_swapin(pcn, pani); /* leaves arg on stack */
    if (nd_inited) ndfini(&nd);
  }
  if (etc && i <= buflen(pab)) {
    /* collect remaining arguments into a stack array of va_arg_t */
    sym_t pname = rpalloc(VT_I32); /* arg frame ptr (wasm32) */
    node_t tn = mknd(); /* for temporary type conversions */
    /* frame is an array of uint64s (va_arg_t), aligned to 16 */
    size_t asz = 8, nargs = buflen(pab)-i, framesz = (size_t)((nargs*asz + 15) & ~0xFULL), basei;
    inscode_t *pic = icbnewfr(&pcn->data); pic->in = IN_REGDECL;
    pic->id = pname; pic->arg.u = VT_I32; /* wasm32 pointer */
    acode_pushin_id_mod(pcn, IN_GLOBAL_GET, g_sp_id, g_crt_mod);
    acode_pushin_uarg(pcn, IN_I32_CONST, framesz);
    acode_pushin(pcn, IN_I32_SUB);
    acode_pushin_id(pcn, IN_LOCAL_TEE, pname);
    acode_pushin_id_mod(pcn, IN_GLOBAL_SET, g_sp_id, g_crt_mod); 
    /* start of va_arg_t[nargs] array is now in pname, fill it */
    for (basei = i; i < buflen(pab); ++i) {
      node_t **ppani = bufref(pab, i), *pani = *ppani;
      node_t *ptni = acode_type(pani); size_t size, align;
      /* first, make C-like promotions for ... arguments */
      if (ts_numerical(ptni->ts) && ptni->ts < TS_INT) {
        ndcpy(&tn, ptni); tn.ts = TS_INT; ptni = &tn;
        pani = compile_cast(prn, ptni, pani);
      } else if (ptni->ts == TS_FLOAT) {
        ndcpy(&tn, ptni); tn.ts = TS_DOUBLE; ptni = &tn;
        pani = compile_cast(prn, ptni, pani);
      } else if (ptni->ts == TS_ARRAY) {
        ndcpy(&tn, ptni); tn.ts = TS_PTR; ptni = &tn;
      }
      /* now see what we have got after that */
      measure_type(ptni, prn, &size, &align, 0);
      if (ts_bulk(ptni->ts) || size > asz || align > asz)
        n2eprintf(pani, prn, "can't pass argument[%d]: unsupported type for ... call", i);
      if (ts_numerical(ptni->ts)) {
        if (ptni->ts != ts_integral_promote(ptni->ts))
          n2eprintf(pani, prn, "can't pass argument[%d]: non-promoted type for ... call", i);
      }
      acode_pushin_id(pcn, IN_LOCAL_GET, pname);
      acode_swapin(pcn, pani); /* arg on stack */
      asm_store(ptni->ts, ptni->ts, (unsigned)((i-basei)*asz), &pcn->data);
    }
    acode_pushin_id(pcn, IN_LOCAL_GET, pname); 
    /* put the call instruction followed by sp restore */
    if (cic.in == IN_CALL_INDIRECT) acode_swapin(pcn, pfn); /* func on stack */ 
    asm_pushbk(&pcn->data, &cic);
    acode_pushin_id(pcn, IN_LOCAL_GET, pname);
    acode_pushin_id_mod(pcn, IN_GLOBAL_SET, g_sp_id, g_crt_mod); 
    ndfini(&tn);
  } else {
    /* put the call instruction */
    if (cic.in == IN_CALL_INDIRECT) acode_swapin(pcn, pfn); /* func on stack */ 
    asm_pushbk(&pcn->data, &cic);
  }
  return pcn;
}

/* compile statement (i.e. drop value if any) */
static node_t *compile_stm(node_t *pcn)
{
  node_t *ptni = acode_type(pcn);
  if (ptni->ts != TS_VOID) {
    acode_pushin(pcn, IN_DROP);
    ndsettype(ptni, TS_VOID);
  }
  return pcn;
}

/* compile if statement */
static node_t *compile_if(node_t *prn, node_t *pan1, node_t *pan2, node_t *pan3)
{
  node_t *pcn = npnewcode(prn); ndsettype(ndnewbk(pcn), TS_VOID); 
  acode_swapin(pcn, compile_booltest(prn, pan1));
  acode_pushin_uarg(pcn, IN_IF, BT_VOID);
  acode_swapin(pcn, compile_stm(pan2));
  if (pan3) {
    node_t *pen = compile_stm(pan3);
    if (!acode_empty(pen)) { 
      acode_pushin(pcn, IN_ELSE);
      acode_swapin(pcn, pen);
    }
  }
  acode_pushin(pcn, IN_END);
  return pcn;
}

/* compile if-goto/goto statements; pan is condition or NULL */
static node_t *compile_branch(node_t *prn, node_t *pan, sym_t lname)
{ 
  node_t *pcn = npnewcode(prn); ndsettype(ndnewbk(pcn), TS_VOID);
  if (pan) {
    acode_swapin(pcn, compile_booltest(prn, pan));
    acode_pushin_id(pcn, IN_BR_IF, lname); 
  } else {
    acode_pushin_id(pcn, IN_BR, lname);
  }
  return pcn;
}

/* try to compile switch to a jump table (br_table) or return NULL if too sparse */
static node_t *compile_switch_table(node_t *prn, const node_t *pan, const node_t *pcv, size_t cc)
{
  size_t i, ti; sym_t dlname; int off, curidx, idx;
  const node_t *pni, *psn, *pgn; node_t *pcn; size_t dfillc = 0;
  assert(cc >= 1); /* default goto, followed by 0 or more case gotos */
  if (cc < 3) return NULL; /* we need at least 2 cases */
  pcn = npnewcode(prn); ndsettype(ndnewbk(pcn), TS_VOID);
  acode_copyin(pcn, pan);
  assert(pcv->nt == NT_DEFAULT && ndlen(pcv) == 1);
  pgn = ndcref(pcv, 0); assert(pgn->nt == NT_GOTO && pgn->name != 0);  
  dlname = pgn->name; /* default label */
  pni = pcv+1; assert(pni->nt == NT_CASE && ndlen(pni) == 2);
  psn = ndcref(pni, 0); assert(psn->nt == NT_LITERAL && psn->ts == TS_INT);
  if ((off = (int)psn->val.i) != 0) {
    acode_pushin_iarg(pcn, IN_I32_CONST, off); 
    acode_pushin(pcn, IN_I32_SUB);
  }
  ti = icblen(&pcn->data); /* index of br_table */
  acode_pushin_uarg(pcn, IN_BR_TABLE, 42); /* to be patched later */ 
  for (curidx = 0, i = 1; i < cc; ++i) {
    const node_t *pni = pcv+i; assert(pni->nt == NT_CASE && ndlen(pni) == 2);
    psn = ndcref(pni, 0); assert(psn->nt == NT_LITERAL && psn->ts == TS_INT);
    pgn = ndcref(pni, 1); assert(pgn->nt == NT_GOTO && pgn->name != 0);
    idx = (int)psn->val.i - off; assert(idx >= 0);
    while (curidx < idx) { /* fill gap with defaults */
      acode_pushin_id(pcn, IN_BR, dlname); /* NB: not an actual instruction! */
      ++curidx; ++dfillc;
      if (dfillc > cc) return NULL; /* nah.. */
    }
    acode_pushin_id(pcn, IN_BR, pgn->name); /* NB: not an actual instruction! */
    ++curidx;
  }
  icbref(&pcn->data, ti)->arg.u = (unsigned)curidx; /* patch size, add default */
  acode_pushin_id(pcn, IN_BR, dlname); /* NB: not an actual instruction! */
  return pcn;
}

/* compile switch to a bunch of eq/br_if instructions */
static node_t *compile_switch_ifs(node_t *prn, node_t *pan, node_t *pcv, size_t cc)
{
  size_t i; sym_t vname; inscode_t *pic;
  node_t *psn, *pgn, *pcn = npnewcode(prn); ndsettype(ndnewbk(pcn), TS_VOID);
  assert(cc >= 1); /* default goto, followed by 0 or more case gotos */
  if (acode_rval_get(pan) == IN_LOCAL_GET) {
    vname = icbref(&pan->data, 0)->id;
  } else { 
    vname = rpalloc(VT_I32); 
    pic = icbnewfr(&pan->data); pic->in = IN_REGDECL;
    pic->id = vname; pic->arg.u = VT_I32;
    acode_swapin(pcn, pan);
    acode_pushin_id(pcn, IN_LOCAL_SET, vname);
  }  
  for (i = 1; i < cc; ++i) { /* in case-val increasing order */
    node_t *pni = pcv+i; assert(pni->nt == NT_CASE && ndlen(pni) == 2);
    psn = ndref(pni, 0); assert(psn->nt == NT_LITERAL && psn->ts == TS_INT);
    pgn = ndref(pni, 1); assert(pgn->nt == NT_GOTO && pgn->name != 0);
    acode_swapin(pcn, compile_numlit(psn, TS_INT, &psn->val));
    acode_pushin_id(pcn, IN_LOCAL_GET, vname);
    acode_pushin(pcn, IN_I32_EQ);
    acode_pushin_id(pcn, IN_BR_IF, pgn->name); 
  }
  assert(pcv->nt == NT_DEFAULT && ndlen(pcv) == 1);
  pgn = ndref(pcv, 0); assert(pgn->nt == NT_GOTO && pgn->name != 0);
  acode_pushin_id(pcn, IN_BR, pgn->name);
  return pcn;
}

/* compile return statement; if bpid != 0, insert freea code */
static node_t *compile_return(node_t *prn, node_t *pan, const node_t *ptn, sym_t bpid)
{
  node_t *pcn = npnewcode(prn); ndsettype(ndnewbk(pcn), TS_VOID);
  assert(ptn && ptn->nt == NT_TYPE);
  if (pan) { /* got argument */
    node_t *patn = acode_type(pan);
    if (!assign_compatible(ptn, patn)) neprintf(prn, "unexpected returned type");
    if (!same_type(ptn, patn)) pan = compile_cast(prn, ptn, pan);
    acode_swapin(pcn, pan);
  } else { /* no argument */
    if (ptn->ts != TS_VOID) neprintf(prn, "return statement should return a value");
  }
  if (bpid) {
    acode_pushin_id(pcn, IN_LOCAL_GET, bpid);
    acode_pushin_id_mod(pcn, IN_GLOBAL_SET, g_sp_id, g_crt_mod);
  }
  acode_pushin(pcn, IN_RETURN);
  return pcn;
}

/* compile bulk assignment from two reference codes */
static node_t *compile_bulkasn(node_t *prn, node_t *pdan, node_t *psan)
{
  node_t *ptdan = acode_type(pdan), *ptsan = acode_type(psan), *ptn;
  node_t *pcn; size_t size, align;
  if (ts_bulk(ptdan->ts) && ts_bulk(ptsan->ts)) {
    if (!same_type(ptdan, ptsan)) neprintf(prn, "source and destination have different types"); 
    ptn = ptdan; /* bulk type -- otherwise memcopy isn't needed */ 
    pcn = npnewcode(prn);
    ndsettype(ndnewbk(pcn), TS_VOID); /* todo: chained bulk assignment nyi */
    measure_type(ptn, prn, &size, &align, 0);
    acode_swapin(pcn, pdan);
    acode_swapin(pcn, psan);
    acode_pushin_uarg(pcn, IN_I32_CONST, size);
    acode_pushin(pcn, IN_MEMORY_COPY);
    return pcn;
  }
  return NULL;
}

/* compile expr/statement (that is, convert it to asm tree); prib is var/reg info,
 * ret is return type for statements, NULL for expressions returning a value
 * NB: code that has bulk type actually produces a pointer (cf. arrays) */
static node_t *expr_compile(node_t *pn, buf_t *prib, const node_t *ret)
{
  node_t *pcn = NULL; numval_t v;
  /* fold constants that might have been introduced by wasmify */
  if (arithmetic_constant_expr(pn)) {
    node_t nd = mknd();
    if (ret && getwlevel() < 1) nwprintf(pn, "warning: constant value is not used"); 
    if (arithmetic_eval(pn, prib, &nd)) ndswap(&nd, pn);
    ndfini(&nd);
  }
  switch (pn->nt) {
    /* expressions */
    case NT_LITERAL: {
      if (ret && getwlevel() < 1) nwprintf(pn, "warning: literal value is not used");
      switch (pn->ts) {
        case TS_STRING: case TS_LSTRING: {
          ts_t ts; sym_t id; assert(pn->data.esz == 1);
          id = intern_strlit(pn); ts = (pn->ts == TS_STRING ? TS_CHAR : TS_INT);
          pcn = npnewcode(pn); wrap_type_pointer(ndsettype(ndnewbk(pcn), ts));
          acode_pushin_id_mod_iarg(pcn, IN_REF_DATA, id, g_curmod, 0);
        } break;
        default: {
          assert(ts_numerical(pn->ts));
          pcn = npnewcode(pn); ndsettype(ndnewbk(pcn), pn->ts);
          asm_numerical_const(pn->ts, &pn->val, icbnewbk(&pcn->data));
        } break;
      }
    } break; 
    case NT_IDENTIFIER: {
      sym_t mod = 0; const node_t *ptn = lookup_var_type(pn->name, prib, &mod);
      if (!ptn) neprintf(pn, "compile: undefined identifier %s", symname(pn->name));
      if (ret && getwlevel() < 1) nwprintf(pn, "warning: identifier value is not used");
      pcn = compile_idref(pn, mod, pn->name, ptn);
    } break;
    case NT_SUBSCRIPT: {
      node_t *pan1 = expr_compile(ndcref(pn, 0), prib, NULL);
      node_t *pan2 = expr_compile(ndcref(pn, 1), prib, NULL);
      pcn = compile_subscript(pn, pan1, pan2);
    } break;
    case NT_CALL: {
      size_t i; buf_t apb = mkbuf(sizeof(node_t*));
      node_t *pfn = expr_compile(ndref(pn, 0), prib, NULL);
      for (i = 1; i < ndlen(pn); ++i) 
        *(node_t**)bufnewbk(&apb) = expr_compile(ndref(pn, i), prib, NULL);
      pcn = compile_call(pn, pfn, &apb, NULL); /* regular call (no lval = on the left) */
      buffini(&apb);
    } break;
    case NT_INTRCALL: {
      switch (pn->intr) {
        case INTR_ALLOCA: {
          if (ndlen(pn) == 1) {
            node_t *pan = expr_compile(ndref(pn, 0), prib, NULL);
            node_t *ptni = acode_type(pan); inscode_t *pic; numval_t *pnv;
            ts_t ts = acode_const(pan, &pnv);
            if (ts == TS_INT) { /* sizeof?: calc frame size statically */
              pnv->i = (pnv->i + 15) & ~0xFLL;
              pic = icbnewfr(&pan->data); pic->in = IN_GLOBAL_GET; 
              pic->id = g_sp_id; pic->arg2.mod = g_crt_mod;
              acode_pushin(pan, IN_I32_SUB); 
              acode_pushin_id_mod(pan, IN_GLOBAL_SET, g_sp_id, g_crt_mod);
              acode_pushin_id_mod(pan, IN_GLOBAL_GET, g_sp_id, g_crt_mod);
            } else { /* calc frame size dynamically */
              sym_t sname = rpalloc(VT_I32), pname = rpalloc(VT_I32); /* wasm32 */
              pic = icbnewfr(&pan->data); pic->in = IN_REGDECL; pic->id = sname; pic->arg.u = VT_I32;
              if (!ts_numerical(ptni->ts) || ts_arith_common(ptni->ts, TS_ULONG) != TS_ULONG)
                neprintf(pn, "invalid alloca() intrinsic's argument");
              if (ptni->ts != TS_ULONG) asm_numerical_cast(TS_ULONG, ptni->ts, &pan->data);
              acode_pushin_uarg(pan, IN_I32_CONST, 15);  
              acode_pushin(pan, IN_I32_ADD); 
              acode_pushin_uarg(pan, IN_I32_CONST, 0xFFFFFFF0U);
              acode_pushin(pan, IN_I32_AND);   
              acode_pushin_id(pan, IN_LOCAL_SET, sname);
              acode_pushin_id_mod(pan, IN_GLOBAL_GET, g_sp_id, g_crt_mod);
              acode_pushin_id(pan, IN_LOCAL_GET, sname);
              acode_pushin(pan, IN_I32_SUB);
              acode_pushin_id_mod(pan, IN_GLOBAL_SET, g_sp_id, g_crt_mod);
              acode_pushin_id_mod(pan, IN_GLOBAL_GET, g_sp_id, g_crt_mod);
            }
            wrap_type_pointer(ndsettype(acode_type(pan), TS_VOID));
            pcn = pan;
          } else {
            neprintf(pn, "alloca() intrinsic expects one argument");
          }
        } break;
        case INTR_ASU32: {
          if (ndlen(pn) == 1) {
            node_t *pan = expr_compile(ndref(pn, 0), prib, NULL);
            node_t *ptni = acode_type(pan);
            if (ptni->ts != TS_FLOAT) neprintf(pn, "asuint32() argument should have 'float' type");
            acode_pushin(pan, IN_I32_REINTERPRET_F32);
            ptni->ts = TS_UINT;
            pcn = pan;
          } else {
            neprintf(pn, "asuint32() intrinsic expects one argument");
          }
        } break;
        case INTR_ASFLT: {
          if (ndlen(pn) == 1) {
            node_t *pan = expr_compile(ndref(pn, 0), prib, NULL);
            node_t *ptni = acode_type(pan);
            if (ptni->ts != TS_UINT) neprintf(pn, "asfloat() argument should have 'uint32_t' type");
            acode_pushin(pan, IN_F32_REINTERPRET_I32);
            ptni->ts = TS_FLOAT;
            pcn = pan;
          } else {
            neprintf(pn, "asfloat() intrinsic expects one argument");
          }
        } break;
        case INTR_ASU64: {
          if (ndlen(pn) == 1) {
            node_t *pan = expr_compile(ndref(pn, 0), prib, NULL);
            node_t *ptni = acode_type(pan);
            if (ptni->ts != TS_DOUBLE) neprintf(pn, "asuint64() argument should have 'double' type");
            acode_pushin(pan, IN_I64_REINTERPRET_F64);
            ptni->ts = TS_ULLONG;
            pcn = pan;
          } else {
            neprintf(pn, "asuint64() intrinsic expects one argument");
          }
        } break;
        case INTR_ASDBL: {
          if (ndlen(pn) == 1) {
            node_t *pan = expr_compile(ndref(pn, 0), prib, NULL);
            node_t *ptni = acode_type(pan);
            if (ptni->ts != TS_ULLONG) neprintf(pn, "asdouble() argument should have 'uint64_t' type");
            acode_pushin(pan, IN_F64_REINTERPRET_I64);
            ptni->ts = TS_DOUBLE;
            pcn = pan;
          } else {
            neprintf(pn, "asdouble() intrinsic expects one argument");
          }
        } break;
        case INTR_SIZEOF: case INTR_ALIGNOF: {
          size_t size, align; numval_t v;
          node_t *ptni = ndref(pn, 0);
          if (ptni->nt != NT_TYPE) ptni = acode_type(expr_compile(ptni, prib, NULL));
          measure_type(ptni, pn, &size, &align, 0);
          v.i = (int)(pn->intr == INTR_SIZEOF ? size : align);
          pcn = npnewcode(pn); ndsettype(ndnewbk(pcn), TS_INT);
          asm_numerical_const(TS_INT, &v, icbnewbk(&pcn->data));
        } break;
        case INTR_OFFSETOF: {
          size_t offset; numval_t v;
          node_t *ptni = ndref(pn, 0);
          if (ptni->nt != NT_TYPE) ptni = acode_type(expr_compile(ptni, prib, NULL));
          offset = measure_offset(ptni, pn, ndref(pn, 1)->name, NULL);
          v.i = (int)offset;
          pcn = npnewcode(pn); ndsettype(ndnewbk(pcn), TS_INT);
          asm_numerical_const(TS_INT, &v, icbnewbk(&pcn->data));
        } break;
        case INTR_VAETC: 
        case INTR_VAARG: assert(false); /* wasmified */
        case INTR_SASSERT: {
          check_static_assert(pn, prib);
          pcn = npnewcode(pn);
          ndsettype(ndnewbk(pcn), TS_VOID);
        } break;
        default: assert(false);
      }
    } break;
    case NT_CAST: {
      if (ndref(pn, 0)->ts == TS_ARRAY && ndref(pn, 1)->nt == NT_LITERAL) {
        /* treat as if it were a static display */
        size_t size, align;
        size_t pdidx = watieblen(&g_curpwm->exports);
        watie_t *pd = watiebnewbk(&g_curpwm->exports, IEK_DATA);
        node_t *ptn = ndref(pn, 0); assert(ptn->nt == NT_TYPE); 
        pd->mod = g_curmod; pd->id = internf("ds%d$", (int)pdidx);
        measure_type(ptn, pn, &size, &align, 0);
        bufresize(&pd->data, size); /* fills with zeroes */
        pd->align = (int)align; pd->mut = MT_CONST;
        pd = initialize_bulk_data(pdidx, pd, 0, ptn, ndref(pn, 1));
        pcn = npnewcode(pn); ndcpy(ndnewbk(pcn), ptn);
        acode_pushin_id_mod_iarg(pcn, IN_REF_DATA, pd->id, g_curmod, 0);
      } else {
        pcn = compile_cast(pn, ndref(pn, 0), expr_compile(ndref(pn, 1), prib, NULL));
      }
    } break;
    case NT_POSTFIX: {
      switch (pn->op) {
        case TT_DOT: {
          pcn = compile_dot(pn, expr_compile(ndref(pn, 0), prib, NULL), pn->name);  
        } break;
        case TT_PLUS_PLUS: case TT_MINUS_MINUS: {
          node_t *pn0 = ndref(pn, 0), *pln = (pn0->nt == NT_CAST) ? ndref(pn0, 1) : pn0;
          node_t *ptn = (pn0->nt == NT_CAST) ? ndref(pn0, 0) : NULL;
          node_t *pan = expr_compile(pln, prib, NULL);
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
            pcn = expr_compile(ndref(pn0, 0), prib, NULL);
          } else {
            pcn = compile_addrof(pn, expr_compile(pn0, prib, NULL));
          }
        } break;
        case TT_STAR: {
          if (pn0->nt == NT_PREFIX && pn0->op == TT_AND) {
            pcn = expr_compile(ndref(pn0, 0), prib, NULL);
          } else {
            pcn = compile_ataddr(pn, expr_compile(pn0, prib, NULL));
          }
        } break;
        case TT_PLUS_PLUS: case TT_MINUS_MINUS: {
          node_t *pln = (pn0->nt == NT_CAST) ? ndref(pn0, 1) : pn0;
          node_t *ptn = (pn0->nt == NT_CAST) ? ndref(pn0, 0) : NULL;
          node_t *pan = expr_compile(pln, prib, NULL);
          tt_t op = pn->op == TT_PLUS_PLUS ? TT_PLUS : TT_MINUS; 
          pcn = compile_asncombo(pn, pan, ptn, op, compile_numlit(pn, TS_INT, (v.i = 1, &v)), false); 
        } break;
        default: { /* normal unary op */
          node_t *pan = expr_compile(pn0, prib, NULL);
          pcn = compile_unary(pn, pn->op, pan);
        } break;
      }    
    } break;
    case NT_INFIX: {
      node_t *pan1 = expr_compile(ndcref(pn, 0), prib, NULL);
      node_t *pan2 = expr_compile(ndcref(pn, 1), prib, NULL);
      switch (pn->op) {
        case TT_AND_AND: {
          pcn = compile_cond(pn, pan1, pan2, compile_numlit(pn, TS_INT, (v.i = 0, &v)));
        } break;
        case TT_OR_OR: {
          pcn = compile_cond(pn, pan1, compile_numlit(pn, TS_INT, (v.i = 1, &v)), pan2);
        } break;
        default: { /* normal binary */
          pcn = compile_binary(pn, pan1, pn->op, pan2);
        } break;
      }
    } break;
    case NT_COND: {
      node_t *pan1 = expr_compile(ndref(pn, 0), prib, NULL);
      node_t *pan2 = expr_compile(ndref(pn, 1), prib, NULL);
      node_t *pan3 = expr_compile(ndref(pn, 2), prib, NULL);
      pcn = compile_cond(pn, pan1, pan2, pan3);
    } break;
    case NT_ASSIGN: {
      node_t *pn0 = ndref(pn, 0), *pln = (pn0->nt == NT_CAST) ? ndref(pn0, 1) : pn0;
      node_t *ptn = (pn0->nt == NT_CAST) ? ndref(pn0, 0) : NULL, *pvn = ndref(pn, 1);
      node_t *pan = expr_compile(pln, prib, NULL), *pan2 = NULL;
      pcn = NULL;
      if (pn->op == TT_ASN && ptn == NULL && ts_bulk(acode_type(pan)->ts)) { 
        if (pvn->nt == NT_CALL) { /* pass pan reference as extra arg */
          size_t i; buf_t apb = mkbuf(sizeof(node_t*));
          node_t *pfn = expr_compile(ndref(pvn, 0), prib, NULL);
          for (i = 1; i < ndlen(pvn); ++i) 
            *(node_t**)bufnewbk(&apb) = expr_compile(ndref(pvn, i), prib, NULL);
          /* compile_call will return NULL if it is given lval and it is not bulk */
          pcn = compile_call(pvn, pfn, &apb, compile_addrof(pln, pan)); 
          buffini(&apb);
        } else { /* try regular bulk assignment */
          pan2 = expr_compile(pvn, prib, NULL);
          pcn = compile_bulkasn(pn, pan, pan2); /* returns NULL on failure */
        }
      }
      if (!pcn) { /* failed? must be scalar assignment */
        tt_t op; /* TT_PLUS from TT_PLUS_ASN etc. */
        if (!pan2) pan2 = expr_compile(pvn, prib, NULL);
        op = (tt_t)((pn->op >= TT_PLUS_ASN && pn->op <= TT_SHR_ASN) ? (int)pn->op - 1 : 0);
        pcn = compile_asncombo(pn, pan, ptn, op, pan2, false); 
      }
    } break;
    case NT_COMMA: {
      size_t i; pcn = npnewcode(pn); assert(ndlen(pn) > 0);
      for (i = 0; i < ndlen(pn); ++i) {
        node_t *pcni = expr_compile(ndref(pn, i), prib, NULL);
        node_t *ptni = acode_type(pcni); bool drop = false;
        if (i == ndlen(pn) - 1) ndcpy(ndinsnew(pcn, 0), ptni);
        else drop = ptni->ts != TS_VOID;
        acode_swapin(pcn, pcni);
        if (drop) acode_pushin(pcn, IN_DROP);
      }
    } break;
    case NT_ACODE: {
      pcn = pn;
    } break;
    /* statements */
    case NT_BLOCK: {
      size_t i; bool end = false; 
      if (ret == NULL) neprintf(pn, "{} block used as an expression");
      pcn = npnewcode(pn); ndsettype(ndnewbk(pcn), TS_VOID);
      if (pn->name && pn->op == TT_BREAK_KW) {
        acode_pushin_id_uarg(pcn, IN_BLOCK, pn->name, BT_VOID);
        end = true;
      } else if (pn->name && pn->op == TT_CONTINUE_KW) {
        acode_pushin_id_uarg(pcn, IN_LOOP, pn->name, BT_VOID);
        end = true;
      }
      for (i = 0; i < ndlen(pn); ++i) {
        node_t *pcni = expr_compile(ndref(pn, i), prib, ret);
        acode_swapin(pcn, compile_stm(pcni));
      }
      if (end) acode_pushin_id(pcn, IN_END, pn->name);
    } break;
    case NT_IF: {
      size_t n = ndlen(pn); 
      node_t *pn2, *pan1, *pan2, *pan3;
      assert(ret);
      pan1 = expr_compile(ndref(pn, 0), prib, NULL);
      pn2 = ndref(pn, 1);
      if (n == 2 && pn2->nt == NT_GOTO) {
        pcn = compile_branch(pn, pan1, pn2->name);
      } else {
        pan2 = expr_compile(pn2, prib, ret);
        pan3 = n == 3 ? expr_compile(ndref(pn, 2), prib, ret) : NULL;
        pcn = compile_if(pn, pan1, pan2, pan3);
      }
    } break;
    case NT_SWITCH: {
      node_t *pan; assert(ndlen(pn) >= 2); /* (x) followed by default */
      assert(ret);
      pan = compile_intsel(pn, expr_compile(ndref(pn, 0), prib, NULL));
      pcn = compile_switch_table(pn, pan, ndref(pn, 1), ndlen(pn)-1);
      if (!pcn) pcn = compile_switch_ifs(pn, pan, ndref(pn, 1), ndlen(pn)-1);
    } break;
    case NT_CASE:
    case NT_DEFAULT: assert(false); break; /* handled by switch */
    case NT_WHILE: 
    case NT_DO: 
    case NT_FOR: assert(false); break; /* dewasmified */
    case NT_GOTO: {
      pcn = compile_branch(pn, NULL, pn->name);
    } break; 
    case NT_RETURN: {
      node_t *pan = NULL;
      assert(ret);
      if (ndlen(pn) == 1) pan = expr_compile(ndref(pn, 0), prib, NULL);
      pcn = compile_return(pn, pan, ret, pn->name);
    } break;
    case NT_BREAK:
    case NT_CONTINUE: assert(false); break; /* dewasmified */
    case NT_DISPLAY: {
      /* static displays are processed here */
      assert(ndlen(pn) > 0);
      if (pn->sc == SC_STATIC && ts_bulk(ndref(pn, 0)->ts)) {
        size_t size, align;
        size_t pdidx = watieblen(&g_curpwm->exports);
        watie_t *pd = watiebnewbk(&g_curpwm->exports, IEK_DATA);
        node_t *ptn = ndref(pn, 0); assert(ptn->nt == NT_TYPE); 
        pd->mod = g_curmod; pd->id = internf("ds%d$", (int)pdidx);
        measure_type(ptn, pn, &size, &align, 0);
        bufresize(&pd->data, size); /* fills with zeroes */
        pd->align = (int)align; pd->mut = MT_CONST;
        pd = initialize_bulk_data(pdidx, pd, 0, ptn, pn);
        pcn = npnewcode(pn); ndcpy(ndnewbk(pcn), ptn);
        acode_pushin_id_mod_iarg(pcn, IN_REF_DATA, pd->id, g_curmod, 0);
      } 
    } break;
    case NT_NULL: { /* empty statement */
      pcn = npnewcode(pn); ndsettype(ndnewbk(pcn), TS_VOID);
    } break;
    default:;
  }

  if (!pcn) 
    neprintf(pn, "failed to compile expression");
  assert(pcn->nt == NT_ACODE);
  if (pcn->nt == NT_ACODE) assert(ndlen(pcn) > 0 && ndref(pcn, 0)->nt == NT_TYPE);
  return pcn;  
}

/* compile function -- that is, convert it to asm tree */
static node_t *fundef_compile(node_t *pdn)
{
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1), *pln = NULL;
  buf_t rib = mkbuf(sizeof(ri_t)); node_t *ret = NULL; size_t i;
  /* output is created anew from pieces of input and pool nodes */
  node_t *prn = npnew(NT_FUNDEF, pdn->pwsid, pdn->startpos), *prtn, *prbn; 
  prn->name = pdn->name; prn->sc = pdn->sc;
  ndnewbk(prn); ndnewbk(prn); prtn = ndref(prn, 0), prbn = ndref(prn, 1); 
  ndset(prtn, NT_TYPE, ptn->pwsid, ptn->startpos); prtn->ts = TS_FUNCTION;
  ndset(prbn, NT_BLOCK, pbn->pwsid, pbn->startpos);
  
  /* collect types for args and local vars; sort for bsearch */
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION && ndlen(ptn) > 0);
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
  bufqsort(&rib, &sym_cmp);

  /* go over body statements */
  for (/* use current i */; i < ndlen(pbn); ++i) {
    node_t *pni = ndref(pbn, i), *pcn;
    if (i+1 == ndlen(pbn)) pln = pni; 
    pcn = expr_compile(pni, &rib, ret);
    ndswap(ndnewbk(prbn), compile_stm(pcn));
  }

  /* check for unexpected implicit return */
  if (ret->ts != TS_VOID) {
    if (!ends_in_return(pln))
      n2eprintf(pln, pdn, "non-void function: last statement must be explicit 'return'");
  }

  buffini(&rib);
  return prn;
}

/* flatten asm tree */
static void expr_flatten(node_t *pn, node_t *pcn, icbuf_t *prb) 
{
  size_t i, ri = 1;
  for (i = 0; i < buflen(&pn->data); ++i) {
    inscode_t *pic = bufref(&pn->data, i);
    if (pic->in == IN_PLACEHOLDER) {
      assert(ri < ndlen(pn));
      expr_flatten(ndref(pn, ri), pcn, prb);
      ++ri;
    } else if (pic->in == IN_REGDECL) {
      asm_pushbk(prb, pic); 
    } else {
      asm_pushbk(&pcn->data, pic); 
    }
  }  
}

/* flatten nested acodes into a single acode */
static node_t *fundef_flatten(node_t *pdn) 
{
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1);
  node_t *pcn = npnewcode(pdn); 
  size_t i; icbuf_t icb; icbinit(&icb);
  for (i = 1; i < ndlen(ptn); ++i) {
    node_t *pn = ndref(ptn, i);
    if (pn->nt == NT_VARDECL) {
      inscode_t *pic = icbnewbk(&icb); 
      pic->in = IN_REGDECL; pic->id = pn->name;
      pic->arg.u = ts_to_blocktype(ndref(pn, 0)->ts);      
      assert(VT_F64 <= pic->arg.u && pic->arg.u <= VT_I32);
      lift_arg0(pn); /* vardecl => type */
    }
  }
  for (i = 0; i < ndlen(pbn); ++i) {
    node_t *pn = ndref(pbn, i);
    if (pn->nt == NT_VARDECL) {
      inscode_t *pic = icbnewbk(&icb); 
      pic->in = IN_REGDECL; pic->id = pn->name;
      pic->arg.u = ts_to_blocktype(ndref(pn, 0)->ts);      
      assert(VT_F64 <= pic->arg.u && pic->arg.u <= VT_I32);
    } else if (pn->nt == NT_ACODE) {
      expr_flatten(pn, pcn, &icb);
    } else assert(false);
  } 
  for (i = icblen(&icb); i > 0; --i) {
    inscode_t *pic = icbnewfr(&pcn->data);
    *pic = *icbref(&icb, i-1);
  }
  ndswap(ndnewbk(pcn), ptn);
  icbfini(&icb);
  return pcn;
}

/* fixme: simple peephole optimization */
static void fundef_peephole(node_t *pcn, int optlvl)
{
  icbuf_t *picb = &pcn->data; size_t i, nmods;
  do {
    for (nmods = i = 0; i < icblen(picb); /* rem or bump i */) {
      size_t nexti = i+1;
      if (i > 0) {
        inscode_t *pprevi = icbref(picb, i-1);
        inscode_t *pthisi = icbref(picb, i);
        if (pthisi->in == IN_DROP) {
          if (pprevi->in == IN_LOCAL_TEE) {
            pprevi->in = IN_LOCAL_SET;
            icbrem(picb, i);
            ++nmods; nexti = i;
          } else if (pprevi->in == IN_LOCAL_GET || pprevi->in == IN_GLOBAL_GET) {
            icbrem(picb, i-1);
            icbrem(picb, i-1);
            ++nmods; nexti = i-1;
          } else if (IN_I32_LOAD <= pprevi->in && pprevi->in <= IN_I64_LOAD32_U) {
            icbrem(picb, i-1);
            ++nmods; nexti = i-1;
          }
        } else if (pthisi->in == IN_LOCAL_GET) {
          if (pprevi->in == IN_LOCAL_SET && pprevi->id && pprevi->id == pthisi->id) {
            pprevi->in = IN_LOCAL_TEE;
            icbrem(picb, i);
            ++nmods; nexti = i;
          }
        } else if (pthisi->in == IN_I32_ADD) {
          if (pprevi->in == IN_I32_CONST && pprevi->arg.i == 0) {
            icbrem(picb, i-1);
            icbrem(picb, i-1);
            ++nmods; nexti = i-1;
          }
        } else if (pthisi->in == IN_I32_EXTEND8_S) {
          if (pprevi->in == IN_I32_EXTEND8_S || pprevi->in == IN_I64_LOAD8_S) {
            icbrem(picb, i);
            ++nmods; nexti = i;
          }
        } else if (pthisi->in == IN_I32_EXTEND16_S) {
          if (pprevi->in == IN_I32_EXTEND16_S || pprevi->in == IN_I32_LOAD16_S) {
            icbrem(picb, i);
            ++nmods; nexti = i;
          }
        }
      }
      if (i >= icblen(picb)) break;
      if (i > 1) {
        inscode_t *ppprei = icbref(picb, i-2);
        inscode_t *pprevi = icbref(picb, i-1);
        inscode_t *pthisi = icbref(picb, i);
        if (pthisi->in == IN_I32_MUL) {
          if (pprevi->in == IN_I32_CONST && ppprei->in == IN_I32_CONST) {
            unsigned long long n1 = ppprei->arg.u, n2 = pprevi->arg.u;
            ppprei->arg.i = (long long)((((n1 * n2) & 0xFFFFFFFFU) << 32) >> 32);
            icbrem(picb, i-1);
            icbrem(picb, i-1);
            ++nmods; nexti = i-1; 
          }
        } else if (IN_I32_LOAD <= pthisi->in && pthisi->in <= IN_I64_LOAD32_U) {
          if (pprevi->in == IN_I32_ADD && ppprei->in == IN_I32_CONST) {
            unsigned long long n = pthisi->arg.u & 0xFFFFFFFFU;
            long long off = (ppprei->arg.i << 32) >> 32; /* may be negative */
            if (off >= 0 && (n + (unsigned long long)off) < 0xFFFFFFFFU) { 
              pthisi->arg.u += (unsigned long long)off;
              icbrem(picb, i-2);
              icbrem(picb, i-2);
              ++nmods; nexti = i-2;
            }  
          }
        } 
      }
      i = nexti;
    }
  } while (nmods > 0 && --optlvl > 0);
}

/* compile single expression on top level */
static node_t *expr_compile_top(node_t *pn)
{
  buf_t rib = mkbuf(sizeof(ri_t));
  node_t *pcn = expr_compile(pn, &rib, NULL);
  if (getverbosity() > 0) {
    fprintf(stderr, "expr_compile ==>\n");
    dump_node(pcn, stderr);
  }
  pcn = fundef_flatten(pcn);
  if (getverbosity() > 0) {
    fprintf(stderr, "fundef_flatten ==>\n");
    dump_node(pcn, stderr);
  }
  if (g_optlvl > 0) {
    fundef_peephole(pcn, (int)g_optlvl);
    if (getverbosity() > 0) {
      fprintf(stderr, "fundef_peephole[-O%d] ==>\n", (int)g_optlvl);
      dump_node(pcn, stderr);
    }
  }
  buffini(&rib);
  return pcn;
}


/* convert function param/result type node to a valtype */
static void tn2vt(node_t *ptn, vtbuf_t *pvtb)
{
  assert(ptn->nt == NT_TYPE);
  if (ptn->ts == TS_VOID) {
    /* put nothing */
  } else {
    valtype_t vt;
    if (ptn->ts == TS_ARRAY && ndlen(ptn) == 2 && ndref(ptn, 1)->nt == NT_NULL) vt = ts2vt(TS_PTR);
    else vt = ts2vt(ptn->ts);
    if (vt == VT_UNKN) {
      neprintf(ptn, "function arguments of this type are not supported");
    } else {
      *vtbnewbk(pvtb) = vt;
    }
  }
}

/* convert function type to a function signature */
funcsig_t *ftn2fsig(node_t *ptn, funcsig_t *pfs)
{
  size_t i; node_t *ptni;
  assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  bufclear(&pfs->partypes);
  bufclear(&pfs->restypes);
  ptni = ndref(ptn, 0); assert(ptn->nt == NT_TYPE);
  if (ts_bulk(ptni->ts)) { /* passed as pointer in 1st arg */
    /* fixme: should depend on wasm32/wasm64 model */
    *vtbnewbk(&pfs->partypes) = VT_I32; /* restypes stays empty */  
  } else {
    tn2vt(ptni, &pfs->restypes);
  }
  for (i = 1; i < ndlen(ptn); ++i) {
    ptni = ndref(ptn, i); 
    if (ptni->nt == NT_VARDECL) { assert(ndlen(ptni) == 1); ptni = ndref(ptni, 0); }
    assert(ptni->nt == NT_TYPE);
    if (i+1 == ndlen(ptn) && ptni->ts == TS_ETC) {
      /* fixme: should depend on wasm32/wasm64 model */
      *vtbnewbk(&pfs->partypes) = VT_I32; /* ... => ap$ */  
    } else {
      tn2vt(ptni, &pfs->partypes);
    }
  }
  return pfs;
}

/* process function definition in module body */
static void process_fundef(sym_t mmod, node_t *pn, wat_module_t *pm)
{
  node_t *pcn, *ptn; watie_t *pf;
  assert(pn->nt == NT_FUNDEF && pn->name && ndlen(pn) == 2);
  ptn = ndref(pn, 0); assert(ptn->nt == NT_TYPE && ptn->ts == TS_FUNCTION);
  clear_regpool(); /* reset reg name generator */
  if (getverbosity() > 0) {
    fprintf(stderr, "process_fundef:\n");
    dump_node(pn, stderr);
  }
  fundef_check_type(ndref(pn, 0)); /* param: x[] => *x */
  if (pn->name == intern("main")) {
    if (ndlen(ptn) == 1 && ndref(ptn, 0)->ts == TS_INT) {
      pm->main = MAIN_VOID;
    } else { /* expect argc, argv */ 
      int sigok = true; node_t *psn = NULL;
      if (ndlen(ptn) != 3 || ndref(ptn, 0)->ts != TS_INT) 
        sigok = false;
      if (sigok && (psn = ndref(ptn, 1), psn = (psn->nt == NT_VARDECL) ? ndref(psn, 0) : psn)->ts != TS_INT) 
        sigok = false;
      if (sigok && (psn = ndref(ptn, 2), psn = (psn->nt == NT_VARDECL) ? ndref(psn, 0) : psn)->ts != TS_PTR) 
        sigok = false;
      if (sigok && ndref(psn, 0)->ts != TS_PTR && (psn = ndref(psn, 0))->ts != TS_CHAR) 
        sigok = false;
      if (!sigok) neprintf(ptn, "main function should be defined as int main(int argc, char *argv[])");  
      pm->main = MAIN_ARGC_ARGV;
    }
  }
  { /* post appropriate vardecl for later use */
    node_t nd = mknd();
    ndset(&nd, NT_VARDECL, pn->pwsid, pn->startpos);
    nd.name = pn->name; ndpushbk(&nd, ndref(pn, 0)); 
    /* post function symbol as final, hide it if static */ 
    post_vardecl(mmod, &nd, true, pn->sc == SC_STATIC); 
    ndfini(&nd); 
  }
  /* hoist local variables */
  fundef_hoist_locals(pn);
  if (getverbosity() > 0) {
    fprintf(stderr, "fundef_hoist_locals ==>\n");
    dump_node(pn, stderr);
  }  
  /* fold constant expressions */
  fundef_fold_constants(pn);
  if (getverbosity() > 0) {
    fprintf(stderr, "fundef_fold_constants ==>\n");
    dump_node(pn, stderr);
  }  
  /* convert entry to wasm conventions, normalize a bit */
  fundef_wasmify(pn);
  if (getverbosity() > 0) {
    fprintf(stderr, "fundef_wasmify ==>\n");
    dump_node(pn, stderr);
  }  
  /* compile to asm tree */
  pcn = fundef_compile(pn);
  if (getverbosity() > 0) {
    fprintf(stderr, "fundef_compile ==>\n");
    dump_node(pcn, stderr);
  }
  /* flatten asm tree */
  pcn = fundef_flatten(pcn);
  if (getverbosity() > 0) {
    fprintf(stderr, "fundef_flatten ==>\n");
    dump_node(pcn, stderr);
  }
  if (g_optlvl > 0) {
    fundef_peephole(pcn, (int)g_optlvl);
    if (getverbosity() > 0) {
      fprintf(stderr, "fundef_peephole[-O%d] ==>\n", (int)g_optlvl);
      dump_node(pcn, stderr);
    }
  }
  /* add to watf module */
  pf = watiebnewbk(&pm->exports, IEK_FUNC);
  pf->id = pn->name; pf->mod = mmod;
  pf->exported = (pn->sc != SC_STATIC);
  ftn2fsig(acode_type(pcn), &pf->fs);
  bufswap(&pcn->data, &pf->code);
  
  /* free all nodes allocated with np-functions at once */
  clear_nodepool();
}

/* process top-level intrinsic call */
static void process_top_intrcall(node_t *pn)
{
  if (getverbosity() > 0) {
    dump_node(pn, stderr);
  }
  if (pn->intr == INTR_SASSERT) check_static_assert(pn, NULL);
  else neprintf(pn, "unexpected top-level form");
}

/* process single top node (from module or include) */
static void process_top_node(sym_t mmod, node_t *pn, wat_module_t *pm)
{
  /* ignore empty blocks left from macros */
  if (pn->nt == NT_BLOCK && ndlen(pn) == 0) return;
  /* ignore typedefs (they are already processed) */  
  if (pn->nt == NT_TYPEDEF) {
    if (getverbosity() > 0) {
      size_t size = 0, align = 0;
      dump_node(pn, stderr); assert(ndlen(pn) == 1);
      /* measure_type(ndref(pn, 0), pn, &size, &align, 0);
      fprintf(stderr, "size: %d, align: %d\n", (int)size, (int)align); */
    }
    return;
  }
  /* remaining decls/defs are processed differently */
  if (!mmod) {
    /* in header: declarations only */
    size_t i;
    if (pn->nt == NT_FUNDEF) neprintf(pn, "function definition in header");
    else if (pn->nt == NT_INTRCALL) process_top_intrcall(pn); 
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
        if (ptn->ts == TS_FUNCTION) fundef_check_type(ptn); /* param: x[] => *x */
        else vardef_check_type(ptn); /* evals array sizes */
        post_vardecl(pn->name, pni, false, false); /* not final, not hidden */
      } else {
        neprintf(pni, "extern declaration expected");
      }
    }
  } else {
    /* in module mmod: produce code */
    size_t i;
    g_curmod = mmod; g_curpwm = pm;
    if (pn->nt == NT_FUNDEF) process_fundef(mmod, pn, pm);
    else if (pn->nt == NT_INTRCALL) process_top_intrcall(pn); 
    else if (pn->nt != NT_BLOCK) neprintf(pn, "unexpected top-level declaration");
    else for (i = 0; i < ndlen(pn); ++i) {
      node_t *pni = ndref(pn, i);
      if (pni->nt == NT_VARDECL && pni->sc == SC_EXTERN) {
        neprintf(pni, "extern declaration in module (extern is for use in headers only)");
      } else if (pni->nt == NT_VARDECL) {
        if (i+1 < ndlen(pn) && ndref(pn, i+1)->nt == NT_ASSIGN) {
          process_vardecl(mmod, pni, ndref(pn, ++i));
        } else {
          process_vardecl(mmod, pni, NULL);
        }
      } else {
        neprintf(pni, "top-level declaration or definition expected");
      }
    }    
  }
}

/* parse/process include file and its includes */
static void process_include(pws_t *pw, int startpos, bool sys, sym_t name, wat_module_t *pm)
{
  pws_t *pwi; node_t nd = mknd();
  pwi = pws_from_modname(sys, name); /* searches g_ibases */
  if (pwi) {
    /* this should be workspace #1... */
    assert(pwsid(pwi) > 0);
    while (parse_top_form(pwi, &nd)) {
      if (nd.nt == NT_INCLUDE) {
        bool sys = (nd.op != TT_STRING);
        process_include(pwi, nd.startpos, sys, nd.name, pm);
      } else if (nd.nt != NT_NULL) {
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
static sym_t process_module(const char *fname, wat_module_t *pm)
{
  pws_t *pw; node_t nd = mknd(); sym_t mod = 0;
  pw = newpws(fname);
  if (pw) {
    /* this should be workspace #0 */
    assert(pwsid(pw) == 0);
    assert(pwscurmod(pw));
    /* parse top level */
    while (parse_top_form(pw, &nd)) {
      if (nd.nt == NT_INCLUDE) {
        bool sys = (nd.op != TT_STRING);
        process_include(pw, nd.startpos, sys, nd.name, pm);
      } else if (nd.nt != NT_NULL) {
        process_top_node(pwscurmod(pw), &nd, pm);
      }
    }
    mod = pwscurmod(pw);
    closepws(pw);
  } else {
    exprintf("cannot read module file: %s", fname);
  }
  ndfini(&nd);
  return mod;
}

/* compile source file to in-memory wat output module */
void compile_module_to_wat(const char *ifname, wat_module_t *pwm)
{
  size_t i; sym_t mod;

  init_compiler();
  wat_module_clear(pwm);

  { /* add builtin declarations: stack pointer */
    node_t nd = mknd(); ndset(&nd, NT_VARDECL, -1, -1);
    nd.name = g_sp_id; wrap_type_pointer(ndsettype(ndnewbk(&nd), TS_VOID)); 
    post_symbol(g_crt_mod, &nd, false, false);
    ndfini(&nd);
  }

  mod = process_module(ifname, pwm); assert(mod);
  pwm->name = mod;

  /* add standard imports/exports */
  switch (pwm->main) {
    case MAIN_ABSENT: {
      if (mod == g_crt_mod) { /* special case */
        watie_t *pe, *pg; inscode_t *pic; 
        /* (memory $crt:memory (export "memory") 2) */
        pe = watiebnewbk(&pwm->exports, IEK_MEM);
        pe->mod = g_crt_mod; pe->id = g_lm_id; pe->exported = true;
        pe->lt = LT_MIN; pe->n = 2; /* patched by linker */
        /* (global $crt:sp$ (export "sp$") (mut i32) (i32.const 4242)) */
        watiebdel(&pwm->exports, IEK_GLOBAL, g_crt_mod, g_sp_id);
        pg = watiebnewbk(&pwm->exports, IEK_GLOBAL);
        pg->mod = g_crt_mod; pg->id = g_sp_id; pg->exported = true;
        pg->mut = MT_VAR; pg->vt = VT_I32;
        pic = &pg->ic; pic->in = IN_I32_CONST; pic->arg.i = 4242; /* patched by linker */
        /* (global $crt:stack_base (export "stack_base") i32 (i32.const 42424)) */
        watiebdel(&pwm->exports, IEK_GLOBAL, g_crt_mod, g_sb_id);
        pg = watiebnewbk(&pwm->exports, IEK_GLOBAL);
        pg->mod = g_crt_mod; pg->id = g_sb_id; pg->exported = true;
        pg->mut = MT_CONST; pg->vt = VT_I32;
        pic = &pg->ic; pic->in = IN_I32_CONST; pic->arg.i = 4242; /* patched by linker */
        /* (global $crt:heap_base (export "heap_base") i32 (i32.const 42424)) */
        watiebdel(&pwm->exports, IEK_GLOBAL, g_crt_mod, g_hb_id);
        pg = watiebnewbk(&pwm->exports, IEK_GLOBAL);
        pg->mod = g_crt_mod; pg->id = g_hb_id; pg->exported = true;
        pg->mut = MT_CONST; pg->vt = VT_I32;
        pic = &pg->ic; pic->in = IN_I32_CONST; pic->arg.i = 4242; /* patched by linker */
      } else { /* regular module without 'main'*/
        watie_t *pi;
        /* (import "crt" "sp$" (global $crt:sp$ (mut i32))) */
        pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
        pi->mod = g_crt_mod; pi->id = g_sp_id; 
        pi->mut = MT_VAR; pi->vt = VT_I32;
        /* (import "crt" "memory" (memory $crt:memory 0)) */
        pi = watiebnewbk(&pwm->imports, IEK_MEM); 
        pi->mod = g_crt_mod; pi->id = g_lm_id; 
        pi->lt = LT_MIN; pi->n = 0;
      }
    } break;
    case MAIN_ARGC_ARGV: {
      watie_t *pi, *pf; inscode_t *pic;
      /* (import "crt" "sp$" (global $crt:sp$ (mut i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
      pi->mod = g_crt_mod; pi->id = g_sp_id; 
      pi->mut = MT_VAR; pi->vt = VT_I32;
      /* (import "crt" "stack_base" (global $crt:stack_base i32)) */
      pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
      pi->mod = g_crt_mod; pi->id = g_sb_id; 
      pi->mut = MT_CONST; pi->vt = VT_I32;
      /* (import "crt" "heap_base" (global $crt:heap_base i32)) */
      pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
      pi->mod = g_crt_mod; pi->id = g_hb_id; 
      pi->mut = MT_CONST; pi->vt = VT_I32;
      /* (import "crt" "memory" (memory $crt:memory 0)) */
      pi = watiebnewbk(&pwm->imports, IEK_MEM);
      pi->mod = g_crt_mod; pi->id = g_lm_id; 
      pi->lt = LT_MIN; pi->n = 0;
      /* (import "crt" "initialize" (func $...)) */
      pi = watiebnewbk(&pwm->imports, IEK_FUNC);
      pi->mod = g_crt_mod; pi->id = intern("initialize");
      /* (import "crt" "_argc" (global $crt:_argc (mut i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
      pi->mod = g_crt_mod; pi->id = intern("_argc");
      pi->mut = MT_VAR; pi->vt = VT_I32;
      /* (import "crt" "_argv" (global $crt:_argv (mut i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
      pi->mod = g_crt_mod; pi->id = intern("_argv");
      pi->mut = MT_VAR; pi->vt = VT_I32;
      /* (import "crt" "terminate" (func $... (param i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_FUNC);
      pi->mod = g_crt_mod; pi->id = intern("terminate");
      *vtbnewbk(&pi->fs.partypes) = VT_I32;
      /* (func $_start ...) */
      pf = watiebnewbk(&pwm->exports, IEK_FUNC); 
      pf->mod = mod; pf->id = intern("_start"); /* fs is void->void */
      pf->exported = true;
      pic = icbnewbk(&pf->code); pic->in = IN_CALL; pic->id = intern("initialize"); pic->arg2.mod = g_crt_mod; 
      pic = icbnewbk(&pf->code); pic->in = IN_GLOBAL_GET; pic->id = intern("_argc"); pic->arg2.mod = g_crt_mod;
      pic = icbnewbk(&pf->code); pic->in = IN_GLOBAL_GET; pic->id = intern("_argv"); pic->arg2.mod = g_crt_mod;
      pic = icbnewbk(&pf->code); pic->in = IN_CALL; pic->id = intern("main"); pic->arg2.mod = mod; 
      pic = icbnewbk(&pf->code); pic->in = IN_CALL; pic->id = intern("terminate"); pic->arg2.mod = g_crt_mod;
    } break;
    case MAIN_VOID: {
      watie_t *pi, *pf; inscode_t *pic;
      /* (import "crt" "sp$" (global $crt:sp$ (mut i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
      pi->mod = g_crt_mod; pi->id = g_sp_id; 
      pi->mut = MT_VAR; pi->vt = VT_I32;
      /* (import "crt" "memory" (memory $crt:memory 0)) */
      pi = watiebnewbk(&pwm->imports, IEK_MEM);
      pi->mod = g_crt_mod; pi->id = g_lm_id; 
      pi->lt = LT_MIN; pi->n = 0;
      /* (import "crt" "initialize" (func $...)) */
      pi = watiebnewbk(&pwm->imports, IEK_FUNC);
      pi->mod = g_crt_mod; pi->id = intern("initialize");
      /* (import "crt" "terminate" (func $... (param i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_FUNC);
      pi->mod = g_crt_mod; pi->id = intern("terminate");
      *vtbnewbk(&pi->fs.partypes) = VT_I32;
      /* (func $_start ...) */
      pf = watiebnewbk(&pwm->exports, IEK_FUNC); 
      pf->mod = mod; pf->id = intern("_start"); /* fs is void->void */
      pf->exported = true;
      pic = icbnewbk(&pf->code); pic->in = IN_CALL; pic->id = intern("initialize"); pic->arg2.mod = g_crt_mod; 
      pic = icbnewbk(&pf->code); pic->in = IN_CALL; pic->id = intern("main"); pic->arg2.mod = mod; 
      pic = icbnewbk(&pf->code); pic->in = IN_CALL; pic->id = intern("terminate"); pic->arg2.mod = g_crt_mod;
    } break;
    default: assert(false);
  }

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
          /* this mod:id needs to be imported as function */
          watie_t *pi;
          if (getverbosity() > 0) {
            fprintf(stderr, "imported function %s:%s =>\n", symname(pn->name), symname(id));
            dump_node(ptn, stderr);
          }
          pi = watiebnewbk(&pwm->imports, IEK_FUNC);
          pi->mod = pn->name, pi->id = id;
          ftn2fsig(ptn, &pi->fs);
        } else {
          watie_t *pi;
          if (getverbosity() > 0) {
            fprintf(stderr, "imported global %s:%s =>\n", symname(pn->name), symname(id));
            dump_node(ptn, stderr);
          }
          pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
          pi->mod = pn->name, pi->id = id;
          pi->vt = ts2vt(ts_bulk(ptn->ts) ? TS_PTR : ptn->ts);
        } 
      }
    }
  }

  /* done */
  fini_compiler();
}

int main(int argc, char **argv)
{
  int opt; char *eoarg;
  const char *ifile_arg = "-";
  const char *ofile_arg = NULL;
  bool c_opt = false;
  long lvl_arg = 3;
  unsigned long s_arg = 131072; /* 128K default */
  unsigned long a_arg = 4096; /* 4K default */
  const char *path;
  dsbuf_t incv, libv; 
  
  dsbinit(&incv);
  if ((path = getenv("WCPL_INCLUDE_PATH")) != NULL) dsbpushbk(&incv, (dstr_t*)&path);
  dsbinit(&libv); 
  if ((path = getenv("WCPL_LIBRARY_PATH")) != NULL) dsbpushbk(&libv, (dstr_t*)&path);

  setprogname(argv[0]);
  setusage
    ("[OPTIONS] [infile] ...\n"
     "options are:\n"
     "  -w        Suppress warnings\n"
     "  -v        Increase verbosity\n"
     "  -q        Suppress logging ('quiet')\n"
     "  -c        Compile single input file\n"
     "  -O lvl    Optimization level; defaults to 3\n"
     "  -o ofile  Output file (use .wasm suffix for binary executable)\n"
     "  -I path   Add include path (must end with path separator)\n"
     "  -L path   Add library path (must end with path separator)\n"
     "  -s stksz  Stack size in bytes; defaults to 131072 (128K)\n"
     "  -a argsz  Argument area size in bytes (use 0 for malloc); defaults to 4096\n"
     "  -h        This help");
  while ((opt = egetopt(argc, argv, "wvqcO:o:L:I:s:a:h")) != EOF) {
    switch (opt) {
      case 'w':  setwlevel(3); break;
      case 'v':  incverbosity(); break;
      case 'q':  incquietness(); break;
      case 'c':  c_opt = true; break;
      case 'O':  lvl_arg = atol(eoptarg); break;
      case 'o':  ofile_arg = eoptarg; break;
      case 'I':  eoarg = eoptarg; dsbpushbk(&incv, &eoarg); break;
      case 'L':  eoarg = eoptarg; dsbpushbk(&libv, &eoarg); break;
      case 's':  s_arg = strtoul(eoptarg, NULL, 0); break; 
      case 'a':  a_arg = strtoul(eoptarg, NULL, 0); break; 
      case 'h':  eusage("WCPL 0.04 built on " __DATE__);
    }
  }

  if (s_arg < 1024 || s_arg > 16777216)
    eusage("-s argument is outside of reasonable range");
  if (a_arg != 0 && (a_arg < 512 || a_arg > 65536))
    eusage("-a argument is outside of reasonable range");

  init_wcpl(&incv, &libv, lvl_arg, (size_t)s_arg, (size_t)a_arg);

  if (c_opt) {
    /* compile single source file */
    wat_module_t wm; wat_module_init(&wm);
    if (eoptind < argc) ifile_arg = argv[eoptind++];
    if (eoptind < argc) eusage("too many input files for -c mode");
    /* todo: autogenerate output file name (stdout for now) */
    compile_module_to_wat(ifile_arg, &wm);
    if (ofile_arg) {
      FILE *pf = fopen(ofile_arg, "w");
      if (!pf) exprintf("cannot open output file %s:", ofile_arg);
      write_wat_module(&wm, pf);
      fclose(pf);
      logef("# object module written to %s\n", ofile_arg);
    } else {
      write_wat_module(&wm, stdout);
    }
    wat_module_fini(&wm);   
  } else {
    /* load/compile input files, fetch libraries, link */
    wat_module_t wm; wat_module_buf_t wmb;
    if (eoptind == argc) eusage("one or more input file expected");
    wat_module_init(&wm); wat_module_buf_init(&wmb);
    while (eoptind < argc) {
      wat_module_t *pwm = wat_module_buf_newbk(&wmb);
      ifile_arg = argv[eoptind++];
      if (strsuf(ifile_arg, ".o") != NULL || strsuf(ifile_arg, ".wo") != NULL || strsuf(ifile_arg, ".wat") != NULL) {
        logef("# loading object module from %s\n", ifile_arg);
        read_wat_object_module(ifile_arg, pwm);
        logef("# object module '%s' loaded\n", symname(pwm->name));
      } else {
        logef("# compiling source file %s\n", ifile_arg);
        compile_module_to_wat(ifile_arg, pwm);
        logef("# object module '%s' created\n", symname(pwm->name));
      }
    }
    link_wat_modules(&wmb, &wm);
    logef("# all modules linked successfully\n", symname(g_wasi_mod));

    if (ofile_arg) {
      FILE *pf;
      if (strsuf(ofile_arg, ".wasm")) {
        wasm_module_t wbm;
        pf = fopen(ofile_arg, "wb");
        if (!pf) exprintf("cannot open output file %s:", ofile_arg);
        wasm_module_init(&wbm);
        wat_to_wasm(&wm, &wbm);
        write_wasm_module(&wbm, pf);
        wasm_module_fini(&wbm);
      } else {
        pf = fopen(ofile_arg, "w");
        if (!pf) exprintf("cannot open output file %s:", ofile_arg);
        write_wat_module(&wm, pf);
      }
      fclose(pf);
      logef("# executable module written to %s\n", ofile_arg);
    } else {
      write_wat_module(&wm, stdout);
    }

    wat_module_fini(&wm);
    wat_module_buf_fini(&wmb); 
  }

  fini_wcpl();
  dsbfini(&incv); dsbfini(&libv);
  return EXIT_SUCCESS;
}

