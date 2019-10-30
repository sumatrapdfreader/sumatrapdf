#!/bin/bash

target=i686-w64-mingw32

unset CC
unset CXX
unset CPP
unset LD
unset LDFLAGS
unset CFLAGS
unset CXXFLAGS
unset PKG_CONFIG_PATH

# Removed -static from the following
export CFLAGS="-static-libgcc"
export CXXFLAGS="-static-libgcc -static-libstdc++"
export CPPFLAGS="-I$HOME/.local/$target/include -O2"
export LDFLAGS=-L$HOME/.local/$target/lib
export PKG_CONFIG_LIBDIR=$HOME/.local/$target/lib/pkgconfig
export PATH=$HOME/.local/$target/bin:$PATH

../configure --build=`../config.guess` --host=$target --prefix=$HOME/.local/$target "$@"
