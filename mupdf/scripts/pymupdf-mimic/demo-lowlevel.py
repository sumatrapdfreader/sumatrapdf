#! /usr/bin/env python3

import mupdf

import os
import sys

assert len(sys.argv) == 7
filename, page_num, zoom, rotate, output, needle = sys.argv[1:]
page_num = int(page_num)
zoom = int(zoom)
rotate = int(rotate)

document = mupdf.Document(filename)

for key in mupdf.metadata_keys:
    value = document.lookup_metadata(key)
    if value is not None:
        print(f'{key}: {value}')
