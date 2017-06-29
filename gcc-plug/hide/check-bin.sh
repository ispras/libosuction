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
# staticvar is only used locally and thus shouldn't get a name (?)
# libprivate call should not go through plt
(objdump -d test-libpriv.so | egrep "(foo|staticvar|\blibprivate@plt)") && fail

# Uppercase letters in nm output mean global symbol visibility.
(nm test-libpriv.so | grep libpriv | grep [A-Z]) && fail

pass
