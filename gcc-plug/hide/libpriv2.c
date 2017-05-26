#include "libpriv1.h"

int bar (int a)
{
  return libprivate (a - 1) + a;
}
