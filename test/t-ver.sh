#!/bin/sh -

as ver.s -o 1-ver.o
$CC -c -fpic ver.c -o 2-ver.o
$CC -Wl,--version-script,ver.lds -e main -shared [12]-ver.o -o exe-ver
