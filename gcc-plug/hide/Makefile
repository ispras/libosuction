CXXFLAGS += -fpic -fno-exceptions -fno-rtti
CPPFLAGS += -I$(shell $(CC) -print-file-name=plugin)/include

PLUGNAME = libmkpriv
PLUGIN = $(PLUGNAME).so

$(PLUGIN): plug.o
	$(CXX) -shared -o $@ $^

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O1 -fplugin=./$(PLUGIN) test.c

vis.txt: FORCE
	echo "1 1" > vis.txt
	echo "9 staticfoo" >> vis.txt
	echo "10 libprivate" >> vis.txt

test-libprivate.so: $(PLUGIN) libpriv1.c libpriv2.c libpriv1.h vis.txt
	$(CC) -fplugin=./$(PLUGIN) -fplugin-arg-$(PLUGNAME)-fname=vis.txt -c -fpic -O2 -flto libpriv1.c -o libpriv1.o
	$(CC) -fplugin=./$(PLUGIN) -fplugin-arg-$(PLUGNAME)-fname=vis.txt -c -fpic -O2 -flto libpriv2.c -o libpriv2.o
	$(CC) -shared libpriv1.o libpriv2.o -o test-libprivate.so

clean:
	-rm -f $(PLUGIN) plug.o a.out

FORCE:
