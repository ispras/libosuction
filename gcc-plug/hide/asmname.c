#include <stdio.h>

__attribute__((noinline)) int staticfoo () __asm__("staticfoo3");

int staticfoo ()
{
  return 42;
}

int caller ()
{
  return staticfoo ();
}

int main()
{
  printf ("%d\n", caller());
}
