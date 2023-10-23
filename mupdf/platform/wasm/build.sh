#!/bin/bash

MUPDF_DIR=../..
EMSDK_DIR=/opt/emsdk

MUPDF_OPTS="-Os -DTOFU -DTOFU_CJK -DFZ_ENABLE_XPS=0 -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_CBZ=0 -DFZ_ENABLE_IMG=0 -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_JS=0 -DFZ_ENABLE_OCR_OUTPUT=0 -DFZ_ENABLE_DOCX_OUTPUT=0 -DFZ_ENABLE_ODT_OUTPUT=0"

export EMSDK_QUIET=1
source $EMSDK_DIR/emsdk_env.sh
echo

echo BUILDING MUPDF CORE
make -j4 -C $MUPDF_DIR build=release OS=wasm XCFLAGS="$MUPDF_OPTS" libs
echo

echo BUILDING MUPDF WASM
emcc -o lib/mupdf-wasm.js -I $MUPDF_DIR/include lib/mupdf.c \
	--no-entry \
	-sABORTING_MALLOC=0 \
	-sALLOW_MEMORY_GROWTH=1 \
	-sMODULARIZE=1 \
	-sNODEJS_CATCH_EXIT=0 \
	-sWASM_ASYNC_COMPILATION=0 \
	-sEXPORT_NAME='"libmupdf"' \
	-sEXPORTED_RUNTIME_METHODS='["ccall","UTF8ToString","lengthBytesUTF8","stringToUTF8"]' \
	 $MUPDF_DIR/build/wasm/release/libmupdf.a \
	 $MUPDF_DIR/build/wasm/release/libmupdf-third.a
echo
