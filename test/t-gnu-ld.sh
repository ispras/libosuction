#!/bin/sh -

# Test that ld wrapper doesn't break without input files, and still calls the real ld.
# Grep works fine for both bfd and gold.

ld -o conftest -v < /dev/null | grep GNU &>/dev/null || exit 3
