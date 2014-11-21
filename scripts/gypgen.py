#!/usr/bin/env python

# assumes that:
# - gyp is checked out into ../gyp directory as:
#   git clone --depth=1 https://github.com/svn2github/gyp.git
# - you run it as python ./scripts/gypgen.py

import sys, os
gyp_dir = os.path.realpath(os.path.join("..", "gyp", "pylib"))
sys.path.append(gyp_dir)

import gyp

if __name__ == '__main__':
    # TODO: set the right generators, it seems to default to msvc
    print("Generating sumatra.sln from sumatra.gyp")
    sys.exit(gyp.script_main())
