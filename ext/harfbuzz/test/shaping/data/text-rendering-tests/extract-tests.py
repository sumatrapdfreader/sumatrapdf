#!/usr/bin/env python

from __future__ import print_function, division, absolute_import

import sys
import xml.etree.ElementTree as ET

# Can we extract this from HTML element itself? I couldn't.
namespaces = {
	'ft': 'https://github.com/OpenType/fonttest',
	'xlink': 'http://www.w3.org/1999/xlink',
}
def ns(s):
	ns,s = s.split(':')
	return '{%s}%s' % (namespaces[ns], s)

def unistr(s):
	return ','.join('U+%04X' % ord(c) for c in s)

def glyphstr(glyphs):
	out = []
	for glyphname,x,y in glyphs:
		if x or y:
			out.append('%s@%d,%d' % (glyphname, x, y))
		else:
			out.append(glyphname)
	return '['+'|'.join(out)+']'

html = ET.fromstring(sys.stdin.read())
found = False
for elt in html.findall(".//*[@class='expected'][@ft:id]", namespaces):
	found = True
	name = elt.get(ns('ft:id'))
	text = elt.get(ns('ft:render'))
	font = elt.get(ns('ft:font'))
	vars = elt.get(ns('ft:var'), '').replace(':', '=').replace(';', ',')
	glyphs = []
	for use in elt.findall(".//use"):
		x = int(use.get('x'))
		y = int(use.get('y'))
		href = use.get(ns('xlink:href'))
		assert href[0] == '#'
		glyphname = '.'.join(href[1:].split('/')[1].split('.')[1:])
		glyphs.append((glyphname, x, y))
	opts = '--font-size=1000 --ned --remove-default-ignorables --font-funcs=ft'
	if vars:
		opts = opts + ' --variations=%s' % vars
	print ("../fonts/%s:%s:%s:%s" % (font, opts, unistr(text), glyphstr(glyphs)))

sys.exit(0 if found else 1)
