/*
 * Copyright © 2010  Behdad Esfahbod
 * Copyright © 2011,2012  Google, Inc.
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
 * Google Author(s): Garret Rieger, Rod Sheeter
 */

#include <stdio.h>

#include "main-font-text.hh"
#include "hb-subset.h"

/*
 * Command line interface to the harfbuzz font subsetter.
 */

struct subset_consumer_t
{
  subset_consumer_t (option_parser_t *parser)
      : failed (false), options (parser), subset_options (parser), font (nullptr), input (nullptr) {}

  void init (hb_buffer_t  *buffer_,
	     const font_options_t *font_opts)
  {
    font = hb_font_reference (font_opts->get_font ());
    input = hb_subset_input_reference (subset_options.input);
  }

  void consume_line (const char   *text,
		     unsigned int  text_len,
		     const char   *text_before,
		     const char   *text_after)
  {
    // TODO(Q1) does this only get called with at least 1 codepoint?
    hb_set_t *codepoints = hb_subset_input_unicode_set (input);
    if (0 == strcmp (text, "*"))
    {
      hb_face_t *face = hb_font_get_face (font);
      hb_face_collect_unicodes (face, codepoints);
      return;
    }

    gchar *c = (gchar *)text;
    do {
      gunichar cp = g_utf8_get_char(c);
      hb_codepoint_t hb_cp = cp;
      hb_set_add (codepoints, hb_cp);
    } while ((c = g_utf8_find_next_char(c, text + text_len)));
  }

  hb_bool_t
  write_file (const char *output_file, hb_blob_t *blob) {
    unsigned int size;
    const char* data = hb_blob_get_data (blob, &size);

    if (!output_file)
      fail (true, "No output file was specified");

    FILE *fp = fopen(output_file, "wb");
    if (!fp)
      fail (false, "Cannot open output file `%s': %s",
	    g_filename_display_name (output_file), strerror (errno));

    while (size) {
      size_t ret = fwrite (data, 1, size, fp);
      size -= ret;
      data += ret;
      if (size && ferror (fp))
        fail (false, "Failed to write output: %s", strerror (errno));
    }

    fclose (fp);

    return true;
  }

  void finish (const font_options_t *font_opts)
  {
    hb_face_t *face = hb_font_get_face (font);

    hb_face_t *new_face = hb_subset (face, input);
    hb_blob_t *result = hb_face_reference_blob (new_face);

    failed = !hb_blob_get_length (result);
    if (!failed)
      write_file (options.output_file, result);

    hb_subset_input_destroy (input);
    hb_blob_destroy (result);
    hb_face_destroy (new_face);
    hb_font_destroy (font);
  }

  public:
  bool failed;

  private:
  output_options_t options;
  subset_options_t subset_options;
  hb_font_t *font;
  hb_subset_input_t *input;
};

int
main (int argc, char **argv)
{
  main_font_text_t<subset_consumer_t, 10, 0> driver;
  return driver.main (argc, argv);
}
