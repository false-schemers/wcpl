/* c.h (wcpl compiler) -- esl */

#ifndef _C_H_INCLUDED
#define _C_H_INCLUDED

/* globals */
extern buf_t *g_pbases;   /* module search bases */
extern sym_t g_env_mod;   /* environment module */
extern sym_t g_wasi_mod;  /* module for wasi */
extern sym_t g_lm_id;     /* id for linear memory */
extern sym_t g_sp_id;     /* id for stack pointer global */
extern size_t g_sdbaddr;  /* static data allocation start */
extern size_t g_stacksz;  /* stack size in bytes */

/* g_dsmap element */
typedef struct dsme {
  chbuf_t data; /* data segment data */
  int align;    /* alignment in bytes: 1,2,4,8,16 */
  size_t ind;   /* unique index within this map */
} dsme_t;

extern dsme_t* dsmeinit(dsme_t* pe);
extern void dsmefini(dsme_t* pe);
typedef buf_t dsmebuf_t; 
#define dsmebinit(mem) bufinit(mem, sizeof(dsme_t))
extern void dsmebfini(dsmebuf_t* pb);
#define dsmeblen(pb) buflen(pb)
#define dsmebref(pb, i) ((dsme_t*)bufref(pb, i))
#define dsmebnewbk(pb) dsmeinit(bufnewbk(pb))
extern int dsme_cmp(const void *p1, const void *p2);


/* calc size/align for ptn; prn is NULL or reference node for errors, use 0 for lvl */
extern void measure_type(node_t *ptn, node_t *prn, size_t *psize, size_t *palign, int lvl);
/* calc offset for ptn.fld; prn is NULL or reference node for errors */
extern size_t measure_offset(node_t *ptn, node_t *prn, sym_t fld, node_t **ppftn);
/* evaluate pen expression statically, putting result into prn */
extern bool static_eval(node_t *pen, node_t *prn);
/* evaluate integer expression pen statically, putting result into pi */
extern bool static_eval_to_int(node_t *pen, int *pri);



#endif /* ndef _C_H_INCLUDED */
