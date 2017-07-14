CFLAGS += -std=gnu11
CFLAGS += -fpic
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wall -Wno-parentheses
CFLAGS += -g

CFLAGS += -D_FILE_OFFSET_BITS=64

TEST-OBJ =  test1a.o test1b.o

PLUGIN-DEPS = libplug.so
PLUGIN-PRIV = libplug-priv.so
MERGE = merge
DAEMON = daemon
WRAPPER = wrapper
GCCWRAP1 = gcc-wrapper-1
GCCWRAP2 = gcc-wrapper-2

all: $(DAEMON) $(WRAPPER) $(MERGE) $(PLUGIN-DEPS) $(PLUGIN-PRIV) $(GCCWRAP1) $(GCCWRAP2)

test: libtest1.so

libtest1.so: $(TEST-OBJ) $(PLUGIN-DEPS)
	$(CC) -shared -o $@ $(TEST-OBJ) -Wl,--plugin,./$(PLUGIN-DEPS),--plugin-opt,1:_,--gc-sections,--version-script,vers-test1

$(PLUGIN-DEPS): plug.o
	$(CC) -shared -o $@ $^ -Wl,--version-script,vers

$(PLUGIN-PRIV): plug-priv.o
	$(CC) -shared -o $@ $^ -Wl,--version-script,vers

daemon: CFLAGS += -fopenmp

$(GCCWRAP1): gcc-wrapper.c
	$(CC) $(CFLAGS) -o $@ $^ -DGCC_RUN=1

$(GCCWRAP2): gcc-wrapper.c
	$(CC) $(CFLAGS) -o $@ $^ -DGCC_RUN=2
