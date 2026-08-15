#!/bin/bash
#
# Script to build the WASM binary.
#
# The following environment variables can be used to configure the build:
#   EMSDK, BUILD, SUFFIX, DEFINES, FEATURES
#
# EMSDK must point to the directory where emsdk is installed
# BUILD should be "small" or "memento"
# DEFINES should be a set of -D defines (e.g. "-DTOFU")
# FEATURES should be a list of "feature=no" (e.g. "brotli=no mujs=no extract=no xps=no svg=no html=no")
# SUFFIX should be a tag to keep build files for different configurations separate
#

EMSDK=${EMSDK:-/opt/emsdk}
BUILD=${BUILD:-small}
DEFINES=${DEFINES:--DTOFU -DTOFU_CJK_EXT -DFZ_ENABLE_HYPHEN=0}
FEATURES=${FEATURES:-brotli=no mujs=no extract=no xps=no svg=no}

if [ "$BUILD" = "memento" ]
then
	MEMENTO="-DMEMENTO -DMEMENTO_STACKTRACE_METHOD=0"
fi

export EMSDK_QUIET=1
source $EMSDK/emsdk_env.sh

emsdk install 4.0.8 >/dev/null || exit
emsdk activate 4.0.8 >/dev/null || exit

mkdir -p dist
rm -f dist/*

echo "COMPILING ($BUILD)"

make --no-print-directory -j $(nproc) \
	-C ../.. \
	build=$BUILD \
	build_suffix=$SUFFIX \
	OS=wasm \
	XCFLAGS="$MEMENTO $DEFINES" \
	$FEATURES \
	libs

echo "LINKING"

emcc -o dist/mupdf-wasm.js \
	-I ../../include \
	-Os -g2 \
	$MEMENTO \
	--no-entry \
	-mno-nontrapping-fptoint \
	-fwasm-exceptions \
	-sSUPPORT_LONGJMP=wasm \
	-sMODULARIZE=1 \
	-sEXPORT_ES6=1 \
	-sEXPORT_NAME='"libmupdf_wasm"' \
	-sALLOW_MEMORY_GROWTH=1 \
	-sTEXTDECODER=2 \
	-sFILESYSTEM=0 \
	-sEXPORTED_RUNTIME_METHODS='["UTF8ToString","lengthBytesUTF8","stringToUTF8","HEAPU8","HEAP32","HEAPU32","HEAPF32"]' \
	lib/mupdf.c \
	 ../../build/wasm/$BUILD$SUFFIX/libmupdf.a \
	 ../../build/wasm/$BUILD$SUFFIX/libmupdf-third.a

# remove executable bit from wasm-ld output
chmod -x dist/mupdf-wasm.wasm

echo "TYPESCRIPT"

sed < lib/mupdf.c '/#include/d' | emcc -E - | node tools/make-wasm-type.js > lib/mupdf-wasm.d.ts
cp lib/mupdf-wasm.d.ts dist/mupdf-wasm.d.ts

npx tsc -p .

# convert spaces to tabs
sed -i -e 's/    /\t/g' dist/mupdf.js

echo "TERSER"

npx terser --module -c -m -o dist/mupdf-wasm.js dist/mupdf-wasm.js
# npx terser --module -c -m -o dist/mupdf.min.js dist/mupdf.js
