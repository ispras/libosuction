#include <dlfcn.h>

__attribute__((noinline)) char *function_caller ()
{
  return "foo";
}

int main ()
{
  char *name = function_caller ();
  void * library_handle = dlopen ("noname", RTLD_NOW);
  void (* my_dyn_func)(int) = (void (*)(int)) dlsym (library_handle, name);
  my_dyn_func(2);
}

