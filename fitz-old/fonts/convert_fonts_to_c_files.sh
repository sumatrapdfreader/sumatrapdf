#!/bin/bash
for fl in *.cff; do
  xxd -i $fl | sed -e 's/unsigned/const unsigned/' | sed -e 's/\([^[:space:]]*\)_cff/fonts_\1_cff/' > $fl.c
  ls -lah $fl.c
done