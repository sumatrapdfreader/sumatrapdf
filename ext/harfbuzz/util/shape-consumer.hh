/*
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
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_SHAPE_CONSUMER_HH
#define HB_SHAPE_CONSUMER_HH

#include "hb.hh"
#include "options.hh"


template <typename output_t>
struct shape_consumer_t
{
  shape_consumer_t (option_parser_t *parser)
		  : failed (false),
		    shaper (parser),
		    output (parser),
		    font (nullptr),
		    buffer (nullptr) {}

  void init (hb_buffer_t  *buffer_,
	     const font_options_t *font_opts)
  {
    font = hb_font_reference (font_opts->get_font ());
    failed = false;
    buffer = hb_buffer_reference (buffer_);

    output.init (buffer, font_opts);
  }
  void consume_line (const char   *text,
		     unsigned int  text_len,
		     const char   *text_before,
		     const char   *text_after)
  {
    output.new_line ();

    for (unsigned int n = shaper.num_iterations; n; n--)
    {
      const char *error = nullptr;

      shaper.populate_buffer (buffer, text, text_len, text_before, text_after);
      if (n == 1)
	output.consume_text (buffer, text, text_len, shaper.utf8_clusters);
      if (!shaper.shape (font, buffer, &error))
      {
	failed = true;
	output.error (error);
	if (hb_buffer_get_content_type (buffer) == HB_BUFFER_CONTENT_TYPE_GLYPHS)
	  break;
	else
	  return;
      }
    }

    output.consume_glyphs (buffer, text, text_len, shaper.utf8_clusters);
  }
  void finish (const font_options_t *font_opts)
  {
    output.finish (buffer, font_opts);
    hb_font_destroy (font);
    font = nullptr;
    hb_buffer_destroy (buffer);
    buffer = nullptr;
  }

  public:
  bool failed;

  protected:
  shape_options_t shaper;
  output_t output;

  hb_font_t *font;
  hb_buffer_t *buffer;
};


#endif
