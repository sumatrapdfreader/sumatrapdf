/*
 * Copyright © 2022  Google, Inc.
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

#ifndef HELPER_CAIRO_USER_HH
#define HELPER_CAIRO_USER_HH

#include "font-options.hh"

#include <cairo.h>
#include <hb.h>

#include "hb-blob.hh"

static void
move_to (hb_draw_funcs_t *dfuncs,
	 cairo_t *cr,
	 hb_draw_state_t *st,
	 float to_x, float to_y,
	 void *)
{
  cairo_move_to (cr,
		 (double) to_x, (double) to_y);
}

static void
line_to (hb_draw_funcs_t *dfuncs,
	 cairo_t *cr,
	 hb_draw_state_t *st,
	 float to_x, float to_y,
	 void *)
{
  cairo_line_to (cr,
		 (double) to_x, (double) to_y);
}

static void
cubic_to (hb_draw_funcs_t *dfuncs,
	  cairo_t *cr,
	  hb_draw_state_t *st,
	  float control1_x, float control1_y,
	  float control2_x, float control2_y,
	  float to_x, float to_y,
	  void *)
{
  cairo_curve_to (cr,
		  (double) control1_x, (double) control1_y,
		  (double) control2_x, (double) control2_y,
		  (double) to_x, (double) to_y);
}

static void
close_path (hb_draw_funcs_t *dfuncs,
	    cairo_t *cr,
	    hb_draw_state_t *st,
	    void *)
{
  cairo_close_path (cr);
}


static hb_draw_funcs_t *
get_cairo_draw_funcs ()
{
  static hb_draw_funcs_t *funcs;

  if (!funcs)
  {
    funcs = hb_draw_funcs_create ();
    hb_draw_funcs_set_move_to_func (funcs, (hb_draw_move_to_func_t) move_to, nullptr, nullptr);
    hb_draw_funcs_set_line_to_func (funcs, (hb_draw_line_to_func_t) line_to, nullptr, nullptr);
    hb_draw_funcs_set_cubic_to_func (funcs, (hb_draw_cubic_to_func_t) cubic_to, nullptr, nullptr);
    hb_draw_funcs_set_close_path_func (funcs, (hb_draw_close_path_func_t) close_path, nullptr, nullptr);
  }

  return funcs;
}

static const cairo_user_data_key_t _hb_font_cairo_user_data_key = {0};

static cairo_status_t
render_glyph (cairo_scaled_font_t  *scaled_font,
	      unsigned long         glyph,
	      cairo_t              *cr,
	      cairo_text_extents_t *extents)
{
  hb_font_t *font = (hb_font_t *) (cairo_font_face_get_user_data (cairo_scaled_font_get_font_face (scaled_font),
								  &_hb_font_cairo_user_data_key));

  hb_position_t x_scale, y_scale;
  hb_font_get_scale (font, &x_scale, &y_scale);
  cairo_scale (cr, +1./x_scale, -1./y_scale);

  hb_font_get_glyph_shape (font, glyph, get_cairo_draw_funcs (), cr);
  cairo_fill (cr);

  return CAIRO_STATUS_SUCCESS;
}

#ifdef HAVE_CAIRO_USER_FONT_FACE_SET_RENDER_COLOR_GLYPH_FUNC

#ifdef CAIRO_HAS_PNG_FUNCTIONS
static inline cairo_status_t
_hb_bytes_read_func (hb_bytes_t	*src,
		     char	*data,
		     unsigned	 length)
{
  if (unlikely (src->length < length))
    return CAIRO_STATUS_READ_ERROR;

  memcpy (data, src->arrayZ, length);
  *src += length;

  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
render_color_glyph_png (cairo_scaled_font_t  *scaled_font,
			unsigned long         glyph,
			cairo_t              *cr,
			cairo_text_extents_t *extents)
{
  hb_font_t *font = (hb_font_t *) (cairo_font_face_get_user_data (cairo_scaled_font_get_font_face (scaled_font),
								  &_hb_font_cairo_user_data_key));

  hb_blob_t *blob = hb_ot_color_glyph_reference_png (font, glyph);
  if (blob == hb_blob_get_empty ())
    return CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED;

  hb_position_t x_scale, y_scale;
  hb_font_get_scale (font, &x_scale, &y_scale);
  cairo_scale (cr, +1./x_scale, -1./y_scale);

  /* Draw PNG. */
  hb_bytes_t bytes = blob->as_bytes ();
  cairo_surface_t *surface = cairo_image_surface_create_from_png_stream ((cairo_read_func_t) _hb_bytes_read_func,
									 std::addressof (bytes));
  hb_blob_destroy (blob);

  if (unlikely (cairo_surface_status (surface)) != CAIRO_STATUS_SUCCESS)
  {
    cairo_surface_destroy (surface);
    return CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED;
  }

  int width = cairo_image_surface_get_width (surface);
  int height = cairo_image_surface_get_width (surface);

  hb_glyph_extents_t hb_extents;
  if (unlikely (!hb_font_get_glyph_extents (font, glyph, &hb_extents)))
  {
    cairo_surface_destroy (surface);
    return CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED;
  }

  cairo_pattern_t *pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  cairo_matrix_t matrix = {(double) width, 0, 0, (double) height, 0, 0};
  cairo_pattern_set_matrix (pattern, &matrix);

  cairo_translate (cr, hb_extents.x_bearing, hb_extents.y_bearing);
  cairo_scale (cr, hb_extents.width, hb_extents.height);
  cairo_set_source (cr, pattern);

  cairo_rectangle (cr, 0, 0, 1, 1);
  cairo_fill (cr);
  cairo_pattern_destroy (pattern);

  cairo_surface_destroy (surface);
  return CAIRO_STATUS_SUCCESS;
}
#endif

static cairo_status_t
render_color_glyph_layers (cairo_scaled_font_t  *scaled_font,
			   unsigned long         glyph,
			   cairo_t              *cr,
			   cairo_text_extents_t *extents)
{
  hb_font_t *font = (hb_font_t *) (cairo_font_face_get_user_data (cairo_scaled_font_get_font_face (scaled_font),
								  &_hb_font_cairo_user_data_key));
  hb_face_t *face = hb_font_get_face (font);

  unsigned count = hb_ot_color_glyph_get_layers (face, glyph, 0, nullptr, nullptr);
  if (!count)
    return CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED;

  hb_ot_color_layer_t layers[16];
  unsigned offset = 0, len;
  do {
    len = ARRAY_LENGTH (layers);
    hb_ot_color_glyph_get_layers (face, glyph,
				  offset,
				  &len,
				  layers);
    for (unsigned i = 0; i < len; i++)
    {
      hb_color_t color;
      unsigned clen = 1;
      unsigned color_index = layers[i].color_index;
      bool is_foreground = color_index == 65535;

      if (!is_foreground)
      {
	hb_ot_color_palette_get_colors (face,
					0/*palette_index*/,
					color_index/*start_offset*/,
					&clen/*color_count*/,
					&color);
	if (clen < 1)
	  continue;
      }

      cairo_save (cr);
      {
	if (!is_foreground)
	  cairo_set_source_rgba (cr,
				 hb_color_get_red (color) / 255.,
				 hb_color_get_green (color) / 255.,
				 hb_color_get_blue (color) / 255.,
				 hb_color_get_alpha (color) / 255.);

	cairo_status_t ret = render_glyph (scaled_font, layers[i].glyph, cr, extents);
	if (ret != CAIRO_STATUS_SUCCESS)
	  return ret;
      }
      cairo_restore (cr);
    }
  }
  while (len == ARRAY_LENGTH (layers));
  return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
render_color_glyph (cairo_scaled_font_t  *scaled_font,
		    unsigned long         glyph,
		    cairo_t              *cr,
		    cairo_text_extents_t *extents)
{
  cairo_status_t ret = CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED;

#ifdef CAIRO_HAS_PNG_FUNCTIONS
  ret = render_color_glyph_png (scaled_font, glyph, cr, extents);
  if (ret != CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED)
    return ret;
#endif

  ret = render_color_glyph_layers (scaled_font, glyph, cr, extents);
  if (ret != CAIRO_STATUS_USER_FONT_NOT_IMPLEMENTED)
    return ret;

  return render_glyph (scaled_font, glyph, cr, extents);
}

#endif

static inline cairo_font_face_t *
helper_cairo_create_user_font_face (const font_options_t *font_opts)
{
  cairo_font_face_t *cairo_face = cairo_user_font_face_create ();

  cairo_font_face_set_user_data (cairo_face,
				 &_hb_font_cairo_user_data_key,
				 hb_font_reference (font_opts->font),
				 (cairo_destroy_func_t) hb_font_destroy);

  cairo_user_font_face_set_render_glyph_func (cairo_face, render_glyph);
#ifdef HAVE_CAIRO_USER_FONT_FACE_SET_RENDER_COLOR_GLYPH_FUNC
  hb_face_t *face = hb_font_get_face (font_opts->font);
  if (hb_ot_color_has_png (face) || hb_ot_color_has_layers (face))
    cairo_user_font_face_set_render_color_glyph_func (cairo_face, render_color_glyph);
#endif

  return cairo_face;
}

static inline bool
helper_cairo_user_font_face_has_data (cairo_font_face_t *font_face)
{
  return cairo_font_face_get_user_data (font_face, &_hb_font_cairo_user_data_key);
}

static inline bool
helper_cairo_user_scaled_font_has_color (cairo_scaled_font_t *scaled_font)
{
  /* Ignoring SVG for now, since we cannot render it. */
  hb_font_t *font = (hb_font_t *) (cairo_font_face_get_user_data (cairo_scaled_font_get_font_face (scaled_font),
								  &_hb_font_cairo_user_data_key));
  hb_face_t *face = hb_font_get_face (font);
  return hb_ot_color_has_png (face) || hb_ot_color_has_layers (face);
}

#endif
