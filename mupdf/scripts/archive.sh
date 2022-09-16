#!/bin/bash

REV=$(git describe --tags)
STEM=mupdf-$REV-source

echo git archive $STEM.tar
git archive --format=tar --prefix=$STEM/ -o $STEM.tar HEAD

function make_submodule_archive {
	# Make tarballs for submodules, stripped of unneccessary files.
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

make_submodule_archive curl		tests
make_submodule_archive extract		test
make_submodule_archive freeglut
make_submodule_archive freetype		tests
make_submodule_archive gumbo-parser	benchmarks tests
make_submodule_archive harfbuzz		test perf
make_submodule_archive jbig2dec
make_submodule_archive lcms2		testbed plugins/fast_float
make_submodule_archive leptonica	prog
make_submodule_archive libjpeg		libjpeg/test*
make_submodule_archive mujs
make_submodule_archive openjpeg
make_submodule_archive tesseract	unittest
make_submodule_archive zlib		test contrib

echo gzip $STEM.tar
pigz -f -k -11 $STEM.tar

echo lzip $STEM.tar
plzip -9 -f -k $STEM.tar
