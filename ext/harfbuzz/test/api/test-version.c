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

/* Unit tests for hb-version.h */


static void
test_version (void)
{
  unsigned int major, minor, micro;
  char *s;

  hb_version (&major, &minor, &micro);

  g_assert_cmpint (major, ==, HB_VERSION_MAJOR);
  g_assert_cmpint (minor, ==, HB_VERSION_MINOR);
  g_assert_cmpint (micro, ==, HB_VERSION_MICRO);

  s = g_strdup_printf ("%u.%u.%u", major, minor, micro);
  g_assert (0 == strcmp (HB_VERSION_STRING, s));
  g_free (s);
  g_assert (0 == strcmp (HB_VERSION_STRING, hb_version_string ()));

  g_assert (HB_VERSION_ATLEAST (major, minor, micro));
  if (major)
    g_assert (HB_VERSION_ATLEAST (major-1, minor, micro));
  if (minor)
    g_assert (HB_VERSION_ATLEAST (major, minor-1, micro));
  if (micro)
    g_assert (HB_VERSION_ATLEAST (major, minor, micro-1));
  g_assert (!HB_VERSION_ATLEAST (major+1, minor, micro));
  g_assert (!HB_VERSION_ATLEAST (major, minor+1, micro));
  g_assert (!HB_VERSION_ATLEAST (major, minor, micro+1));
  g_assert (!HB_VERSION_ATLEAST (major, minor, micro+1));

  g_assert (hb_version_atleast (major, minor, micro));
  if (major)
    g_assert (hb_version_atleast (major-1, minor, micro));
  if (minor)
    g_assert (hb_version_atleast (major, minor-1, micro));
  if (micro)
    g_assert (hb_version_atleast (major, minor, micro-1));
  g_assert (!hb_version_atleast (major+1, minor, micro));
  g_assert (!hb_version_atleast (major, minor+1, micro));
  g_assert (!hb_version_atleast (major, minor, micro+1));
}

int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_version);

  return hb_test_run();
}
