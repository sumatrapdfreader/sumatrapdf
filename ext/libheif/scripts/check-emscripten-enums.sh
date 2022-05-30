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

DEFINE_TYPES="
    heif_error_
    heif_suberror_
    heif_compression_
    heif_chroma_
    heif_colorspace_
    heif_channel_
    "

API_DEFINES=""
for type in $DEFINE_TYPES; do
    DEFINES=$(grep "^[ \t]*$type" libheif/heif.h | sed 's|[[:space:]]*\([^ \t=]*\)[[:space:]]*=.*|\1|g')
    if [ -z "$API_DEFINES" ]; then
        API_DEFINES="$DEFINES"
    else
        API_DEFINES="$API_DEFINES
$DEFINES"
    fi
    ALIASES=$(grep "^[ \t]*#define $type" libheif/heif.h | sed 's|[[:space:]]*#define \([^ \t]*\)[[:space:]]*.*|\1|g')
    if [ ! -z "$ALIASES" ]; then
        API_DEFINES="$API_DEFINES
$ALIASES"
    fi
done
API_DEFINES=$(echo "$API_DEFINES" | sort)

EMSCRIPTEN_DEFINES=""
for type in $DEFINE_TYPES; do
    DEFINES=$(grep "\.value(\"$type" libheif/heif_emscripten.h | sed 's|[^\"]*\"\(.*\)\".*|\1|g')
    if [ -z "$EMSCRIPTEN_DEFINES" ]; then
        EMSCRIPTEN_DEFINES="$DEFINES"
    else
        EMSCRIPTEN_DEFINES="$EMSCRIPTEN_DEFINES
$DEFINES"
    fi
done
EMSCRIPTEN_DEFINES=$(echo "$EMSCRIPTEN_DEFINES" | sort)

set +e
CHANGES=$(diff -u <(echo "$API_DEFINES") <(echo "$EMSCRIPTEN_DEFINES"))
set -e
if [ -z "$CHANGES" ]; then
    echo "All defines from heif.h are present in heif_emscripten.h"
    exit 0
fi

echo "Differences found between enum defines in heif.h and heif_emscripten.h."
echo "Lines prefixed with '+' are only in heif_emscripten.h, resulting in"
echo "compile errors. Lines prefixed with '-' are missing in heif_emscripten.h"
echo
echo "$CHANGES"
exit 1
