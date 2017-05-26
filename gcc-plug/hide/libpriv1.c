#include "libpriv1.h"

int
libprivate (int a)
{
  if (a == 0)
    return 0;
  return 8 + a;
}

int __attribute__ ((noinline))
staticfoo (int a)
{
  return libprivate (a) * libprivate (a + 1) + 118;
}
