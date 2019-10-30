#!/usr/bin/env python

from __future__ import print_function, division, absolute_import

import io, os, re, sys

if len (sys.argv) < 3:
	sys.exit("usage: gen-def.py harfbuzz.def hb.h [hb-blob.h hb-buffer.h ...]")

output_file = sys.argv[1]
header_paths = sys.argv[2:]

headers_content = []
for h in header_paths:
	if h.endswith (".h"):
		with io.open (h, encoding='utf-8') as f: headers_content.append (f.read ())

result = """EXPORTS
%s
LIBRARY lib%s-0.dll""" % (
	"\n".join (sorted (re.findall (r"^hb_\w+(?= \()", "\n".join (headers_content), re.M))),
	output_file.replace ('.def', '')
)

with open (output_file, "w") as f: f.write (result)
