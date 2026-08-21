#!/usr/bin/env python

"""
Parses the output of efi.exe.

TODO:
 - do a per .obj file string size changes
"""

import bz2, bisect

g_file_name = "efi.txt"

(SECTION_CODE, SECTION_DATA, SECTION_BSS, SECTION_UNKNOWN) = ("C", "D", "B", "U")

# maps a numeric string idx to string. We take advantage of the fact that
# strings in efi.exe output are stored with consequitive indexes
class Strings():
	def __init__(self):
		self.strings = []

	def add(self, idx, str):
		assert idx == len(self.strings)
		self.strings.append(str)

	def idx_to_str(self, idx):
		return self.strings[idx]

# type | sectionNo | length | offset | objFileId
# C|1|35|0|C:\Users\kkowalczyk\src\sumatrapdf\obj-dbg\sumatrapdf\SumatraPDF.obj
class Section(object):
	def __init__(self, l, strings):
		parts = l.split("|")
		assert len(parts) == 5
		self.type = parts[0]
		self.section_no = int(parts[1])
		self.size = int(parts[2])
		self.offset = int(parts[3])
		idx = int(parts[4])
		self.name = strings.idx_to_str(idx)

def print_i_off_sec(i, off, section):
	print("""i: %d
off:            %d
section.offset: %d
""" % (i, off, section.offset))

class SectionsSorted(object):
	def __init__(self):
		self.offsets = []
		self.sections = []

	def add(self, section):
		prev_sec_idx = len(self.offsets) - 1
		self.offsets.append(section.offset)
		self.sections.append(section)
		if prev_sec_idx > 1:
			prev_off = self.offsets[prev_sec_idx]
			assert prev_off <= section.offset

	def objname_by_offset(self, off):
		i = bisect.bisect_left(self.offsets, off)
		if i >= len(self.sections):
			i = len(self.sections) - 1
		section = self.sections[i]
		if off < section.offset:
			try:
				assert i > 0
			except:
				print_i_off_sec(i, off, section)
				raise
			i -= 1
			section = self.sections[i]
		try:
			assert off >= section.offset
		except:
			print_i_off_sec(i, off, section)
			raise
		if len(self.sections) < i + 1:
			next_section = self.sections[i+1]
			assert off < next_section.offset
		return section.name

class SectionToObjFile(object):
	def __init__(self, sections, strings):
		self.strings = strings

		sec_no_to_sec = {}
		curr_sec_no = -1
		curr_sec_sorted = None
		for s in sections:
			if s.section_no != curr_sec_no:
				assert s.section_no not in sec_no_to_sec
				assert s.section_no > curr_sec_no
				curr_sec_no = s.section_no
				curr_sec_sorted = SectionsSorted()
				sec_no_to_sec[curr_sec_no] = curr_sec_sorted
			curr_sec_sorted.add(s)
		self.sec_no_to_sec = sec_no_to_sec

	def get_objname_by_sec_no_off(self, sec_no, sec_off):
		# Note: it does happen that we have symbols in sections
		# that are not in sections list, like:
		# P|6|553|0|0|__except_list
		# D|6|0|553|0|__safe_se_handler_count|
		if sec_no not in self.sec_no_to_sec:
			return ""
		sec_sorted = self.sec_no_to_sec[sec_no]
		return sec_sorted.objname_by_offset(sec_off)

	def get_objname_by_symbol(self, sym):
		return self.get_objname_by_sec_no_off(sym.section, sym.offset)

(SYM_NULL, SYM_EXE, SYM_COMPILAND, SYM_COMPILAND_DETAILS) = ("N", "Exe", "C", "CD")
(SYM_COMPILAND_ENV, SYM_FUNCTION, SYM_BLOCK, SYM_DATA) = ("CE", "F", "B", "D")
(SYM_ANNOTATION, SYM_LABEL, SYM_PUBLIC, SYM_UDT, SYM_ENUM) = ("A", "L", "P", "U", "E")
(SYM_FUNC_TYPE, SYM_POINTER_TYPE, SYM_ARRAY_TYPE) = ("FT", "PT", "AT")
(SYM_BASE_TYPE, SYM_TYPEDEF, SYM_BASE_CLASS, SYM_FRIEND) = ("BT", "T", "BC", "Friend")
(SYM_FUNC_ARG_TYPE, SYM_FUNC_DEBUG_START, SYM_FUNC_DEBUG_END) = ("FAT", "FDS", "FDE")
(SYM_USING_NAMESPACE, SYM_VTABLE_SHAPE, SYM_VTABLE, SYM_CUSTOM) = ("UN", "VTS", "VT", "Custom")
(SYM_THUNK, SYM_CUSTOM_TYPE, SYM_MANAGED_TYPE, SYM_DIMENSION) = ("Thunk", "CT", "MT", "Dim")

# type | section | length | offset | rva | name
# F|1|35|0|4096|AllocArray<wchar_t>|wchar_t*__cdeclAllocArray<wchar_t>(unsignedint)
class Symbol(object):
	def __init__(self, l):
		parts = l.split("|")
		assert len(parts) in (6,7), "len(parts) is %d\n'%s'" % (len(parts), l)
		self.type = parts[0]
		self.section = int(parts[1])
		self.size = int(parts[2])
		self.offset = int(parts[3])
		self.rva = int(parts[4])
		self.name = parts[5]
		if self.type == SYM_THUNK:
			self.thunk_type = parts[6]
		elif self.type == SYM_DATA:
			self.data_type_name = parts[6]
		self.objname = None

	def full_name(self):
		return self.name + "@" + self.objname

class Type(object):
	def __init__(self, l):
		# TODO: parse the line
		self.line = l

def print_sym(sym):
	print(sym)
	print("name : %s" % sym.name)
	print("off  : %d" % sym.offset)
	print("size : %d" % sym.size)

class ParseState(object):
	def __init__(self, fo, obj_file_splitters):
		self.fo = fo
		self.obj_file_splitters = obj_file_splitters
		self.strings = Strings()
		self.types = []
		self.symbols = []
		self.sections = []
		# functions, strings etc. are laid out rounded so e.g. a function 11 bytes
		# in size really takes 16 bytes in the binary, due to rounding of the symbol
		# after it. Those values allow us to calculate how much is wasted due
		# to rounding
		self.symbols_unrounded_size = 0
		self.symbols_rounding_waste = 0

	def add_symbol(self, sym):
		self.symbols.append(sym)
		self.symbols_unrounded_size += sym.size
		prev_sym_idx = len(self.symbols) - 2
		if prev_sym_idx < 0: return
		prev_sym = self.symbols[prev_sym_idx]
		prev_sym_rounded_size = sym.offset - prev_sym.offset
		# prev_sym_rounded_size/ prev_sym_wasted can be < 0 in rare cases because
		# symbols can be inter-leaved e.g. a data symbol can be inside function
		# symbol, which breaks the simplistic logic of calculating rounded size
		# as curr.offset - prev.offset it can also happen when we cross section
		# boundaries. We just ignore those cases because approximate data is
		# better than no data
		if prev_sym_rounded_size < 0: return
		prev_sym_wasted = prev_sym_rounded_size - prev_sym.size
		# Note: I don't understand why but efi dump shows some very large gaps
		# between e.g. 2 functions. I filter everything above 16 bytes, since
		# wastage shouldn't be bigger than that
		if prev_sym_wasted > 16:
			#prev_sym_off = prev_sym.offset
			#sym_off = sym.offset
			return
		if prev_sym_wasted > 0:
			self.symbols_rounding_waste += prev_sym_wasted

	def readline(self):
		l = self.fo.readline()
		if not l:
			return None
		l = l.rstrip()
		#print("'%s'" % l)
		return l

def parse_start(state):
	l = state.readline()
	if l is None or len(l) == 0:
		return None
	assert l == "Format: 1", "unexpected line: '%s'" % l
	return parse_next_section

def parse_next_section(state):
	l = state.readline()
	#print("'%s'" % l)
	if l == None: return None
	if l == "": return parse_next_section
	if l == "Strings:":
		return parse_strings
	if l == "Types:":
		return parse_types
	if l == "Sections:":
		return parse_sections
	if l == "Symbols:":
		return parse_symbols
	print("Unknonw section: '%s'" % l)
	return None

def parse_strings(state):
	while True:
		l = state.readline()
		if l == None: return None
		if l == "": return parse_next_section
		parts = l.split("|", 2)
		idx = int(parts[0])
		s = parts[1]
		for splitter in state.obj_file_splitters:
			pos = s.find(splitter)
			if -1 != pos:
				s = s[pos + len(splitter):]
				break
		state.strings.add(idx, s)

def parse_sections(state):
	while True:
		l = state.readline()
		if l == None: return None
		if l == "": return parse_next_section
		state.sections.append(Section(l, state.strings))

def parse_symbols(state):
	while True:
		l = state.readline()
		if l == None: return None
		if l == "": return parse_next_section
		state.add_symbol(Symbol(l))

def parse_types(state):
	while True:
		l = state.readline()
		if l == None: return None
		if l == "": return parse_next_section
		# TODO: should parse structs, not just count them
		if l.startswith("struct"):
			state.types.append(Type(l))

def calc_symbols_objname(state):
	sec_to_objfile = SectionToObjFile(state.sections, state.strings)
	for sym in state.symbols:
		sym.objname = sec_to_objfile.get_objname_by_symbol(sym)

def parse_file_object(fo, obj_file_splitters):
	state = ParseState(fo, obj_file_splitters)
	curr = parse_start
	while curr:
		curr = curr(state)
	calc_symbols_objname(state)
	return state

def parse_file(file_name, obj_file_splitters=[]):
	print("parse_file: %s" % file_name)
	if file_name.endswith(".bz2"):
		with bz2.BZ2File(file_name, "r", buffering=2*1024*1024) as fo:
			return parse_file_object(fo, obj_file_splitters)
	with open(file_name, "r") as fo:
		return parse_file_object(fo, obj_file_splitters)

def n_as_str(n):
	if n > 0: return "+" + str(n)
	return str(n)

class Diff(object):
	def __init__(self):
		self.added = []
		self.removed = []
		self.changed = []
		self.str_sizes1 = 0
		self.str_sizes2 = 0

		self.n_symbols1 = 0
		self.symbols_unrounded_size1 = 0
		self.symbols_rounding_waste1 = 0

		self.n_symbols2 = 0
		self.symbols_unrounded_size2 = 0
		self.symbols_rounding_waste2 = 0

	def __repr__(self):
		str_sizes1 = self.str_sizes1
		str_sizes2 = self.str_sizes2
		str_sizes_diff = n_as_str(str_sizes2 - str_sizes1)
		n_symbols1 = self.n_symbols1
		n_symbols2 = self.n_symbols2
		symbols_diff = n_as_str(n_symbols2 - n_symbols1)
		sym_size1 = self.symbols_unrounded_size1
		sym_size2 = self.symbols_unrounded_size2
		sym_size_diff = n_as_str(sym_size2 - sym_size1)
		wasted1 = self.symbols_rounding_waste1
		wasted2 = self.symbols_rounding_waste2
		wasted_diff = n_as_str(wasted2 - wasted1)
		n_added = len(self.added)
		n_removed = len(self.removed)
		n_changed = len(self.changed)
		s = """symbols       : %(symbols_diff)-6s (%(n_symbols1)d => %(n_symbols2)d)
added         : %(n_added)d
removed       : %(n_removed)d
changed       : %(n_changed)d
symbol sizes  : %(sym_size_diff)-6s (%(sym_size1)d => %(sym_size2)d)
wasted rouding: %(wasted_diff)-6s (%(wasted1)d => %(wasted2)d)
string sizes  : %(str_sizes_diff)-6s (%(str_sizes1)d => %(str_sizes2)d)""" % locals()
		return s

def same_sym_sizes(syms):
	sizes = []
	for sym in syms:
		if sym.size in sizes:
			return True
		sizes.append(sym.size)
	return False

def syms_len(syms):
	if isinstance(syms, list):
		return len(syms)
	return 1

class ChangedSymbol(object):
	def __init__(self, sym1, sym2):
		assert sym1.name == sym2.name
		self.name = sym1.name
		self.size_diff = sym2.size - sym1.size
		self._full_name = sym1.full_name()

	def full_name(self):
		return self._full_name

class SymbolStats(object):
	def __init__(self):
		self.name_to_sym = {}
		self.str_sizes = 0

	def process_symbols(self, symbols):
		for sym in symbols:
			name = sym.name
			# for anonymous strings, we just count their total size
			# since we don't have a way to tell one string from another
			if name == "*str":
				self.str_sizes += sym.size
				continue

			if name not in self.name_to_sym:
				self.name_to_sym[name] = sym
				continue
			v = self.name_to_sym[name]
			if isinstance(v, list):
				v.append(sym)
			else:
				v = [v, sym]
			self.name_to_sym[name] = v

	def syms_len(self, name):
		if name not in self.name_to_sym:
			return 0
		return syms_len(self.name_to_sym[name])

def find_added(name, diff_syms1, diff_syms2):
	if name not in diff_syms1.name_to_sym:
		syms = diff_syms2.name_to_sym[name]
		if isinstance(syms, list):
			return syms
		return [syms]
	return []

def diff(parse1, parse2):
	assert isinstance(parse1, ParseState)
	assert isinstance(parse2, ParseState)
	diff_syms1 = SymbolStats()
	diff_syms1.process_symbols(parse1.symbols)

	diff_syms2 = SymbolStats()
	diff_syms2.process_symbols(parse2.symbols)

	added = []
	changed = []

	for name in diff_syms2.name_to_sym.keys():
		len1 = diff_syms1.syms_len(name)
		len2 = diff_syms2.syms_len(name)
		if len2 > len1:
			added += find_added(name, diff_syms1, diff_syms2)
		else:
			if len1 == 1 and len2 == 1:
				sym1 = diff_syms1.name_to_sym[name]
				sym2 = diff_syms2.name_to_sym[name]
				if sym1.size != sym2.size:
					changed += [ChangedSymbol(sym1, sym2)]

	removed = []
	for name in diff_syms1.name_to_sym.keys():
		len1 = diff_syms2.syms_len(name)
		len2 = diff_syms1.syms_len(name)
		if len2 > len1:
			removed += find_added(name, diff_syms2, diff_syms1)

	diff = Diff()
	diff.syms1 = diff_syms1
	diff.syms2 = diff_syms2
	diff.str_sizes1 =  diff_syms1.str_sizes
	diff.str_sizes2 =  diff_syms2.str_sizes
	diff.added = added
	diff.removed = removed
	diff.changed = changed
	diff.n_symbols1 = len(parse1.symbols)
	diff.symbols_unrounded_size1 = parse1.symbols_unrounded_size
	diff.symbols_rounding_waste1 = parse1.symbols_rounding_waste

	diff.n_symbols2 = len(parse2.symbols)
	diff.symbols_unrounded_size2 = parse2.symbols_unrounded_size
	diff.symbols_rounding_waste2 = parse2.symbols_rounding_waste
	return diff

def main():
	state = parse_file(g_file_name)
	print("%d types, %d sections, %d symbols" % (len(state.types), len(state.sections), len(state.symbols)))

if __name__ == "__main__":
	main()
