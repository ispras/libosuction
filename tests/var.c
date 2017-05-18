#include <dlfcn.h>

int main()
{
  void * library_handle;
  void (* my_dyn_func)(int);
  char * func_name = "bar";
  library_handle = dlopen("./testlib.so", RTLD_NOW); 
  my_dyn_func = (void (*)(int))dlsym(library_handle, func_name);
  my_dyn_func(2);
}
