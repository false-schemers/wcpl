#include <fenv.h>

/* NB: WASM does not support one, so this is a fake */

void feclearexcept(int mask)
{
}

void fegetexceptflag(fexcept_t* flagp, int excepts)
{
}

void feraiseexcept(int excepts)
{
}

void fesetexceptflag(const fexcept_t* flagp, int excepts)
{
}

int fetestexcept(int excepts)
{
	return 0;
}

int fegetround(void)
{
	return FE_TONEAREST;
}

int fesetround(int round)
{
	return 0;
}

void fegetenv(fenv_t *envp)
{
}

int feholdexcept(fenv_t* envp)
{
	return 0;
}

void fesetenv(const fenv_t *envp)
{
}

void feupdateenv(const fenv_t* envp)
{
}
