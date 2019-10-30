/*
 * Copyright © 2013  Google, Inc.
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

/* Unit tests for hb-set.h */


static void
test_empty (hb_set_t *s)
{
  hb_codepoint_t next;
  g_assert_cmpint (hb_set_get_population (s), ==, 0);
  g_assert_cmpint (hb_set_get_min (s), ==, HB_SET_VALUE_INVALID);
  g_assert_cmpint (hb_set_get_max (s), ==, HB_SET_VALUE_INVALID);
  g_assert (!hb_set_has (s, 13));
  next = 53043;
  g_assert (!hb_set_next (s, &next));
  g_assert_cmpint (next, ==, HB_SET_VALUE_INVALID);
  next = 07734;
  g_assert (!hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, HB_SET_VALUE_INVALID);
  g_assert (hb_set_is_empty (s));
}

static void
test_not_empty (hb_set_t *s)
{
  hb_codepoint_t next;
  g_assert_cmpint (hb_set_get_population (s), !=, 0);
  g_assert_cmpint (hb_set_get_min (s), !=, HB_SET_VALUE_INVALID);
  g_assert_cmpint (hb_set_get_max (s), !=, HB_SET_VALUE_INVALID);
  next = HB_SET_VALUE_INVALID;
  g_assert (hb_set_next (s, &next));
  g_assert_cmpint (next, !=, HB_SET_VALUE_INVALID);
  next = HB_SET_VALUE_INVALID;
  g_assert (hb_set_previous (s, &next));
  g_assert_cmpint (next, !=, HB_SET_VALUE_INVALID);
}

static void
test_set_basic (void)
{
  hb_set_t *s = hb_set_create ();

  test_empty (s);
  hb_set_add (s, 13);
  test_not_empty (s);

  hb_set_clear (s);
  test_empty (s);

  hb_set_add (s, 33000);
  test_not_empty (s);
  hb_set_clear (s);

  hb_set_add_range (s, 10, 29);
  test_not_empty (s);
  g_assert (hb_set_has (s, 13));
  g_assert_cmpint (hb_set_get_population (s), ==, 20);
  g_assert_cmpint (hb_set_get_min (s), ==, 10);
  g_assert_cmpint (hb_set_get_max (s), ==, 29);

  test_not_empty (s);
  g_assert (hb_set_has (s, 13));
  g_assert_cmpint (hb_set_get_population (s), ==, 20);
  g_assert_cmpint (hb_set_get_min (s), ==, 10);
  g_assert_cmpint (hb_set_get_max (s), ==, 29);

  hb_set_del_range (s, 10, 18);
  test_not_empty (s);
  g_assert (!hb_set_has (s, 13));

  hb_set_add_range (s, 200, 800);
  test_not_empty (s);
  g_assert (!hb_set_has (s, 100));
  g_assert (!hb_set_has (s, 199));
  g_assert (hb_set_has (s, 200));
  g_assert (hb_set_has (s, 201));
  g_assert (hb_set_has (s, 243));
  g_assert (hb_set_has (s, 254));
  g_assert (hb_set_has (s, 255));
  g_assert (hb_set_has (s, 256));
  g_assert (hb_set_has (s, 257));
  g_assert (hb_set_has (s, 511));
  g_assert (hb_set_has (s, 512));
  g_assert (hb_set_has (s, 600));
  g_assert (hb_set_has (s, 767));
  g_assert (hb_set_has (s, 768));
  g_assert (hb_set_has (s, 769));
  g_assert (hb_set_has (s, 782));
  g_assert (hb_set_has (s, 798));
  g_assert (hb_set_has (s, 799));
  g_assert (hb_set_has (s, 800));
  g_assert (!hb_set_has (s, 801));
  g_assert (!hb_set_has (s, 802));

  hb_set_destroy (s);
}


// static inline void
// print_set (hb_set_t *s)
// {
//   hb_codepoint_t next;
//   printf ("{");
//   for (next = HB_SET_VALUE_INVALID; hb_set_next (s, &next); )
//     printf ("%d, ", next);
//   printf ("}\n");
// }

static void
test_set_algebra (void)
{
  hb_set_t *s = hb_set_create ();
  hb_set_t *o = hb_set_create ();
  hb_set_t *o2 = hb_set_create ();

  hb_set_add (o, 13);
  hb_set_add (o, 19);

  hb_set_add (o2, 0x660E);

  test_empty (s);
  g_assert (!hb_set_is_equal (s, o));
  g_assert (hb_set_is_subset (s, o));
  g_assert (!hb_set_is_subset (o, s));
  hb_set_set (s, o);
  g_assert (hb_set_is_equal (s, o));
  g_assert (hb_set_is_subset (s, o));
  g_assert (hb_set_is_subset (o, s));
  test_not_empty (s);
  g_assert_cmpint (hb_set_get_population (s), ==, 2);

  hb_set_clear (s);
  test_empty (s);
  hb_set_add (s, 10);
  g_assert_cmpint (hb_set_get_population (s), ==, 1);
  hb_set_union (s, o);
  g_assert_cmpint (hb_set_get_population (s), ==, 3);
  g_assert (hb_set_has (s, 10));
  g_assert (hb_set_has (s, 13));

  hb_set_clear (s);
  test_empty (s);
  g_assert_cmpint (hb_set_get_population (s), ==, 0);
  hb_set_union (s, o2);
  g_assert_cmpint (hb_set_get_population (s), ==, 1);
  g_assert (hb_set_has (s, 0x660E));

  hb_set_clear (s);
  test_empty (s);
  hb_set_add_range (s, 10, 17);
  g_assert (!hb_set_is_equal (s, o));
  hb_set_intersect (s, o);
  g_assert (!hb_set_is_equal (s, o));
  test_not_empty (s);
  g_assert_cmpint (hb_set_get_population (s), ==, 1);
  g_assert (!hb_set_has (s, 10));
  g_assert (hb_set_has (s, 13));

  hb_set_clear (s);
  test_empty (s);
  hb_set_add_range (s, 10, 17);
  g_assert (!hb_set_is_equal (s, o));
  hb_set_subtract (s, o);
  g_assert (!hb_set_is_equal (s, o));
  test_not_empty (s);
  g_assert_cmpint (hb_set_get_population (s), ==, 7);
  g_assert (hb_set_has (s, 12));
  g_assert (!hb_set_has (s, 13));
  g_assert (!hb_set_has (s, 19));

  hb_set_clear (s);
  test_empty (s);
  hb_set_add_range (s, 10, 17);
  g_assert (!hb_set_is_equal (s, o));
  hb_set_symmetric_difference (s, o);
  g_assert (!hb_set_is_equal (s, o));
  test_not_empty (s);
  g_assert_cmpint (hb_set_get_population (s), ==, 8);
  g_assert (hb_set_has (s, 12));
  g_assert (!hb_set_has (s, 13));
  g_assert (hb_set_has (s, 19));

  /* https://github.com/harfbuzz/harfbuzz/issues/579 */
  hb_set_clear (s);
  test_empty (s);
  hb_set_add_range (s, 886, 895);
  hb_set_add (s, 1024);
  hb_set_add (s, 1152);
  hb_set_clear (o);
  test_empty (o);
  hb_set_add (o, 889);
  hb_set_add (o, 1024);
  g_assert (!hb_set_is_equal (s, o));
  hb_set_intersect (o, s);
  test_not_empty (o);
  g_assert (!hb_set_is_equal (s, o));
  g_assert_cmpint (hb_set_get_population (o), ==, 2);
  g_assert (hb_set_has (o, 889));
  g_assert (hb_set_has (o, 1024));
  hb_set_clear (o);
  test_empty (o);
  hb_set_add_range (o, 887, 889);
  hb_set_add (o, 1121);
  g_assert (!hb_set_is_equal (s, o));
  hb_set_intersect (o, s);
  test_not_empty (o);
  g_assert (!hb_set_is_equal (s, o));
  g_assert_cmpint (hb_set_get_population (o), ==, 3);
  g_assert (hb_set_has (o, 887));
  g_assert (hb_set_has (o, 888));
  g_assert (hb_set_has (o, 889));

  hb_set_clear (s);
  test_empty (s);
  hb_set_add_range (s, 886, 895);
  hb_set_add (s, 1014);
  hb_set_add (s, 1017);
  hb_set_add (s, 1024);
  hb_set_add (s, 1113);
  hb_set_add (s, 1121);
  g_assert_cmpint (hb_set_get_population (s), ==, 15);

  hb_set_clear (o);
  test_empty (o);
  hb_set_add (o, 889);
  g_assert_cmpint (hb_set_get_population (o), ==, 1);
  hb_set_intersect (o, s);
  g_assert_cmpint (hb_set_get_population (o), ==, 1);
  g_assert (hb_set_has (o, 889));

  hb_set_add (o, 511);
  g_assert_cmpint (hb_set_get_population (o), ==, 2);
  hb_set_intersect (o, s);
  g_assert_cmpint (hb_set_get_population (o), ==, 1);
  g_assert (hb_set_has (o, 889));

  hb_set_destroy (s);
  hb_set_destroy (o);
}

static void
test_set_iter (void)
{
  hb_codepoint_t next, first, last;
  hb_set_t *s = hb_set_create ();

  hb_set_add (s, 13);
  hb_set_add_range (s, 6, 6);
  hb_set_add_range (s, 10, 15);
  hb_set_add (s, 1100);
  hb_set_add (s, 1200);
  hb_set_add (s, 20005);

  test_not_empty (s);

  next = HB_SET_VALUE_INVALID;
  g_assert (hb_set_next (s, &next));
  g_assert_cmpint (next, ==, 6);
  g_assert (hb_set_next (s, &next));
  g_assert_cmpint (next, ==, 10);
  g_assert (hb_set_next (s, &next));
  g_assert (hb_set_next (s, &next));
  g_assert (hb_set_next (s, &next));
  g_assert_cmpint (next, ==, 13);
  g_assert (hb_set_next (s, &next));
  g_assert (hb_set_next (s, &next));
  g_assert_cmpint (next, ==, 15);
  g_assert (hb_set_next (s, &next));
  g_assert_cmpint (next, ==, 1100);
  g_assert (hb_set_next (s, &next));
  g_assert_cmpint (next, ==, 1200);
  g_assert (hb_set_next (s, &next));
  g_assert_cmpint (next, ==, 20005);
  g_assert (!hb_set_next (s, &next));
  g_assert_cmpint (next, ==, HB_SET_VALUE_INVALID);

  next = HB_SET_VALUE_INVALID;
  g_assert (hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, 20005);
  g_assert (hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, 1200);
  g_assert (hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, 1100);
  g_assert (hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, 15);
  g_assert (hb_set_previous (s, &next));
  g_assert (hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, 13);
  g_assert (hb_set_previous (s, &next));
  g_assert (hb_set_previous (s, &next));
  g_assert (hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, 10);
  g_assert (hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, 6);
  g_assert (!hb_set_previous (s, &next));
  g_assert_cmpint (next, ==, HB_SET_VALUE_INVALID);

  first = last = HB_SET_VALUE_INVALID;
  g_assert (hb_set_next_range (s, &first, &last));
  g_assert_cmpint (first, ==, 6);
  g_assert_cmpint (last,  ==, 6);
  g_assert (hb_set_next_range (s, &first, &last));
  g_assert_cmpint (first, ==, 10);
  g_assert_cmpint (last,  ==, 15);
  g_assert (hb_set_next_range (s, &first, &last));
  g_assert_cmpint (first, ==, 1100);
  g_assert_cmpint (last,  ==, 1100);
  g_assert (hb_set_next_range (s, &first, &last));
  g_assert_cmpint (first, ==, 1200);
  g_assert_cmpint (last,  ==, 1200);
  g_assert (hb_set_next_range (s, &first, &last));
  g_assert_cmpint (first, ==, 20005);
  g_assert_cmpint (last,  ==, 20005);
  g_assert (!hb_set_next_range (s, &first, &last));
  g_assert_cmpint (first, ==, HB_SET_VALUE_INVALID);
  g_assert_cmpint (last,  ==, HB_SET_VALUE_INVALID);

  first = last = HB_SET_VALUE_INVALID;
  g_assert (hb_set_previous_range (s, &first, &last));
  g_assert_cmpint (first, ==, 20005);
  g_assert_cmpint (last,  ==, 20005);
  g_assert (hb_set_previous_range (s, &first, &last));
  g_assert_cmpint (first, ==, 1200);
  g_assert_cmpint (last,  ==, 1200);
  g_assert (hb_set_previous_range (s, &first, &last));
  g_assert_cmpint (first, ==, 1100);
  g_assert_cmpint (last,  ==, 1100);
  g_assert (hb_set_previous_range (s, &first, &last));
  g_assert_cmpint (first, ==, 10);
  g_assert_cmpint (last,  ==, 15);
  g_assert (hb_set_previous_range (s, &first, &last));
  g_assert_cmpint (first, ==, 6);
  g_assert_cmpint (last,  ==, 6);
  g_assert (!hb_set_previous_range (s, &first, &last));
  g_assert_cmpint (first, ==, HB_SET_VALUE_INVALID);
  g_assert_cmpint (last,  ==, HB_SET_VALUE_INVALID);

  hb_set_destroy (s);
}

static void
test_set_empty (void)
{
  hb_set_t *b = hb_set_get_empty ();

  g_assert (hb_set_get_empty ());
  g_assert (hb_set_get_empty () == b);

  g_assert (!hb_set_allocation_successful (b));

  test_empty (b);

  hb_set_add (b, 13);

  test_empty (b);

  hb_set_invert (b);

  test_empty (b);

  g_assert (!hb_set_allocation_successful (b));

  hb_set_clear (b);

  test_empty (b);

  g_assert (!hb_set_allocation_successful (b));

  hb_set_destroy (b);
}

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_set_basic);
  hb_test_add (test_set_algebra);
  hb_test_add (test_set_iter);
  hb_test_add (test_set_empty);

  return hb_test_run();
}
