#!/usr/bin/env python

"""
Usage: efi_cmp.py $svn_ver1 $svn_ver2

Builds release build of both version. Uses efi.exe to dump symbol information
for each version and shows differences.

Requires:
 - efi.exe to be in %PATH%
 - ../sumatrapdf_efi directory with Sumatra's svn checkout to exist.

It caches build artifacts (.exe, .pdb etc.) in ../sumatrapdf_efi/efi_cache
directory. You might occasionally delete it, if disk space is a concern.
"""

"""
TODO:
 - diff for symbols in text format
 - summary of biggest functions
 - summary of
 - upload efi results as part of buildbot
 - diff for symbols in html format
 - upload efi html diff as part of buildbot
"""

import sys, os
# assumes is being run as ./scripts/efi_cmp.py
efi_scripts_dir = os.path.join("tools", "efi")
sys.path.append(efi_scripts_dir)
import os, sys, shutil, util, efiparse

g_top_dir = os.path.realpath("..")

def sum_efi_dir():
	return os.path.join(g_top_dir, "sumatrapdf_efi")

def sum_efi_cache_dir(ver):
	# make it outside of sumatrapdf_efi directory?
	d = os.path.join(sum_efi_dir(), "efi_cache", str(ver))
	return util.create_dir(d)

def efi_result_file(ver):
	return os.path.join(sum_efi_cache_dir(ver), "efi.txt")

def usage():
	name = os.path.basename(__file__)
	print("Usage: %s $svn_ver1 $svn_ver2" % name)
	sys.exit(1)

def verify_efi_present():
	try:
		(out, err, errcode) = util.run_cmd("efi.exe")
	except:
		print("Must have efi.exe in the %PATH%!!!")
		sys.exit(1)
	if "Usage:" not in out:
		print("efi.exe created unexpected output:\n%s" % out)
		sys.exit(1)

g_build_artifacts = ["SumatraPDF.exe", "SumatraPDF.pdb"]
def already_built(ver):
	d = sum_efi_cache_dir(ver)
	for f in g_build_artifacts:
		p = os.path.join(d, f)
		if not os.path.exists(p): return False
	return True

def build_ver(ver):
	print("Building release version %d" % ver)
	config = "CFG=rel"
	obj_dir = "obj-rel"
	extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
	platform = "PLATFORM=X86"

	if already_built(ver):
		print("Version %d already built!" % ver)
		return
	os.chdir(sum_efi_dir())
	util.run_cmd_throw("svn", "update", "-r%d" % ver)

	shutil.rmtree(obj_dir, ignore_errors=True)
	shutil.rmtree(os.path.join("mupdf", "generated"), ignore_errors=True)
	(out, err, errcode) = util.run_cmd("nmake", "-f", "makefile.msvc", config, extcflags, platform, "all_sumatrapdf")
	for f in g_build_artifacts:
		src = os.path.join(obj_dir, f)
		dst = os.path.join(sum_efi_cache_dir(ver), f)
		shutil.copyfile(src, dst)

def build_efi_result(ver):
	path = efi_result_file(ver)
	if os.path.exists(path): return # was already done
	os.chdir(sum_efi_cache_dir(ver))
	util.run_cmd_throw("efi", "SumatraPDF.exe", ">efi.txt")

def main():
	# early checks
	assert os.path.exists(sum_efi_dir()), "Need %s directory" % sum_efi_dir()
	verify_efi_present()
	if len(sys.argv) != 3:
		usage()
	svn_ver1 = int(sys.argv[1])
	svn_ver2 = int(sys.argv[2])
	if svn_ver1 == svn_ver2:
		print("Versions have to be different!")
		usage()
	print("Comparing %d to %d" % (svn_ver1, svn_ver2))
	build_ver(svn_ver1)
	build_efi_result(svn_ver1)
	build_ver(svn_ver2)
	build_efi_result(svn_ver2)

	efi1 = efiparse.parse_file(efi_result_file(svn_ver1))
	efi2 = efiparse.parse_file(efi_result_file(svn_ver2))
	diff = efiparse.diff(efi1, efi2)
	#print("Diffing done")
	print(diff)
	diff.added.sort(key=lambda sym: sym.size, reverse=True)
	diff.removed.sort(key=lambda sym: sym.size, reverse=True)
	diff.changed.sort(key=lambda sym: sym.size_diff, reverse=True)

	max = 5
	added = diff.added
	if len(added) > 0:
		print("\nAdded symbols:")
		for sym in added[:max]:
			#sym = diff.syms2.name_to_sym[sym_name]
			size = sym.size
			print("%4d : %s" % (size, sym.name))

	removed = diff.removed
	if len(removed) > 0:
		print("\nAdded symbols:")
		for sym in removed[:max]:
			#sym = diff.syms2.name_to_sym[sym_name]
			size = sym.size
			print("%4d : %s" % (size, sym.name))

	changed = diff.changed
	if len(changed) > 0:
		changed = diff.changed[:20]
		print("\nChanged symbols:")
		for sym in changed:
			size = sym.size_diff
			print("%4d : %s" % (size, sym.name))

if __name__ == "__main__":
	main()
