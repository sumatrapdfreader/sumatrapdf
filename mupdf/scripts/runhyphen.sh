#!/bin/bash
#
# Generate hyphenation data zip archive.
#
# requires: python and advzip (from advcomp linux package)

set -x

mkdir -p generated/resources/hyphen

python scripts/zip.py resources/hyphen/hyph-std.zip resources/hyphen/core/*
python scripts/zip.py resources/hyphen/hyph-all.zip resources/hyphen/core/* resources/hyphen/extra/*

advzip -4 -z resources/hyphen/hyph-std.zip
advzip -4 -z resources/hyphen/hyph-all.zip

bash scripts/hexdump.sh > generated/resources/hyphen/hyph-std.zip.c resources/hyphen/hyph-std.zip
bash scripts/hexdump.sh > generated/resources/hyphen/hyph-all.zip.c resources/hyphen/hyph-all.zip
