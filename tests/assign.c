#include <dlfcn.h>

static const struct
{
  const char* fun[2];
} names[] =
{
    {{"bar", "bar1"}}
};

int main ()
{
  void *handle;
  void (* my_dyn_func)(int);
  int i = 0;

  handle = dlopen("./testlib.so", RTLD_NOW);

  for (i = 0; i < 1; i++) {
      my_dyn_func = (void (*)(int)) dlsym (handle, names[i].fun[0]);
      my_dyn_func(2);
  }   

  return 0;
}

