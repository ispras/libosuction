#include <dlfcn.h>

struct extra
{
  const char *fun_name;
};

static const struct
{
  const struct extra fun[2];
} names[] =
{
    {{{"bar"}}},
    {{{"foo"}}}
};



int main ()
{
  void *handle;
  void (* my_dyn_func)(int);
  int i = 0;

  handle = dlopen("./testlib.so", RTLD_NOW);

  for (i = 0; i < 2; i++) {
      my_dyn_func = (void (*)(int)) dlsym (handle, names[i].fun[0].fun_name);
      my_dyn_func(2);
  }   

  return 0;
}

