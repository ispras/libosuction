#include <dlfcn.h>

int main ()
{
  void *handle;
  void (* my_dyn_func)(int);
  int i = 0;

  const char * symbols[] = { 
      "foo",
      "bar",
      "baz",
  };  

  handle = dlopen("./testlib.so", RTLD_NOW);

  for (i = 0; i < 3; i++) {
      my_dyn_func = (void (*)(int)) dlsym (handle, symbols[i]);
      my_dyn_func(2);
  }   

  return 0;
}

