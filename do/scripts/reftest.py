"""
Renders all files in a directory to page TGA images and an XML dump
and checks these renderings against a previous known-good reference
rendering, creating TGA difference files for easier comparison.

Use for regression testing:

reftest.py /dir/to/test -refdir /dir/for/references
reftest.py /dir/one /dir/two /dir/three
"""

import os, sys, struct, fnmatch
from subprocess import Popen, PIPE
pjoin = os.path.join

def EngineDump(EngineDumpExe, file, tgaPath):
	proc = Popen([EngineDumpExe, file, "-full", "-render", tgaPath], stdout=PIPE)
	xmlDump, _ = proc.communicate()
	return xmlDump

def TgaRleUnpack(data):
	# unpacks data from a type 2 TGA file (24-bit uncompressed)
	# or a type 10 TGA file (24-bit run-length encoded)
	if len(data) >= 18 and struct.unpack("B", data[2]) == 2:
		return data[18:]
	exp, i = [], 18
	while i + 4 < len(data):
		bit = struct.unpack("B", data[i])[0] + 1
		i += 1
		if bit <= 128:
			exp.append(data[i:i + bit * 3])
			i += bit * 3
		else:
			exp.append(data[i:i + 3] * (bit - 128))
			i += 3
	return "".join(exp)

def TgaCmpColor(col1, col2):
	# returns 0 for identical colors, 1 for similar colors and 2 for different colors
	if col1 == col2:
		return 0
	rgb1, rgb2 = struct.unpack("<BBB", col1), struct.unpack("<BBB", col2)
	if (rgb1[0] - rgb2[0]) ** 2 + (rgb1[1] - rgb2[1]) ** 2 + (rgb1[2] - rgb2[2]) ** 2 < 75:
		return 1
	return 2

def BitmapDiff(tgaRef, tgaCmp, tgaDiff):
	# ensure that the comparison bitmap exists
	if not os.path.isfile(tgaCmp):
		open(tgaCmp, "wb").write("")
	ref = open(tgaRef, "rb").read()
	cmp = open(tgaCmp, "rb").read()
	
	# bail if the files are either identical or the reference is broken
	if ref == cmp:
		return False
	if len(ref) < 18:
		return True
	
	# determine bitmap dimensions
	width, height = struct.unpack("<HH", ref[12:16])
	
	# unpack the run-length encoded data
	refData = TgaRleUnpack(ref)
	if len(refData) < width * height * 3:
		refData += "\x00" * (width * height * 3 - len(refData))
	cmpData = TgaRleUnpack(cmp)
	if len(cmpData) < width * height * 3:
		cmpData += "\xFF" * (width * height * 3 - len(cmpData))
	
	# bail if the files are just differently compressed
	if refData == cmpData:
		return False
	
	# write a black pixel for identical, a dark red pixel for similar and a
	# bright red pixel for different color values (packed as a type 9 TGA file,
	# 8-bit indexed run-length encoded)
	diff = [struct.pack("<BBBHHBHHHHBB", 0, 1, 9, 0, 3, 24, 0, 0, width, height, 8, 0), "\x00\x00\x00", "\x00\x00\x80", "\x00\x00\xFF"]
	for i in range(height):
		refLine = refData[i * width * 3:(i + 1) * width * 3]
		cmpLine = cmpData[i * width * 3:(i + 1) * width * 3]
		j = 0
		# reencode the difference file line-by-line
		while j < width:
			color = TgaCmpColor(refLine[j*3:j*3+3], cmpLine[j*3:j*3+3])
			k = j + 1
			while k < width and k - j < 128 and color == TgaCmpColor(refLine[k*3:k*3+3], cmpLine[k*3:k*3+3]):
				k += 1
			diff.append(struct.pack("BB", k - j - 1 + 128, color))
			j = k
	
	# add the file footer
	diff.append("\x00" * 8 + "TRUEVISION-XFILE.\x00")
	
	# write the red-on-black difference file to disk
	open(tgaDiff, "wb").write("".join(diff))
	return True

def RefTestFile(EngineDumpExe, file, refdir):
	# create an XML dump and bitmap renderings of all pages
	base = os.path.splitext(os.path.split(file)[1])[0]
	tgaPath = pjoin(refdir, base + "-%d.cmp.tga")
	xmlDump = EngineDump(EngineDumpExe, file, tgaPath)
	
	# compare the XML dumps (remove the dump if it's the same as the reference)
	xmlRefPath = pjoin(refdir, base + ".ref.xml")
	xmlCmpPath = pjoin(refdir, base + ".cmp.xml")
	if not os.path.isfile(xmlRefPath):
		open(xmlRefPath, "wb").write(xmlDump)
	elif open(xmlRefPath, "rb").read() != xmlDump:
		open(xmlCmpPath, "wb").write(xmlDump)
		print "  FAIL!", xmlCmpPath
	elif os.path.isfile(xmlCmpPath):
		os.remove(xmlCmpPath)
	
	# compare all bitmap renderings (and create diff bitmaps where needed)
	for file in fnmatch.filter(os.listdir(refdir), base + "-[0-9]*.ref.tga"):
		tgaRefPath = pjoin(refdir, file)
		tgaCmpPath, tgaDiffPath = tgaRefPath[:-8] + ".cmp.tga", tgaRefPath[:-8] + ".diff.tga"
		if BitmapDiff(tgaRefPath, tgaCmpPath, tgaDiffPath):
			print "  FAIL!", tgaCmpPath
		else:
			os.remove(tgaCmpPath)
			if os.path.isfile(tgaDiffPath):
				os.remove(tgaDiffPath)
	for file in fnmatch.filter(os.listdir(refdir), base + "-[0-9]*.cmp.tga"):
		tgaCmpPath = pjoin(refdir, file)
		tgaRefPath = tgaCmpPath[:-8] + ".ref.tga"
		if not os.path.isfile(tgaRefPath):
			os.rename(tgaCmpPath, tgaRefPath)

def RefTestDir(EngineDumpExe, dir, refdir):
	# create reference directory, if it doesn't exists yet
	if not os.path.isdir(refdir):
		os.makedirs(refdir)
	
	# test all files in the directory to test
	for file in os.listdir(dir):
		file = pjoin(dir, file)
		if os.path.isfile(file):
			print "Testing", file
			RefTestFile(EngineDumpExe, file, refdir)
	
	# list all differences (again)
	diffs = fnmatch.filter(os.listdir(refdir), "*.cmp.*")
	if diffs:
		print "\n" + "=" * 25 + " RENDERING DIFFERENCES " + "=" * 25
		for file in diffs:
			print pjoin(refdir, file)
		print
	
	return len(diffs)

def main(args):
	# find a path to EngineDump.exe (defaults to ../obj-dbg/EngineDump.exe)
	if len(args) > 1 and args[1].lower().endswith(".exe"):
		EngineDumpExe = args.pop(1)
	else:
		EngineDumpExe = pjoin(os.path.dirname(__file__), "..", "obj-dbg", "EngineDump.exe")
	
	# minimal sanity check of arguments
	if not args[1:] or not os.path.isdir(args[1]):
		print "Usage: %s [EngineDump.exe] <dir> [-refdir <dir>] [<dir> ...]" % (os.path.split(args[0])[1])
		return
	
	# collect all directories to test (and the corresonding reference directories)
	dirs, ix = [], 1
	while ix < len(args):
		if ix + 2 < len(args) and args[ix + 1] == "-refdir":
			dirs.append((args[ix], args[ix + 2]))
			ix += 3
		else:
			dirs.append((args[ix], pjoin(args[ix], "references")))
			ix += 1
	
	# run the test
	fails = 0
	for (dir, refdir) in dirs:
		fails += RefTestDir(EngineDumpExe, dir, refdir)
	sys.exit(fails)

if __name__ == "__main__":
	main(sys.argv[:])
