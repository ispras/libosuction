TEST-OBJ =  test1a.o test1b.o
PLUGIN-DEPS = libplug.so

test: libtest1.so

libtest1.so: $(TEST-OBJ) $(PLUGIN-DEPS)
	$(CC) -shared -o $@ $(TEST-OBJ) -Wl,--plugin,./$(PLUGIN-DEPS),--plugin-opt,1:_,--gc-sections,--version-script,vers-test1
