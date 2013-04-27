#!/usr/bin/env python

"""
This de-duplicates callstacks generated via SaveCallstackLogs() in Sumatra.
This is a debugging aid.

The process is:
- call dbghelp::RememberCallstackLogs() at the beginning of WinMain
- add dbghelp::LogCallstack() to functions you want to instrument
- call SaveCallstackLogs() to save callstacks to callstacks.txt file in the
  same directory where SumatraPDF.exe lives. Presumably, it'll be obj-dbg
- run this script to generated callstacks_2.txt, which collapses the same
  callstacks

This was written to help track down AddRef()/Release() mismatch, where
lots of calls make analyzing the raw output difficult. It might be applicable
in other scenarios.
"""

import os, sys

# if True, will indent callstacks with Release() with spaces, to make
# it easier to tell them from AddRef callstacks
g_indent_release = True

g_scripts_dir = os.path.dirname(os.path.realpath(__file__))

def top_dir(): return os.path.dirname(g_scripts_dir)

def verify_file_exists(path):
	if not os.path.exists(path):
		print("file %s doesn't exist" % path)
		sys.exit(1)

"""
Turns:
00FD0389 01:0013F389 sumatrapdf.exe!dbghelp::LogCallstack+0x39 c:\users\kkowalczyk\src\sumatrapdf\src\utils\dbghelpdyn.cpp+497
into:
sumatrapdf.exe!dbghelp::LogCallstack+0x39 c:\users\kkowalczyk\src\sumatrapdf\src\utils\dbghelpdyn.cpp+497
"""
def shorten_cs_line(s):
	parts = s.split(" ", 3)
	return parts[2]

def iter_callstacks(file_path):
	curr = []
	for l in open(file_path, "r"):
		l = l.strip()
		if 0 == len(l):
			if len(curr) > 0:
				yield curr
				curr = []
		else:
			l = shorten_cs_line(l)
			# omit first if it's sumatrapdf.exe!dbghelp::LogCallstack
			if 0 == len(curr) and "dbghelp::LogCallstack" in l:
				continue
			curr.append(l)
	if len(curr) > 0:
		yield curr

def is_release(txt):
	lines = txt.split("\n")
	return len(lines) > 0 and "::Release+" in lines[0]

def fmt_release(txt):
	lines = txt.split("\n")
	return "    " + "\n    ".join(lines)

class CallStack(object):
	def __init__(self, txt):
		self.txt = txt
		self.count = 1

def cs_add_or_inc_count(callstacks, txt):
	for cs in callstacks:
		if cs.txt == txt:
			cs.count += 1
			return
	callstacks.append(CallStack(txt))

def parse_callstacks(file_path):
	callstacks = []
	for cs_lines in iter_callstacks(file_path):
		txt = "\n".join(cs_lines)
		cs_add_or_inc_count(callstacks, txt)
	return callstacks

def save_callstacks(file_path, callstacks):
	fo = open(file_path, "w")
	for cs in callstacks:
		fo.write("count: %d\n" % cs.count)
		txt = cs.txt
		if g_indent_release and is_release(txt):
			txt = fmt_release(txt)
		fo.write(txt + "\n\n")
	fo.close()

def main():
	file_name = os.path.join(top_dir(), "obj-dbg", "callstacks.txt")
	if len(sys.argv) > 1:
		file_name = sys.argv[1]
	verify_file_exists(file_name)
	callstacks = parse_callstacks(file_name)
	dst_name = file_name.replace(".txt", "_2.txt")
	save_callstacks(dst_name, callstacks)
	total = 0
	for cs in callstacks:
		total += cs.count
	print("Collapsed %d callstacks into %d unique" % (total, len(callstacks)))

if __name__ == "__main__":
	main()
