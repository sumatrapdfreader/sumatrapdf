#!/bin/bash
set -e

meson --cross-file=.ci/win32-cross-file.txt win32build --wrap-mode=forcefallback -Dtests=disabled \
	-Dglib=enabled -Dfreetype=enabled -Dgdi=enabled -Dcairo=enabled --buildtype=release $@
ninja -Cwin32build -j3 # building with all the cores won't work fine with CricleCI for some reason

rm -rf win32build/harfbuzz-win32
mkdir win32build/harfbuzz-win32
cp win32build/util/hb-*.exe win32build/harfbuzz-win32
find win32build -name '*.dll' -exec cp {} win32build/harfbuzz-win32 \;
i686-w64-mingw32-strip win32build/harfbuzz-win32/*.{dll,exe}
rm -f harfbuzz-win32.zip
(cd win32build/harfbuzz-win32 && zip ../../harfbuzz-win32.zip -r .)
echo "harfbuzz-win32.zip is ready."
