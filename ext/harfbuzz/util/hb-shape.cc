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
 * Google Author(s): Behdad Esfahbod
 */

#include "batch.hh"
#include "font-options.hh"
#include "main-font-text.hh"
#include "output-options.hh"
#include "shape-consumer.hh"
#include "shape-format.hh"
#include "text-options.hh"

const unsigned DEFAULT_FONT_SIZE = FONT_SIZE_UPEM;
const unsigned SUBPIXEL_BITS = 0;

struct output_buffer_t : output_options_t<>
{
  void add_options (option_parser_t *parser)
  {
    parser->set_summary ("Shape text with given font.");
    output_options_t::add_options (parser, hb_buffer_serialize_list_formats ());
    format.add_options (parser);
  }

  void init (hb_buffer_t *buffer, const font_options_t *font_opts)
  {
    gs = g_string_new (nullptr);
    line_no = 0;
    font = hb_font_reference (font_opts->font);

    if (!output_format)
      serialize_format = HB_BUFFER_SERIALIZE_FORMAT_TEXT;
    else
      serialize_format = hb_buffer_serialize_format_from_string (output_format, -1);
    /* An empty "output_format" parameter basically skips output generating.
     * Useful for benchmarking. */
    if ((!output_format || *output_format) &&
	!hb_buffer_serialize_format_to_string (serialize_format))
    {
      if (explicit_output_format)
	fail (false, "Unknown output format `%s'; supported formats are: %s",
	      output_format,
	      g_strjoinv ("/", const_cast<char**> (hb_buffer_serialize_list_formats ())));
      else
	/* Just default to TEXT if not explicitly requested and the
	 * file extension is not recognized. */
	serialize_format = HB_BUFFER_SERIALIZE_FORMAT_TEXT;
    }

    unsigned int flags = HB_BUFFER_SERIALIZE_FLAG_DEFAULT;
    if (!format.show_glyph_names)
      flags |= HB_BUFFER_SERIALIZE_FLAG_NO_GLYPH_NAMES;
    if (!format.show_clusters)
      flags |= HB_BUFFER_SERIALIZE_FLAG_NO_CLUSTERS;
    if (!format.show_positions)
      flags |= HB_BUFFER_SERIALIZE_FLAG_NO_POSITIONS;
    if (!format.show_advances)
      flags |= HB_BUFFER_SERIALIZE_FLAG_NO_ADVANCES;
    if (format.show_extents)
      flags |= HB_BUFFER_SERIALIZE_FLAG_GLYPH_EXTENTS;
    if (format.show_flags)
      flags |= HB_BUFFER_SERIALIZE_FLAG_GLYPH_FLAGS;
    serialize_flags = (hb_buffer_serialize_flags_t) flags;

    if (format.trace)
      hb_buffer_set_message_func (buffer, message_func, this, nullptr);
  }
  void new_line () { line_no++; }
  void consume_text (hb_buffer_t  *buffer,
		     const char   *text,
		     unsigned int  text_len,
		     hb_bool_t     utf8_clusters)
  {
    g_string_set_size (gs, 0);
    format.serialize_buffer_of_text (buffer, line_no, text, text_len, font, gs);
    fprintf (out_fp, "%s", gs->str);
  }
  void error (const char *message)
  {
    g_string_set_size (gs, 0);
    format.serialize_message (line_no, "error", message, gs);
    fprintf (out_fp, "%s", gs->str);
  }
  void consume_glyphs (hb_buffer_t  *buffer,
		       const char   *text,
		       unsigned int  text_len,
		       hb_bool_t     utf8_clusters)
  {
    g_string_set_size (gs, 0);
    format.serialize_buffer_of_glyphs (buffer, line_no, text, text_len, font,
				       serialize_format, serialize_flags, gs);
    fprintf (out_fp, "%s", gs->str);
  }
  void finish (hb_buffer_t *buffer, const font_options_t *font_opts)
  {
    hb_buffer_set_message_func (buffer, nullptr, nullptr, nullptr);
    hb_font_destroy (font);
    g_string_free (gs, true);
    gs = nullptr;
    font = nullptr;
  }

  static hb_bool_t
  message_func (hb_buffer_t *buffer,
		hb_font_t *font,
		const char *message,
		void *user_data)
  {
    output_buffer_t *that = (output_buffer_t *) user_data;
    that->trace (buffer, font, message);
    return true;
  }

  void
  trace (hb_buffer_t *buffer,
	 hb_font_t *font,
	 const char *message)
  {
    g_string_set_size (gs, 0);
    format.serialize_line_no (line_no, gs);
    g_string_append_printf (gs, "trace: %s	buffer: ", message);
    format.serialize (buffer, font, serialize_format, serialize_flags, gs);
    g_string_append_c (gs, '\n');
    fprintf (out_fp, "%s", gs->str);
  }


  protected:

  shape_format_options_t format;

  GString *gs = nullptr;
  unsigned int line_no = 0;
  hb_font_t *font = nullptr;
  hb_buffer_serialize_format_t serialize_format = HB_BUFFER_SERIALIZE_FORMAT_INVALID;
  hb_buffer_serialize_flags_t serialize_flags = HB_BUFFER_SERIALIZE_FLAG_DEFAULT;
};

int
main (int argc, char **argv)
{
  using main_t = main_font_text_t<shape_consumer_t<output_buffer_t>, font_options_t, shape_text_options_t>;
  return batch_main<main_t> (argc, argv);
}
