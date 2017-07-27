PLUGIN = libplug.so

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O2 -shared -fPIC tests/testlib.c -o tests/testlib.so; \
	cd "tests" || exit 1; \
	CC=$(CC) CXX=$(CXX) PLUGIN="../$(PLUGIN)" ./symbols-test.sh || exit 1; \
	CC=$(CC) CXX=$(CXX) PLUGIN="../$(PLUGIN)" ./jfunc-test.sh || exit 1;

clean:
	-rm -f $(PLUGIN) *.o a.out
	-rm -f tests/*.out tests/*.so
