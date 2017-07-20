#include <dlfcn.h>

#define MACRO_WRAPPER(fn) dlsym(lib, #fn);


int main()
{
  void * lib;
  void (* my_dyn_func)(int);
  lib = dlopen ("./testlib.so", RTLD_NOW); 
  my_dyn_func = (void (*)(int)) MACRO_WRAPPER (bar);
  my_dyn_func(2);
  my_dyn_func = (void (*)(int)) MACRO_WRAPPER (foo);
  my_dyn_func(2);
}
