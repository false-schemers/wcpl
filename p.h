/* p.h (wcpl parser) -- esl */

#pragma once

/* module names and files */
extern sym_t base_from_path(const char *path);
extern sym_t modname_from_path(const char *fname);
extern struct pws *pws_from_modname(bool sys, sym_t mod);

/* lexical token types (NB: order is significant!) */
typedef enum tt {
  TT_WHITESPACE,  TT_IDENTIFIER,  TT_INT,         TT_UINT,
  TT_LONG,        TT_ULONG,       TT_LLONG,       TT_ULLONG,
  TT_FLOAT,       TT_DOUBLE,      /* no LDOUBLE / VEC128 */
  TT_CHAR,        TT_LCHAR,       TT_STRING,      TT_LSTRING,      
  TT_LPAR,        TT_RPAR,        TT_LBRK,        TT_RBRK,        
  TT_LBRC,        TT_RBRC,        TT_COMMA,       TT_SEMICOLON,   
  TT_ELLIPSIS,    TT_AND_AND,     TT_OR_OR,       TT_NOT,
  TT_PLUS_PLUS,   TT_MINUS_MINUS, TT_HASH,        TT_HASHHASH,    
  TT_PLUS,        TT_PLUS_ASN,    TT_MINUS,       TT_MINUS_ASN,
  TT_AND,         TT_AND_ASN,     TT_OR,          TT_OR_ASN,
  TT_XOR,         TT_XOR_ASN,     TT_REM,         TT_REM_ASN,
  TT_SLASH,       TT_SLASH_ASN,   TT_STAR,        TT_STAR_ASN, 
  TT_SHL,         TT_SHL_ASN,     TT_SHR,         TT_SHR_ASN,
  TT_LT,          TT_LE,          TT_GT,          TT_GE,
  TT_EQ,          TT_NE,          TT_DOT,         TT_ARROW,
  TT_ASN,         TT_QMARK,       TT_COLON,       TT_TILDE,
  TT_ASM_KW,      TT_AUTO_KW,     TT_BREAK_KW,    TT_CASE_KW,   
  TT_CHAR_KW,     TT_CONST_KW,    TT_CONTINUE_KW, TT_DEFAULT_KW,
  TT_DO_KW,       TT_DOUBLE_KW,   TT_ELSE_KW,     TT_ENUM_KW,
  TT_EXTERN_KW,   TT_FLOAT_KW,    TT_FOR_KW,      TT_GOTO_KW,
  TT_IF_KW,       TT_INT_KW,      TT_LONG_KW,     TT_REGISTER_KW,
  TT_RETURN_KW,   TT_SHORT_KW,    TT_SIGNED_KW,   TT_STATIC_KW,
  TT_STRUCT_KW,   TT_SWITCH_KW,   TT_TYPEDEF_KW,  TT_UNION_KW,
  TT_UNSIGNED_KW, TT_VOID_KW,     TT_VOLATILE_KW, TT_WHILE_KW,    
  TT_TYPE_NAME,   TT_MACRO_NAME,  TT_ENUM_NAME,   TT_INTR_NAME,   
  TT_EOF = -1
} tt_t;

/* parser workspaces */
typedef struct pws pws_t;

extern void init_workspaces(void);
extern void fini_workspaces(void);

/* create new workspace for inifile; return NULL if inifile can't be opened */
extern pws_t *newpws(const char *infile);
/* close existing workspace, leaving only data used for error reporting */
extern void closepws(pws_t *pw);
/* access to pws public fields */
extern int pwsid(pws_t *pw);
extern sym_t pwscurmod(pws_t *pw);

/* buffers of grammar nodes */
typedef buf_t ndbuf_t;

/* symbol table */
extern buf_t g_syminfo; /* triples of <sym_t, tt_t, info> sorted by sym */
extern ndbuf_t g_nodes; /* nodes referred to by some syminfos */
extern void init_symbols(void);
extern void fini_symbols(void);

/* report parsing error, possibly printing location information, and exit */
extern void reprintf(pws_t *pw, int startpos, const char *fmt, ...);

/* storage class specifier */
typedef enum sc {
  SC_NONE,   SC_EXTERN,  SC_STATIC,
  SC_AUTO,   SC_REGISTER
} sc_t;

/* base type specifier */
typedef enum ts {
  TS_VOID,     TS_ETC,                    TS_BOOL, 
  TS_CHAR,     TS_UCHAR,     TS_SHORT,    TS_USHORT,
  TS_INT,      TS_UINT,      TS_LONG,     TS_ULONG,
  TS_LLONG,    TS_ULLONG,    TS_FLOAT,    TS_DOUBLE,
  TS_STRING,   TS_LSTRING,   TS_ENUM,     TS_ARRAY,
  TS_STRUCT,   TS_UNION,     TS_FUNCTION, TS_PTR
} ts_t;

/* intrinsics and special forms */
typedef enum intr {
  INTR_NONE,   INTR_ALLOCA,
  INTR_ASU32,  INTR_ASFLT,   
  INTR_ASU64,  INTR_ASDBL,   INTR_ACODE,
  INTR_SIZEOF, INTR_ALIGNOF, INTR_OFFSETOF,
  INTR_GENERIC,
  INTR_VAETC,  INTR_VAARG,   INTR_SASSERT
} intr_t;

/* grammar node data type */
typedef enum nt {
  NT_NULL = 0,    /* NOP */
  NT_LITERAL,     /* numerical/char/string literals */
  NT_IDENTIFIER,  /* variable reference */
  NT_SUBSCRIPT,   /* x[i] */
  NT_CALL,        /* x(...) */
  NT_INTRCALL,    /* a(...) */
  NT_CAST,        /* (t)x*/  
  NT_POSTFIX,     /* x op */
  NT_PREFIX,      /* op x */
  NT_INFIX,       /* x op y */
  NT_COND,        /* x ? y : z */
  NT_ASSIGN,      /* x op= y */
  NT_COMMA,       /* x, y */
  NT_ACODE,       /* (t)asm(...) */
  NT_BLOCK,       /* {...} {l: ...} {... l:} */
  NT_IF,          /* if (x) y else z */
  NT_SWITCH,      /* switch (x) {case ...} */
  NT_CASE,        /* case x: y... */
  NT_DEFAULT,     /* default: y... */
  NT_WHILE,       /* while (x) y */
  NT_DO,          /* do x while (y) */
  NT_FOR,         /* for (a; b; c) t */
  NT_GOTO,        /* goto l */
  NT_RETURN,      /* return x */
  NT_BREAK,       /* break */
  NT_CONTINUE,    /* continue */
  NT_TYPE,        /* t */
  NT_VARDECL,     /* t x */
  NT_DISPLAY,     /* {x, ...} */
  NT_FUNDEF,      /* t x(...) {...} */
  NT_TYPEDEF,     /* typedef t i */
  NT_MACRODEF,    /* #define x y */
  NT_INCLUDE,     /* #include <m> */
  NT_IMPORT,      /* m t (symbol table only) */
} nt_t;

typedef struct node {
  nt_t nt;        /* node type */
  int pwsid;      /* id of origin pws */
  int startpos;   /* start position in origin pws */
  sym_t name;     /* IDENTIFIER/TYPE/VARDECL/FUNDEF/INTRCALL */
  buf_t data;     /* LITERAL(chbuf)/ACODE(icbuf) */
  numval_t val;   /* LITERAL(numeric); type defined by ts */
  intr_t intr;    /* INTRCALL */
  tt_t op;        /* POSTFIX/PREFIX/INFIX; op is tt of an operator */
  ts_t ts;        /* TYPE/LITERAL */
  sc_t sc;        /* VARDECL/FUNDEF/DISPLAY/IMPORT */
  ndbuf_t body;   /* SUBSCRIPT/CALL/CAST/UNARY/BINARY/... */
} node_t;

extern node_t mknd(void);
extern node_t *ndinit(node_t* pn);
extern node_t *ndicpy(node_t* mem, const node_t* pn);
extern void ndfini(node_t* pn);
extern node_t *ndcpy(node_t* pn, const node_t* pr);
extern node_t *ndset(node_t *dst, nt_t nt, int pwsid, int startpos);
extern node_t *ndsettype(node_t *dst, ts_t ts);
extern void ndclear(node_t* pn);
extern void ndrem(node_t* pn, size_t i);
#define ndswap(pn1, pn2) (memswap(pn1, pn2, sizeof(node_t)))
#define ndbinit(mem) (bufinit(mem, sizeof(node_t)))
extern void ndbicpy(buf_t* mem, const ndbuf_t* pb);
extern void ndbfini(ndbuf_t* pb);
extern void ndbclear(ndbuf_t* pb);
#define ndblen(pb) (buflen(pb))
#define ndbref(pb, i) ((node_t*)bufref(pb, i))
#define ndbnewbk(pb) (ndinit(bufnewbk(pb)))
#define ndbinsnew(pb, i) (ndinit(bufins(pb, i)))
#define ndbpushbk(pb, pn) (ndicpy(bufnewbk(pb), pn))
#define ndbrem(pb, i) do { ndbuf_t *_pb = pb; size_t _i = i; ndfini(bufref(_pb, _i)); bufrem(_pb, _i); } while(0)
#define ndlen(pn) (ndblen(&(pn)->body))
#define ndref(pn, i) (ndbref(&(pn)->body, i))
#define ndcref(pn, i) (ndbref(&((node_t*)(pn))->body, i))
#define ndnewfr(pn) (ndbinsnew(&(pn)->body, 0))
#define ndnewbk(pn) (ndbnewbk(&(pn)->body))
#define ndpushbk(pn, psn) (ndbpushbk(&(pn)->body, psn))
#define ndinsnew(pn, i) (ndbinsnew(&(pn)->body, i))
extern node_t *ndinsfr(node_t *pn, nt_t nt);
extern node_t *ndinsbk(node_t *pn, nt_t nt);

/* node pool (allocated nodes are freed at once) */
extern void init_nodepool(void);
extern void fini_nodepool(void);
extern void clear_nodepool(void);
extern node_t *npalloc(void);
extern node_t *npnew(nt_t nt, int pwsid, int startpos);
extern node_t *npnewcode(const node_t *psn);
extern node_t *npdup(const node_t *pr);

/* unique register name pool (per-function) */
extern void init_regpool(void);
extern void fini_regpool(void);
extern void clear_regpool(void);
extern sym_t rpalloc(valtype_t vt);
extern sym_t rpalloc_label(void);

/* compare NT_TYPE nodes for 'deep' equivalence */
extern bool same_type(const node_t *pctn1, const node_t *pctn2);

/* node builders; modify pn in place and return it */
/* wrap node into nt node as a subnode */
extern node_t *wrap_node(node_t *pn, nt_t nt);
/* wrap node into NT_SUBSCRIPT node */
extern node_t *wrap_subscript(node_t *pn, node_t *psn);
/* wrap expr node into NT_POSTFIX type node */
extern node_t *wrap_postfix_operator(node_t *pn, tt_t op, sym_t id);
/* wrap expr node into NT_PREFIX type node */
extern node_t *wrap_unary_operator(node_t *pn, int startpos, tt_t op);
/* wrap expr node into NT_CAST type node */
extern node_t *wrap_cast(node_t *pcn, node_t *pn);
/* wrap expr node into NT_INFIX with second expr */
extern node_t *wrap_binary(node_t *pn, tt_t op, node_t *pn2);
/* wrap expr node into NT_COND with second/third exprs */
extern node_t *wrap_conditional(node_t *pn, node_t *pn2, node_t *pn3);
/* wrap expr node into NT_ASSIGN with second expr */
extern node_t *wrap_assignment(node_t *pn, tt_t op, node_t *pn2);
/* wrap expr node into NT_COMMA with second expr */
extern node_t *wrap_comma(node_t *pn, node_t *pn2);
/* wrap type node into TS_PTR type node */
extern node_t *wrap_type_pointer(node_t *pn);
/* wrap type node and expr node into TS_ARRAY type node */
extern node_t *wrap_type_array(node_t *pn, node_t *pi);
/* wrap type node and vec of type nodes into TS_FUNCTION type node */
extern node_t *wrap_type_function(node_t *pn, ndbuf_t *pnb);
/* flatten TS_ARRAY type node into TS_PTR type node */
extern node_t *flatten_type_array(node_t *pn);
/* flatten node into its argument #0 */
extern node_t *lift_arg0(node_t *pn);
/* flatten node into its argument #1 */
extern node_t *lift_arg1(node_t *pn);
/* replace content with int literal n, keep posinfo */
extern node_t *set_to_int(node_t *pn, int n);

/* parse single top-level declaration/definition */
extern bool parse_top_form(pws_t *pw, node_t *pn);
/* parse and collect top-level declarations/definitions */
extern void parse_translation_unit(pws_t *pw, ndbuf_t *pnb);
/* report node error, possibly printing location information, and exit */
extern void neprintf(const node_t *pn, const char *fmt, ...);
/* report node error, printing second node location, and exit */
extern void n2eprintf(const node_t *pn, const node_t *pn2, const char *fmt, ...);
/* report node warning, possibly printing location information */
extern void nwprintf(const node_t *pn, const char *fmt, ...);
/* post imported/forward symbol to symbol table; forward unless final */
extern const node_t *post_symbol(sym_t mod, node_t *pvn, bool final, bool hide);
/* return ptr to NT_IMPORT node or NULL if name is not declared */
extern const node_t *lookup_global(sym_t name);
/* mark NT_IMPORT node as referenced (affects imports only) */
extern void mark_global_referenced(const node_t *pgn);
/* return ptr to NT_TYPE node or NULL if name is not declared */
extern const node_t *lookup_eus_type(ts_t ts, sym_t name); /* enum/union/struct */

/* simple comparison of NT_TYPE nodes for equivalence */
extern bool same_type(const node_t *ptn1, const node_t *ptn2);
/* dump node in s-expression format */
extern void dump_node(const node_t *pn, FILE *out);

/* messaging and debug help */
extern const char *ts_name(ts_t ts);
extern const char *sc_name(sc_t sc);
extern const char *intr_name(intr_t intr);
extern const char *op_name(tt_t op); /* operators only */
