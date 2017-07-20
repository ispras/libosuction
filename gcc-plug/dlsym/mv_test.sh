#!/bin/bash

# It is a multi-version testing script, for checking plugin with different
# GCC versions.

# GCC 4.9.0
CC="/home/eugene/Workspace/install/gcc-4_9_0/bin/x86_64-unknown-linux-gnu-gcc"
CXX="/home/eugene/Workspace/install/gcc-4_9_0/bin/x86_64-unknown-linux-gnu-g++"
make test CC=$CC CXX=$CXX -B

# System GCC 5.4.0
CC=gcc
CXX=g++
make test CC=$CC CXX=$CXX -B

# GCC 6.1.0
CC="/home/eugene/Workspace/install/gcc-6_1_0/bin/x86_64-pc-linux-gnu-gcc"
CXX="/home/eugene/Workspace/install/gcc-6_1_0/bin/x86_64-pc-linux-gnu-g++"
make test CC=$CC CXX=$CXX -B

# GCC 8.0.0
CC="/home/eugene/Workspace/install/gcc-trunk/bin/x86_64-pc-linux-gnu-gcc"
CXX="/home/eugene/Workspace/install/gcc-trunk/bin/x86_64-pc-linux-gnu-g++"
make test CC=$CC CXX=$CXX -B

