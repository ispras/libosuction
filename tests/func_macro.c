#include <dlfcn.h>

__attribute__((noinline)) void *foo (char *lib_name)
{
  void * library_handle = dlopen (lib_name, RTLD_NOW); 
  return dlsym (library_handle, __FUNCTION__);
}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int)) foo ("./testlib.so");
  my_dyn_func(2);
}

