#!/usr/bin/env python

# The most time consuming part of writing cmake is writing the list of
# files to compile for each target.
# This script helps to automate that by extracting info from existing
# makefile.msvc files
#
# It's not perfect and re
import os


# if true, paths generated to be relative to cmake directory, so that we
# can place CMakeLists*.txt inside cmake directory
dirs_relative_to_cmake = True


class DirInfo(object):
	def __init__(self, dir, files):
		self.dir = dir
		self.files = files


def strip_ext(file_name):
	return os.path.splitext(file_name)[0]


# returns dict mapping file name without extension to DirInfo
def build_files_map():
	res = {}
	for dir, dirnames, filenames in os.walk("."):
		#print("%s, %s, %s\n" % (dir, dirnames, filenames))
		di = DirInfo(dir, filenames)
		for f in filenames:
			res[strip_ext(f)] = di
	return res


# given $(OWP)\alpha.obj return alpha
def clean_obj_name(s):
	parts = s.split("\\")
	if len(parts) == 1:
		return None
	assert len(parts) == 2, "len(parts) != 2, parts: '%s'" % parts
	s = parts[-1]
	if s.endswith(".obj"):
		s = s[:-4]
	if s.endswith(".exe"): # mudraw.exe
		s = s[:-4]
	return s


# returns dict mapping string defining a target (e.g. UNRAR_OBJS) to a list
# of source (.c, .cpp, .asm) files (full paths)
def parse_makefile(file_path, res):
	in_obj_list = False
	curr_name = None
	curr_objs = None
	for l in open(file_path).readlines():
		l = l.strip()
		if in_obj_list:
			parts = l.split()
			for p in parts:
				 if p != "\\":
				 	p = clean_obj_name(p)
				 	if p != None:
					 	curr_objs.append(p)
				 	#print(p)
			if len(parts) > 0 and parts[-1] != "\\":
				in_obj_list = False
				res[curr_name] = curr_objs
				curr_name = None
				curr_objs = None
				#print("")
		elif "_OBJS =" in l:
			parts = l.split()
			name = parts[0]
			#print("'%s'" % name)
			in_obj_list = True
			curr_name = name
			curr_objs = []
	return res


def parse_makefile_in_dir(dir, defs):
	return parse_makefile(os.path.join(dir, "makefile.msvc"), defs)


def dump_defs(defs):
	for name in defs.keys():
		print("\n%s" % name)
		for v in defs[name]:
			v = clean_obj_name(v)
			print("  %s" % v)


def get_dir_for_files(files, files_map):
	if len(files) == 0:
		return None
	f = files[0] # TODO: find the one with the most matches
	return files_map[f]


# file_name is name without extenstion. files is a list of files in a directory
# find a compilable file (.c, .cpp, .asm) matching file_name
def find_compilable_for_name(file_name, files):
	for f in files:
		base, ext = os.path.splitext(f)
		if base == file_name and ext in [".cpp", ".cxx", ".c", ".asm"]:
			return f
	return None


def quote(s):
	return '"' + s + '"'


def fixup_dirname(s):
	if s.startswith("./"):
		s = s[2:]
		if dirs_relative_to_cmake:
			s = "../" + s
	return s


def gen_cmake(defs, files_map):
	lines = []
	for name in defs.keys():
		if name in ["UNINSTALLER_OBJS"]:
			continue
		files = defs[name]
		name = name.replace("_OBJS", "_SRC")
		lines.append("file (GLOB %s" % name)
		di = get_dir_for_files(files, files_map)
		if di == None:
			print("no dir for %s" % name)
			continue
		# add all .h files by default, using a single line, for shorter files
		dir = fixup_dirname(di.dir)
		s = "%s/*.h" % dir
		lines.append("\t" + quote(s))
		for f in files:
			name_with_ext = find_compilable_for_name(f, di.files)
			s = "%s/%s" % (dir, name_with_ext)
			lines.append("\t" + quote(s))
		lines.append("\t)\n")
	return "\n".join(lines)


def main():
	files_map = build_files_map()
	defs = parse_makefile_in_dir("mupdf", {})
	defs = parse_makefile_in_dir("ext", defs)
	defs = parse_makefile_in_dir(".", defs)
	#dump_defs(in_ext)
	s = gen_cmake(defs, files_map)
	print(s)


if __name__ == "__main__":
	main()
