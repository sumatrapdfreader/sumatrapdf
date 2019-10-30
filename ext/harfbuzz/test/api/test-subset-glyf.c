/*
 * Copyright © 2018  Google, Inc.
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
 * Google Author(s): Garret Rieger
 */

#include "hb-test.h"
#include "hb-subset-test.h"

/* Unit tests for hb-subset-glyf.h */

static void check_maxp_field (uint8_t *raw_maxp, unsigned int offset, uint16_t expected_value)
{
  uint16_t actual_value = (raw_maxp[offset] << 8) + raw_maxp[offset + 1];
  g_assert_cmpuint(expected_value, ==, actual_value);
}

static void check_maxp_num_glyphs (hb_face_t *face, uint16_t expected_num_glyphs, bool hints)
{
  hb_blob_t *maxp_blob = hb_face_reference_table (face, HB_TAG ('m','a','x', 'p'));

  unsigned int maxp_len;
  uint8_t *raw_maxp = (uint8_t *) hb_blob_get_data(maxp_blob, &maxp_len);

  check_maxp_field (raw_maxp, 4, expected_num_glyphs); // numGlyphs
  if (!hints)
  {
    check_maxp_field (raw_maxp, 14, 1); // maxZones
    check_maxp_field (raw_maxp, 16, 0); // maxTwilightPoints
    check_maxp_field (raw_maxp, 18, 0); // maxStorage
    check_maxp_field (raw_maxp, 20, 0); // maxFunctionDefs
    check_maxp_field (raw_maxp, 22, 0); // maxInstructionDefs
    check_maxp_field (raw_maxp, 24, 0); // maxStackElements
    check_maxp_field (raw_maxp, 26, 0); // maxSizeOfInstructions
  }

  hb_blob_destroy (maxp_blob);
}

static void
test_subset_glyf (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font ("fonts/Roboto-Regular.abc.ttf");
  hb_face_t *face_ac = hb_subset_test_open_font ("fonts/Roboto-Regular.ac.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_face_t *face_abc_subset;
  hb_set_add (codepoints, 97);
  hb_set_add (codepoints, 99);
  face_abc_subset = hb_subset_test_create_subset (face_abc, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('l','o','c', 'a'));
  check_maxp_num_glyphs(face_abc_subset, 3, true);

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
  hb_face_destroy (face_ac);
}

static void
test_subset_glyf_with_components (void)
{
  hb_face_t *face_components = hb_subset_test_open_font ("fonts/Roboto-Regular.components.ttf");
  hb_face_t *face_subset = hb_subset_test_open_font ("fonts/Roboto-Regular.components.subset.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_face_t *face_generated_subset;
  hb_set_add (codepoints, 0x1fc);
  face_generated_subset = hb_subset_test_create_subset (face_components, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  hb_subset_test_check (face_subset, face_generated_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_subset, face_generated_subset, HB_TAG ('l','o','c', 'a'));
  check_maxp_num_glyphs(face_generated_subset, 4, true);

  hb_face_destroy (face_generated_subset);
  hb_face_destroy (face_subset);
  hb_face_destroy (face_components);
}

static void
test_subset_glyf_with_gsub (void)
{
  hb_face_t *face_fil = hb_subset_test_open_font ("fonts/Roboto-Regular.gsub.fil.ttf");
  hb_face_t *face_fi = hb_subset_test_open_font ("fonts/Roboto-Regular.gsub.fi.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_set_add (codepoints, 102); // f
  hb_set_add (codepoints, 105); // i

  hb_subset_input_t *input = hb_subset_test_create_input (codepoints);
  hb_set_destroy (codepoints);
  hb_subset_input_set_drop_layout (input, false);

  hb_face_t *face_subset = hb_subset_test_create_subset (face_fil, input);

  hb_subset_test_check (face_fi, face_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_fi, face_subset, HB_TAG ('l','o','c', 'a'));
  check_maxp_num_glyphs(face_subset, 5, true);

  hb_face_destroy (face_subset);
  hb_face_destroy (face_fi);
  hb_face_destroy (face_fil);
}

static void
test_subset_glyf_without_gsub (void)
{
  hb_face_t *face_fil = hb_subset_test_open_font ("fonts/Roboto-Regular.gsub.fil.ttf");
  hb_face_t *face_fi = hb_subset_test_open_font ("fonts/Roboto-Regular.nogsub.fi.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_set_add (codepoints, 102); // f
  hb_set_add (codepoints, 105); // i

  hb_subset_input_t *input = hb_subset_test_create_input (codepoints);
  hb_set_destroy (codepoints);
  hb_subset_input_set_drop_layout (input, true);

  hb_face_t *face_subset = hb_subset_test_create_subset (face_fil, input);

  hb_subset_test_check (face_fi, face_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_fi, face_subset, HB_TAG ('l','o','c', 'a'));
  check_maxp_num_glyphs(face_subset, 3, true);

  hb_face_destroy (face_subset);
  hb_face_destroy (face_fi);
  hb_face_destroy (face_fil);
}

static void
test_subset_glyf_noop (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font("fonts/Roboto-Regular.abc.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_face_t *face_abc_subset;
  hb_set_add (codepoints, 97);
  hb_set_add (codepoints, 98);
  hb_set_add (codepoints, 99);
  face_abc_subset = hb_subset_test_create_subset (face_abc, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  hb_subset_test_check (face_abc, face_abc_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_abc, face_abc_subset, HB_TAG ('l','o','c', 'a'));
  check_maxp_num_glyphs(face_abc_subset, 4, true);

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
}

static void
test_subset_glyf_strip_hints_simple (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font ("fonts/Roboto-Regular.abc.ttf");
  hb_face_t *face_ac = hb_subset_test_open_font ("fonts/Roboto-Regular.ac.nohints.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_subset_input_t *input;
  hb_face_t *face_abc_subset;
  hb_set_add (codepoints, 'a');
  hb_set_add (codepoints, 'c');
  input = hb_subset_test_create_input (codepoints);
  hb_subset_input_set_drop_hints (input, true);
  face_abc_subset = hb_subset_test_create_subset (face_abc, input);
  hb_set_destroy (codepoints);

  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('l','o','c', 'a'));
  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('g','l','y','f'));
  check_maxp_num_glyphs(face_abc_subset, 3, false);

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
  hb_face_destroy (face_ac);
}

static void
test_subset_glyf_strip_hints_composite (void)
{
  hb_face_t *face_components = hb_subset_test_open_font ("fonts/Roboto-Regular.components.ttf");
  hb_face_t *face_subset = hb_subset_test_open_font ("fonts/Roboto-Regular.components.1fc.nohints.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_subset_input_t *input;
  hb_face_t *face_generated_subset;
  hb_set_add (codepoints, 0x1fc);
  input = hb_subset_test_create_input (codepoints);
  hb_subset_input_set_drop_hints (input, true);

  face_generated_subset = hb_subset_test_create_subset (face_components, input);
  hb_set_destroy (codepoints);

  hb_subset_test_check (face_subset, face_generated_subset, HB_TAG ('g','l','y','f'));
  hb_subset_test_check (face_subset, face_generated_subset, HB_TAG ('l','o','c', 'a'));
  check_maxp_num_glyphs(face_generated_subset, 4, false);

  hb_face_destroy (face_generated_subset);
  hb_face_destroy (face_subset);
  hb_face_destroy (face_components);
}

static void
test_subset_glyf_strip_hints_invalid (void)
{
  hb_face_t *face = hb_subset_test_open_font ("fonts/oom-ccc61c92d589f895174cdef6ff2e3b20e9999a1a");

  hb_set_t *codepoints = hb_set_create();
  const hb_codepoint_t text[] =
  {
    'A', 'B', 'C', 'D', 'E', 'X', 'Y', 'Z', '1', '2',
    '3', '@', '_', '%', '&', ')', '*', '$', '!'
  };
  unsigned int i;
  for (i = 0; i < sizeof (text) / sizeof (hb_codepoint_t); i++)
  {
    hb_set_add (codepoints, text[i]);
  }

  hb_subset_input_t *input = hb_subset_test_create_input (codepoints);
  hb_subset_input_set_drop_hints (input, true);
  hb_set_destroy (codepoints);

  hb_face_t *face_subset = hb_subset_test_create_subset (face, input);
  g_assert (face_subset);
  g_assert (face_subset == hb_face_get_empty ());

  hb_face_destroy (face_subset);
  hb_face_destroy (face);
}

// TODO(grieger): test for long loca generation.

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_subset_glyf_noop);
  hb_test_add (test_subset_glyf);
  hb_test_add (test_subset_glyf_strip_hints_simple);
  hb_test_add (test_subset_glyf_strip_hints_composite);
  hb_test_add (test_subset_glyf_strip_hints_invalid);
  hb_test_add (test_subset_glyf_with_components);
  hb_test_add (test_subset_glyf_with_gsub);
  hb_test_add (test_subset_glyf_without_gsub);

  return hb_test_run();
}
