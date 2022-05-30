#!/bin/bash
set -e
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

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"

BUILD_ROOT=$ROOT/..

PKG_CONFIG_PATH=
if [ "$WITH_LIBDE265" = "2" ]; then
    PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$BUILD_ROOT/libde265/dist/lib/pkgconfig/"
fi

if [ "$WITH_RAV1E" = "1" ]; then
    PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$BUILD_ROOT/third-party/rav1e/dist/lib/pkgconfig/"
fi

if [ "$WITH_DAV1D" = "1" ]; then
    PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$BUILD_ROOT/third-party/dav1d/dist/lib/x86_64-linux-gnu/pkgconfig/"
fi
if [ ! -z "$PKG_CONFIG_PATH" ]; then
    export PKG_CONFIG_PATH="$PKG_CONFIG_PATH"
fi

CONFIGURE_HOST=
if [ "$MINGW" == "32" ]; then
    CONFIGURE_HOST=i686-w64-mingw32
elif [ "$MINGW" == "64" ]; then
    CONFIGURE_HOST=x86_64-w64-mingw32
fi

if [ -z "$CHECK_LICENSES" ] && [ -z "$CPPLINT" ] && [ -z "$CMAKE" ]; then
    ./autogen.sh
    CONFIGURE_ARGS=
    if [ -z "$CONFIGURE_HOST" ]; then
        if [ ! -z "$FUZZER" ]; then
            export CC="$BUILD_ROOT/clang/bin/clang"
            export CXX="$BUILD_ROOT/clang/bin/clang++"
            FUZZER_FLAGS="-fsanitize=fuzzer-no-link,address,shift,integer -fno-sanitize-recover=shift,integer"
            export CFLAGS="$CFLAGS -g -O0 $FUZZER_FLAGS"
            export CXXFLAGS="$CXXFLAGS -g -O0 $FUZZER_FLAGS"
            CONFIGURE_ARGS="$CONFIGURE_ARGS --enable-libfuzzer=-fsanitize=fuzzer"
        fi
    else
        # Make sure the correct compiler will be used.
        unset CC
        unset CXX
        CONFIGURE_ARGS="$CONFIGURE_ARGS --host=$CONFIGURE_HOST"
    fi
    if [ ! -z "$GO" ]; then
        CONFIGURE_ARGS="$CONFIGURE_ARGS --prefix=$BUILD_ROOT/dist --disable-gdk-pixbuf"
    else
        CONFIGURE_ARGS="$CONFIGURE_ARGS --disable-go"
    fi
    if [ ! -z "$TESTS" ]; then
        CONFIGURE_ARGS="$CONFIGURE_ARGS --enable-tests"
    fi
    if [ "$WITH_RAV1E" = "1" ]; then
        CONFIGURE_ARGS="$CONFIGURE_ARGS --enable-local-rav1e"
    fi
    ./configure $CONFIGURE_ARGS
fi
