#include <dlfcn.h>


void *library_handle;
void (* my_dyn_func)(int);


__attribute__((noinline)) void function_caller(char *lib_name)
{
  library_handle = dlopen(lib_name, RTLD_NOW); 
  my_dyn_func = (void (*)(int))dlsym(library_handle, "baz");
  my_dyn_func(1);
}

int main()
{
  function_caller("./testlib.so");
}

