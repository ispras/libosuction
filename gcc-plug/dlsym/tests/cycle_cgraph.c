#include <dlfcn.h>

__attribute__((noinline)) void *caller2(char *lib_name, char *func_name);
int var = 5;

__attribute__((noinline)) void *caller1(char *lib_name, char *func_name)
{
  var -= 1;
  if (var)
    return caller2(lib_name, func_name);
}
__attribute__((noinline)) void *caller2(char *lib_name, char *func_name)
{
  var -= 1;
  if (var)
    return caller1(lib_name, func_name);
  void * library_handle = dlopen(lib_name, RTLD_NOW); 
  return dlsym(library_handle, func_name);
}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))caller1("./testlib.so", "bar");
  my_dyn_func(2);
}

