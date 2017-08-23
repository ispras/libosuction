int foo(void)
{
	return 42;
}

__attribute__((visibility("hidden")))
int baz(void)
{
	return 1;
}
