#include <dlfcn.h>

void* function_caller(char *lib_name, char *func_name)
{
  void * library_handle = dlopen(lib_name, RTLD_NOW); 
  return dlsym(library_handle, func_name);
}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))function_caller("./testlib.so", "bar");
  my_dyn_func(2);
}

