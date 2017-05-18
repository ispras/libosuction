#include <dlfcn.h>

static __attribute__((noinline)) void* function_caller(char *lib_name, char *func_name)
    
{
  void * library_handle = dlopen(lib_name, RTLD_NOW); 
  return dlsym(library_handle, func_name);
}

int main(int argc, char **argv)
{
  void * library_handle;
  void (* my_dyn_func)(int);
  char * func_name;
  switch (argc)
    {
      case 0:
	func_name = "foo";
	break;
      default:
	func_name = "baz";
	break;
    }
  my_dyn_func = (void (*)(int))function_caller("./testlib.so", func_name);
  my_dyn_func(2);
}
