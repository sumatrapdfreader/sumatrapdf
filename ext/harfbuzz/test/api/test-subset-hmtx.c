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
 * Google Author(s): Roderick Sheeter
 */

#include "hb-test.h"
#include "hb-subset-test.h"

/* Unit tests for hmtx subsetting */

static void check_num_hmetrics(hb_face_t *face, uint16_t expected_num_hmetrics)
{
  hb_blob_t *hhea_blob = hb_face_reference_table (face, HB_TAG ('h','h','e','a'));
  hb_blob_t *hmtx_blob = hb_face_reference_table (face, HB_TAG ('h','m','t','x'));

  // TODO I sure wish I could just use the hmtx table struct!
  unsigned int hhea_len;
  uint8_t *raw_hhea = (uint8_t *) hb_blob_get_data(hhea_blob, &hhea_len);
  uint16_t num_hmetrics = (raw_hhea[hhea_len - 2] << 8) + raw_hhea[hhea_len - 1];
  g_assert_cmpuint(expected_num_hmetrics, ==, num_hmetrics);

  hb_blob_destroy (hhea_blob);
  hb_blob_destroy (hmtx_blob);
}

static void
test_subset_hmtx_simple_subset (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font ("fonts/Roboto-Regular.abc.ttf");
  hb_face_t *face_ac = hb_subset_test_open_font ("fonts/Roboto-Regular.ac.ttf");

  hb_set_t *codepoints = hb_set_create ();
  hb_face_t *face_abc_subset;
  hb_set_add (codepoints, 'a');
  hb_set_add (codepoints, 'c');
  face_abc_subset = hb_subset_test_create_subset (face_abc, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  check_num_hmetrics(face_abc_subset, 3); /* nothing has same width */
  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('h','m','t','x'));

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
  hb_face_destroy (face_ac);
}


static void
test_subset_hmtx_monospace (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font ("fonts/Inconsolata-Regular.abc.ttf");
  hb_face_t *face_ac = hb_subset_test_open_font ("fonts/Inconsolata-Regular.ac.ttf");

  hb_set_t *codepoints = hb_set_create ();
  hb_face_t *face_abc_subset;
  hb_set_add (codepoints, 'a');
  hb_set_add (codepoints, 'c');
  face_abc_subset = hb_subset_test_create_subset (face_abc, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  check_num_hmetrics(face_abc_subset, 1); /* everything has same width */
  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('h','m','t','x'));

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
  hb_face_destroy (face_ac);
}


static void
test_subset_hmtx_keep_num_metrics (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font ("fonts/Inconsolata-Regular.abc.widerc.ttf");
  hb_face_t *face_ac = hb_subset_test_open_font ("fonts/Inconsolata-Regular.ac.widerc.ttf");

  hb_set_t *codepoints = hb_set_create ();
  hb_face_t *face_abc_subset;
  hb_set_add (codepoints, 'a');
  hb_set_add (codepoints, 'c');
  face_abc_subset = hb_subset_test_create_subset (face_abc, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  check_num_hmetrics(face_abc_subset, 3); /* c is wider */
  hb_subset_test_check (face_ac, face_abc_subset, HB_TAG ('h','m','t','x'));

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
  hb_face_destroy (face_ac);
}

static void
test_subset_hmtx_decrease_num_metrics (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font ("fonts/Inconsolata-Regular.abc.widerc.ttf");
  hb_face_t *face_ab = hb_subset_test_open_font ("fonts/Inconsolata-Regular.ab.ttf");

  hb_set_t *codepoints = hb_set_create ();
  hb_face_t *face_abc_subset;
  hb_set_add (codepoints, 'a');
  hb_set_add (codepoints, 'b');
  face_abc_subset = hb_subset_test_create_subset (face_abc, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  check_num_hmetrics(face_abc_subset, 1); /* everything left has same width */
  hb_subset_test_check (face_ab, face_abc_subset, HB_TAG ('h','m','t','x'));

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
  hb_face_destroy (face_ab);
}

static void
test_subset_hmtx_noop (void)
{
  hb_face_t *face_abc = hb_subset_test_open_font("fonts/Roboto-Regular.abc.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_face_t *face_abc_subset;
  hb_set_add (codepoints, 'a');
  hb_set_add (codepoints, 'b');
  hb_set_add (codepoints, 'c');
  face_abc_subset = hb_subset_test_create_subset (face_abc, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  check_num_hmetrics(face_abc_subset, 4); /* nothing has same width */
  hb_subset_test_check (face_abc, face_abc_subset, HB_TAG ('h','m','t','x'));

  hb_face_destroy (face_abc_subset);
  hb_face_destroy (face_abc);
}

static void
test_subset_invalid_hmtx (void)
{
  hb_face_t *face = hb_subset_test_open_font("fonts/crash-e4e0bb1458a91b692eba492c907ae1f94e635480");

  hb_subset_input_t *input = hb_subset_input_create_or_fail ();
  hb_set_t *codepoints = hb_subset_input_unicode_set (input);
  hb_set_add (codepoints, 'a');
  hb_set_add (codepoints, 'b');
  hb_set_add (codepoints, 'c');

  hb_face_t *subset = hb_subset (face, input);
  g_assert (subset);
  g_assert (subset == hb_face_get_empty ());

  hb_subset_input_destroy (input);
  hb_face_destroy (subset);
  hb_face_destroy (face);
}

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_subset_hmtx_simple_subset);
  hb_test_add (test_subset_hmtx_monospace);
  hb_test_add (test_subset_hmtx_keep_num_metrics);
  hb_test_add (test_subset_hmtx_decrease_num_metrics);
  hb_test_add (test_subset_hmtx_noop);
  hb_test_add (test_subset_invalid_hmtx);

  return hb_test_run();
}
