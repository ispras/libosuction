-include config.mak

CPPFLAGS += -D_FILE_OFFSET_BITS=64
CFLAGS += -std=gnu11
CFLAGS += -fpic
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wall -Wno-parentheses

CXXFLAGS += $(filter-out -std=%, $(CFLAGS)) -fno-rtti -fno-exceptions
CPPFLAGS += -I$(shell $(CC) -print-file-name=plugin)/include

LD_PLUG_NAMES = plug plug-priv
LD_PLUG_LIBS = $(addprefix ld-plug/, $(LD_PLUG_NAMES:%=lib%.so))

GCC_PLUG_NAMES = mkpriv plug
GCC_PLUG_LIBS = $(addprefix gcc-plug/, $(join hide/ dlsym/, $(GCC_PLUG_NAMES:%=lib%.so)))

WRAP_COMMON = $(addprefix util/, wrapper-common.h wrapper-common.c)
JF2SIGN = util/jf2sign
MERGE = util/merge
DAEMON = util/daemon
GCC_WRAP = $(addprefix util/, gcc-wrapper-0 gcc-wrapper-1 gcc-wrapper-2)
LD_WRAP = $(addprefix util/, wrapper-1 wrapper-2)

WRAPPERS = $(GCC_WRAP) $(LD_WRAP)
TOOLS = $(WRAPPERS) $(DAEMON) $(MERGE) $(JF2SIGN)
RT_AUX = util/srcid.o
ALL = $(LD_PLUG_LIBS) $(GCC_PLUG_LIBS) $(TOOLS) $(RT_AUX)

all: $(ALL)
install: all
	cp $(GCC_PLUG_LIBS) $(plugdir)
	mkdir -p $(plugdir)/ld && cp $(LD_PLUG_LIBS) $(plugdir)/ld
	ar x /usr/lib64/libg.a dummy.o && mv dummy.o $(plugdir)/ld
	cp $(RT_AUX) $(plugdir)
export
check: all install
	$(MAKE) -C gcc-plug/hide check
	$(MAKE) -C gcc-plug/dlsym test
	$(MAKE) -C ld-plug test
	test/run-all-passes.sh test/run-t.sh && test/run-c.sh
clean:
	find . -name '*.o' -o -name '*.so' | xargs -I % rm %
	rm $(TOOLS)
distclean: clean
	rm -f confdef.h config.mak


$(LD_PLUG_LIBS): ld-plug/lib%.so: ld-plug/%.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -shared -o $@ $^ -Wl,--version-script,ld-plug/vers

gcc-plug/hide/libmkpriv.so: gcc-plug/hide/plug.o
gcc-plug/dlsym/libplug.so: $(addprefix gcc-plug/dlsym/, plug.o \
							symbols-pass.o \
							jfunc-pass.o)
$(GCC_PLUG_LIBS):
	$(CXX) $(CPPFLAGS) -shared -o $@ $^

$(DAEMON): CFLAGS += -fopenmp
$(GCC_WRAP): util/gcc-wrapper-% : $(WRAP_COMMON) util/gcc-wrapper.c
$(LD_WRAP): util/wrapper-% : $(WRAP_COMMON) util/wrapper.c
$(WRAPPERS):
	$(CC) $(CPPFLAGS) $(CFLAGS) -static -DGCC_RUN=$* $(filter %.c, $^) -o $@

# TODO:
#  - deal with warnings (from new g++)
