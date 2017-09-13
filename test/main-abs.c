#include <stdio.h>
extern void *_nl_current_used;
int main()
{
  printf ("%p\n", _nl_current_used);
  return 0;
}
