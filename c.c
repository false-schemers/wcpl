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

/* initialization */
void init_compiler(const char *larg, const char *lenv)
{
  bufinit(&g_bases, sizeof(sym_t));
  *(sym_t*)bufnewbk(&g_bases) = intern(""); 
  if (larg) *(sym_t*)bufnewbk(&g_bases) = intern(larg); 
  if (lenv) *(sym_t*)bufnewbk(&g_bases) = intern(lenv);
  init_workspaces();
  init_symbols();
}

void fini_compiler(void)
{
  buffini(&g_bases);
  fini_workspaces();
  fini_symbols();
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

/* evaluate pn expression statically, putting result into prn (VERY limited) */
bool static_eval(node_t *pn, node_t *prn)
{
  /* for now, deal with ints only */
  switch (pn->nt) {
    case NT_LITERAL: {
      if (pn->ts == TS_INT) {
        ndcpy(prn, pn);
        return true;
      }
    } break;
    case NT_INTRCALL: {
      if (pn->name == intern("sizeof") || pn->name == intern("alignof")) {
        size_t size, align; assert(ndlen(pn) == 1);
        measure_type(ndref(pn, 0), pn, &size, &align, 0);
        ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); prn->ts = TS_INT; 
        if (pn->name == intern("sizeof")) chbsetf(&prn->data, "%d", (int)size);
        else chbsetf(&prn->data, "%d", (int)align);
        return true;
      } else if (pn->name = intern("offsetof")) {
        size_t offset; assert(ndlen(pn) == 2 && ndref(pn, 1)->nt == NT_IDENTIFIER);
        offset = measure_offset(ndref(pn, 0), pn, ndref(pn, 1)->name);
        ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); prn->ts = TS_INT; 
        chbsetf(&prn->data, "%d", (int)offset);
        return true;
      }
    } break;
    case NT_PREFIX: {
      node_t nx; bool ok = false;
      ndinit(&nx);
      assert(ndlen(pn) == 1); 
      if (static_eval(ndref(pn, 0), &nx)) {
        if (nx.nt == NT_LITERAL && nx.ts == TS_INT) {
          int x = atoi(chbdata(&nx.data)), z;
          switch (pn->op) {
            case TT_PLUS:  z = x; ok = true; break;
            case TT_MINUS: z = -x; ok = true; break;
            case TT_NOT:   z = !x; ok = true; break;
            case TT_TILDE: z = ~x; ok = true; break;
          }
          if (ok) {
            ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); prn->ts = TS_INT;
            chbsetf(&prn->data, "%d", z); 
          } 
        }
      }
      ndfini(&nx);
      return ok;
    } break;
    case NT_INFIX: {
      node_t nx, ny; bool ok = false;
      ndinit(&nx), ndinit(&ny);
      assert(ndlen(pn) == 2); 
      if (static_eval(ndref(pn, 0), &nx) && static_eval(ndref(pn, 1), &ny)) {
        if (nx.nt == NT_LITERAL && nx.ts == TS_INT && ny.nt == NT_LITERAL && ny.ts == TS_INT) {
          int x = atoi(chbdata(&nx.data)), y = atoi(chbdata(&ny.data)), z;
          switch (pn->op) {
            case TT_PLUS:  z = x + y; ok = true; break;
            case TT_MINUS: z = x - y; ok = true; break;
            case TT_STAR:  z = x * y; ok = true; break;
            case TT_AND:   z = x & y; ok = true; break;
            case TT_OR:    z = x | y; ok = true; break;
            case TT_XOR:   z = x ^ y; ok = true; break;
            case TT_SLASH: if (y) z = x / y, ok = true; break;
            case TT_REM:   if (y) z = x % y, ok = true; break;
            case TT_SHL:   if (y >= 0 && y < 32) z = x << y, ok = true; break;
            case TT_SHR:   if (y >= 0 && y < 32) z = x >> y, ok = true; break;
            case TT_EQ:    z = x == y; ok = true; break;
            case TT_NE:    z = x != y; ok = true; break;
            case TT_LT:    z = x < y; ok = true; break;
            case TT_GT:    z = x > y; ok = true; break;
            case TT_LE:    z = x <= y; ok = true; break;
            case TT_GE:    z = x >= y; ok = true; break;
          }
          if (ok) {
            ndset(prn, NT_LITERAL, pn->pwsid, pn->startpos); prn->ts = TS_INT;
            chbsetf(&prn->data, "%d", z); 
          } 
        }
      }
      ndfini(&nx), ndfini(&ny);
      return ok;
    } break;
  }
  return false;
}

/* evaluate integer expression pn statically, putting result into pi (VERY limited) */
bool static_eval_to_int(node_t *pn, int *pri)
{
  node_t nd; bool ok = false;
  if (static_eval(pn, ndinit(&nd)) && nd.nt == NT_LITERAL && nd.ts == TS_INT) {
    *pri = atoi(chbdata(&nd.data)), ok = true;
  }
  ndfini(&nd);
  return ok;
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
    switch (ptni->ts) {
      case TS_VOID: /* ok if alone */ 
        if (i == 0) break; /* return type can be void */
        neprintf(ptn, "unexpected void function argument type");
      case TS_INT: case TS_UINT:
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
    node_t nd; ndinit(&nd);
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
  if (pn->name == intern("static_assert")) {
    node_t *pen, *psn; int res;
    assert(ndlen(pn) == 2);
    pen = ndref(pn, 0), psn = ndref(pn, 1);
    if (psn->nt != NT_LITERAL || psn->ts != TS_STRING) 
      neprintf(pn, "unexpected 2nd arg of static_assert (string literal expected)");
    if (!static_eval_to_int(pen, &res))
      n2eprintf(pen, pn, "unexpected lhs of static_assert comparison (static test expected)");
    if (res == 0) 
      neprintf(pn, "static_assert failed (%s)", chbdata(&psn->data));
  } else neprintf(pn, "unexpected top-level form");
}

/* process single node (from module or include) */
static void process_node(sym_t mainmod, node_t *pn, module_t *pm)
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
  pws_t *pwi; node_t nd;
  ndinit(&nd);
  pwi = pws_from_modname(name, &g_bases);
  if (pwi) {
    /* this should be workspace #1... */
    assert(pwi->id > 0);
    while (parse_top_form(pwi, &nd)) {
      if (nd.nt == NT_INCLUDE) {
        process_include(pwi, nd.startpos, nd.name, pm);
      } else {
        process_node(0, &nd, pm);
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
  pws_t *pw; node_t nd; sym_t mod = 0;
  ndinit(&nd);
  pw = newpws(fname);
  if (pw) {
    /* this should be workspace #0 */
    assert(pw->id == 0);
    assert(pw->curmod);
    while (parse_top_form(pw, &nd)) {
      if (nd.nt == NT_INCLUDE) {
        process_include(pw, nd.startpos, nd.name, pm);
      } else {
        process_node(pw->curmod, &nd, pm);
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
