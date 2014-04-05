#!/usr/bin/env python

# This is a script to automate runnig of flint
# (https://code.facebook.com/posts/729709347050548/under-the-hood-building-and-open-sourcing-flint/), facebook's C++ linter.
# So far I only got it to compile on Mac.
# Things to know about flint:
# - it dumps output to stderr
# - it contains rules we don't care about, so we need to filter thouse out

import os
import sys
#import glob
import subprocess
import util
import util2


def verify_flint_installed():
    try:
        util.run_cmd_throw("flint")
    except:
        print("flint doesn't seem to be installed")
        print("https://github.com/facebook/flint")
        sys.exit(1)


@util2.memoize
def src_dir():
    return "src"


@util2.memoize
def src_utils_dir():
    return os.path.join("src", "utils")

def run_flint_in_dir(dir):
    curr_dir = os.getcwd()
    os.chdir(dir)

    res = subprocess.check_output("flint *.cpp *.h", stderr=subprocess.STDOUT, shell=True, universal_newlines=True)

    #cpp_files = glob.glob("*.cpp")
    #h_files = glob.glob("*.h")
    #files = " ".join(cpp_files) #+ " ".join(h_files)
    #out, err = util.run_cmd_throw("flint" + " " + files)
    os.chdir(curr_dir)
    #print(out)
    #print(err)
    return res


def run_flint_all():
    out1 = run_flint_in_dir(src_dir())
    out2 = run_flint_in_dir(src_utils_dir())
    return out1 + out2


def should_filter(s):
    to_filter = [
        "Advice: Prefer `nullptr' to `NULL' in new C++ code",
        "A symbol may not start with an underscore followed by a capital letter.",
        "Warning: Avoid using static at global or namespace scope in C++ header files.",
        "Using directive not allowed at top level or inside namespace",
        "The associated header file of .cpp files should be included before any other includes",
        "This helps catch missing header file dependencies in the .h",
        # VS 2010 doesn't seem to support 'explicit' on conversion operators
        "Implicit conversion to '",
        "Symbol __IDownloadManager_INTERFACE_DEFINED__"
    ]
    for f in to_filter:
        if f in s:
            return True
    valid_missing_inlude = ["Vec.h", "StrUtil.h", "StrFormat.h", "resource.h", "Allocator.h", "GeomUtil.h", "Scoped.h"]
    if "Missing include guard" in s:
        for v in valid_missing_inlude:
            if s.startswith(v):
                return True
    return False


def filter(out):
    lines = out.split("\n")
    warnings = []
    filtered = []
    for l in lines:
        if should_filter(l):
            filtered.append(l)
        else:
            warnings.append(l)
    return (warnings, filtered)

def main():
    verify_flint_installed()
    res = run_flint_all()
    (warnings, filtered) = filter(res)
    for w in warnings:
        print(w + "\n")
    print("%d warnings, %d filtered" % (len(warnings), len(filtered)))

if __name__ == "__main__":
    main()
