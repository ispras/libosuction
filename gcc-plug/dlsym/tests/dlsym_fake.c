__attribute__((noinline)) void *dlsym ()
{
  return 0;
}

int main (void)
{
  void (* my_dyn_func)(int);
  my_dyn_func = (void (*)(int))dlsym ();
  my_dyn_func (2);
  return 0;
}

