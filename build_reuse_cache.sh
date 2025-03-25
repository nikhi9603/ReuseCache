# Correct way to set environment variables
export PATH=~/BTP/gcc7.5/gcc-7.5.0/bin/bin:$PATH
export CC=~/BTP/gcc7.5/gcc-7.5.0/bin/bin/gcc
export CXX=~/BTP/gcc7.5/gcc-7.5.0/bin/bin/g++

make clean

make

bin/champsim -traces traces/445.gobmk-2B.champsimtrace.xz 