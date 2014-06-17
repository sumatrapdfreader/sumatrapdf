#!/usr/bin/env python

# The most time consuming part of writing cmake is writing the list of
# files to compile for each target.
# This script helps to automate that by extracting info from existing
# makefile.msvc files
#
# TODO:
#  - find directory for each list of objects
#  - produce the cmake formatted output

import os

# returns dict mapping string defining a target (e.g. UNRAR_OBJS) to a list
# of source (.c, .cpp, .asm) files (full paths)
def parse_makefile(file_path):
	in_obj_list = False
	res = {}
	curr_name = None
	curr_objs = None
	for l in open(file_path).readlines():
		l = l.strip()
		if in_obj_list:
			parts = l.split()
			for p in parts:
				 if p != "\\":
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


def parse_makefile_in_dir(dir):
	return parse_makefile(os.path.join(dir, "makefile.msvc"))


# given $(OWP)\alpha.obj return alpha
def clean_obj_name(s):
	parts = s.split("\\")
	s = parts[-1]
	if s.endswith(".obj"):
		s = s[:-4]
	return s


def dump_defs(defs):
	for name in defs.keys():
		print("\n%s" % name)
		for v in defs[name]:
			v = clean_obj_name(v)
			print("  %s" % v)


def main():
	in_ext = parse_makefile_in_dir("ext")
	dump_defs(in_ext)


if __name__ == "__main__":
	main()
