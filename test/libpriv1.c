#include "libpriv1.h"

int libprivatevar = 1;
int staticvar = 0;
int staticvarcommon;
int extvar = -1;

int
libprivate (int a)
{
  if (a == 0)
    return 0;
  staticvar++;
  staticvarcommon++;
  libprivatevar++;
  return 8 + a;
}

int __attribute__ ((noinline))
staticfoo (int a)
{
  staticvar = staticvarcommon = 34 + libprivatevar;
  return libprivate (a) * libprivate2 (a + 1);
}

int external()
{
  volatile int v = 1;
  return staticfoo (v);
}
