#include <dlfcn.h>

__attribute__((noinline)) void *function_caller(int rnd, char *lib_name, char *func_name)
{
  void * library_handle = dlopen(lib_name, RTLD_NOW); 
  char* temp_var = func_name;
  if (rnd)
    func_name = "zoo";
  dlsym(library_handle, func_name);
  return dlsym(library_handle, temp_var);
}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))function_caller(0, "./testlib.so", "bar");
  my_dyn_func(2);
  my_dyn_func = (void (*)(int))function_caller(0, "./testlib.so", "baz");
  my_dyn_func(2);
}

