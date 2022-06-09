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

#ifndef FACE_OPTIONS_HH
#define FACE_OPTIONS_HH

#include "options.hh"

struct face_options_t
{
  ~face_options_t ()
  {
    g_free (font_file);
  }

  void set_face (hb_face_t *face_)
  { face = face_; }

  void add_options (option_parser_t *parser);

  void post_parse (GError **error);

  static struct cache_t
  {
    ~cache_t ()
    {
      g_free (font_path);
      hb_blob_destroy (blob);
      hb_face_destroy (face);
    }

    char *font_path = nullptr;
    hb_blob_t *blob = nullptr;
    unsigned face_index = (unsigned) -1;
    hb_face_t *face = nullptr;
  } cache;

  char *font_file = nullptr;
  unsigned face_index = 0;

  hb_blob_t *blob = nullptr;
  hb_face_t *face = nullptr;
};


face_options_t::cache_t face_options_t::cache {};

void
face_options_t::post_parse (GError **error)
{
  if (!font_file)
  {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		 "No font file set");
    return;
  }

  assert (font_file);

  const char *font_path = font_file;

  if (0 == strcmp (font_path, "-"))
  {
#if defined(_WIN32) || defined(__CYGWIN__)
    setmode (fileno (stdin), O_BINARY);
    font_path = "STDIN";
#else
    font_path = "/dev/stdin";
#endif
  }

  if (!cache.font_path || 0 != strcmp (cache.font_path, font_path))
  {
    hb_blob_destroy (cache.blob);
    cache.blob = hb_blob_create_from_file_or_fail (font_path);

    free ((char *) cache.font_path);
    cache.font_path = g_strdup (font_path);

    if (!cache.blob)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		   "%s: Failed reading file", font_path);
      return;
    }

    hb_face_destroy (cache.face);
    cache.face = nullptr;
    cache.face_index = (unsigned) -1;
  }

  if (cache.face_index != face_index)
  {
    hb_face_destroy (cache.face);
    cache.face = hb_face_create (cache.blob, face_index);
    cache.face_index = face_index;
  }

  blob = cache.blob;
  face = cache.face;
}

void
face_options_t::add_options (option_parser_t *parser)
{
  GOptionEntry entries[] =
  {
    {"font-file",	0, 0, G_OPTION_ARG_STRING,	&this->font_file,		"Set font file-name",				"filename"},
    {"face-index",	0, 0, G_OPTION_ARG_INT,		&this->face_index,		"Set face index (default: 0)",			"index"},
    {nullptr}
  };
  parser->add_group (entries,
		     "face",
		     "Font-face options:",
		     "Options for the font face",
		     this);
}

#endif
