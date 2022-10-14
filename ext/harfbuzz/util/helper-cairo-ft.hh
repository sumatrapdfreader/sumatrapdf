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

#ifndef HELPER_CAIRO_FT_HH
#define HELPER_CAIRO_FT_HH

#include "font-options.hh"

#include <cairo-ft.h>
#include <hb-ft.h>
#include FT_MULTIPLE_MASTERS_H

static FT_Library ft_library;

#ifdef HAVE_ATEXIT
static inline
void free_ft_library ()
{
  FT_Done_FreeType (ft_library);
}
#endif

static inline cairo_font_face_t *
helper_cairo_create_ft_font_face (const font_options_t *font_opts)
{
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
#if !defined(HB_NO_VAR) && defined(HAVE_FT_SET_VAR_BLEND_COORDINATES)
    unsigned int num_coords;
    const float *coords = hb_font_get_var_coords_design (font_opts->font, &num_coords);
    if (num_coords)
    {
      FT_Fixed *ft_coords = (FT_Fixed *) calloc (num_coords, sizeof (FT_Fixed));
      if (ft_coords)
      {
	for (unsigned int i = 0; i < num_coords; i++)
	  ft_coords[i] = coords[i] * 65536.f;
	FT_Set_Var_Design_Coordinates (ft_face, num_coords, ft_coords);
	free (ft_coords);
      }
    }
#endif

    cairo_face = cairo_ft_font_face_create_for_ft_face (ft_face, font_opts->ft_load_flags);
  }
  return cairo_face;
}

static inline bool
helper_cairo_ft_scaled_font_has_color (cairo_scaled_font_t *scaled_font)
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

#endif
