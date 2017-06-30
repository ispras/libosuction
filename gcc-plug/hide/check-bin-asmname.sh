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

(nm test-asmname.so | grep staticfoo3 | grep [A-Z]) && fail

pass
