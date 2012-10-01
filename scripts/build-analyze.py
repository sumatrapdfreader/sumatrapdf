import os, os.path, shutil, sys, time, re, string, datetime, json
from util import log, run_cmd_throw, run_cmd
from util import verify_started_in_right_directory
from buildbot import extract_analyze_errors, strip_empty_lines

def sort_errors(lines, rel_path_start):
	if len(lines) == 0: return ([], [], [])
	sumatra_errors = []
	mupdf_errors = []
	ext_errors = []
	for l in lines:
		l = l[rel_path_start:]
		if l.startswith("src\\"):     sumatra_errors.append(l)
		elif l.startswith("mupdf\\"): mupdf_errors.append(l)
		elif l.startswith("ext\\"):   ext_errors.append(l)
		else: assert(False)
	return (sumatra_errors, mupdf_errors, ext_errors)

def print_lines(lines):
	for l in lines: print("%s\n" % l)

def pretty_print_errors(s):
	lines = extract_analyze_errors(s)
	(sumatra, mupdf, ext) = sort_errors(lines, len(os.getcwd()) + 1)
	print("******** EXT warnings\n")
	print_lines(ext)
	print("******** MUPDF warnings\n")
	print_lines(mupdf)
	print("******** SUMATRA warnings\n")
	print_lines(sumatra)

def main():
	verify_started_in_right_directory()
	config = "CFG=rel"
	obj_dir = "obj-rel"
	ver = "1000" # doesn't matter what version we claim
	extcflags = "EXTCFLAGS=-DSVN_PRE_RELEASE_VER=%s" % ver
	platform = "PLATFORM=X86"
	shutil.rmtree(obj_dir, ignore_errors=True)
	shutil.rmtree(os.path.join("mupdf", "generated"), ignore_errors=True)
	(out, err, errcode) = run_cmd("nmake", "-f", "makefile.msvc", "WITH_ANALYZE=yes", config, extcflags, platform, "all_sumatrapdf")
	pretty_print_errors(out)

if __name__ == "__main__":
	#main()
	test_anal_temp()