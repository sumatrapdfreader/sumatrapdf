#!/usr/bin/env python3

import os
import re
import sys

srcdir = sys.argv[1]
base_srcdir = sys.argv[2]
builddir = sys.argv[3]

os.chdir(srcdir)


def removeprefix(s):
    abs_path = os.path.join(base_srcdir, s)
    return os.path.relpath(abs_path, srcdir)


HBHEADERS = [os.path.basename(x) for x in os.getenv("HBHEADERS", "").split()] or [
    x for x in os.listdir(".") if x.startswith("hb") and x.endswith(".h")
]
HBSOURCES = [removeprefix(x) for x in os.getenv("HBSOURCES", "").split()] or [
    x for x in os.listdir(".") if x.startswith("hb") and x.endswith((".cc", ".hh"))
]


stat = 0

for x in HBHEADERS + HBSOURCES:
    if not x.endswith("h") or x == "hb-gobject-structs.h":
        continue
    print(f"Checking {x}")
    tag = (
        x.upper()
        .replace(".", "_")
        .replace("-", "_")
        .replace(os.path.sep, "_")
        .replace("/", "_")
    )
    with open(x, "r", encoding="utf-8") as f:
        content = f.read()
    if len(re.findall(tag + r"\b", content)) != 3:
        print(
            "Ouch, header file %s does not have correct preprocessor guards. Expected: %s"
            % (x, tag)
        )
        stat = 1

sys.exit(stat)
