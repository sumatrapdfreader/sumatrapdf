#!/usr/bin/env python3

import sys
import array
import gi
gi.require_version('HarfBuzz', '0.0')
from gi.repository import HarfBuzz as hb
from gi.repository import GLib

fontdata = open (sys.argv[1], 'rb').read ()
text = sys.argv[2]
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
class Debugger (object):
	def message (self, buf, font, msg, data, _x_what_is_this):
		print (msg)
		return True
debugger = Debugger ()
hb.buffer_set_message_func (buf, debugger.message, 1, 0)

##
## Add text to buffer
##
#
# See https://github.com/harfbuzz/harfbuzz/pull/271
#
# If you do not care about cluster values reflecting Python
# string indices, then this is quickest way to add text to
# buffer:
# hb.buffer_add_utf8 (buf, text.encode('utf-8'), 0, -1)
# Otherwise, then following handles both narrow and wide
# Python builds (the first item in the array is BOM, so we skip it):
if sys.maxunicode == 0x10FFFF:
	hb.buffer_add_utf32 (buf, array.array ('I', text.encode ('utf-32'))[1:], 0, -1)
else:
	hb.buffer_add_utf16 (buf, array.array ('H', text.encode ('utf-16'))[1:], 0, -1)


hb.buffer_guess_segment_properties (buf)

hb.shape (font, buf, [])
del font

infos = hb.buffer_get_glyph_infos (buf)
positions = hb.buffer_get_glyph_positions (buf)

for info, pos in zip (infos, positions):
	gid = info.codepoint
	cluster = info.cluster
	x_advance = pos.x_advance
	x_offset = pos.x_offset
	y_offset = pos.y_offset

	print ("gid%d=%d@%d,%d+%d" % (gid, cluster, x_advance, x_offset, y_offset))
