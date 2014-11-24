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
    #gyp_file = "sumatra-all.gyp"
    gyp_file = "sumatra.gyp"
    sln_file = gyp_file.split(".")[0]+".sln"
    print("Processing %s to generate %s" % (gyp_file, sln_file))

    os.chdir("gyp")
    # TODO: other possible args:
    # --depth=build\ia32 -f msvs -I common.gypi --generator-output=build\ia32 -G msvs_version=2012 -Dtarget_arch=ia32 -Dhost_arch=ia32
    # -f:
    #   msvs - generates .sln VS solution
    #   ninja - generates ninja makefile
    #   msvs-ninja - generates .sln VS solution that call ninja for the build
    # -d : variables, includes, general, all
    args = ["-G", "msvs_version=2013",
            "-f", "msvs",
            "-Icommon.gypi",
            "--depth", ".",
            #"--depth", "build/32",
            # TODO: this fails with:
            # Warning: Missing input files:
            # build/32/..\..\..\..\..\..\..\bin\nasm.exe
            #"--generator-output=build/32",
            gyp_file]
    # when something goes wrong, add -d all arg to get reasonable error message
    #args.append("-d"); args.append("all")
    gyp.main(args)
