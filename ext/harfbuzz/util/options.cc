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

#include "options.hh"

#ifdef HAVE_FREETYPE
#include <hb-ft.h>
#endif
#include <hb-ot.h>

#define DELIMITERS "<+>{},;&#\\xXuUnNiI\n\t\v\f\r "

static struct supported_font_funcs_t {
	char name[4];
	void (*func) (hb_font_t *);
} supported_font_funcs[] =
{
#ifdef HAVE_FREETYPE
  {"ft",	hb_ft_font_set_funcs},
#endif
  {"ot",	hb_ot_font_set_funcs},
};


void
fail (hb_bool_t suggest_help, const char *format, ...)
{
  const char *msg;

  va_list vap;
  va_start (vap, format);
  msg = g_strdup_vprintf (format, vap);
  va_end (vap);
  const char *prgname = g_get_prgname ();
  g_printerr ("%s: %s\n", prgname, msg);
  if (suggest_help)
    g_printerr ("Try `%s --help' for more information.\n", prgname);

  exit (1);
}


static gchar *
shapers_to_string ()
{
  GString *shapers = g_string_new (nullptr);
  const char **shaper_list = hb_shape_list_shapers ();

  for (; *shaper_list; shaper_list++) {
    g_string_append (shapers, *shaper_list);
    g_string_append_c (shapers, ',');
  }
  g_string_truncate (shapers, MAX (0, (gint)shapers->len - 1));

  return g_string_free (shapers, false);
}

static G_GNUC_NORETURN gboolean
show_version (const char *name G_GNUC_UNUSED,
	      const char *arg G_GNUC_UNUSED,
	      gpointer    data G_GNUC_UNUSED,
	      GError    **error G_GNUC_UNUSED)
{
  g_printf ("%s (%s) %s\n", g_get_prgname (), PACKAGE_NAME, PACKAGE_VERSION);

  char *shapers = shapers_to_string ();
  g_printf ("Available shapers: %s\n", shapers);
  g_free (shapers);
  if (strcmp (HB_VERSION_STRING, hb_version_string ()))
    g_printf ("Linked HarfBuzz library has a different version: %s\n", hb_version_string ());

  exit(0);
}


void
option_parser_t::add_main_options ()
{
  GOptionEntry entries[] =
  {
    {"version",		0, G_OPTION_FLAG_NO_ARG,
			      G_OPTION_ARG_CALLBACK,	(gpointer) &show_version,	"Show version numbers",			nullptr},
    {nullptr}
  };
  g_option_context_add_main_entries (context, entries, nullptr);
}

static gboolean
pre_parse (GOptionContext *context G_GNUC_UNUSED,
	   GOptionGroup *group G_GNUC_UNUSED,
	   gpointer data,
	   GError **error)
{
  option_group_t *option_group = (option_group_t *) data;
  option_group->pre_parse (error);
  return !*error;
}

static gboolean
post_parse (GOptionContext *context G_GNUC_UNUSED,
	    GOptionGroup *group G_GNUC_UNUSED,
	    gpointer data,
	    GError **error)
{
  option_group_t *option_group = static_cast<option_group_t *>(data);
  option_group->post_parse (error);
  return !*error;
}

void
option_parser_t::add_group (GOptionEntry   *entries,
			    const gchar    *name,
			    const gchar    *description,
			    const gchar    *help_description,
			    option_group_t *option_group)
{
  GOptionGroup *group = g_option_group_new (name, description, help_description,
					    static_cast<gpointer>(option_group), nullptr);
  g_option_group_add_entries (group, entries);
  g_option_group_set_parse_hooks (group, pre_parse, post_parse);
  g_option_context_add_group (context, group);
}

void
option_parser_t::parse (int *argc, char ***argv)
{
  setlocale (LC_ALL, "");

  GError *parse_error = nullptr;
  if (!g_option_context_parse (context, argc, argv, &parse_error))
  {
    if (parse_error)
    {
      fail (true, "%s", parse_error->message);
      //g_error_free (parse_error);
    }
    else
      fail (true, "Option parse error");
  }
}


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


static gboolean
parse_shapers (const char *name G_GNUC_UNUSED,
	       const char *arg,
	       gpointer    data,
	       GError    **error)
{
  shape_options_t *shape_opts = (shape_options_t *) data;
  char **shapers = g_strsplit (arg, ",", 0);

  for (char **shaper = shapers; *shaper; shaper++) {
    bool found = false;
    for (const char **hb_shaper = hb_shape_list_shapers (); *hb_shaper; hb_shaper++) {
      if (strcmp (*shaper, *hb_shaper) == 0) {
	found = true;
	break;
      }
    }
    if (!found) {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Unknown or unsupported shaper: %s", *shaper);
      g_strfreev (shapers);
      return false;
    }
  }

  g_strfreev (shape_opts->shapers);
  shape_opts->shapers = shapers;
  return true;
}

static G_GNUC_NORETURN gboolean
list_shapers (const char *name G_GNUC_UNUSED,
	      const char *arg G_GNUC_UNUSED,
	      gpointer    data G_GNUC_UNUSED,
	      GError    **error G_GNUC_UNUSED)
{
  for (const char **shaper = hb_shape_list_shapers (); *shaper; shaper++)
    g_printf ("%s\n", *shaper);

  exit(0);
}


static gboolean
parse_features (const char *name G_GNUC_UNUSED,
		const char *arg,
		gpointer    data,
		GError    **error G_GNUC_UNUSED)
{
  shape_options_t *shape_opts = (shape_options_t *) data;
  char *s = (char *) arg;
  char *p;

  shape_opts->num_features = 0;
  g_free (shape_opts->features);
  shape_opts->features = nullptr;

  if (!*s)
    return true;

  /* count the features first, so we can allocate memory */
  p = s;
  do {
    shape_opts->num_features++;
    p = strchr (p, ',');
    if (p)
      p++;
  } while (p);

  shape_opts->features = (hb_feature_t *) calloc (shape_opts->num_features, sizeof (*shape_opts->features));
  if (!shape_opts->features)
    return false;

  /* now do the actual parsing */
  p = s;
  shape_opts->num_features = 0;
  while (p && *p) {
    char *end = strchr (p, ',');
    if (hb_feature_from_string (p, end ? end - p : -1, &shape_opts->features[shape_opts->num_features]))
      shape_opts->num_features++;
    p = end ? end + 1 : nullptr;
  }

  return true;
}

static gboolean
parse_variations (const char *name G_GNUC_UNUSED,
		  const char *arg,
		  gpointer    data,
		  GError    **error G_GNUC_UNUSED)
{
  font_options_t *font_opts = (font_options_t *) data;
  char *s = (char *) arg;
  char *p;

  font_opts->num_variations = 0;
  g_free (font_opts->variations);
  font_opts->variations = nullptr;

  if (!*s)
    return true;

  /* count the variations first, so we can allocate memory */
  p = s;
  do {
    font_opts->num_variations++;
    p = strchr (p, ',');
    if (p)
      p++;
  } while (p);

  font_opts->variations = (hb_variation_t *) calloc (font_opts->num_variations, sizeof (*font_opts->variations));
  if (!font_opts->variations)
    return false;

  /* now do the actual parsing */
  p = s;
  font_opts->num_variations = 0;
  while (p && *p) {
    char *end = strchr (p, ',');
    if (hb_variation_from_string (p, end ? end - p : -1, &font_opts->variations[font_opts->num_variations]))
      font_opts->num_variations++;
    p = end ? end + 1 : nullptr;
  }

  return true;
}

static gboolean
parse_text (const char *name G_GNUC_UNUSED,
	    const char *arg,
	    gpointer    data,
	    GError    **error G_GNUC_UNUSED)
{
  text_options_t *text_opts = (text_options_t *) data;

  if (text_opts->text)
  {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		 "Either --text or --unicodes can be provided but not both");
    return false;
  }

  text_opts->text_len = -1;
  text_opts->text = g_strdup (arg);
  return true;
}


static gboolean
parse_unicodes (const char *name G_GNUC_UNUSED,
		const char *arg,
		gpointer    data,
		GError    **error G_GNUC_UNUSED)
{
  text_options_t *text_opts = (text_options_t *) data;

  if (text_opts->text)
  {
    g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		 "Either --text or --unicodes can be provided but not both");
    return false;
  }

  GString *gs = g_string_new (nullptr);
  if (0 == strcmp (arg, "*"))
  {
    g_string_append_c (gs, '*');
  }
  else
  {

    char *s = (char *) arg;
    char *p;

    while (s && *s)
    {
      while (*s && strchr (DELIMITERS, *s))
	s++;
      if (!*s)
	break;

      errno = 0;
      hb_codepoint_t u = strtoul (s, &p, 16);
      if (errno || s == p)
      {
	g_string_free (gs, TRUE);
	g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		     "Failed parsing Unicode values at: '%s'", s);
	return false;
      }

      g_string_append_unichar (gs, u);

      s = p;
    }
  }

  text_opts->text_len = gs->len;
  text_opts->text = g_string_free (gs, FALSE);
  return true;
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

void
shape_options_t::add_options (option_parser_t *parser)
{
  GOptionEntry entries[] =
  {
    {"list-shapers",	0, G_OPTION_FLAG_NO_ARG,
			      G_OPTION_ARG_CALLBACK,	(gpointer) &list_shapers,	"List available shapers and quit",	nullptr},
    {"shaper",		0, G_OPTION_FLAG_HIDDEN,
			      G_OPTION_ARG_CALLBACK,	(gpointer) &parse_shapers,	"Hidden duplicate of --shapers",	nullptr},
    {"shapers",		0, 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_shapers,	"Set comma-separated list of shapers to try","list"},
    {"direction",	0, 0, G_OPTION_ARG_STRING,	&this->direction,		"Set text direction (default: auto)",	"ltr/rtl/ttb/btt"},
    {"language",	0, 0, G_OPTION_ARG_STRING,	&this->language,		"Set text language (default: $LANG)",	"langstr"},
    {"script",		0, 0, G_OPTION_ARG_STRING,	&this->script,			"Set text script (default: auto)",	"ISO-15924 tag"},
    {"bot",		0, 0, G_OPTION_ARG_NONE,	&this->bot,			"Treat text as beginning-of-paragraph",	nullptr},
    {"eot",		0, 0, G_OPTION_ARG_NONE,	&this->eot,			"Treat text as end-of-paragraph",	nullptr},
    {"preserve-default-ignorables",0, 0, G_OPTION_ARG_NONE,	&this->preserve_default_ignorables,	"Preserve Default-Ignorable characters",	nullptr},
    {"remove-default-ignorables",0, 0, G_OPTION_ARG_NONE,	&this->remove_default_ignorables,	"Remove Default-Ignorable characters",	nullptr},
    {"invisible-glyph",	0, 0, G_OPTION_ARG_INT,		&this->invisible_glyph,		"Glyph value to replace Default-Ignorables with",	nullptr},
    {"utf8-clusters",	0, 0, G_OPTION_ARG_NONE,	&this->utf8_clusters,		"Use UTF8 byte indices, not char indices",	nullptr},
    {"cluster-level",	0, 0, G_OPTION_ARG_INT,		&this->cluster_level,		"Cluster merging level (default: 0)",	"0/1/2"},
    {"normalize-glyphs",0, 0, G_OPTION_ARG_NONE,	&this->normalize_glyphs,	"Rearrange glyph clusters in nominal order",	nullptr},
    {"verify",		0, 0, G_OPTION_ARG_NONE,	&this->verify,			"Perform sanity checks on shaping results",	nullptr},
    {"num-iterations", 'n', 0, G_OPTION_ARG_INT,		&this->num_iterations,		"Run shaper N times (default: 1)",	"N"},
    {nullptr}
  };
  parser->add_group (entries,
		     "shape",
		     "Shape options:",
		     "Options for the shaping process",
		     this);

  const gchar *features_help = "Comma-separated list of font features\n"
    "\n"
    "    Features can be enabled or disabled, either globally or limited to\n"
    "    specific character ranges.  The format for specifying feature settings\n"
    "    follows.  All valid CSS font-feature-settings values other than 'normal'\n"
    "    and the global values are also accepted, though not documented below.\n"
    "    CSS string escapes are not supported."
    "\n"
    "    The range indices refer to the positions between Unicode characters,\n"
    "    unless the --utf8-clusters is provided, in which case range indices\n"
    "    refer to UTF-8 byte indices. The position before the first character\n"
    "    is always 0.\n"
    "\n"
    "    The format is Python-esque.  Here is how it all works:\n"
    "\n"
    "      Syntax:       Value:    Start:    End:\n"
    "\n"
    "    Setting value:\n"
    "      \"kern\"        1         0         ∞         # Turn feature on\n"
    "      \"+kern\"       1         0         ∞         # Turn feature on\n"
    "      \"-kern\"       0         0         ∞         # Turn feature off\n"
    "      \"kern=0\"      0         0         ∞         # Turn feature off\n"
    "      \"kern=1\"      1         0         ∞         # Turn feature on\n"
    "      \"aalt=2\"      2         0         ∞         # Choose 2nd alternate\n"
    "\n"
    "    Setting index:\n"
    "      \"kern[]\"      1         0         ∞         # Turn feature on\n"
    "      \"kern[:]\"     1         0         ∞         # Turn feature on\n"
    "      \"kern[5:]\"    1         5         ∞         # Turn feature on, partial\n"
    "      \"kern[:5]\"    1         0         5         # Turn feature on, partial\n"
    "      \"kern[3:5]\"   1         3         5         # Turn feature on, range\n"
    "      \"kern[3]\"     1         3         3+1       # Turn feature on, single char\n"
    "\n"
    "    Mixing it all:\n"
    "\n"
    "      \"aalt[3:5]=2\" 2         3         5         # Turn 2nd alternate on for range";

  GOptionEntry entries2[] =
  {
    {"features",	0, 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_features,	features_help,	"list"},
    {nullptr}
  };
  parser->add_group (entries2,
		     "features",
		     "Features options:",
		     "Options for font features used",
		     this);
}

static gboolean
parse_font_size (const char *name G_GNUC_UNUSED,
		 const char *arg,
		 gpointer    data,
		 GError    **error G_GNUC_UNUSED)
{
  font_options_t *font_opts = (font_options_t *) data;
  if (0 == strcmp (arg, "upem"))
  {
    font_opts->font_size_y = font_opts->font_size_x = FONT_SIZE_UPEM;
    return true;
  }
  switch (sscanf (arg, "%lf%*[ ,]%lf", &font_opts->font_size_x, &font_opts->font_size_y)) {
    case 1: font_opts->font_size_y = font_opts->font_size_x; HB_FALLTHROUGH;
    case 2: return true;
    default:
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "%s argument should be one or two space-separated numbers",
		   name);
      return false;
  }
}

static gboolean
parse_font_ppem (const char *name G_GNUC_UNUSED,
		 const char *arg,
		 gpointer    data,
		 GError    **error G_GNUC_UNUSED)
{
  font_options_t *font_opts = (font_options_t *) data;
  switch (sscanf (arg, "%d%*[ ,]%d", &font_opts->x_ppem, &font_opts->y_ppem)) {
    case 1: font_opts->y_ppem = font_opts->x_ppem; HB_FALLTHROUGH;
    case 2: return true;
    default:
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "%s argument should be one or two space-separated numbers",
		   name);
      return false;
  }
}

void
font_options_t::add_options (option_parser_t *parser)
{
  char *text = nullptr;

  {
    static_assert ((ARRAY_LENGTH_CONST (supported_font_funcs) > 0),
		   "No supported font-funcs found.");
    GString *s = g_string_new (nullptr);
    g_string_printf (s, "Set font functions implementation to use (default: %s)\n\n    Supported font function implementations are: %s",
		     supported_font_funcs[0].name,
		     supported_font_funcs[0].name);
    for (unsigned int i = 1; i < ARRAY_LENGTH (supported_font_funcs); i++)
    {
      g_string_append_c (s, '/');
      g_string_append (s, supported_font_funcs[i].name);
    }
    text = g_string_free (s, FALSE);
    parser->free_later (text);
  }

  char *font_size_text;
  if (default_font_size == FONT_SIZE_UPEM)
    font_size_text = (char *) "Font size (default: upem)";
  else
  {
    font_size_text = g_strdup_printf ("Font size (default: %d)", default_font_size);
    parser->free_later (font_size_text);
  }

  GOptionEntry entries[] =
  {
    {"font-file",	0, 0, G_OPTION_ARG_STRING,	&this->font_file,		"Set font file-name",				"filename"},
    {"face-index",	0, 0, G_OPTION_ARG_INT,		&this->face_index,		"Set face index (default: 0)",			"index"},
    {"font-size",	0, default_font_size ? 0 : G_OPTION_FLAG_HIDDEN,
			      G_OPTION_ARG_CALLBACK,	(gpointer) &parse_font_size,	font_size_text,					"1/2 integers or 'upem'"},
    {"font-ppem",	0, 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_font_ppem,	"Set x,y pixels per EM (default: 0; disabled)",	"1/2 integers"},
    {"font-ptem",	0, 0, G_OPTION_ARG_DOUBLE,	&this->ptem,			"Set font point-size (default: 0; disabled)",	"point-size"},
    {"font-funcs",	0, 0, G_OPTION_ARG_STRING,	&this->font_funcs,		text,						"impl"},
    {"ft-load-flags",	0, 0, G_OPTION_ARG_INT,		&this->ft_load_flags,		"Set FreeType load-flags (default: 2)",		"integer"},
    {nullptr}
  };
  parser->add_group (entries,
		     "font",
		     "Font options:",
		     "Options for the font",
		     this);

  const gchar *variations_help = "Comma-separated list of font variations\n"
    "\n"
    "    Variations are set globally. The format for specifying variation settings\n"
    "    follows.  All valid CSS font-variation-settings values other than 'normal'\n"
    "    and 'inherited' are also accepted, though, not documented below.\n"
    "\n"
    "    The format is a tag, optionally followed by an equals sign, followed by a\n"
    "    number. For example:\n"
    "\n"
    "      \"wght=500\"\n"
    "      \"slnt=-7.5\"\n";

  GOptionEntry entries2[] =
  {
    {"variations",	0, 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_variations,	variations_help,	"list"},
    {nullptr}
  };
  parser->add_group (entries2,
		     "variations",
		     "Variations options:",
		     "Options for font variations used",
		     this);
}

void
text_options_t::add_options (option_parser_t *parser)
{
  GOptionEntry entries[] =
  {
    {"text",		0, 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_text,		"Set input text",			"string"},
    {"text-file",	0, 0, G_OPTION_ARG_STRING,	&this->text_file,		"Set input text file-name\n\n    If no text is provided, standard input is used for input.\n",		"filename"},
    {"unicodes",      'u', 0, G_OPTION_ARG_CALLBACK,	(gpointer) &parse_unicodes,		"Set input Unicode codepoints",		"list of hex numbers"},
    {"text-before",	0, 0, G_OPTION_ARG_STRING,	&this->text_before,		"Set text context before each line",	"string"},
    {"text-after",	0, 0, G_OPTION_ARG_STRING,	&this->text_after,		"Set text context after each line",	"string"},
    {nullptr}
  };
  parser->add_group (entries,
		     "text",
		     "Text options:",
		     "Options for the input text",
		     this);
}

void
output_options_t::add_options (option_parser_t *parser)
{
  const char *text;

  if (!supported_formats)
    text = "Set output serialization format";
  else
  {
    char *items = g_strjoinv ("/", const_cast<char **> (supported_formats));
    text = g_strdup_printf ("Set output format\n\n    Supported output formats are: %s", items);
    g_free (items);
    parser->free_later ((char *) text);
  }

  GOptionEntry entries[] =
  {
    {"output-file",   'o', 0, G_OPTION_ARG_STRING,	&this->output_file,		"Set output file-name (default: stdout)","filename"},
    {"output-format", 'O', 0, G_OPTION_ARG_STRING,	&this->output_format,		text,					"format"},
    {nullptr}
  };
  parser->add_group (entries,
		     "output",
		     "Output destination & format options:",
		     "Options for the destination & form of the output",
		     this);
}



hb_font_t *
font_options_t::get_font () const
{
  if (font)
    return font;

  /* Create the blob */
  if (!font_file)
    fail (true, "No font file set");

  const char *font_path = font_file;

  if (0 == strcmp (font_path, "-"))
  {
#if defined(_WIN32) || defined(__CYGWIN__)
    setmode (fileno (stdin), O_BINARY);
    font_path = "STDIN";
#else
    font_path = "/dev/stdin";
#endif
  }

  blob = hb_blob_create_from_file (font_path);

  if (blob == hb_blob_get_empty ())
    fail (false, "Couldn't read or find %s, or it was empty.", font_path);

  /* Create the face */
  hb_face_t *face = hb_face_create (blob, face_index);
  hb_blob_destroy (blob);


  font = hb_font_create (face);

  if (font_size_x == FONT_SIZE_UPEM)
    font_size_x = hb_face_get_upem (face);
  if (font_size_y == FONT_SIZE_UPEM)
    font_size_y = hb_face_get_upem (face);

  hb_font_set_ppem (font, x_ppem, y_ppem);
  hb_font_set_ptem (font, ptem);

  int scale_x = (int) scalbnf (font_size_x, subpixel_bits);
  int scale_y = (int) scalbnf (font_size_y, subpixel_bits);
  hb_font_set_scale (font, scale_x, scale_y);
  hb_face_destroy (face);

  hb_font_set_variations (font, variations, num_variations);

  void (*set_font_funcs) (hb_font_t *) = nullptr;
  if (!font_funcs)
  {
    set_font_funcs = supported_font_funcs[0].func;
  }
  else
  {
    for (unsigned int i = 0; i < ARRAY_LENGTH (supported_font_funcs); i++)
      if (0 == g_ascii_strcasecmp (font_funcs, supported_font_funcs[i].name))
      {
	set_font_funcs = supported_font_funcs[i].func;
	break;
      }
    if (!set_font_funcs)
    {
      GString *s = g_string_new (nullptr);
      for (unsigned int i = 0; i < ARRAY_LENGTH (supported_font_funcs); i++)
      {
	if (i)
	  g_string_append_c (s, '/');
	g_string_append (s, supported_font_funcs[i].name);
      }
      char *p = g_string_free (s, FALSE);
      fail (false, "Unknown font function implementation `%s'; supported values are: %s; default is %s",
	    font_funcs,
	    p,
	    supported_font_funcs[0].name);
      //free (p);
    }
  }
  set_font_funcs (font);
#ifdef HAVE_FREETYPE
  hb_ft_font_set_load_flags (font, ft_load_flags);
#endif

  return font;
}


const char *
text_options_t::get_line (unsigned int *len)
{
  if (text) {
    if (!line)
    {
      line = text;
      line_len = text_len;
    }
    if (line_len == UINT_MAX)
      line_len = strlen (line);

    if (!line_len) {
      *len = 0;
      return nullptr;
    }

    const char *ret = line;
    const char *p = (const char *) memchr (line, '\n', line_len);
    unsigned int ret_len;
    if (!p) {
      ret_len = line_len;
      line += ret_len;
      line_len = 0;
    } else {
      ret_len = p - ret;
      line += ret_len + 1;
      line_len -= ret_len + 1;
    }

    *len = ret_len;
    return ret;
  }

  if (!fp) {
    if (!text_file)
      fail (true, "At least one of text or text-file must be set");

    if (0 != strcmp (text_file, "-"))
      fp = fopen (text_file, "r");
    else
      fp = stdin;

    if (!fp)
      fail (false, "Failed opening text file `%s': %s",
	    text_file, strerror (errno));

    gs = g_string_new (nullptr);
  }

  g_string_set_size (gs, 0);
  char buf[BUFSIZ];
  while (fgets (buf, sizeof (buf), fp)) {
    unsigned int bytes = strlen (buf);
    if (bytes && buf[bytes - 1] == '\n') {
      bytes--;
      g_string_append_len (gs, buf, bytes);
      break;
    }
      g_string_append_len (gs, buf, bytes);
  }
  if (ferror (fp))
    fail (false, "Failed reading text: %s",
	  strerror (errno));
  *len = gs->len;
  return !*len && feof (fp) ? nullptr : gs->str;
}


FILE *
output_options_t::get_file_handle ()
{
  if (fp)
    return fp;

  if (output_file)
    fp = fopen (output_file, "wb");
  else {
#if defined(_WIN32) || defined(__CYGWIN__)
    setmode (fileno (stdout), O_BINARY);
#endif
    fp = stdout;
  }
  if (!fp)
    fail (false, "Cannot open output file `%s': %s",
	  g_filename_display_name (output_file), strerror (errno));

  return fp;
}

static gboolean
parse_verbose (const char *name G_GNUC_UNUSED,
	       const char *arg G_GNUC_UNUSED,
	       gpointer    data G_GNUC_UNUSED,
	       GError    **error G_GNUC_UNUSED)
{
  format_options_t *format_opts = (format_options_t *) data;
  format_opts->show_text = format_opts->show_unicode = format_opts->show_line_num = true;
  return true;
}

static gboolean
parse_ned (const char *name G_GNUC_UNUSED,
	   const char *arg G_GNUC_UNUSED,
	   gpointer    data G_GNUC_UNUSED,
	   GError    **error G_GNUC_UNUSED)
{
  format_options_t *format_opts = (format_options_t *) data;
  format_opts->show_clusters = format_opts->show_advances = false;
  return true;
}

void
format_options_t::add_options (option_parser_t *parser)
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

void
format_options_t::serialize (hb_buffer_t *buffer,
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

void
format_options_t::serialize_line_no (unsigned int  line_no,
				     GString      *gs)
{
  if (show_line_num)
    g_string_append_printf (gs, "%d: ", line_no);
}
void
format_options_t::serialize_buffer_of_text (hb_buffer_t  *buffer,
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
void
format_options_t::serialize_message (unsigned int  line_no,
				     const char   *type,
				     const char   *msg,
				     GString      *gs)
{
  serialize_line_no (line_no, gs);
  g_string_append_printf (gs, "%s: %s", type, msg);
  g_string_append_c (gs, '\n');
}
void
format_options_t::serialize_buffer_of_glyphs (hb_buffer_t  *buffer,
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
