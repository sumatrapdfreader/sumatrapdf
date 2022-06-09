#!/bin/bash
set -e

meson 	--cross-file=.ci/win64-cross-file.txt \
	--wrap-mode=forcefallback \
	-Dtests=disabled \
	-Dcairo=enabled \
	-Dcairo:fontconfig=disabled \
	-Dglib=enabled \
	-Dfreetype=enabled \
	-Dgdi=enabled \
	-Ddirectwrite=enabled \
	-Dcairo=enabled \
	win64build \
	$@

ninja -Cwin64build -j3 # building with all the cores won't work fine with CricleCI for some reason

rm -rf win64build/harfbuzz-win64
mkdir win64build/harfbuzz-win64
cp win64build/util/hb-*.exe win64build/harfbuzz-win64
find win64build -name '*.dll' -exec cp {} win64build/harfbuzz-win64 \;
x86_64-w64-mingw32-strip win64build/harfbuzz-win64/*.{dll,exe}
rm -f harfbuzz-win64.zip
(cd win64build && zip -r ../harfbuzz-win64.zip harfbuzz-win64)
echo "harfbuzz-win64.zip is ready."
