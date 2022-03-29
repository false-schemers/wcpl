#pragma module "env"

#include <wasi/api.h>

void *__stack_base;
void *__heap_base;

char **_argv = NULL;
int _argc = 0;
static char *empty_argv[] = { "", NULL };

void initialize(void) 
{
  if (_argv != NULL) {
    return;
  } else {
    size_t args_count, args_buf_size;
    errno_t error = args_sizes_get(&args_count, &args_buf_size);
    if (error != ERRNO_SUCCESS) goto err;
    if (args_count == 0) goto err; 
    size_t num_ptrs = args_count + 1;
    if (num_ptrs == 0) goto err; 
    char *args_buf = __stack_base;
    char *ptrs_buf = args_buf + args_buf_size;
    size_t n = (size_t)ptrs_buf % 16;
    if (n > 0) ptrs_buf += 16-n; 
    char *ptrs_end = ptrs_buf + num_ptrs*sizeof(char*);
    if (ptrs_end > __heap_base) goto err;
    char **args_ptrs = (char **)ptrs_buf;
    error = args_get((uint8_t **)args_ptrs, (uint8_t *)args_buf);
    if (error != ERRNO_SUCCESS) goto err;
    args_ptrs[args_count] = NULL;
    _argv = args_ptrs;
    _argc = (int)args_count;
    return;
  err:;
  }
  ciovec_t iov; size_t ret;
  iov.buf = (uint8_t*)"internal error: unable to retrieve command-line arguments\n"; 
  iov.buf_len = 58;
  fd_write(2, &iov, 1, &ret);
  _argv = &empty_argv[0];
  _argc = 1;
}
