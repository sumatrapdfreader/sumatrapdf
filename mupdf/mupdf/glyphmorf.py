#!/usr/bin/python

import sys

agl = []
comments = []
agltab = []
aglmap = {}
aglnames = []

f = open("glyphlist.txt", "r")
for line in f.readlines():
	if line[0] == '#':
		comments.append(line.strip());
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

print "/*"
for line in comments:
	print line
print "*/"
print

agltab.sort()
print "static const struct { char *name; int ucs; }"
print "aglcodes[] = {"
for name, ucs in agltab:
	print "{\"%s\", 0x%04X}," % (name, ucs)
print "};"
print

keys = aglmap.keys()
keys.sort()
print "static const struct { int ucs; int ofs; }"
print "agldupcodes[] = {"
for ucs in keys:
	namelist = aglmap[ucs]
	ofs = len(aglnames)
	if len(namelist) > 1:
		print "{0x%04X, %d}," % (ucs, ofs)
		for name in namelist:
			aglnames.append(name)
		aglnames.append(0)
print "};"
print

print "static char *agldupnames[] = {"
for name in aglnames:
	if name:
		print ("\"%s\"," % name),
	else:
		print "0,"
print "};"
print

print """
#include "fitz.h"
#include "mupdf.h"

int pdf_lookupagl(char *name)
{
	char buf[64];
	char *p;
	int l = 0;
	int r = nelem(aglcodes) - 1;

	strlcpy(buf, name, sizeof buf);

	/* kill anything after first period and underscore */
	p = strchr(buf, '.');
	if (p) p[0] = 0;
	p = strchr(buf, '_');
	if (p) p[0] = 0;

	while (l <= r)
	{
		int m = (l + r) >> 1;
		int c = strcmp(buf, aglcodes[m].name);
		if (c < 0)
			r = m - 1;
		else if (c > 0)
			l = m + 1;
		else
			return aglcodes[m].ucs;
	}

	if (strstr(buf, "uni") == buf)
		return strtol(buf + 3, nil, 16);
	else if (strstr(buf, "u") == buf)
		return strtol(buf + 1, nil, 16);
	else if (strstr(buf, "a") == buf && strlen(buf) >= 3)
		return strtol(buf + 1, nil, 10);

	return 0;
}

static char *aglnoname[1] = { 0 };

char **pdf_lookupaglnames(int ucs)
{
	int l = 0;
	int r = nelem(agldupcodes) - 1;
	while (l <= r)
	{
		int m = (l + r) >> 1;
		if (ucs < agldupcodes[m].ucs)
			r = m - 1;
		else if (ucs > agldupcodes[m].ucs)
			l = m + 1;
		else
			return agldupnames + agldupcodes[m].ofs;
	}
	return aglnoname;
}
"""
