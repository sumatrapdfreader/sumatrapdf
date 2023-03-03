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

#include "font-options.hh"
#include "shape-options.hh"
#include "text-options.hh"


template <typename output_t>
struct shape_consumer_t : shape_options_t
{
  void add_options (option_parser_t *parser)
  {
    shape_options_t::add_options (parser);
    output.add_options (parser);
  }

  template <typename app_t>
  void init (const app_t *app)
  {
    failed = false;
    buffer = hb_buffer_create ();

    output.init (buffer, app);
  }
  template <typename app_t>
  bool consume_line (app_t &app)
  {
    unsigned int text_len;
    const char *text;
    if (!(text = app.get_line (&text_len)))
      return false;

    output.new_line ();

    for (unsigned int n = num_iterations; n; n--)
    {
      populate_buffer (buffer, text, text_len, app.text_before, app.text_after);
      if (n == 1)
	output.consume_text (buffer, text, text_len, utf8_clusters);

      const char *error = nullptr;
      if (!shape (app.font, buffer, &error))
      {
	failed = true;
	output.error (error);
	if (hb_buffer_get_content_type (buffer) == HB_BUFFER_CONTENT_TYPE_GLYPHS)
	  break;
	else
	  return true;
      }
    }

    output.consume_glyphs (buffer, text, text_len, utf8_clusters);
    return true;
  }
  template <typename app_t>
  void finish (const app_t *app)
  {
    output.finish (buffer, app);
    hb_buffer_destroy (buffer);
    buffer = nullptr;
  }

  public:
  bool failed = false;

  protected:
  output_t output;

  hb_buffer_t *buffer = nullptr;
};


#endif
