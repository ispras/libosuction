PLUGIN = libplug.so

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O2 -shared -fPIC tests/testlib.c -o tests/testlib.so; \
	cd "tests" || exit 1; \
	CC=$(CC) CXX=$(CXX) PLUGIN="../$(PLUGIN)" ./run_tests.sh || exit 1;

clean:
	-rm -f $(PLUGIN) plug.o symbols-pass.o a.out
	-rm -f tests/*.out tests/*.so
