#!/bin/sh -

fail()
{
  echo FAILED
  exit 1
}

pass()
{
  exit 0
}

cd $(dirname $(realpath $0))

# Unused symbols should not appear in the dump.
{
    objdump -d libtpriv.so | egrep "unused(foo)"
} && fail

# Uppercase letters in nm output mean global symbol visibility.
{
    nm libtpriv.so | egrep "(libpriv|static)" | grep -v common | grep [A-Z]
} && fail

# Weak symbols should remain weak.
{
  nm main2.o | c++filt |
    grep 'MakefileGenerator::foo() const::{lambda()#1}::operator()() const::data' |
      grep " [^wWvVu] "
} && fail

# The executalbe should run and produce the expected return value.
LD_LIBRARY_PATH=. ./exe1
[ "$?" != 125 ] && fail

./exe3-1
[ "$?" != 113 ] && fail

./exe3-2
[ "$?" != 113 ] && fail

pass
