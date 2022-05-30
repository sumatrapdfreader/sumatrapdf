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

# Use script from https://chromium.googlesource.com/chromium/src/tools/clang/
# to download prebuilt version of clang. This commit defines which version of
# the script should be used (and thus defines the version of clang).
COMMIT_HASH=f30572cab0ed7d31dc5547e709670ac9d252c6c0

DEST=$1

if [ -z "${DEST}" ]; then
    echo "USAGE: $0 <destination>"
    exit 1
fi

url="https://chromium.googlesource.com/chromium/src/tools/clang/+/${COMMIT_HASH}/scripts/update.py?format=TEXT"

tmpdir=$(mktemp -d)
echo "Using ${tmpdir} as temporary folder"

script_folder=${tmpdir}/tools/clang/scripts
mkdir -p "${script_folder}"
echo "Downloading from ${url} ..."
curl -o "${script_folder}/update.py.b64" ${url}

echo "Decoding base64 ..."
base64 --decode "${script_folder}/update.py.b64" > "${script_folder}/update.py"

echo "Running ${script_folder}/update.py ..."
python "${script_folder}/update.py"

echo "Copying to ${DEST} ..."
mkdir -p "$DEST"
cp -rf "${tmpdir}/third_party/llvm-build/Release+Asserts/"* "${DEST}"

echo "Cleaning up ..."
rm -rf "${tmpdir}"
