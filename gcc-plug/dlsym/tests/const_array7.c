#include <dlfcn.h>

struct container
{
  char *sym;
  char *sym1;
};


int main (int argc, char **argv)
{
  void *handle;
  void (* my_dyn_func)(int);
  int i = 0;
  char **fake;
  struct container c1 = {"foo", "bar"};
  struct container c2 = {"foo1", "bar1"};

  struct container *cont[] ={ &c1, &c2};
  //  fake = &cont[argc].sym1;
  handle = dlopen("./testlib.so", RTLD_NOW);
  //  *fake = "baz";
  //  printf("%A", &cont[argc]->sym1);
  my_dyn_func = (void (*)(int)) dlsym (handle, cont[argc]->sym1);
  my_dyn_func(2);

  return 0;
}

