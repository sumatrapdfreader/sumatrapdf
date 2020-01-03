#!/bin/bash

set -x
set -o errexit -o nounset

# 22.0.16 is the libtool version of 2.9.0
if pkg-config --atleast-version 22.0.16 freetype2; then exit; fi

pushd $HOME
wget http://download.savannah.gnu.org/releases/freetype/freetype-2.9.tar.bz2
tar xf freetype-2.9.tar.bz2
pushd freetype-2.9
./autogen.sh
./configure --prefix=$HOME/.local
make -j4 install
popd
popd
