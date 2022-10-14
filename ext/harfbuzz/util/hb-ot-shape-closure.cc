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

#include "batch.hh"
#include "font-options.hh"
#include "main-font-text.hh"
#include "shape-options.hh"
#include "text-options.hh"

const unsigned DEFAULT_FONT_SIZE = FONT_SIZE_NONE;
const unsigned SUBPIXEL_BITS = 0;

struct shape_closure_consumer_t
{
  void add_options (struct option_parser_t *parser)
  {
    parser->set_summary ("Find glyph set from input text under shaping closure.");
    shaper.add_options (parser);

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

  void init (const font_options_t *font_opts)
  {
    glyphs = hb_set_create ();
    font = hb_font_reference (font_opts->font);
    failed = false;
    buffer = hb_buffer_create ();
  }
  template <typename text_options_type>
  bool consume_line (text_options_type &text_opts)
  {
    unsigned int text_len;
    const char *text;
    if (!(text = text_opts.get_line (&text_len)))
      return false;

    hb_set_clear (glyphs);
    shaper.shape_closure (text, text_len, font, buffer, glyphs);

    if (hb_set_is_empty (glyphs))
      return true;

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

    return true;
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
  hb_bool_t show_glyph_names = true;

  hb_set_t *glyphs = nullptr;
  hb_font_t *font = nullptr;
  hb_buffer_t *buffer = nullptr;
};

int
main (int argc, char **argv)
{
  using main_t = main_font_text_t<shape_closure_consumer_t, font_options_t, text_options_t>;
  return batch_main<main_t> (argc, argv);
}
