/* runtime definitions */

#pragma once
#pragma module "crt"

#define ONTERM_MAX 32
typedef void (*onterm_func_t)(void);
extern onterm_func_t onterm_funcs[ONTERM_MAX];
extern size_t onterm_count;
extern void terminate(int status);

