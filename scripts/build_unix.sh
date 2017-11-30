#!/bin/bash
set -u -e -o pipefail -o verbose

go run -race tools/build/build_unix.go $@

