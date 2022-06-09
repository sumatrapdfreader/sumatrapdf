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

#include "view-options.hh"
#include "output-options.hh"
#include "helper-cairo.hh"

struct view_cairo_t : view_options_t, output_options_t<>
{
  ~view_cairo_t ()
  {
    cairo_debug_reset_static_data ();
  }

  void add_options (option_parser_t *parser)
  {
    parser->set_summary ("View text with given font.");
    view_options_t::add_options (parser);
    output_options_t::add_options (parser, helper_cairo_supported_formats);
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

  void render (const font_options_t *font_opts);

  hb_direction_t direction = HB_DIRECTION_INVALID; // Remove this, make segment_properties accessible
  GArray *lines = nullptr;
  int scale_bits = 0;
};

inline void
view_cairo_t::render (const font_options_t *font_opts)
{
  bool vertical = HB_DIRECTION_IS_VERTICAL (direction);
  int vert  = vertical ? 1 : 0;
  int horiz = vertical ? 0 : 1;

  int x_sign = font_opts->font_size_x < 0 ? -1 : +1;
  int y_sign = font_opts->font_size_y < 0 ? -1 : +1;

  hb_font_t *font = font_opts->font;

  if (!have_font_extents)
  {
    hb_font_extents_t hb_extents;
    hb_font_get_extents_for_direction (font, direction, &hb_extents);
    font_extents.ascent = scalbn ((double) hb_extents.ascender, scale_bits);
    font_extents.descent = -scalbn ((double) hb_extents.descender, scale_bits);
    font_extents.line_gap = scalbn ((double) hb_extents.line_gap, scale_bits);
    have_font_extents = true;
  }

  double ascent = y_sign * font_extents.ascent;
  double descent = y_sign * font_extents.descent;
  double line_gap = y_sign * font_extents.line_gap + line_space;
  double leading = ascent + descent + line_gap;

  /* Calculate surface size. */
  double w = 0, h = 0;
  (vertical ? w : h) = (int) lines->len * leading - (font_extents.line_gap + line_space);
  (vertical ? h : w) = 0;
  for (unsigned int i = 0; i < lines->len; i++) {
    helper_cairo_line_t &line = g_array_index (lines, helper_cairo_line_t, i);
    double x_advance, y_advance;
    line.get_advance (&x_advance, &y_advance);
    if (vertical)
      h =  MAX (h, y_sign * y_advance);
    else
      w =  MAX (w, x_sign * x_advance);
  }

  cairo_scaled_font_t *scaled_font = helper_cairo_create_scaled_font (font_opts);

  /* See if font needs color. */
  cairo_content_t content = CAIRO_CONTENT_ALPHA;
  if (helper_cairo_scaled_font_has_color (scaled_font))
    content = CAIRO_CONTENT_COLOR;

  /* Create surface. */
  cairo_t *cr = helper_cairo_create_context (w + margin.l + margin.r,
					     h + margin.t + margin.b,
					     this,
					     this,
					     content);
  cairo_set_scaled_font (cr, scaled_font);

  /* Setup coordinate system. */
  cairo_translate (cr, margin.l, margin.t);
  if (vertical)
    cairo_translate (cr,
		     w - ascent, /* We currently always stack lines right to left */
		     y_sign < 0 ? h : 0);
  else
   {
    cairo_translate (cr,
		     x_sign < 0 ? w : 0,
		     y_sign < 0 ? descent : ascent);
   }

  /* Draw. */
  cairo_translate (cr, +vert * leading, -horiz * leading);
  for (unsigned int i = 0; i < lines->len; i++)
  {
    helper_cairo_line_t &l = g_array_index (lines, helper_cairo_line_t, i);

    cairo_translate (cr, -vert * leading, +horiz * leading);

    if (annotate) {
      cairo_save (cr);

      /* Draw actual glyph origins */
      cairo_set_source_rgba (cr, 1., 0., 0., .5);
      cairo_set_line_width (cr, 5);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
      for (unsigned i = 0; i < l.num_glyphs; i++) {
	cairo_move_to (cr, l.glyphs[i].x, l.glyphs[i].y);
	cairo_rel_line_to (cr, 0, 0);
      }
      cairo_stroke (cr);

      cairo_restore (cr);
    }

    if (0 && cairo_surface_get_type (cairo_get_target (cr)) == CAIRO_SURFACE_TYPE_IMAGE) {
      /* cairo_show_glyphs() doesn't support subpixel positioning */
      cairo_glyph_path (cr, l.glyphs, l.num_glyphs);
      cairo_fill (cr);
    } else if (l.num_clusters)
      cairo_show_text_glyphs (cr,
			      l.utf8, l.utf8_len,
			      l.glyphs, l.num_glyphs,
			      l.clusters, l.num_clusters,
			      l.cluster_flags);
    else
      cairo_show_glyphs (cr, l.glyphs, l.num_glyphs);
  }

  /* Clean up. */
  helper_cairo_destroy_context (cr);
  cairo_scaled_font_destroy (scaled_font);
}

#endif
