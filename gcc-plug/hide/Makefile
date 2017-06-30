CXXFLAGS += -fpic -fno-exceptions -fno-rtti -g3 -O0 -fno-inline-functions
CPPFLAGS += -I$(shell $(CC) -print-file-name=plugin)/include

PLUGNAME = libmkpriv
PLUGIN = $(PLUGNAME).so

all: $(PLUGIN)

$(PLUGIN): plug.o
	$(CXX) -shared -o $@ $^

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O1 -fplugin=./$(PLUGIN) test.c

check: test-libpriv.so test-asmname.so
	$(shell ./check-bin.sh)
	$(shell ./check-bin-asmname.sh)

test-libpriv.so: $(PLUGIN) libpriv1.c libpriv2.c libpriv1.h vis.txt
	$(CC) -fplugin=./$(PLUGIN) -fplugin-arg-$(PLUGNAME)-fname=vis.txt -c -fpic -O2 -flto libpriv1.c -o libpriv1.o
	$(CC) -fplugin=./$(PLUGIN) -fplugin-arg-$(PLUGNAME)-fname=vis.txt -c -fpic -O2 -flto libpriv2.c -o libpriv2.o
	$(CC) -shared libpriv1.o libpriv2.o -o $@

test-asmname.so: $(PLUGIN) asmname.vis asmname.c
	$(CC) -fplugin=./$(PLUGIN) -fplugin-arg-$(PLUGNAME)-fname=asmname.vis -c -fpic -O2 asmname.c -o asmname.o
	$(CC) -shared asmname.o -o $@

clean:
	-rm -f $(PLUGIN) plug.o a.out
