#include <dlfcn.h>
char * const func_name = "foo";

__attribute__((noinline)) void *function_caller(char *lib_name)
{
  void * library_handle = dlopen(lib_name, RTLD_NOW); 
  return dlsym(library_handle, func_name);
}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))function_caller("./testlib.so");
  my_dyn_func(2);
}

