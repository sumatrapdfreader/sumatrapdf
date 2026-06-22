#!/usr/bin/env python3

import difflib
import os
import re
import shutil
import subprocess
import sys

os.environ["LC_ALL"] = "C"  # otherwise 'nm' prints in wrong order

srcdir = sys.argv[1]
base_srcdir = sys.argv[2]
builddir = sys.argv[3]

IGNORED_SYMBOLS = [
    "_fini",
    "_init",
    "_fdata",
    "_ftext",
    "_fbss",
    "__bss_start",
    "__bss_start__",
    "__bss_end__",
    "_edata",
    "_end",
    "_bss_end__",
    "__end__",
    "__gcov_.*",
    "llvm_.*",
    "flush_fn_list",
    "writeout_fn_list",
    "mangle_path",
    "lprofDirMode",
    "reset_fn_list",
]

# Rust
IGNORED_SYMBOLS += [
    "rust_eh_personality",
    "_.*3std9panicking11EMPTY_PANIC.*",  # 'std.*::panicking::EMPTY_PANIC::.*'
    "_.*3std3sys3pal4unix4args3imp15ARGV_INIT_ARRAY.*",  # 'std.*::sys::pal::unix::args::imp::ARGV_INIT_ARRAY::.*'
    "_.*3std3sys4args4unix3imp15ARGV_INIT_ARRAY.*",  # std.*::sys::args::unix::imp::ARGV_INIT_ARRAY::.*
    "_.*17compiler_builtins.*",  # 'compiler_builtins.*::.*'
    ".*__rustc.*",  # '.*__rustc.*' # Eg. _RNvCsgSLETaxrkfn_7___rustc17___rust_probestack
    '_hb_harfrust_.*_rs',
]

IGNORED_SYMBOLS = "|".join(IGNORED_SYMBOLS)

nm = os.getenv("NM", shutil.which("nm"))
if not nm:
    print("check-symbols.py: 'nm' not found; skipping test")
    sys.exit(77)

tested = False
stat = 0

for soname in [
    "harfbuzz",
    "harfbuzz-subset",
    "harfbuzz-icu",
    "harfbuzz-gobject",
    "harfbuzz-cairo",
]:
    for suffix in ["so", "dylib"]:
        so = os.path.join(builddir, "lib%s.%s" % (soname, suffix))
        if not os.path.exists(so):
            continue

        # On macOS, C symbols are prefixed with _
        symprefix = "_" if suffix == "dylib" else ""

        EXPORTED_SYMBOLS = [
            s.split()[2]
            for s in re.findall(
                r"^.+ [BCDGIRSTu] .+$",
                subprocess.check_output(nm.split() + [so]).decode("utf-8"),
                re.MULTILINE,
            )
            if not re.match(r".* %s(%s)\b" % (symprefix, IGNORED_SYMBOLS), s)
        ]

        prefix = (
            (symprefix + os.path.basename(so))
            .replace("libharfbuzz", "hb")
            .replace("-", "_")
            .split(".")[0]
        )

        print("Checking that %s does not expose internal symbols" % so)
        suspicious_symbols = [
            x for x in EXPORTED_SYMBOLS if not re.match(r"^%s(_|$)" % prefix, x)
        ]
        if suspicious_symbols:
            print("Ouch, internal symbols exposed:", suspicious_symbols)
            stat = 1

        def_path = os.path.join(builddir, soname + ".def")
        if not os.path.exists(def_path):
            print("'%s' not found; skipping" % def_path)
        else:
            print("Checking that %s has the same symbol list as %s" % (so, def_path))
            with open(def_path, "r", encoding="utf-8") as f:
                def_file = f.read()
            diff_result = list(
                difflib.context_diff(
                    def_file.splitlines(),
                    ["EXPORTS"]
                    + [re.sub("^%shb" % symprefix, "hb", x) for x in EXPORTED_SYMBOLS]
                    +
                    # cheat: copy the last line from the def file!
                    [def_file.splitlines()[-1]],
                )
            )

            if diff_result:
                print("\n".join(diff_result))
                stat = 1

            tested = True

if not tested:
    print("check-symbols.py: no shared libraries found; skipping test")
    sys.exit(77)

sys.exit(stat)
