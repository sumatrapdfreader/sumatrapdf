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

/* Unit tests for hmtx subsetting */

static void check_num_vmetrics(hb_face_t *face, uint16_t expected_num_vmetrics)
{
  hb_blob_t *vhea_blob = hb_face_reference_table (face, HB_TAG ('v','h','e','a'));
  hb_blob_t *vmtx_blob = hb_face_reference_table (face, HB_TAG ('v','m','t','x'));

  unsigned int vhea_len;
  uint8_t *raw_vhea = (uint8_t *) hb_blob_get_data(vhea_blob, &vhea_len);
  uint16_t num_vmetrics = (raw_vhea[vhea_len - 2] << 8) + raw_vhea[vhea_len - 1];
  g_assert_cmpuint(expected_num_vmetrics, ==, num_vmetrics);

  hb_blob_destroy (vhea_blob);
  hb_blob_destroy (vmtx_blob);
}

static void
test_subset_vmtx_simple_subset (void)
{
  hb_face_t *face_full = hb_subset_test_open_font ("fonts/Mplus1p-Regular.660E,6975,73E0,5EA6,8F38,6E05.ttf");
  hb_face_t *face_subset = hb_subset_test_open_font ("fonts/Mplus1p-Regular.660E.ttf");

  hb_set_t *codepoints = hb_set_create ();
  hb_set_add (codepoints, 0x660E);

  hb_face_t *face_full_subset = hb_subset_test_create_subset (face_full, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  check_num_vmetrics(face_full_subset, 1); /* nothing has same width */
  hb_subset_test_check (face_subset, face_full_subset, HB_TAG ('v','m','t','x'));

  hb_face_destroy (face_full_subset);
  hb_face_destroy (face_full);
  hb_face_destroy (face_subset);
}

static void
test_subset_vmtx_noop (void)
{
  hb_face_t *face_full = hb_subset_test_open_font ("fonts/Mplus1p-Regular.660E,6975,73E0,5EA6,8F38,6E05.ttf");

  hb_set_t *codepoints = hb_set_create();
  hb_set_add (codepoints, 0x660E);
  hb_set_add (codepoints, 0x6975);
  hb_set_add (codepoints, 0x73E0);
  hb_set_add (codepoints, 0x5EA6);
  hb_set_add (codepoints, 0x8F38);
  hb_set_add (codepoints, 0x6E05);
  hb_face_t *face_full_subset = hb_subset_test_create_subset (face_full, hb_subset_test_create_input (codepoints));
  hb_set_destroy (codepoints);

  check_num_vmetrics(face_full_subset, 1); /* all have the same width */
  hb_subset_test_check (face_full, face_full_subset, HB_TAG ('v','m','t','x'));

  hb_face_destroy (face_full_subset);
  hb_face_destroy (face_full);
}

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_subset_vmtx_simple_subset);
  hb_test_add (test_subset_vmtx_noop);

  return hb_test_run();
}
