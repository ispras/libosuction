#!/bin/sh -

./exe-dls > /dev/null || die
./exe-dls | grep FAIL && die

true
