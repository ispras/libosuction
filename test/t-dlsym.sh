# Test dlsym support.
$CC -shared libdls.c -o libtdls.so || exit 13
$CC dls.c -ldl -o exe-dls || exit 14
