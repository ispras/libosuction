#include <dlfcn.h>

static const char *mn = "zoo";

int main (int argc, char **argv)
{
  void *handle;
  void (* my_dyn_func)(int);
  int i = 0;

  const char * symbols[2] = {
      "foo",
  };  

  symbols[1] = mn;
  handle = dlopen("./testlib.so", RTLD_NOW);

  for (i = 0; i < 2; i++) {
      my_dyn_func = (void (*)(int)) dlsym (handle, symbols[i]);
      my_dyn_func(2);
  }   

  return 0;
}

