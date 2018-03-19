#!/bin/sh -

$CC -c -fpic -O2 -flto libpriv1.c -o libpriv1.o || exit 1
$CC -c -fpic -O2 -flto libpriv2.c -o libpriv2.o || exit 2
$CC -shared libpriv1.o libpriv2.o -o libtpriv.so || exit 3

$CC -c main.c -o main.o  || exit 4
$CC main.o -L. -ltpriv -o exe1 || exit 5

$CXX -fno-rtti -g -O0 -c main2.cc -o main2.o  || exit 7
$CXX main2.o -L. -ltpriv -o exe2 || exit 8

$CC -c -fPIC [123].c || exit 9
# Test --gc-sections and workaround for ld bug (the latter could be tested more
# precisely by ld invocation with --as-needed -ltpriv --no-as-needed [12].o)
$CXX [123].o -o exe3-2 || exit 10
$CC  [123].o -o exe3-1 || exit 10
# Test ld -r
ld -r 2.o 3.o -o exe-reloc.os || exit 11
ld -shared -fPIC exe-reloc.os -lc -o lib4.so || exit 12
$CC unrelated.c -L. -l4 -o exe4
