#include <dlfcn.h>
typedef void *(*create_t)(int);
int main()
{
  void * handle = dlopen("./testlib.so", RTLD_NOW); 
  create_t creator = reinterpret_cast<create_t>(dlsym(handle, "bar"));
  creator(2);
}
