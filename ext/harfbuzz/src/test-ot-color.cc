/*
 * Copyright © 2018  Ebrahim Byagowi
 * Copyright © 2018  Khaled Hosny
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
 */

#include "hb.hh"

#include <cairo.h>

#ifdef HB_NO_OPEN
#define hb_blob_create_from_file(x)  hb_blob_get_empty ()
#endif

#if !defined(HB_NO_COLOR) && defined(CAIRO_HAS_SVG_SURFACE)

#include "hb-ot.h"

#include "hb-ft.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <cairo-ft.h>
#include <cairo-svg.h>

#include <stdlib.h>
#include <stdio.h>

static void
svg_dump (hb_face_t *face, unsigned int face_index)
{
  unsigned glyph_count = hb_face_get_glyph_count (face);

  for (unsigned int glyph_id = 0; glyph_id < glyph_count; glyph_id++)
  {
    hb_blob_t *blob = hb_ot_color_glyph_reference_svg (face, glyph_id);

    if (hb_blob_get_length (blob) == 0) continue;

    unsigned int length;
    const char *data = hb_blob_get_data (blob, &length);

    char output_path[255];
    sprintf (output_path, "out/svg-%u-%u.svg%s",
	     glyph_id,
	     face_index,
	     // append "z" if the content is gzipped, https://stackoverflow.com/a/6059405
	     (length > 2 && (data[0] == '\x1F') && (data[1] == '\x8B')) ? "z" : "");

    FILE *f = fopen (output_path, "wb");
    fwrite (data, 1, length, f);
    fclose (f);

    hb_blob_destroy (blob);
  }
}

/* _png API is so easy to use unlike the below code, don't get confused */
static void
png_dump (hb_face_t *face, unsigned int face_index)
{
  unsigned glyph_count = hb_face_get_glyph_count (face);
  hb_font_t *font = hb_font_create (face);

  /* scans the font for strikes */
  unsigned int sample_glyph_id;
  /* we don't care about different strikes for different glyphs at this point */
  for (sample_glyph_id = 0; sample_glyph_id < glyph_count; sample_glyph_id++)
  {
    hb_blob_t *blob = hb_ot_color_glyph_reference_png (font, sample_glyph_id);
    unsigned int blob_length = hb_blob_get_length (blob);
    hb_blob_destroy (blob);
    if (blob_length != 0)
      break;
  }

  unsigned int upem = hb_face_get_upem (face);
  unsigned int blob_length = 0;
  unsigned int strike = 0;
  for (unsigned int ppem = 1; ppem < upem; ppem++)
  {
    hb_font_set_ppem (font, ppem, ppem);
    hb_blob_t *blob = hb_ot_color_glyph_reference_png (font, sample_glyph_id);
    unsigned int new_blob_length = hb_blob_get_length (blob);
    hb_blob_destroy (blob);
    if (new_blob_length != blob_length)
    {
      for (unsigned int glyph_id = 0; glyph_id < glyph_count; glyph_id++)
      {
	hb_blob_t *blob = hb_ot_color_glyph_reference_png (font, glyph_id);

	if (hb_blob_get_length (blob) == 0) continue;

	unsigned int length;
	const char *data = hb_blob_get_data (blob, &length);

	char output_path[255];
	sprintf (output_path, "out/png-%u-%u-%u.png", glyph_id, strike, face_index);

	FILE *f = fopen (output_path, "wb");
	fwrite (data, 1, length, f);
	fclose (f);

	hb_blob_destroy (blob);
      }

      strike++;
      blob_length = new_blob_length;
    }
  }

  hb_font_destroy (font);
}

static void
layered_glyph_dump (hb_face_t *face, cairo_font_face_t *cairo_face, unsigned int face_index)
{
  unsigned int upem = hb_face_get_upem (face);

  unsigned glyph_count = hb_face_get_glyph_count (face);
  for (hb_codepoint_t gid = 0; gid < glyph_count; ++gid)
  {
    unsigned int num_layers = hb_ot_color_glyph_get_layers (face, gid, 0, nullptr, nullptr);
    if (!num_layers)
      continue;

    hb_ot_color_layer_t *layers = (hb_ot_color_layer_t*) malloc (num_layers * sizeof (hb_ot_color_layer_t));

    hb_ot_color_glyph_get_layers (face, gid, 0, &num_layers, layers);
    if (num_layers)
    {
      // Measure
      cairo_text_extents_t extents;
      {
	cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create (surface);
	cairo_set_font_face (cr, cairo_face);
	cairo_set_font_size (cr, upem);

	cairo_glyph_t *glyphs = (cairo_glyph_t *) calloc (num_layers, sizeof (cairo_glyph_t));
	for (unsigned int j = 0; j < num_layers; ++j)
	  glyphs[j].index = layers[j].glyph;
	cairo_glyph_extents (cr, glyphs, num_layers, &extents);
	free (glyphs);
	cairo_surface_destroy (surface);
	cairo_destroy (cr);
      }

      // Add a slight margin
      extents.width += extents.width / 10;
      extents.height += extents.height / 10;
      extents.x_bearing -= extents.width / 20;
      extents.y_bearing -= extents.height / 20;

      // Render
      unsigned int palette_count = hb_ot_color_palette_get_count (face);
      for (unsigned int palette = 0; palette < palette_count; palette++)
      {
	unsigned int num_colors = hb_ot_color_palette_get_colors (face, palette, 0, nullptr, nullptr);
	if (!num_colors)
	  continue;

	hb_color_t *colors = (hb_color_t*) calloc (num_colors, sizeof (hb_color_t));
	hb_ot_color_palette_get_colors (face, palette, 0, &num_colors, colors);
	if (num_colors)
	{
	  char output_path[255];
	  sprintf (output_path, "out/colr-%u-%u-%u.svg", gid, palette, face_index);

	  cairo_surface_t *surface = cairo_svg_surface_create (output_path, extents.width, extents.height);
	  cairo_t *cr = cairo_create (surface);
	  cairo_set_font_face (cr, cairo_face);
	  cairo_set_font_size (cr, upem);

	  for (unsigned int layer = 0; layer < num_layers; ++layer)
	  {
	    hb_color_t color = 0x000000FF;
	    if (layers[layer].color_index != 0xFFFF)
	      color = colors[layers[layer].color_index];
	    cairo_set_source_rgba (cr,
				   hb_color_get_red (color) / 255.,
				   hb_color_get_green (color) / 255.,
				   hb_color_get_blue (color) / 255.,
				   hb_color_get_alpha (color) / 255.);

	    cairo_glyph_t glyph;
	    glyph.index = layers[layer].glyph;
	    glyph.x = -extents.x_bearing;
	    glyph.y = -extents.y_bearing;
	    cairo_show_glyphs (cr, &glyph, 1);
	  }

	  cairo_surface_destroy (surface);
	  cairo_destroy (cr);
	}
	free (colors);
      }
    }

    free (layers);
  }
}

static void
dump_glyphs (cairo_font_face_t *cairo_face, unsigned int upem,
	     unsigned int num_glyphs, unsigned int face_index)
{
  for (unsigned int i = 0; i < num_glyphs; ++i)
  {
    cairo_text_extents_t extents;
    cairo_glyph_t glyph = {0};
    glyph.index = i;

    // Measure
    {
      cairo_surface_t *surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
      cairo_t *cr = cairo_create (surface);
      cairo_set_font_face (cr, cairo_face);
      cairo_set_font_size (cr, upem);

      cairo_glyph_extents (cr, &glyph, 1, &extents);
      cairo_surface_destroy (surface);
      cairo_destroy (cr);
    }

    // Add a slight margin
    extents.width += extents.width / 10;
    extents.height += extents.height / 10;
    extents.x_bearing -= extents.width / 20;
    extents.y_bearing -= extents.height / 20;

    // Render
    {
      char output_path[255];
      sprintf (output_path, "out/%u-%u.svg", face_index, i);
      cairo_surface_t *surface = cairo_svg_surface_create (output_path, extents.width, extents.height);
      cairo_t *cr = cairo_create (surface);
      cairo_set_font_face (cr, cairo_face);
      cairo_set_font_size (cr, upem);
      glyph.x = -extents.x_bearing;
      glyph.y = -extents.y_bearing;
      cairo_show_glyphs (cr, &glyph, 1);
      cairo_surface_destroy (surface);
      cairo_destroy (cr);
    }
  }
}

int
main (int argc, char **argv)
{
  if (argc != 2) {
    fprintf (stderr, "usage: %s font-file.ttf\n"
		     "run it like `rm -rf out && mkdir out && %s font-file.ttf`\n",
		     argv[0], argv[0]);
    exit (1);
  }


  FILE *font_name_file = fopen ("out/.dumped_font_name", "r");
  if (font_name_file != nullptr)
  {
    fprintf (stderr, "Purge or move ./out folder in order to run a new dump\n");
    exit (1);
  }

  font_name_file = fopen ("out/.dumped_font_name", "w");
  if (font_name_file == nullptr)
  {
    fprintf (stderr, "./out is not accessible as a folder, create it please\n");
    exit (1);
  }
  fwrite (argv[1], 1, strlen (argv[1]), font_name_file);
  fclose (font_name_file);

  hb_blob_t *blob = hb_blob_create_from_file (argv[1]);
  unsigned int num_faces = hb_face_count (blob);
  if (num_faces == 0)
  {
    fprintf (stderr, "error: The file (%s) was corrupted, empty or not found", argv[1]);
    exit (1);
  }

  for (unsigned int face_index = 0; face_index < hb_face_count (blob); face_index++)
  {
    hb_face_t *face = hb_face_create (blob, face_index);
    hb_font_t *font = hb_font_create (face);

    if (hb_ot_color_has_png (face)) printf ("Dumping png (cbdt/sbix)...\n");
    png_dump (face, face_index);

    if (hb_ot_color_has_svg (face)) printf ("Dumping svg...\n");
    svg_dump (face, face_index);

    cairo_font_face_t *cairo_face;
    {
      FT_Library library;
      FT_Init_FreeType (&library);
      FT_Face ft_face;
      FT_New_Face (library, argv[1], 0, &ft_face);
      cairo_face = cairo_ft_font_face_create_for_ft_face (ft_face, 0);
    }
    if (hb_ot_color_has_layers (face) && hb_ot_color_has_palettes (face))
      printf ("Dumping layered color glyphs...\n");
    layered_glyph_dump (face, cairo_face, face_index);

    unsigned int num_glyphs = hb_face_get_glyph_count (face);
    unsigned int upem = hb_face_get_upem (face);

    // disabled when color font as cairo rendering of NotoColorEmoji is soooo slow
    if (!hb_ot_color_has_layers (face) &&
	!hb_ot_color_has_png (face) &&
	!hb_ot_color_has_svg (face))
      dump_glyphs (cairo_face, upem, num_glyphs, face_index);

    hb_font_destroy (font);
    hb_face_destroy (face);
    }

  hb_blob_destroy (blob);

  return 0;
}

#else
int main (int argc, char **argv) { return 0; }
#endif
