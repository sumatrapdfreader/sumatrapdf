#!/usr/bin/env python

# The most time consuming part of writing cmake is writing the list of
# files to compile for each target.
# This script helps to automate that by extracting info from existing
# makefile.msvc files
#
# It's not perfect and re
import os

s_objs = """
	$(OT)\jfsseflt-64.obj $(OT)\jccolss2-64.obj $(OT)\jdcolss2-64.obj $(OT)\jcgrass2-64.obj \
	$(OT)\jcsamss2-64.obj $(OT)\jdsamss2-64.obj $(OT)\jdmerss2-64.obj $(OT)\jcqnts2i-64.obj  \
	$(OT)\jfss2fst-64.obj $(OT)\jfss2int-64.obj $(OT)\jiss2red-64.obj $(OT)\jiss2int-64.obj \
	$(OT)\jiss2fst-64.obj $(OT)\jcqnts2f-64.obj $(OT)\jiss2flt-64.obj $(OT)\jsimd_x86_64.obj
"""

s_objs2 = """
	$(OT)\jsimdcpu.obj $(OT)\jccolmmx.obj $(OT)\jcgrammx.obj $(OT)\jdcolmmx.obj \
	$(OT)\jcsammmx.obj $(OT)\jdsammmx.obj $(OT)\jdmermmx.obj $(OT)\jcqntmmx.obj \
	$(OT)\jfmmxfst.obj $(OT)\jfmmxint.obj $(OT)\jimmxred.obj $(OT)\jimmxint.obj \
	$(OT)\jimmxfst.obj $(OT)\jcqnt3dn.obj $(OT)\jf3dnflt.obj $(OT)\ji3dnflt.obj \
	$(OT)\jcqntsse.obj $(OT)\jfsseflt.obj $(OT)\jisseflt.obj $(OT)\jccolss2.obj \
	$(OT)\jcgrass2.obj $(OT)\jdcolss2.obj $(OT)\jcsamss2.obj $(OT)\jdsamss2.obj \
	$(OT)\jdmerss2.obj $(OT)\jcqnts2i.obj $(OT)\jfss2fst.obj $(OT)\jfss2int.obj \
	$(OT)\jiss2red.obj $(OT)\jiss2int.obj $(OT)\jiss2fst.obj $(OT)\jcqnts2f.obj \
	$(OT)\jiss2flt.obj $(OT)\jsimd_i386.obj
"""

# if true, paths generated to be relative to cmake directory, so that we
# can place CMakeLists*.txt inside cmake directory
dirs_relative_to_cmake = False


class DirInfo(object):
	def __init__(self, dir, files):
		self.dir = dir
		self.files = files


def strip_ext(file_name):
	return os.path.splitext(file_name)[0]


# returns dict mapping file name without extension to DirInfo
def build_files_map():
	res = {}
	for dir, dirnames, filenames in os.walk("ext"):
		#print("%s, %s, %s\n" % (dir, dirnames, filenames))
		di = DirInfo(dir, filenames)
		for f in filenames:
			res[strip_ext(f)] = di
	return res


# given $(OWP)\alpha.obj return alpha
def clean_obj_name(s):
	parts = s.split("\\")
	assert len(parts) == 2, "len(parts) != 2, parts: '%s'" % parts
	s = parts[-1]
	if s.endswith(".obj"):
		s = s[:-4]
	return s


def parse_objs(s):
	res = []
	for l in s.split("\n"):
		l = l.strip()
		parts = l.split()
		for p in parts:
			if p == "\\":
				continue
			p = clean_obj_name(p)
			res.append(p)
			#print(p)
	return res


def dump_defs(defs):
	for v in defs:
		#v = clean_obj_name(v)
		print("  %s" % v)


def get_dir_for_files(files, files_map):
	if len(files) == 0:
		return None
	f = files[0] # TODO: find the one with the most matches
	return files_map[f]


def get_dir_for_file(file, files_map):
	return files_map[file]


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


def gen_cmake(obj_names, files_map):
	lines = []
	for file_no_ext in obj_names:
		di = get_dir_for_file(file_no_ext, files_map)
		assert di is not None, "no dir for %s" % file_no_ext
		dir = fixup_dirname(di.dir)
		# add all .h files by default, using a single line, for shorter files
		#s = "%s/*.h" % dir
		#lines.append("\t" + quote(s))
		name_with_ext = find_compilable_for_name(file_no_ext, di.files)
		s = "%s/%s" % (dir, name_with_ext)
		lines.append("\t" + quote(s))
	lines.append("\t)\n")
	return "\n".join(lines)


def main():
	files_map = build_files_map()
	obj_names = parse_objs(s_objs2)
	dump_defs(obj_names)
	s = gen_cmake(obj_names, files_map)
	print(s)


if __name__ == "__main__":
	main()
