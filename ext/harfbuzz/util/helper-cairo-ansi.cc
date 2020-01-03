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

#include "helper-cairo-ansi.hh"
#include "options.hh"

#include "ansi-print.hh"


cairo_status_t
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
    ansi_print_image_rgb24 (data, width, height, stride / 4);

  cairo_surface_destroy (surface);
  return CAIRO_STATUS_SUCCESS;
}
