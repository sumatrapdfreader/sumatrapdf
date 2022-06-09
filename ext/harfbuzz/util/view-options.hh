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

#ifndef VIEW_OPTIONS_HH
#define VIEW_OPTIONS_HH

#include "options.hh"

#define DEFAULT_MARGIN 16
#define DEFAULT_FORE "#000000"
#define DEFAULT_BACK "#FFFFFF"

struct view_options_t
{
  ~view_options_t ()
  {
    g_free (fore);
    g_free (back);
  }

  void add_options (option_parser_t *parser);

  hb_bool_t annotate = false;
  char *fore = nullptr;
  char *back = nullptr;
  double line_space = 0;
  bool have_font_extents = false;
  struct font_extents_t {
    double ascent, descent, line_gap;
  } font_extents = {0., 0., 0.};
  struct margin_t {
    double t, r, b, l;
  } margin = {DEFAULT_MARGIN, DEFAULT_MARGIN, DEFAULT_MARGIN, DEFAULT_MARGIN};
};


static gboolean
parse_font_extents (const char *name G_GNUC_UNUSED,
		    const char *arg,
		    gpointer    data,
		    GError    **error G_GNUC_UNUSED)
{
  view_options_t *view_opts = (view_options_t *) data;
  view_options_t::font_extents_t &e = view_opts->font_extents;
  switch (sscanf (arg, "%lf%*[ ,]%lf%*[ ,]%lf", &e.ascent, &e.descent, &e.line_gap)) {
    case 1: HB_FALLTHROUGH;
    case 2: HB_FALLTHROUGH;
    case 3:
      view_opts->have_font_extents = true;
      return true;
    default:
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "%s argument should be one to three space-separated numbers",
		   name);
      return false;
  }
}

static gboolean
parse_margin (const char *name G_GNUC_UNUSED,
	      const char *arg,
	      gpointer    data,
	      GError    **error G_GNUC_UNUSED)
{
  view_options_t *view_opts = (view_options_t *) data;
  view_options_t::margin_t &m = view_opts->margin;
  switch (sscanf (arg, "%lf%*[ ,]%lf%*[ ,]%lf%*[ ,]%lf", &m.t, &m.r, &m.b, &m.l)) {
    case 1: m.r = m.t; HB_FALLTHROUGH;
    case 2: m.b = m.t; HB_FALLTHROUGH;
    case 3: m.l = m.r; HB_FALLTHROUGH;
    case 4: return true;
    default:
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "%s argument should be one to four space-separated numbers",
		   name);
      return false;
  }
}

void
view_options_t::add_options (option_parser_t *parser)
{
  GOptionEntry entries[] =
  {
    {"annotate",	0, 0, G_OPTION_ARG_NONE,	&this->annotate,		"Annotate output rendering",				nullptr},
    {"background",	0, 0, G_OPTION_ARG_STRING,	&this->back,			"Set background color (default: " DEFAULT_BACK ")",	"rrggbb/rrggbbaa"},
    {"foreground",	0, 0, G_OPTION_ARG_STRING,	&this->fore,			"Set foreground color (default: " DEFAULT_FORE ")",	"rrggbb/rrggbbaa"},
    {"line-space",	0, 0, G_OPTION_ARG_DOUBLE,	&this->line_space,		"Set space between lines (default: 0)",			"units"},
    {"font-extents",	0, 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_font_extents,	"Set font ascent/descent/line-gap (default: auto)","one to three numbers"},
    {"margin",		0, 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_margin,	"Margin around output (default: " G_STRINGIFY(DEFAULT_MARGIN) ")","one to four numbers"},
    {nullptr}
  };
  parser->add_group (entries,
		     "view",
		     "View options:",
		     "Options for output rendering",
		     this);
}

#endif
