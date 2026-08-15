#!/bin/bash

echo BROTLI
test -f dist/mupdf.js.br || brotli dist/mupdf.js
test -f dist/mupdf-wasm.js.br || brotli dist/mupdf-wasm.js
test -f dist/mupdf-wasm.wasm.br || brotli dist/mupdf-wasm.wasm

echo LICENSE
test -f ../../COPYING && cp -f ../../COPYING LICENSE
test -f ../../LICENSE && cp -f ../../LICENSE LICENSE
true
