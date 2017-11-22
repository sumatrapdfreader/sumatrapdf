#!/bin/bash
set -u -e -o pipefail

LDFLAGS="-L/usr/local/opt/llvm/lib -Wl,-rpath,/usr/local/opt/llvm/lib"
CXX_FLAGS="-std=c++1z -Wall -Wextra"
INC="-Iext/unarr -Isrc/utils -Isrc"
CC="clang"
OUTDIR=""

set_mac_rel_64() {
    OUT_DIR="out/rel64"
    OBJ_DIR="${OUT_DIR}/obj"
    rm -rf "${OBJ_DIR}"
    mkdir -p "${OBJ_DIR}"
}

set_mac_dbg_64() {
    OUT_DIR="out/dbg64"
    OBJ_DIR="${OUT_DIR}/obj"
    rm -rf "${OBJ_DIR}"
    mkdir -p "${OBJ_DIR}"
}

build() {
    echo "building"

    $CC ${CXX_FLAGS} ${INC} "src/utils/Archive.cpp" -c -o "${OBJ_DIR}/Archive.o"
    $CC ${CXX_FLAGS} ${INC}  "src/utils/BaseUtil.cpp" -c -o "${OBJ_DIR}/BaseUtil.o"
    $CC ${CXX_FLAGS} ${INC}  "src/utils/FileUtil.cpp" -c -o "${OBJ_DIR}/FileUtil.o"
    $CC ${CXX_FLAGS} ${INC}  "src/utils/StrUtil.cpp" -c -o "${OBJ_DIR}/StrUtil.o"
    $CC ${CXX_FLAGS} ${INC}  "src/utils/UtAssert.cpp" -c -o "${OBJ_DIR}/UtAssert.o"
    $CC ${CXX_FLAGS} ${INC}  "tools/test_unix/main.cpp" -c -o "${OBJ_DIR}/test_unix_main.o"
}

set_mac_rel_64
echo "OUT_DIR: $OUT_DIR"
build

set_mac_dbg_64
echo "OUT_DIR: $OUT_DIR"
