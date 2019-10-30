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

#include "hb-test.h"

/* Unit tests for hb-shape.h */

/*
 * This test provides a framework to test aspects of hb_shape() that are
 * font-independent.  Please add tests for any feature that fits that
 * description.
 */

/* TODO Make this test data-driven and add some real test data */
/* TODO Test positions too. And test non-native direction.  Test commit 2e18c6dbdfb */


static const char test_data[] = "test\0data";

static hb_position_t
glyph_h_advance_func (hb_font_t *font, void *font_data,
		      hb_codepoint_t glyph,
		      void *user_data)
{
  switch (glyph) {
  case 1: return 10;
  case 2: return 6;
  case 3: return 5;
  }
  return 0;
}

static hb_bool_t
glyph_func (hb_font_t *font, void *font_data,
	    hb_codepoint_t unicode, hb_codepoint_t variant_selector,
	    hb_codepoint_t *glyph,
	    void *user_data)
{
  switch (unicode) {
  case 'T': *glyph = 1; return TRUE;
  case 'e': *glyph = 2; return TRUE;
  case 's': *glyph = 3; return TRUE;
  }
  return FALSE;
}

static hb_position_t
glyph_h_kerning_func (hb_font_t *font, void *font_data,
		      hb_codepoint_t left, hb_codepoint_t right,
		      void *user_data)
{
  if (left == 1 && right == 2)
    return -2;

  return 0;
}

static const char TesT[] = "TesT";

static void
test_shape (void)
{
  hb_blob_t *blob;
  hb_face_t *face;
  hb_font_funcs_t *ffuncs;
  hb_font_t *font;
  hb_buffer_t *buffer;
  unsigned int len;
  hb_glyph_info_t *glyphs;
  hb_glyph_position_t *positions;

  blob = hb_blob_create (test_data, sizeof (test_data), HB_MEMORY_MODE_READONLY, NULL, NULL);
  face = hb_face_create (blob, 0);
  hb_blob_destroy (blob);
  font = hb_font_create (face);
  hb_face_destroy (face);
  hb_font_set_scale (font, 10, 10);

  ffuncs = hb_font_funcs_create ();
  hb_font_funcs_set_glyph_h_advance_func (ffuncs, glyph_h_advance_func, NULL, NULL);
  hb_font_funcs_set_glyph_func (ffuncs, glyph_func, malloc (10), free);
  hb_font_funcs_set_glyph_h_kerning_func (ffuncs, glyph_h_kerning_func, NULL, NULL);
  hb_font_set_funcs (font, ffuncs, NULL, NULL);
  hb_font_funcs_destroy (ffuncs);

  buffer =  hb_buffer_create ();
  hb_buffer_set_direction (buffer, HB_DIRECTION_LTR);
  hb_buffer_add_utf8 (buffer, TesT, 4, 0, 4);

  hb_shape (font, buffer, NULL, 0);

  len = hb_buffer_get_length (buffer);
  glyphs = hb_buffer_get_glyph_infos (buffer, NULL);
  positions = hb_buffer_get_glyph_positions (buffer, NULL);

  {
    const hb_codepoint_t output_glyphs[] = {1, 2, 3, 1};
    const hb_position_t output_x_advances[] = {9, 5, 5, 10};
    const hb_position_t output_x_offsets[] = {0, -1, 0, 0};
    unsigned int i;
    g_assert_cmpint (len, ==, 4);
    for (i = 0; i < len; i++) {
      g_assert_cmphex (glyphs[i].codepoint, ==, output_glyphs[i]);
      g_assert_cmphex (glyphs[i].cluster,   ==, i);
    }
    for (i = 0; i < len; i++) {
      g_assert_cmpint (output_x_advances[i], ==, positions[i].x_advance);
      g_assert_cmpint (output_x_offsets [i], ==, positions[i].x_offset);
      g_assert_cmpint (0, ==, positions[i].y_advance);
      g_assert_cmpint (0, ==, positions[i].y_offset);
    }
  }

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
}

static void
test_shape_clusters (void)
{
  hb_face_t *face;
  hb_font_t *font;
  hb_buffer_t *buffer;
  unsigned int len;
  hb_glyph_info_t *glyphs;

  face = hb_face_create (NULL, 0);
  font = hb_font_create (face);
  hb_face_destroy (face);

  buffer =  hb_buffer_create ();
  hb_buffer_set_direction (buffer, HB_DIRECTION_LTR);
  {
    /* https://crbug.com/497578 */
    hb_codepoint_t test[] = {0xFFF1, 0xF0B6};
    hb_buffer_add_utf32 (buffer, test, 2, 0, 2);
  }

  hb_shape (font, buffer, NULL, 0);

  len = hb_buffer_get_length (buffer);
  glyphs = hb_buffer_get_glyph_infos (buffer, NULL);

  {
    const hb_codepoint_t output_glyphs[] = {0};
    const hb_position_t output_clusters[] = {0};
    unsigned int i;
    g_assert_cmpint (len, ==, 1);
    for (i = 0; i < len; i++) {
      g_assert_cmphex (glyphs[i].codepoint, ==, output_glyphs[i]);
      g_assert_cmphex (glyphs[i].cluster,   ==, output_clusters[i]);
    }
  }

  hb_buffer_destroy (buffer);
  hb_font_destroy (font);
}


static void
test_shape_list (void)
{
  const char **shapers = hb_shape_list_shapers ();

  unsigned int i;
  for (i = 0; shapers[i]; i++)
    ;

  g_assert_cmpint (i, >, 1);
  g_assert (!strcmp (shapers[i - 1], "fallback"));
}

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_shape);
  hb_test_add (test_shape_clusters);
  /* TODO test fallback shaper */
  /* TODO test shaper_full */
  hb_test_add (test_shape_list);

  return hb_test_run();
}
