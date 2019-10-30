/*
 * Copyright © 2016  Google, Inc.
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
 * Google Author(s): Sascha Brawer
 */

#include "hb-test.h"

#include <hb-ot.h>
#include <stdlib.h>
#include <stdio.h>

/* Unit tests for hb-ot-color.h */

/* Test font with the following CPAL v0 table, as TTX and manual disassembly:

  <CPAL>
    <version value="0"/>
    <numPaletteEntries value="2"/>
    <palette index="0">
      <color index="0" value="#000000FF"/>
      <color index="1" value="#66CCFFFF"/>
    </palette>
    <palette index="1">
      <color index="0" value="#000000FF"/>
      <color index="1" value="#800000FF"/>
    </palette>
  </CPAL>

   0 | 0000                           # version=0
   2 | 0002                           # numPaletteEntries=2
   4 | 0002                           # numPalettes=2
   6 | 0004                           # numColorRecords=4
   8 | 00000010                       # offsetToFirstColorRecord=16
  12 | 0000 0002                      # colorRecordIndex=[0, 2]
  16 | 000000ff ffcc66ff              # colorRecord #0, #1 (BGRA)
  24 | 000000ff 000080ff              # colorRecord #2, #3 (BGRA)
 */
static hb_face_t *cpal_v0 = NULL;

/* Test font with the following CPAL v1 table, as TTX and manual disassembly:

  <CPAL>
    <version value="1"/>
    <numPaletteEntries value="2"/>
    <palette index="0" label="257" type="2">
      <color index="0" value="#000000FF"/>
      <color index="1" value="#66CCFFFF"/>
    </palette>
    <palette index="1" label="65535" type="1">
      <color index="0" value="#000000FF"/>
      <color index="1" value="#FFCC66FF"/>
    </palette>
    <palette index="2" label="258" type="0">
      <color index="0" value="#000000FF"/>
      <color index="1" value="#800000FF"/>
    </palette>
    <paletteEntryLabels>
      <label index="0" value="65535"/>
      <label index="1" value="256"/>
    </paletteEntryLabels>
  </CPAL>

   0 | 0001                           # version=1
   2 | 0002                           # numPaletteEntries=2
   4 | 0003                           # numPalettes=3
   6 | 0006                           # numColorRecords=6
   8 | 0000001e                       # offsetToFirstColorRecord=30
  12 | 0000 0002 0004                 # colorRecordIndex=[0, 2, 4]
  18 | 00000036                       # offsetToPaletteTypeArray=54
  22 | 00000042                       # offsetToPaletteLabelArray=66
  26 | 00000048                       # offsetToPaletteEntryLabelArray=72
  30 | 000000ff ffcc66ff 000000ff     # colorRecord #0, #1, #2 (BGRA)
  42 | 66ccffff 000000ff 000080ff     # colorRecord #3, #4, #5 (BGRA)
  54 | 00000002 00000001 00000000     # paletteFlags=[2, 1, 0]
  66 | 0101 ffff 0102                 # paletteName=[257, 0xffff, 258]
  72 | ffff 0100                      # paletteEntryLabel=[0xffff, 256]
*/
static hb_face_t *cpal_v1 = NULL;


#define assert_color_rgba(colors, i, r, g, b, a) G_STMT_START {	\
  const hb_ot_color_t *_colors = (colors); \
  const size_t _i = (i); \
  const uint8_t red = (r), green = (g), blue = (b), alpha = (a); \
  if (_colors[_i].red != red) { \
    g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
				"colors[" #i "].red", _colors[_i].red, "==", red, 'x'); \
  } \
  if (_colors[_i].green != green) { \
    g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
				"colors[" #i "].green", _colors[_i].green, "==", green, 'x'); \
  } \
  if (_colors[_i].blue != blue) { \
    g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
				"colors[" #i "].blue", colors[i].blue, "==", blue, 'x'); \
  } \
  if (_colors[_i].alpha != alpha) { \
    g_assertion_message_cmpnum (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
				"colors[" #i "].alpha", _colors[_i].alpha, "==", alpha, 'x'); \
  } \
} G_STMT_END


#if 0
static void
test_hb_ot_color_get_palette_count (void)
{
  g_assert_cmpint (hb_ot_color_get_palette_count (hb_face_get_empty()), ==, 0);
  g_assert_cmpint (hb_ot_color_get_palette_count (cpal_v0), ==, 2);
  g_assert_cmpint (hb_ot_color_get_palette_count (cpal_v1), ==, 3);
}


static void
test_hb_ot_color_get_palette_name_id_empty (void)
{
  /* numPalettes=0, so all calls are for out-of-bounds palette indices */
  g_assert_cmpint (hb_ot_color_get_palette_name_id (hb_face_get_empty(), 0), ==, 0xffff);
  g_assert_cmpint (hb_ot_color_get_palette_name_id (hb_face_get_empty(), 1), ==, 0xffff);
}


static void
test_hb_ot_color_get_palette_name_id_v0 (void)
{
  g_assert_cmpint (hb_ot_color_get_palette_name_id (cpal_v0, 0), ==, 0xffff);
  g_assert_cmpint (hb_ot_color_get_palette_name_id (cpal_v0, 1), ==, 0xffff);

  /* numPalettes=2, so palette #2 is out of bounds */
  g_assert_cmpint (hb_ot_color_get_palette_name_id (cpal_v0, 2), ==, 0xffff);
}


static void
test_hb_ot_color_get_palette_name_id_v1 (void)
{
  g_assert_cmpint (hb_ot_color_get_palette_name_id (cpal_v1, 0), ==, 257);
  g_assert_cmpint (hb_ot_color_get_palette_name_id (cpal_v1, 1), ==, 0xffff);
  g_assert_cmpint (hb_ot_color_get_palette_name_id (cpal_v1, 2), ==, 258);

  /* numPalettes=3, so palette #3 is out of bounds */
  g_assert_cmpint (hb_ot_color_get_palette_name_id (cpal_v1, 3), ==, 0xffff);
}

static void
test_hb_ot_color_get_palette_flags_empty (void)
{
  /* numPalettes=0, so all calls are for out-of-bounds palette indices */
  g_assert_cmpint (hb_ot_color_get_palette_flags (hb_face_get_empty(), 0), ==, HB_OT_COLOR_PALETTE_FLAG_DEFAULT);
  g_assert_cmpint (hb_ot_color_get_palette_flags (hb_face_get_empty(), 1), ==, HB_OT_COLOR_PALETTE_FLAG_DEFAULT);
}


static void
test_hb_ot_color_get_palette_flags_v0 (void)
{
  g_assert_cmpint (hb_ot_color_get_palette_flags (cpal_v0, 0), ==, HB_OT_COLOR_PALETTE_FLAG_DEFAULT);
  g_assert_cmpint (hb_ot_color_get_palette_flags (cpal_v0, 1), ==, HB_OT_COLOR_PALETTE_FLAG_DEFAULT);

  /* numPalettes=2, so palette #2 is out of bounds */
  g_assert_cmpint (hb_ot_color_get_palette_flags (cpal_v0, 2), ==, HB_OT_COLOR_PALETTE_FLAG_DEFAULT);
}


static void
test_hb_ot_color_get_palette_flags_v1 (void)
{
  g_assert_cmpint (hb_ot_color_get_palette_flags (cpal_v1, 0), ==, HB_OT_COLOR_PALETTE_FLAG_FOR_DARK_BACKGROUND);
  g_assert_cmpint (hb_ot_color_get_palette_flags (cpal_v1, 1), ==, HB_OT_COLOR_PALETTE_FLAG_FOR_LIGHT_BACKGROUND);
  g_assert_cmpint (hb_ot_color_get_palette_flags (cpal_v0, 2), ==, HB_OT_COLOR_PALETTE_FLAG_DEFAULT);

  /* numPalettes=3, so palette #3 is out of bounds */
  g_assert_cmpint (hb_ot_color_get_palette_flags (cpal_v0, 3), ==, HB_OT_COLOR_PALETTE_FLAG_DEFAULT);
}


static void
test_hb_ot_color_get_palette_colors_empty (void)
{
  hb_face_t *empty = hb_face_get_empty ();
  g_assert_cmpint (hb_ot_color_get_palette_colors (empty, 0, 0, NULL, NULL), ==, 0);
}


static void
test_hb_ot_color_get_palette_colors_v0 (void)
{
  unsigned int num_colors = hb_ot_color_get_palette_colors (cpal_v0, 0, 0, NULL, NULL);
  hb_ot_color_t *colors = (hb_ot_color_t*) alloca (num_colors * sizeof (hb_ot_color_t));
  size_t colors_size = num_colors * sizeof(*colors);
  g_assert_cmpint (num_colors, ==, 2);

  /* Palette #0, start_index=0 */
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v0, 0, 0, &num_colors, colors), ==, 2);
  g_assert_cmpint (num_colors, ==, 2);
  assert_color_rgba (colors, 0, 0x00, 0x00, 0x00, 0xff);
  assert_color_rgba (colors, 1, 0x66, 0xcc, 0xff, 0xff);

  /* Palette #1, start_index=0 */
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v0, 1, 0, &num_colors, colors), ==, 2);
  g_assert_cmpint (num_colors, ==, 2);
  assert_color_rgba (colors, 0, 0x00, 0x00, 0x00, 0xff);
  assert_color_rgba (colors, 1, 0x80, 0x00, 0x00, 0xff);

  /* Palette #2 (there are only #0 and #1 in the font, so this is out of bounds) */
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v0, 2, 0, &num_colors, colors), ==, 0);

  /* Palette #0, start_index=1 */
  memset(colors, 0x33, colors_size);
  num_colors = 2;
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v0, 0, 1, &num_colors, colors), ==, 2);
  g_assert_cmpint (num_colors, ==, 1);
  assert_color_rgba (colors, 0, 0x66, 0xcc, 0xff, 0xff);
  assert_color_rgba (colors, 1, 0x33, 0x33, 0x33, 0x33);  /* untouched */

  /* Palette #0, start_index=0, pretend that we have only allocated space for 1 color */
  memset(colors, 0x44, colors_size);
  num_colors = 1;
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v0, 0, 0, &num_colors, colors), ==, 2);
  g_assert_cmpint (num_colors, ==, 1);
  assert_color_rgba (colors, 0, 0x00, 0x00, 0x00, 0xff);
  assert_color_rgba (colors, 1, 0x44, 0x44, 0x44, 0x44);  /* untouched */

  /* start_index > numPaletteEntries */
  memset(colors, 0x44, colors_size);
  num_colors = 2;
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v0, 0, 9876, &num_colors, colors), ==, 2);
  g_assert_cmpint (num_colors, ==, 0);
  assert_color_rgba (colors, 0, 0x44, 0x44, 0x44, 0x44);  /* untouched */
  assert_color_rgba (colors, 1, 0x44, 0x44, 0x44, 0x44);  /* untouched */
}


static void
test_hb_ot_color_get_palette_colors_v1 (void)
{
  hb_ot_color_t colors[3];
  unsigned int num_colors = hb_ot_color_get_palette_colors (cpal_v1, 0, 0, NULL, NULL);
  size_t colors_size = 3 * sizeof(*colors);
  g_assert_cmpint (num_colors, ==, 2);

  /* Palette #0, start_index=0 */
  memset(colors, 0x77, colors_size);
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v1, 0, 0, &num_colors, colors), ==, 2);
  g_assert_cmpint (num_colors, ==, 2);
  assert_color_rgba (colors, 0, 0x00, 0x00, 0x00, 0xff);
  assert_color_rgba (colors, 1, 0x66, 0xcc, 0xff, 0xff);
  assert_color_rgba (colors, 2, 0x77, 0x77, 0x77, 0x77);  /* untouched */

  /* Palette #1, start_index=0 */
  memset(colors, 0x77, colors_size);
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v1, 1, 0, &num_colors, colors), ==, 2);
  g_assert_cmpint (num_colors, ==, 2);
  assert_color_rgba (colors, 0, 0x00, 0x00, 0x00, 0xff);
  assert_color_rgba (colors, 1, 0xff, 0xcc, 0x66, 0xff);
  assert_color_rgba (colors, 2, 0x77, 0x77, 0x77, 0x77);  /* untouched */

  /* Palette #2, start_index=0 */
  memset(colors, 0x77, colors_size);
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v1, 2, 0, &num_colors, colors), ==, 2);
  g_assert_cmpint (num_colors, ==, 2);
  assert_color_rgba (colors, 0, 0x00, 0x00, 0x00, 0xff);
  assert_color_rgba (colors, 1, 0x80, 0x00, 0x00, 0xff);
  assert_color_rgba (colors, 2, 0x77, 0x77, 0x77, 0x77);  /* untouched */

  /* Palette #3 (out of bounds), start_index=0 */
  memset(colors, 0x77, colors_size);
  g_assert_cmpint (hb_ot_color_get_palette_colors (cpal_v1, 3, 0, &num_colors, colors), ==, 0);
  g_assert_cmpint (num_colors, ==, 0);
  assert_color_rgba (colors, 0, 0x77, 0x77, 0x77, 0x77);  /* untouched */
  assert_color_rgba (colors, 1, 0x77, 0x77, 0x77, 0x77);  /* untouched */
  assert_color_rgba (colors, 2, 0x77, 0x77, 0x77, 0x77);  /* untouched */
}
#endif

int
main (int argc, char **argv)
{
  int status = 0;

  hb_test_init (&argc, &argv);
  // cpal_v0 = hb_test_load_face ("../shaping/data/in-house/fonts/e90374e5e439e00725b4fe7a8d73db57c5a97f82.ttf");
  // cpal_v1 = hb_test_load_face ("../shaping/data/in-house/fonts/319f5d7ebffbefc5c5e6569f8cea73444d7a7268.ttf");
  // hb_test_add (test_hb_ot_color_get_palette_count);
  // hb_test_add (test_hb_ot_color_get_palette_name_id_empty);
  // hb_test_add (test_hb_ot_color_get_palette_name_id_v0);
  // hb_test_add (test_hb_ot_color_get_palette_name_id_v1);
  // hb_test_add (test_hb_ot_color_get_palette_flags_empty);
  // hb_test_add (test_hb_ot_color_get_palette_flags_v0);
  // hb_test_add (test_hb_ot_color_get_palette_flags_v1);
  // hb_test_add (test_hb_ot_color_get_palette_colors_empty);
  // hb_test_add (test_hb_ot_color_get_palette_colors_v0);
  // hb_test_add (test_hb_ot_color_get_palette_colors_v1);
  status = hb_test_run();
  hb_face_destroy (cpal_v0);
  hb_face_destroy (cpal_v1);
  return status;
}
