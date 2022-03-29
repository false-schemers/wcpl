#pragma module "env"
#include <wasi/api.h>

char *_argv[] = { "", NULL };
int _argc = 1;

void initialize(void)
{
}

void terminate(int status)
{
  proc_exit((exitcode_t)status);
}
