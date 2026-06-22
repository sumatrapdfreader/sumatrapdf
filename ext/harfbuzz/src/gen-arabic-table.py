#!/usr/bin/env python3

"""usage: ./gen-arabic-table.py [--rust] ArabicShaping.txt UnicodeData.txt Blocks.txt

Input files:
* https://unicode.org/Public/UCD/latest/ucd/ArabicShaping.txt
* https://unicode.org/Public/UCD/latest/ucd/UnicodeData.txt
* https://unicode.org/Public/UCD/latest/ucd/Blocks.txt
"""

import os.path, sys

import packTab

if len(sys.argv) > 1 and sys.argv[1] == "--rust":
	del sys.argv[1]
	language = packTab.languages["rust"]
else:
	language = packTab.languages["c"]

if len (sys.argv) != 4:
	sys.exit (__doc__)

files = [open (x, encoding='utf-8') for x in sys.argv[1:]]

headers = [[files[0].readline (), files[0].readline ()], [files[2].readline (), files[2].readline ()]]
headers.append (["UnicodeData.txt does not have a header."])
while files[0].readline ().find ('##################') < 0:
	pass

blocks = {}
# Keep this in sync with hb_arabic_joiner_type_t in hb-ot-shaper-arabic.cc.
JOINING_CODE = {
	"JOINING_TYPE_U": 0,
	"JOINING_TYPE_L": 1,
	"JOINING_TYPE_R": 2,
	"JOINING_TYPE_D": 3,
	"JOINING_TYPE_C": 3,
	"JOINING_GROUP_ALAPH": 4,
	"JOINING_GROUP_DALATH_RISH": 5,
	"JOINING_TYPE_T": 6,
	"JOINING_TYPE_X": 7,
}

def read_blocks(f):
	global blocks
	for line in f:

		j = line.find ('#')
		if j >= 0:
			line = line[:j]

		fields = [x.strip () for x in line.split (';')]
		if len (fields) == 1:
			continue

		uu = fields[0].split ('..')
		start = int (uu[0], 16)
		if len (uu) == 1:
			end = start
		else:
			end = int (uu[1], 16)

		t = fields[1]

		for u in range (start, end + 1):
			blocks[u] = t

def print_joining_table(f):

	values = {}
	for line in f:

		if line[0] == '#':
			continue

		fields = [x.strip () for x in line.split (';')]
		if len (fields) == 1:
			continue

		u = int (fields[0], 16)

		if fields[3] in ["ALAPH", "DALATH RISH"]:
			value = "JOINING_GROUP_" + fields[3].replace(' ', '_')
		else:
			value = "JOINING_TYPE_" + fields[2]
		values[u] = value

	code = packTab.Code ('_hb_arabic_joining')
	data = {u: JOINING_CODE[v] for u, v in values.items ()}
	sol = packTab.pack_table (data, default=JOINING_CODE['JOINING_TYPE_X'], compression=9)
	if language.name == "c":
		sol.genCode (code, 'joining_type', language=language, private=True)

		print ("#include \"hb.hh\"")
		print ()
		code.print_code (file=sys.stdout, language=language, private=True)
		print ()
		print ("static unsigned int")
		print ("joining_type (hb_codepoint_t u)")
		print ("{")
		print ("  return _hb_arabic_joining_joining_type (u);")
		print ("}")
		print ()
	elif language.name == "rust":
		sol.genCode (code, 'joining_type_u8', language=language, private=False)

		print ("#![allow(unused_parens)]")
		print ("#![allow(clippy::unnecessary_cast, clippy::unreadable_literal, clippy::double_parens)]")
		print ()
		print ("use crate::hb::unicode::Codepoint;")
		print ()
		print (
			"use super::ot_shaper_arabic::hb_arabic_joining_type_t::{"
			"self, D, GroupAlaph, GroupDalathRish, L, R, T, U, X};"
		)
		print ()
		code.print_code (file=sys.stdout, language=language, private=False)
		print ()
		print ("#[inline]")
		print ("pub(crate) fn joining_type (u: Codepoint) -> hb_arabic_joining_type_t")
		print ("{")
		print ("  match _hb_arabic_joining_joining_type_u8 (u as usize) {")
		print ("    0 => U,")
		print ("    1 => L,")
		print ("    2 => R,")
		print ("    3 => D,")
		print ("    4 => GroupAlaph,")
		print ("    5 => GroupDalathRish,")
		print ("    6 => T,")
		print ("    7 => X,")
		print ("    _ => unreachable! (),")
		print ("  }")
		print ("}")
		print ()
	else:
		assert False, "Unknown language: %s" % language.name

LIGATURES = (
	0xF2EE, 0xFC08, 0xFC0E, 0xFC12, 0xFC32, 0xFC3F, 0xFC40, 0xFC41, 0xFC42,
	0xFC44, 0xFC4E, 0xFC5E, 0xFC60, 0xFC61, 0xFC62, 0xFC6A, 0xFC6D, 0xFC6F,
	0xFC70, 0xFC73, 0xFC75, 0xFC86, 0xFC8F, 0xFC91, 0xFC94, 0xFC9C, 0xFC9D,
	0xFC9E, 0xFC9F, 0xFCA1, 0xFCA2, 0xFCA3, 0xFCA4, 0xFCA8, 0xFCAA, 0xFCAC,
	0xFCB0, 0xFCC9, 0xFCCA, 0xFCCB, 0xFCCC, 0xFCCD, 0xFCCE, 0xFCCF, 0xFCD0,
	0xFCD1, 0xFCD2, 0xFCD3, 0xFCD5, 0xFCDA, 0xFCDB, 0xFCDC, 0xFCDD, 0xFD30,
	0xFD88, 0xFEF5, 0xFEF6, 0xFEF7, 0xFEF8, 0xFEF9, 0xFEFA, 0xFEFB, 0xFEFC,
	0xF201, 0xF211, 0xF2EE,
)

def print_shaping_table(f):

	shapes = {}
	ligatures = {}
	names = {}
	lines = f.readlines()
	lines += [
		"F201;PUA ARABIC LIGATURE LELLAH ISOLATED FORM;Lo;0;AL;<isolated> 0644 0644 0647;;;;N;;;;;",
		"F211;PUA ARABIC LIGATURE LAM WITH MEEM WITH JEEM INITIAL FORM;Lo;0;AL;<initial> 0644 0645 062C;;;;N;;;;;",
		"F2EE;PUA ARABIC LIGATURE SHADDA WITH FATHATAN ISOLATED FORM;Lo;0;AL;<isolated> 0020 064B 0651;;;;N;;;;;",
	]
	for line in lines:

		fields = [x.strip () for x in line.split (';')]
		if fields[5][0:1] != '<':
			continue

		items = fields[5].split (' ')
		shape, items = items[0][1:-1], tuple (int (x, 16) for x in items[1:])
		c = int (fields[0], 16)

		if not shape in ['initial', 'medial', 'isolated', 'final']:
			continue

		if len (items) != 1:
			# Mark ligatures start with space and are in visual order, so we
			# remove the space and reverse the items.
			if items[0] == 0x0020:
				items = items[:0:-1]
				shape = None
			# We only care about a subset of ligatures
			if c not in LIGATURES:
				continue

			# Save ligature
			names[c] = fields[1]
			if items not in ligatures:
				ligatures[items] = {}
			ligatures[items][shape] = c
		else:
			# Save shape
			if items[0] not in names:
				names[items[0]] = fields[1]
			else:
				names[items[0]] = os.path.commonprefix ([names[items[0]], fields[1]]).strip ()
			if items[0] not in shapes:
				shapes[items[0]] = {}
			shapes[items[0]][shape] = c

	print ()
	print ("static const uint16_t shaping_table[][4] =")
	print ("{")

	keys = shapes.keys ()
	min_u, max_u = min (keys), max (keys)
	for u in range (min_u, max_u + 1):
		s = [shapes[u][shape] if u in shapes and shape in shapes[u] else 0
		     for shape in  ['initial', 'medial', 'final', 'isolated']]
		value = ', '.join ("0x%04Xu" % c for c in s)
		print ("  {%s}, /* U+%04X %s */" % (value, u, names[u] if u in names else ""))

	print ("};")
	print ()
	print ("#define SHAPING_TABLE_FIRST	0x%04Xu" % min_u)
	print ("#define SHAPING_TABLE_LAST	0x%04Xu" % max_u)
	print ()

	ligas_2 = {}
	ligas_3 = {}
	ligas_mark_2 = {}
	for key in ligatures.keys ():
		for shape in ligatures[key]:
			c = ligatures[key][shape]
			if len(key) == 3:
				if shape == 'isolated':
					liga = (shapes[key[0]]['initial'], shapes[key[1]]['medial'], shapes[key[2]]['final'])
				elif shape == 'final':
					liga = (shapes[key[0]]['medial'], shapes[key[1]]['medial'], shapes[key[2]]['final'])
				elif shape == 'initial':
					liga = (shapes[key[0]]['initial'], shapes[key[1]]['medial'], shapes[key[2]]['medial'])
				else:
					raise Exception ("Unexpected shape", shape)
				if liga[0] not in ligas_3:
					ligas_3[liga[0]] = []
				ligas_3[liga[0]].append ((liga[1], liga[2], c))
			elif len(key) == 2:
				if shape is None:
					liga = key
					if liga[0] not in ligas_mark_2:
						ligas_mark_2[liga[0]] = []
					ligas_mark_2[liga[0]].append ((liga[1], c))
					continue
				elif shape == 'isolated':
					liga = (shapes[key[0]]['initial'], shapes[key[1]]['final'])
				elif shape == 'final':
					liga = (shapes[key[0]]['medial'], shapes[key[1]]['final'])
				elif shape == 'initial':
					liga = (shapes[key[0]]['initial'], shapes[key[1]]['medial'])
				else:
					raise Exception ("Unexpected shape", shape)
				if liga[0] not in ligas_2:
					ligas_2[liga[0]] = []
				ligas_2[liga[0]].append ((liga[1], c))
			else:
				raise Exception ("Unexpected number of ligature components", key)
	max_i = max (len (ligas_2[l]) for l in ligas_2)
	print ()
	print ("static const struct ligature_set_t {")
	print (" uint16_t first;")
	print (" struct ligature_pairs_t {")
	print ("   uint16_t components[1];")
	print ("   uint16_t ligature;")
	print (" } ligatures[%d];" % max_i)
	print ("} ligature_table[] =")
	print ("{")
	for first in sorted (ligas_2.keys ()):

		print ("  { 0x%04Xu, {" % (first))
		for liga in ligas_2[first]:
			print ("    { {0x%04Xu}, 0x%04Xu }, /* %s */" % (liga[0], liga[1], names[liga[1]]))
		print ("  }},")

	print ("};")
	print ()

	max_i = max (len (ligas_mark_2[l]) for l in ligas_mark_2)
	print ()
	print ("static const struct ligature_mark_set_t {")
	print (" uint16_t first;")
	print (" struct ligature_pairs_t {")
	print ("   uint16_t components[1];")
	print ("   uint16_t ligature;")
	print (" } ligatures[%d];" % max_i)
	print ("} ligature_mark_table[] =")
	print ("{")
	for first in sorted (ligas_mark_2.keys ()):

		print ("  { 0x%04Xu, {" % (first))
		for liga in ligas_mark_2[first]:
			print ("    { {0x%04Xu}, 0x%04Xu }, /* %s */" % (liga[0], liga[1], names[liga[1]]))
		print ("  }},")

	print ("};")
	print ()

	max_i = max (len (ligas_3[l]) for l in ligas_3)
	print ()
	print ("static const struct ligature_3_set_t {")
	print (" uint16_t first;")
	print (" struct ligature_triplets_t {")
	print ("   uint16_t components[2];")
	print ("   uint16_t ligature;")
	print (" } ligatures[%d];" % max_i)
	print ("} ligature_3_table[] =")
	print ("{")
	for first in sorted (ligas_3.keys ()):

		print ("  { 0x%04Xu, {" % (first))
		for liga in ligas_3[first]:
			print ("    { {0x%04Xu, 0x%04Xu}, 0x%04Xu}, /* %s */" % (liga[0], liga[1], liga[2], names[liga[2]]))
		print ("  }},")

	print ("};")
	print ()



print ("/* == Start of generated table == */")
print ("/*")
print (" * The following table is generated by running:")
print (" *")
print (" *   ./gen-arabic-table.py %sArabicShaping.txt UnicodeData.txt Blocks.txt" %
		("--rust " if language.name == "rust" else ""))
print (" *")
print (" * on files with these headers:")
print (" *")
for h in headers:
	for l in h:
		print (" * %s" % (l.strip()))
print (" */")
print ()
read_blocks (files[2])
if language.name == "c":
	print ("#ifndef HB_OT_SHAPER_ARABIC_TABLE_HH")
	print ("#define HB_OT_SHAPER_ARABIC_TABLE_HH")
	print ()

	print_joining_table (files[0])
	print_shaping_table (files[1])

	print ()
	print ("#endif /* HB_OT_SHAPER_ARABIC_TABLE_HH */")
elif language.name == "rust":
	print_joining_table (files[0])
else:
	assert False, "Unknown language: %s" % language.name
print ()
print ("/* == End of generated table == */")
