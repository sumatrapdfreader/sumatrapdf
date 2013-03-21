#!/usr/bin/env python

"""
Parses the output of sizer.exe.
Shows a summary or a comparison.

TODO:
 - parse compact mode
"""

g_file_name = "sum_sizer.txt"

# type | sectionNo | length | offset | objFileId
# C|1|35|0|C:\Users\kkowalczyk\src\sumatrapdf\obj-dbg\sumatrapdf\SumatraPDF.obj
class Section(object):
	def __init__(self, l):
		parts = l.split("|")
		assert len(parts) == 5
		self.type = parts[0]
		self.section_no = int(parts[1])
		self.len = int(parts[2])
		self.offset = int(parts[3])
		# it's either name id in compact mode or full name
		self.name = parts[4]

# type | section | length | offset | rva | name
# or:
# type | section | length | offset | rva | name | undecoratedName
# Function|1|35|0|4096|AllocArray<wchar_t>|wchar_t*__cdeclAllocArray<wchar_t>(unsignedint)
class Symbol(object):
	def __init__(self, l):
		parts = l.split("|")
		assert len(parts) in (6, 7), "len(parts) is %d\n'%s'" % (len(parts), l)
		self.type = parts[0]
		self.section = int(parts[1])
		self.len = int(parts[2])
		self.offset = int(parts[3])
		self.rva = int(parts[4])
		self.name = parts[5]
		self.undecorated_name = None
		if len(parts) == 6:
			self.undecorated_name = parts[5]

class Type(object):
	def __init__(self, l):
		# TODO: parse the line
		self.line = l

class ParseState(object):
	def __init__(self, fo):
		self.fo = fo
		self.types = []
		self.symbols = []
		self.sections = []

	def readline(self):
		return self.fo.readline().strip()

def parse_start(state):
	l = state.readline()
	assert l == "Format: 1", "unexpected line: %s" % l
	return parse_next_section

def parse_next_section(state):
	l = state.readline()
	if l == None: return None
	if l == "": return None # TODO: why ?
	if l == "Types:":
		return parse_types
	if l == "Sections:":
		return parse_sections
	if l == "Symbols:":
		return parse_symbols
	print("Unkonw section: '%s'" % l)
	return None

def parse_sections(state):
	while True:
		l = state.readline()
		if l == None: return None
		if l == "": return parse_next_section
		state.sections.append(Section(l))

def parse_symbols(state):
	while True:
		l = state.readline()
		if l == None: return None
		if l == "": return parse_next_section
		state.symbols.append(Symbol(l))

def parse_types(state):
	while True:
		l = state.readline()
		if l == None: return None
		if l == "": return parse_next_section
		# TODO: should actually parse structs, not just count them
		if l.startswith("struct"):
			state.types.append(Type(l))

def parse_file_object(fo):
	curr = parse_start
	state = ParseState(fo)
	while curr:
		curr = curr(state)
	print("%d types, %d sections, %d symbols" % (len(state.types), len(state.sections), len(state.symbols)))

def parse_file(file_name):
	with open(file_name, "r") as fo:
		return parse_file_object(fo)

def main():
	parse_file(g_file_name)

if __name__ == "__main__":
	main()
