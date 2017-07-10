#include <dlfcn.h>
#include <stdio.h>

static __attribute__((noinline)) void* function_caller(char *lib_name, int num, char *func_name)
    
{
  int i;
  void * library_handle = dlopen(lib_name, RTLD_NOW); 
  for (i = 0; i < num; ++ i)
    printf ("inc");
  return dlsym(library_handle, func_name);
}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))function_caller("./testlib.so", 2, "bar");
  my_dyn_func(2);
  my_dyn_func = (void (*)(int))function_caller("./testlib.so", 2, "baz");
  my_dyn_func(2);
}

