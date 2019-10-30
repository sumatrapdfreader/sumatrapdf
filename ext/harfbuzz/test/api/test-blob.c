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

/* Unit tests for hb-blob.h */

#if defined(HAVE_SYS_MMAN_H) && defined(HAVE_MPROTECT) && defined(HAVE_MMAP)

# define TEST_MMAP 1

#ifdef HAVE_SYS_MMAN_H
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/mman.h>
#endif /* HAVE_SYS_MMAN_H */

#endif


static void
test_blob_empty (void)
{
  hb_blob_t *blob;
  unsigned int len;
  const char *data;
  char *data_writable;

  g_assert (hb_blob_is_immutable (hb_blob_get_empty ()));
  g_assert (hb_blob_get_empty () != NULL);
  g_assert (hb_blob_get_empty () == hb_blob_create (NULL, 0, HB_MEMORY_MODE_READONLY, NULL, NULL));
  g_assert (hb_blob_get_empty () == hb_blob_create ("asdf", 0, HB_MEMORY_MODE_READONLY, NULL, NULL));
  g_assert (hb_blob_get_empty () == hb_blob_create (NULL, -1, HB_MEMORY_MODE_READONLY, NULL, NULL));
  g_assert (hb_blob_get_empty () == hb_blob_create ("asdfg", -1, HB_MEMORY_MODE_READONLY, NULL, NULL));

  blob = hb_blob_get_empty ();
  g_assert (blob == hb_blob_get_empty ());

  len = hb_blob_get_length (blob);
  g_assert_cmpint (len, ==, 0);

  data = hb_blob_get_data (blob, NULL);
  g_assert (data == NULL);

  data = hb_blob_get_data (blob, &len);
  g_assert (data == NULL);
  g_assert_cmpint (len, ==, 0);

  data_writable = hb_blob_get_data_writable (blob, NULL);
  g_assert (data_writable == NULL);

  data_writable = hb_blob_get_data_writable (blob, &len);
  g_assert (data_writable == NULL);
  g_assert_cmpint (len, ==, 0);
}

static const char test_data[] = "test\0data";

static const char *blob_names[] = {
  "duplicate",
  "readonly",
  "writable"
#ifdef TEST_MMAP
   , "readonly-may-make-writable"
#endif
};

typedef struct
{
  hb_blob_t *blob;
  int freed;
  char *data;
  unsigned int len;
} fixture_t;

static void
free_up (fixture_t *fixture)
{
  g_assert_cmpint (fixture->freed, ==, 0);
  fixture->freed++;
}

static void
free_up_free (fixture_t *fixture)
{
  free_up (fixture);
  free (fixture->data);
}


#ifdef TEST_MMAP
static uintptr_t
get_pagesize (void)
{
  uintptr_t pagesize = (uintptr_t) -1;

#if defined(HAVE_SYSCONF) && defined(_SC_PAGE_SIZE)
  pagesize = (uintptr_t) sysconf (_SC_PAGE_SIZE);
#elif defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
  pagesize = (uintptr_t) sysconf (_SC_PAGESIZE);
#elif defined(HAVE_GETPAGESIZE)
  pagesize = (uintptr_t) getpagesize ();
#endif

  g_assert (pagesize != (uintptr_t) -1);

  return pagesize;
}

static void
free_up_munmap (fixture_t *fixture)
{
  free_up (fixture);
  munmap (fixture->data, get_pagesize ());
}
#endif

#include <errno.h>
static void
fixture_init (fixture_t *fixture, gconstpointer user_data)
{
  hb_memory_mode_t mm = (hb_memory_mode_t) GPOINTER_TO_INT (user_data);
  unsigned int len;
  const char *data;
  hb_destroy_func_t free_func;

  switch (GPOINTER_TO_INT (user_data))
  {
    case HB_MEMORY_MODE_DUPLICATE:
      data = test_data;
      len = sizeof (test_data);
      free_func = (hb_destroy_func_t) free_up;
      break;

    case HB_MEMORY_MODE_READONLY:
      data = test_data;
      len = sizeof (test_data);
      free_func = (hb_destroy_func_t) free_up;
      break;

    case HB_MEMORY_MODE_WRITABLE:
      data = malloc (sizeof (test_data));
      memcpy ((char *) data, test_data, sizeof (test_data));
      len = sizeof (test_data);
      free_func = (hb_destroy_func_t) free_up_free;
      break;

#ifdef TEST_MMAP
    case HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE:
    {
      uintptr_t pagesize = get_pagesize ();

      data = mmap (NULL, pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
      g_assert (data != (char *) -1);
      memcpy ((char *) data, test_data, sizeof (test_data));
      mprotect ((char *) data, pagesize, PROT_READ);
      len = sizeof (test_data);
      free_func = (hb_destroy_func_t) free_up_munmap;
      break;
    }
#endif

    default:
      g_assert_not_reached ();
  }

  fixture->freed = 0;
  fixture->data = (char *) data;
  fixture->len = len;
  fixture->blob = hb_blob_create (data, len, mm, fixture, free_func);
}

static void
fixture_finish (fixture_t *fixture, gconstpointer user_data)
{
  hb_blob_destroy (fixture->blob);
  g_assert_cmpint (fixture->freed, ==, 1);
}


static void
test_blob (fixture_t *fixture, gconstpointer user_data)
{
  hb_blob_t *b = fixture->blob;
  hb_memory_mode_t mm = GPOINTER_TO_INT (user_data);
  unsigned int len;
  const char *data;
  char *data_writable;
  unsigned int i;

  g_assert (b);

  len = hb_blob_get_length (b);
  g_assert_cmpint (len, ==, fixture->len);

  data = hb_blob_get_data (b, &len);
  g_assert_cmpint (len, ==, fixture->len);
  if (mm == HB_MEMORY_MODE_DUPLICATE) {
    g_assert (data != fixture->data);
    g_assert_cmpint (fixture->freed, ==, 1);
    mm = HB_MEMORY_MODE_WRITABLE;
  } else {
    g_assert (data == fixture->data);
    g_assert_cmpint (fixture->freed, ==, 0);
  }

  data_writable = hb_blob_get_data_writable (b, &len);
  g_assert_cmpint (len, ==, fixture->len);
  g_assert (data_writable);
  g_assert (0 == memcmp (data_writable, fixture->data, fixture->len));
  if (mm == HB_MEMORY_MODE_READONLY) {
    g_assert (data_writable != data);
    g_assert_cmpint (fixture->freed, ==, 1);
  } else {
    g_assert (data_writable == data);
  }

  data = hb_blob_get_data (b, &len);
  g_assert_cmpint (len, ==, fixture->len);
  g_assert (data == data_writable);

  memset (data_writable, 0, fixture->len);

  /* Now, make it immutable and watch get_data_writable() fail */

  g_assert (!hb_blob_is_immutable (b));
  hb_blob_make_immutable (b);
  g_assert (hb_blob_is_immutable (b));

  data_writable = hb_blob_get_data_writable (b, &len);
  g_assert (!data_writable);
  g_assert_cmpint (len, ==, 0);

  data = hb_blob_get_data (b, &len);
  g_assert_cmpint (len, ==, fixture->len);
  for (i = 0; i < len; i++)
    g_assert ('\0' == data[i]);
}

static void
test_blob_subblob (fixture_t *fixture, gconstpointer user_data)
{
  hb_blob_t *b = fixture->blob;
  hb_memory_mode_t mm = GPOINTER_TO_INT (user_data);
  unsigned int len;
  const char *data;
  char *data_writable;
  unsigned int i;

  if (mm == HB_MEMORY_MODE_DUPLICATE) {
    g_assert_cmpint (fixture->freed, ==, 1);
    fixture->data = (char *) hb_blob_get_data (b, NULL);
  } else {
    g_assert_cmpint (fixture->freed, ==, 0);
  }
  fixture->blob = hb_blob_create_sub_blob (b, 1, fixture->len - 2);
  hb_blob_destroy (b);
  b = fixture->blob;

  /* A sub-blob is always created READONLY. */

  g_assert (b);

  len = hb_blob_get_length (b);
  g_assert_cmpint (len, ==, fixture->len - 2);

  data = hb_blob_get_data (b, &len);
  g_assert_cmpint (len, ==, fixture->len - 2);
  g_assert (data == fixture->data + 1);

  data_writable = hb_blob_get_data_writable (b, &len);
  g_assert_cmpint (len, ==, fixture->len - 2);
  g_assert (data_writable);
  if (mm == HB_MEMORY_MODE_READONLY)
    g_assert (0 == memcmp (data_writable, fixture->data + 1, fixture->len - 2));
  g_assert (data_writable != data);
  g_assert_cmpint (fixture->freed, ==, 1);

  data = hb_blob_get_data (b, &len);
  g_assert_cmpint (len, ==, fixture->len - 2);
  g_assert (data == data_writable);

  memset (data_writable, 0, fixture->len - 2);

  /* Now, make it immutable and watch get_data_writable() fail */

  g_assert (!hb_blob_is_immutable (b));
  hb_blob_make_immutable (b);
  g_assert (hb_blob_is_immutable (b));

  data_writable = hb_blob_get_data_writable (b, &len);
  g_assert (!data_writable);
  g_assert_cmpint (len, ==, 0);

  data = hb_blob_get_data (b, &len);
  g_assert_cmpint (len, ==, fixture->len - 2);
  for (i = 0; i < len; i++)
    g_assert ('\0' == data[i]);
}


int
main (int argc, char **argv)
{
  unsigned int i;

  hb_test_init (&argc, &argv);

  hb_test_add (test_blob_empty);

  for (i = 0; i < G_N_ELEMENTS (blob_names); i++)
  {
    const void *blob_type = GINT_TO_POINTER (i);
    const char *blob_name = blob_names[i];

    hb_test_add_fixture_flavor (fixture, blob_type, blob_name, test_blob);
    hb_test_add_fixture_flavor (fixture, blob_type, blob_name, test_blob_subblob);
  }

  /*
   * create_sub_blob
   */

  return hb_test_run ();
}
