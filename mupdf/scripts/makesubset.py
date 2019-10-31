#!/usr/bin/env python3

# Convert MES-2 (or WGL4) character set to list of glyphs for font subsetting.
# Also add small-caps glyph names for small letters and ligatures.

import sys

glyphs = {}
for line in open("scripts/glyphlist.txt").readlines():
	if len(line) > 0 and line[0] != '#':
		n, u = line.rstrip().split(';')
		if len(u) == 4:
			u = int(u, base=16)
			if u >= 0x0000 and u <= 0x001F: continue	# control block 1
			if u >= 0x007F and u <= 0x009F: continue	# control block 2
			if u >= 0x2500 and u <= 0x25FF: continue	# Box Drawing, Block Elements, Geometric Shapes
			if u not in glyphs:
				glyphs[u] = [n]
			else:
				glyphs[u].append(n)

# Ligatures are mapped to 'fi' and 'fl'; we also want them using the 'f_i' convention.

table = {}
do_small = False

def load_table(fn):
	for line in open(fn).readlines():
		is_small = ('SMALL LETTER' in line) or ('SMALL LIGATURE' in line)
		u = int(line.split()[0], 16)
		if u in glyphs:
			for n in glyphs[u]:
				table[n] = u
				if do_small and is_small:
					table[n+'.sc'] = u
		if u >= 128:
			table['uni%04X'%u] = u
			if do_small and is_small:
				table['uni%04X.sc'%u] = u

def load_ligs():
	table['ff'] = 0xFB00
	table['fi'] = 0xFB01
	table['fl'] = 0xFB02
	table['ffi'] = 0xFB03
	table['ffl'] = 0xFB04
	if do_small:
		table['f_f.sc'] = 0xFB00
		table['f_i.sc'] = 0xFB01
		table['f_l.sc'] = 0xFB02
		table['f_f_i.sc'] = 0xFB03
		table['f_f_l.sc'] = 0xFB04

if len(sys.argv) < 2:
	print('usage: python scripts/makesubset.py scripts/MES-2.TXT', file=sys.stderr)
else:
	for input in sys.argv[1:]:
		if input == '-sc':
			do_small = True
		elif input == '-lig':
			load_ligs()
		else:
			load_table(input)

	if len(sys.argv) > 2 and sys.argv[2] == '-scdump':
		smcp = []
		for n in list(table.keys()):
			u = table[n]
			if u > 0 and n.endswith('.sc') and not n.startswith('uni'):
				smcp.append('{0x%04X, "%s"},' % (u,n))
		smcp.sort()
		print('\n\t'.join(smcp))
	else:
		list = list(table.keys())
		list.sort()
		print(','.join(list))
