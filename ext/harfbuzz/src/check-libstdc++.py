#!/usr/bin/env python3

import sys, os, shutil, subprocess

os.chdir (os.getenv ('srcdir', os.path.dirname (__file__)))

libs = os.getenv ('libs', '.libs')

ldd = os.getenv ('LDD', shutil.which ('ldd'))
if not ldd:
	otool = os.getenv ('OTOOL', shutil.which ('otool'))
	if otool:
		ldd = otool + ' -L'
	else:
		print ('check-libstdc++.py: \'ldd\' not found; skipping test')
		sys.exit (77)

stat = 0
tested = False

# harfbuzz-icu links to libstdc++ because icu does.
for soname in ['harfbuzz', 'harfbuzz-subset', 'harfbuzz-gobject']:
	for suffix in ['so', 'dylib']:
		so = os.path.join (libs, 'lib%s.%s' % (soname, suffix))
		if not os.path.exists (so): continue

		print ('Checking that we are not linking to libstdc++ or libc++ in %s' % so)
		ldd_result = subprocess.check_output (ldd.split() + [so])
		if (b'libstdc++' in ldd_result) or (b'libc++' in ldd_result):
			print ('Ouch, %s is linked to libstdc++ or libc++' % so)
			stat = 1

		tested = True

if not tested:
	print ('check-libstdc++.py: libharfbuzz shared library not found; skipping test')
	sys.exit (77)

sys.exit (stat)
