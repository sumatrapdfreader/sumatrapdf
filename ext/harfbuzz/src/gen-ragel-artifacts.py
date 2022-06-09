#!/usr/bin/env python3

"This tool is intended to be used from meson"

import os, os.path, sys, subprocess, shutil

ragel = sys.argv[1]
if not ragel:
	sys.exit ('You have to install ragel if you are going to develop HarfBuzz itself')

if len (sys.argv) < 4:
	sys.exit (__doc__)

OUTPUT = sys.argv[2]
CURRENT_SOURCE_DIR = sys.argv[3]
INPUT = sys.argv[4]

outdir = os.path.dirname (OUTPUT)
shutil.copy (INPUT, outdir)
rl = os.path.basename (INPUT)
hh = rl.replace ('.rl', '.hh')
subprocess.Popen (ragel.split() + ['-e', '-F1', '-o', hh, rl], cwd=outdir).wait ()

# copy it also to src/
shutil.copyfile (os.path.join (outdir, hh), os.path.join (CURRENT_SOURCE_DIR, hh))
