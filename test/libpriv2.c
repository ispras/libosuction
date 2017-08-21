#include "libpriv1.h"

int __attribute__ ((noinline))
libprivate2 (int a)
{
  return libprivatevar + libprivate (a - 1) + a;
}

int unusedfoo (int a)
{
  return libprivatevar - libprivate (a - 1) + a;
}
