#include <iostream>
#include <cstdlib>
#include <dlfcn.h>

int main(int argc, char **argv) {
  const auto lib = dlopen("./cat.so", RTLD_LAZY);
  const auto func = dlsym(lib, "Create");
  dlclose(lib);
}
