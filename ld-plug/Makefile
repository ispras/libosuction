TEST-OBJ =  test1a.o test1b.o
PLUGIN-DEPS = libplug.so

test: libtest1.so

libtest1.so: $(TEST-OBJ) $(PLUGIN-DEPS)
	$(CC) -shared -o $@ $(TEST-OBJ) -Wl,--plugin,./$(PLUGIN-DEPS),--plugin-opt,0000:_:1,--gc-sections,--version-script,vers-test1
