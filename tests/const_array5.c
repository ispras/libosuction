#include <dlfcn.h>

static const char *mn = "zoo";

int main (int argc, char **argv)
{
  void *handle;
  void (* my_dyn_func)(int);
  int i = 0;
  char ** fake;

  char * symbols[] = {
      "foo",
      "bar",
  };

  fake = &symbols[1];
  handle = dlopen("./testlib.so", RTLD_NOW);
  *fake = "baz";
//  printf("%A", &symbols[2]);
  for (i = 0; i < 2; i++) {
      my_dyn_func = (void (*)(int)) dlsym (handle, symbols[i]);
      my_dyn_func(2);
  }

  return 0;
}

