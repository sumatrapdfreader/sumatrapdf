/*
 * Copyright © 2012  Google, Inc.
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

#ifdef HAVE_FREETYPE
#include <hb-ft.h>
#endif

struct shape_closure_consumer_t : option_group_t
{
  shape_closure_consumer_t (option_parser_t *parser) :
			    shaper (parser),
			    show_glyph_names (true)
  {
    add_options (parser);
  }

  void add_options (struct option_parser_t *parser)
  {
    GOptionEntry entries[] =
    {
      {"no-glyph-names",	0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE,	&this->show_glyph_names,	"Use glyph indices instead of names",	nullptr},
      {nullptr}
    };
    parser->add_group (entries,
		       "format",
		       "Format options:",
		       "Options controlling output formatting",
		       this);
  }

  void init (hb_buffer_t  *buffer_,
	     const font_options_t *font_opts)
  {
    glyphs = hb_set_create ();
    font = hb_font_reference (font_opts->get_font ());
    failed = false;
    buffer = hb_buffer_reference (buffer_);
  }
  void consume_line (const char   *text,
		     unsigned int  text_len,
		     const char   *text_before,
		     const char   *text_after)
  {
    hb_set_clear (glyphs);
    shaper.shape_closure (text, text_len, font, buffer, glyphs);

    if (hb_set_is_empty (glyphs))
      return;

    /* Print it out! */
    bool first = true;
    for (hb_codepoint_t i = -1; hb_set_next (glyphs, &i);)
    {
      if (first)
	first = false;
      else
	printf (" ");
      if (show_glyph_names)
      {
	char glyph_name[64];
	hb_font_glyph_to_string (font, i, glyph_name, sizeof (glyph_name));
	printf ("%s", glyph_name);
      } else
	printf ("%u", i);
    }
  }
  void finish (const font_options_t *font_opts)
  {
    printf ("\n");
    hb_font_destroy (font);
    font = nullptr;
    hb_set_destroy (glyphs);
    glyphs = nullptr;
    hb_buffer_destroy (buffer);
    buffer = nullptr;
  }

  bool failed;

  protected:
  shape_options_t shaper;
  hb_bool_t show_glyph_names;

  hb_set_t *glyphs;
  hb_font_t *font;
  hb_buffer_t *buffer;
};

int
main (int argc, char **argv)
{
  main_font_text_t<shape_closure_consumer_t, FONT_SIZE_NONE, 0> driver;
  return driver.main (argc, argv);
}
