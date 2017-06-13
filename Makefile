CFLAGS += -fpic
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wall -Wno-parentheses
CFLAGS += -g

TEST-OBJ =  test1a.o test1b.o

PLUGIN-DEPS = libplug.so
PLUGIN-PRIV = libplug-priv.so
MERGE = merge

all: $(MERGE) $(PLUGIN-DEPS) $(PLUGIN-PRIV)

test: libtest1.so

libtest1.so: $(TEST-OBJ) $(PLUGIN-DEPS)
	$(CC) -shared -o $@ $(TEST-OBJ) -Wl,--plugin,./$(PLUGIN-DEPS),--gc-sections,--version-script,vers-test1

$(PLUGIN-DEPS): plug.o
	$(CC) -shared -o $@ $^ -Wl,--version-script,vers

$(PLUGIN-PRIV): plug-priv.o
	$(CC) -shared -o $@ $^ -Wl,--version-script,vers
