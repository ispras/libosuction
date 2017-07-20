#include <dlfcn.h>

const char *global_name = "foo";

int change_it (int rnd)
{
  if (rnd)
    global_name = "goo";
}

int main (int argc, char **argv)
{
  void *handle = dlopen ("./testlib.so", RTLD_NOW);
  void (* my_dyn_func)(int) = (void (*)(int)) dlsym (handle, global_name);
  my_dyn_func (2);
  return 0;
}

