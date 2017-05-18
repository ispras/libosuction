GCCDIR = /home/eugene/Workspace/install/gcc-trunk/bin
CXX = $(GCCDIR)/x86_64-pc-linux-gnu-g++                
TARGETS=a.out
#CXXFLAGS=-std=c++0x -O0 -ggdb3 -ldl
CXXFLAGS=-std=c++0x -O2 -ldl -fplugin=../../libplug.so
DYLIB_SRCS=cat.cpp dog.cpp
DYLIBS=$(DYLIB_SRCS:.cpp=.so)

all: $(TARGETS) $(DYLIBS)

$(TARGETS):
	$(CXX) $(CXXFLAGS) main.cpp

.SUFFIXES: .so
.cpp.so:
	$(CXX) $(CXXFLAGS) -shared -fPIC $< -o $@


.PHONY: clean
clean:
	$(RM) *.o *.so $(TARGETS)
