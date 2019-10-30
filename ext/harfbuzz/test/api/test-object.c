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

/* Unit tests for hb-object-private.h */


static void *
create_blob (void)
{
  static char data[] = "test data";
  return hb_blob_create (data, sizeof (data), HB_MEMORY_MODE_READONLY, NULL, NULL);
}
static void *
create_blob_from_inert (void)
{
  return hb_blob_create (NULL, 0, HB_MEMORY_MODE_DUPLICATE, NULL, NULL);
}

static void *
create_buffer (void)
{
  return hb_buffer_create ();
}
static void *
create_buffer_from_inert (void)
{
  return NULL;
}

static void *
create_set (void)
{
  return hb_set_create ();
}
static void *
create_set_from_inert (void)
{
  return NULL;
}

static void *
create_face (void)
{
  hb_blob_t *blob = (hb_blob_t *) create_blob ();
  hb_face_t *face = hb_face_create (blob, 0);
  hb_blob_destroy (blob);
  return face;
}
static void *
create_face_from_inert (void)
{
  return hb_face_create (hb_blob_get_empty (), 0);
}

static void *
create_font (void)
{
  hb_face_t *face = (hb_face_t *) create_face ();
  hb_font_t *font = hb_font_create (face);
  hb_face_destroy (face);
  return font;
}
static void *
create_font_from_inert (void)
{
  return hb_font_create (hb_face_get_empty ());
}

static void *
create_font_funcs (void)
{
  return hb_font_funcs_create ();
}
static void *
create_font_funcs_from_inert (void)
{
  return NULL;
}

static void *
create_unicode_funcs (void)
{
  return hb_unicode_funcs_create (NULL);
}
static void *
create_unicode_funcs_from_inert (void)
{
  return hb_unicode_funcs_create (hb_unicode_funcs_get_empty ());
}



typedef void     *(*create_func_t)         (void);
typedef void     *(*reference_func_t)      (void *obj);
typedef void      (*destroy_func_t)        (void *obj);
typedef hb_bool_t (*set_user_data_func_t)  (void *obj, hb_user_data_key_t *key, void *data, hb_destroy_func_t destroy, hb_bool_t replace);
typedef void *    (*get_user_data_func_t)  (void *obj, hb_user_data_key_t *key);
typedef void      (*make_immutable_func_t) (void *obj);
typedef hb_bool_t (*is_immutable_func_t)   (void *obj);

typedef struct {
  create_func_t          create;
  create_func_t          create_from_inert;
  create_func_t          get_empty;
  reference_func_t       reference;
  destroy_func_t         destroy;
  set_user_data_func_t   set_user_data;
  get_user_data_func_t   get_user_data;
  make_immutable_func_t  make_immutable;
  is_immutable_func_t    is_immutable;
  const char            *name;
} object_t;

#define OBJECT_WITHOUT_IMMUTABILITY(name) \
  { \
    (create_func_t)         create_##name, \
    (create_func_t)         create_##name##_from_inert, \
    (create_func_t)         hb_##name##_get_empty, \
    (reference_func_t)      hb_##name##_reference, \
    (destroy_func_t)        hb_##name##_destroy, \
    (set_user_data_func_t)  hb_##name##_set_user_data, \
    (get_user_data_func_t)  hb_##name##_get_user_data, \
    (make_immutable_func_t) NULL, \
    (is_immutable_func_t)   NULL, \
    #name, \
  }
#define OBJECT_WITH_IMMUTABILITY(name) \
  { \
    (create_func_t)         create_##name, \
    (create_func_t)         create_##name##_from_inert, \
    (create_func_t)         hb_##name##_get_empty, \
    (reference_func_t)      hb_##name##_reference, \
    (destroy_func_t)        hb_##name##_destroy, \
    (set_user_data_func_t)  hb_##name##_set_user_data, \
    (get_user_data_func_t)  hb_##name##_get_user_data, \
    (make_immutable_func_t) hb_##name##_make_immutable, \
    (is_immutable_func_t)   hb_##name##_is_immutable, \
    #name, \
  }
static const object_t objects[] =
{
  OBJECT_WITHOUT_IMMUTABILITY (buffer),
  OBJECT_WITHOUT_IMMUTABILITY (set),
  OBJECT_WITH_IMMUTABILITY (blob),
  OBJECT_WITH_IMMUTABILITY (face),
  OBJECT_WITH_IMMUTABILITY (font),
  OBJECT_WITH_IMMUTABILITY (font_funcs),
  OBJECT_WITH_IMMUTABILITY (unicode_funcs)
};
#undef OBJECT


#define MAGIC0 0x12345678
#define MAGIC1 0x76543210

typedef struct {
  int value;
  gboolean freed;
} data_t;

static int global_data;

static void global_free_up (void *p G_GNUC_UNUSED)
{
  global_data++;
}

static void free_up0 (void *p)
{
  data_t *data = (data_t *) p;

  g_assert_cmphex (data->value, ==, MAGIC0);
  g_assert (!data->freed);
  data->freed = TRUE;
}

static void free_up1 (void *p)
{
  data_t *data = (data_t *) p;

  g_assert_cmphex (data->value, ==, MAGIC1);
  g_assert (!data->freed);
  data->freed = TRUE;
}


typedef struct {
  const object_t *klass;
  void *object;
  hb_user_data_key_t key;
} deadlock_test_t;

static void free_deadlock_test (void *p)
{
  deadlock_test_t *t = (deadlock_test_t *) p;

  g_assert (NULL == t->klass->get_user_data (t->object, &t->key));
}


static void
test_object (void)
{
  unsigned int i;

  for (i = 0; i < G_N_ELEMENTS (objects); i++) {
    const object_t *o = &objects[i];
    void *obj;
    hb_user_data_key_t key[1001];

    {
      unsigned int j;
      data_t data[2] = {{MAGIC0, FALSE}, {MAGIC1, FALSE}};
      deadlock_test_t deadlock_test;

      g_test_message ("Testing object %s", o->name);

      g_test_message ("->create()");
      obj = o->create ();
      g_assert (obj);

      g_assert (obj == o->reference (obj));
      o->destroy (obj);

      if (o->is_immutable)
	g_assert (!o->is_immutable (obj));

      g_assert (o->set_user_data (obj, &key[0], &data[0], free_up0, TRUE));
      g_assert (o->get_user_data (obj, &key[0]) == &data[0]);

      if (o->is_immutable) {
	o->make_immutable (obj);
	g_assert (o->is_immutable (obj));
      }

      /* Should still work even if object is made immutable */
      g_assert (o->set_user_data (obj, &key[1], &data[1], free_up1, TRUE));
      g_assert (o->get_user_data (obj, &key[1]) == &data[1]);

      g_assert (!o->set_user_data (obj, NULL, &data[0], free_up0, TRUE));
      g_assert (o->get_user_data (obj, &key[0]) == &data[0]);
      g_assert (o->set_user_data (obj, &key[0], &data[1], NULL, TRUE));
      g_assert (data[0].freed);
      g_assert (o->get_user_data (obj, &key[0]) == &data[1]);
      g_assert (!data[1].freed);

      data[0].freed = FALSE;
      g_assert (o->set_user_data (obj, &key[0], &data[0], free_up0, TRUE));
      g_assert (!data[0].freed);
      g_assert (o->set_user_data (obj, &key[0], NULL, NULL, TRUE));
      g_assert (data[0].freed);

      data[0].freed = FALSE;
      global_data = 0;
      g_assert (o->set_user_data (obj, &key[0], &data[0], free_up0, TRUE));
      g_assert (!o->set_user_data (obj, &key[0], &data[0], free_up0, FALSE));
      g_assert_cmpuint (global_data, ==, 0);
      g_assert (o->set_user_data (obj, &key[0], NULL, global_free_up, TRUE));
      g_assert_cmpuint (global_data, ==, 0);
      g_assert (o->set_user_data (obj, &key[0], NULL, NULL, TRUE));
      g_assert_cmpuint (global_data, ==, 1);

      global_data = 0;
      for (j = 2; j < 1000; j++)
	g_assert (o->set_user_data (obj, &key[j], &data[j], global_free_up, TRUE));
      for (j = 2; j < 1000; j++)
	g_assert (o->get_user_data (obj, &key[j]) == &data[j]);
      for (j = 100; j < 1000; j++)
	g_assert (o->set_user_data (obj, &key[j], NULL, NULL, TRUE));
      for (j = 2; j < 100; j++)
	g_assert (o->get_user_data (obj, &key[j]) == &data[j]);
      for (j = 100; j < 1000; j++)
	g_assert (!o->get_user_data (obj, &key[j]));
      g_assert_cmpuint (global_data, ==, 900);

      /* Test set_user_data where the destroy() func calls user_data functions.
       * Make sure it doesn't deadlock or corrupt memory. */
      deadlock_test.klass = o;
      deadlock_test.object = obj;
      g_assert (o->set_user_data (obj, &deadlock_test.key, &deadlock_test, free_deadlock_test, TRUE));
      g_assert (o->set_user_data (obj, &deadlock_test.key, NULL, NULL, TRUE));

      g_assert (!data[1].freed);
      o->destroy (obj);
      g_assert (data[0].freed);
      g_assert (data[1].freed);
      g_assert_cmpuint (global_data, ==, 1000-2);
    }

    {
      data_t data[2] = {{MAGIC0, FALSE}, {MAGIC1, FALSE}};

      g_test_message ("->get_empty()");
      obj = o->get_empty ();
      g_assert (obj);

      g_assert (obj == o->reference (obj));
      o->destroy (obj);

      if (o->is_immutable)
	g_assert (o->is_immutable (obj));

      g_assert (!o->set_user_data (obj, &key[0], &data[0], free_up0, TRUE));
      g_assert (!o->get_user_data (obj, &key[0]));

      o->destroy (obj);
      o->destroy (obj);
      o->destroy (obj);
      o->destroy (obj);
      o->destroy (obj);

      g_assert (!data[0].freed);
    }

    {
      data_t data[2] = {{MAGIC0, FALSE}, {MAGIC1, FALSE}};

      g_test_message ("->create_from_inert()");
      obj = o->create_from_inert ();
      if (!obj)
	continue;
      if (obj == o->get_empty ())
        continue; /* Tested already */

      g_assert (obj == o->reference (obj));
      o->destroy (obj);

      if (o->is_immutable)
	g_assert (!o->is_immutable (obj));

      g_assert (o->set_user_data (obj, &key[0], &data[0], free_up0, TRUE));
      g_assert (o->get_user_data (obj, &key[0]));

      o->destroy (obj);

      g_assert (data[0].freed);
    }
  }
}


int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_object);

  return hb_test_run ();
}
