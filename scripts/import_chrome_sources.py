#!/usr/bin/env python

"""
Copy source files from Chrome sources to ext/chrome directory.
We need a script because I don't want to copy everything (it's a lot)
and Chrome files are a maze of dependencies so doing it manually is tedious.
"""

import os, os.path, sys, re, shutil, string
from chrome_makefile_tmpl import *

pj = os.path.join

def verify_path_exists(p):
	if not os.path.exists(p):
		print("Path %s doesn't exists" % p)
		sys.exit(1)

def ensure_dir(p):
    if not os.path.exists(p):
        os.makedirs(p)

def ensure_dir_for_file(f):
    d = os.path.dirname(f)
    if not os.path.exists(d):
        os.makedirs(d)

gRejectedIncludes = {}

def is_h_file(fp): return fp.endswith(".h")

# try to reject includes that are not in chrome source tree,
# like system or stl includes
def is_chrome_include(fp):
	s = fp
	d = ""
	while True:
		(h,d) = os.path.split(s)
		if "" == h:
			break
		s = h
	if d in ["base", "build", "testing"]: return True
	gRejectedIncludes[fp] = True
	return False

def extract_includes(fp):
	content = open(fp, "r").read()
	includes = re.findall(r'(?m)^#include ["<]([^">]+)[">]', content)
	includes = [el for el in includes if is_chrome_include(el)]
	print("Includes in %s: %s" % (fp, includes))
	return includes

# some files shouldn't be copied even if our heuristic flags it as needed
def is_cpp_blaclisted(fp):
	for s in ["file_descriptor_shuffle.cc", "string16.cc", "dtoa.cc"]:
		if fp.endswith(s): return True
	return False

# see if we have foo.cc matching foo.h
def matching_cpp_file(dir, fp):
	if not is_h_file(fp): return None
	fname = fp[:-2] + ".cc"
	s = pj(dir, fname)
	if os.path.exists(s) and not is_cpp_blaclisted(s):
		print("Found %s matching %s" % (fname, fp))
		return fname
	return None

# see if we have foo_win.cc matching foo.h
def matching_cpp_win_file(dir, fp):
	if not is_h_file(fp): return None
	fname = fp[:-2] + "_win.cc"
	s = pj(dir, fname)
	if os.path.exists(s):
		print("Found %s matching %s" % (fname, fp))
		return fname
	return None

# find .cc files matching .h files e.g. foo.cc matches foo.h
# (assuming it exists)
def get_cpp_matching_h(dir, files):
	additional = []
	for f in files:
		cpp = matching_cpp_file(dir, f)
		if cpp: additional.append(cpp)
		cpp = matching_cpp_win_file(dir, f)
		if cpp: additional.append(cpp)
	if len(additional) > 0: print(additional)
	return additional

# we just blindly extract #include strings so we might get some that
# are under #ifdef for non-windows platforms. luckily, chrome has strong
# naming conventions where we can detect platform-specific files by
# looking at file names
def not_win_file(f):
	for s in ["unittest", "stub", "posix", "android", "glib", "gtk", "mac", "wayland", "x", "freebsd", "linux", "nacl", "openbsd", "solaris", "libevent", "gcc", "macosx"]:
		if "_" + s + "." in f: return True
	if pj("base", "mac") in f: return True
	if pj("base", "nix") in f: return True
	if pj("base", "android") in f: return True
	return False

def copy_files(srcDir, dstDir, files):
	for f in files:
		src = pj(srcDir, f)
		dst = pj(dstDir, f)
		ensure_dir_for_file(dst)
		shutil.copy(src,dst)
		# 644 - user can read/write, group and other can read
		#m = stat.S_IROTH | stat.S_IRGRP | stat.S_IRUSR | stat.S_IWUSR
		#os.chmod(dst, m)

def get_src_dir():
	if os.path.exists(r"z:\src\chrome\src"):
		srcDir = r"z:\src\chrome\src"
	elif os.path.exists("build-pre-release.bat"):
		srcDir = pj("..", "..", "chrome", "src")
	elif os.path.exists("scripts"):
		srcDir = pj("..", "chrome", "src")
	else:
		print("Current directory is not sumatra's dir or script dir")
		sys.exit(1)
	verify_path_exists(pj(srcDir, "base"))
	return srcDir

def get_dst_dir():
	if os.path.exists("build-pre-release.bat"):
		dstDir = pj("..", "ext")
	elif os.path.exists("scripts"):
		dstDir = "ext"
	else:
		print("Current directory is not sumatra's dir or script dir")
		sys.exit(1)
	verify_path_exists(pj(dstDir, "freetype2"))
	return pj(dstDir, "chrome")

def unique_dirs(files):
	dirs = {}
	for f in files:
		(d, f) = os.path.split(f)
		if d not in dirs:
			dirs[d] = True
	return dirs.keys()

def get_license_files(srcDir, files):
	dirs = unique_dirs(files)
	license_files = []
	for d in dirs:
		fp = pj(d, "LICENSE")
		if os.path.exists(pj(srcDir, fp)):
			license_files.append(fp)
	return license_files

def cpp_rules(dirs):
	rules = [DIR_TMPL % d for d in dirs]
	return string.join(rules, "")

def chrome_inc():
	return "/I$(EXTDIR)\\chrome /I$(EXTDIR)\\chrome\\base\\third_party\\valgrind/"

def objname_from_cname(s):
	if s.endswith(".c"): return s[:-1] + "obj"
	if s.endswith(".cc"): return s[:-2] + "obj"
	print("%s is not .c or .cc file!!!" % s)
	sys.exit(1)

def write_makefile(dstDir, files):
	makefile_filename = pj(dstDir, "makefile.msvc")
	cpp_files = [f.replace("/", "\\") for f in files if f.endswith(".cc") or f.endswith(".c")]
	dirs = unique_dirs(cpp_files)
	cpp_files = [os.path.basename(f) for f in cpp_files]
 	# a special case, dtoa.cc is #included in dtoa_wrapper.cc, so we need to copy
 	# it but not compile it
 	cpp_files.remove("dtoa.cc")
 	cpp_files = list(set(cpp_files)) # remove duplicates
 	cpp_files.sort()

	objs = ["$(OCH)\\%s" % objname_from_cname(f) for f in cpp_files]
	objs_grouped = []
	while len(objs) > 0:
		part = objs[:2]
		objs_grouped.append(string.join(part, " "))
		objs = objs[2:]
	objs = string.join(objs_grouped, " \\\n\t")
	rules = cpp_rules(dirs)
	# all rules are for .cc files, this is a single exception
	rules += """
{$(EXTDIR)\\chrome\\base\\third_party\\dynamic_annotations}.c{$(OCH)}.obj::
	$(CC) $(CHROME_CFLAGS) /Fo$(OCH)\\ /Fd$(O)\\vc80.pdb $<
	"""
	inc = chrome_inc()
	s = MAKEFILE_TMPL % (inc, objs, rules)
	print(s)
	#print(dirs)
	open(makefile_filename, "wb").write(s.replace("\n", "\r\n"))

def main():
	srcDir = get_src_dir()
	dstDir = get_dst_dir()

	ensure_dir(dstDir)
	bootstrapFiles = [
		pj("base", "bind.h"),
		pj("base", "threading", "thread.cc"),
		pj("base", "third_party", "dmg_fp", "dtoa_wrapper.cc"),
		pj("base", "third_party", "dmg_fp", "g_fmt.cc"),
		pj("base", "third_party", "valgrind", "valgrind.h"),
		pj("base", "third_party", "dynamic_annotations", "dynamic_annotations.c")]
	for f in bootstrapFiles: verify_path_exists(pj(srcDir, f))
	visited = {}
	toVisit = [el for el in bootstrapFiles]
	while len(toVisit) > 0:
		f = toVisit.pop(0)
		if f in visited or not_win_file(f): continue
		newToVisit = extract_includes(pj(srcDir, f))
		matchingCpp = get_cpp_matching_h(srcDir, newToVisit)
		toVisit.extend(newToVisit)
		toVisit.extend(matchingCpp)
		visited[f] = True

	print("Rejected includes: %s" % gRejectedIncludes.keys())
	files = visited.keys()
	files.sort()
	write_makefile(dstDir, files)
	files.extend(get_license_files(srcDir, files))
	files.extend(["AUTHORS", "LICENSE"]) # known files to copy
	copy_files(srcDir, dstDir, files)
	#for f in files: print(f)

if __name__ == "__main__":
	main()
