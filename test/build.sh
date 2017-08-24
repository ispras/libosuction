#!/bin/bash -

cd $(dirname $(realpath $0))

$CC -c -fpic -O2 -flto libpriv1.c -o libpriv1.o || exit 1
$CC -c -fpic -O2 -flto libpriv2.c -o libpriv2.o || exit 2
$CC -shared libpriv1.o libpriv2.o -o libtpriv.so || exit 3

$CC -c main.c -o main.o  || exit 4
$CC main.o -L. -ltpriv -o exe1 || exit 5