#!/usr/bin/env python3

# Convert unicode mapping table to C arrays mapping glyph names and unicode values.
#
# ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MISC/KOI8-U.TXT
# ftp://ftp.unicode.org/Public/MAPPINGS/ISO8859/8859-1.TXT
# ftp://ftp.unicode.org/Public/MAPPINGS/ISO8859/8859-7.TXT
# ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1250.TXT
# ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1251.TXT
# ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP1252.TXT
#

BANNED = [
	"controlSTX", "controlSOT", "controlETX", "controlEOT", "controlENQ",
	"controlACK", "controlBEL", "controlBS", "controlHT", "controlLF",
	"controlVT", "controlFF", "controlCR", "controlSO", "controlSI",
	"controlDLE", "controlDC1", "controlDC2", "controlDC3", "controlDC4",
	"controlNAK", "controlSYN", "controlETB", "controlCAN", "controlEM",
	"controlSUB", "controlESC", "controlFS", "controlGS", "controlRS",
	"controlUS",
	"SF100000", "SF110000", "SF010000", "SF030000", "SF020000", "SF040000",
	"SF080000", "SF090000", "SF060000", "SF070000", "SF050000", "SF430000",
	"SF240000", "SF510000", "SF390000", "SF250000", "SF500000", "SF490000",
	"SF380000", "SF280000", "SF260000", "SF360000", "SF370000", "SF420000",
	"SF190000", "SF230000", "SF410000", "SF450000", "SF460000", "SF400000",
	"SF540000", "SF440000",
]

glyphs = {}
for line in open("scripts/glyphlist.txt").readlines():
	if line[0] != '#':
		n, u = line.rstrip().split(';')
		if len(u) == 4:
			u = int(u, base=16)
			if u not in glyphs and n not in BANNED:
				glyphs[u] = n

def load_table(fn):
	table = [0] * 256
	for line in open(fn).readlines():
		line = line.strip()
		if line[0] != '#' and not line.endswith("#UNDEFINED"):
			line = line.split()
			c = int(line[0][2:], base=16)
			u = int(line[1][2:], base=16)
			table[c] = u
	return table

def dump_table(name, table):
	print("unsigned short fz_unicode_from_%s[256] = {" % name)
	for u in table:
		print('\t%d,' % u)
	print("};")
	print()

	print("const char *fz_glyph_name_from_%s[%d] = {" % (name, len(table)))
	for u in table:
		if u in glyphs:
			print('\t"%s",' % glyphs[u])
		else:
			print('\t_notdef,')
	print("};")
	print()

	rev = []
	i = 0
	for u in table:
		if u in glyphs:
			if u >= 128:
				rev += ['{0x%04x,%d},' % (u, i)]
		i = i + 1
	rev.sort()

	print("static const struct { unsigned short u, c; } %s_from_unicode[] = {" % name)
	for s in rev:
		print("\t" + s)
	print("};")
	print()

dump_table("iso8859_1", load_table("scripts/8859-1.TXT"))
dump_table("iso8859_7", load_table("scripts/8859-7.TXT"))
dump_table("koi8u", load_table("scripts/KOI8-U.TXT"))
dump_table("windows_1250", load_table("scripts/CP1250.TXT"))
dump_table("windows_1251", load_table("scripts/CP1251.TXT"))
dump_table("windows_1252", load_table("scripts/CP1252.TXT"))
