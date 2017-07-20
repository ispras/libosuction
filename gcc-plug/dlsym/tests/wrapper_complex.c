#include <dlfcn.h>
#include <string.h>

__attribute__((noinline)) void *function_caller(char *lib_name, char *func_name)
{
  char str[10];
  strcat (str, "b");
  strcat (str, func_name);
  void * library_handle = dlopen(lib_name, RTLD_NOW); 
  return dlsym(library_handle, str);
}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))function_caller("./testlib.so", "ar");
  my_dyn_func(2);
//  my_dyn_func = (void (*)(int))function_caller("./testlib.so", "az");
//  my_dyn_func(2);
}

