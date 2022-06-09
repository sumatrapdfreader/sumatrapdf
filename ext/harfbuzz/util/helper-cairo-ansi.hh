/*
 * Copyright © 2012  Google, Inc.
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

#ifndef HELPER_CAIRO_ANSI_HH
#define HELPER_CAIRO_ANSI_HH

#include "hb.hh"

#include <cairo.h>

#include "ansi-print.hh"

#ifdef HAVE_CHAFA
# include <chafa.h>

/* Similar to ansi-print.cc */
# define CELL_W 8
# define CELL_H (2 * CELL_W)

static void
chafa_print_image_rgb24 (const void *data, int width, int height, int stride)
{
  ChafaTermInfo *term_info;
  ChafaSymbolMap *symbol_map;
  ChafaCanvasConfig *config;
  ChafaCanvas *canvas;
  GString *gs;
  unsigned int cols = (width +  CELL_W - 1) / CELL_W;
  unsigned int rows = (height + CELL_H - 1) / CELL_H;
  gchar **environ;
  ChafaCanvasMode mode;
  ChafaPixelMode pixel_mode;

  /* Adapt to terminal; use sixels if available, and fall back to symbols
   * with as many colors as are supported */

  environ = g_get_environ ();
  term_info = chafa_term_db_detect (chafa_term_db_get_default (),
                                    environ);

  pixel_mode = CHAFA_PIXEL_MODE_SYMBOLS;

  if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_BEGIN_SIXELS))
  {
    pixel_mode = CHAFA_PIXEL_MODE_SIXELS;
    mode = CHAFA_CANVAS_MODE_TRUECOLOR;
  }
//  else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_DIRECT))
//    mode = CHAFA_CANVAS_MODE_TRUECOLOR;
  else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_256))
    mode = CHAFA_CANVAS_MODE_INDEXED_240;
  else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_SET_COLOR_FGBG_16))
    mode = CHAFA_CANVAS_MODE_INDEXED_16;
  else if (chafa_term_info_have_seq (term_info, CHAFA_TERM_SEQ_INVERT_COLORS))
    mode = CHAFA_CANVAS_MODE_FGBG_BGFG;
  else
    mode = CHAFA_CANVAS_MODE_FGBG;

  /* Create the configuration */

  symbol_map = chafa_symbol_map_new ();
  chafa_symbol_map_add_by_tags (symbol_map,
                                (ChafaSymbolTags) (CHAFA_SYMBOL_TAG_BLOCK
                                                   | CHAFA_SYMBOL_TAG_SPACE));

  config = chafa_canvas_config_new ();
  chafa_canvas_config_set_canvas_mode (config, mode);
  chafa_canvas_config_set_pixel_mode (config, pixel_mode);
  chafa_canvas_config_set_cell_geometry (config, 10, 20);
  chafa_canvas_config_set_geometry (config, cols, rows);
  chafa_canvas_config_set_symbol_map (config, symbol_map);
  chafa_canvas_config_set_color_extractor (config, CHAFA_COLOR_EXTRACTOR_MEDIAN);
  chafa_canvas_config_set_work_factor (config, 1.0f);

  /* Create canvas, draw to it and render output string */

  canvas = chafa_canvas_new (config);
  chafa_canvas_draw_all_pixels (canvas,
                                /* Cairo byte order is host native */
                                G_BYTE_ORDER == G_LITTLE_ENDIAN
                                  ? CHAFA_PIXEL_BGRA8_PREMULTIPLIED
                                  : CHAFA_PIXEL_ARGB8_PREMULTIPLIED,
                                (const guint8 *) data,
                                width,
                                height,
                                stride);
  gs = chafa_canvas_print (canvas, term_info);

  /* Print the string */

  fwrite (gs->str, sizeof (char), gs->len, stdout);

  if (pixel_mode != CHAFA_PIXEL_MODE_SIXELS)
    fputc ('\n', stdout);

  /* Free resources */

  g_string_free (gs, TRUE);
  chafa_canvas_unref (canvas);
  chafa_canvas_config_unref (config);
  chafa_symbol_map_unref (symbol_map);
  chafa_term_info_unref (term_info);
  g_strfreev (environ);
}

#endif /* HAVE_CHAFA */

static inline cairo_status_t
helper_cairo_surface_write_to_ansi_stream (cairo_surface_t	*surface,
					   cairo_write_func_t	write_func,
					   void			*closure)
{
  unsigned int width = cairo_image_surface_get_width (surface);
  unsigned int height = cairo_image_surface_get_height (surface);
  if (cairo_image_surface_get_format (surface) != CAIRO_FORMAT_RGB24) {
    cairo_surface_t *new_surface = cairo_image_surface_create (CAIRO_FORMAT_RGB24, width, height);
    cairo_t *cr = cairo_create (new_surface);
    if (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_A8) {
      cairo_set_source_rgb (cr, 0., 0., 0.);
      cairo_paint (cr);
      cairo_set_source_rgb (cr, 1., 1., 1.);
      cairo_mask_surface (cr, surface, 0, 0);
    } else {
      cairo_set_source_rgb (cr, 1., 1., 1.);
      cairo_paint (cr);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_paint (cr);
    }
    cairo_destroy (cr);
    surface = new_surface;
  } else
    cairo_surface_reference (surface);

  unsigned int stride = cairo_image_surface_get_stride (surface);
  const uint32_t *data = (uint32_t *) (void *) cairo_image_surface_get_data (surface);

  /* We don't have rows to spare on the terminal window...
   * Find the tight image top/bottom and only print in between. */

  /* Use corner color as background color. */
  uint32_t bg_color = data ? * (uint32_t *) data : 0;

  /* Drop first row while empty */
  while (height)
  {
    unsigned int i;
    for (i = 0; i < width; i++)
      if (data[i] != bg_color)
	break;
    if (i < width)
      break;
    data += stride / 4;
    height--;
  }

  /* Drop last row while empty */
  unsigned int orig_height = height;
  while (height)
  {
    const uint32_t *row = data + (height - 1) * stride / 4;
    unsigned int i;
    for (i = 0; i < width; i++)
      if (row[i] != bg_color)
	break;
    if (i < width)
      break;
    height--;
  }
  if (height < orig_height)
    height++; /* Add one last blank row for padding. */

  if (width && height)
  {
#ifdef HAVE_CHAFA
    if (true)
      chafa_print_image_rgb24 (data, width, height, stride);
    else
#endif
      ansi_print_image_rgb24 (data, width, height, stride / 4);
  }

  cairo_surface_destroy (surface);
  return CAIRO_STATUS_SUCCESS;
}


#endif
