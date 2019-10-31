#!/usr/bin/env python3

# Parse a CMap file and dump it back out.

import sys

# Decode a subset of CMap syntax (only what is needed for our built-in resources)
# We require that tokens are whitespace separated.

def cleancmap(filename):
	codespacerange = []
	usecmap = ""
	cmapname = ""
	cmapversion = "1.0"
	csi_registry = "(Adobe)"
	csi_ordering = "(Unknown)"
	csi_supplement = 1
	wmode = 0
	isbf = False

	map = {}

	def tocode(s):
		if s[0] == '<' and s[-1] == '>':
			return int(s[1:-1], 16)
		return int(s, 10)

	def map_cidchar(lo, v):
		map[lo] = v

	def map_cidrange(lo, hi, v):
		while lo <= hi:
			map[lo] = v
			lo = lo + 1
			v = v + 1

	def add_bf(lo, v):
		# Decode unicode surrogate pairs
		if len(v) == 2 and v[0] >= 0xd800 and v[0] <= 0xdbff and v[1] >= 0xdc00 and v[1] <= 0xdfff:
			map[lo] = ((v[0] - 0xd800) << 10) + (v[1] - 0xdc00) + 0x10000
		elif len(v) == 1:
			map[lo] = v[0]
		elif len(v) <= 8:
			map[lo] = v[:]
		else:
			print("/* warning: too long one-to-many mapping: %s */" % (v))

	def map_bfchar(lo, bf):
		bf = bf[1:-1] # drop < >
		v = [int(bf[i:i+4],16) for i in range(0, len(bf), 4)]
		add_bf(lo, v)

	def map_bfrange(lo, hi, bf):
		bf = bf[1:-1] # drop < >
		v = [int(bf[i:i+4],16) for i in range(0, len(bf), 4)]
		while lo <= hi:
			add_bf(lo, v)
			lo = lo + 1
			v[-1] = v[-1] + 1

	current = None
	for line in open(filename, "r").readlines():
		if line[0] == '%':
			continue
		line = line.strip().split()
		if len(line) == 0:
			continue
		if line[0] == '/CMapVersion': cmapversion = line[1]
		elif line[0] == '/CMapName': cmapname = line[1][1:]
		elif line[0] == '/WMode': wmode = int(line[1])
		elif line[0] == '/Registry': csi_registry = line[1]
		elif line[0] == '/Ordering': csi_ordering = line[1]
		elif line[0] == '/Supplement': csi_supplement = line[1]
		elif len(line) > 1 and line[1] == 'usecmap': usecmap = line[0][1:]
		elif len(line) > 1 and line[1] == 'begincodespacerange': current = 'codespacerange'
		elif len(line) > 1 and line[1] == 'begincidrange': current = 'cidrange'
		elif len(line) > 1 and line[1] == 'beginbfrange': current = 'bfrange'; isbf = True
		elif len(line) > 1 and line[1] == 'begincidchar': current = 'cidchar'
		elif len(line) > 1 and line[1] == 'beginbfchar': current = 'bfchar'; isbf = True
		elif line[0] == 'begincodespacerange': current = 'codespacerange'
		elif line[0] == 'begincidrange': current = 'cidrange'
		elif line[0] == 'beginbfrange': current = 'bfrange'; isbf = True
		elif line[0] == 'begincidchar': current = 'cidchar'
		elif line[0] == 'beginbfchar': current = 'bfchar'; isbf = True
		elif line[0].startswith("end"):
			current = None
		elif current == 'codespacerange' and len(line) == 2:
			n, a, b = (len(line[0])-2)/2, tocode(line[0]), tocode(line[1])
			codespacerange.append((n, a, b))
		elif current == 'cidrange' and len(line) == 3:
			a, b, c = tocode(line[0]), tocode(line[1]), tocode(line[2])
			map_cidrange(a, b, c)
		elif current == 'cidchar' and len(line) == 2:
			a, b = tocode(line[0]), tocode(line[1])
			map_cidchar(a, b)
		elif current == 'bfchar' and len(line) == 2:
			a, b = tocode(line[0]), line[1]
			map_bfchar(a, b)
		elif current == 'bfrange' and len(line) == 3:
			a, b, c = tocode(line[0]), tocode(line[1]), line[2]
			map_bfrange(a, b, c)

	# Create ranges

	singles = []
	ranges = []
	mranges = []

	out_lo = -100
	out_hi = -100
	out_v_lo = 0
	out_v_hi = 0

	def flush_range():
		if out_lo >= 0:
			if out_lo == out_hi:
				singles.append((out_lo, out_v_lo))
			else:
				ranges.append((out_lo, out_hi, out_v_lo))

	keys = list(map.keys())
	keys.sort()
	for code in keys:
		v = map[code]
		if type(v) is not int:
			flush_range()
			out_lo = out_hi = -100
			mranges.append((code, v))
		else:
			if code != out_hi + 1 or v != out_v_hi + 1:
				flush_range()
				out_lo = out_hi = code
				out_v_lo = out_v_hi = v
			else:
				out_hi = out_hi + 1
				out_v_hi = out_v_hi + 1
	flush_range()

	# Print CMap file

	print("%!PS-Adobe-3.0 Resource-CMap")
	print("%%DocumentNeededResources: procset (CIDInit)")
	print("%%IncludeResource: procset (CIDInit)")
	print("%%%%BeginResource: CMap (%s)" % cmapname)
	print("%%%%Version: %s" % cmapversion)
	print("%%EndComments")
	print("/CIDInit /ProcSet findresource begin")
	print("12 dict begin")
	print("begincmap")
	if usecmap: print("/%s usecmap" % usecmap)
	print("/CIDSystemInfo 3 dict dup begin")
	print("  /Registry %s def" % csi_registry)
	print("  /Ordering %s def" % csi_ordering)
	print("  /Supplement %s def" % csi_supplement)
	print("end def")
	print("/CMapName /%s def" % cmapname)
	print("/CMapVersion %s def" % cmapversion)
	print("/CMapType 1 def")
	print("/WMode %d def" % wmode)

	if len(codespacerange):
		print("%d begincodespacerange" % len(codespacerange))
		for r in codespacerange:
			fmt = "<%%0%dx> <%%0%dx>" % (r[0]*2, r[0]*2)
			print(fmt % (r[1], r[2]))
		print("endcodespacerange")

	if len(singles) > 0:
		if isbf:
			print("%d beginbfchar" % len(singles))
			for s in singles:
				print("<%04x> <%04x>" % s)
			print("endbfchar")
		else:
			print("%d begincidchar" % len(singles))
			for s in singles:
				print("<%04x> %d" % s)
			print("endcidchar")

	if len(ranges) > 0:
		if isbf:
			print("%d beginbfrange" % len(ranges))
			for r in ranges:
				print("<%04x> <%04x> <%04x>" % r)
			print("endbfrange")
		else:
			print("%d begincidrange" % len(ranges))
			for r in ranges:
				print("<%04x> <%04x> %d" % r)
			print("endcidrange")

	if len(mranges) > 0:
		print("%d beginbfchar" % len(mranges))
		for cid, v in mranges:
			print("<%04x> <%s>" % (cid, "".join(["%04x" % ch for ch in v])))
		print("endbfchar")

	print("endcmap")
	print("CMapName currentdict /CMap defineresource pop")
	print("end")
	print("end")
	print("%%EndResource")
	print("%%EOF")

for arg in sys.argv[1:]:
	cleancmap(arg)
