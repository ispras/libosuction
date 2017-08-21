PLUGNAME = libmkpriv
PLUGIN = $(PLUGNAME).so

plug = -fplugin=./$(PLUGIN)
hash = $(plug) -fplugin-arg-$(PLUGNAME)-run=1
vis  = $(plug) -fplugin-arg-$(PLUGNAME)-run=2 -fplugin-arg-$(PLUGNAME)-fname

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O1 -fplugin=./$(PLUGIN) test.c

check: test-asmname.so
	for checker in check-bin*.sh ; do \
	  ./"$$checker" ; \
	  if [ $$? -ne 0 ] ; then \
	    echo "$$checker" ; \
	    exit 1 ; \
	  fi ; \
	done;

test-asmname.so: $(PLUGIN) asmname.c asmname.vis.def
	$(CC) $(hash) -c -fpic -O2 asmname.c -o asmname.o
	./pastehash.sh asmname
	$(CC) $(vis)=asmname.vis -c -fpic -O2 asmname.c -o asmname.o
	$(CC) -shared asmname.o -o $@

clean:
	-rm -f $(PLUGIN) plug.o a.out
