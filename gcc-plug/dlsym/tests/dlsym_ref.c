#include <dlfcn.h>

void * xyz (void *, const char*) asm("dlsym");

struct {
  void* (*ptr)(void*, const char*);
  void* (*ptr1)(void*, const char*);
} singleton;

int main()
{
  singleton.ptr = xyz;
  singleton.ptr1 = dlsym;
}
