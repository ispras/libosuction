#include "dlfcn.h"

namespace interseption {
__attribute__((noinline)) void* GetRealFunctionAddress(const char *func_name) {
     return dlsym(RTLD_NEXT, func_name);
}

}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))interseption::GetRealFunctionAddress("_setjmp");
  my_dyn_func(2);
  }

