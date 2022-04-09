/* c.h (wcpl compiler) -- esl */

#pragma once

/* globals */
extern long    g_optlvl;   /* -O arg */
extern buf_t  *g_ibases;   /* module include search bases */
extern buf_t  *g_lbases;   /* module object module search bases */
extern fsbuf_t g_funcsigs; /* unique function signatures */
extern sym_t   g_crt_mod;  /* C runtime module */
extern sym_t   g_wasi_mod; /* WASI module */
extern sym_t   g_lm_id;    /* id for linear memory */
extern sym_t   g_sp_id;    /* id for stack pointer global */
extern sym_t   g_sb_id;    /* id for stack base global */
extern sym_t   g_hb_id;    /* id for heap base global */
extern size_t  g_sdbaddr;  /* static data allocation start */
extern size_t  g_stacksz;  /* stack size in bytes */
extern size_t  g_argvbsz;  /* argv buf size in bytes */

/* convert function type to a function signature */
extern funcsig_t *ftn2fsig(node_t *ptn, funcsig_t *pfs);
/* calc size/align for ptn; prn is NULL or reference node for errors, use 0 for lvl */
extern void measure_type(const node_t *ptn, node_t *prn, size_t *psize, size_t *palign, int lvl);
/* calc offset for ptn.fld; prn is NULL or reference node for errors */
extern size_t measure_offset(const node_t *ptn, node_t *prn, sym_t fld, node_t **ppftn);
/* evaluate pen arithmetic expression statically, putting result into prn */
extern bool arithmetic_eval(node_t *pen, buf_t *prib, node_t *prn);
/* evaluate integer expression pen statically, putting result into pi */
extern bool arithmetic_eval_to_int(node_t *pen, buf_t *prib, int *pri);
