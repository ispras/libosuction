#include <dlfcn.h>

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
  library_handle = dlopen("./testlib.so", RTLD_NOW); 
  my_dyn_func = (void (*)(int))dlsym(library_handle, func_name);
  my_dyn_func(2);
}
