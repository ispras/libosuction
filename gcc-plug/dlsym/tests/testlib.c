#include <stdio.h>
#include <stdbool.h>

void bar(int n)
{
  int i;
  for (i = 0; i < n; ++ i)
    printf("Dynamic call of 'bar' function\n");
}

void foo(int n)
{
  int i;
  for (i = 0; i < n; ++ i)
    printf("Dynamic call of 'foo' function\n");
}

void baz(int n)
{
  int i;
  for (i = 0; i < n; ++ i)
    printf("Dynamic call of 'baz' function\n");
}

bool random()
{
  return true;
}
