#!/usr/bin/python

import sys

agl = []
agltab = []
aglmap = {}

print "/*"

f = open("glyphlist.txt", "r")
for line in f.readlines():
	if line[0] == '#':
		print line.strip()
		continue
	line = line[:-1]
	name, list = line.split(';')
	list = map(lambda x: int(x, 16), list.split(' '))
	agl.append((name, list))

for name, ucslist in agl:
	num = len(ucslist)
	ucs = ucslist[0]
	agltab.append((name, ucs))
	if ucs not in aglmap:
		aglmap[ucs] = []
	aglmap[ucs].append(name)

print "*/"
print

def dumplist(list):
	n = 0;
	for item in list:
		n += len(item) + 1
		if n > 78:
			sys.stdout.write("\n")
			n = len(item) + 1
		sys.stdout.write(item)
		sys.stdout.write(",")
	sys.stdout.write("\n")

agltab.sort()
namelist = []
codelist = []
for name, ucs in agltab:
	namelist.append("\"%s\"" % name)
	codelist.append("%d" % ucs)

keys = aglmap.keys()
keys.sort()
dupoffsets = []
dupnames = []
for ucs in keys:
	list = aglmap[ucs]
	ofs = len(dupnames)
	if len(list) > 1:
		dupoffsets.append("%d,%d" % (ucs, ofs))
		for name in list:
			dupnames.append("\"%s\"" % name)
		dupnames.append("0")

print "static const char *agl_name_list[] = {"
dumplist(namelist)
print "};"
print
print "static const unsigned short agl_code_list[] = {"
dumplist(codelist)
print "};"
print
print "static const unsigned short agl_dup_offsets[] = {"
dumplist(dupoffsets)
print "};"
print
print "static const char *agl_dup_names[] = {"
dumplist(dupnames)
print "};"
