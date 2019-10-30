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

/* Unit tests for hb-buffer.h */


static const char utf8[10] = "ab\360\240\200\200defg";
static const uint16_t utf16[8] = {'a', 'b', 0xD840, 0xDC00, 'd', 'e', 'f', 'g'};
static const uint32_t utf32[7] = {'a', 'b', 0x20000, 'd', 'e', 'f', 'g'};


typedef enum {
  BUFFER_EMPTY,
  BUFFER_ONE_BY_ONE,
  BUFFER_UTF32,
  BUFFER_UTF16,
  BUFFER_UTF8,
  BUFFER_NUM_TYPES,
} buffer_type_t;

static const char *buffer_names[] = {
  "empty",
  "one-by-one",
  "utf32",
  "utf16",
  "utf8"
};

typedef struct
{
  hb_buffer_t *buffer;
} fixture_t;

static void
fixture_init (fixture_t *fixture, gconstpointer user_data)
{
  hb_buffer_t *b;
  unsigned int i;

  b = fixture->buffer = hb_buffer_create ();

  switch (GPOINTER_TO_INT (user_data))
  {
    case BUFFER_EMPTY:
      break;

    case BUFFER_ONE_BY_ONE:
      for (i = 1; i < G_N_ELEMENTS (utf32) - 1; i++)
	hb_buffer_add (b, utf32[i], i);
      break;

    case BUFFER_UTF32:
      hb_buffer_add_utf32 (b, utf32, G_N_ELEMENTS (utf32), 1, G_N_ELEMENTS (utf32) - 2);
      break;

    case BUFFER_UTF16:
      hb_buffer_add_utf16 (b, utf16, G_N_ELEMENTS (utf16), 1, G_N_ELEMENTS (utf16) - 2);
      break;

    case BUFFER_UTF8:
      hb_buffer_add_utf8  (b, utf8,  G_N_ELEMENTS (utf8),  1, G_N_ELEMENTS (utf8)  - 2);
      break;

    default:
      g_assert_not_reached ();
  }
}

static void
fixture_finish (fixture_t *fixture, gconstpointer user_data)
{
  hb_buffer_destroy (fixture->buffer);
}


static void
test_buffer_properties (fixture_t *fixture, gconstpointer user_data)
{
  hb_buffer_t *b = fixture->buffer;
  hb_unicode_funcs_t *ufuncs;

  /* test default properties */

  g_assert (hb_buffer_get_unicode_funcs (b) == hb_unicode_funcs_get_default ());
  g_assert (hb_buffer_get_direction (b) == HB_DIRECTION_INVALID);
  g_assert (hb_buffer_get_script (b) == HB_SCRIPT_INVALID);
  g_assert (hb_buffer_get_language (b) == NULL);


  /* test property changes are retained */
  ufuncs = hb_unicode_funcs_create (NULL);
  hb_buffer_set_unicode_funcs (b, ufuncs);
  hb_unicode_funcs_destroy (ufuncs);
  g_assert (hb_buffer_get_unicode_funcs (b) == ufuncs);

  hb_buffer_set_direction (b, HB_DIRECTION_RTL);
  g_assert (hb_buffer_get_direction (b) == HB_DIRECTION_RTL);

  hb_buffer_set_script (b, HB_SCRIPT_ARABIC);
  g_assert (hb_buffer_get_script (b) == HB_SCRIPT_ARABIC);

  hb_buffer_set_language (b, hb_language_from_string ("fa", -1));
  g_assert (hb_buffer_get_language (b) == hb_language_from_string ("Fa", -1));

  hb_buffer_set_flags (b, HB_BUFFER_FLAG_BOT);
  g_assert (hb_buffer_get_flags (b) == HB_BUFFER_FLAG_BOT);

  hb_buffer_set_replacement_codepoint (b, (unsigned int) -1);
  g_assert (hb_buffer_get_replacement_codepoint (b) == (unsigned int) -1);


  /* test clear_contents clears all these properties: */

  hb_buffer_clear_contents (b);

  g_assert (hb_buffer_get_unicode_funcs (b) == ufuncs);
  g_assert (hb_buffer_get_direction (b) == HB_DIRECTION_INVALID);
  g_assert (hb_buffer_get_script (b) == HB_SCRIPT_INVALID);
  g_assert (hb_buffer_get_language (b) == NULL);

  /* but not these: */

  g_assert (hb_buffer_get_flags (b) != HB_BUFFER_FLAGS_DEFAULT);
  g_assert (hb_buffer_get_replacement_codepoint (b) != HB_BUFFER_REPLACEMENT_CODEPOINT_DEFAULT);


  /* test reset clears all properties */

  hb_buffer_set_direction (b, HB_DIRECTION_RTL);
  g_assert (hb_buffer_get_direction (b) == HB_DIRECTION_RTL);

  hb_buffer_set_script (b, HB_SCRIPT_ARABIC);
  g_assert (hb_buffer_get_script (b) == HB_SCRIPT_ARABIC);

  hb_buffer_set_language (b, hb_language_from_string ("fa", -1));
  g_assert (hb_buffer_get_language (b) == hb_language_from_string ("Fa", -1));

  hb_buffer_set_flags (b, HB_BUFFER_FLAG_BOT);
  g_assert (hb_buffer_get_flags (b) == HB_BUFFER_FLAG_BOT);

  hb_buffer_set_replacement_codepoint (b, (unsigned int) -1);
  g_assert (hb_buffer_get_replacement_codepoint (b) == (unsigned int) -1);

  hb_buffer_reset (b);

  g_assert (hb_buffer_get_unicode_funcs (b) == hb_unicode_funcs_get_default ());
  g_assert (hb_buffer_get_direction (b) == HB_DIRECTION_INVALID);
  g_assert (hb_buffer_get_script (b) == HB_SCRIPT_INVALID);
  g_assert (hb_buffer_get_language (b) == NULL);
  g_assert (hb_buffer_get_flags (b) == HB_BUFFER_FLAGS_DEFAULT);
  g_assert (hb_buffer_get_replacement_codepoint (b) == HB_BUFFER_REPLACEMENT_CODEPOINT_DEFAULT);
}

static void
test_buffer_contents (fixture_t *fixture, gconstpointer user_data)
{
  hb_buffer_t *b = fixture->buffer;
  unsigned int i, len, len2;
  buffer_type_t buffer_type = GPOINTER_TO_INT (user_data);
  hb_glyph_info_t *glyphs;

  if (buffer_type == BUFFER_EMPTY) {
    g_assert_cmpint (hb_buffer_get_length (b), ==, 0);
    return;
  }

  len = hb_buffer_get_length (b);
  hb_buffer_get_glyph_infos (b, NULL); /* test NULL */
  glyphs = hb_buffer_get_glyph_infos (b, &len2);
  g_assert_cmpint (len, ==, len2);
  g_assert_cmpint (len, ==, 5);

  for (i = 0; i < len; i++) {
    g_assert_cmphex (glyphs[i].mask,      ==, 0);
    g_assert_cmphex (glyphs[i].var1.u32,  ==, 0);
    g_assert_cmphex (glyphs[i].var2.u32,  ==, 0);
  }

  for (i = 0; i < len; i++) {
    unsigned int cluster;
    cluster = 1+i;
    if (i >= 2) {
      if (buffer_type == BUFFER_UTF16)
	cluster++;
      else if (buffer_type == BUFFER_UTF8)
        cluster += 3;
    }
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[1+i]);
    g_assert_cmphex (glyphs[i].cluster,   ==, cluster);
  }

  /* reverse, test, and reverse back */

  hb_buffer_reverse (b);
  for (i = 0; i < len; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[len-i]);

  hb_buffer_reverse (b);
  for (i = 0; i < len; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[1+i]);

  /* reverse_clusters works same as reverse for now since each codepoint is
   * in its own cluster */

  hb_buffer_reverse_clusters (b);
  for (i = 0; i < len; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[len-i]);

  hb_buffer_reverse_clusters (b);
  for (i = 0; i < len; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[1+i]);

  /* now form a cluster and test again */
  glyphs[2].cluster = glyphs[1].cluster;

  /* reverse, test, and reverse back */

  hb_buffer_reverse (b);
  for (i = 0; i < len; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[len-i]);

  hb_buffer_reverse (b);
  for (i = 0; i < len; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[1+i]);

  /* reverse_clusters twice still should return the original string,
   * but when applied once, the 1-2 cluster should be retained. */

  hb_buffer_reverse_clusters (b);
  for (i = 0; i < len; i++) {
    unsigned int j = len-1-i;
    if (j == 1)
      j = 2;
    else if (j == 2)
      j = 1;
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[1+j]);
  }

  hb_buffer_reverse_clusters (b);
  for (i = 0; i < len; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[1+i]);


  /* test setting length */

  /* enlarge */
  g_assert (hb_buffer_set_length (b, 10));
  glyphs = hb_buffer_get_glyph_infos (b, NULL);
  g_assert_cmpint (hb_buffer_get_length (b), ==, 10);
  for (i = 0; i < 5; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[1+i]);
  for (i = 5; i < 10; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, 0);
  /* shrink */
  g_assert (hb_buffer_set_length (b, 3));
  glyphs = hb_buffer_get_glyph_infos (b, NULL);
  g_assert_cmpint (hb_buffer_get_length (b), ==, 3);
  for (i = 0; i < 3; i++)
    g_assert_cmphex (glyphs[i].codepoint, ==, utf32[1+i]);


  g_assert (hb_buffer_allocation_successful (b));


  /* test reset clears content */

  hb_buffer_reset (b);
  g_assert_cmpint (hb_buffer_get_length (b), ==, 0);
}

static void
test_buffer_positions (fixture_t *fixture, gconstpointer user_data)
{
  hb_buffer_t *b = fixture->buffer;
  unsigned int i, len, len2;
  hb_glyph_position_t *positions;

  /* Without shaping, positions should all be zero */
  len = hb_buffer_get_length (b);
  hb_buffer_get_glyph_positions (b, NULL); /* test NULL */
  positions = hb_buffer_get_glyph_positions (b, &len2);
  g_assert_cmpint (len, ==, len2);
  for (i = 0; i < len; i++) {
    g_assert_cmpint (0, ==, positions[i].x_advance);
    g_assert_cmpint (0, ==, positions[i].y_advance);
    g_assert_cmpint (0, ==, positions[i].x_offset);
    g_assert_cmpint (0, ==, positions[i].y_offset);
    g_assert_cmpint (0, ==, positions[i].var.i32);
  }

  /* test reset clears content */
  hb_buffer_reset (b);
  g_assert_cmpint (hb_buffer_get_length (b), ==, 0);
}

static void
test_buffer_allocation (fixture_t *fixture, gconstpointer user_data)
{
  hb_buffer_t *b = fixture->buffer;

  g_assert_cmpint (hb_buffer_get_length (b), ==, 0);

  g_assert (hb_buffer_pre_allocate (b, 100));
  g_assert_cmpint (hb_buffer_get_length (b), ==, 0);
  g_assert (hb_buffer_allocation_successful (b));

  /* lets try a huge allocation, make sure it fails */
  g_assert (!hb_buffer_pre_allocate (b, (unsigned int) -1));
  g_assert_cmpint (hb_buffer_get_length (b), ==, 0);
  g_assert (!hb_buffer_allocation_successful (b));

  /* small one again */
  g_assert (hb_buffer_pre_allocate (b, 50));
  g_assert_cmpint (hb_buffer_get_length (b), ==, 0);
  g_assert (!hb_buffer_allocation_successful (b));

  hb_buffer_reset (b);
  g_assert (hb_buffer_allocation_successful (b));

  /* all allocation and size  */
  g_assert (!hb_buffer_pre_allocate (b, ((unsigned int) -1) / 20 + 1));
  g_assert (!hb_buffer_allocation_successful (b));

  hb_buffer_reset (b);
  g_assert (hb_buffer_allocation_successful (b));

  /* technically, this one can actually pass on 64bit machines, but
   * I'm doubtful that any malloc allows 4GB allocations at a time.
   * But let's only enable it on a 32-bit machine. */
  if (sizeof (long) == 4) {
    g_assert (!hb_buffer_pre_allocate (b, ((unsigned int) -1) / 20 - 1));
    g_assert (!hb_buffer_allocation_successful (b));
  }

  hb_buffer_reset (b);
  g_assert (hb_buffer_allocation_successful (b));
}


typedef struct {
  const char utf8[8];
  const uint32_t codepoints[8];
} utf8_conversion_test_t;

/* note: we skip the first and last byte when adding to buffer */
static const utf8_conversion_test_t utf8_conversion_tests[] = {
  {"a\303\207", {(hb_codepoint_t) -1}},
  {"a\303\207b", {0xC7}},
  {"ab\303cd", {'b', (hb_codepoint_t) -1, 'c'}},
  {"ab\303\302\301cd", {'b', (hb_codepoint_t) -1, (hb_codepoint_t) -1, (hb_codepoint_t) -1, 'c'}}
};

static void
test_buffer_utf8_conversion (void)
{
  hb_buffer_t *b;
  hb_glyph_info_t *glyphs;
  unsigned int bytes, chars, i, j, len;

  b = hb_buffer_create ();
  hb_buffer_set_replacement_codepoint (b, (hb_codepoint_t) -1);

  for (i = 0; i < G_N_ELEMENTS (utf8_conversion_tests); i++)
  {
    const utf8_conversion_test_t *test = &utf8_conversion_tests[i];
    char *escaped;

    escaped = g_strescape (test->utf8, NULL);
    g_test_message ("UTF-8 test #%d: %s", i, escaped);
    g_free (escaped);

    bytes = strlen (test->utf8);
    for (chars = 0; test->codepoints[chars]; chars++)
      ;

    hb_buffer_clear_contents (b);
    hb_buffer_add_utf8 (b, test->utf8, bytes,  1, bytes - 2);

    glyphs = hb_buffer_get_glyph_infos (b, &len);
    g_assert_cmpint (len, ==, chars);
    for (j = 0; j < chars; j++)
      g_assert_cmphex (glyphs[j].codepoint, ==, test->codepoints[j]);
  }

  hb_buffer_destroy (b);
}



/* Following test table is adapted from glib/glib/tests/utf8-validate.c
 * with relicensing permission from Matthias Clasen. */

typedef struct {
  const char *utf8;
  int max_len;
  unsigned int offset;
  gboolean valid;
} utf8_validity_test_t;

static const utf8_validity_test_t utf8_validity_tests[] = {
  /* some tests to check max_len handling */
  /* length 1 */
  { "abcde", -1, 5, TRUE },
  { "abcde", 3, 3, TRUE },
  { "abcde", 5, 5, TRUE },
  /* length 2 */
  { "\xc2\xa9\xc2\xa9\xc2\xa9", -1, 6, TRUE },
  { "\xc2\xa9\xc2\xa9\xc2\xa9",  1, 0, FALSE },
  { "\xc2\xa9\xc2\xa9\xc2\xa9",  2, 2, TRUE },
  { "\xc2\xa9\xc2\xa9\xc2\xa9",  3, 2, FALSE },
  { "\xc2\xa9\xc2\xa9\xc2\xa9",  4, 4, TRUE },
  { "\xc2\xa9\xc2\xa9\xc2\xa9",  5, 4, FALSE },
  { "\xc2\xa9\xc2\xa9\xc2\xa9",  6, 6, TRUE },
  /* length 3 */
  { "\xe2\x89\xa0\xe2\x89\xa0", -1, 6, TRUE },
  { "\xe2\x89\xa0\xe2\x89\xa0",  1, 0, FALSE },
  { "\xe2\x89\xa0\xe2\x89\xa0",  2, 0, FALSE },
  { "\xe2\x89\xa0\xe2\x89\xa0",  3, 3, TRUE },
  { "\xe2\x89\xa0\xe2\x89\xa0",  4, 3, FALSE },
  { "\xe2\x89\xa0\xe2\x89\xa0",  5, 3, FALSE },
  { "\xe2\x89\xa0\xe2\x89\xa0",  6, 6, TRUE },

  /* examples from https://www.cl.cam.ac.uk/~mgk25/ucs/examples/UTF-8-test.txt */
  /* greek 'kosme' */
  { "\xce\xba\xe1\xbd\xb9\xcf\x83\xce\xbc\xce\xb5", -1, 11, TRUE },
  /* first sequence of each length */
  { "\x00", -1, 0, TRUE },
  { "\xc2\x80", -1, 2, TRUE },
  { "\xe0\xa0\x80", -1, 3, TRUE },
  { "\xf0\x90\x80\x80", -1, 4, TRUE },
  { "\xf8\x88\x80\x80\x80", -1, 0, FALSE },
  { "\xfc\x84\x80\x80\x80\x80", -1, 0, FALSE },
  /* last sequence of each length */
  { "\x7f", -1, 1, TRUE },
  { "\xdf\xbf", -1, 2, TRUE },
  { "\xef\xbf\xbf", -1, 0, TRUE },
  { "\xf4\x8f\xbf\xbf", -1, 0, TRUE },
  { "\xf4\x90\xbf\xbf", -1, 0, FALSE },
  { "\xf7\xbf\xbf\xbf", -1, 0, FALSE },
  { "\xfb\xbf\xbf\xbf\xbf", -1, 0, FALSE },
  { "\xfd\xbf\xbf\xbf\xbf\xbf", -1, 0, FALSE },
  /* other boundary conditions */
  { "\xed\x9f\xbf", -1, 3, TRUE },
  { "\xed\xa0\x80", -1, 0, FALSE },
  { "\xed\xbf\xbf", -1, 0, FALSE },
  { "\xee\x80\x80", -1, 3, TRUE },
  { "\xef\xbf\xbd", -1, 3, TRUE },
  { "\xf4\x8f\xbf\xbf", -1, 0, TRUE },
  /* malformed sequences */
  /* continuation bytes */
  { "\x80", -1, 0, FALSE },
  { "\xbf", -1, 0, FALSE },
  { "\x80\xbf", -1, 0, FALSE },
  { "\x80\xbf\x80", -1, 0, FALSE },
  { "\x80\xbf\x80\xbf", -1, 0, FALSE },
  { "\x80\xbf\x80\xbf\x80", -1, 0, FALSE },
  { "\x80\xbf\x80\xbf\x80\xbf", -1, 0, FALSE },
  { "\x80\xbf\x80\xbf\x80\xbf\x80", -1, 0, FALSE },

  /* all possible continuation byte */
  { "\x80", -1, 0, FALSE },
  { "\x81", -1, 0, FALSE },
  { "\x82", -1, 0, FALSE },
  { "\x83", -1, 0, FALSE },
  { "\x84", -1, 0, FALSE },
  { "\x85", -1, 0, FALSE },
  { "\x86", -1, 0, FALSE },
  { "\x87", -1, 0, FALSE },
  { "\x88", -1, 0, FALSE },
  { "\x89", -1, 0, FALSE },
  { "\x8a", -1, 0, FALSE },
  { "\x8b", -1, 0, FALSE },
  { "\x8c", -1, 0, FALSE },
  { "\x8d", -1, 0, FALSE },
  { "\x8e", -1, 0, FALSE },
  { "\x8f", -1, 0, FALSE },
  { "\x90", -1, 0, FALSE },
  { "\x91", -1, 0, FALSE },
  { "\x92", -1, 0, FALSE },
  { "\x93", -1, 0, FALSE },
  { "\x94", -1, 0, FALSE },
  { "\x95", -1, 0, FALSE },
  { "\x96", -1, 0, FALSE },
  { "\x97", -1, 0, FALSE },
  { "\x98", -1, 0, FALSE },
  { "\x99", -1, 0, FALSE },
  { "\x9a", -1, 0, FALSE },
  { "\x9b", -1, 0, FALSE },
  { "\x9c", -1, 0, FALSE },
  { "\x9d", -1, 0, FALSE },
  { "\x9e", -1, 0, FALSE },
  { "\x9f", -1, 0, FALSE },
  { "\xa0", -1, 0, FALSE },
  { "\xa1", -1, 0, FALSE },
  { "\xa2", -1, 0, FALSE },
  { "\xa3", -1, 0, FALSE },
  { "\xa4", -1, 0, FALSE },
  { "\xa5", -1, 0, FALSE },
  { "\xa6", -1, 0, FALSE },
  { "\xa7", -1, 0, FALSE },
  { "\xa8", -1, 0, FALSE },
  { "\xa9", -1, 0, FALSE },
  { "\xaa", -1, 0, FALSE },
  { "\xab", -1, 0, FALSE },
  { "\xac", -1, 0, FALSE },
  { "\xad", -1, 0, FALSE },
  { "\xae", -1, 0, FALSE },
  { "\xaf", -1, 0, FALSE },
  { "\xb0", -1, 0, FALSE },
  { "\xb1", -1, 0, FALSE },
  { "\xb2", -1, 0, FALSE },
  { "\xb3", -1, 0, FALSE },
  { "\xb4", -1, 0, FALSE },
  { "\xb5", -1, 0, FALSE },
  { "\xb6", -1, 0, FALSE },
  { "\xb7", -1, 0, FALSE },
  { "\xb8", -1, 0, FALSE },
  { "\xb9", -1, 0, FALSE },
  { "\xba", -1, 0, FALSE },
  { "\xbb", -1, 0, FALSE },
  { "\xbc", -1, 0, FALSE },
  { "\xbd", -1, 0, FALSE },
  { "\xbe", -1, 0, FALSE },
  { "\xbf", -1, 0, FALSE },
  /* lone start characters */
  { "\xc0\x20", -1, 0, FALSE },
  { "\xc1\x20", -1, 0, FALSE },
  { "\xc2\x20", -1, 0, FALSE },
  { "\xc3\x20", -1, 0, FALSE },
  { "\xc4\x20", -1, 0, FALSE },
  { "\xc5\x20", -1, 0, FALSE },
  { "\xc6\x20", -1, 0, FALSE },
  { "\xc7\x20", -1, 0, FALSE },
  { "\xc8\x20", -1, 0, FALSE },
  { "\xc9\x20", -1, 0, FALSE },
  { "\xca\x20", -1, 0, FALSE },
  { "\xcb\x20", -1, 0, FALSE },
  { "\xcc\x20", -1, 0, FALSE },
  { "\xcd\x20", -1, 0, FALSE },
  { "\xce\x20", -1, 0, FALSE },
  { "\xcf\x20", -1, 0, FALSE },
  { "\xd0\x20", -1, 0, FALSE },
  { "\xd1\x20", -1, 0, FALSE },
  { "\xd2\x20", -1, 0, FALSE },
  { "\xd3\x20", -1, 0, FALSE },
  { "\xd4\x20", -1, 0, FALSE },
  { "\xd5\x20", -1, 0, FALSE },
  { "\xd6\x20", -1, 0, FALSE },
  { "\xd7\x20", -1, 0, FALSE },
  { "\xd8\x20", -1, 0, FALSE },
  { "\xd9\x20", -1, 0, FALSE },
  { "\xda\x20", -1, 0, FALSE },
  { "\xdb\x20", -1, 0, FALSE },
  { "\xdc\x20", -1, 0, FALSE },
  { "\xdd\x20", -1, 0, FALSE },
  { "\xde\x20", -1, 0, FALSE },
  { "\xdf\x20", -1, 0, FALSE },
  { "\xe0\x20", -1, 0, FALSE },
  { "\xe1\x20", -1, 0, FALSE },
  { "\xe2\x20", -1, 0, FALSE },
  { "\xe3\x20", -1, 0, FALSE },
  { "\xe4\x20", -1, 0, FALSE },
  { "\xe5\x20", -1, 0, FALSE },
  { "\xe6\x20", -1, 0, FALSE },
  { "\xe7\x20", -1, 0, FALSE },
  { "\xe8\x20", -1, 0, FALSE },
  { "\xe9\x20", -1, 0, FALSE },
  { "\xea\x20", -1, 0, FALSE },
  { "\xeb\x20", -1, 0, FALSE },
  { "\xec\x20", -1, 0, FALSE },
  { "\xed\x20", -1, 0, FALSE },
  { "\xee\x20", -1, 0, FALSE },
  { "\xef\x20", -1, 0, FALSE },
  { "\xf0\x20", -1, 0, FALSE },
  { "\xf1\x20", -1, 0, FALSE },
  { "\xf2\x20", -1, 0, FALSE },
  { "\xf3\x20", -1, 0, FALSE },
  { "\xf4\x20", -1, 0, FALSE },
  { "\xf5\x20", -1, 0, FALSE },
  { "\xf6\x20", -1, 0, FALSE },
  { "\xf7\x20", -1, 0, FALSE },
  { "\xf8\x20", -1, 0, FALSE },
  { "\xf9\x20", -1, 0, FALSE },
  { "\xfa\x20", -1, 0, FALSE },
  { "\xfb\x20", -1, 0, FALSE },
  { "\xfc\x20", -1, 0, FALSE },
  { "\xfd\x20", -1, 0, FALSE },
  /* missing continuation bytes */
  { "\x20\xc0", -1, 1, FALSE },
  { "\x20\xe0\x80", -1, 1, FALSE },
  { "\x20\xf0\x80\x80", -1, 1, FALSE },
  { "\x20\xf8\x80\x80\x80", -1, 1, FALSE },
  { "\x20\xfc\x80\x80\x80\x80", -1, 1, FALSE },
  { "\x20\xdf", -1, 1, FALSE },
  { "\x20\xef\xbf", -1, 1, FALSE },
  { "\x20\xf7\xbf\xbf", -1, 1, FALSE },
  { "\x20\xfb\xbf\xbf\xbf", -1, 1, FALSE },
  { "\x20\xfd\xbf\xbf\xbf\xbf", -1, 1, FALSE },
  /* impossible bytes */
  { "\x20\xfe\x20", -1, 1, FALSE },
  { "\x20\xff\x20", -1, 1, FALSE },
  /* overlong sequences */
  { "\x20\xc0\xaf\x20", -1, 1, FALSE },
  { "\x20\xe0\x80\xaf\x20", -1, 1, FALSE },
  { "\x20\xf0\x80\x80\xaf\x20", -1, 1, FALSE },
  { "\x20\xf8\x80\x80\x80\xaf\x20", -1, 1, FALSE },
  { "\x20\xfc\x80\x80\x80\x80\xaf\x20", -1, 1, FALSE },
  { "\x20\xc1\xbf\x20", -1, 1, FALSE },
  { "\x20\xe0\x9f\xbf\x20", -1, 1, FALSE },
  { "\x20\xf0\x8f\xbf\xbf\x20", -1, 1, FALSE },
  { "\x20\xf8\x87\xbf\xbf\xbf\x20", -1, 1, FALSE },
  { "\x20\xfc\x83\xbf\xbf\xbf\xbf\x20", -1, 1, FALSE },
  { "\x20\xc0\x80\x20", -1, 1, FALSE },
  { "\x20\xe0\x80\x80\x20", -1, 1, FALSE },
  { "\x20\xf0\x80\x80\x80\x20", -1, 1, FALSE },
  { "\x20\xf8\x80\x80\x80\x80\x20", -1, 1, FALSE },
  { "\x20\xfc\x80\x80\x80\x80\x80\x20", -1, 1, FALSE },
  /* illegal code positions */
  { "\x20\xed\xa0\x80\x20", -1, 1, FALSE },
  { "\x20\xed\xad\xbf\x20", -1, 1, FALSE },
  { "\x20\xed\xae\x80\x20", -1, 1, FALSE },
  { "\x20\xed\xaf\xbf\x20", -1, 1, FALSE },
  { "\x20\xed\xb0\x80\x20", -1, 1, FALSE },
  { "\x20\xed\xbe\x80\x20", -1, 1, FALSE },
  { "\x20\xed\xbf\xbf\x20", -1, 1, FALSE },
  { "\x20\xed\xa0\x80\xed\xb0\x80\x20", -1, 1, FALSE },
  { "\x20\xed\xa0\x80\xed\xbf\xbf\x20", -1, 1, FALSE },
  { "\x20\xed\xad\xbf\xed\xb0\x80\x20", -1, 1, FALSE },
  { "\x20\xed\xad\xbf\xed\xbf\xbf\x20", -1, 1, FALSE },
  { "\x20\xed\xae\x80\xed\xb0\x80\x20", -1, 1, FALSE },
  { "\x20\xed\xae\x80\xed\xbf\xbf\x20", -1, 1, FALSE },
  { "\x20\xed\xaf\xbf\xed\xb0\x80\x20", -1, 1, FALSE },
  { "\x20\xed\xaf\xbf\xed\xbf\xbf\x20", -1, 1, FALSE },
#if 0 /* We don't consider U+FFFE / U+FFFF and similar invalid. */
  { "\x20\xef\xbf\xbe\x20", -1, 1, FALSE },
  { "\x20\xef\xbf\xbf\x20", -1, 1, FALSE },
#endif
  { "", -1, 0, TRUE }
};

static void
test_buffer_utf8_validity (void)
{
  hb_buffer_t *b;
  unsigned int i;

  b = hb_buffer_create ();
  hb_buffer_set_replacement_codepoint (b, (hb_codepoint_t) -1);

  for (i = 0; i < G_N_ELEMENTS (utf8_validity_tests); i++)
  {
    const utf8_validity_test_t *test = &utf8_validity_tests[i];
    unsigned int text_bytes, segment_bytes, j, len;
    hb_glyph_info_t *glyphs;
    char *escaped;

    escaped = g_strescape (test->utf8, NULL);
    g_test_message ("UTF-8 test #%d: %s", i, escaped);
    g_free (escaped);

    text_bytes = strlen (test->utf8);
    if (test->max_len == -1)
      segment_bytes = text_bytes;
    else
      segment_bytes = test->max_len;

    hb_buffer_clear_contents (b);
    hb_buffer_add_utf8 (b, test->utf8, text_bytes,  0, segment_bytes);

    glyphs = hb_buffer_get_glyph_infos (b, &len);
    for (j = 0; j < len; j++)
      if (glyphs[j].codepoint == (hb_codepoint_t) -1)
	break;

    g_assert (test->valid ? j == len : j < len);
    if (!test->valid)
      g_assert (glyphs[j].cluster == test->offset);
  }

  hb_buffer_destroy (b);
}


typedef struct {
  const uint16_t utf16[8];
  const uint32_t codepoints[8];
} utf16_conversion_test_t;

/* note: we skip the first and last item from utf16 when adding to buffer */
static const utf16_conversion_test_t utf16_conversion_tests[] = {
  {{0x41, 0x004D, 0x0430, 0x4E8C, 0xD800, 0xDF02, 0x61} , {0x004D, 0x0430, 0x4E8C, 0x10302}},
  {{0x41, 0xD800, 0xDF02, 0x61}, {0x10302}},
  {{0x41, 0xD800, 0xDF02}, {(hb_codepoint_t) -1}},
  {{0x41, 0x61, 0xD800, 0xDF02}, {0x61, (hb_codepoint_t) -1}},
  {{0x41, 0xD800, 0x61, 0xDF02}, {(hb_codepoint_t) -1, 0x61}},
  {{0x41, 0xDF00, 0x61}, {(hb_codepoint_t) -1}},
  {{0x41, 0x61}, {0}}
};

static void
test_buffer_utf16_conversion (void)
{
  hb_buffer_t *b;
  unsigned int i;

  b = hb_buffer_create ();
  hb_buffer_set_replacement_codepoint (b, (hb_codepoint_t) -1);

  for (i = 0; i < G_N_ELEMENTS (utf16_conversion_tests); i++)
  {
    const utf16_conversion_test_t *test = &utf16_conversion_tests[i];
    unsigned int u_len, chars, j, len;
    hb_glyph_info_t *glyphs;

    g_test_message ("UTF-16 test #%d", i);

    for (u_len = 0; test->utf16[u_len]; u_len++)
      ;
    for (chars = 0; test->codepoints[chars]; chars++)
      ;

    hb_buffer_clear_contents (b);
    hb_buffer_add_utf16 (b, test->utf16, u_len,  1, u_len - 2);

    glyphs = hb_buffer_get_glyph_infos (b, &len);
    g_assert_cmpint (len, ==, chars);
    for (j = 0; j < chars; j++)
      g_assert_cmphex (glyphs[j].codepoint, ==, test->codepoints[j]);
  }

  hb_buffer_destroy (b);
}


typedef struct {
  const uint32_t utf32[8];
  const uint32_t codepoints[8];
} utf32_conversion_test_t;

/* note: we skip the first and last item from utf32 when adding to buffer */
static const utf32_conversion_test_t utf32_conversion_tests[] = {
  {{0x41, 0x004D, 0x0430, 0x4E8C, 0xD800, 0xDF02, 0x61} , {0x004D, 0x0430, 0x4E8C, (hb_codepoint_t) -3, (hb_codepoint_t) -3}},
  {{0x41, 0x004D, 0x0430, 0x4E8C, 0x10302, 0x61} , {0x004D, 0x0430, 0x4E8C, 0x10302}},
  {{0x41, 0xD800, 0xDF02, 0x61}, {(hb_codepoint_t) -3, (hb_codepoint_t) -3}},
  {{0x41, 0xD800, 0xDF02}, {(hb_codepoint_t) -3}},
  {{0x41, 0x61, 0xD800, 0xDF02}, {0x61, (hb_codepoint_t) -3}},
  {{0x41, 0xD800, 0x61, 0xDF02}, {(hb_codepoint_t) -3, 0x61}},
  {{0x41, 0xDF00, 0x61}, {(hb_codepoint_t) -3}},
  {{0x41, 0x10FFFF, 0x61}, {0x10FFFF}},
  {{0x41, 0x110000, 0x61}, {(hb_codepoint_t) -3}},
  {{0x41, 0x61}, {0}}
};

static void
test_buffer_utf32_conversion (void)
{
  hb_buffer_t *b;
  unsigned int i;

  b = hb_buffer_create ();
  hb_buffer_set_replacement_codepoint (b, (hb_codepoint_t) -3);

  for (i = 0; i < G_N_ELEMENTS (utf32_conversion_tests); i++)
  {
    const utf32_conversion_test_t *test = &utf32_conversion_tests[i];
    unsigned int u_len, chars, j, len;
    hb_glyph_info_t *glyphs;

    g_test_message ("UTF-32 test #%d", i);

    for (u_len = 0; test->utf32[u_len]; u_len++)
      ;
    for (chars = 0; test->codepoints[chars]; chars++)
      ;

    hb_buffer_clear_contents (b);
    hb_buffer_add_utf32 (b, test->utf32, u_len,  1, u_len - 2);

    glyphs = hb_buffer_get_glyph_infos (b, &len);
    g_assert_cmpint (len, ==, chars);
    for (j = 0; j < chars; j++)
      g_assert_cmphex (glyphs[j].codepoint, ==, test->codepoints[j]);
  }

  hb_buffer_destroy (b);
}


static void
test_empty (hb_buffer_t *b)
{
  g_assert_cmpint (hb_buffer_get_length (b), ==, 0);
  g_assert (!hb_buffer_get_glyph_infos (b, NULL));
  g_assert (!hb_buffer_get_glyph_positions (b, NULL));
}

static void
test_buffer_empty (void)
{
  hb_buffer_t *b = hb_buffer_get_empty ();

  g_assert (hb_buffer_get_empty ());
  g_assert (hb_buffer_get_empty () == b);

  g_assert (!hb_buffer_allocation_successful (b));

  test_empty (b);

  hb_buffer_add_utf32 (b, utf32, G_N_ELEMENTS (utf32), 1, G_N_ELEMENTS (utf32) - 2);

  test_empty (b);

  hb_buffer_reverse (b);
  hb_buffer_reverse_clusters (b);

  g_assert (!hb_buffer_set_length (b, 10));

  test_empty (b);

  g_assert (hb_buffer_set_length (b, 0));

  test_empty (b);

  g_assert (!hb_buffer_allocation_successful (b));

  hb_buffer_reset (b);

  test_empty (b);

  g_assert (!hb_buffer_allocation_successful (b));
}

int
main (int argc, char **argv)
{
  unsigned int i;

  hb_test_init (&argc, &argv);

  for (i = 0; i < BUFFER_NUM_TYPES; i++)
  {
    const void *buffer_type = GINT_TO_POINTER (i);
    const char *buffer_name = buffer_names[i];

    hb_test_add_fixture_flavor (fixture, buffer_type, buffer_name, test_buffer_properties);
    hb_test_add_fixture_flavor (fixture, buffer_type, buffer_name, test_buffer_contents);
    hb_test_add_fixture_flavor (fixture, buffer_type, buffer_name, test_buffer_positions);
  }

  hb_test_add_fixture (fixture, GINT_TO_POINTER (BUFFER_EMPTY), test_buffer_allocation);

  hb_test_add (test_buffer_utf8_conversion);
  hb_test_add (test_buffer_utf8_validity);
  hb_test_add (test_buffer_utf16_conversion);
  hb_test_add (test_buffer_utf32_conversion);
  hb_test_add (test_buffer_empty);

  return hb_test_run();
}
