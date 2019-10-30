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

static void
test_collect_unicodes_format4 (void)
{
  hb_face_t *face = hb_subset_test_open_font("fonts/Roboto-Regular.abc.format4.ttf");
  hb_set_t *codepoints = hb_set_create();

  hb_face_collect_unicodes (face, codepoints);

  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x61, ==, cp);
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x62, ==, cp);
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x63, ==, cp);
  g_assert (!hb_set_next (codepoints, &cp));

  hb_set_destroy (codepoints);
  hb_face_destroy (face);
}

static void
test_collect_unicodes_format12 (void)
{
  hb_face_t *face = hb_subset_test_open_font("fonts/Roboto-Regular.abc.format12.ttf");
  hb_set_t *codepoints = hb_set_create();

  hb_face_collect_unicodes (face, codepoints);

  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x61, ==, cp);
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x62, ==, cp);
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x63, ==, cp);
  g_assert (!hb_set_next (codepoints, &cp));

  hb_set_destroy (codepoints);
  hb_face_destroy (face);
}

static void
test_collect_unicodes (void)
{
  hb_face_t *face = hb_subset_test_open_font("fonts/Roboto-Regular.abc.ttf");
  hb_set_t *codepoints = hb_set_create();

  hb_face_collect_unicodes (face, codepoints);

  hb_codepoint_t cp = HB_SET_VALUE_INVALID;
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x61, ==, cp);
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x62, ==, cp);
  g_assert (hb_set_next (codepoints, &cp));
  g_assert_cmpuint (0x63, ==, cp);
  g_assert (!hb_set_next (codepoints, &cp));

  hb_set_destroy (codepoints);
  hb_face_destroy (face);
}

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_collect_unicodes);
  hb_test_add (test_collect_unicodes_format4);
  hb_test_add (test_collect_unicodes_format12);

  return hb_test_run();
}
