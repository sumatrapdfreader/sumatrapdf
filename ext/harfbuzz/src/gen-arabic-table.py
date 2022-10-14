#!/usr/bin/env python3

"""usage: ./gen-arabic-table.py ArabicShaping.txt UnicodeData.txt Blocks.txt

Input files:
* https://unicode.org/Public/UCD/latest/ucd/ArabicShaping.txt
* https://unicode.org/Public/UCD/latest/ucd/UnicodeData.txt
* https://unicode.org/Public/UCD/latest/ucd/Blocks.txt
"""

import os.path, sys

if len (sys.argv) != 4:
	sys.exit (__doc__)

files = [open (x, encoding='utf-8') for x in sys.argv[1:]]

headers = [[files[0].readline (), files[0].readline ()], [files[2].readline (), files[2].readline ()]]
headers.append (["UnicodeData.txt does not have a header."])
while files[0].readline ().find ('##################') < 0:
	pass

blocks = {}
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

	short_value = {}
	for value in sorted (set ([v for v in values.values ()] + ['JOINING_TYPE_X'])):
		short = ''.join(x[0] for x in value.split('_')[2:])
		assert short not in short_value.values()
		short_value[value] = short

	print ()
	for value,short in short_value.items():
		print ("#define %s	%s" % (short, value))

	uu = sorted(values.keys())
	num = len(values)
	all_blocks = set([blocks[u] for u in uu])

	last = -100000
	ranges = []
	for u in uu:
		if u - last <= 1+16*5:
			ranges[-1][-1] = u
		else:
			ranges.append([u,u])
		last = u

	print ()
	print ("static const uint8_t joining_table[] =")
	print ("{")
	last_block = None
	offset = 0
	for start,end in ranges:

		print ()
		print ("#define joining_offset_0x%04xu %d" % (start, offset))

		for u in range(start, end+1):

			block = blocks.get(u, last_block)
			value = values.get(u, "JOINING_TYPE_X")

			if block != last_block or u == start:
				if u != start:
					print ()
				if block in all_blocks:
					print ("\n  /* %s */" % block)
				else:
					print ("\n  /* FILLER */")
				last_block = block
				if u % 32 != 0:
					print ()
					print ("  /* %04X */" % (u//32*32), "  " * (u % 32), end="")

			if u % 32 == 0:
				print ()
				print ("  /* %04X */ " % u, end="")
			print ("%s," % short_value[value], end="")
		print ()

		offset += end - start + 1
	print ()
	occupancy = num * 100. / offset
	print ("}; /* Table items: %d; occupancy: %d%% */" % (offset, occupancy))
	print ()

	page_bits = 12
	print ()
	print ("static unsigned int")
	print ("joining_type (hb_codepoint_t u)")
	print ("{")
	print ("  switch (u >> %d)" % page_bits)
	print ("  {")
	pages = set([u>>page_bits for u in [s for s,e in ranges]+[e for s,e in ranges]])
	for p in sorted(pages):
		print ("    case 0x%0Xu:" % p)
		for (start,end) in ranges:
			if p not in [start>>page_bits, end>>page_bits]: continue
			offset = "joining_offset_0x%04xu" % start
			print ("      if (hb_in_range<hb_codepoint_t> (u, 0x%04Xu, 0x%04Xu)) return joining_table[u - 0x%04Xu + %s];" % (start, end, start, offset))
		print ("      break;")
		print ("")
	print ("    default:")
	print ("      break;")
	print ("  }")
	print ("  return X;")
	print ("}")
	print ()
	for value,short in short_value.items():
		print ("#undef %s" % (short))
	print ()

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
print (" *   ./gen-arabic-table.py ArabicShaping.txt UnicodeData.txt Blocks.txt")
print (" *")
print (" * on files with these headers:")
print (" *")
for h in headers:
	for l in h:
		print (" * %s" % (l.strip()))
print (" */")
print ()
print ("#ifndef HB_OT_SHAPER_ARABIC_TABLE_HH")
print ("#define HB_OT_SHAPER_ARABIC_TABLE_HH")
print ()

read_blocks (files[2])
print_joining_table (files[0])
print_shaping_table (files[1])

print ()
print ("#endif /* HB_OT_SHAPER_ARABIC_TABLE_HH */")
print ()
print ("/* == End of generated table == */")
