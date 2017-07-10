#include <dlfcn.h>

static const char *mn = "zoo";


__attribute__((noinline)) void consume (char **feed)
{
  *feed = "fake";
}

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

  handle = dlopen("./testlib.so", RTLD_NOW);
  consume (&symbols[1]);
  for (i = 0; i < 2; i++) {
      my_dyn_func = (void (*)(int)) dlsym (handle, symbols[i]);
      my_dyn_func(2);
  }

  return 0;
}

