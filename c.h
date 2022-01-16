/* c.h (wcpl compiler) -- esl */

#ifndef _C_H_INCLUDED
#define _C_H_INCLUDED

/* compiler */

/* compiler initialization (including symbols and workstaces) */
extern void init_compiler(const char *larg, const char *lenv);
extern void fini_compiler(void);

/* compile module file */
extern void compile_module(const char *fname);

/* calc size/align for ptn; prn is NULL or reference node for errors, use 0 for lvl */
extern void measure_type(node_t *ptn, node_t *prn, size_t *psize, size_t *palign, int lvl);
/* calc offset for ptn.fld; prn is NULL or reference node for errors */
extern size_t measure_offset(node_t *ptn, node_t *prn, sym_t fld);
/* evaluate pen expression statically, putting result into prn (VERY limited) */
extern bool static_eval(node_t *pen, node_t *prn);
/* evaluate integer expression pen statically, putting result into pi (VERY limited) */
extern bool static_eval_to_int(node_t *pen, int *pri);



#endif /* ndef _C_H_INCLUDED */
