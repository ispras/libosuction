#include <dlfcn.h>
#include <stdio.h>

const char *global_name = "foo";

int main (int argc, char **argv)
{
  void *handle;
  void (* my_dyn_func)(int);

//global_name = "foo";
  handle = dlopen("./testlib.so", RTLD_NOW);
  my_dyn_func = (void (*)(int)) dlsym (handle, global_name);
  my_dyn_func (2);

//  printf ("%A", &global_name);
  return 0;
}

