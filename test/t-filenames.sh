#!/bin/sh -

# Test that generated srcid is independent of the file's name.

cp filenames.c filenames-$$.c
$CC filenames-$$.c
rm filenames-$$.c
