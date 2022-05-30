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

INSTALL_PACKAGES="\
    autoconf \
    automake \
    pkg-config \
    "
REMOVE_PACKAGES=

if [ ! -z "$WITH_AOM" ]; then
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        aom \
        "
fi

if [ ! -z "$WITH_LIBDE265" ]; then
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        libde265 \
        "
fi

if [ ! -z "$WITH_X265" ]; then
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        x265 \
        "
fi

if [ -z "$WITH_GRAPHICS" ] && [ -z "$CHECK_LICENSES" ] && [ -z "$CPPLINT" ]; then
    REMOVE_PACKAGES="$REMOVE_PACKAGES \
        libjpeg \
        libpng \
        "
fi

if [ ! -z "$WITH_GRAPHICS" ]; then
    INSTALL_PACKAGES="$INSTALL_PACKAGES \
        libjpeg \
        libpng \
        "
fi

if [ ! -z "$REMOVE_PACKAGES" ] || [ ! -z "$INSTALL_PACKAGES" ]; then
    # Need to update homebrew before installing / removing packages to make sure
    # the correct version of ruby is used.
    brew update
fi

if [ ! -z "$REMOVE_PACKAGES" ]; then
    echo "Removing packages $REMOVE_PACKAGES ..."
    for package in $REMOVE_PACKAGES; do
        brew list $package &>/dev/null && brew uninstall --ignore-dependencies $package
    done
fi

if [ ! -z "$INSTALL_PACKAGES" ]; then
    echo "Installing packages $INSTALL_PACKAGES ..."
    for package in $INSTALL_PACKAGES; do
        brew list $package &>/dev/null || brew install $package
    done
fi
