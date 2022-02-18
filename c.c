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

/* wcpl globals */
long   g_optlvl;   /* -O arg */
buf_t *g_ibases;   /* module include search bases */
buf_t *g_lbases;   /* module object module search bases */
sym_t  g_env_mod;  /* environment module */
sym_t  g_wasi_mod; /* module for wasi */
sym_t  g_lm_id;    /* id for linear memory */
sym_t  g_sp_id;    /* id for stack pointer global */
size_t g_sdbaddr;  /* static data allocation start */
size_t g_stacksz;  /* stack size in bytes */

/* initialization */
void init_wcpl(dsbuf_t *pincv, dsbuf_t *plibv, long optlvl)
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
  g_wasi_mod = intern("wasi_snapshot_preview1");
  g_env_mod = intern("env");
  g_lm_id = intern("__linear_memory");
  g_sp_id = intern("__stack_pointer");
  g_sdbaddr = 1024; /* >0, 16-aligned: address 0 reserved for NULL */
  g_stacksz = 64*1024; /* 64K default */
} 

void fini_wcpl(void)
{
  freebuf(g_ibases);
  freebuf(g_lbases);
  g_wasi_mod = g_env_mod = 0;
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


/* type predicates and operations */

static bool ts_numerical(ts_t ts) 
{
  return TS_CHAR <= ts && ts <= TS_DOUBLE;
}

static bool ts_bulk(ts_t ts) 
{
  return ts == TS_ARRAY || ts == TS_STRUCT || ts == TS_UNION;
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
    case TS_CHAR:  case TS_UCHAR: 
    case TS_SHORT: case TS_USHORT:
    case TS_INT:   
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

/* produce numerical constant instruction from literal */
static void asm_numerical_const(ts_t ts, numval_t *pval, inscode_t *pic)
{
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
        if (!arithmetic_eval_to_int(pcn, &count) || count <= 0)
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

/* static eval value */
typedef struct seval {
  ts_t ts;      /* TS_PTR or ts_numerical */
  numval_t val; /* ts_numerical */
  sym_t id;     /* TS_PTR */
} seval_t; 

/* evaluate pn expression statically, putting result into pr */
bool static_eval(node_t *pn, seval_t *pr)
{
  switch (pn->nt) {
    case NT_LITERAL: {
      if (ts_numerical(pn->ts)) {
        pr->ts = pn->ts;
        pr->val = pn->val;
        return true;
      }
    } break;
    case NT_INTRCALL: {
      if (pn->intr == INTR_SIZEOF || pn->intr == INTR_ALIGNOF) {
        size_t size, align; assert(ndlen(pn) == 1);
        measure_type(ndref(pn, 0), pn, &size, &align, 0);
        pr->ts = TS_INT; 
        if (pn->intr == INTR_SIZEOF) pr->val.i = (int)size;
        else pr->val.i = (int)align;
        return true;
      } else if (pn->intr == INTR_OFFSETOF) {
        size_t offset; assert(ndlen(pn) == 2 && ndref(pn, 1)->nt == NT_IDENTIFIER);
        offset = measure_offset(ndref(pn, 0), pn, ndref(pn, 1)->name, NULL);
        pr->ts = TS_INT; 
        pr->val.i = (int)offset;
        return true;
      }
    } break;
    case NT_PREFIX: {
      seval_t rx; bool ok = false; 
      assert(ndlen(pn) == 1);
      if (pn->op == TT_AND && ndref(pn, 0)->nt == NT_IDENTIFIER) {
        pr->ts = TS_PTR; pr->val.i = 0; pr->id = ndref(pn, 0)->name;
        ok = true;
      } else if (static_eval(ndref(pn, 0), &rx)) {
        if (ts_numerical(rx.ts)) {
          numval_t vz; ts_t tz = numval_unop(pn->op, rx.ts, rx.val, &vz);
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
      if (static_eval(ndref(pn, 0), &rx) && static_eval(ndref(pn, 1), &ry)) {
        if (ts_numerical(rx.ts) && ts_numerical(ry.ts)) {
          numval_t vz; ts_t tz = numval_binop(pn->op, rx.ts, rx.val, ry.ts, ry.val, &vz);
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
      if (arithmetic_eval_to_int(ndref(pn, 0), &cond)) {
        if (cond) ok = static_eval(ndref(pn, 1), &r);
        else ok = static_eval(ndref(pn, 2), &r);
        if (ok) *pr = r; /* can be num or ptr! */
      } 
      return ok;
    } break;
    case NT_CAST: {
      seval_t rx; ts_t tc; bool ok = false;
      assert(ndlen(pn) == 2); assert(ndref(pn, 0)->nt == NT_TYPE);
      tc = ndref(pn, 0)->ts; 
      if (ts_numerical(tc) && static_eval(ndref(pn, 1), &rx) && ts_numerical(rx.ts)) {
        numval_t vc = rx.val; numval_convert(tc, rx.ts, &vc);
        pr->ts = tc; pr->val = vc; ok = true;
      }
      return ok;
    } break;
  }
  return false;
}

/* evaluate integer expression pn statically, putting result into pi */
bool arithmetic_eval_to_int(node_t *pn, int *pri)
{
  seval_t r; bool ok = false;
  if (static_eval(pn, &r) && ts_numerical(r.ts)) {
    numval_convert(TS_INT, r.ts, &r.val);
    *pri = (int)r.val.i;
    ok = true;
  }
  return ok;
}

/* evaluate pn expression statically, putting result into prn (numbers only) */
bool arithmetic_eval(node_t *pn, node_t *prn)
{
  seval_t r; bool ok = false;
  if (static_eval(pn, &r) && ts_numerical(r.ts)) {
    ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); 
    prn->ts = r.ts; prn->val = r.val; 
    ok = true;
  }
  return ok;
}

static void check_static_assert(node_t *pn)
{
  if (pn->intr == INTR_SASSERT) {
    node_t *pen, *psn; int res;
    assert(ndlen(pn) == 1 || ndlen(pn) == 2);
    pen = ndref(pn, 0), psn = ndlen(pn) == 2 ? ndref(pn, 1) : NULL;
    if (psn && (psn->nt != NT_LITERAL || psn->ts != TS_STRING))
      neprintf(pn, "unexpected 2nd arg of static_assert (string literal expected)");
    if (!arithmetic_eval_to_int(pen, &res))
      n2eprintf(pen, pn, "unexpected lhs of static_assert comparison (static test expected)");
    if (res == 0) {
      if (!psn) neprintf(pn, "static_assert failed");
      else neprintf(pn, "static_assert failed (%s)", chbdata(&psn->data));
    }
  }
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
      }
      return false;
    case NT_PREFIX:
      assert(ndlen(pn) == 1); 
      switch (pn->op) {
        case TT_PLUS:  case TT_MINUS: case TT_TILDE: case TT_NOT:
          return arithmetic_constant_expr(ndref(pn, 0));
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
      } 
      return false;
    case NT_COND:
      assert(ndlen(pn) == 3); 
      return arithmetic_constant_expr(ndref(pn, 0)) && 
             arithmetic_constant_expr(ndref(pn, 1)) && arithmetic_constant_expr(ndref(pn, 2));
    case NT_CAST: 
      assert(ndlen(pn) == 2); assert(ndref(pn, 0)->nt == NT_TYPE);
      return ts_numerical(ndref(pn, 0)->ts) && arithmetic_constant_expr(ndref(pn, 1));
  }
  return false;
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
    fprintf(stderr, "%s final=%d, hide=%d =>\n", symname(pn->name), final, hide);
    dump_node(pin, stderr);
  }
}

/* check if this global variable type is legal in WCPL */
static void vardef_check_type(node_t *ptn)
{
  assert(ptn->nt == NT_TYPE);
  switch (ptn->ts) {
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
      assert(ndlen(ptn) == 2);
      if (ndref(ptn, 1)->nt == NT_NULL) {
        neprintf(ptn, "not supported: fexible array as global variable type");
      }
      break;
    case TS_STRUCT:
    case TS_UNION:
      break;
    default:
      neprintf(ptn, "global variables of this type are not supported");
  }
}

#if 0
/* check if this expression will produce valid const code */
static void check_constnd(sym_t mmod, node_t *pn, node_t *pvn)
{
  /* valid ins: t.const ref.null ref.func global.get referring to imported const */
  switch (pn->nt) {
    case NT_LITERAL: /* compiles to t.const ref.null */
      return; 
    case NT_IDENTIFIER: { /* may compile to valid global.get */
      const node_t *pin = lookup_global(pn->name);
      /* all our imported globals are const, so we need to check module only */ 
      if (pin && pin->name != mmod) return; 
    } break;  
  }
  n2eprintf(pn, pvn, "non-constant initializer expression");
}
#endif

/* initialize preallocated bulk data with display initializer */
static void initialize_bulk_data(watie_t *pd, size_t off, node_t *ptn, node_t *pdn)
{
  if (ts_bulk(ptn->ts) && pdn->nt == NT_DISPLAY) {
    /* element-by-element initialization */
    if (ndref(pdn, 0)->ts != TS_VOID && !same_type(ptn, ndref(pdn, 0)))
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
        else initialize_bulk_data(pd, off+offi, petn, ndref(pdn, j));
        offi += size;
      }
      if (offi < asize) neprintf(pdn, "too few initializers for array");
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
          initialize_bulk_data(pd, off+offi, pfni, ndref(pdn, j));
        } else {
          neprintf(pdn, "initializer is too short");
        }
        if (ptn->ts == TS_UNION) break; /* first member only! */
      }
      if (j != ndlen(pdn)) 
        neprintf(pdn, "initializer is too long");
    }
  } else if (ts_numerical(ptn->ts)) {
    seval_t r; buf_t cb = mkchb();
    if (!static_eval(pdn, &r) || !ts_numerical(r.ts)) 
      neprintf(pdn, "non-constant initializer");
    switch (r.ts) {
      case TS_CHAR:   binchar((int)r.val.i, &cb); break;
      case TS_UCHAR:  binuchar((unsigned)r.val.u, &cb); break;
      case TS_SHORT:  binshort((int)r.val.i, &cb); break;
      case TS_USHORT: binushort((unsigned)r.val.u, &cb); break;
      case TS_LONG:   /* wasm32 */
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
    seval_t r; const node_t *pin;
    if (!static_eval(pdn, &r) || r.ts != TS_PTR) 
      neprintf(pdn, "constant address initializer expected");
    assert(r.id); pin = lookup_global(r.id);
    if (!pin) neprintf(pdn, "unknown name in address initializer");
    assert(pin->nt == NT_IMPORT && ndlen(pin) == 1 && ndcref(pin, 0)->nt == NT_TYPE);
    if (ts_bulk(ndcref(pin, 0)->ts)) {
      inscode_t *pic = icbnewbk(&pd->code);
      pic->in = IN_REF_DATA; pic->id = r.id; pic->arg2.mod = pin->name;
      pic->arg.i = r.val.i; /* 0 or numerical offset in 'elements' */
      if (r.val.i != 0) { /* have to use scale factor to get byte offset */
        /* got to make sure that static_eval allows things like &intarray[4] */
        neprintf(pdn, "NYI: offset in address initializer");
      }      
    } else {
      neprintf(pdn, "address initializer should be a reference to a nonscalar type");
    }
  }
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
  vardef_check_type(ptn);
#if 0
  /* if initializer is present, check it for constness */  
  if (pin) { 
    assert(pin->nt == NT_ASSIGN && ndlen(pin) == 2);
    assert(ndref(pin, 0)->nt == NT_IDENTIFIER && ndref(pin, 0)->name == pdn->name);
    /* todo: try to const-fold it first! */
    check_constnd(mmod, ndref(pin, 1), pin);
  }
#endif
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
      watie_t *pd = watiebnewbk(&g_curpwm->exports, IEK_DATA);
      pd->mod = mmod; pd->id = pdn->name;
      measure_type(ptn, pdn, &size, &align, 0);
      bufresize(&pd->data, size); /* fills with zeroes */
      pd->align = align;
      if (pin) {
        assert(pin->nt == NT_ASSIGN && ndlen(pin) == 2);
        assert(ndref(pin, 0)->nt == NT_IDENTIFIER && ndref(pin, 0)->name == pdn->name);
        initialize_bulk_data(pd, 0, ptn, ndref(pin, 1));
      }
      pd->mut = MT_VAR;
    } else { /* compiles to a global */
      watie_t *pg = watiebnewbk(&g_curpwm->exports, IEK_GLOBAL);
      pg->mod = mmod; pg->id = pdn->name;
      pg->exported = !hide;
      if (pin) {
        seval_t r; assert(pin->nt == NT_ASSIGN && ndlen(pin) == 2);
        assert(ndref(pin, 0)->nt == NT_IDENTIFIER && ndref(pin, 0)->name == pdn->name);
        if (!static_eval(ndref(pin, 1), &r) || !ts_numerical(r.ts)) 
          neprintf(pin, "non-constant initializer");
        asm_numerical_const(r.ts, &r.val, &pg->ic);
      }  
      pg->mut = MT_VAR;    
    }
  }
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
      case TS_CHAR: case TS_UCHAR:   /* ok but widened to int */
      case TS_SHORT: case TS_USHORT: /* ok but widened to int */
      case TS_INT: case TS_UINT: 
      case TS_LONG: case TS_ULONG:  
      case TS_LLONG: case TS_ULLONG:  
      case TS_FLOAT: case TS_DOUBLE:
      case TS_PTR: 
        break;
      case TS_ARRAY:
        assert(ndlen(ptni) == 2);
        if (ndref(ptni, 1)->nt == NT_NULL) { /* t foo[] == t* foo */
          ndbrem(&ptni->body, 1); ptni->ts = TS_PTR;
        } else neprintf(ptni, "not supported: array as function parameter/return type");
        break;
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
        neprintf(ptni, "function arguments of this type are not supported");
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

/* convert node wasm conventions, simplify operators and control */
static void expr_wasmify(node_t *pn, buf_t *pvib, buf_t *plib)
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
      } /* else global: todo: static globals may need to be converted too! */
    } break;
    case NT_INTRCALL: {
      switch (pn->intr) {
        case INTR_VAETC: { /* () => ap$ */
          sym_t ap = intern("ap$");
          vi_t *pvi = bufbsearch(pvib, &ap, sym_cmp);
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
      }
    } break;
    case NT_TYPE: {
      /* nothing of interest in here */
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
      if (ndlen(pn) > 2) qsort(ndref(pn, 1), ndlen(pn)-1, sizeof(node_t), case_cmp);
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
      if (arithmetic_eval_to_int(ndref(pn, 0), &x)) set_to_int(ndref(pn, 0), x);
      else neprintf(pn, "noninteger case label"); 
      for (i = 1; i < ndlen(pn); ++i) expr_wasmify(ndref(pn, i), pvib, plib);
    } break;
    case NT_DEFAULT: {
      size_t i;
      for (i = 0; i < ndlen(pn); ++i) expr_wasmify(ndref(pn, i), pvib, plib);
    } break;
    case NT_GOTO: {
      int *pil = bufsearch(plib, &pn->name, sym_cmp);
      if (!pil) neprintf(pn, "goto to unknown or mispositioned label");
      else assert(pil[1] == 0); /* not a break/continue target */
    } break;
    case NT_RETURN: {
      sym_t rp = intern("rp$"), fp = intern("fp$"); 
      vi_t *prvi, *pfvi; node_t *psn;
      if (ndlen(pn) > 0) {
        assert(ndlen(pn) == 1);
        expr_wasmify(ndref(pn, 0), pvib, plib);
      }
      prvi = bufbsearch(pvib, &rp, sym_cmp);
      pfvi = bufbsearch(pvib, &fp, sym_cmp);
      if (prvi) { /* return x => { *rp$ = x; return; } */
        if (ndlen(pn) != 1) neprintf(pn, "return value expected"); 
        wrap_node(lift_arg0(pn), NT_ASSIGN); pn->op = TT_ASN;
        psn = ndinsfr(pn, NT_PREFIX); psn->op = TT_STAR;
        psn = ndinsbk(psn, NT_IDENTIFIER); psn->name = rp;
        wrap_node(pn, NT_BLOCK); 
        if (pfvi) { /* insert freea(fp$) if needed */
          psn = ndinsbk(pn, NT_INTRCALL); psn->intr = INTR_FREEA;
          psn = ndinsbk(psn, NT_IDENTIFIER); psn->name = fp;
        }
        ndinsbk(pn, NT_RETURN);
      } else if (pfvi) { /* just insert freea(fp$) */
        wrap_node(pn, NT_BLOCK);
        psn = ndinsfr(pn, NT_INTRCALL); psn->intr = INTR_FREEA;
        psn = ndinsbk(psn, NT_IDENTIFIER); psn->name = fp;
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
    default: {
      size_t i;
      for (i = 0; i < ndlen(pn); ++i) expr_wasmify(ndref(pn, i), pvib, plib);
    } break;
  }
}

/* convert function to wasm conventions, simplify */
static void fundef_wasmify(node_t *pdn)
{
  /* special registers (scalar/poiner vars with internal names): 
   * fp$ (frame pointer), ap$ (... data pointer) */
  node_t *ptn = ndref(pdn, 0), *pbn = ndref(pdn, 1);
  buf_t vib = mkbuf(sizeof(vi_t)); node_t frame = mknd();
  ndbuf_t asb; buf_t lib; size_t i; 
  ndbinit(&asb); bufinit(&lib, sizeof(int)*2); /* <tt, lbsym> */
  
  /* init frame structure, in case there are auto locals*/
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
        neprintf(pdni, "function arguments of this type are not supported");
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
    expr_wasmify(pni, &vib, &lib);
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

  buffini(&lib);
  ndbfini(&asb);
  ndfini(&frame);
  buffini(&vib);
}

/* 'register' (wasm local var) info for typecheck */
typedef struct ri {
  sym_t name; /* register or global var name */
  const node_t *ptn; /* NT_TYPE (not owned) */
} ri_t;

static const node_t *lookup_var_type(sym_t name, buf_t *prib, sym_t *pmod)
{
  ri_t *pri; const node_t *pgn;
  if ((pri = bufbsearch(prib, &name, sym_cmp)) != NULL) {
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
static inscode_t *asm_load(ts_t tsto, ts_t tsfrom, unsigned off, icbuf_t *pdata)
{ /* tsfrom may be narrow, tsto is integral-promoted */
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
    }
  }  
  return TS_VOID; /* 0 */
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
  else if (!cast_compatible(ptn, patn)) neprintf(prn, "compile: impossible cast");
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
      if (rts == TS_FLOAT) v.f = 0.0; else v.d = 0.0;
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
  } else { /* good as-is */
    return pan;
  }
}

/* compile x to serve as an integer selector */
node_t *compile_intsel(node_t *prn, node_t *pan)
{
  node_t *patn = acode_type(pan);
  if (patn->ts != TS_INT) {
    node_t tn = mknd(); ndsettype(&tn, TS_INT);
    if (!assign_compatible(&tn, patn)) neprintf(prn, "integer expression is expected"); 
    if (!same_type(&tn, patn)) pan = compile_cast(prn, &tn, pan);
  }
  return pan;
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
    acode_swapin(pcn, pan1); 
    acode_swapin(pcn, pan2); 
    acode_swapin(pcn, pan3);
    acode_pushin(pcn, IN_SELECT);
  } else { 
    /* fixme: 'if'-'else' block is needed */
    unsigned bt = ts_to_blocktype(pctn->ts); 
    assert(bt >= VT_F64 && bt <= VT_I32);
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
  } else { 
    /* {scalar_t* | x} => {IP(scalar_t) | {scalar_t* | x} fetch} */
    node_t *pcn = npnewcode(prn), *ptn = ndpushbk(pcn, petn);
    if (ts_numerical(petn->ts)) ptn->ts = ts_integral_promote(petn->ts);
    acode_swapin(pcn, pan);
    asm_load(ptn->ts, petn->ts, 0, &pcn->data);
    return pcn;
  }
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
    acode_swapin(psn, pan); petn = acode_type(pcn);
    if (off > 0) { acode_pushin_uarg(psn, IN_I32_CONST, off); acode_pushin(psn, IN_I32_ADD); }
    acode_swapin(pcn, psn);
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
      neprintf(prn, "can't modify lval: unexpected type on the right");
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
      neprintf(prn, "can't modify lval: unexpected type");
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
  } else {
    neprintf(prn, "not a valid lvalue");
    return NULL;
  }
} 

/* compile call expression; pdn != NULL for suspected bulk return call */
static node_t *compile_call(node_t *prn, node_t *pfn, buf_t *pab, node_t *pdn)
{
  node_t *pcn = npnewcode(prn), *pftn = acode_type(pfn), *psn; size_t i;
  inscode_t cic; size_t alen = ndlen(pftn); bool etc = false;
  if (pftn->ts != TS_FUNCTION) 
    n2eprintf(ndref(prn, 0), prn, "can't call non-function type");
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
    n2eprintf(ndref(prn, 0), prn, 
      "non-global-identifier function expressions are not yet supported");
  }
  for (i = 0; i < buflen(pab) && (!etc || i < ndlen(pftn)-2); ++i) {
    node_t **ppani = bufref(pab, i), *pani = *ppani;
    node_t *ptni = acode_type(pani), *pftni = ndref(pftn, i+1);
    if (pftni->nt == NT_VARDECL) pftni = ndref(pftni, 0);
    if (!assign_compatible(pftni, ptni))
      n2eprintf(pani, prn, "can't pass argument[%d]: unexpected type", i);
    if (!same_type(pftni, ptni)) pani = compile_cast(prn, pftni, pani);
    acode_swapin(pcn, pani); /* leaves arg on stack */
  }
  if (etc && i < buflen(pab)) {
    /* collect remaining arguments into a stack array of va_arg_t */
    sym_t pname = rpalloc(VT_I32); /* arg frame ptr (wasm32) */
    /* frame is an array of uint64s (va_arg_t), aligned to 16 */
    size_t asz = 8, nargs = buflen(pab)-i, framesz = (nargs*asz + 15) & ~0xFLL, basei;
    inscode_t *pic = icbnewfr(&pcn->data); pic->in = IN_REGDECL;
    pic->id = pname; pic->arg.u = VT_I32; /* wasm32 pointer */
    acode_pushin_id_mod(pcn, IN_GLOBAL_GET, g_sp_id, g_env_mod);
    acode_pushin_id(pcn, IN_LOCAL_TEE, pname);
    acode_pushin_uarg(pcn, IN_I32_CONST, framesz);
    acode_pushin(pcn, IN_I32_SUB);
    acode_pushin_id_mod(pcn, IN_GLOBAL_SET, g_sp_id, g_env_mod); 
    /* start of va_arg_t[nargs] array is now in pname, fill it */
    for (basei = i; i < buflen(pab); ++i) {
      node_t **ppani = bufref(pab, i), *pani = *ppani;
      node_t *ptni = acode_type(pani); size_t size, align;
      measure_type(ptni, prn, &size, &align, 0);
      if (ts_bulk(ptni->ts) || size > asz || align > asz)
        n2eprintf(pani, prn, "can't pass argument[%d]: unsupported type for ... call", i);
      if (ts_numerical(ptni->ts)) assert(ptni->ts == ts_integral_promote(ptni->ts));
      acode_pushin_id(pcn, IN_LOCAL_GET, pname);
      acode_swapin(pcn, pani); /* arg on stack */
      asm_store(ptni->ts, ptni->ts, (i-basei)*asz, &pcn->data);
    }
    acode_pushin_id(pcn, IN_LOCAL_GET, pname); 
    /* put the call instruction followed by sp restore */
    asm_pushbk(&pcn->data, &cic);
    acode_pushin_id(pcn, IN_LOCAL_GET, pname);
    acode_pushin_id_mod(pcn, IN_GLOBAL_SET, g_sp_id, g_env_mod); 
  } else {
    /* just put the call instruction */
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
    acode_pushin(pcn, IN_ELSE);
    acode_swapin(pcn, compile_stm(pan3));
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

/* compile return statement */
static node_t *compile_return(node_t *prn, node_t *pan, const node_t *ptn)
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
  acode_pushin(pcn, IN_RETURN);
  return pcn;
}

/* compile bulk assignment from two reference codes */
static node_t *compile_bulkasn(node_t *prn, node_t *pdan, node_t *psan)
{
  node_t *ptdan = acode_type(pdan), *ptsan = acode_type(psan), *ptn;
  node_t *pcn; size_t size, align;
  if (!ts_bulk(ptdan->ts) || !ts_bulk(ptsan->ts)) return NULL;
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

/* intern the string literal, return data id */
static sym_t intern_strlit(node_t *pn)
{
  size_t n = watieblen(&g_curpwm->exports);
  watie_t *pd = watiebnewbk(&g_curpwm->exports, IEK_DATA);
  pd->mod = g_curmod;
  pd->id = internf("ds%d$", (int)n);
  chbcpy(&pd->data, &pn->data); 
  pd->align = (pn->ts == TS_LSTRING) ? 4 /* wchar_t */ : 1 /* char */;
  pd->mut = MT_CONST;
  return pd->id;
}

/* compile expr/statement (that is, convert it to asm tree); prib is var/reg info,
 * ret is return type for statements, NULL for expressions returning a value
 * NB: code that has bulk type actually produces a pointer (cf. arrays) */
static node_t *expr_compile(node_t *pn, buf_t *prib, const node_t *ret)
{
  node_t *pcn = NULL; numval_t v;
  if (arithmetic_constant_expr(pn)) {
    node_t nd = mknd();
    if (ret && getwlevel() < 1) nwprintf(pn, "warning: constant value is not used"); 
    if (arithmetic_eval(pn, &nd)) ndswap(&nd, pn);
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
      if (!ptn) neprintf(pn, "compile: undefined identifier");
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
              pic->id = g_sp_id; pic->arg2.mod = g_env_mod;
              pic = icbnewfr(&pan->data); pic->in = IN_GLOBAL_GET; 
              pic->id = g_sp_id; pic->arg2.mod = g_env_mod;
              acode_pushin(pan, IN_I32_SUB); 
              acode_pushin_id_mod(pan, IN_GLOBAL_SET, g_sp_id, g_env_mod);
            } else { /* calc frame size dynamically */
              sym_t sname = rpalloc(VT_I32), pname = rpalloc(VT_I32); /* wasm32 */
              pic = icbnewfr(&pan->data); pic->in = IN_REGDECL; pic->id = sname; pic->arg.u = VT_I32;
              if (!ts_numerical(ptni->ts) || ts_arith_common(ptni->ts, TS_ULONG) != TS_ULONG)
                neprintf(pn, "invalid alloca() intrinsic's argument");
              if (ptni->ts != TS_ULONG) asm_numerical_cast(TS_ULONG, ptni->ts, &pan->data);
              acode_pushin_uarg(pan, IN_I32_CONST, 15);  
              acode_pushin(pan, IN_I32_ADD); 
              acode_pushin_uarg(pan, IN_I32_CONST, 0xFFFFFFF0);
              acode_pushin(pan, IN_I32_AND);   
              acode_pushin_id(pan, IN_LOCAL_SET, sname);
              acode_pushin_id_mod(pan, IN_GLOBAL_GET, g_sp_id, g_env_mod);
              acode_pushin_id(pan, IN_LOCAL_TEE, pname);
              acode_pushin_id(pan, IN_LOCAL_GET, sname);
              pic = icbnewfr(&pan->data); pic->in = IN_REGDECL; pic->id = pname; pic->arg.u = VT_I32;
              acode_pushin(pan, IN_I32_SUB);
              acode_pushin_id_mod(pan, IN_GLOBAL_SET, g_sp_id, g_env_mod);
              acode_pushin_id(pan, IN_LOCAL_GET, pname);
            }
            wrap_type_pointer(ndsettype(acode_type(pan), TS_VOID));
            pcn = pan;
          } else {
            neprintf(pn, "alloca() intrinsic expects one argument");
          }
        } break;
        case INTR_FREEA: {
          if (ndlen(pn) == 1) {
            node_t *pan = expr_compile(ndref(pn, 0), prib, NULL);
            node_t *ptni = acode_type(pan);
            if (ptni->ts != TS_PTR)
              neprintf(pn, "invalid freea() intrinsic's argument");
            acode_pushin_id_mod(pan, IN_GLOBAL_SET, g_sp_id, g_env_mod);
            ndsettype(acode_type(pan), TS_VOID);
            pcn = pan;
          } else {
            neprintf(pn, "freea() intrinsic expects one argument");
          }
        } break;
        case INTR_VAETC: 
        case INTR_VAARG: assert(false); /* wasmified */
        case INTR_SASSERT: {
          check_static_assert(pn);
          pcn = ndsettype(npnewcode(pn), TS_VOID);
        } break;
      }
    } break;
    case NT_CAST: {
      pcn = compile_cast(pn, ndref(pn, 0), expr_compile(ndref(pn, 1), prib, NULL));
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
        op = (pn->op >= TT_PLUS_ASN && pn->op <= TT_SHR_ASN) ? pn->op - 1 : 0;  
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
      size_t i; bool end = false; assert(ret);
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
      pcn = compile_return(pn, pan, ret);
    } break;
    case NT_BREAK:
    case NT_CONTINUE: assert(false); break; /* dewasmified */
  }

  if (!pcn) neprintf(pn, "failed to compile expression");
  assert(pcn->nt == NT_ACODE);
  if (pcn->nt == NT_ACODE) assert(ndlen(pcn) > 0 && ndref(pcn, 0)->nt == NT_TYPE);
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
  bufqsort(&rib, sym_cmp);

  /* go over body statements */
  for (/* use current i */; i < ndlen(pbn); ++i) {
    node_t *pni = ndref(pbn, i);
    node_t *pcn = expr_compile(pni, &rib, ret);
    ndswap(ndnewbk(prbn), compile_stm(pcn));
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

/* simple peephole optimization */
static void fundef_peephole(node_t *pcn, int optlvl)
{
  icbuf_t *picb = &pcn->data; size_t i, nmods;
repeat:
  for (nmods = i = 0; i < icblen(picb); /* rem or bump i */) {
    bool nexti = i+1;
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
          icbrem(picb, i-1);
          ++nmods; nexti = i-1;
        }
      } else if (pthisi->in == IN_LOCAL_GET) {
        if (pprevi->in == IN_LOCAL_SET && pprevi->id && pprevi->id == pthisi->id) {
          pprevi->in = IN_LOCAL_TEE;
          icbrem(picb, i);
          ++nmods; nexti = i;
        }
      }
    }
    i = nexti;
  }
  if (nmods > 0 && --optlvl > 0) goto repeat;
}

/* convert type node to a valtype */
static void tn2vt(node_t *ptn, vtbuf_t *pvtb)
{
  assert(ptn->nt == NT_TYPE);
  switch (ptn->ts) {
    case TS_VOID: /* put nothing */ break;
    case TS_CHAR:  case TS_UCHAR:
    case TS_SHORT: case TS_USHORT:
    case TS_INT:   case TS_UINT:  
      *vtbnewbk(pvtb) = VT_I32; break;  
    case TS_LONG:  case TS_ULONG:  
    case TS_PTR: 
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
  fundef_check_type(ndref(pn, 0));
  if (pn->name == intern("main")) {
    if (ndlen(ptn) == 1 && ndref(ptn, 0)->ts == TS_INT) {
      pm->main = MAIN_VOID;
    } else { /* expect argc, argv */ 
      int sigok = true; node_t *psn;
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
  if (pn->intr == INTR_SASSERT) check_static_assert(pn);
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
      measure_type(ndref(pn, 0), pn, &size, &align, 0);
      fprintf(stderr, "size: %d, align: %d\n", (int)size, (int)align);
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
        if (ptn->ts == TS_FUNCTION) fundef_check_type(ptn);
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
static sym_t process_module(const char *fname, wat_module_t *pm)
{
  pws_t *pw; node_t nd = mknd(); sym_t mod = 0;
  pw = newpws(fname);
  if (pw) {
    /* this should be workspace #0 */
    assert(pwsid(pw) == 0);
    assert(pwscurmod(pw));
    while (parse_top_form(pw, &nd)) {
      if (nd.nt == NT_INCLUDE) {
        bool sys = (nd.op != TT_STRING);
        process_include(pw, nd.startpos, sys, nd.name, pm);
      } else {
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
  
  mod = process_module(ifname, pwm); assert(mod);
  pwm->name = mod;

  /* add standard imports/exports */
  switch (pwm->main) {
    case MAIN_ABSENT: {
      /* fixme: add standard imports */
      watie_t *pi;
      /* (import "env" "__stack_pointer" (global $env:__stack_pointer (mut i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
      pi->mod = g_env_mod; pi->id = g_sp_id; 
      pi->mut = MT_VAR; pi->vt = VT_I32;
      /* (import "env" "__linear_memory" (memory $env:__linear_memory 0)) */
      pi = watiebnewbk(&pwm->imports, IEK_MEM); 
      pi->mod = g_env_mod; pi->id = g_lm_id; 
      pi->lt = LT_MIN; pi->n = 0;
    } break;
    case MAIN_ARGC_ARGV: {
      /* fixme: add standard startup code and exports */
      exprintf("NYI: can't set up environment for main(argc, argv)");
    } break;
    case MAIN_VOID: {
      /* fixme: add standard startup code and exports */
      watie_t *pi, *pf; inscode_t *pic; sym_t r;
      /* (import "env" "__stack_pointer" (global $env:__stack_pointer (mut i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_GLOBAL);
      pi->mod = g_env_mod; pi->id = g_sp_id; 
      pi->mut = MT_VAR; pi->vt = VT_I32;
      /* (import "env" "__linear_memory" (memory $env:__linear_memory 0)) */
      pi = watiebnewbk(&pwm->imports, IEK_MEM);
      pi->mod = g_env_mod; pi->id = g_lm_id; 
      pi->lt = LT_MIN; pi->n = 0;
      /* (import "wasi_snapshot_preview1" "proc_exit" (func $... (param i32))) */
      pi = watiebnewbk(&pwm->imports, IEK_FUNC);
      pi->mod = g_wasi_mod; pi->id = intern("proc_exit");
      *vtbnewbk(&pi->fs.partypes) = VT_I32;
      /* (func $_start ...) */
      pf = watiebnewbk(&pwm->exports, IEK_FUNC); 
      pf->mod = mod; pf->id = intern("_start"); /* fs is void->void */
      pf->exported = true;
      pic = icbnewbk(&pf->code); pic->in = IN_REGDECL; pic->id = (r = intern("res")); pic->arg.u = VT_I32;
      pic = icbnewbk(&pf->code); pic->in = IN_CALL; pic->id = intern("main"); pic->arg2.mod = mod; 
      pic = icbnewbk(&pf->code); pic->in = IN_LOCAL_TEE; pic->id = r;
      pic = icbnewbk(&pf->code); pic->in = IN_IF; pic->arg.u = BT_VOID;
      pic = icbnewbk(&pf->code); pic->in = IN_LOCAL_GET; pic->id = r;
      pic = icbnewbk(&pf->code); pic->in = IN_CALL; pic->id = intern("proc_exit"); pic->arg2.mod = g_wasi_mod;
      pic = icbnewbk(&pf->code); pic->in = IN_END;
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
          if (getverbosity() > 0) {
            fprintf(stderr, "imported global %s:%s =>\n", symname(pn->name), symname(id));
            dump_node(ptn, stderr);
          }
        } 
      }
    }
  }

  /* done */
  fini_compiler();
}


int main(int argc, char **argv)
{
  int opt;
  const char *ifile_arg = "-";
  const char *ofile_arg = NULL;
  bool c_opt = false;
  long lvl_arg = 2;
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
     "  -O lvl    Optimization level; defaults to 2\n"
     "  -o ofile  Output file\n"
     "  -I path   Add include path (must end with separator)\n"
     "  -L path   Add library path (must end with separator)\n"
     "  -h        This help");
  while ((opt = egetopt(argc, argv, "wvqcO:o:L:I:h")) != EOF) {
    switch (opt) {
      case 'w':  setwlevel(3); break;
      case 'v':  incverbosity(); break;
      case 'q':  incquietness(); break;
      case 'c':  c_opt = true; break;
      case 'O':  lvl_arg = atol(eoptarg); break;
      case 'o':  ofile_arg = eoptarg; break;
      case 'I':  dsbpushbk(&incv, &eoptarg); break;
      case 'L':  dsbpushbk(&libv, &eoptarg); break;
      case 'h':  eusage("WCPL 0.01 built on "__DATE__);
    }
  }

  init_wcpl(&incv, &libv, lvl_arg);

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
      if (strsuf(ifile_arg, ".o") || strsuf(ifile_arg, ".wo") || strsuf(ifile_arg, ".wat")) {
        logef("# loading object module from %s\n", ifile_arg);
        read_wat_module(ifile_arg, pwm);
        logef("# object module '%s' loaded\n", symname(pwm->name));
      } else {
        logef("# compiling source file %s\n", ifile_arg);
        compile_module_to_wat(ifile_arg, pwm);
        logef("# object module '%s' created\n", symname(pwm->name));
      }
    }
    link_wat_modules(&wmb, &wm);

    logef("# all modules linked successfully\n", symname(g_wasi_mod));
    write_wat_module(&wm, stdout);

    wat_module_fini(&wm);
    wat_module_buf_fini(&wmb); 
  }

  fini_wcpl();
  dsbfini(&incv); dsbfini(&libv);
  return EXIT_SUCCESS;
}

