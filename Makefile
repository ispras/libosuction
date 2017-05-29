CFLAGS += -fpic
CFLAGS += -ffunction-sections
CFLAGS += -Wall -Wno-parentheses
CFLAGS += -g

TEST-OBJ =  test1a.o test1b.o

PLUGIN = libplug.so
MERGE = merge

all: $(MERGE)

libtest1.so: $(TEST-OBJ) $(PLUGIN)
	$(CC) -shared -o $@ $(TEST-OBJ) -Wl,--plugin,./$(PLUGIN),--gc-sections,--version-script,vers-test1

$(PLUGIN): plug.o
	$(CC) -shared -o $@ $^ -Wl,--version-script,vers
