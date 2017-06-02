#include "libpriv1.h"

int libprivatevar = 1;
int staticvar;

int
libprivate (int a)
{
  if (a == 0)
    return 0;
  staticvar++;
  /* With this assignment the symbol libprivatevar ends up having symbol type
     "d" in nm output.  Without it the variable is removed completely.  Without
     the plugin it has "D" type.  */
  libprivatevar++;
  return 8 + a;
}

int __attribute__ ((noinline))
staticfoo (int a)
{
  staticvar = 34 + libprivatevar;
  return libprivate (a) * libprivate (a + 1) + 118;
}
