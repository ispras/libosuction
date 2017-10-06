#!/bin/sh -

# Unused symbols should not appear in the dump.
objdump -d libpriv.a | egrep "unused(foo)" && exit 1

true
