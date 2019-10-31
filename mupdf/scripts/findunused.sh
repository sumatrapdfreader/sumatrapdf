#!/bin/bash
make build=debug -j4
rm -f build/debug/mutool build/debug/mupdf-gl
make build=debug XLIBS=-Wl,--print-gc-sections build/debug/mutool 2>&1 | grep 'libmupdf\.' | sort > build/debug/mutool.gc
make build=debug XLIBS=-Wl,--print-gc-sections build/debug/mupdf-gl 2>&1 | grep 'libmupdf\.' | sort >build/debug/mupdf-gl.gc
comm -12 build/debug/mutool.gc build/debug/mupdf-gl.gc
