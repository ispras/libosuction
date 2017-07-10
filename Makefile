GCCDIR = /home/eugene/Workspace/install/gcc-trunk/bin
CXX=$(GCCDIR)/x86_64-pc-linux-gnu-g++
CC=$(GCCDIR)/x86_64-pc-linux-gnu-gcc
CXXFLAGS += -O0 -ggdb3 -fno-exceptions -fno-rtti -std=c++11 -fpic -Wall 
CPPFLAGS += -I$(shell $(CXX) -print-file-name=plugin)/include

PLUGIN = libplug.so

$(PLUGIN): plug.o
	$(CXX) -shared -o $@ $^

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O2 -shared -fPIC tests/testlib.c -o tests/testlib.so; \
	cd "tests" || exit 1; \
	CC=$(CC) CXX=$(CXX) PLUGIN="../$(PLUGIN)" ./run_tests.sh || exit 1;

clean:
	-rm -f $(PLUGIN) plug.o a.out
	-rm -f tests/*.out tests/*.so 
