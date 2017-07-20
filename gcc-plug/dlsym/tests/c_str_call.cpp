#include <iostream>
#include <cstdlib>
#include <dlfcn.h>

using std::cerr;
using std::endl;
using std::string;

void *LoadFuncOrDie(void *lib, const string& func_name) {
  const auto func = dlsym(lib, func_name.c_str());
  const auto dlsym_error = dlerror();
  return func;
}

void *LoadLibOrDie(const string& path) {
  const auto lib = dlopen(path.c_str(), RTLD_LAZY);
  return lib;
}

int main(int argc, char **argv) {
  const auto catlib = LoadLibOrDie("./cat.so");
  void *cat_c = LoadFuncOrDie(catlib, "Create");
  dlclose(catlib);
}
