#include <dlfcn.h>
#include <string.h>

int main(int argc, char **argv)
{
  void * library_handle;
  void (* my_dyn_func)(int);
  char func_name[80] = "_init";
  char *z = func_name;
  if (argc)
    z = func_name + 1;
  library_handle = dlopen("./testlib.so", RTLD_NOW); 
  my_dyn_func = (void (*)(int))dlsym(library_handle, z);
  my_dyn_func(2);
}
