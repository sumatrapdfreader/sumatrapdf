#!/bin/bash
set -u -e -o pipefail -o verbose

go run -race tools/build/build_unix.go -release $@
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all ./out/rel64/test_unix
