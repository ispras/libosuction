#!/bin/bash

# It is a multi-version testing script, for checking plugin with different
# GCC versions.
COMPILER_PATH=/mnt/wspace/Workspace/install

# GCC 4.9.0
CC="$COMPILER_PATH/gcc-4_9_0/bin/x86_64-unknown-linux-gnu-gcc"
CXX="$COMPILER_PATH/gcc-4_9_0/bin/x86_64-unknown-linux-gnu-g++"
make -C ../../ CC=$CC CXX=$CXX -B
make test CC=$CC CXX=$CXX -B

# System GCC 5.4.0
CC=gcc
CXX=g++
make -C ../../ CC=$CC CXX=$CXX -B
make test CC=$CC CXX=$CXX -B

# GCC 6.1.0
CC="$COMPILER_PATH/gcc-6_1_0/bin/x86_64-pc-linux-gnu-gcc"
CXX="$COMPILER_PATH/gcc-6_1_0/bin/x86_64-pc-linux-gnu-g++"
make -C ../../ CC=$CC CXX=$CXX -B
make test CC=$CC CXX=$CXX -B

# GCC 7.1.0
CC="$COMPILER_PATH/gcc-7_1_0/bin/x86_64-pc-linux-gnu-gcc"
CXX="$COMPILER_PATH/gcc-7_1_0/bin/x86_64-pc-linux-gnu-g++"
make -C ../../ CC=$CC CXX=$CXX -B
make test CC=$CC CXX=$CXX -B

