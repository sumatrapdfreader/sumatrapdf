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

    os.chdir("gyp")
    # TODO: other possible args:
    # --depth=build\ia32 -f msvs -I common.gypi --generator-output=build\ia32 -G msvs_version=2012 -Dtarget_arch=ia32 -Dhost_arch=ia32
    # -f:
    #   msvs - generates .sln VS solution
    #   ninja - generates ninja makefile
    #   msvs-ninja - generates .sln VS solution that call ninja for the build
    # -d : variables, includes, general, all
    args = ["-G", "msvs_version=2013", "-f", "msvs", "--depth=.", "sumatra.gyp"]
    # when something goes wrong, add -d all arg to get reasonable error message
    #args.append("-d"); args.append("all")
    gyp.main(args)
