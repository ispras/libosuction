#!/bin/sh -

# Unused symbols should not appear in the dump.
{
    objdump -d libtpriv.so | egrep "unused(foo)"
} && die

# Uppercase letters in nm output mean global symbol visibility.
{
    nm -D libtpriv.so | egrep "(libpriv|static)" | grep -v common | grep [A-Z]
} && die

# Weak symbols should remain weak.
{
  nm main2.o | c++filt |
    grep 'MakefileGenerator::foo() const::{lambda()#1}::operator()() const::data' |
      grep " [^wWvVu] "
} && die

# The executalbe should run and produce the expected return value.
LD_LIBRARY_PATH=. ./exe1
[ "$?" != 125 ] && die

./exe3-1
[ "$?" != 113 ] && die

./exe3-2
[ "$?" != 113 ] && die

true
