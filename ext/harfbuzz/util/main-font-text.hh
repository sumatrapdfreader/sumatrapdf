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

#ifndef HB_MAIN_FONT_TEXT_HH
#define HB_MAIN_FONT_TEXT_HH

#include "options.hh"

/* main() body for utilities taking font and processing text.*/

template <typename consumer_t,
	  typename font_options_type,
	  typename text_options_type>
struct main_font_text_t :
       option_parser_t,
       font_options_type,
       text_options_type,
       consumer_t
{
  int operator () (int argc, char **argv)
  {
    add_options ();
    parse (&argc, &argv);

    this->init (this);

    while (this->consume_line (*this))
      ;

    this->finish (this);

    return this->failed ? 1 : 0;
  }

  protected:

  void add_options ()
  {
    font_options_type::add_options (this);
    text_options_type::add_options (this);
    consumer_t::add_options (this);

    GOptionEntry entries[] =
    {
      {G_OPTION_REMAINING,	0, G_OPTION_FLAG_IN_MAIN,
				G_OPTION_ARG_CALLBACK,	(gpointer) &collect_rest,	nullptr,	"[FONT-FILE] [TEXT]"},
      {nullptr}
    };
    add_main_group (entries, this);
    option_parser_t::add_options ();
  }

  private:

  static gboolean
  collect_rest (const char *name G_GNUC_UNUSED,
		const char *arg,
		gpointer    data,
		GError    **error)
  {
    main_font_text_t *thiz = (main_font_text_t *) data;

    if (!thiz->font_file)
    {
      thiz->font_file = g_strdup (arg);
      return true;
    }

    if (!thiz->text && !thiz->text_file)
    {
      thiz->text = g_strdup (arg);
      return true;
    }

    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		 "Too many arguments on the command line");
    return false;
  }
};

#endif
