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

#include "helper-cairo.hh"

#include <cairo-ft.h>
#include <hb-ft.h>
#include FT_MULTIPLE_MASTERS_H

#include "helper-cairo-ansi.hh"
#ifdef CAIRO_HAS_SVG_SURFACE
#  include <cairo-svg.h>
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
#  include <cairo-pdf.h>
#endif
#ifdef CAIRO_HAS_PS_SURFACE
#  include <cairo-ps.h>
#  if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1,6,0)
#    define HAS_EPS 1

static cairo_surface_t *
_cairo_eps_surface_create_for_stream (cairo_write_func_t  write_func,
				      void               *closure,
				      double              width,
				      double              height)
{
  cairo_surface_t *surface;

  surface = cairo_ps_surface_create_for_stream (write_func, closure, width, height);
  cairo_ps_surface_set_eps (surface, true);

  return surface;
}

#  else
#    undef HAS_EPS
#  endif
#endif


static FT_Library ft_library;

#ifdef HAVE_ATEXIT
static inline
void free_ft_library ()
{
  FT_Done_FreeType (ft_library);
}
#endif

cairo_scaled_font_t *
helper_cairo_create_scaled_font (const font_options_t *font_opts)
{
  hb_font_t *font = hb_font_reference (font_opts->get_font ());

  cairo_font_face_t *cairo_face;
  /* We cannot use the FT_Face from hb_font_t, as doing so will confuse hb_font_t because
   * cairo will reset the face size.  As such, create new face...
   * TODO Perhaps add API to hb-ft to encapsulate this code. */
  FT_Face ft_face = nullptr;//hb_ft_font_get_face (font);
  if (!ft_face)
  {
    if (!ft_library)
    {
      FT_Init_FreeType (&ft_library);
#ifdef HAVE_ATEXIT
      atexit (free_ft_library);
#endif
    }

    unsigned int blob_length;
    const char *blob_data = hb_blob_get_data (font_opts->blob, &blob_length);

    if (FT_New_Memory_Face (ft_library,
			    (const FT_Byte *) blob_data,
			    blob_length,
			    font_opts->face_index,
			    &ft_face))
      fail (false, "FT_New_Memory_Face fail");
  }
  if (!ft_face)
  {
    /* This allows us to get some boxes at least... */
    cairo_face = cairo_toy_font_face_create ("@cairo:sans",
					     CAIRO_FONT_SLANT_NORMAL,
					     CAIRO_FONT_WEIGHT_NORMAL);
  }
  else
  {
#ifdef HAVE_FT_SET_VAR_BLEND_COORDINATES
    unsigned int num_coords;
    const int *coords = hb_font_get_var_coords_normalized (font, &num_coords);
    if (num_coords)
    {
      FT_Fixed *ft_coords = (FT_Fixed *) calloc (num_coords, sizeof (FT_Fixed));
      if (ft_coords)
      {
	for (unsigned int i = 0; i < num_coords; i++)
	  ft_coords[i] = coords[i] << 2;
	FT_Set_Var_Blend_Coordinates (ft_face, num_coords, ft_coords);
	free (ft_coords);
      }
    }
#endif

    cairo_face = cairo_ft_font_face_create_for_ft_face (ft_face, font_opts->ft_load_flags);
  }
  cairo_matrix_t ctm, font_matrix;
  cairo_font_options_t *font_options;

  cairo_matrix_init_identity (&ctm);
  cairo_matrix_init_scale (&font_matrix,
			   font_opts->font_size_x,
			   font_opts->font_size_y);
  font_options = cairo_font_options_create ();
  cairo_font_options_set_hint_style (font_options, CAIRO_HINT_STYLE_NONE);
  cairo_font_options_set_hint_metrics (font_options, CAIRO_HINT_METRICS_OFF);

  cairo_scaled_font_t *scaled_font = cairo_scaled_font_create (cairo_face,
							       &font_matrix,
							       &ctm,
							       font_options);

  cairo_font_options_destroy (font_options);
  cairo_font_face_destroy (cairo_face);

  static cairo_user_data_key_t key;
  if (cairo_scaled_font_set_user_data (scaled_font,
				       &key,
				       (void *) font,
				       (cairo_destroy_func_t) hb_font_destroy))
    hb_font_destroy (font);

  return scaled_font;
}

bool
helper_cairo_scaled_font_has_color (cairo_scaled_font_t *scaled_font)
{
  bool ret = false;
#ifdef FT_HAS_COLOR
  FT_Face ft_face = cairo_ft_scaled_font_lock_face (scaled_font);
  if (ft_face)
  {
    if (FT_HAS_COLOR (ft_face))
      ret = true;
    cairo_ft_scaled_font_unlock_face (scaled_font);
  }
#endif
  return ret;
}


struct finalize_closure_t {
  void (*callback)(finalize_closure_t *);
  cairo_surface_t *surface;
  cairo_write_func_t write_func;
  void *closure;
};
static cairo_user_data_key_t finalize_closure_key;


static void
finalize_ansi (finalize_closure_t *closure)
{
  cairo_status_t status;
  status = helper_cairo_surface_write_to_ansi_stream (closure->surface,
						      closure->write_func,
						      closure->closure);
  if (status != CAIRO_STATUS_SUCCESS)
    fail (false, "Failed to write output: %s",
	  cairo_status_to_string (status));
}

static cairo_surface_t *
_cairo_ansi_surface_create_for_stream (cairo_write_func_t write_func,
				       void *closure,
				       double width,
				       double height,
				       cairo_content_t content)
{
  cairo_surface_t *surface;
  int w = ceil (width);
  int h = ceil (height);

  switch (content) {
    case CAIRO_CONTENT_ALPHA:
      surface = cairo_image_surface_create (CAIRO_FORMAT_A8, w, h);
      break;
    default:
    case CAIRO_CONTENT_COLOR:
      surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
      break;
    case CAIRO_CONTENT_COLOR_ALPHA:
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
      break;
  }
  cairo_status_t status = cairo_surface_status (surface);
  if (status != CAIRO_STATUS_SUCCESS)
    fail (false, "Failed to create cairo surface: %s",
	  cairo_status_to_string (status));

  finalize_closure_t *ansi_closure = g_new0 (finalize_closure_t, 1);
  ansi_closure->callback = finalize_ansi;
  ansi_closure->surface = surface;
  ansi_closure->write_func = write_func;
  ansi_closure->closure = closure;

  if (cairo_surface_set_user_data (surface,
				   &finalize_closure_key,
				   (void *) ansi_closure,
				   (cairo_destroy_func_t) g_free))
    g_free ((void *) closure);

  return surface;
}


#ifdef CAIRO_HAS_PNG_FUNCTIONS

static void
finalize_png (finalize_closure_t *closure)
{
  cairo_status_t status;
  status = cairo_surface_write_to_png_stream (closure->surface,
					      closure->write_func,
					      closure->closure);
  if (status != CAIRO_STATUS_SUCCESS)
    fail (false, "Failed to write output: %s",
	  cairo_status_to_string (status));
}

static cairo_surface_t *
_cairo_png_surface_create_for_stream (cairo_write_func_t write_func,
				      void *closure,
				      double width,
				      double height,
				      cairo_content_t content)
{
  cairo_surface_t *surface;
  int w = ceil (width);
  int h = ceil (height);

  switch (content) {
    case CAIRO_CONTENT_ALPHA:
      surface = cairo_image_surface_create (CAIRO_FORMAT_A8, w, h);
      break;
    default:
    case CAIRO_CONTENT_COLOR:
      surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, w, h);
      break;
    case CAIRO_CONTENT_COLOR_ALPHA:
      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, w, h);
      break;
  }
  cairo_status_t status = cairo_surface_status (surface);
  if (status != CAIRO_STATUS_SUCCESS)
    fail (false, "Failed to create cairo surface: %s",
	  cairo_status_to_string (status));

  finalize_closure_t *png_closure = g_new0 (finalize_closure_t, 1);
  png_closure->callback = finalize_png;
  png_closure->surface = surface;
  png_closure->write_func = write_func;
  png_closure->closure = closure;

  if (cairo_surface_set_user_data (surface,
				   &finalize_closure_key,
				   (void *) png_closure,
				   (cairo_destroy_func_t) g_free))
    g_free ((void *) closure);

  return surface;
}

#endif

static cairo_status_t
stdio_write_func (void                *closure,
		  const unsigned char *data,
		  unsigned int         size)
{
  FILE *fp = (FILE *) closure;

  while (size) {
    size_t ret = fwrite (data, 1, size, fp);
    size -= ret;
    data += ret;
    if (size && ferror (fp))
      fail (false, "Failed to write output: %s", strerror (errno));
  }

  return CAIRO_STATUS_SUCCESS;
}

const char *helper_cairo_supported_formats[] =
{
  "ansi",
  #ifdef CAIRO_HAS_PNG_FUNCTIONS
  "png",
  #endif
  #ifdef CAIRO_HAS_SVG_SURFACE
  "svg",
  #endif
  #ifdef CAIRO_HAS_PDF_SURFACE
  "pdf",
  #endif
  #ifdef CAIRO_HAS_PS_SURFACE
  "ps",
   #ifdef HAS_EPS
    "eps",
   #endif
  #endif
  nullptr
};

cairo_t *
helper_cairo_create_context (double w, double h,
			     view_options_t *view_opts,
			     output_options_t *out_opts,
			     cairo_content_t content)
{
  cairo_surface_t *(*constructor) (cairo_write_func_t write_func,
				   void *closure,
				   double width,
				   double height) = nullptr;
  cairo_surface_t *(*constructor2) (cairo_write_func_t write_func,
				    void *closure,
				    double width,
				    double height,
				    cairo_content_t content) = nullptr;

  const char *extension = out_opts->output_format;
  if (!extension) {
#if HAVE_ISATTY
    if (isatty (fileno (out_opts->get_file_handle ())))
      extension = "ansi";
    else
#endif
    {
#ifdef CAIRO_HAS_PNG_FUNCTIONS
      extension = "png";
#else
      extension = "ansi";
#endif
    }
  }
  if (0)
    ;
    else if (0 == g_ascii_strcasecmp (extension, "ansi"))
      constructor2 = _cairo_ansi_surface_create_for_stream;
  #ifdef CAIRO_HAS_PNG_FUNCTIONS
    else if (0 == g_ascii_strcasecmp (extension, "png"))
      constructor2 = _cairo_png_surface_create_for_stream;
  #endif
  #ifdef CAIRO_HAS_SVG_SURFACE
    else if (0 == g_ascii_strcasecmp (extension, "svg"))
      constructor = cairo_svg_surface_create_for_stream;
  #endif
  #ifdef CAIRO_HAS_PDF_SURFACE
    else if (0 == g_ascii_strcasecmp (extension, "pdf"))
      constructor = cairo_pdf_surface_create_for_stream;
  #endif
  #ifdef CAIRO_HAS_PS_SURFACE
    else if (0 == g_ascii_strcasecmp (extension, "ps"))
      constructor = cairo_ps_surface_create_for_stream;
   #ifdef HAS_EPS
    else if (0 == g_ascii_strcasecmp (extension, "eps"))
      constructor = _cairo_eps_surface_create_for_stream;
   #endif
  #endif


  unsigned int fr, fg, fb, fa, br, bg, bb, ba;
  const char *color;
  br = bg = bb = 0; ba = 255;
  color = view_opts->back ? view_opts->back : DEFAULT_BACK;
  sscanf (color + (*color=='#'), "%2x%2x%2x%2x", &br, &bg, &bb, &ba);
  fr = fg = fb = 0; fa = 255;
  color = view_opts->fore ? view_opts->fore : DEFAULT_FORE;
  sscanf (color + (*color=='#'), "%2x%2x%2x%2x", &fr, &fg, &fb, &fa);

  if (content == CAIRO_CONTENT_ALPHA)
  {
    if (view_opts->annotate ||
	br != bg || bg != bb ||
	fr != fg || fg != fb)
      content = CAIRO_CONTENT_COLOR;
  }
  if (ba != 255)
    content = CAIRO_CONTENT_COLOR_ALPHA;

  cairo_surface_t *surface;
  FILE *f = out_opts->get_file_handle ();
  if (constructor)
    surface = constructor (stdio_write_func, f, w, h);
  else if (constructor2)
    surface = constructor2 (stdio_write_func, f, w, h, content);
  else
    fail (false, "Unknown output format `%s'; supported formats are: %s%s",
	  extension,
	  g_strjoinv ("/", const_cast<char**> (helper_cairo_supported_formats)),
	  out_opts->explicit_output_format ? "" :
	  "\nTry setting format using --output-format");

  cairo_t *cr = cairo_create (surface);
  content = cairo_surface_get_content (surface);

  switch (content) {
    case CAIRO_CONTENT_ALPHA:
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, 1., 1., 1., br / 255.);
      cairo_paint (cr);
      cairo_set_source_rgba (cr, 1., 1., 1.,
			     (fr / 255.) * (fa / 255.) + (br / 255) * (1 - (fa / 255.)));
      break;
    default:
    case CAIRO_CONTENT_COLOR:
    case CAIRO_CONTENT_COLOR_ALPHA:
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba (cr, br / 255., bg / 255., bb / 255., ba / 255.);
      cairo_paint (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_set_source_rgba (cr, fr / 255., fg / 255., fb / 255., fa / 255.);
      break;
  }

  cairo_surface_destroy (surface);
  return cr;
}

void
helper_cairo_destroy_context (cairo_t *cr)
{
  finalize_closure_t *closure = (finalize_closure_t *)
				cairo_surface_get_user_data (cairo_get_target (cr),
							     &finalize_closure_key);
  if (closure)
    closure->callback (closure);

  cairo_status_t status = cairo_status (cr);
  if (status != CAIRO_STATUS_SUCCESS)
    fail (false, "Failed: %s",
	  cairo_status_to_string (status));
  cairo_destroy (cr);
}


void
helper_cairo_line_from_buffer (helper_cairo_line_t *l,
			       hb_buffer_t         *buffer,
			       const char          *text,
			       unsigned int         text_len,
			       int                  scale_bits,
			       hb_bool_t            utf8_clusters)
{
  memset (l, 0, sizeof (*l));

  l->num_glyphs = hb_buffer_get_length (buffer);
  hb_glyph_info_t *hb_glyph = hb_buffer_get_glyph_infos (buffer, nullptr);
  hb_glyph_position_t *hb_position = hb_buffer_get_glyph_positions (buffer, nullptr);
  l->glyphs = cairo_glyph_allocate (l->num_glyphs + 1);

  if (text) {
    l->utf8 = g_strndup (text, text_len);
    l->utf8_len = text_len;
    l->num_clusters = l->num_glyphs ? 1 : 0;
    for (unsigned int i = 1; i < l->num_glyphs; i++)
      if (hb_glyph[i].cluster != hb_glyph[i-1].cluster)
	l->num_clusters++;
    l->clusters = cairo_text_cluster_allocate (l->num_clusters);
  }

  if ((l->num_glyphs && !l->glyphs) ||
      (l->utf8_len && !l->utf8) ||
      (l->num_clusters && !l->clusters))
  {
    l->finish ();
    return;
  }

  hb_position_t x = 0, y = 0;
  int i;
  for (i = 0; i < (int) l->num_glyphs; i++)
  {
    l->glyphs[i].index = hb_glyph[i].codepoint;
    l->glyphs[i].x = scalbn ((double)  hb_position->x_offset + x, scale_bits);
    l->glyphs[i].y = scalbn ((double) -hb_position->y_offset + y, scale_bits);
    x +=  hb_position->x_advance;
    y += -hb_position->y_advance;

    hb_position++;
  }
  l->glyphs[i].index = -1;
  l->glyphs[i].x = scalbn ((double) x, scale_bits);
  l->glyphs[i].y = scalbn ((double) y, scale_bits);

  if (l->num_clusters) {
    memset ((void *) l->clusters, 0, l->num_clusters * sizeof (l->clusters[0]));
    hb_bool_t backward = HB_DIRECTION_IS_BACKWARD (hb_buffer_get_direction (buffer));
    l->cluster_flags = backward ? CAIRO_TEXT_CLUSTER_FLAG_BACKWARD : (cairo_text_cluster_flags_t) 0;
    unsigned int cluster = 0;
    const char *start = l->utf8, *end;
    l->clusters[cluster].num_glyphs++;
    if (backward) {
      for (i = l->num_glyphs - 2; i >= 0; i--) {
	if (hb_glyph[i].cluster != hb_glyph[i+1].cluster) {
	  g_assert (hb_glyph[i].cluster > hb_glyph[i+1].cluster);
	  if (utf8_clusters)
	    end = start + hb_glyph[i].cluster - hb_glyph[i+1].cluster;
	  else
	    end = g_utf8_offset_to_pointer (start, hb_glyph[i].cluster - hb_glyph[i+1].cluster);
	  l->clusters[cluster].num_bytes = end - start;
	  start = end;
	  cluster++;
	}
	l->clusters[cluster].num_glyphs++;
      }
      l->clusters[cluster].num_bytes = l->utf8 + text_len - start;
    } else {
      for (i = 1; i < (int) l->num_glyphs; i++) {
	if (hb_glyph[i].cluster != hb_glyph[i-1].cluster) {
	  g_assert (hb_glyph[i].cluster > hb_glyph[i-1].cluster);
	  if (utf8_clusters)
	    end = start + hb_glyph[i].cluster - hb_glyph[i-1].cluster;
	  else
	    end = g_utf8_offset_to_pointer (start, hb_glyph[i].cluster - hb_glyph[i-1].cluster);
	  l->clusters[cluster].num_bytes = end - start;
	  start = end;
	  cluster++;
	}
	l->clusters[cluster].num_glyphs++;
      }
      l->clusters[cluster].num_bytes = l->utf8 + text_len - start;
    }
  }
}
