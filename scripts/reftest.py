"""
Renders all files in a directory to page bitmaps and an XML dump and
checks these renderings against a previous known-good reference
rendering, creating bitmap difference files for easier comparison.

Use for regression testing:

reftest.py /dir/to/test -refdir /dir/for/references
reftest.py /dir/one /dir/two /dir/three
"""

import os, sys, struct, fnmatch
from subprocess import Popen, PIPE
pjoin = os.path.join

def EngineDump(EngineDumpExe, file, bmpPath):
	proc = Popen([EngineDumpExe, file, "-full", "-render", bmpPath], stdout=PIPE)
	xmlDump, _ = proc.communicate()
	return xmlDump

def BitmapDiff(bmpRef, bmpCmp, bmpDiff):
	# ensure that the comparison bitmap exists
	if not os.path.isfile(bmpCmp):
		open(bmpCmp, "wb").write("")
	ref = open(bmpRef, "rb").read()
	cmp = open(bmpCmp, "rb").read()
	
	# bail if the files are either identical or the reference is broken
	if ref == cmp:
		return False
	if len(ref) < 22:
		return True
	
	# determine bitmap dimensions
	header_len, _, width = struct.unpack('LLL', ref[10:22])
	
	# compare as long as both files contain pixels
	i, lenRef = header_len, len(ref)
	lenMin = min(lenRef, len(cmp))
	
	# write a black pixel for identical and a red pixel for different color values
	diff = [ref[:header_len]]
	while i + 3 <= lenMin:
		diff.append("\x00\x00\x00" if ref[i:i+3] == cmp[i:i+3] else "\x00\x00\xFF")
		i += 3
		# pad the difference between actual width and stride
		if width % 4 != 0 and len(diff) % width == 1:
			padding = 4 - (3 * width % 4)
			diff[-1] += "\x88" * padding
			i += padding
	
	# fill up the remaining pixels with gray
	diff.append("\x80" * (lenRef - i))
	
	# write the red-on-black difference file to disk
	open(bmpDiff, "wb").write("".join(diff))
	return True

def RefTestFile(EngineDumpExe, file, refdir):
	# create an XML dump and bitmap renderings of all pages
	base = os.path.splitext(os.path.split(file)[1])[0]
	bmpPath = pjoin(refdir, base + "-%d.cmp.bmp")
	xmlDump = EngineDump(EngineDumpExe, file, bmpPath)
	
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
	for file in fnmatch.filter(os.listdir(refdir), base + "-[0-9]*.ref.bmp"):
		bmpRefPath = pjoin(refdir, file)
		bmpCmpPath, bmpDiffPath = bmpRefPath[:-8] + ".cmp.bmp", bmpRefPath[:-8] + ".diff.bmp"
		if BitmapDiff(bmpRefPath, bmpCmpPath, bmpDiffPath):
			print "  FAIL!", bmpCmpPath
		else:
			os.remove(bmpCmpPath)
			if os.path.isfile(bmpDiffPath):
				os.remove(bmpDiffPath)
	for file in fnmatch.filter(os.listdir(refdir), base + "-[0-9]*.cmp.bmp"):
		bmpCmpPath = pjoin(refdir, file)
		bmpRefPath = bmpCmpPath[:-8] + ".ref.bmp"
		if not os.path.isfile(bmpRefPath):
			os.rename(bmpCmpPath, bmpRefPath)

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
		EngineDumpExe = pjoin(os.path.split(__file__)[0], "..", "obj-dbg", "EngineDump.exe")
	
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
