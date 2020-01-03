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

#ifndef HELPER_CAIRO_HH
#define HELPER_CAIRO_HH

#include "hb.hh"
#include "options.hh"

#include <cairo.h>


cairo_scaled_font_t *
helper_cairo_create_scaled_font (const font_options_t *font_opts);

bool
helper_cairo_scaled_font_has_color (cairo_scaled_font_t *scaled_font);

extern const char *helper_cairo_supported_formats[];

cairo_t *
helper_cairo_create_context (double w, double h,
			     view_options_t *view_opts,
			     output_options_t *out_opts,
			     cairo_content_t content);

void
helper_cairo_destroy_context (cairo_t *cr);


struct helper_cairo_line_t {
  cairo_glyph_t *glyphs;
  unsigned int num_glyphs;
  char *utf8;
  unsigned int utf8_len;
  cairo_text_cluster_t *clusters;
  unsigned int num_clusters;
  cairo_text_cluster_flags_t cluster_flags;

  void finish () {
    if (glyphs)
      cairo_glyph_free (glyphs);
    if (clusters)
      cairo_text_cluster_free (clusters);
    if (utf8)
      g_free (utf8);
  }

  void get_advance (double *x_advance, double *y_advance) {
    *x_advance = glyphs[num_glyphs].x;
    *y_advance = glyphs[num_glyphs].y;
  }
};

void
helper_cairo_line_from_buffer (helper_cairo_line_t *l,
			       hb_buffer_t         *buffer,
			       const char          *text,
			       unsigned int         text_len,
			       int                  scale_bits,
			       hb_bool_t            utf8_clusters);

#endif
