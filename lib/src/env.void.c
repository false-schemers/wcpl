#pragma module "env"
#include <wasi/api.h>
#include <sys/crt.h>

char *_argv[] = { "", NULL };
int _argc = 1;

void initialize(void)
{
}

onterm_func_t onterm_funcs[ONTERM_MAX];
size_t onterm_count = 0;

void terminate(int status)
{
  while (onterm_count > 0) {
    onterm_func_t fn = onterm_funcs[--onterm_count];
    (*fn)();
  }
  proc_exit((exitcode_t)status);
}
