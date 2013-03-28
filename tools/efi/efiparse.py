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
		self.size = int(parts[2])
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
		self.size = int(parts[2])
		self.offset = int(parts[3])
		self.rva = int(parts[4])
		self.name = parts[5]
		# in case of C++ symbols full_name is undecorated_name. It might be
		# harder to read but unlike name, is unique
		self.full_name = self.name
		if len(parts) == 6:
			self.full_name = parts[5]

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
		l = self.fo.readline()
		if not l:
			return None
		l = l.rstrip()
		#print("'%s'" % l)
		return l

def parse_start(state):
	l = state.readline()
	assert l == "Format: 1", "unexpected line: '%s'" % l
	return parse_next_section

def parse_next_section(state):
	l = state.readline()
	#print("'%s'" % l)
	if l == None: return None
	if l == "": return parse_next_section
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
		# TODO: should parse structs, not just count them
		if l.startswith("struct"):
			state.types.append(Type(l))

def parse_file_object(fo):
	curr = parse_start
	state = ParseState(fo)
	while curr:
		curr = curr(state)
	print("%d types, %d sections, %d symbols" % (len(state.types), len(state.sections), len(state.symbols)))
	return state

def parse_file(file_name):
	print("parse_file: %s" % file_name)
	with open(file_name, "r") as fo:
		return parse_file_object(fo)

class Diff(object):
	def __init__(self):
		self.added = []
		self.removed = []
		self.changed = []

	def __repr__(self):
		s = "%d added\n%d removed\n%d changed" % (len(self.added), len(self.removed), len(self.changed))
		return s

# TODO: need add_sym and rem_sym because we have symbols with the same name
# Need to figure out why we don't always dump undecorated name
# (e.g. for RememberDefaultWindowPosition() in Sumatra)
def add_sym(symbols, sym):
	name = sym.full_name
	if name not in symbols:
		symbols[name] = (sym, 1)
	(sym, count) = symbols[name]
	symbols[name] = (sym, count+1)

def rem_sym(symbols, sym):
	name = sym.full_name
	if name not in symbols: return
	(sym, count) = symbols[name]
	if 1 == count:
		del symbols[name]
	else:
		symbols[name] = (sym, count-1)

def sym_changed(symbols, sym):
	name = sym.full_name
	(sym2, count) = symbols[name]
	if count != 1:
		# TODO: broken, we assume it didn't change while it might have
		return False
	return sym2.size != sym.size

def diff(parse1, parse2):
	assert isinstance(parse1, ParseState)
	assert isinstance(parse2, ParseState)
	symbols1 = {}
	for sym in parse1.symbols:
		add_sym(symbols1, sym)
	symbols2 = {}
	for sym in parse2.symbols:
		add_sym(symbols2, sym)

	added = []
	changed = []
	for sym in parse2.symbols:
		name = sym.full_name
		if name not in symbols1:
			added += [sym]
		else:
			if sym_changed(symbols1, sym):
				changed += [sym]
			# we remove those we've seen so that at the end the only symbols
			# left in symbols1 are those that were removed (i.e. not present in symbols2)
			rem_sym(symbols1, sym)

	removed = symbols1.values()
	diff = Diff()
	diff.added = added
	diff.removed = removed
	diff.changed = changed
	return diff

def main():
	parse_file(g_file_name)

if __name__ == "__main__":
	main()
