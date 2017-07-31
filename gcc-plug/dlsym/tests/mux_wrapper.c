extern void *mux_dlsym (const char *var1, const char *var2);

int main ()
{
  void (* my_dyn_func) (int) = (void (*) (int)) mux_dlsym ("foo", "bar");
  my_dyn_func (2);
  return 0;
}

