#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import print_function, division, absolute_import

import sys
import array
from gi.repository import HarfBuzz as hb
from gi.repository import GLib

# Python 2/3 compatibility
try:
	unicode
except NameError:
	unicode = str

def tounicode(s, encoding='utf-8'):
	if not isinstance(s, unicode):
		return s.decode(encoding)
	else:
		return s

fontdata = open (sys.argv[1], 'rb').read ()
text = tounicode(sys.argv[2])
# Need to create GLib.Bytes explicitly until this bug is fixed:
# https://bugzilla.gnome.org/show_bug.cgi?id=729541
blob = hb.glib_blob_create (GLib.Bytes.new (fontdata))
face = hb.face_create (blob, 0)
del blob
font = hb.font_create (face)
upem = hb.face_get_upem (face)
del face
hb.font_set_scale (font, upem, upem)
#hb.ft_font_set_funcs (font)
hb.ot_font_set_funcs (font)

buf = hb.buffer_create ()
class Debugger(object):
	def message (self, buf, font, msg, data, _x_what_is_this):
		print(msg)
		return True
debugger = Debugger()
hb.buffer_set_message_func (buf, debugger.message, 1, 0)

##
## Add text to buffer
##
#
# See https://github.com/harfbuzz/harfbuzz/pull/271
#
if False:
	# If you do not care about cluster values reflecting Python
	# string indices, then this is quickest way to add text to
	# buffer:
	hb.buffer_add_utf8 (buf, text.encode('utf-8'), 0, -1)
	# Otherwise, then following handles both narrow and wide
	# Python builds:
elif sys.maxunicode == 0x10FFFF:
	hb.buffer_add_utf32 (buf, array.array('I', text.encode('utf-32')), 0, -1)
else:
	hb.buffer_add_utf16 (buf, array.array('H', text.encode('utf-16')), 0, -1)


hb.buffer_guess_segment_properties (buf)

hb.shape (font, buf, [])
del font

infos = hb.buffer_get_glyph_infos (buf)
positions = hb.buffer_get_glyph_positions (buf)

for info,pos in zip(infos, positions):
	gid = info.codepoint
	cluster = info.cluster
	x_advance = pos.x_advance
	x_offset = pos.x_offset
	y_offset = pos.y_offset

	print("gid%d=%d@%d,%d+%d" % (gid, cluster, x_advance, x_offset, y_offset))
