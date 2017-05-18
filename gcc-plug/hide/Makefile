CXXFLAGS += -fpic -fno-exceptions -fno-rtti
CPPFLAGS += -I$(shell $(CC) -print-file-name=plugin)/include

PLUGIN = libplug.so

$(PLUGIN): plug.o
	$(CXX) -shared -o $@ $^

.PHONY: clean test

test: $(PLUGIN)
	$(CC) -O1 -fplugin=./$(PLUGIN) test.c

clean:
	-rm -f $(PLUGIN) plug.o a.out
