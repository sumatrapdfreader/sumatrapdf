/*
 * Copyright © 2011  Codethink Limited
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
 * Codethink Author(s): Ryan Lortie
 * Google Author(s): Behdad Esfahbod
 */

#include "hb-test.h"

/* Unit tests for hb-unicode.h */
/* Unit tests for hb-glib.h */
/* Unit tests for hb-icu.h */


#ifdef HAVE_GLIB
#include <hb-glib.h>
#endif
#ifdef HAVE_ICU
#include <hb-icu.h>
#endif


/* Some useful stuff */

#define MAGIC0 0x12345678
#define MAGIC1 0x76543210

typedef struct {
  int value;
  gboolean freed;
} data_t;

static void free_up (void *p)
{
  data_t *data = (data_t *) p;

  g_assert (data->value == MAGIC0 || data->value == MAGIC1);
  g_assert (!data->freed);
  data->freed = TRUE;
}

static hb_script_t
simple_get_script (hb_unicode_funcs_t *ufuncs,
                   hb_codepoint_t      codepoint,
                   void               *user_data)
{
  data_t *data = (data_t *) user_data;

  g_assert (hb_unicode_funcs_get_parent (ufuncs) != NULL);
  g_assert_cmphex (data->value, ==, MAGIC0);
  g_assert (!data->freed);

  if ('a' <= codepoint && codepoint <= 'z')
    return HB_SCRIPT_LATIN;
  else
    return HB_SCRIPT_UNKNOWN;
}

static hb_script_t
a_is_for_arabic_get_script (hb_unicode_funcs_t *ufuncs,
                            hb_codepoint_t      codepoint,
                            void               *user_data)
{
  data_t *data = (data_t *) user_data;

  g_assert (hb_unicode_funcs_get_parent (ufuncs) != NULL);
  g_assert_cmphex (data->value, ==, MAGIC1);
  g_assert (!data->freed);

  if (codepoint == 'a') {
    return HB_SCRIPT_ARABIC;
  } else {
    hb_unicode_funcs_t *parent = hb_unicode_funcs_get_parent (ufuncs);

    return hb_unicode_script (parent, codepoint);
  }
}



/* Check all properties */

/* Some of the following tables where adapted from glib/glib/tests/utf8-misc.c.
 * The license is compatible. */

typedef struct {
  hb_codepoint_t unicode;
  unsigned int   value;
} test_pair_t;

static const test_pair_t combining_class_tests[] =
{
  {   0x0020, 0 },
  {   0x0334, 1 },
  {   0x093C, 7 },
  {   0x3099, 8 },
  {   0x094D, 9 },
  {   0x05B0, 10 },
  {   0x05B1, 11 },
  {   0x05B2, 12 },
  {   0x05B3, 13 },
  {   0x05B4, 14 },
  {   0x05B5, 15 },
  {   0x05B6, 16 },
  {   0x05B7, 17 },
  {   0x05B8, 18 },
  {   0x05B9, 19 },
  {   0x05BB, 20 },
  {   0x05BC, 21 },
  {   0x05BD, 22 },
  {   0x05BF, 23 },
  {   0x05C1, 24 },
  {   0x05C2, 25 },
  {   0xFB1E, 26 },
  {   0x064B, 27 },
  {   0x064C, 28 },
  {   0x064D, 29 },
  /* ... */
  {   0x05AE, 228 },
  {   0x0300, 230 },
  {   0x302C, 232 },
  {   0x0362, 233 },
  {   0x0360, 234 },
  {   0x0345, 240 },

  { 0x111111, 0 }
};
static const test_pair_t combining_class_tests_more[] =
{
  /* Unicode-5.1 character additions */
  {   0x1DCD, 234 },

  /* Unicode-5.2 character additions */
  {   0xA8E0, 230 },

  /* Unicode-6.0 character additions */
  {   0x135D, 230 },

  { 0x111111, 0 }
};

static const test_pair_t eastasian_width_tests[] =
{
  /* Neutral */
  {   0x0000, 1 },
  {   0x0483, 1 },
  {   0x0641, 1 },
  {   0xFFFC, 1 },
  {  0x10000, 1 },
  {  0xE0001, 1 },

  /* Narrow */
  {   0x0020, 1 },
  {   0x0041, 1 },
  {   0x27E6, 1 },

  /* Halfwidth */
  {   0x20A9, 1 },
  {   0xFF61, 1 },
  {   0xFF69, 1 },
  {   0xFFEE, 1 },

  /* Ambiguous */
  {   0x00A1, 1 },
  {   0x00D8, 1 },
  {   0x02DD, 1 },
  {  0xE0100, 1 },
  { 0x100000, 1 },

  /* Fullwidth */
  {   0x3000, 2 },
  {   0xFF60, 2 },

  /* Wide */
  {   0x2329, 2 },
  {   0x3001, 2 },
  {   0xFE69, 2 },
  {  0x30000, 2 },
  {  0x3FFFD, 2 },

  { 0x111111, 1 }
};
static const test_pair_t eastasian_width_tests_more[] =
{
  /* Default Wide blocks */
  {   0x4DBF, 2 },
  {   0x9FFF, 2 },
  {   0xFAFF, 2 },
  {  0x2A6DF, 2 },
  {  0x2B73F, 2 },
  {  0x2B81F, 2 },
  {  0x2FA1F, 2 },

  /* Uniode-5.2 character additions */
  /* Wide */
  {   0x115F, 2 },

  /* Uniode-6.0 character additions */
  /* Wide */
  {  0x2B740, 2 },
  {  0x1B000, 2 },

  { 0x111111, 1 }
};

static const test_pair_t general_category_tests[] =
{
  {   0x000D, HB_UNICODE_GENERAL_CATEGORY_CONTROL },
  {   0x200E, HB_UNICODE_GENERAL_CATEGORY_FORMAT },
  {   0x0378, HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED },
  {   0xE000, HB_UNICODE_GENERAL_CATEGORY_PRIVATE_USE },
  {   0xD800, HB_UNICODE_GENERAL_CATEGORY_SURROGATE },
  {   0x0061, HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER },
  {   0x02B0, HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER },
  {   0x3400, HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER },
  {   0x01C5, HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER },
  {   0xFF21, HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER },
  {   0x0903, HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK },
  {   0x20DD, HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK },
  {   0xA806, HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK },
  {   0xFF10, HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER },
  {   0x16EE, HB_UNICODE_GENERAL_CATEGORY_LETTER_NUMBER },
  {   0x17F0, HB_UNICODE_GENERAL_CATEGORY_OTHER_NUMBER },
  {   0x005F, HB_UNICODE_GENERAL_CATEGORY_CONNECT_PUNCTUATION },
  {   0x058A, HB_UNICODE_GENERAL_CATEGORY_DASH_PUNCTUATION },
  {   0x0F3B, HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION },
  {   0x2019, HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION },
  {   0x2018, HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION },
  {   0x2016, HB_UNICODE_GENERAL_CATEGORY_OTHER_PUNCTUATION },
  {   0x0F3A, HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION },
  {   0x20A0, HB_UNICODE_GENERAL_CATEGORY_CURRENCY_SYMBOL },
  {   0x309B, HB_UNICODE_GENERAL_CATEGORY_MODIFIER_SYMBOL },
  {   0xFB29, HB_UNICODE_GENERAL_CATEGORY_MATH_SYMBOL },
  {   0x00A6, HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL },
  {   0x2028, HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR },
  {   0x2029, HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR },
  {   0x202F, HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR },

  { 0x111111, HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED }
};
static const test_pair_t general_category_tests_more[] =
{
  /* Unicode-5.2 character additions */
  {  0x1F131, HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL },

  /* Unicode-6.0 character additions */
  {   0x0620, HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER },

  { 0x111111, HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED }
};

static const test_pair_t mirroring_tests[] =
{
  /* Some characters that do NOT mirror */
  {   0x0020, 0x0020 },
  {   0x0041, 0x0041 },
  {   0x00F0, 0x00F0 },
  {   0x27CC, 0x27CC },
  {  0xE01EF, 0xE01EF },
  {  0x1D7C3, 0x1D7C3 },
  { 0x100000, 0x100000 },

  /* Some characters that do mirror */
  {   0x0029, 0x0028 },
  {   0x0028, 0x0029 },
  {   0x003E, 0x003C },
  {   0x003C, 0x003E },
  {   0x005D, 0x005B },
  {   0x005B, 0x005D },
  {   0x007D, 0x007B },
  {   0x007B, 0x007D },
  {   0x00BB, 0x00AB },
  {   0x00AB, 0x00BB },
  {   0x226B, 0x226A },
  {   0x226A, 0x226B },
  {   0x22F1, 0x22F0 },
  {   0x22F0, 0x22F1 },
  {   0xFF60, 0xFF5F },
  {   0xFF5F, 0xFF60 },
  {   0xFF63, 0xFF62 },
  {   0xFF62, 0xFF63 },

  { 0x111111, 0x111111 },
};
static const test_pair_t mirroring_tests_more[] =
{
  /* No new mirroring characters have been encoded in recent Unicode versions. */
  { 0x111111, 0x111111 }
};

static const test_pair_t script_tests[] =
{
  {   0x002A, HB_SCRIPT_COMMON },
  {   0x0670, HB_SCRIPT_INHERITED },
  {   0x060D, HB_SCRIPT_ARABIC },
  {   0x0559, HB_SCRIPT_ARMENIAN },
  {   0x09CD, HB_SCRIPT_BENGALI },
  {   0x31B6, HB_SCRIPT_BOPOMOFO },
  {   0x13A2, HB_SCRIPT_CHEROKEE },
  {   0x2CFD, HB_SCRIPT_COPTIC },
  {   0x0482, HB_SCRIPT_CYRILLIC },
  {  0x10401, HB_SCRIPT_DESERET },
  {   0x094D, HB_SCRIPT_DEVANAGARI },
  {   0x1258, HB_SCRIPT_ETHIOPIC },
  {   0x10FC, HB_SCRIPT_GEORGIAN },
  {  0x10341, HB_SCRIPT_GOTHIC },
  {   0x0375, HB_SCRIPT_GREEK },
  {   0x0A83, HB_SCRIPT_GUJARATI },
  {   0x0A3C, HB_SCRIPT_GURMUKHI },
  {   0x3005, HB_SCRIPT_HAN },
  {   0x1100, HB_SCRIPT_HANGUL },
  {   0x05BF, HB_SCRIPT_HEBREW },
  {   0x309F, HB_SCRIPT_HIRAGANA },
  {   0x0CBC, HB_SCRIPT_KANNADA },
  {   0x30FF, HB_SCRIPT_KATAKANA },
  {   0x17DD, HB_SCRIPT_KHMER },
  {   0x0EDD, HB_SCRIPT_LAO },
  {   0x0061, HB_SCRIPT_LATIN },
  {   0x0D3D, HB_SCRIPT_MALAYALAM },
  {   0x1843, HB_SCRIPT_MONGOLIAN },
  {   0x1031, HB_SCRIPT_MYANMAR },
  {   0x169C, HB_SCRIPT_OGHAM },
  {  0x10322, HB_SCRIPT_OLD_ITALIC },
  {   0x0B3C, HB_SCRIPT_ORIYA },
  {   0x16EF, HB_SCRIPT_RUNIC },
  {   0x0DBD, HB_SCRIPT_SINHALA },
  {   0x0711, HB_SCRIPT_SYRIAC },
  {   0x0B82, HB_SCRIPT_TAMIL },
  {   0x0C03, HB_SCRIPT_TELUGU },
  {   0x07B1, HB_SCRIPT_THAANA },
  {   0x0E31, HB_SCRIPT_THAI },
  {   0x0FD4, HB_SCRIPT_TIBETAN },
  {   0x1401, HB_SCRIPT_CANADIAN_SYLLABICS },
  {   0xA015, HB_SCRIPT_YI },
  {   0x1700, HB_SCRIPT_TAGALOG },
  {   0x1720, HB_SCRIPT_HANUNOO },
  {   0x1740, HB_SCRIPT_BUHID },
  {   0x1760, HB_SCRIPT_TAGBANWA },

  /* Unicode-4.0 additions */
  {   0x2800, HB_SCRIPT_BRAILLE },
  {  0x10808, HB_SCRIPT_CYPRIOT },
  {   0x1932, HB_SCRIPT_LIMBU },
  {  0x10480, HB_SCRIPT_OSMANYA },
  {  0x10450, HB_SCRIPT_SHAVIAN },
  {  0x10000, HB_SCRIPT_LINEAR_B },
  {   0x1950, HB_SCRIPT_TAI_LE },
  {  0x1039F, HB_SCRIPT_UGARITIC },

  /* Unicode-4.1 additions */
  {   0x1980, HB_SCRIPT_NEW_TAI_LUE },
  {   0x1A1F, HB_SCRIPT_BUGINESE },
  {   0x2C00, HB_SCRIPT_GLAGOLITIC },
  {   0x2D6F, HB_SCRIPT_TIFINAGH },
  {   0xA800, HB_SCRIPT_SYLOTI_NAGRI },
  {  0x103D0, HB_SCRIPT_OLD_PERSIAN },
  {  0x10A3F, HB_SCRIPT_KHAROSHTHI },

  /* Unicode-5.0 additions */
  {   0x0378, HB_SCRIPT_UNKNOWN },
  {   0x1B04, HB_SCRIPT_BALINESE },
  {  0x12000, HB_SCRIPT_CUNEIFORM },
  {  0x10900, HB_SCRIPT_PHOENICIAN },
  {   0xA840, HB_SCRIPT_PHAGS_PA },
  {   0x07C0, HB_SCRIPT_NKO },

  /* Unicode-5.1 additions */
  {   0xA900, HB_SCRIPT_KAYAH_LI },
  {   0x1C00, HB_SCRIPT_LEPCHA },
  {   0xA930, HB_SCRIPT_REJANG },
  {   0x1B80, HB_SCRIPT_SUNDANESE },
  {   0xA880, HB_SCRIPT_SAURASHTRA },
  {   0xAA00, HB_SCRIPT_CHAM },
  {   0x1C50, HB_SCRIPT_OL_CHIKI },
  {   0xA500, HB_SCRIPT_VAI },
  {  0x102A0, HB_SCRIPT_CARIAN },
  {  0x10280, HB_SCRIPT_LYCIAN },
  {  0x1093F, HB_SCRIPT_LYDIAN },

  { 0x111111, HB_SCRIPT_UNKNOWN }
};
static const test_pair_t script_tests_more[] =
{
  /* Unicode-5.2 additions */
  {  0x10B00, HB_SCRIPT_AVESTAN },
  {   0xA6A0, HB_SCRIPT_BAMUM },
  {  0x13000, HB_SCRIPT_EGYPTIAN_HIEROGLYPHS },
  {  0x10840, HB_SCRIPT_IMPERIAL_ARAMAIC },
  {  0x10B60, HB_SCRIPT_INSCRIPTIONAL_PAHLAVI },
  {  0x10B40, HB_SCRIPT_INSCRIPTIONAL_PARTHIAN },
  {   0xA980, HB_SCRIPT_JAVANESE },
  {  0x11082, HB_SCRIPT_KAITHI },
  {   0xA4D0, HB_SCRIPT_LISU },
  {   0xABE5, HB_SCRIPT_MEETEI_MAYEK },
  {  0x10A60, HB_SCRIPT_OLD_SOUTH_ARABIAN },
  {  0x10C00, HB_SCRIPT_OLD_TURKIC },
  {   0x0800, HB_SCRIPT_SAMARITAN },
  {   0x1A20, HB_SCRIPT_TAI_THAM },
  {   0xAA80, HB_SCRIPT_TAI_VIET },

  /* Unicode-6.0 additions */
  {   0x1BC0, HB_SCRIPT_BATAK },
  {  0x11000, HB_SCRIPT_BRAHMI },
  {   0x0840, HB_SCRIPT_MANDAIC },

  /* Unicode-5.2 character additions */
  {   0x1CED, HB_SCRIPT_INHERITED },
  {   0x1400, HB_SCRIPT_CANADIAN_ABORIGINAL },

  { 0x111111, HB_SCRIPT_UNKNOWN }
};


typedef unsigned int (*get_func_t)         (hb_unicode_funcs_t *ufuncs,
					    hb_codepoint_t      unicode,
					    void               *user_data);
typedef unsigned int (*func_setter_func_t) (hb_unicode_funcs_t *ufuncs,
					    get_func_t          func,
					    void               *user_data,
					    hb_destroy_func_t   destroy);
typedef unsigned int (*getter_func_t)      (hb_unicode_funcs_t *ufuncs,
					    hb_codepoint_t      unicode);

typedef struct {
  const char         *name;
  func_setter_func_t  func_setter;
  getter_func_t       getter;
  const test_pair_t  *tests;
  unsigned int        num_tests;
  const test_pair_t  *tests_more;
  unsigned int        num_tests_more;
  unsigned int        default_value;
} property_t;

#define RETURNS_UNICODE_ITSELF ((unsigned int) -1)

#define PROPERTY(name, DEFAULT) \
  { \
    #name, \
    (func_setter_func_t) hb_unicode_funcs_set_##name##_func, \
    (getter_func_t) hb_unicode_##name, \
    name##_tests, \
    G_N_ELEMENTS (name##_tests), \
    name##_tests_more, \
    G_N_ELEMENTS (name##_tests_more), \
    DEFAULT \
  }
static const property_t properties[] =
{
  PROPERTY (combining_class, 0),
  PROPERTY (eastasian_width, 1),
  PROPERTY (general_category, (unsigned int) HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER),
  PROPERTY (mirroring, RETURNS_UNICODE_ITSELF),
  PROPERTY (script, (unsigned int) HB_SCRIPT_UNKNOWN)
};
#undef PROPERTY

static void
test_unicode_properties (gconstpointer user_data)
{
  hb_unicode_funcs_t *uf = (hb_unicode_funcs_t *) user_data;
  unsigned int i, j;
  gboolean failed = TRUE;

  g_assert (hb_unicode_funcs_is_immutable (uf));
  g_assert (hb_unicode_funcs_get_parent (uf));

  for (i = 0; i < G_N_ELEMENTS (properties); i++) {
    const property_t *p = &properties[i];
    const test_pair_t *tests;

    g_test_message ("Testing property %s", p->name);
    tests = p->tests;
    for (j = 0; j < p->num_tests; j++) {
      g_test_message ("Test %s #%d: U+%04X", p->name, j, tests[j].unicode);
      g_assert_cmphex (p->getter (uf, tests[j].unicode), ==, tests[j].value);
    }
    /* These tests are from Unicode 5.2 onward and older glib/ICU
     * don't get them right.  Just warn instead of assert. */
    tests = p->tests_more;
    for (j = 0; j < p->num_tests_more; j++) {
      g_test_message ("Test %s more #%d: U+%04X", p->name, j, tests[j].unicode);
      if (p->getter (uf, tests[j].unicode) != tests[j].value) {
	g_test_message ("Soft fail: Received %x, expected %x", p->getter (uf, tests[j].unicode), tests[j].value);
        failed = TRUE;
      }
    }
  }

  if (failed)
    g_test_message ("Some property tests failed.  You probably have an old version of one of the libraries used.");
}

static hb_codepoint_t
default_value (hb_codepoint_t _default_value, hb_codepoint_t unicode)
{
  return _default_value == RETURNS_UNICODE_ITSELF ?  unicode : _default_value;
}

static void
_test_unicode_properties_nil (hb_unicode_funcs_t *uf)
{
  unsigned int i, j;

  for (i = 0; i < G_N_ELEMENTS (properties); i++) {
    const property_t *p = &properties[i];
    const test_pair_t *tests;

    g_test_message ("Testing property %s", p->name);
    tests = p->tests;
    for (j = 0; j < p->num_tests; j++) {
      g_test_message ("Test %s #%d: U+%04X", p->name, j, tests[j].unicode);
      g_assert_cmphex (p->getter (uf, tests[j].unicode), ==, default_value (p->default_value, tests[j].unicode));
    }
    tests = p->tests_more;
    for (j = 0; j < p->num_tests_more; j++) {
      g_test_message ("Test %s more #%d: U+%04X", p->name, j, tests[j].unicode);
      g_assert_cmphex (p->getter (uf, tests[j].unicode), ==, default_value (p->default_value, tests[j].unicode));
    }
  }
}

static void
test_unicode_properties_nil (void)
{
  hb_unicode_funcs_t *uf = hb_unicode_funcs_create (NULL);

  g_assert (!hb_unicode_funcs_is_immutable (uf));
  _test_unicode_properties_nil (uf);

  hb_unicode_funcs_destroy (uf);
}

static void
test_unicode_properties_empty (void)
{
  hb_unicode_funcs_t *uf = hb_unicode_funcs_get_empty ();

  g_assert (uf);
  g_assert (hb_unicode_funcs_is_immutable (uf));
  _test_unicode_properties_nil (uf);
}


static void
test_unicode_chainup (void)
{
  hb_unicode_funcs_t *uf, *uf2;

  /* Chain-up to nil */

  uf = hb_unicode_funcs_create (NULL);
  g_assert (!hb_unicode_funcs_is_immutable (uf));

  uf2 = hb_unicode_funcs_create (uf);
  g_assert (hb_unicode_funcs_is_immutable (uf));
  hb_unicode_funcs_destroy (uf);

  g_assert (!hb_unicode_funcs_is_immutable (uf2));
  _test_unicode_properties_nil (uf2);

  hb_unicode_funcs_destroy (uf2);

  /* Chain-up to default */

  uf = hb_unicode_funcs_create (hb_unicode_funcs_get_default ());
  g_assert (!hb_unicode_funcs_is_immutable (uf));

  uf2 = hb_unicode_funcs_create (uf);
  g_assert (hb_unicode_funcs_is_immutable (uf));
  hb_unicode_funcs_destroy (uf);

  g_assert (!hb_unicode_funcs_is_immutable (uf2));
  hb_unicode_funcs_make_immutable (uf2);
  test_unicode_properties (uf2);

  hb_unicode_funcs_destroy (uf2);

}

static void
test_unicode_setters (void)
{
  hb_unicode_funcs_t *uf;
  unsigned int i;

  /* This is cruel: we use script-returning functions to test all properties,
   * but it works. */

  for (i = 0; i < G_N_ELEMENTS (properties); i++) {
    const property_t *p = &properties[i];
    data_t data[2] = {{MAGIC0, FALSE}, {MAGIC1, FALSE}};

    g_test_message ("Testing property %s", p->name);

    uf = hb_unicode_funcs_create (NULL);
    g_assert (!hb_unicode_funcs_is_immutable (uf));

    p->func_setter (uf, (get_func_t) simple_get_script, &data[0], free_up);

    g_assert_cmphex (p->getter (uf, 'a'), ==, HB_SCRIPT_LATIN);
    g_assert_cmphex (p->getter (uf, '0'), ==, HB_SCRIPT_UNKNOWN);

    p->func_setter (uf, (get_func_t) NULL, NULL, NULL);
    g_assert (data[0].freed && !data[1].freed);

    g_assert (!hb_unicode_funcs_is_immutable (uf));
    hb_unicode_funcs_make_immutable (uf);
    g_assert (hb_unicode_funcs_is_immutable (uf));

    /* Since uf is immutable now, the following setter should do nothing. */
    p->func_setter (uf, (get_func_t) a_is_for_arabic_get_script, &data[1], free_up);

    g_assert (data[0].freed && !data[1].freed);
    hb_unicode_funcs_destroy (uf);
    g_assert (data[0].freed && !data[1].freed);
  }
}



typedef struct {
  data_t data[2];
} data_fixture_t;

static void
data_fixture_init (data_fixture_t *f, gconstpointer user_data)
{
  f->data[0].value = MAGIC0;
  f->data[1].value = MAGIC1;
}
static void
data_fixture_finish (data_fixture_t *f, gconstpointer user_data)
{
}

static void
test_unicode_subclassing_nil (data_fixture_t *f, gconstpointer user_data)
{
  hb_unicode_funcs_t *uf, *aa;

  uf = hb_unicode_funcs_create (NULL);

  aa = hb_unicode_funcs_create (uf);

  hb_unicode_funcs_destroy (uf);

  hb_unicode_funcs_set_script_func (aa, a_is_for_arabic_get_script,
                                    &f->data[1], free_up);

  g_assert_cmphex (hb_unicode_script (aa, 'a'), ==, HB_SCRIPT_ARABIC);
  g_assert_cmphex (hb_unicode_script (aa, 'b'), ==, HB_SCRIPT_UNKNOWN);

  g_assert (!f->data[0].freed && !f->data[1].freed);
  hb_unicode_funcs_destroy (aa);
  g_assert (!f->data[0].freed && f->data[1].freed);
}

static void
test_unicode_subclassing_default (data_fixture_t *f, gconstpointer user_data)
{
  hb_unicode_funcs_t *uf, *aa;

  uf = hb_unicode_funcs_get_default ();
  aa = hb_unicode_funcs_create (uf);

  hb_unicode_funcs_set_script_func (aa, a_is_for_arabic_get_script,
                                    &f->data[1], free_up);

  g_assert_cmphex (hb_unicode_script (aa, 'a'), ==, HB_SCRIPT_ARABIC);
  g_assert_cmphex (hb_unicode_script (aa, 'b'), ==, HB_SCRIPT_LATIN);

  g_assert (!f->data[0].freed && !f->data[1].freed);
  hb_unicode_funcs_destroy (aa);
  g_assert (!f->data[0].freed && f->data[1].freed);
}

static void
test_unicode_subclassing_deep (data_fixture_t *f, gconstpointer user_data)
{
  hb_unicode_funcs_t *uf, *aa;

  uf = hb_unicode_funcs_create (NULL);

  hb_unicode_funcs_set_script_func (uf, simple_get_script,
                                    &f->data[0], free_up);

  aa = hb_unicode_funcs_create (uf);

  hb_unicode_funcs_destroy (uf);

  /* make sure the 'uf' didn't get freed, since 'aa' holds a ref */
  g_assert (!f->data[0].freed);

  hb_unicode_funcs_set_script_func (aa, a_is_for_arabic_get_script,
                                    &f->data[1], free_up);

  g_assert_cmphex (hb_unicode_script (aa, 'a'), ==, HB_SCRIPT_ARABIC);
  g_assert_cmphex (hb_unicode_script (aa, 'b'), ==, HB_SCRIPT_LATIN);
  g_assert_cmphex (hb_unicode_script (aa, '0'), ==, HB_SCRIPT_UNKNOWN);

  g_assert (!f->data[0].freed && !f->data[1].freed);
  hb_unicode_funcs_destroy (aa);
  g_assert (f->data[0].freed && f->data[1].freed);
}


static hb_script_t
script_roundtrip_default (hb_script_t script)
{
  return hb_script_from_iso15924_tag (hb_script_to_iso15924_tag (script));
}

#ifdef HAVE_GLIB
static hb_script_t
script_roundtrip_glib (hb_script_t script)
{
  return hb_glib_script_to_script (hb_glib_script_from_script (script));
}
#endif

#ifdef HAVE_ICU
static hb_script_t
script_roundtrip_icu (hb_script_t script)
{
  return hb_icu_script_to_script (hb_icu_script_from_script (script));
}
#endif

static void
test_unicode_script_roundtrip (gconstpointer user_data)
{
  typedef hb_script_t (*roundtrip_func_t) (hb_script_t);
  roundtrip_func_t roundtrip_func = (roundtrip_func_t) user_data;
  unsigned int i;
  gboolean failed = FALSE;

  for (i = 0; i < G_N_ELEMENTS (script_tests); i++) {
    const test_pair_t *test = &script_tests[i];
    hb_script_t script = test->value;

    g_test_message ("Test script roundtrip #%d: %x", i, script);
    g_assert_cmphex (script, ==, roundtrip_func (script));
  }
  for (i = 0; i < G_N_ELEMENTS (script_tests_more); i++) {
    const test_pair_t *test = &script_tests_more[i];
    hb_script_t script = test->value;

    g_test_message ("Test script roundtrip more #%d: %x", i, script);
    if (script != roundtrip_func (script)) {
      g_test_message ("Soft fail: Received %x, expected %x", roundtrip_func (script), script);
      failed = TRUE;
    }
  }

  g_assert_cmphex (HB_SCRIPT_INVALID, ==, roundtrip_func (HB_SCRIPT_INVALID));

  if (failed)
    g_test_message ("Some script roundtrip tests failed.  You probably have an old version of one of the libraries used.");
}


static void
test_unicode_normalization (gconstpointer user_data)
{
  hb_unicode_funcs_t *uf = (hb_unicode_funcs_t *) user_data;
  gunichar a, b, ab;
  hb_codepoint_t decomposed[HB_UNICODE_MAX_DECOMPOSITION_LEN];


  /* Test compose() */

  /* Not composable */
  g_assert (!hb_unicode_compose (uf, 0x0041, 0x0042, &ab) && ab == 0);
  g_assert (!hb_unicode_compose (uf, 0x0041, 0, &ab) && ab == 0);
  g_assert (!hb_unicode_compose (uf, 0x0066, 0x0069, &ab) && ab == 0);

  /* Singletons should not compose */
  g_assert (!hb_unicode_compose (uf, 0x212B, 0, &ab) && ab == 0);
  g_assert (!hb_unicode_compose (uf, 0x00C5, 0, &ab) && ab == 0);
  g_assert (!hb_unicode_compose (uf, 0x2126, 0, &ab) && ab == 0);
  g_assert (!hb_unicode_compose (uf, 0x03A9, 0, &ab) && ab == 0);

  /* Non-starter pairs should not compose */
  g_assert (!hb_unicode_compose (uf, 0x0308, 0x0301, &ab) && ab == 0); /* !0x0344 */
  g_assert (!hb_unicode_compose (uf, 0x0F71, 0x0F72, &ab) && ab == 0); /* !0x0F73 */

  /* Pairs */
  g_assert (hb_unicode_compose (uf, 0x0041, 0x030A, &ab) && ab == 0x00C5);
  g_assert (hb_unicode_compose (uf, 0x006F, 0x0302, &ab) && ab == 0x00F4);
  g_assert (hb_unicode_compose (uf, 0x1E63, 0x0307, &ab) && ab == 0x1E69);
  g_assert (hb_unicode_compose (uf, 0x0073, 0x0323, &ab) && ab == 0x1E63);
  g_assert (hb_unicode_compose (uf, 0x0064, 0x0307, &ab) && ab == 0x1E0B);
  g_assert (hb_unicode_compose (uf, 0x0064, 0x0323, &ab) && ab == 0x1E0D);

  /* Hangul */
  g_assert (hb_unicode_compose (uf, 0xD4CC, 0x11B6, &ab) && ab == 0xD4DB);
  g_assert (hb_unicode_compose (uf, 0x1111, 0x1171, &ab) && ab == 0xD4CC);
  g_assert (hb_unicode_compose (uf, 0xCE20, 0x11B8, &ab) && ab == 0xCE31);
  g_assert (hb_unicode_compose (uf, 0x110E, 0x1173, &ab) && ab == 0xCE20);


  /* Test decompose() */

  /* Not decomposable */
  g_assert (!hb_unicode_decompose (uf, 0x0041, &a, &b) && a == 0x0041 && b == 0);
  g_assert (!hb_unicode_decompose (uf, 0xFB01, &a, &b) && a == 0xFB01 && b == 0);
  g_assert (!hb_unicode_decompose (uf, 0x1F1EF, &a, &b) && a == 0x1F1EF && b == 0);

  /* Singletons */
  g_assert (hb_unicode_decompose (uf, 0x212B, &a, &b) && a == 0x00C5 && b == 0);
  g_assert (hb_unicode_decompose (uf, 0x2126, &a, &b) && a == 0x03A9 && b == 0);

  /* Non-starter pairs decompose, but not compose */
  g_assert (hb_unicode_decompose (uf, 0x0344, &a, &b) && a == 0x0308 && b == 0x0301);
  g_assert (hb_unicode_decompose (uf, 0x0F73, &a, &b) && a == 0x0F71 && b == 0x0F72);

  /* Pairs */
  g_assert (hb_unicode_decompose (uf, 0x00C5, &a, &b) && a == 0x0041 && b == 0x030A);
  g_assert (hb_unicode_decompose (uf, 0x00F4, &a, &b) && a == 0x006F && b == 0x0302);
  g_assert (hb_unicode_decompose (uf, 0x1E69, &a, &b) && a == 0x1E63 && b == 0x0307);
  g_assert (hb_unicode_decompose (uf, 0x1E63, &a, &b) && a == 0x0073 && b == 0x0323);
  g_assert (hb_unicode_decompose (uf, 0x1E0B, &a, &b) && a == 0x0064 && b == 0x0307);
  g_assert (hb_unicode_decompose (uf, 0x1E0D, &a, &b) && a == 0x0064 && b == 0x0323);

  /* Hangul */
  g_assert (hb_unicode_decompose (uf, 0xD4DB, &a, &b) && a == 0xD4CC && b == 0x11B6);
  g_assert (hb_unicode_decompose (uf, 0xD4CC, &a, &b) && a == 0x1111 && b == 0x1171);
  g_assert (hb_unicode_decompose (uf, 0xCE31, &a, &b) && a == 0xCE20 && b == 0x11B8);
  g_assert (hb_unicode_decompose (uf, 0xCE20, &a, &b) && a == 0x110E && b == 0x1173);


  /* Test decompose_compatibility() */

  /* Not decomposable */
  g_assert (hb_unicode_decompose_compatibility (uf, 0x0041, decomposed) == 0);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x1F632, decomposed) == 0);

  /* Singletons */
  g_assert (hb_unicode_decompose_compatibility (uf, 0x00B5, decomposed) == 1 && decomposed[0] == 0x03BC);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x03D6, decomposed) == 1 && decomposed[0] == 0x03C0);

  /* Arabic compatibility */
  g_assert (hb_unicode_decompose_compatibility (uf, 0xFB54, decomposed) == 1 && decomposed[0] == 0x067B);

  /* Longest decomposition ever */
  g_assert (18 <= HB_UNICODE_MAX_DECOMPOSITION_LEN);
  g_assert (hb_unicode_decompose_compatibility (uf, 0xFDFA, decomposed) == 18 && decomposed[17] == 0x0645);

  /* Note: we deliberately don't test characters that have canonical decompositions but no
   * compatibility decomposition against the decompose_compatibility() function as that we
   * leave up to implementations (for now). */

  /* Spaces */
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2002, decomposed) == 1 && decomposed[0] == 0x0020);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2003, decomposed) == 1 && decomposed[0] == 0x0020);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2004, decomposed) == 1 && decomposed[0] == 0x0020);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2005, decomposed) == 1 && decomposed[0] == 0x0020);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2006, decomposed) == 1 && decomposed[0] == 0x0020);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2008, decomposed) == 1 && decomposed[0] == 0x0020);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2009, decomposed) == 1 && decomposed[0] == 0x0020);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x200A, decomposed) == 1 && decomposed[0] == 0x0020);

  /* Pairs */
  g_assert (hb_unicode_decompose_compatibility (uf, 0x0587, decomposed) == 2 &&
            decomposed[0] == 0x0565 && decomposed[1] == 0x0582);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2017, decomposed) == 2 &&
            decomposed[0] == 0x0020 && decomposed[1] == 0x0333);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2025, decomposed) == 2 &&
            decomposed[0] == 0x002E && decomposed[1] == 0x002E);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2033, decomposed) == 2 &&
            decomposed[0] == 0x2032 && decomposed[1] == 0x2032);

  /* Triples */
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2026, decomposed) == 3 &&
            decomposed[0] == 0x002E && decomposed[1] == 0x002E && decomposed[2] == 0x002E);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x2034, decomposed) == 3 &&
            decomposed[0] == 0x2032 && decomposed[1] == 0x2032 && decomposed[2] == 0x2032);
  g_assert (hb_unicode_decompose_compatibility (uf, 0x213B, decomposed) == 3 &&
            decomposed[0] == 0x0046 && decomposed[1] == 0x0041 && decomposed[2] == 0x0058);
}



int
main (int argc, char **argv)
{
  hb_test_init (&argc, &argv);

  hb_test_add (test_unicode_properties_nil);
  hb_test_add (test_unicode_properties_empty);

  hb_test_add_data_flavor (hb_unicode_funcs_get_default (),          "default", test_unicode_properties);
  hb_test_add_data_flavor (hb_unicode_funcs_get_default (),          "default", test_unicode_normalization);
  hb_test_add_data_flavor ((gconstpointer) script_roundtrip_default, "default", test_unicode_script_roundtrip);
#ifdef HAVE_GLIB
  hb_test_add_data_flavor (hb_glib_get_unicode_funcs (),             "glib",    test_unicode_properties);
  hb_test_add_data_flavor (hb_glib_get_unicode_funcs (),             "glib",    test_unicode_normalization);
  hb_test_add_data_flavor ((gconstpointer) script_roundtrip_glib,    "glib",    test_unicode_script_roundtrip);
#endif
#ifdef HAVE_ICU
  hb_test_add_data_flavor (hb_icu_get_unicode_funcs (),              "icu",     test_unicode_properties);
  hb_test_add_data_flavor (hb_icu_get_unicode_funcs (),              "icu",     test_unicode_normalization);
  hb_test_add_data_flavor ((gconstpointer) script_roundtrip_icu,     "icu",     test_unicode_script_roundtrip);
#endif

  hb_test_add (test_unicode_chainup);

  hb_test_add (test_unicode_setters);

  hb_test_add_fixture (data_fixture, NULL, test_unicode_subclassing_nil);
  hb_test_add_fixture (data_fixture, NULL, test_unicode_subclassing_default);
  hb_test_add_fixture (data_fixture, NULL, test_unicode_subclassing_deep);

  return hb_test_run ();
}
