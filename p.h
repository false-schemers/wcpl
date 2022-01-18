/* p.h (wcpl parser) -- esl */

#ifndef _P_H_INCLUDED
#define _P_H_INCLUDED

/* module names and files */

extern char *path_filename(const char *path);
extern sym_t base_from_path(const char *path);
extern sym_t modname_from_path(const char *fname);
extern struct pws_tag *pws_from_modname(sym_t mod, buf_t *pbases);

/* lexical token types */
typedef enum tt_tag {
  TT_WHITESPACE,  TT_IDENTIFIER,  TT_INT,         TT_UINT,
  TT_LONG,        TT_ULONG,       TT_LLONG,       TT_ULLONG,
  TT_FLOAT,       TT_DOUBLE,      /* no LDOUBLE / VEC128 */
  TT_CHAR,        TT_LCHAR,       TT_STRING,      TT_LSTRING,      
  TT_LPAR,        TT_RPAR,        TT_LBRK,        TT_RBRK,        
  TT_LBRC,        TT_RBRC,        TT_COMMA,       TT_SEMICOLON,   
  TT_PLUS,        TT_MINUS,       TT_HASH,        TT_HASHHASH,    
  TT_ELLIPSIS,    TT_AND_AND,     TT_OR_OR,       TT_NOT,
  TT_AND,         TT_AND_ASN,     TT_OR,          TT_OR_ASN,
  TT_XOR,         TT_XOR_ASN,     TT_REM,         TT_REM_ASN,
  TT_SLASH,       TT_SLASH_ASN,   TT_STAR,        TT_STAR_ASN, 
  TT_PLUS_PLUS,   TT_PLUS_ASN,    TT_MINUS_MINUS, TT_MINUS_ASN,
  TT_SHL,         TT_SHL_ASN,     TT_SHR,         TT_SHR_ASN,
  TT_LT,          TT_LE,          TT_GT,          TT_GE,
  TT_EQ,          TT_NE,          TT_DOT,         TT_ARROW,
  TT_ASN,         TT_QMARK,       TT_COLON,       TT_TILDE,
  TT_AUTO_KW,     TT_BREAK_KW,    TT_CASE_KW,     TT_CHAR_KW,
  TT_CONST_KW,    TT_CONTINUE_KW, TT_DEFAULT_KW,  TT_DO_KW,
  TT_DOUBLE_KW,   TT_ELSE_KW,     TT_ENUM_KW,     TT_EXTERN_KW,
  TT_FLOAT_KW,    TT_FOR_KW,      TT_GOTO_KW,     TT_IF_KW,
  TT_INT_KW,      TT_LONG_KW,     TT_REGISTER_KW, TT_RETURN_KW,
  TT_SHORT_KW,    TT_SIGNED_KW,   TT_STATIC_KW,   TT_STRUCT_KW,
  TT_SWITCH_KW,   TT_TYPEDEF_KW,  TT_UNION_KW,    TT_UNSIGNED_KW, 
  TT_VOID_KW,     TT_VOLATILE_KW, TT_WHILE_KW,    TT_TYPE_NAME,
  TT_MACRO_NAME,  TT_ENUM_NAME,   TT_INTR_NAME,   TT_EOF = -1
} tt_t;

/* parser workspaces */
typedef struct pws_tag {
  int id;             /* sequential id of this pws or -1 */
  dstr_t infile;      /* current input file name or "-" */
  sym_t curmod;       /* current module name or 0 */
  FILE *input;        /* current input stream */
  bool inateof;       /* current input is exausted */  
  chbuf_t inchb;      /* input buffer for parser */  
  buf_t lsposs;       /* line start positions */  
  size_t discarded;   /* count of discarded chars */  
  chbuf_t chars;      /* line buffer of chars */
  int curi;           /* current input position in buf */
  bool gottk;         /* lookahead token is available */
  tt_t ctk;           /* lookahead token type */
  chbuf_t token;      /* lookahead token char data */
  char *tokstr;       /* lookahead token string */
  int pos;            /* absolute pos of la token start */
} pws_t;

extern void init_workspaces(void);
extern void fini_workspaces(void);

/* create new workspace for inifile; return NULL if inifile can't be opened */
extern pws_t *newpws(const char *infile);
/* close existing workspace, leaving only data used for error reporting */
extern void closepws(pws_t *pw);

/* buffers of grammar nodes */
#define ndbuf_t buf_t

/* symbol table */
extern buf_t g_syminfo; /* triples of <sym_t, tt_t, info> sorted by sym */
extern ndbuf_t g_nodes; /* nodes referred to by some syminfos */
extern void init_symbols(void);
extern void fini_symbols(void);

/* split input into tokens */
extern tt_t lex(pws_t *pw, chbuf_t *pcb);
/* report parsing error, possibly printing location information, and exit */
extern void reprintf(pws_t *pw, int startpos, const char *fmt, ...);

/* storage class specifier */
typedef enum sc_tag {
  SC_NONE,   SC_EXTERN,  SC_STATIC,
  SC_AUTO,   SC_REGISTER
} sc_t;

/* base type specifier */
typedef enum ts_tag {
  /* pseudo-types: */    TS_VOID,    TS_ETC, 
  TS_CHAR,   TS_UCHAR,   TS_SHORT,   TS_USHORT,
  TS_INT,    TS_UINT,    TS_LONG,    TS_ULONG,
  TS_LLONG,  TS_ULLONG,  TS_FLOAT,   TS_DOUBLE,
  /* literals only: */   TS_STRING,  TS_LSTRING, 
  TS_ENUM,   TS_STRUCT,  TS_UNION,
  TS_PTR,    TS_ARRAY,   TS_FUNCTION
} ts_t;

/* numerical value */
typedef union numval_tag {
  long long i; unsigned long long u; 
  float f; double d;
} numval_t;

/* grammar node data type */
typedef enum nt_tag {
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
  NT_BLOCK,       /* {...} */
  NT_IF,          /* if (x) y else z */
  NT_SWITCH,      /* switch (x) {case ...} */
  NT_CASE,        /* case x: y... */
  NT_DEFAULT,     /* default: y... */
  NT_WHILE,       /* while (x) y */
  NT_DO,          /* do x while (y) */
  NT_FOR,         /* for (a; b; c) t */
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
typedef struct node_tag {
  nt_t nt;        /* node type */
  int pwsid;      /* id of origin pws */
  int startpos;   /* start position in origin pws */
  sym_t name;     /* IDENTIFIER/TYPE/VARDECL/FUNDEF */
  chbuf_t data;   /* LITERAL (string); data is in utf-8 */
  numval_t val;   /* LITERAL (numeric); type defined by ts */
  tt_t op;        /* POSTFIX/PREFIX/INFIX; op is tt of an operator */
  ts_t ts;        /* TYPE/LITERAL */
  sc_t sc;        /* VARDECL/FUNDEF/IMPORT */
  ndbuf_t body;   /* SUBSCRIPT/CALL/CAST/UNARY/BINARY/... */
} node_t;

extern node_t* ndinit(node_t* pn);
extern void ndicpy(node_t* mem, const node_t* pn);
extern void ndfini(node_t* pn);
extern void ndcpy(node_t* pn, const node_t* pr);
extern void ndset(node_t *dst, nt_t nt, int pwsid, int startpos);
extern void ndclear(node_t* pn);
#define ndswap(pn1, pn2) memswap(pn1, pn2, sizeof(node_t))
#define ndbinit(mem) bufinit(mem, sizeof(node_t))
extern void ndbicpy(buf_t* mem, const ndbuf_t* pb);
extern void ndbfini(ndbuf_t* pb);
extern void ndbclear(ndbuf_t* pb);
#define ndblen(pb) buflen(pb)
#define ndbref(pb, i) ((node_t*)bufref(pb, i))
#define ndbnewbk(pb) ndinit(bufnewbk(pb))
#define ndbpushbk(pb, pn) ndicpy(bufnewbk(pb), pn)
#define ndbrem(pb, i) do { ndbuf_t *_pb = pb; size_t _i = i; ndfini(bufref(_pb, _i)); bufrem(_pb, _i); } while(0)
#define ndlen(pn) ndblen(&(pn)->body)
#define ndref(pn, i) ndbref(&(pn)->body, i)
#define ndnewbk(pn) ndbnewbk(&(pn)->body)

/* parse single top-level declaration/definition */
extern bool parse_top_form(pws_t *pw, node_t *pn);
/* parse and collect top-level declarations/definitions */
extern void parse_translation_unit(pws_t *pw, ndbuf_t *pnb);
/* report node error, possibly printing location information, and exit */
extern void neprintf(node_t *pn, const char *fmt, ...);
/* report node error, printing second node location, and exit */
extern void n2eprintf(node_t *pn, node_t *pn2, const char *fmt, ...);
/* post imported/forward symbol to symbol table */
extern const node_t *post_symbol(sym_t mod, node_t *pvn);
/* return ptr to NT_IMPORT node or NULL if name is not declared */
extern const node_t *lookup_global(sym_t name);
/* return ptr to NT_TYPE node or NULL if name is not declared */
extern const node_t *lookup_eus_type(ts_t ts, sym_t name); /* enum/union/struct */

/* simple comparison of NT_TYPE nodes for equivalence */
extern bool same_type(const node_t *ptn1, const node_t *ptn2);
/* dump node in s-expression format */
extern void dump_node(const node_t *pn, FILE *out);

#endif /* ndef _P_H_INCLUDED */
