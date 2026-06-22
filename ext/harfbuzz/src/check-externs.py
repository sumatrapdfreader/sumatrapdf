#!/usr/bin/env python3

import os
import re
import sys

srcdir = sys.argv[1]
base_srcdir = sys.argv[2]
builddir = sys.argv[3]

os.chdir(srcdir)

HBHEADERS = [os.path.basename(x) for x in os.getenv("HBHEADERS", "").split()] or [
    x for x in os.listdir(".") if x.startswith("hb") and x.endswith(".h")
]

stat = 0

print("Checking that all public symbols are exported with HB_EXTERN")
for x in HBHEADERS:
    print(f"Checking {x}")
    with open(x, "r", encoding="utf-8") as f:
        content = f.read()
    for s in re.findall(r"\n.+\nhb_.+\n", content):
        if not s.startswith("\nHB_EXTERN "):
            print("failure on:", s)
            stat = 1

sys.exit(stat)
