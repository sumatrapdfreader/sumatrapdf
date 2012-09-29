#!/usr/bin/python

"""Copy files from source directory to a destination directory,
naming the files as sha1 of their content, but only if the file
doesn't already exist in destination directory.
Deletes the source file if -delete flag is given"""

import sys, os, os.path, shutil
from util import test_for_flag, file_sha1

pj = os.path.join

def usage():
	print("Usage: archivefiles.py srcDir dstDir [-move]")
	sys.exit(1)

def is_cygwin(): return os.name == "posix"

def default_dst_dir():
	if is_cygwin():
		return "/cygdrive/z/sumdocs"
	else:
		return "z:\\cygdrive\\sumdocs"

def exit_if_path_not_exists(p):
	if not os.path.exists(p):
		print("%s path doesn't exist" % p)
		sys.exit(1)

def is_pdf(s): return s.lower().endswith(".pdf")
def is_djvu(s): return s.lower().endswith(".djvu")
def is_chm(s): return s.lower().endswith(".chm")
def is_xps(s): return s.lower().endswith(".xps")
def is_epub(s):
	if s.lower().endswith(".epub"): return True
	return False
def is_mobi(s):
	if s.lower().endswith(".mobi"): return True
	return False

def copy_file(srcDir, dstDir, moveFiles, fileName, dstDir2):
	srcPath = pj(srcDir, fileName)
	sha = file_sha1(srcPath)
	ext = os.path.splitext(fileName)[1].lower()
	dstPath = pj(dstDir, dstDir2, sha + ext)
	print("Copying %s to %s" % (srcPath, dstPath))
	if os.path.exists(dstPath):
		print("  %s already exists" % dstPath)
	else:
		shutil.copyfile(srcPath, dstPath)
	if moveFiles:
		print("  deleting %s" % srcPath)
		os.remove(srcPath)
	return sha

# TODO: if there is a .jpg|.jpeg|.opf file with the same basename
# as fileName, copy it using the sha name as for the .mobi file
def copy_mobi_file(srcDir, dstDir, moveFiles, fileName):
	copy_file(srcDir, dstDir, moveFiles, f, "mobi")

def copy_files(srcDir, dstDir, moveFiles):
	print("Copy files from %s to %s, move: %s" % (srcDir, dstDir, moveFiles))
	files = os.listdir(srcDir)
	for f in files:
		if is_pdf(f):  copy_file(srcDir, dstDir, moveFiles, f, "pdf")
		if is_djvu(f): copy_file(srcDir, dstDir, moveFiles, f, "djvu")
		if is_chm(f):  copy_file(srcDir, dstDir, moveFiles, f, "chm")
		if is_xps(f):  copy_file(srcDir, dstDir, moveFiles, f, "xps")
		if is_epub(f): copy_file(srcDir, dstDir, moveFiles, f, "epub")
		if is_mobi(f): copy_mobi_file(srcDir, dstDir, moveFiles, f)

def verify_dirs(srcDir, dstDir):
	exit_if_path_not_exists(srcDir)
	exit_if_path_not_exists(dstDir)

	for d in ["pdf", "djvu", "chm", "xps", "epub", "mobi"]:
		exit_if_path_not_exists(pj(dstDir, d))

def main():
	args = sys.argv[1:]
   	moveFiles = test_for_flag(args, "-move")
   	if len(args) not in [1,2]: usage()
   	srcDir = args[0]
   	if len(args) == 2:
	   	dstDir = args[1]
	else:
		dstDir = default_dst_dir()

	verify_dirs(srcDir, dstDir)
	copy_files(srcDir, dstDir, moveFiles)

if __name__ == "__main__":
    main()
