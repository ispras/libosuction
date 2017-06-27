#include <dlfcn.h>

struct extra
{
  const char *fun_name;
};

static struct extra st1 = {"bar"};
static struct extra st2 = {"baz"};

int main (int argc, char **argv)
{
  void *handle;
  void (* my_dyn_func)(int);
  int i = 0;
  const char* str;
  handle = dlopen("./testlib.so", RTLD_NOW);
  if (argc)
    str = st1.fun_name;
  else
    str = st2.fun_name;
  my_dyn_func = (void (*)(int)) dlsym (handle, str);
  my_dyn_func(2);

  return 0;
}

