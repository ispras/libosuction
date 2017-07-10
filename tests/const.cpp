#include <iostream>
#include <cstdlib>
#include <dlfcn.h>

int main(int argc, char **argv) {
  void *lib = dlopen("./cat.so", RTLD_LAZY);
  void *func = dlsym(lib, "Create");
  dlclose(lib);
}
