/*
 * Copyright © 2014  Google, Inc.
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

#include <stdlib.h>
#include <stdio.h>

#include "hb-fc.h"

static hb_bool_t
hb_fc_get_glyph (hb_font_t *font /*HB_UNUSED*/,
		 void *font_data,
		 hb_codepoint_t unicode,
		 hb_codepoint_t variation_selector,
		 hb_codepoint_t *glyph,
		 void *user_data /*HB_UNUSED*/)

{
  FcCharSet *cs = (FcCharSet *) font_data;

  if (variation_selector)
  {
    /* Fontconfig doesn't cache cmap-14 info.  However:
     * 1. If the font maps the variation_selector, assume it's
     *    supported,
     * 2. If the font doesn't map it, still say it's supported,
     *    but return 0.  This way, the caller will see the zero
     *    and reject.  If we return unsupported here, then the
     *    variation selector will be hidden and ignored.
     */
    if (FcCharSetHasChar (cs, unicode) &&
	FcCharSetHasChar (cs, variation_selector))
    {
      unsigned int var_num = 0;
      if (variation_selector - 0xFE00u < 16)
	var_num = variation_selector - 0xFE00 + 1;
      else if (variation_selector - 0xE0100u < (256 - 16))
	var_num = variation_selector - 0xE0100 + 17;
      *glyph = (var_num << 21) | unicode;
    }
    else
    {
      *glyph = 0;
    }
    return true;
  }

  *glyph = FcCharSetHasChar (cs, unicode) ? unicode : 0;
  return *glyph != 0;
}

static hb_font_funcs_t *
_hb_fc_get_font_funcs ()
{
  static const hb_font_funcs_t *fc_ffuncs;

  const hb_font_funcs_t *ffuncs;

  if (!(ffuncs = fc_ffuncs))
  {
    hb_font_funcs_t *newfuncs = hb_font_funcs_create ();

    hb_font_funcs_set_glyph_func (newfuncs, hb_fc_get_glyph, nullptr, nullptr);

    /* XXX MT-unsafe */
    if (fc_ffuncs)
      hb_font_funcs_destroy (newfuncs);
    else
      fc_ffuncs = ffuncs = newfuncs;
  }

  return const_cast<hb_font_funcs_t *> (fc_ffuncs);
}


hb_font_t *
hb_fc_font_create (FcPattern *fcfont)
{
  static hb_face_t *face;
  hb_font_t *font;

  FcCharSet *cs;
  if (FcResultMatch != FcPatternGetCharSet (fcfont, FC_CHARSET, 0, &cs))
    return hb_font_get_empty ();

  if (!face) /* XXX MT-unsafe */
    face = hb_face_create (hb_blob_get_empty (), 0);

  font = hb_font_create (face);

  hb_font_set_funcs (font,
		     _hb_fc_get_font_funcs (),
		     FcCharSetCopy (cs),
		     (hb_destroy_func_t) FcCharSetDestroy);

  return font;
}

hb_bool_t
hb_fc_can_render (hb_font_t *font, const char *text)
{
  static const char *ot[] = {"ot", nullptr};

  hb_buffer_t *buffer = hb_buffer_create ();
  hb_buffer_add_utf8 (buffer, text, -1, 0, -1);

  /* XXX Do we need this?  I think Arabic and Hangul shapers are the
   * only one that make any use of this.  The Hangul case is not really
   * needed, and for Arabic we'll miss a very narrow set of fonts.
   * Might be better to force generic shaper perhaps. */
  hb_buffer_guess_segment_properties (buffer);

  if (!hb_shape_full (font, buffer, nullptr, 0, ot))
    abort (); /* hb-ot shaper not enabled? */

  unsigned int len;
  hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &len);
  for (unsigned int i = 0; i < len; i++)
   {
    if (!info[i].codepoint)
     {
      return false;
     }
   }

  return true;
}
