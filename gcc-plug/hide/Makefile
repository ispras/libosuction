CXXFLAGS += -fpic -fno-exceptions -fno-rtti -g3 -O0 -fno-inline-functions
CPPFLAGS += -I$(shell $(CC) -print-file-name=plugin)/include

PLUGNAME = libmkpriv
PLUGIN = $(PLUGNAME).so

plug = -fplugin=./$(PLUGIN)
hash = $(plug) -fplugin-arg-$(PLUGNAME)-run=1
vis  = $(plug) -fplugin-arg-$(PLUGNAME)-run=2 -fplugin-arg-$(PLUGNAME)-fname

all: $(PLUGIN)

$(PLUGIN): plug.o
	$(CXX) -shared -o $@ $^

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O1 -fplugin=./$(PLUGIN) test.c

check: test-libpriv.so test-asmname.so
	for checker in check-bin*.sh ; do \
	  ./"$$checker" ; \
	  if [ $$? -ne 0 ] ; then \
	    echo "$$checker" ; \
	    exit 1 ; \
	  fi ; \
	done;

test-libpriv.so: $(PLUGIN) libpriv1.c libpriv2.c libpriv1.h libpriv.vis.def
	$(CC) $(hash) -c -fpic -O2 -flto libpriv1.c -o libpriv1.o
	$(CC) $(hash) -c -fpic -O2 -flto libpriv2.c -o libpriv2.o
	./pastehash.sh libpriv1 libpriv2
	$(CC) $(vis)=libpriv.vis -c -fpic -O2 -flto libpriv1.c -o libpriv1.o
	$(CC) $(vis)=libpriv.vis -c -fpic -O2 -flto libpriv2.c -o libpriv2.o
	$(CC) -shared libpriv1.o libpriv2.o -o $@

test-asmname.so: $(PLUGIN) asmname.c asmname.vis.def
	$(CC) $(hash) -c -fpic -O2 asmname.c -o asmname.o
	./pastehash.sh asmname
	$(CC) $(vis)=asmname.vis -c -fpic -O2 asmname.c -o asmname.o
	$(CC) -shared asmname.o -o $@

clean:
	-rm -f $(PLUGIN) plug.o a.out
