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

static __attribute__((noinline)) void* function_caller2(int num)
{
  return (void (*)(int))function_caller("./testlib.so", 2 + 7 * num + 109 % num, "baz");
}
int main()
{
  void (* my_dyn_func)(int) = function_caller2(5);
  my_dyn_func(2);
}


