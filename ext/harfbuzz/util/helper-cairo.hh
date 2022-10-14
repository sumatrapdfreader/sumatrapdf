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

#include "view-options.hh"
#include "output-options.hh"
#include "helper-cairo-ft.hh"
#include "helper-cairo-user.hh"

#include <cairo.h>
#include <hb.h>

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

static inline bool
helper_cairo_use_hb_draw (const font_options_t *font_opts)
{
  const char *env = getenv ("HB_DRAW");
  if (!env)
    return cairo_version () >= CAIRO_VERSION_ENCODE (1, 17, 5);
  return atoi (env);
}

static inline cairo_scaled_font_t *
helper_cairo_create_scaled_font (const font_options_t *font_opts)
{
  bool use_hb_draw = helper_cairo_use_hb_draw (font_opts);
  hb_font_t *font = hb_font_reference (font_opts->font);

  cairo_font_face_t *cairo_face;
  if (use_hb_draw)
    cairo_face = helper_cairo_create_user_font_face (font_opts);
  else
    cairo_face = helper_cairo_create_ft_font_face (font_opts);

  cairo_matrix_t ctm, font_matrix;
  cairo_font_options_t *font_options;

  cairo_matrix_init_identity (&ctm);
  cairo_matrix_init_scale (&font_matrix,
			   font_opts->font_size_x,
			   font_opts->font_size_y);
  if (use_hb_draw)
    font_matrix.xy = -font_opts->slant * font_opts->font_size_x;

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

static inline bool
helper_cairo_scaled_font_has_color (cairo_scaled_font_t *scaled_font)
{
  if (helper_cairo_user_font_face_has_data (cairo_scaled_font_get_font_face (scaled_font)))
    return helper_cairo_user_scaled_font_has_color (scaled_font);
  else
    return helper_cairo_ft_scaled_font_has_color (scaled_font);
}


enum class image_protocol_t {
  NONE = 0,
  ITERM2,
  KITTY,
};

struct finalize_closure_t {
  void (*callback)(finalize_closure_t *);
  cairo_surface_t *surface;
  cairo_write_func_t write_func;
  void *closure;
  image_protocol_t protocol;
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
				       cairo_content_t content,
				       image_protocol_t protocol HB_UNUSED)
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

static cairo_status_t
byte_array_write_func (void                *closure,
		       const unsigned char *data,
		       unsigned int         size)
{
  g_byte_array_append ((GByteArray *) closure, data, size);
  return CAIRO_STATUS_SUCCESS;
}

static void
finalize_png (finalize_closure_t *closure)
{
  cairo_status_t status;
  GByteArray *bytes = nullptr;
  GString *string;
  gchar *base64;
  size_t base64_len;

  if (closure->protocol == image_protocol_t::NONE)
  {
    status = cairo_surface_write_to_png_stream (closure->surface,
						closure->write_func,
						closure->closure);
  }
  else
  {
    bytes = g_byte_array_new ();
    status = cairo_surface_write_to_png_stream (closure->surface,
						byte_array_write_func,
						bytes);
  }

  if (status != CAIRO_STATUS_SUCCESS)
    fail (false, "Failed to write output: %s",
	  cairo_status_to_string (status));

  if (closure->protocol == image_protocol_t::NONE)
    return;

  base64 = g_base64_encode (bytes->data, bytes->len);
  base64_len = strlen (base64);

  string = g_string_new (NULL);
  if (closure->protocol == image_protocol_t::ITERM2)
  {
    /* https://iterm2.com/documentation-images.html */
    g_string_printf (string, "\033]1337;File=inline=1;size=%zu:%s\a\n",
		     base64_len, base64);
  }
  else if (closure->protocol == image_protocol_t::KITTY)
  {
#define CHUNK_SIZE 4096
    /* https://sw.kovidgoyal.net/kitty/graphics-protocol.html */
    for (size_t pos = 0; pos < base64_len; pos += CHUNK_SIZE)
    {
      size_t len = base64_len - pos;

      if (pos == 0)
	g_string_append (string, "\033_Ga=T,f=100,m=");
      else
	g_string_append (string, "\033_Gm=");

      if (len > CHUNK_SIZE)
      {
	g_string_append (string, "1;");
	g_string_append_len (string, base64 + pos, CHUNK_SIZE);
      }
      else
      {
	g_string_append (string, "0;");
	g_string_append_len (string, base64 + pos, len);
      }

      g_string_append (string, "\033\\");
    }
    g_string_append (string, "\n");
#undef CHUNK_SIZE
  }

  closure->write_func (closure->closure, (unsigned char *) string->str, string->len);

  g_byte_array_unref (bytes);
  g_free (base64);
  g_string_free (string, TRUE);
}

static cairo_surface_t *
_cairo_png_surface_create_for_stream (cairo_write_func_t write_func,
				      void *closure,
				      double width,
				      double height,
				      cairo_content_t content,
				      image_protocol_t protocol)
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
  png_closure->protocol = protocol;

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

static const char *helper_cairo_supported_formats[] =
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

template <typename view_options_t,
	 typename output_options_type>
static inline cairo_t *
helper_cairo_create_context (double w, double h,
			     view_options_t *view_opts,
			     output_options_type *out_opts,
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
				    cairo_content_t content,
				    image_protocol_t protocol) = nullptr;

  image_protocol_t protocol = image_protocol_t::NONE;
  const char *extension = out_opts->output_format;
  if (!extension) {
#if HAVE_ISATTY
    if (isatty (fileno (out_opts->out_fp)))
    {
#ifdef CAIRO_HAS_PNG_FUNCTIONS
      const char *name;
      /* https://gitlab.com/gnachman/iterm2/-/issues/7154 */
      if ((name = getenv ("LC_TERMINAL")) != nullptr &&
	  0 == g_ascii_strcasecmp (name, "iTerm2"))
      {
	extension = "png";
	protocol = image_protocol_t::ITERM2;
      }
      else if ((name = getenv ("TERM_PROGRAM")) != nullptr &&
	  0 == g_ascii_strcasecmp (name, "WezTerm"))
      {
	extension = "png";
	protocol = image_protocol_t::ITERM2;
      }
      else if ((name = getenv ("TERM")) != nullptr &&
	       0 == g_ascii_strcasecmp (name, "xterm-kitty"))
      {
	extension = "png";
	protocol = image_protocol_t::KITTY;
      }
      else
	extension = "ansi";
#else
      extension = "ansi";
#endif
    }
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
  FILE *f = out_opts->out_fp;
  if (constructor)
    surface = constructor (stdio_write_func, f, w, h);
  else if (constructor2)
    surface = constructor2 (stdio_write_func, f, w, h, content, protocol);
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

static inline void
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

static inline void
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

#endif
