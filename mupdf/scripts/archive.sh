#!/bin/bash

REV=$(git describe --tags)
STEM=mupdf-$REV-source
CSTEM=mupdf-$REV-source-commercial

echo git archive $STEM.tar
git archive --format=tar --prefix=$STEM/ -o $STEM.tar HEAD

function make_submodule_archive {
	# Make tarballs for submodules, stripped of unnecessary files.
	M=$1
	shift
	echo git archive submodule-$M.tar
	git archive --format=tar --remote=thirdparty/$M --prefix=$STEM/thirdparty/$M/ -o submodule-$M.tar HEAD
	for DIR in $*
	do
		tar f submodule-$M.tar --wildcards --delete "*/$DIR"
	done
	tar Af $STEM.tar submodule-$M.tar
	rm -f submodule-$M.tar
}

# Remove test files from thirdparty source archives.

make_submodule_archive brotli		tests
make_submodule_archive curl		tests
make_submodule_archive extract		test
make_submodule_archive freeglut
make_submodule_archive freetype		tests
make_submodule_archive gumbo-parser	benchmarks tests
make_submodule_archive harfbuzz		perf
make_submodule_archive jbig2dec
make_submodule_archive lcms2		testbed plugins/fast_float
make_submodule_archive leptonica	prog
make_submodule_archive libjpeg		libjpeg/test*
make_submodule_archive mujs
make_submodule_archive openjpeg
make_submodule_archive tesseract	unittest
make_submodule_archive zint
make_submodule_archive zlib		test contrib
make_submodule_archive zxing-cpp	core/src/libzint

# Generate commercial tarball
cp $STEM.tar $CSTEM.tar
tar f $CSTEM.tar --wildcards --delete ${STEM}/COPYING
tar -r -f $CSTEM.tar --owner=0 --group=0 --mode=664 --transform=s,$(dirname "$0")/customer\.txt,${STEM}/LICENSE, "$(dirname "$0")/customer.txt"

echo gzip $CSTEM.tar
pigz -f -k -11 $CSTEM.tar
rm -f $CSTEM.tar

echo gzip $STEM.tar
pigz -f -k -11 $STEM.tar

echo lzip $STEM.tar
plzip -9 -f -k $STEM.tar
rm -f $STEM.tar
