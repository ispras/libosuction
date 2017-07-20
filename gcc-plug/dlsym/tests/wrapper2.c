#include <dlfcn.h>

__attribute__((noinline)) void *function_caller_1(char *lib_name, char *func_name)
{
  void * library_handle = dlopen(lib_name, RTLD_NOW); 
  return dlsym(library_handle, func_name);
}

__attribute__((noinline)) void *function_caller(char *func_name)
{
  return function_caller_1("./testlib.so", func_name);
}
int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))function_caller("bar");
  my_dyn_func(2);
  my_dyn_func = (void (*)(int))function_caller("baz");
  my_dyn_func(2);
}

