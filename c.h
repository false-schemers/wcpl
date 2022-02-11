/* c.h (wcpl compiler) -- esl */

#ifndef _C_H_INCLUDED
#define _C_H_INCLUDED

/* compiler */

/* compiler initialization (including symbols and workstaces) */
extern void init_compiler(dsbuf_t *plibv);
extern void fini_compiler(void);

/* compile module file to .wo output file (if not NULL) */
extern void compile_module(dsbuf_t *plibv, const char *ifname, const char *ofname);

/* calc size/align for ptn; prn is NULL or reference node for errors, use 0 for lvl */
extern void measure_type(node_t *ptn, node_t *prn, size_t *psize, size_t *palign, int lvl);
/* calc offset for ptn.fld; prn is NULL or reference node for errors */
extern size_t measure_offset(node_t *ptn, node_t *prn, sym_t fld, node_t **ppftn);
/* evaluate pen expression statically, putting result into prn */
extern bool static_eval(node_t *pen, node_t *prn);
/* evaluate integer expression pen statically, putting result into pi */
extern bool static_eval_to_int(node_t *pen, int *pri);



#endif /* ndef _C_H_INCLUDED */
