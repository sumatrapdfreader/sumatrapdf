#!/usr/bin/env python3

# Parse a Uni* CMap file and flatten it.
#
# The Uni* CMap files only have 'cidchar' and 'cidrange' sections, never
# 'bfchar' or 'bfrange'.

import sys

def flattencmap(filename):
	codespacerange = []
	usecmap = ""
	cmapname = ""
	cmapversion = "1.0"
	csi_registry = "(Adobe)"
	csi_ordering = "(Unknown)"
	csi_supplement = 1
	wmode = 0

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
		elif len(line) > 1 and line[1] == 'begincidchar': current = 'cidchar'
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

	# Print flattened CMap file

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

	keys = list(map.keys())
	keys.sort()
	print("%d begincidchar" % len(keys))
	for code in keys:
		v = map[code]
		print("<%04x> %d" % (code, v))
	print("endcidchar")

	print("endcmap")
	print("CMapName currentdict /CMap defineresource pop")
	print("end")
	print("end")
	print("%%EndResource")
	print("%%EOF")

for arg in sys.argv[1:]:
	flattencmap(arg)
