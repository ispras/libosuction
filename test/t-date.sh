#!/bin/sh -

cat > date-nano.h << EOF
#define NANO "$(date +%N)"
EOF

$CC -Wno-pointer-to-int-cast date.c -o exe-date  || exit 1
