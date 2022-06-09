#!/usr/bin/env python3

"This tool is intended to be used from meson"

import os, sys, shutil, re

if len (sys.argv) < 4:
	sys.exit(__doc__)

version = sys.argv[1]
major, minor, micro = version.split (".")

OUTPUT = sys.argv[2]
INPUT = sys.argv[3]
CURRENT_SOURCE_DIR = os.path.dirname(INPUT)

try:
	with open (OUTPUT, "r", encoding='utf-8') as old_output:
		for line in old_output:
			old_version = re.match (r"#define HB_VERSION_STRING \"(\d.\d.\d)\"", line)
			if old_version and old_version[1] == version:
				sys.exit ()
except IOError:
	pass

with open (INPUT, "r", encoding='utf-8') as template:
	with open (OUTPUT, "wb") as output:
		output.write (template.read ()
			.replace ("@HB_VERSION_MAJOR@", major)
			.replace ("@HB_VERSION_MINOR@", minor)
			.replace ("@HB_VERSION_MICRO@", micro)
			.replace ("@HB_VERSION@", version)
			.encode ())

# copy it also to the source tree, but only if it has changed
baseline_filename = os.path.join (CURRENT_SOURCE_DIR, os.path.basename (OUTPUT))
with open(baseline_filename, "rb") as baseline:
	with open(OUTPUT, "rb") as generated:
		if baseline.read() != generated.read():
			shutil.copyfile (OUTPUT, baseline_filename)
