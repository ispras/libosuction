#include "dlfcn.h"

namespace interseption {
    __attribute__((noinline)) void* GetRealFunctionAddress(const char *func_name) {
	return dlsym(RTLD_NEXT, func_name);
    }

    __attribute__((noinline)) void* GetRealFunctionAddress(const char *func_name,
							   const char *aux) {
	return GetRealFunctionAddress(func_name);
    }

    __attribute__((noinline)) void* GetRealFunctionAddress(const char *func_name,
							   double smth) {
	return GetRealFunctionAddress(func_name, "aux");
    }
}

int main()
{
  void (* my_dyn_func)(int) = (void (*)(int))interseption::GetRealFunctionAddress("foo", 3.14);
  my_dyn_func(2);
}

