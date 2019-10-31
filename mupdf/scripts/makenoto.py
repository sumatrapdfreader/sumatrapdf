#!/usr/bin/env python3

import sys, glob, os

scripts = []

for line in open("include/mupdf/ucdn.h").readlines():
	if line.startswith("#define"):
		name = line.split()[1]
		if name.startswith("UCDN_SCRIPT_"):
			scripts.append(name)

blacklist = [
	"UCDN_SCRIPT_UNKNOWN",
	"UCDN_SCRIPT_INHERITED",

	"UCDN_SCRIPT_COMMON",
	"UCDN_SCRIPT_LATIN",
	"UCDN_SCRIPT_GREEK",
	"UCDN_SCRIPT_CYRILLIC",
	"UCDN_SCRIPT_HIRAGANA",
	"UCDN_SCRIPT_KATAKANA",
	"UCDN_SCRIPT_BOPOMOFO",
	"UCDN_SCRIPT_HAN",
	"UCDN_SCRIPT_HANGUL",

	"UCDN_SCRIPT_BRAILLE",
	"UCDN_SCRIPT_MEROITIC_CURSIVE",
	"UCDN_SCRIPT_MEROITIC_HIEROGLYPHS",
	"UCDN_SCRIPT_SYRIAC",
]

for s in blacklist:
	scripts.remove(s)

fonts = glob.glob("resources/fonts/noto/*.?tf")
#fonts.remove("resources/fonts/noto/NotoSans-Regular.otf")
#fonts.remove("resources/fonts/noto/NotoSerif-Regular.otf")
#fonts.remove("resources/fonts/noto/NotoSansSymbols-Regular.ttf")
#fonts.remove("resources/fonts/noto/NotoEmoji-Regular.ttf")

lower = {}
for f in fonts:
	lower[f.lower()] = os.path.basename(f)
unused = list(lower.keys())
unused.sort()

def casefont(us, ss, n):
	if n in lower:
		nn = lower[n].replace('.','_').replace('-','_')
		print("case %s: RETURN(noto_%s);" % (us,nn))
		del lower[n]
		return True
	return False

for us in scripts:
	ss = "".join([s.capitalize() for s in us.split("_")[2:]])
	list = []
	list.append("resources/fonts/noto/NotoSerif" + ss + "-Regular.otf")
	list.append("resources/fonts/noto/NotoSans" + ss + "-Regular.otf")
	list.append("resources/fonts/noto/NotoSerif" + ss + "-Regular.ttf")
	list.append("resources/fonts/noto/NotoSans" + ss + "-Regular.ttf")
	found = False
	for font in list:
		if casefont(us, ss, font.lower()):
			found = True
			break
	if not found:
		print("case %s: break;" % us)
	for font in list:
		if font.lower() in unused: unused.remove(font.lower())

for f in unused:
	print("// unmapped font:", lower[f])

for f in lower:
	if not f in unused:
		print("// unused font file:", lower[f])
