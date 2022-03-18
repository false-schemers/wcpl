/* Floating-point environment */

#pragma once

/* NB: WASM does not support one, so this is a fake */

#define FE_ALL_EXCEPT 0
#define FE_TONEAREST  0

typedef unsigned long fexcept_t;

typedef struct {
	unsigned long __cw;
} fenv_t;

#define FE_DFL_ENV ((const fenv_t *)-1)

extern void feclearexcept(int excepts);
extern void fegetexceptflag(fexcept_t* flagp, int excepts);
extern void feraiseexcept(int excepts);
extern void fesetexceptflag(const fexcept_t* flagp, int excepts);
extern int  fetestexcept(int excepts);
extern int  fegetround(void);
extern int  fesetround(int round);
extern void fegetenv(fenv_t* envp);
extern int  feholdexcept(fenv_t* envp);
extern void fesetenv(const fenv_t* envp);
extern void feupdateenv(const fenv_t* envp);

