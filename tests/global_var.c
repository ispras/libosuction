#include <dlfcn.h>
#include <stdio.h>

struct container
{
  char *sym;
  char *sym1;
};

struct container cont = { "foo", "bar" };

int main (int argc, char **argv)
{
  void *handle;
  void (* my_dyn_func)(int);
  int i = 0;
  char **fake;

  fake = &cont.sym1;
  handle = dlopen("./testlib.so", RTLD_NOW);
//  *fake = "baz";
//  printf("%A", &cont.sym);
  my_dyn_func = (void (*)(int)) dlsym (handle, cont.sym1);
  my_dyn_func(2);

  return 0;
}

