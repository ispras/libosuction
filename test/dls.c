#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>

typedef int (*func)(int);

int main ()
{
  void *handle = dlopen("./libtdls.so", RTLD_NOW);

  char *error = NULL;
  dlerror ();
  func dls_foo = (func) dlsym (handle, "dls_foo");
  if ((error = dlerror ()) != NULL) {
    printf ("%s\n", error);
    printf ("FAIL\n");
    return 1;
  }

  int retval = dls_foo (16);

  dlclose (handle);
  if ((error = dlerror ()) != NULL) {
    printf ("%s\n", error);
    printf ("FAIL\n");
    return 1;
  }

  if (retval != 4) {
    printf ("FAIL\n");
    return 2;
  }

  printf ("OK\n");
  return 0;
}
