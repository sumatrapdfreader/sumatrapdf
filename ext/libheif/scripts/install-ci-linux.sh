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

INSTALL_PACKAGES=
REMOVE_PACKAGES=
BUILD_ROOT=$ROOT/..
UPDATE_APT=
ADD_LIBHEIF_PPA=
CURRENT_BRANCH=$TRAVIS_BRANCH
if [ -z "$CURRENT_BRANCH" ]; then
    CURRENT_BRANCH=${GITHUB_REF##*/}
fi

if [ "$WITH_LIBDE265" = "1" ]; then
    echo "Adding PPA strukturag/libde265 ..."
    sudo add-apt-repository -y ppa:strukturag/libde265
    UPDATE_APT=1
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        libde265-dev \
        "
fi

if [ "$WITH_LIBDE265" = "2" ]; then
    echo "Installing libde265 from frame-parallel branch ..."
    git clone --depth 1 -b frame-parallel https://github.com/strukturag/libde265.git
    pushd libde265
    ./autogen.sh
    ./configure \
        --prefix=$BUILD_ROOT/libde265/dist \
        --disable-dec265 \
        --disable-sherlock265 \
        --disable-hdrcopy \
        --disable-enc265 \
        --disable-acceleration_speed
    make -j $(nproc) && make -j $(nproc) install
    popd
fi

if [ "$WITH_AOM" = "1" ]; then
    ADD_LIBHEIF_PPA=1
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        libaom-dev \
        "
fi

if [ "$WITH_X265" = "1" ]; then
    ADD_LIBHEIF_PPA=1
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        libx265-dev \
        "
fi

if [ "$WITH_DAV1D" = "1" ]; then
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        nasm \
        ninja-build \
        python3-pip \
        python3-setuptools \
        python3-wheel \
        "
fi

if [ "$WITH_RAV1E" = "1" ]; then
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        cargo \
        nasm \
        "
fi

if [ ! -z "$CHECK_LICENSES" ]; then
    sudo curl --location --output /usr/bin/licensecheck "https://github.com/Debian/devscripts/raw/v2.16.5/scripts/licensecheck.pl"
    sudo chmod a+x /usr/bin/licensecheck
fi

if [ -z "$WITH_GRAPHICS" ] && [ -z "$CHECK_LICENSES" ] && [ -z "$CPPLINT" ]; then
    REMOVE_PACKAGES="$REMOVE_PACKAGES \
        libjpeg.*-dev \
        libpng.*-dev \
        "
fi

if [ ! -z "$WITH_GRAPHICS" ]; then
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        libgdk-pixbuf2.0-dev \
        libjpeg-dev \
        libpng-dev \
        "
fi

if [ "$MINGW" == "32" ]; then
    sudo dpkg --add-architecture i386
    # https://github.com/actions/virtual-environments/issues/4589
    sudo apt install -y --allow-downgrades libpcre2-8-0=10.34-7
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        binutils-mingw-w64-i686 \
        g++-mingw-w64-i686 \
        gcc-mingw-w64-i686 \
        mingw-w64-i686-dev \
        wine-stable \
        wine32 \
        "
    UPDATE_APT=1
elif [ "$MINGW" == "64" ]; then
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        binutils-mingw-w64-x86-64 \
        g++-mingw-w64-x86-64 \
        gcc-mingw-w64-x86-64 \
        mingw-w64-x86-64-dev \
        wine-stable \
        "
fi

if [ ! -z "$ADD_LIBHEIF_PPA" ]; then
    echo "Adding PPA strukturag/libheif ..."
    sudo add-apt-repository -y ppa:strukturag/libheif
    UPDATE_APT=1
fi

if [ ! -z "$INSTALL_PACKAGES" ]; then
    # The CI environment might have old package lists, so always update before installing.
    UPDATE_APT=1
fi

if [ ! -z "$UPDATE_APT" ]; then
    echo "Updating package lists ..."
    sudo apt-get update
fi

if [ ! -z "$INSTALL_PACKAGES" ]; then
    echo "Installing packages $INSTALL_PACKAGES ..."
    sudo apt-get install $INSTALL_PACKAGES
fi

if [ ! -z "$REMOVE_PACKAGES" ]; then
    echo "Removing packages $REMOVE_PACKAGES ..."
    sudo apt-get remove $REMOVE_PACKAGES
fi

if [ ! -z "$EMSCRIPTEN_VERSION" ]; then
    echo "Installing emscripten $EMSCRIPTEN_VERSION to $BUILD_ROOT/emscripten ..."
    mkdir -p $BUILD_ROOT/emscripten
    ./scripts/install-emscripten.sh $EMSCRIPTEN_VERSION $BUILD_ROOT/emscripten
fi

if [ ! -z "$FUZZER" ]; then
    ./scripts/install-clang.sh "$BUILD_ROOT/clang"
fi

if [ "$CURRENT_BRANCH" = "coverity" ]; then
    echo "Installing coverity build tool ..."
    echo -n | openssl s_client -connect scan.coverity.com:443 | sed -ne '/-BEGIN CERTIFICATE-/,/-END CERTIFICATE-/p' | sudo tee -a /etc/ssl/certs/ca-certificates.crt
fi

if [ "$MINGW" == "32" ]; then
    if [ -x "/usr/bin/i686-w64-mingw32-g++-posix" ]; then
        sudo update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix
    fi
elif [ "$MINGW" == "64" ]; then
    if [ -x "/usr/bin/x86_64-w64-mingw32-g++-posix" ]; then
        sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
    fi
fi

if [ "$WITH_DAV1D" = "1" ]; then
    pip3 install --user meson

    export PATH="$PATH:$HOME/.local/bin"
    cd third-party
    sh dav1d.cmd # dav1d does not support this option anymore: -Denable_avx512=false
    cd ..
fi

if [ "$WITH_RAV1E" = "1" ]; then
    cargo install --force cargo-c

    export PATH="$PATH:$HOME/.cargo/bin"
    cd third-party
    sh rav1e.cmd
    cd ..
fi
