#include <dlfcn.h>

typedef void (*_getproc_fn) (void);
typedef _getproc_fn (*fp_getproc)(const char *);

__attribute__((noinline)) static void function_caller(void *getproc)
{
  fp_getproc gp = (fp_getproc) getproc;
  _getproc_fn temp;
  temp = gp ? gp ("func_name") : dlsym (RTLD_LOCAL, "func_name");
  temp ();
}

void *_dlsym (const char *sym)
{
  return dlsym (RTLD_LOCAL, sym);
}

int main()
{
  function_caller(_dlsym);
}

