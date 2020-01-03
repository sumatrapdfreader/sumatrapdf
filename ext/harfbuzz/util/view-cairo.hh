/*
 * Copyright © 2011  Google, Inc.
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

#ifndef VIEW_CAIRO_HH
#define VIEW_CAIRO_HH

#include "hb.hh"
#include "options.hh"
#include "helper-cairo.hh"


struct view_cairo_t
{
  view_cairo_t (option_parser_t *parser)
	       : output_options (parser, helper_cairo_supported_formats),
		 view_options (parser),
		 direction (HB_DIRECTION_INVALID),
		 lines (0), scale_bits (0) {}
  ~view_cairo_t () {
    cairo_debug_reset_static_data ();
  }

  void init (hb_buffer_t *buffer, const font_options_t *font_opts)
  {
    lines = g_array_new (false, false, sizeof (helper_cairo_line_t));
    scale_bits = - (int) font_opts->subpixel_bits;
  }
  void new_line () {}
  void consume_text (hb_buffer_t  *buffer,
		     const char   *text,
		     unsigned int  text_len,
		     hb_bool_t     utf8_clusters) {}
  void error (const char *message)
  { g_printerr ("%s: %s\n", g_get_prgname (), message); }
  void consume_glyphs (hb_buffer_t  *buffer,
		       const char   *text,
		       unsigned int  text_len,
		       hb_bool_t     utf8_clusters)
  {
    direction = hb_buffer_get_direction (buffer);
    helper_cairo_line_t l;
    helper_cairo_line_from_buffer (&l, buffer, text, text_len, scale_bits, utf8_clusters);
    g_array_append_val (lines, l);
  }
  void finish (hb_buffer_t *buffer, const font_options_t *font_opts)
  {
    render (font_opts);

    for (unsigned int i = 0; i < lines->len; i++) {
      helper_cairo_line_t &line = g_array_index (lines, helper_cairo_line_t, i);
      line.finish ();
    }
#if GLIB_CHECK_VERSION (2, 22, 0)
    g_array_unref (lines);
#else
    g_array_free (lines, TRUE);
#endif
  }

  protected:

  output_options_t output_options;
  view_options_t view_options;

  void render (const font_options_t *font_opts);

  hb_direction_t direction; // Remove this, make segment_properties accessible
  GArray *lines;
  int scale_bits;
};

#endif
