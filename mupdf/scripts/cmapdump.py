#!/usr/bin/env python3

# Parse a CMap file and dump it as a C struct.

import sys

# Decode a subset of CMap syntax (only what is needed for our built-in resources)
# We require that tokens are whitespace separated.

def dumpcmap(filename):
	codespacerange = []
	usecmap = ""
	cmapname = ""
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
		if line[0] == '/CMapName':
			cmapname = line[1][1:]
		elif line[0] == '/WMode':
			wmode = int(line[1])
		elif len(line) > 1 and line[1] == 'usecmap':
			usecmap = line[0][1:]
		elif len(line) > 1 and line[1] == 'begincodespacerange': current = 'codespacerange'
		elif len(line) > 1 and line[1] == 'begincidrange': current = 'cidrange'
		elif len(line) > 1 and line[1] == 'beginbfrange': current = 'bfrange'
		elif len(line) > 1 and line[1] == 'begincidchar': current = 'cidchar'
		elif len(line) > 1 and line[1] == 'beginbfchar': current = 'bfchar'
		elif line[0] == 'begincodespacerange': current = 'codespacerange'
		elif line[0] == 'begincidrange': current = 'cidrange'
		elif line[0] == 'beginbfrange': current = 'bfrange'
		elif line[0] == 'begincidchar': current = 'cidchar'
		elif line[0] == 'beginbfchar': current = 'bfchar'
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

	ranges = []
	xranges = []
	mranges = []
	mdata = []

	out_lo = -100
	out_hi = -100
	out_v_lo = 0
	out_v_hi = 0

	def flush_range():
		if out_lo >= 0:
			if out_lo > 0xffff or out_hi > 0xffff or out_v_lo > 0xffff:
				xranges.append((out_lo, out_hi, out_v_lo))
			else:
				ranges.append((out_lo, out_hi, out_v_lo))

	keys = list(map.keys())
	keys.sort()
	for code in keys:
		v = map[code]
		if type(v) is not int:
			flush_range()
			out_lo = out_hi = -100
			mranges.append((code, len(mdata)))
			mdata.append(len(v))
			mdata.extend(v)
		else:
			if code != out_hi + 1 or v != out_v_hi + 1:
				flush_range()
				out_lo = out_hi = code
				out_v_lo = out_v_hi = v
			else:
				out_hi = out_hi + 1
				out_v_hi = out_v_hi + 1
	flush_range()

	# Print C file

	cname = cmapname.replace('-', '_')

	print()
	print("/*", cmapname, "*/")
	print()

	if len(ranges) > 0:
		print("static const pdf_range cmap_%s_ranges[] = {" % cname)
		for r in ranges:
			print("{%d,%d,%d}," % r)
		print("};")
		print()
	if len(xranges) > 0:
		print("static const pdf_xrange cmap_%s_xranges[] = {" % cname)
		for r in xranges:
			print("{%d,%d,%d}," % r)
		print("};")
		print()
	if len(mranges) > 0:
		print("static const pdf_mrange cmap_%s_mranges[] = {" % cname)
		for r in mranges:
			print("{%d,%d}," % r)
		print("};")
		print()
		print("static const int cmap_%s_table[] = {" % cname)
		n = mdata[0]
		i = 0
		for r in mdata:
			if i <= n:
				sys.stdout.write("%d," % r)
				i = i + 1
			else:
				sys.stdout.write("\n%d," % r)
				i = 1
				n = r
		sys.stdout.write("\n")
		print("};")
		print()

	print("static pdf_cmap cmap_%s = {" % cname)
	print("\t{ -1, pdf_drop_cmap_imp },")
	print("\t/* cmapname */ \"%s\"," % cmapname)
	print("\t/* usecmap */ \"%s\", NULL," % usecmap)
	print("\t/* wmode */ %d," % wmode)
	print("\t/* codespaces */ %d, {" % len(codespacerange))
	if len(codespacerange) > 0:
		for codespace in codespacerange:
			fmt = "\t\t{ %%d, 0x%%0%dx, 0x%%0%dx }," % (codespace[0]*2, codespace[0]*2)
			print(fmt % codespace)
	else:
			print("\t\t{ 0, 0, 0 },")
	print("\t},")

	if len(ranges) > 0:
		print("\t%d, %d, (pdf_range*)cmap_%s_ranges," % (len(ranges),len(ranges),cname))
	else:
		print("\t0, 0, NULL, /* ranges */")

	if len(xranges) > 0:
		print("\t%d, %d, (pdf_xrange*)cmap_%s_xranges," % (len(xranges),len(xranges),cname))
	else:
		print("\t0, 0, NULL, /* xranges */")

	if len(mranges) > 0:
		print("\t%d, %d, (pdf_mrange*)cmap_%s_mranges," % (len(mranges),len(mranges),cname))
	else:
		print("\t0, 0, NULL, /* mranges */")

	if len(mdata) > 0:
		print("\t%d, %d, (int*)cmap_%s_table," % (len(mdata),len(mdata),cname))
	else:
		print("\t0, 0, NULL, /* table */")

	print("\t0, 0, 0, NULL /* splay tree */")
	print("};")

print("/* This is an automatically generated file. Do not edit. */")

for arg in sys.argv[1:]:
	dumpcmap(arg)
