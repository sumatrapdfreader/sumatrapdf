#!/usr/bin/env bash
set -eufo pipefail
#
# HEIF codec.
# Copyright (c) 2018 struktur AG, Leon Klingele <leon@struktur.de>
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

realpath() { python -c "import os,sys; print(os.path.realpath(sys.argv[1]))" "$1"; }

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" > /dev/null && pwd)"

FILES=$(find "$DIR"/.. -name *.go)
result=0
for filename in $FILES; do
	filename=$(realpath "${filename}")
	newfile=`mktemp /tmp/fmt.XXXXXX` || exit 1
	gofmt "${filename}" > "${newfile}" 2>> /dev/null
	set +e
	diff -u -p "${filename}" --label "${filename}" --label "${filename} (formatted)" "${newfile}"
	r=$?
	set -e
	rm "${newfile}"
	if [ $r != 0 ] ; then
		result=1
	else
		echo "Done processing ${filename}"
	fi
done

if [ $result != 0 ] ; then
	echo
	echo "Please fix the formatting errors above." >& 2
	exit 1
fi
