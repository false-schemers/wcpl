/* w.h (wasm interface) -- esl */

#ifndef _W_H_INCLUDED
#define _W_H_INCLUDED


/* wasm binary types */

typedef enum valtype {
  VT_UNKN = 0,
  BT_VOID = 0x40,
  RT_EXTERNREF = 0x6F,
  RT_FUNCREF = 0x70,
  VT_V128 = 0x7B,
  VT_F64 = 0x7C,
  VT_F32 = 0x7D,
  VT_I64 = 0x7E,
  VT_I32 = 0x7F
} valtype_t;

typedef enum functype {
  FT_FUNCTYPE = 0x60 /* followed by two vectors of parameter and result types */
} functype_t;

typedef enum limtype {
  LT_MIN = 0x00,     /* followed by single u32 */
  LT_MINMAX = 0x01   /* followed by two u32s */
} limtype_t;

typedef enum muttype {
  MT_CONST = 0x00,   /* follows valtype */
  MT_VAR = 0x01      /* follows valtype */
} muttype_t;

typedef enum secid {
  SI_CUSTOM = 0,
  SI_TYPE = 1,
  SI_IMPORT = 2,
  SI_FUNCTION = 3,
  SI_TABLE = 4,
  SI_MEMORY = 5,
  SI_GLOBAL = 6,
  SI_EXPORT = 7,
  SI_START = 8,
  SI_ELEMENT = 9,
  SI_CODE = 10,
  SI_DATA = 11,
  SI_DATACOUNT = 12
} secid_t;

typedef enum entkind {
  EK_FUNC = 0x00,
  EK_TABLE = 0x01,
  EK_MEM = 0x02,
  EK_GLOBAL = 0x03,
} entkind_t;

typedef enum dsmode {
  DS_ACTIVE /* ex bv */ = 0x00,
  DS_PASSIVE /* bv */ = 0x01,
  DS_ACTIVE_MI /* mi ex bv */ = 0x02 /* not used */
} dsmode_t;

typedef enum elemkind {
  ELK_FUNCREF = 0x00
} elemkind_t;

typedef enum esmode {
  ES_ACTIVE_EIV /* e fiv */ = 0x00,
  ES_PASSIVE_KIV /* k fiv */ = 0x01,
  ES_ACTIVE_TEKIV /* ti e k fiv */ = 0x02,
  ES_DECLVE_KIV /* k fiv */ = 0x03,
  ES_ACTIVE_EEV /* e ev */ = 0x04,
  ES_PASSIVE_TEV /* rt ev */ = 0x05,
  ES_ACTIVE_IETEV /* ti e rt ev */ = 0x06,
  ES_DECLVE_TEV /* rt ev */ = 0x07
} esmode_t;

typedef enum instr {
  IN_UNREACHABLE = 0x00,
  IN_NOP = 0x01,
  IN_BLOCK /* [t]? */ = 0x02,
  IN_LOOP /* [t]? */ = 0x03,
  IN_IF /* [t]? */ = 0x04,
  IN_ELSE = 0x05,
  /* reserved: 0x06..0x0A */
  IN_END = 0x0B,
  IN_BR /* l */ = 0x0C,
  IN_BR_IF /* l */ = 0x0D,
  IN_BR_TABLE /* l* l */ = 0x0E,
  IN_RETURN = 0x0F,
  IN_CALL /* x */ = 0x10,
  IN_CALL_INDIRECT /* x */ = 0x11,
  IN_RETURN_CALL /* x */ = 0x12, /* not in core-1 */
  IN_RETURN_CALL_INDIRECT /* x */ = 0x13, /* not in core-1 */
  /* reserved: 0x14..0x19 */
  IN_DROP = 0x1A,
  IN_SELECT = 0x1B,
  IN_SELECT_T = 0x1C, /* not in core-1 */
  /* reserved: 0x1D..0x1F */
  IN_LOCAL_GET /* x */ = 0x20,
  IN_LOCAL_SET /* x */ = 0x21,
  IN_LOCAL_TEE /* x */ = 0x22,
  IN_GLOBAL_GET /* x */ = 0x23,
  IN_GLOBAL_SET /* x */ = 0x24,
  IN_TABLE_GET /* x */ = 0x25, /* not in core-1 */
  IN_TABLE_SET /* x */ = 0x26, /* not in core-1 */
  /* reserved: 0x27 */
  IN_I32_LOAD /* memarg */ = 0x28,
  IN_I64_LOAD /* memarg */ = 0x29,
  IN_F32_LOAD /* memarg */ = 0x2A,
  IN_F64_LOAD /* memarg */ = 0x2B,
  IN_I32_LOAD8_S /* memarg */ = 0x2C,
  IN_I32_LOAD8_U /* memarg */ = 0x2D,
  IN_I32_LOAD16_S /* memarg */ = 0x2E,
  IN_I32_LOAD16_U /* memarg */ = 0x2F,
  IN_I64_LOAD8_S /* memarg */ = 0x30,
  IN_I64_LOAD8_U /* memarg */ = 0x31,
  IN_I64_LOAD16_S /* memarg */ = 0x32,
  IN_I64_LOAD16_U /* memarg */ = 0x33,
  IN_I64_LOAD32_S /* memarg */ = 0x34,
  IN_I64_LOAD32_U /* memarg */ = 0x35,
  IN_I32_STORE /* memarg */ = 0x36,
  IN_I64_STORE /* memarg */ = 0x37,
  IN_F32_STORE /* memarg */ = 0x38,
  IN_F64_STORE /* memarg */ = 0x39,
  IN_I32_STORE8 /* memarg */ = 0x3A,
  IN_I32_STORE16 /* memarg */ = 0x3B,
  IN_I64_STORE8 /* memarg */ = 0x3C,
  IN_I64_STORE16 /* memarg */ = 0x3D,
  IN_I64_STORE32 /* memarg */ = 0x3E,
  IN_MEMORY_SIZE = 0x3F,
  IN_MEMORY_GROW = 0x40,
  IN_I32_CONST /* i32 */ = 0x41,
  IN_I64_CONST /* i64 */ = 0x42,
  IN_F32_CONST /* f32 */ = 0x43,
  IN_F64_CONST /* f64 */ = 0x44,
  IN_I32_EQZ = 0x45,
  IN_I32_EQ = 0x46,
  IN_I32_NE = 0x47,
  IN_I32_LT_S = 0x48,
  IN_I32_LT_U = 0x49,
  IN_I32_GT_S = 0x4A,
  IN_I32_GT_U = 0x4B,
  IN_I32_LE_S = 0x4C,
  IN_I32_LE_U = 0x4D,
  IN_I32_GE_S = 0x4E,
  IN_I32_GE_U = 0x4F,
  IN_I64_EQZ = 0x50,
  IN_I64_EQ = 0x51,
  IN_I64_NE = 0x52,
  IN_I64_LT_S = 0x53,
  IN_I64_LT_U = 0x54,
  IN_I64_GT_S = 0x55,
  IN_I64_GT_U = 0x56,
  IN_I64_LE_S = 0x57,
  IN_I64_LE_U = 0x58,
  IN_I64_GE_S = 0x59,
  IN_I64_GE_U = 0x5A,
  IN_F32_EQ = 0x5B,
  IN_F32_NE = 0x5C,
  IN_F32_LT = 0x5D,
  IN_F32_GT = 0x5E,
  IN_F32_LE = 0x5F,
  IN_F32_GE = 0x60,
  IN_F64_EQ = 0x61,
  IN_F64_NE = 0x62,
  IN_F64_LT = 0x63,
  IN_F64_GT = 0x64,
  IN_F64_LE = 0x65,
  IN_F64_GE = 0x66,
  IN_I32_CLZ = 0x67,
  IN_I32_CTZ = 0x68,
  IN_I32_POPCNT = 0x69,
  IN_I32_ADD = 0x6A,
  IN_I32_SUB = 0x6B,
  IN_I32_MUL = 0x6C,
  IN_I32_DIV_S = 0x6D,
  IN_I32_DIV_U = 0x6E,
  IN_I32_REM_S = 0x6F,
  IN_I32_REM_U = 0x70,
  IN_I32_AND = 0x71,
  IN_I32_OR = 0x72,
  IN_I32_XOR = 0x73,
  IN_I32_SHL = 0x74,
  IN_I32_SHR_S = 0x75,
  IN_I32_SHR_U = 0x76,
  IN_I32_ROTL = 0x77,
  IN_I32_ROTR = 0x78,
  IN_I64_CLZ = 0x79,
  IN_I64_CTZ = 0x7A,
  IN_I64_POPCNT = 0x7B,
  IN_I64_ADD = 0x7C,
  IN_I64_SUB = 0x7D,
  IN_I64_MUL = 0x7E,
  IN_I64_DIV_S = 0x7F,
  IN_I64_DIV_U = 0x80,
  IN_I64_REM_S = 0x81,
  IN_I64_REM_U = 0x82,
  IN_I64_AND = 0x83,
  IN_I64_OR = 0x84,
  IN_I64_XOR = 0x85,
  IN_I64_SHL = 0x86,
  IN_I64_SHR_S = 0x87,
  IN_I64_SHR_U = 0x88,
  IN_I64_ROTL = 0x89,
  IN_I64_ROTR = 0x8A,
  IN_F32_ABS = 0x8B,
  IN_F32_NEG = 0x8C,
  IN_F32_CEIL = 0x8D,
  IN_F32_FLOOR = 0x8E,
  IN_F32_TRUNC = 0x8F,
  IN_F32_NEAREST = 0x90,
  IN_F32_SQRT = 0x91,
  IN_F32_ADD = 0x92,
  IN_F32_SUB = 0x93,
  IN_F32_MUL = 0x94,
  IN_F32_DIV = 0x95,
  IN_F32_MIN = 0x96,
  IN_F32_MAX = 0x97,
  IN_F32_COPYSIGN = 0x98,
  IN_F64_ABS = 0x99,
  IN_F64_NEG = 0x9A,
  IN_F64_CEIL = 0x9B,
  IN_F64_FLOOR = 0x9C,
  IN_F64_TRUNC = 0x9D,
  IN_F64_NEAREST = 0x9E,
  IN_F64_SQRT = 0x9F,
  IN_F64_ADD = 0xA0,
  IN_F64_SUB = 0xA1,
  IN_F64_MUL = 0xA2,
  IN_F64_DIV = 0xA3,
  IN_F64_MIN = 0xA4,
  IN_F64_MAX = 0xA5,
  IN_F64_COPYSIGN = 0xA6,
  IN_I32_WRAP_I64 = 0xA7,
  IN_I32_TRUNC_F32_S = 0xA8,
  IN_I32_TRUNC_F32_U = 0xA9,
  IN_I32_TRUNC_F64_S = 0xAA,
  IN_I32_TRUNC_F64_U = 0xAB,
  IN_I64_EXTEND_I32_S = 0xAC,
  IN_I64_EXTEND_I32_U = 0xAD,
  IN_I64_TRUNC_F32_S = 0xAE,
  IN_I64_TRUNC_F32_U = 0xAF,
  IN_I64_TRUNC_F64_S = 0xB0,
  IN_I64_TRUNC_F64_U = 0xB1,
  IN_F32_CONVERT_I32_S = 0xB2,
  IN_F32_CONVERT_I32_U = 0xB3,
  IN_F32_CONVERT_I64_S = 0xB4,
  IN_F32_CONVERT_I64_U = 0xB5,
  IN_F32_DEMOTE_F64 = 0xB6,
  IN_F64_CONVERT_I32_S = 0xB7,
  IN_F64_CONVERT_I32_U = 0xB8,
  IN_F64_CONVERT_I64_S = 0xB9,
  IN_F64_CONVERT_I64_U = 0xBA,
  IN_F64_PROMOTE_F32 = 0xBB,
  IN_I32_REINTERPRET_F32 = 0xBC,
  IN_I64_REINTERPRET_F64 = 0xBD,
  IN_F32_REINTERPRET_I32 = 0xBE,
  IN_F64_REINTERPRET_I64 = 0xBF,
  IN_I32_EXTEND8_S = 0xC0, /* not in core-1 */
  IN_I32_EXTEND16_S = 0xC1, /* not in core-1 */
  IN_I64_EXTEND8_S = 0xC2, /* not in core-1 */
  IN_I64_EXTEND16_S = 0xC3, /* not in core-1 */
  IN_I64_EXTEND32_S = 0xC4, /* not in core-1 */
  /* reserved: 0xC5..0xCF */
  IN_REF_NULL /* t */ = 0xD0, /* not in core-1 */
  IN_REF_IS_NULL = 0xD1, /* not in core-1 */
  IN_REF_FUNC /* x */ = 0xD2, /* not in core-1 */
  /* reserved: 0xD3..0xFB */
  /* prefix byte for multibyte: 0xFC, not in core-1 */
  /* prefix byte for multibyte: 0xFD, not in core-1 */
  /* reserved: 0xFE..0xFF */
  /* extended (multibyte), not in core-1 */
  IN_I32_TRUNC_SAT_F32_S = 0xFC00,
  IN_I32_TRUNC_SAT_F32_U = 0xFC01,
  IN_I32_TRUNC_SAT_F64_S = 0xFC02,
  IN_I32_TRUNC_SAT_F64_U = 0xFC03,
  IN_I64_TRUNC_SAT_F32_S = 0xFC04,
  IN_I64_TRUNC_SAT_F32_U = 0xFC05,
  IN_I64_TRUNC_SAT_F64_S = 0xFC06,
  IN_I64_TRUNC_SAT_F64_U = 0xFC07,
  IN_MEMORY_INIT /* x */ = 0xFC08,
  IN_DATA_DROP /* x */   = 0xFC09,
  IN_MEMORY_COPY         = 0xFC0A,
  IN_MEMORY_FILL         = 0xFC0B,
  IN_TABLE_INIT/* x y */ = 0xFC0C,
  IN_ELEM_DROP /* x */   = 0xFC0D,
  IN_TABLE_COPY/* x y */ = 0xFC0E,
  IN_TABLE_GROW /* x */  = 0xFC0F,
  IN_TABLE_SIZE /* x */  = 0xFC10,
  IN_TABLE_FILL /* x */  = 0xFC11,
  /* bunch of vector instructions follow */
  /* ... */
  IN_PLACEHOLDER = -1, /* for internal use */
  IN_REGDECL = -2, /* for internal use */
  IN_REF_DATA = -3 /* for internal use */
} instr_t;

typedef enum insig {
  INSIG_NONE = 0, 
  INSIG_BT, INSIG_L, INSIG_LS_L,
  INSIG_XL, INSIG_XG, INSIG_XT,
  INSIG_X_Y, INSIG_T,
  INSIG_I32, INSIG_I64,
  INSIG_F32, INSIG_F64, 
  INSIG_MEMARG
} insig_t;


/* wasm binary representations */

typedef buf_t vtbuf_t; 
typedef struct funcsig {
  vtbuf_t partypes;   /* of valtype_t */ 
  vtbuf_t restypes;   /* of valtype_t */
} funcsig_t;

extern funcsig_t* fsinit(funcsig_t* pf);
extern void fsfini(funcsig_t* pf);
typedef buf_t fsbuf_t; 
#define fsbinit(mem) bufinit(mem, sizeof(funcsig_t))
extern void fsbfini(fsbuf_t* pb);
#define fsblen(pb) buflen(pb)
#define fsbref(pb, i) ((funcsig_t*)bufref(pb, i))
#define fsbnewbk(pb) fsinit(bufnewbk(pb))
extern unsigned fsintern(fsbuf_t* pb, funcsig_t *pfs); /* may clear pfs */
extern unsigned funcsig(fsbuf_t* pb, size_t argc, size_t retc, ...);

/* numerical value */
typedef union numval {
  long long i; unsigned long long u; 
  float f; double d;
} numval_t;

/* instruction */
/* NB: IN_BR_TABLE has count N in arg.u, and is followed by N+1 dummy IN_BR inscodes */
typedef struct inscode {
  instr_t in; 
  numval_t arg; sym_t id;
  union { unsigned u; sym_t mod; } arg2; 
} inscode_t;

typedef buf_t icbuf_t; 
#define icbinit(mem) bufinit(mem, sizeof(inscode_t))
#define icbfini(pb) buffini(pb)
#define icblen(pb) buflen(pb)
#define icbref(pb, i) ((inscode_t*)bufref(pb, i))
#define icbrem(pb, i) (bufrem(pb, i))
#define icbnewfr(pb) ((inscode_t*)bufnewfr(pb))
#define icbnewbk(pb) ((inscode_t*)bufnewbk(pb))
#define icbpopbk(pb) bufpopbk(pb)

#define vtblen(pb) buflen(pb)
#define vtbref(pb, i) ((valtype_t*)bufref(pb, i))
#define vtbnewbk(pb) ((valtype_t*)bufnewbk(pb))
#define idxblen(pb) buflen(pb)
#define idxbref(pb, i) ((unsigned*)bufref(pb, i))
#define idxbnewbk(pb) ((unsigned*)bufnewbk(pb))

typedef struct entry {
  entkind_t ek;     /* as used in import/export sections */
  sym_t mod;        /* != 0 for imported, 0 for internal */
  sym_t name;       /* != 0 for imported, may be 0 for internal */
  unsigned fsi;     /* FUNC: funcsig index */
  bool isstart;     /* FUNC: this is the start function */ 
  valtype_t vt;     /* GLOBAL/TABLE: value type (scalar) */
  muttype_t mut;    /* GLOBAL: var/const */
  limtype_t lt;     /* TABLE/MEM (imported) */
  unsigned n;       /* TABLE/MEM (imported) */
  unsigned m;       /* TABLE/MEM (imported) */
  vtbuf_t loctypes; /* FUNC: buf of valtype_t, for internal funcs only */ 
  icbuf_t code;     /* FUNC/GLOBAL/MEM: buf of inscode_t, for internals only  */
} entry_t;

extern entry_t* entinit(entry_t* pf, entkind_t ek);
extern void entfini(entry_t* pf);
typedef buf_t entbuf_t; 
#define entbinit(mem) bufinit(mem, sizeof(entry_t))
extern void entbfini(entbuf_t* pb);
#define entblen(pb) buflen(pb)
#define entbref(pb, i) ((entry_t*)bufref(pb, i))
#define entbnewbk(pb, ek) entinit(bufnewbk(pb), ek)
#define entbins(pb, i, ek) entinit(bufins(pb, i), ek)

typedef struct dseg {
  dsmode_t dsm;
  unsigned idx;
  chbuf_t data;
  buf_t code;
} dseg_t;

extern dseg_t* dseginit(dseg_t* ps, dsmode_t dsm);
extern void dsegfini(dseg_t* ps);
typedef buf_t dsegbuf_t; 
#define dsegbinit(mem) bufinit(mem, sizeof(dseg_t))
extern void dsegbfini(dsegbuf_t* pb);
#define dsegblen(pb) buflen(pb)
#define dsegbref(pb, i) ((dseg_t*)bufref(pb, i))
#define dsegbnewbk(pb, dsm) dseginit(bufnewbk(pb), dsm)

typedef struct eseg {
  esmode_t esm;
  unsigned idx; /* tableidx */
  elemkind_t ek; /* 0 (funcref) */
  valtype_t rt; /* RT_XXX only */ 
  buf_t fidxs; /* of unsigned funcidx */
  buf_t code; /* of inscode_t */
  size_t cdc; /* count of code runs in codes */
  buf_t codes; /* of cdc inscode_t runs each ending in IN_END */
} eseg_t;

extern eseg_t* eseginit(eseg_t* ps, esmode_t esm);
extern void esegfini(eseg_t* ps);
typedef buf_t esegbuf_t; 
#define esegbinit(mem) bufinit(mem, sizeof(eseg_t))
extern void esegbfini(esegbuf_t* pb);
#define esegblen(pb) buflen(pb)
#define esegbref(pb, i) ((eseg_t*)bufref(pb, i))
#define esegbnewbk(pb, esm) eseginit(bufnewbk(pb), esm)


/* wasm (binary) module for linked executable */
typedef struct wasm_module {
  fsbuf_t funcsigs;
  entbuf_t funcdefs; 
  entbuf_t tabdefs; 
  entbuf_t memdefs; 
  entbuf_t globdefs; 
  esegbuf_t elemdefs;
  dsegbuf_t datadefs;
} wasm_module_t;

extern wasm_module_t* wasm_module_init(wasm_module_t* pm);
extern void wasm_module_fini(wasm_module_t* pm);

/* write 'executable' wasm binary module */
extern void write_wasm_module(wasm_module_t* pm, FILE *pf);

/* introspection */
extern const char *instr_name(instr_t in);
extern const char *valtype_name(valtype_t vt);
extern instr_t name_instr(const char *name);
extern insig_t instr_sig(instr_t in);
extern const char *format_inscode(inscode_t *pic, chbuf_t *pcb);


/* wat text representations */
/* NB: all structures start with mod-id pair used for sorting/bsearch */

/* wat symbol info */
typedef struct wati {
  sym_t mod;
  sym_t id;
  entkind_t ek;
  bool exported;
  funcsig_t fs;   /* FUNC: funcsig index */
  valtype_t vt;   /* GLOBAL/TABLE: value type (scalar) */
  muttype_t mut;  /* GLOBAL: var/const */
  inscode_t ic;   /* GLOBAL: def init code */
  limtype_t lt;   /* TABLE/MEM (imported) */
  unsigned n;     /* TABLE/MEM (imported) */
  unsigned m;     /* TABLE/MEM (imported) */
} wati_t;

extern wati_t* watiinit(wati_t* pi);
extern void watifini(wati_t* pi);
typedef buf_t watibuf_t; 
#define watibinit(mem) bufinit(mem, sizeof(wati_t))
extern void watibfini(watibuf_t* pb);
#define watiblen(pb) buflen(pb)
#define watibref(pb, i) ((wati_t*)bufref(pb, i))
#define watibnewfr(pb) watiinit(bufnewfr(pb))
#define watibnewbk(pb) watiinit(bufnewbk(pb))

/* wat data segment */
typedef struct watd {
  sym_t mod;
  sym_t id;
  buf_t data; /* actual bytes */
} watd_t;

extern watd_t* watdinit(watd_t* ps);
extern void watdfini(watd_t* ps);
typedef buf_t watdbuf_t; 
#define watdbinit(mem) bufinit(mem, sizeof(watd_t))
extern void watdbfini(watdbuf_t* pb);
#define watdblen(pb) buflen(pb)
#define watdbref(pb, i) ((watd_t*)bufref(pb, i))
#define watdbnewfr(pb) watdinit(bufnewfr(pb))
#define watdbnewbk(pb) watdinit(bufnewbk(pb))

/* wat function */
typedef struct watf {
  sym_t mod;
  sym_t id;
  bool exported;
  bool start;
  funcsig_t fs;
  buf_t code; /* of inscode_t; register pseudo-instrs for args&locals */
} watf_t;

extern watf_t* watfinit(watf_t* ps);
extern void watffini(watf_t* ps);
typedef buf_t watfbuf_t; 
#define watfbinit(mem) bufinit(mem, sizeof(watf_t))
extern void watfbfini(watfbuf_t* pb);
#define watfblen(pb) buflen(pb)
#define watfbref(pb, i) ((watf_t*)bufref(pb, i))
#define watfbnewfr(pb) watfinit(bufnewfr(pb))
#define watfbnewbk(pb) watfinit(bufnewbk(pb))

typedef enum main {
  MAIN_ABSENT = 0, 
  MAIN_VOID,
  MAIN_ARGC_ARGV
} main_t;

/* wat (text) module for object files */
typedef struct wat_module {
  sym_t name; 
  watibuf_t imports;
  watibuf_t defs;
  watdbuf_t dsegs;
  watfbuf_t funcs;
  main_t main;
} wat_module_t;

extern wat_module_t* wat_module_init(wat_module_t* pm);
extern void wat_module_clear(wat_module_t* pm);
extern void wat_module_fini(wat_module_t* pm);

typedef buf_t wat_module_buf_t; 
#define wat_module_buf_init(mem) bufinit(mem, sizeof(wat_module_t))
extern void wat_module_buf_fini(wat_module_buf_t* pb);
#define wat_module_buf_len(pb) buflen(pb)
#define wat_module_buf_ref(pb, i) ((wat_module_t*)bufref(pb, i))
#define wat_module_buf_newbk(pb) wat_module_init(bufnewbk(pb))

/* read/write 'object' wat text module */
extern void read_wat_module(const char *fname, wat_module_t* pm);
extern void load_library_wat_module(sym_t mod, wat_module_t* pm);
extern void write_wat_module(wat_module_t* pm, FILE *pf);

/* linker */
extern void link_wat_modules(wat_module_buf_t *pwb, wat_module_t* pm); 

#endif /* ndef _W_H_INCLUDED */
