#include <stdio.h>

/* static  */
int __attribute__ ((noinline)) bar(int a)
{
  if (a == 0)
    return 0;
  return bar(a - 1) + a;
}

int main()
{
  /* return bar(scanf("%d")); */
  return 0;
}
