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

# staticfoo an unused function,
# libprivate call should not go through plt
(objdump -d test-libpriv.so | egrep "(staticfoo|\blibprivate@plt)") && fail

# Uppercase letters in nm output mean global symbol visibility.
(nm test-libpriv.so | egrep "(libpriv|staticvar)" | grep [A-Z]) && fail

pass
