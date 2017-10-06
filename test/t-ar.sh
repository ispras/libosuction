#!/bin/sh -

$CC -c -O2 libpriv1.c -o libpriv1.o || exit 1
$CC -c -O2 libpriv2.c -o libpriv2.o || exit 2
ar rc libpriv.a libpriv1.o libpriv2.o

$CC -c main.c -o main.o  || exit 4
$CC main.o libpriv.a -o exe1 || exit 5
