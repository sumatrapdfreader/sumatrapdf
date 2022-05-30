#!/bin/bash
#
# HEIF codec.
# Copyright (c) 2018 struktur AG, Joachim Bauch <bauch@struktur.de>
#
# This file is part of libheif.
#
# libheif is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# libheif is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with libheif.  If not, see <http://www.gnu.org/licenses/>.
#

FUZZER_FLAGS="-fsanitize=fuzzer-no-link,address,shift,integer -fno-sanitize-recover=shift,integer" # ,undefined
export CFLAGS="$CFLAGS $FUZZER_FLAGS"
export CXXFLAGS="$CXXFLAGS $FUZZER_FLAGS"
export CXX=clang-7
export CC=clang-7

CONFIGURE_ARGS="$CONFIGURE_ARGS --disable-go"
CONFIGURE_ARGS="$CONFIGURE_ARGS --enable-libfuzzer=-fsanitize=fuzzer"
exec ./configure $CONFIGURE_ARGS

export TSAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-7
export MSAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-7
export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-7
