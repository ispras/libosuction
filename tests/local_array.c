#include <dlfcn.h>


int main()
{
  char* func_names[] = {"foo", "bar", "baz"};
  void * library_handle;
  void (* my_dyn_func)(int);
  library_handle = dlopen("./testlib.so", RTLD_NOW); 
  my_dyn_func = (void (*)(int))dlsym(library_handle, func_names[1]);
  my_dyn_func(2);
}
