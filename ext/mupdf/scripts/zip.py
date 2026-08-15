#!/bin/env python
# Create a reproducible build ZIP archive.

import zipfile, sys, os
zip = zipfile.ZipFile(sys.argv[1], 'w')
for file in sorted(sys.argv[2:]):
	zip.writestr(zipfile.ZipInfo(os.path.basename(file), (1997, 8, 29, 2, 14, 0)), open(file).read())
