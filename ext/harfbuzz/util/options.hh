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

#ifndef OPTIONS_HH
#define OPTIONS_HH

#include "hb.hh"
#include "hb-subset.h"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for isatty() */
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
#include <io.h> /* for setmode() under Windows */
#endif

#include <hb.h>
#include <hb-ot.h>
#include <glib.h>
#include <glib/gprintf.h>

void fail (hb_bool_t suggest_help, const char *format, ...) G_GNUC_NORETURN G_GNUC_PRINTF (2, 3);

struct option_group_t
{
  virtual ~option_group_t () {}

  virtual void add_options (struct option_parser_t *parser) = 0;

  virtual void pre_parse (GError **error G_GNUC_UNUSED) {}
  virtual void post_parse (GError **error G_GNUC_UNUSED) {}
};


struct option_parser_t
{
  option_parser_t (const char *usage)
  {
    memset (this, 0, sizeof (*this));
    usage_str = usage;
    context = g_option_context_new (usage);
    to_free = g_ptr_array_new ();

    add_main_options ();
  }

  static void _g_free_g_func (void *p, void * G_GNUC_UNUSED) { g_free (p); }

  ~option_parser_t ()
  {
    g_option_context_free (context);
    g_ptr_array_foreach (to_free, _g_free_g_func, nullptr);
    g_ptr_array_free (to_free, TRUE);
  }

  void add_main_options ();

  void add_group (GOptionEntry   *entries,
		  const gchar    *name,
		  const gchar    *description,
		  const gchar    *help_description,
		  option_group_t *option_group);

  void free_later (char *p) {
    g_ptr_array_add (to_free, p);
  }

  void parse (int *argc, char ***argv);

  G_GNUC_NORETURN void usage () {
    g_printerr ("Usage: %s [OPTION...] %s\n", g_get_prgname (), usage_str);
    exit (1);
  }

  private:
  const char *usage_str;
  GOptionContext *context;
  GPtrArray *to_free;
};


#define DEFAULT_MARGIN 16
#define DEFAULT_FORE "#000000"
#define DEFAULT_BACK "#FFFFFF"
#define FONT_SIZE_UPEM 0x7FFFFFFF
#define FONT_SIZE_NONE 0

struct view_options_t : option_group_t
{
  view_options_t (option_parser_t *parser)
  {
    annotate = false;
    fore = nullptr;
    back = nullptr;
    line_space = 0;
    have_font_extents = false;
    font_extents.ascent = font_extents.descent = font_extents.line_gap = 0;
    margin.t = margin.r = margin.b = margin.l = DEFAULT_MARGIN;

    add_options (parser);
  }
  ~view_options_t () override
  {
    g_free (fore);
    g_free (back);
  }

  void add_options (option_parser_t *parser) override;

  hb_bool_t annotate;
  char *fore;
  char *back;
  double line_space;
  bool have_font_extents;
  struct font_extents_t {
    double ascent, descent, line_gap;
  } font_extents;
  struct margin_t {
    double t, r, b, l;
  } margin;
};


struct shape_options_t : option_group_t
{
  shape_options_t (option_parser_t *parser)
  {
    direction = language = script = nullptr;
    bot = eot = preserve_default_ignorables = remove_default_ignorables = false;
    features = nullptr;
    num_features = 0;
    shapers = nullptr;
    utf8_clusters = false;
    invisible_glyph = 0;
    cluster_level = HB_BUFFER_CLUSTER_LEVEL_DEFAULT;
    normalize_glyphs = false;
    verify = false;
    num_iterations = 1;

    add_options (parser);
  }
  ~shape_options_t () override
  {
    g_free (direction);
    g_free (language);
    g_free (script);
    free (features);
    g_strfreev (shapers);
  }

  void add_options (option_parser_t *parser) override;

  void setup_buffer (hb_buffer_t *buffer)
  {
    hb_buffer_set_direction (buffer, hb_direction_from_string (direction, -1));
    hb_buffer_set_script (buffer, hb_script_from_string (script, -1));
    hb_buffer_set_language (buffer, hb_language_from_string (language, -1));
    hb_buffer_set_flags (buffer, (hb_buffer_flags_t)
				 (HB_BUFFER_FLAG_DEFAULT |
				  (bot ? HB_BUFFER_FLAG_BOT : 0) |
				  (eot ? HB_BUFFER_FLAG_EOT : 0) |
				  (preserve_default_ignorables ? HB_BUFFER_FLAG_PRESERVE_DEFAULT_IGNORABLES : 0) |
				  (remove_default_ignorables ? HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES : 0) |
				  0));
    hb_buffer_set_invisible_glyph (buffer, invisible_glyph);
    hb_buffer_set_cluster_level (buffer, cluster_level);
    hb_buffer_guess_segment_properties (buffer);
  }

  static void copy_buffer_properties (hb_buffer_t *dst, hb_buffer_t *src)
  {
    hb_segment_properties_t props;
    hb_buffer_get_segment_properties (src, &props);
    hb_buffer_set_segment_properties (dst, &props);
    hb_buffer_set_flags (dst, hb_buffer_get_flags (src));
    hb_buffer_set_cluster_level (dst, hb_buffer_get_cluster_level (src));
  }

  void populate_buffer (hb_buffer_t *buffer, const char *text, int text_len,
			const char *text_before, const char *text_after)
  {
    hb_buffer_clear_contents (buffer);
    if (text_before) {
      unsigned int len = strlen (text_before);
      hb_buffer_add_utf8 (buffer, text_before, len, len, 0);
    }
    hb_buffer_add_utf8 (buffer, text, text_len, 0, text_len);
    if (text_after) {
      hb_buffer_add_utf8 (buffer, text_after, -1, 0, 0);
    }

    if (!utf8_clusters) {
      /* Reset cluster values to refer to Unicode character index
       * instead of UTF-8 index. */
      unsigned int num_glyphs = hb_buffer_get_length (buffer);
      hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, nullptr);
      for (unsigned int i = 0; i < num_glyphs; i++)
      {
	info->cluster = i;
	info++;
      }
    }

    setup_buffer (buffer);
  }

  hb_bool_t shape (hb_font_t *font, hb_buffer_t *buffer, const char **error=nullptr)
  {
    hb_buffer_t *text_buffer = nullptr;
    if (verify)
    {
      text_buffer = hb_buffer_create ();
      hb_buffer_append (text_buffer, buffer, 0, -1);
    }

    if (!hb_shape_full (font, buffer, features, num_features, shapers))
    {
      if (error)
	*error = "all shapers failed.";
      goto fail;
    }

    if (normalize_glyphs)
      hb_buffer_normalize_glyphs (buffer);

    if (verify && !verify_buffer (buffer, text_buffer, font, error))
      goto fail;

    if (text_buffer)
      hb_buffer_destroy (text_buffer);

    return true;

  fail:
    if (text_buffer)
      hb_buffer_destroy (text_buffer);

    return false;
  }

  bool verify_buffer (hb_buffer_t  *buffer,
		      hb_buffer_t  *text_buffer,
		      hb_font_t    *font,
		      const char  **error=nullptr)
  {
    if (!verify_buffer_monotone (buffer, error))
      return false;
    if (!verify_buffer_safe_to_break (buffer, text_buffer, font, error))
      return false;
    return true;
  }

  bool verify_buffer_monotone (hb_buffer_t *buffer, const char **error=nullptr)
  {
    /* Check that clusters are monotone. */
    if (cluster_level == HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES ||
	cluster_level == HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS)
    {
      bool is_forward = HB_DIRECTION_IS_FORWARD (hb_buffer_get_direction (buffer));

      unsigned int num_glyphs;
      hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &num_glyphs);

      for (unsigned int i = 1; i < num_glyphs; i++)
	if (info[i-1].cluster != info[i].cluster &&
	    (info[i-1].cluster < info[i].cluster) != is_forward)
	{
	  if (error)
	    *error = "clusters are not monotone.";
	  return false;
	}
    }

    return true;
  }

  bool verify_buffer_safe_to_break (hb_buffer_t  *buffer,
				    hb_buffer_t  *text_buffer,
				    hb_font_t    *font,
				    const char  **error=nullptr)
  {
    if (cluster_level != HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES &&
	cluster_level != HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS)
    {
      /* Cannot perform this check without monotone clusters.
       * Then again, unsafe-to-break flag is much harder to use without
       * monotone clusters. */
      return true;
    }

    /* Check that breaking up shaping at safe-to-break is indeed safe. */

    hb_buffer_t *fragment = hb_buffer_create ();
    hb_buffer_t *reconstruction = hb_buffer_create ();
    copy_buffer_properties (reconstruction, buffer);

    unsigned int num_glyphs;
    hb_glyph_info_t *info = hb_buffer_get_glyph_infos (buffer, &num_glyphs);

    unsigned int num_chars;
    hb_glyph_info_t *text = hb_buffer_get_glyph_infos (text_buffer, &num_chars);

    /* Chop text and shape fragments. */
    bool forward = HB_DIRECTION_IS_FORWARD (hb_buffer_get_direction (buffer));
    unsigned int start = 0;
    unsigned int text_start = forward ? 0 : num_chars;
    unsigned int text_end = text_start;
    for (unsigned int end = 1; end < num_glyphs + 1; end++)
    {
      if (end < num_glyphs &&
	  (info[end].cluster == info[end-1].cluster ||
	   info[end-(forward?0:1)].mask & HB_GLYPH_FLAG_UNSAFE_TO_BREAK))
	  continue;

      /* Shape segment corresponding to glyphs start..end. */
      if (end == num_glyphs)
      {
	if (forward)
	  text_end = num_chars;
	else
	  text_start = 0;
      }
      else
      {
	if (forward)
	{
	  unsigned int cluster = info[end].cluster;
	  while (text_end < num_chars && text[text_end].cluster < cluster)
	    text_end++;
	}
	else
	{
	  unsigned int cluster = info[end - 1].cluster;
	  while (text_start && text[text_start - 1].cluster >= cluster)
	    text_start--;
	}
      }
      assert (text_start < text_end);

      if (0)
	printf("start %d end %d text start %d end %d\n", start, end, text_start, text_end);

      hb_buffer_clear_contents (fragment);
      copy_buffer_properties (fragment, buffer);

      /* TODO: Add pre/post context text. */
      hb_buffer_flags_t flags = hb_buffer_get_flags (fragment);
      if (0 < text_start)
	flags = (hb_buffer_flags_t) (flags & ~HB_BUFFER_FLAG_BOT);
      if (text_end < num_chars)
	flags = (hb_buffer_flags_t) (flags & ~HB_BUFFER_FLAG_EOT);
      hb_buffer_set_flags (fragment, flags);

      hb_buffer_append (fragment, text_buffer, text_start, text_end);
      if (!hb_shape_full (font, fragment, features, num_features, shapers))
      {
	if (error)
	  *error = "all shapers failed while shaping fragment.";
	hb_buffer_destroy (reconstruction);
	hb_buffer_destroy (fragment);
	return false;
      }
      hb_buffer_append (reconstruction, fragment, 0, -1);

      start = end;
      if (forward)
	text_start = text_end;
      else
	text_end = text_start;
    }

    bool ret = true;
    hb_buffer_diff_flags_t diff = hb_buffer_diff (reconstruction, buffer, (hb_codepoint_t) -1, 0);
    if (diff)
    {
      if (error)
	*error = "Safe-to-break test failed.";
      ret = false;

      /* Return the reconstructed result instead so it can be inspected. */
      hb_buffer_set_length (buffer, 0);
      hb_buffer_append (buffer, reconstruction, 0, -1);
    }

    hb_buffer_destroy (reconstruction);
    hb_buffer_destroy (fragment);

    return ret;
  }

  void shape_closure (const char *text, int text_len,
		      hb_font_t *font, hb_buffer_t *buffer,
		      hb_set_t *glyphs)
  {
    hb_buffer_reset (buffer);
    hb_buffer_add_utf8 (buffer, text, text_len, 0, text_len);
    setup_buffer (buffer);
    hb_ot_shape_glyphs_closure (font, buffer, features, num_features, glyphs);
  }

  /* Buffer properties */
  char *direction;
  char *language;
  char *script;

  /* Buffer flags */
  hb_bool_t bot;
  hb_bool_t eot;
  hb_bool_t preserve_default_ignorables;
  hb_bool_t remove_default_ignorables;

  hb_feature_t *features;
  unsigned int num_features;
  char **shapers;
  hb_bool_t utf8_clusters;
  hb_codepoint_t invisible_glyph;
  hb_buffer_cluster_level_t cluster_level;
  hb_bool_t normalize_glyphs;
  hb_bool_t verify;
  unsigned int num_iterations;
};


struct font_options_t : option_group_t
{
  font_options_t (option_parser_t *parser,
		  int default_font_size_,
		  unsigned int subpixel_bits_)
  {
    variations = nullptr;
    num_variations = 0;
    default_font_size = default_font_size_;
    x_ppem = 0;
    y_ppem = 0;
    ptem = 0.;
    subpixel_bits = subpixel_bits_;
    font_file = nullptr;
    face_index = 0;
    font_size_x = font_size_y = default_font_size;
    font_funcs = nullptr;
    ft_load_flags = 2;

    blob = nullptr;
    font = nullptr;

    add_options (parser);
  }
  ~font_options_t () override
  {
    g_free (font_file);
    free (variations);
    g_free (font_funcs);
    hb_font_destroy (font);
  }

  void add_options (option_parser_t *parser) override;

  hb_font_t *get_font () const;

  char *font_file;
  mutable hb_blob_t *blob;
  int face_index;
  hb_variation_t *variations;
  unsigned int num_variations;
  int default_font_size;
  int x_ppem;
  int y_ppem;
  double ptem;
  unsigned int subpixel_bits;
  mutable double font_size_x;
  mutable double font_size_y;
  char *font_funcs;
  int ft_load_flags;

  private:
  mutable hb_font_t *font;
};


struct text_options_t : option_group_t
{
  text_options_t (option_parser_t *parser)
  {
    text_before = nullptr;
    text_after = nullptr;

    text_len = -1;
    text = nullptr;
    text_file = nullptr;

    fp = nullptr;
    gs = nullptr;
    line = nullptr;
    line_len = UINT_MAX;

    add_options (parser);
  }
  ~text_options_t () override
  {
    g_free (text_before);
    g_free (text_after);
    g_free (text);
    g_free (text_file);
    if (gs)
      g_string_free (gs, true);
    if (fp && fp != stdin)
      fclose (fp);
  }

  void add_options (option_parser_t *parser) override;

  void post_parse (GError **error G_GNUC_UNUSED) override {
    if (text && text_file)
      g_set_error (error,
		   G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Only one of text and text-file can be set");
  }

  const char *get_line (unsigned int *len);

  char *text_before;
  char *text_after;

  int text_len;
  char *text;
  char *text_file;

  private:
  FILE *fp;
  GString *gs;
  char *line;
  unsigned int line_len;
};

struct output_options_t : option_group_t
{
  output_options_t (option_parser_t *parser,
		    const char **supported_formats_ = nullptr)
  {
    output_file = nullptr;
    output_format = nullptr;
    supported_formats = supported_formats_;
    explicit_output_format = false;

    fp = nullptr;

    add_options (parser);
  }
  ~output_options_t () override
  {
    g_free (output_file);
    g_free (output_format);
    if (fp && fp != stdout)
      fclose (fp);
  }

  void add_options (option_parser_t *parser) override;

  void post_parse (GError **error G_GNUC_UNUSED) override
  {
    if (output_format)
      explicit_output_format = true;

    if (output_file && !output_format) {
      output_format = strrchr (output_file, '.');
      if (output_format)
      {
	  output_format++; /* skip the dot */
	  output_format = g_strdup (output_format);
      }
    }

    if (output_file && 0 == strcmp (output_file, "-"))
      output_file = nullptr; /* STDOUT */
  }

  FILE *get_file_handle ();

  char *output_file;
  char *output_format;
  const char **supported_formats;
  bool explicit_output_format;

  mutable FILE *fp;
};

struct format_options_t : option_group_t
{
  format_options_t (option_parser_t *parser) {
    show_glyph_names = true;
    show_positions = true;
    show_advances = true;
    show_clusters = true;
    show_text = false;
    show_unicode = false;
    show_line_num = false;
    show_extents = false;
    show_flags = false;
    trace = false;

    add_options (parser);
  }

  void add_options (option_parser_t *parser) override;

  void serialize (hb_buffer_t  *buffer,
			 hb_font_t    *font,
			 hb_buffer_serialize_format_t format,
			 hb_buffer_serialize_flags_t flags,
			 GString      *gs);
  void serialize_line_no (unsigned int  line_no,
			  GString      *gs);
  void serialize_buffer_of_text (hb_buffer_t  *buffer,
				 unsigned int  line_no,
				 const char   *text,
				 unsigned int  text_len,
				 hb_font_t    *font,
				 GString      *gs);
  void serialize_message (unsigned int  line_no,
			  const char   *type,
			  const char   *msg,
			  GString      *gs);
  void serialize_buffer_of_glyphs (hb_buffer_t  *buffer,
				   unsigned int  line_no,
				   const char   *text,
				   unsigned int  text_len,
				   hb_font_t    *font,
				   hb_buffer_serialize_format_t output_format,
				   hb_buffer_serialize_flags_t format_flags,
				   GString      *gs);


  hb_bool_t show_glyph_names;
  hb_bool_t show_positions;
  hb_bool_t show_advances;
  hb_bool_t show_clusters;
  hb_bool_t show_text;
  hb_bool_t show_unicode;
  hb_bool_t show_line_num;
  hb_bool_t show_extents;
  hb_bool_t show_flags;
  hb_bool_t trace;
};

struct subset_options_t : option_group_t
{
  subset_options_t (option_parser_t *parser)
  {
    input = hb_subset_input_create_or_fail ();
    add_options (parser);
  }

  ~subset_options_t () override
  {
    hb_subset_input_destroy (input);
  }

  void add_options (option_parser_t *parser) override;

  hb_subset_input_t *input;
};

/* fallback implementation for scalbn()/scalbnf() for pre-2013 MSVC */
#if defined (_MSC_VER) && (_MSC_VER < 1800)

#ifndef FLT_RADIX
#define FLT_RADIX 2
#endif

__inline long double scalbn (long double x, int exp)
{
  return x * (pow ((long double) FLT_RADIX, exp));
}

__inline float scalbnf (float x, int exp)
{
  return x * (pow ((float) FLT_RADIX, exp));
}
#endif

#endif
