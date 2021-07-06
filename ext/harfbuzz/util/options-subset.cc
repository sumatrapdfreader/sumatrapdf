/*
 * Copyright © 2019  Google, Inc.
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
 * Google Author(s): Garret Rieger
 */

#include "options.hh"

#include "hb-subset-input.hh"

static gboolean
parse_gids (const char *name G_GNUC_UNUSED,
	    const char *arg,
	    gpointer    data,
	    GError    **error G_GNUC_UNUSED)
{
  subset_options_t *subset_opts = (subset_options_t *) data;
  hb_set_t *gids = subset_opts->input->glyphs;

  char *s = (char *) arg;
  char *p;

  while (s && *s)
  {
    while (*s && strchr (", ", *s))
      s++;
    if (!*s)
      break;

    errno = 0;
    hb_codepoint_t start_code = strtoul (s, &p, 10);
    if (s[0] == '-' || errno || s == p)
    {
      hb_set_destroy (gids);
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Failed parsing gids values at: '%s'", s);
      return false;
    }

    if (p && p[0] == '-') //gid ranges
    {
      s = ++p;
      hb_codepoint_t end_code = strtoul (s, &p, 10);
      if (s[0] == '-' || errno || s == p)
      {
	hb_set_destroy (gids);
	g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		     "Failed parsing gids values at: '%s'", s);
	return false;
      }

      if (end_code < start_code)
      {
	hb_set_destroy (gids);
	g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		     "Invalid gids range value %u-%u", start_code, end_code);
	return false;
      }
      hb_set_add_range (gids, start_code, end_code);
    }
    else
    {
      hb_set_add (gids, start_code);
    }
    s = p;
  }

  return true;
}

static gboolean
parse_nameids (const char *name,
	       const char *arg,
	       gpointer    data,
	       GError    **error G_GNUC_UNUSED)
{
  subset_options_t *subset_opts = (subset_options_t *) data;
  hb_set_t *name_ids = subset_opts->input->name_ids;

  char last_name_char = name[strlen (name) - 1];

  if (last_name_char != '+' && last_name_char != '-')
    hb_set_clear (name_ids);

  if (0 == strcmp (arg, "*"))
  {
    if (last_name_char == '-')
      hb_set_del_range (name_ids, 0, 0x7FFF);
    else
      hb_set_add_range (name_ids, 0, 0x7FFF);
    return true;
  }

  char *s = (char *) arg;
  char *p;

  while (s && *s)
  {
    while (*s && strchr (", ", *s))
      s++;
    if (!*s)
      break;

    errno = 0;
    hb_codepoint_t u = strtoul (s, &p, 10);
    if (errno || s == p)
    {
      hb_set_destroy (name_ids);
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Failed parsing nameID values at: '%s'", s);
      return false;
    }

    if (last_name_char != '-')
    {
      hb_set_add (name_ids, u);
    } else {
      hb_set_del (name_ids, u);
    }

    s = p;
  }

  return true;
}

static gboolean
parse_name_languages (const char *name,
		      const char *arg,
		      gpointer    data,
		      GError    **error G_GNUC_UNUSED)
{
  subset_options_t *subset_opts = (subset_options_t *) data;
  hb_set_t *name_languages = subset_opts->input->name_languages;

  char last_name_char = name[strlen (name) - 1];

  if (last_name_char != '+' && last_name_char != '-')
    hb_set_clear (name_languages);

  if (0 == strcmp (arg, "*"))
  {
    if (last_name_char == '-')
      hb_set_del_range (name_languages, 0, 0x5FFF);
    else
      hb_set_add_range (name_languages, 0, 0x5FFF);
    return true;
  }

  char *s = (char *) arg;
  char *p;

  while (s && *s)
  {
    while (*s && strchr (", ", *s))
      s++;
    if (!*s)
      break;

    errno = 0;
    hb_codepoint_t u = strtoul (s, &p, 10);
    if (errno || s == p)
    {
      hb_set_destroy (name_languages);
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Failed parsing name_languages values at: '%s'", s);
      return false;
    }

    if (last_name_char != '-')
    {
      hb_set_add (name_languages, u);
    } else {
      hb_set_del (name_languages, u);
    }

    s = p;
  }

  return true;
}

static gboolean
parse_drop_tables (const char *name,
		   const char *arg,
		   gpointer    data,
		   GError    **error G_GNUC_UNUSED)
{
  subset_options_t *subset_opts = (subset_options_t *) data;
  hb_set_t *drop_tables = subset_opts->input->drop_tables;

  char last_name_char = name[strlen (name) - 1];

  if (last_name_char != '+' && last_name_char != '-')
    hb_set_clear (drop_tables);

  char *s = strtok((char *) arg, ", ");
  while (s)
  {
    if (strlen (s) > 4) // Table tags are at most 4 bytes.
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_BAD_VALUE,
		   "Failed parsing table tag values at: '%s'", s);
      return false;
    }

    hb_tag_t tag = hb_tag_from_string (s, strlen (s));

    if (last_name_char != '-')
      hb_set_add (drop_tables, tag);
    else
      hb_set_del (drop_tables, tag);

    s = strtok(nullptr, ", ");
  }

  return true;
}

void
subset_options_t::add_options (option_parser_t *parser)
{
  GOptionEntry entries[] =
  {
    {"no-hinting", 0, 0, G_OPTION_ARG_NONE,  &this->input->drop_hints,   "Whether to drop hints",   nullptr},
    {"retain-gids", 0, 0, G_OPTION_ARG_NONE,  &this->input->retain_gids,   "If set don't renumber glyph ids in the subset.",   nullptr},
    {"gids", 0, 0, G_OPTION_ARG_CALLBACK,  (gpointer) &parse_gids,  "Specify glyph IDs or ranges to include in the subset", "list of comma/whitespace-separated int numbers or ranges"},
    {"desubroutinize", 0, 0, G_OPTION_ARG_NONE,  &this->input->desubroutinize,   "Remove CFF/CFF2 use of subroutines",   nullptr},
    {"name-IDs", 0, 0, G_OPTION_ARG_CALLBACK,  (gpointer) &parse_nameids,  "Subset specified nameids", "list of int numbers"},
    {"name-legacy", 0, 0, G_OPTION_ARG_NONE,  &this->input->name_legacy,   "Keep legacy (non-Unicode) 'name' table entries",   nullptr},
    {"name-languages", 0, 0, G_OPTION_ARG_CALLBACK,  (gpointer) &parse_name_languages,  "Subset nameRecords with specified language IDs", "list of int numbers"},
    {"drop-tables", 0, 0, G_OPTION_ARG_CALLBACK,  (gpointer) &parse_drop_tables,  "Drop the specified tables.", "list of string table tags."},
    {"drop-tables+", 0, 0, G_OPTION_ARG_CALLBACK,  (gpointer) &parse_drop_tables,  "Drop the specified tables.", "list of string table tags."},
    {"drop-tables-", 0, 0, G_OPTION_ARG_CALLBACK,  (gpointer) &parse_drop_tables,  "Drop the specified tables.", "list of string table tags."},

    {nullptr}
  };
  parser->add_group (entries,
	 "subset",
	 "Subset options:",
	 "Options subsetting",
	 this);
}
