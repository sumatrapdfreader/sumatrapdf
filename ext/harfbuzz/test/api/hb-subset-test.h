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

#ifndef HB_SUBSET_TEST_H
#define HB_SUBSET_TEST_H

#include <stdio.h>

#include "hb-test.h"
#include "hb-subset.h"

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
typedef short bool;
# ifndef true
#  define true 1
# endif
# ifndef false
#  define false 0
# endif
#endif


HB_BEGIN_DECLS

static inline hb_face_t *
hb_subset_test_open_font (const char *font_path)
{
#if GLIB_CHECK_VERSION(2,37,2)
  char* path = g_test_build_filename(G_TEST_DIST, font_path, NULL);
#else
  char* path = g_strdup(font_path);
#endif

  return hb_face_create (hb_blob_create_from_file (path), 0);
}

static inline hb_subset_input_t *
hb_subset_test_create_input(const hb_set_t  *codepoints)
{
  hb_subset_input_t *input = hb_subset_input_create_or_fail ();
  hb_set_t * input_codepoints = hb_subset_input_unicode_set (input);
  hb_set_union (input_codepoints, codepoints);
  return input;
}

static inline hb_face_t *
hb_subset_test_create_subset (hb_face_t *source,
                              hb_subset_input_t *input)
{
  hb_face_t *subset = hb_subset (source, input);
  g_assert (subset);

  hb_subset_input_destroy (input);
  return subset;
}

static inline void
hb_subset_test_check (hb_face_t *expected,
                      hb_face_t *actual,
                      hb_tag_t   table)
{
  hb_blob_t *expected_blob, *actual_blob;
  fprintf(stderr, "compare %c%c%c%c\n", HB_UNTAG(table));
  expected_blob = hb_face_reference_table (expected, table);
  actual_blob = hb_face_reference_table (actual, table);
  hb_test_assert_blobs_equal (expected_blob, actual_blob);
  hb_blob_destroy (expected_blob);
  hb_blob_destroy (actual_blob);
}


HB_END_DECLS

#endif /* HB_SUBSET_TEST_H */
