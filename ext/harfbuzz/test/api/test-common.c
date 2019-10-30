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

/* Unit tests for hb-common.h */


static void
test_types_int (void)
{
  g_assert_cmpint (sizeof (int8_t), ==, 1);
  g_assert_cmpint (sizeof (uint8_t), ==, 1);
  g_assert_cmpint (sizeof (int16_t), ==, 2);
  g_assert_cmpint (sizeof (uint16_t), ==, 2);
  g_assert_cmpint (sizeof (int32_t), ==, 4);
  g_assert_cmpint (sizeof (uint32_t), ==, 4);
  g_assert_cmpint (sizeof (int64_t), ==, 8);
  g_assert_cmpint (sizeof (uint64_t), ==, 8);

  g_assert_cmpint (sizeof (hb_codepoint_t), ==, 4);
  g_assert_cmpint (sizeof (hb_position_t), ==, 4);
  g_assert_cmpint (sizeof (hb_mask_t), ==, 4);
  g_assert_cmpint (sizeof (hb_var_int_t), ==, 4);
}

static void
test_types_direction (void)
{
  g_assert_cmpint ((signed) HB_DIRECTION_INVALID, ==, 0);
  g_assert_cmpint (HB_DIRECTION_LTR, !=, 0);

  g_assert (HB_DIRECTION_IS_HORIZONTAL (HB_DIRECTION_LTR));
  g_assert (HB_DIRECTION_IS_HORIZONTAL (HB_DIRECTION_RTL));
  g_assert (!HB_DIRECTION_IS_HORIZONTAL (HB_DIRECTION_TTB));
  g_assert (!HB_DIRECTION_IS_HORIZONTAL (HB_DIRECTION_BTT));
  g_assert (!HB_DIRECTION_IS_HORIZONTAL (HB_DIRECTION_INVALID));

  g_assert (!HB_DIRECTION_IS_VERTICAL (HB_DIRECTION_LTR));
  g_assert (!HB_DIRECTION_IS_VERTICAL (HB_DIRECTION_RTL));
  g_assert (HB_DIRECTION_IS_VERTICAL (HB_DIRECTION_TTB));
  g_assert (HB_DIRECTION_IS_VERTICAL (HB_DIRECTION_BTT));
  g_assert (!HB_DIRECTION_IS_VERTICAL (HB_DIRECTION_INVALID));

  g_assert (HB_DIRECTION_IS_FORWARD (HB_DIRECTION_LTR));
  g_assert (HB_DIRECTION_IS_FORWARD (HB_DIRECTION_TTB));
  g_assert (!HB_DIRECTION_IS_FORWARD (HB_DIRECTION_RTL));
  g_assert (!HB_DIRECTION_IS_FORWARD (HB_DIRECTION_BTT));
  g_assert (!HB_DIRECTION_IS_FORWARD (HB_DIRECTION_INVALID));

  g_assert (!HB_DIRECTION_IS_BACKWARD (HB_DIRECTION_LTR));
  g_assert (!HB_DIRECTION_IS_BACKWARD (HB_DIRECTION_TTB));
  g_assert (HB_DIRECTION_IS_BACKWARD (HB_DIRECTION_RTL));
  g_assert (HB_DIRECTION_IS_BACKWARD (HB_DIRECTION_BTT));
  g_assert (!HB_DIRECTION_IS_BACKWARD (HB_DIRECTION_INVALID));

  g_assert (HB_DIRECTION_IS_VALID (HB_DIRECTION_LTR));
  g_assert (HB_DIRECTION_IS_VALID (HB_DIRECTION_TTB));
  g_assert (HB_DIRECTION_IS_VALID (HB_DIRECTION_RTL));
  g_assert (HB_DIRECTION_IS_VALID (HB_DIRECTION_BTT));
  g_assert (!HB_DIRECTION_IS_VALID (HB_DIRECTION_INVALID));
  g_assert (!HB_DIRECTION_IS_VALID ((hb_direction_t) 0x12345678));

  g_assert_cmpint (HB_DIRECTION_REVERSE (HB_DIRECTION_LTR), ==, HB_DIRECTION_RTL);
  g_assert_cmpint (HB_DIRECTION_REVERSE (HB_DIRECTION_RTL), ==, HB_DIRECTION_LTR);
  g_assert_cmpint (HB_DIRECTION_REVERSE (HB_DIRECTION_TTB), ==, HB_DIRECTION_BTT);
  g_assert_cmpint (HB_DIRECTION_REVERSE (HB_DIRECTION_BTT), ==, HB_DIRECTION_TTB);
  //g_assert_cmpint (HB_DIRECTION_REVERSE (HB_DIRECTION_INVALID), ==, HB_DIRECTION_INVALID);

  g_assert_cmpint (HB_DIRECTION_INVALID, ==, hb_direction_from_string (NULL, -1));
  g_assert_cmpint (HB_DIRECTION_INVALID, ==, hb_direction_from_string ("", -1));
  g_assert_cmpint (HB_DIRECTION_INVALID, ==, hb_direction_from_string ("t", 0));
  g_assert_cmpint (HB_DIRECTION_INVALID, ==, hb_direction_from_string ("x", -1));
  g_assert_cmpint (HB_DIRECTION_RTL, ==, hb_direction_from_string ("r", -1));
  g_assert_cmpint (HB_DIRECTION_RTL, ==, hb_direction_from_string ("rtl", -1));
  g_assert_cmpint (HB_DIRECTION_RTL, ==, hb_direction_from_string ("RtL", -1));
  g_assert_cmpint (HB_DIRECTION_RTL, ==, hb_direction_from_string ("right-to-left", -1));
  g_assert_cmpint (HB_DIRECTION_TTB, ==, hb_direction_from_string ("ttb", -1));

  g_assert (0 == strcmp ("ltr", hb_direction_to_string (HB_DIRECTION_LTR)));
  g_assert (0 == strcmp ("rtl", hb_direction_to_string (HB_DIRECTION_RTL)));
  g_assert (0 == strcmp ("ttb", hb_direction_to_string (HB_DIRECTION_TTB)));
  g_assert (0 == strcmp ("btt", hb_direction_to_string (HB_DIRECTION_BTT)));
  g_assert (0 == strcmp ("invalid", hb_direction_to_string (HB_DIRECTION_INVALID)));
}

static void
test_types_tag (void)
{
  g_assert_cmphex (HB_TAG_NONE, ==, 0);

  g_assert_cmphex (HB_TAG ('a','B','c','D'), ==, 0x61426344);

  g_assert_cmphex (hb_tag_from_string ("aBcDe", -1), ==, 0x61426344);
  g_assert_cmphex (hb_tag_from_string ("aBcD", -1),  ==, 0x61426344);
  g_assert_cmphex (hb_tag_from_string ("aBc", -1),   ==, 0x61426320);
  g_assert_cmphex (hb_tag_from_string ("aB", -1),    ==, 0x61422020);
  g_assert_cmphex (hb_tag_from_string ("a", -1),     ==, 0x61202020);
  g_assert_cmphex (hb_tag_from_string ("aBcDe",  1), ==, 0x61202020);
  g_assert_cmphex (hb_tag_from_string ("aBcDe",  2), ==, 0x61422020);
  g_assert_cmphex (hb_tag_from_string ("aBcDe",  3), ==, 0x61426320);
  g_assert_cmphex (hb_tag_from_string ("aBcDe",  4), ==, 0x61426344);
  g_assert_cmphex (hb_tag_from_string ("aBcDe",  4), ==, 0x61426344);

  g_assert_cmphex (hb_tag_from_string ("", -1),      ==, HB_TAG_NONE);
  g_assert_cmphex (hb_tag_from_string ("x", 0),      ==, HB_TAG_NONE);
  g_assert_cmphex (hb_tag_from_string (NULL, -1),    ==, HB_TAG_NONE);
}

static void
test_types_script (void)
{
  hb_tag_t arab = HB_TAG_CHAR4 ("arab");
  hb_tag_t Arab = HB_TAG_CHAR4 ("Arab");
  hb_tag_t ARAB = HB_TAG_CHAR4 ("ARAB");

  hb_tag_t wWyZ = HB_TAG_CHAR4 ("wWyZ");
  hb_tag_t Wwyz = HB_TAG_CHAR4 ("Wwyz");

  hb_tag_t x123 = HB_TAG_CHAR4 ("x123");

  g_assert_cmpint (HB_SCRIPT_INVALID, ==, (hb_script_t) HB_TAG_NONE);
  g_assert_cmphex (HB_SCRIPT_ARABIC, !=, HB_SCRIPT_LATIN);

  g_assert_cmphex (HB_SCRIPT_INVALID, ==, hb_script_from_string (NULL, -1));
  g_assert_cmphex (HB_SCRIPT_INVALID, ==, hb_script_from_string ("", -1));
  g_assert_cmphex (HB_SCRIPT_INVALID, ==, hb_script_from_string ("x", 0));
  g_assert_cmphex (HB_SCRIPT_UNKNOWN, ==, hb_script_from_string ("x", -1));

  g_assert_cmphex (HB_SCRIPT_ARABIC, ==, hb_script_from_string ("arab", -1));
  g_assert_cmphex (HB_SCRIPT_ARABIC, ==, hb_script_from_string ("Arab", -1));
  g_assert_cmphex (HB_SCRIPT_ARABIC, ==, hb_script_from_string ("ARAB", -1));
  g_assert_cmphex (HB_SCRIPT_ARABIC, ==, hb_script_from_string ("Arabic", 6));
  g_assert_cmphex (HB_SCRIPT_ARABIC, !=, hb_script_from_string ("Arabic", 3));

  g_assert_cmphex (HB_SCRIPT_ARABIC, ==, hb_script_from_iso15924_tag (arab));
  g_assert_cmphex (HB_SCRIPT_ARABIC, ==, hb_script_from_iso15924_tag (Arab));
  g_assert_cmphex (HB_SCRIPT_ARABIC, ==, hb_script_from_iso15924_tag (ARAB));

  /* Arbitrary tags that look like may be valid ISO 15924 should be preserved. */
  g_assert_cmphex (HB_SCRIPT_UNKNOWN, !=, hb_script_from_string ("wWyZ", -1));
  g_assert_cmphex (HB_SCRIPT_UNKNOWN, !=, hb_script_from_iso15924_tag (wWyZ));
  /* Otherwise, UNKNOWN should be returned. */
  g_assert_cmphex (HB_SCRIPT_UNKNOWN, ==, hb_script_from_string ("x123", -1));
  g_assert_cmphex (HB_SCRIPT_UNKNOWN, ==, hb_script_from_iso15924_tag (x123));

  g_assert_cmphex (hb_script_to_iso15924_tag (HB_SCRIPT_ARABIC), ==, Arab);
  g_assert_cmphex (hb_script_to_iso15924_tag (hb_script_from_iso15924_tag (wWyZ)), ==, Wwyz);

  g_assert_cmpint (hb_script_get_horizontal_direction (HB_SCRIPT_LATIN), ==, HB_DIRECTION_LTR);
  g_assert_cmpint (hb_script_get_horizontal_direction (HB_SCRIPT_ARABIC), ==, HB_DIRECTION_RTL);
  g_assert_cmpint (hb_script_get_horizontal_direction (HB_SCRIPT_OLD_ITALIC), ==, HB_DIRECTION_INVALID);
  g_assert_cmpint (hb_script_get_horizontal_direction (hb_script_from_iso15924_tag (wWyZ)), ==, HB_DIRECTION_LTR);
}

static void
test_types_language (void)
{
  hb_language_t fa = hb_language_from_string ("fa", -1);
  hb_language_t fa_IR = hb_language_from_string ("fa_IR", -1);
  hb_language_t fa_ir = hb_language_from_string ("fa-ir", -1);
  hb_language_t en = hb_language_from_string ("en", -1);

  g_assert (HB_LANGUAGE_INVALID == NULL);

  g_assert (fa != NULL);
  g_assert (fa_IR != NULL);
  g_assert (fa_IR == fa_ir);

  g_assert (en != NULL);
  g_assert (en != fa);

  /* Test recall */
  g_assert (en == hb_language_from_string ("en", -1));
  g_assert (en == hb_language_from_string ("eN", -1));
  g_assert (en == hb_language_from_string ("Enx", 2));

  g_assert (HB_LANGUAGE_INVALID == hb_language_from_string (NULL, -1));
  g_assert (HB_LANGUAGE_INVALID == hb_language_from_string ("", -1));
  g_assert (HB_LANGUAGE_INVALID == hb_language_from_string ("en", 0));
  g_assert (HB_LANGUAGE_INVALID != hb_language_from_string ("en", 1));
  g_assert (NULL == hb_language_to_string (HB_LANGUAGE_INVALID));

  /* Not sure how to test this better.  Setting env vars
   * here doesn't sound like the right approach, and I'm
   * not sure that it even works. */
  g_assert (HB_LANGUAGE_INVALID != hb_language_get_default ());
}

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_types_int);
  hb_test_add (test_types_direction);
  hb_test_add (test_types_tag);
  hb_test_add (test_types_script);
  hb_test_add (test_types_language);

  return hb_test_run();
}
