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

#include "main-font-text.hh"
#include "shape-consumer.hh"

struct output_buffer_t
{
  output_buffer_t (option_parser_t *parser)
		  : options (parser, hb_buffer_serialize_list_formats ()),
		    format (parser),
		    gs (nullptr),
		    line_no (0),
		    font (nullptr),
		    output_format (HB_BUFFER_SERIALIZE_FORMAT_INVALID),
		    format_flags (HB_BUFFER_SERIALIZE_FLAG_DEFAULT) {}

  void init (hb_buffer_t *buffer, const font_options_t *font_opts)
  {
    options.get_file_handle ();
    gs = g_string_new (nullptr);
    line_no = 0;
    font = hb_font_reference (font_opts->get_font ());

    if (!options.output_format)
      output_format = HB_BUFFER_SERIALIZE_FORMAT_TEXT;
    else
      output_format = hb_buffer_serialize_format_from_string (options.output_format, -1);
    /* An empty "output_format" parameter basically skips output generating.
     * Useful for benchmarking. */
    if ((!options.output_format || *options.output_format) &&
	!hb_buffer_serialize_format_to_string (output_format))
    {
      if (options.explicit_output_format)
	fail (false, "Unknown output format `%s'; supported formats are: %s",
	      options.output_format,
	      g_strjoinv ("/", const_cast<char**> (options.supported_formats)));
      else
	/* Just default to TEXT if not explicitly requested and the
	 * file extension is not recognized. */
	output_format = HB_BUFFER_SERIALIZE_FORMAT_TEXT;
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
    format_flags = (hb_buffer_serialize_flags_t) flags;

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
    fprintf (options.fp, "%s", gs->str);
  }
  void error (const char *message)
  {
    g_string_set_size (gs, 0);
    format.serialize_message (line_no, "error", message, gs);
    fprintf (options.fp, "%s", gs->str);
  }
  void consume_glyphs (hb_buffer_t  *buffer,
		       const char   *text,
		       unsigned int  text_len,
		       hb_bool_t     utf8_clusters)
  {
    g_string_set_size (gs, 0);
    format.serialize_buffer_of_glyphs (buffer, line_no, text, text_len, font,
				       output_format, format_flags, gs);
    fprintf (options.fp, "%s", gs->str);
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
    format.serialize (buffer, font, output_format, format_flags, gs);
    g_string_append_c (gs, '\n');
    fprintf (options.fp, "%s", gs->str);
  }


  protected:
  output_options_t options;
  format_options_t format;

  GString *gs;
  unsigned int line_no;
  hb_font_t *font;
  hb_buffer_serialize_format_t output_format;
  hb_buffer_serialize_flags_t format_flags;
};

int
main (int argc, char **argv)
{
  if (argc == 2 && !strcmp (argv[1], "--batch"))
  {
    unsigned int ret = 0;
    char buf[4092];
    while (fgets (buf, sizeof (buf), stdin))
    {
      size_t l = strlen (buf);
      if (l && buf[l - 1] == '\n') buf[l - 1] = '\0';
      main_font_text_t<shape_consumer_t<output_buffer_t>, FONT_SIZE_UPEM, 0> driver;
      char *args[32];
      argc = 0;
      char *p = buf, *e;
      args[argc++] = p;
      unsigned start_offset = 0;
      while ((e = strchr (p + start_offset, ':')) && argc < (int) ARRAY_LENGTH (args))
      {
	*e++ = '\0';
	while (*e == ':')
	  e++;
	args[argc++] = p = e;
	/* Skip 2 first bytes on first argument if is Windows path, "C:\..." */
	start_offset = argc == 2 && p[0] != '\0' && p[0] != ':' && p[1] == ':' && (p[2] == '\\' || p[2] == '/') ? 2 : 0;
      }
      ret |= driver.main (argc, args);
      fflush (stdout);

      if (ret)
	break;
    }
    return ret;
  }
  main_font_text_t<shape_consumer_t<output_buffer_t>, FONT_SIZE_UPEM, 0> driver;
  return driver.main (argc, argv);
}
