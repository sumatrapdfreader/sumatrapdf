/*
 * Copyright © 2013  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_BUFFER_DESERIALIZE_TEXT_UNICODE_HH
#define HB_BUFFER_DESERIALIZE_TEXT_UNICODE_HH

#include "hb.hh"

%%{

machine deserialize_text_unicode;
alphtype unsigned char;
write data;

action clear_item {
	hb_memset (&info, 0, sizeof (info));
}

action add_item {
	buffer->add_info (info);
	if (unlikely (!buffer->successful))
	  return false;
	if (buffer->have_positions)
	  buffer->pos[buffer->len - 1] = pos;
	*end_ptr = p;
}

action tok {
	tok = p;
}

action parse_hexdigits  {if (!parse_hex (tok, p, &info.codepoint )) return false; }

action parse_cluster	{ if (!parse_uint (tok, p, &info.cluster )) return false; }

unum  = '0' | [1-9] digit*;
num	= '-'? unum;

cluster	= '=' (unum >tok %parse_cluster);

unicode = [Uu] '+'? xdigit+ >tok %parse_hexdigits;

unicode_item	=
	(
		unicode
		cluster?
		('|' | '>')
	)
	>clear_item
	%add_item
	;

unicodes = '<'? unicode_item*;

main := unicodes;

}%%

static hb_bool_t
_hb_buffer_deserialize_text_unicode (hb_buffer_t *buffer,
				     const char *buf,
				     unsigned int buf_len,
				     const char **end_ptr,
				     hb_font_t *font)
{
  const char *p = buf, *pe = buf + buf_len, *eof = pe;

  const char *tok = nullptr;
  int cs;
  hb_glyph_info_t info = {0};
  const hb_glyph_position_t pos = {0};
  %%{
    write init;
    write exec;
  }%%

  *end_ptr = p;

  return p == pe;
}

#endif /* HB_BUFFER_DESERIALIZE_TEXT_UNICODE_HH */
