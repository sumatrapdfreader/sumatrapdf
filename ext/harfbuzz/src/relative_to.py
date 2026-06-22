#!/usr/bin/env python3

import sys
from os import path

print(path.relpath(sys.argv[1], sys.argv[2]))
