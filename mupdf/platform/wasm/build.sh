#!/bin/bash

make -j4 -C ../.. generate

source /opt/emsdk/emsdk_env.sh

echo Building library:
make -j4 -C ../.. \
	OS=wasm build=release \
	XCFLAGS="-DTOFU -DTOFU_CJK -DFZ_ENABLE_SVG=0 -DFZ_ENABLE_HTML=0 -DFZ_ENABLE_EPUB=0 -DFZ_ENABLE_JS=0" \
	libs

echo
echo Linking WebAssembly:
emcc -Wall -Os -o libmupdf.js \
	-s WASM=1 \
	-s VERBOSE=0 \
	-s ABORTING_MALLOC=0 \
	-s TOTAL_MEMORY=134217728 \
	-s EXTRA_EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
	-s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE='[$Browser,"memcpy","memset","malloc","free"]' \
	-I ../../include \
	--pre-js wrap.js \
	wrap.c \
	../../build/wasm/release/libmupdf.a \
	../../build/wasm/release/libmupdf-third.a

echo Done.
