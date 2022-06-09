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

#ifndef SHAPE_FORMAT_OPTIONS_HH
#define SHAPE_FORMAT_OPTIONS_HH

#include "options.hh"


struct shape_format_options_t
{
  void add_options (option_parser_t *parser);

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


  hb_bool_t show_glyph_names = true;
  hb_bool_t show_positions = true;
  hb_bool_t show_advances = true;
  hb_bool_t show_clusters = true;
  hb_bool_t show_text = false;
  hb_bool_t show_unicode = false;
  hb_bool_t show_line_num = false;
  hb_bool_t show_extents = false;
  hb_bool_t show_flags = false;
  hb_bool_t trace = false;
};


static gboolean
parse_verbose (const char *name G_GNUC_UNUSED,
	       const char *arg G_GNUC_UNUSED,
	       gpointer    data G_GNUC_UNUSED,
	       GError    **error G_GNUC_UNUSED)
{
  shape_format_options_t *format_opts = (shape_format_options_t *) data;
  format_opts->show_text = format_opts->show_unicode = format_opts->show_line_num = true;
  return true;
}

static gboolean
parse_ned (const char *name G_GNUC_UNUSED,
	   const char *arg G_GNUC_UNUSED,
	   gpointer    data G_GNUC_UNUSED,
	   GError    **error G_GNUC_UNUSED)
{
  shape_format_options_t *format_opts = (shape_format_options_t *) data;
  format_opts->show_clusters = format_opts->show_advances = false;
  return true;
}

inline void
shape_format_options_t::serialize (hb_buffer_t *buffer,
				   hb_font_t   *font,
				   hb_buffer_serialize_format_t output_format,
				   hb_buffer_serialize_flags_t flags,
				   GString     *gs)
{
  unsigned int num_glyphs = hb_buffer_get_length (buffer);
  unsigned int start = 0;

  while (start < num_glyphs)
  {
    char buf[32768];
    unsigned int consumed;
    start += hb_buffer_serialize (buffer, start, num_glyphs,
				  buf, sizeof (buf), &consumed,
				  font, output_format, flags);
    if (!consumed)
      break;
    g_string_append (gs, buf);
  }
}

inline void
shape_format_options_t::serialize_line_no (unsigned int  line_no,
					   GString      *gs)
{
  if (show_line_num)
    g_string_append_printf (gs, "%d: ", line_no);
}
inline void
shape_format_options_t::serialize_buffer_of_text (hb_buffer_t  *buffer,
						  unsigned int  line_no,
						  const char   *text,
						  unsigned int  text_len,
						  hb_font_t    *font,
						  GString      *gs)
{
  if (show_text)
  {
    serialize_line_no (line_no, gs);
    g_string_append_c (gs, '(');
    g_string_append_len (gs, text, text_len);
    g_string_append_c (gs, ')');
    g_string_append_c (gs, '\n');
  }

  if (show_unicode)
  {
    serialize_line_no (line_no, gs);
    serialize (buffer, font, HB_BUFFER_SERIALIZE_FORMAT_TEXT, HB_BUFFER_SERIALIZE_FLAG_DEFAULT, gs);
    g_string_append_c (gs, '\n');
  }
}
inline void
shape_format_options_t::serialize_message (unsigned int  line_no,
					   const char   *type,
					   const char   *msg,
					   GString      *gs)
{
  serialize_line_no (line_no, gs);
  g_string_append_printf (gs, "%s: %s", type, msg);
  g_string_append_c (gs, '\n');
}
inline void
shape_format_options_t::serialize_buffer_of_glyphs (hb_buffer_t  *buffer,
						    unsigned int  line_no,
						    const char   *text,
						    unsigned int  text_len,
						    hb_font_t    *font,
						    hb_buffer_serialize_format_t output_format,
						    hb_buffer_serialize_flags_t format_flags,
						    GString      *gs)
{
  serialize_line_no (line_no, gs);
  serialize (buffer, font, output_format, format_flags, gs);
  g_string_append_c (gs, '\n');
}


void
shape_format_options_t::add_options (option_parser_t *parser)
{
  GOptionEntry entries[] =
  {
    {"show-text",	0, 0, G_OPTION_ARG_NONE,	&this->show_text,		"Prefix each line of output with its corresponding input text",		nullptr},
    {"show-unicode",	0, 0, G_OPTION_ARG_NONE,	&this->show_unicode,		"Prefix each line of output with its corresponding input codepoint(s)",	nullptr},
    {"show-line-num",	0, 0, G_OPTION_ARG_NONE,	&this->show_line_num,		"Prefix each line of output with its corresponding input line number",	nullptr},
    {"verbose",	      'v', G_OPTION_FLAG_NO_ARG,
			      G_OPTION_ARG_CALLBACK,	(gpointer) &parse_verbose,	"Prefix each line of output with all of the above",			nullptr},
    {"no-glyph-names",	0, G_OPTION_FLAG_REVERSE,
			      G_OPTION_ARG_NONE,	&this->show_glyph_names,	"Output glyph indices instead of names",				nullptr},
    {"no-positions",	0, G_OPTION_FLAG_REVERSE,
			      G_OPTION_ARG_NONE,	&this->show_positions,		"Do not output glyph positions",					nullptr},
    {"no-advances",	0, G_OPTION_FLAG_REVERSE,
			      G_OPTION_ARG_NONE,	&this->show_advances,		"Do not output glyph advances",						nullptr},
    {"no-clusters",	0, G_OPTION_FLAG_REVERSE,
			      G_OPTION_ARG_NONE,	&this->show_clusters,		"Do not output cluster indices",					nullptr},
    {"show-extents",	0, 0, G_OPTION_ARG_NONE,	&this->show_extents,		"Output glyph extents",							nullptr},
    {"show-flags",	0, 0, G_OPTION_ARG_NONE,	&this->show_flags,		"Output glyph flags",							nullptr},
    {"ned",	      'v', G_OPTION_FLAG_NO_ARG,
			      G_OPTION_ARG_CALLBACK,	(gpointer) &parse_ned,		"No Extra Data; Do not output clusters or advances",			nullptr},
    {"trace",	      'V', 0, G_OPTION_ARG_NONE,	&this->trace,			"Output interim shaping results",					nullptr},
    {nullptr}
  };
  parser->add_group (entries,
		     "output-syntax",
		     "Output syntax:\n"
	 "    text: [<glyph name or index>=<glyph cluster index within input>@<horizontal displacement>,<vertical displacement>+<horizontal advance>,<vertical advance>|...]\n"
	 "    json: [{\"g\": <glyph name or index>, \"ax\": <horizontal advance>, \"ay\": <vertical advance>, \"dx\": <horizontal displacement>, \"dy\": <vertical displacement>, \"cl\": <glyph cluster index within input>}, ...]\n"
	 "\nOutput syntax options:",
		     "Options for the syntax of the output",
		     this);
}

#endif
