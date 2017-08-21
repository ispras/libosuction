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

# The executalbe should run and produce the expected return value.
LD_LIBRARY_PATH=. ./exe1
[ "$?" != 125 ] && fail

pass
