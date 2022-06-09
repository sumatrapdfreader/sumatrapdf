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

#ifndef OPTIONS_HH
#define OPTIONS_HH

#include "hb.hh"

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* for isatty() */
#endif
#if defined(_WIN32) || defined(__CYGWIN__)
#include <io.h> /* for setmode() under Windows */
#endif

#include <hb.h>
#include <hb-ot.h>
#include <glib.h>
#include <glib/gprintf.h>


static inline void fail (hb_bool_t suggest_help, const char *format, ...) G_GNUC_NORETURN G_GNUC_PRINTF (2, 3);

static inline void
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

struct option_parser_t
{
  option_parser_t (const char *parameter_string = nullptr)
  : context (g_option_context_new (parameter_string)),
    to_free (g_ptr_array_new ())
  {}

  static void _g_free_g_func (void *p, void * G_GNUC_UNUSED) { g_free (p); }

  ~option_parser_t ()
  {
    g_option_context_free (context);
    g_ptr_array_foreach (to_free, _g_free_g_func, nullptr);
    g_ptr_array_free (to_free, TRUE);
  }

  void add_options ();

  static void
  post_parse_ (void *thiz, GError **error) {}
  template <typename Type>
  static auto
  post_parse_ (Type *thiz, GError **error) -> decltype (thiz->post_parse (error))
  { thiz->post_parse (error); }
  template <typename Type>
  static gboolean
  post_parse (GOptionContext *context G_GNUC_UNUSED,
	      GOptionGroup *group G_GNUC_UNUSED,
	      gpointer data,
	      GError **error)
  {
    option_parser_t::post_parse_ (static_cast<Type *> (data), error);
    return !*error;
  }

  template <typename Type>
  void add_group (GOptionEntry   *entries,
		  const gchar    *name,
		  const gchar    *description,
		  const gchar    *help_description,
		  Type           *closure,
		  bool		  add_parse_hooks = true)
  {
    GOptionGroup *group = g_option_group_new (name, description, help_description,
					      static_cast<gpointer>(closure), nullptr);
    g_option_group_add_entries (group, entries);
    if (add_parse_hooks)
      g_option_group_set_parse_hooks (group, nullptr, post_parse<Type>);
    g_option_context_add_group (context, group);
  }

  template <typename Type>
  void add_main_group (GOptionEntry   *entries,
		       Type           *closure)
  {
    GOptionGroup *group = g_option_group_new (nullptr, nullptr, nullptr,
					      static_cast<gpointer>(closure), nullptr);
    g_option_group_add_entries (group, entries);
    /* https://gitlab.gnome.org/GNOME/glib/-/issues/2460 */
    //g_option_group_set_parse_hooks (group, nullptr, post_parse<Type>);
    g_option_context_set_main_group (context, group);
  }

  void set_summary (const char *summary)
  {
    g_option_context_set_summary (context, summary);
  }
  void set_description (const char *description)
  {
    g_option_context_set_description (context, description);
  }

  void free_later (char *p) {
    g_ptr_array_add (to_free, p);
  }

  bool parse (int *argc, char ***argv, bool ignore_error = false);

  GOptionContext *context;
  protected:
  GPtrArray *to_free;
};


static inline gchar *
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

inline void
option_parser_t::add_options ()
{
  GOptionEntry entries[] =
  {
    {"version",		0, G_OPTION_FLAG_NO_ARG,
			      G_OPTION_ARG_CALLBACK,	(gpointer) &show_version,	"Show version numbers",			nullptr},
    {nullptr}
  };
  g_option_context_add_main_entries (context, entries, nullptr);
}

inline bool
option_parser_t::parse (int *argc, char ***argv, bool ignore_error)
{
  setlocale (LC_ALL, "");

  GError *parse_error = nullptr;
  if (!g_option_context_parse (context, argc, argv, &parse_error))
  {
    if (parse_error)
    {
      if (!ignore_error)
	fail (true, "%s", parse_error->message);
      g_error_free (parse_error);
    }
    else
    {
      if (!ignore_error)
	fail (true, "Option parse error");
    }
    return false;
  }
  return true;
}


/* fallback implementation for scalbn()/scalbnf() for pre-2013 MSVC */
#if defined (_MSC_VER) && (_MSC_VER < 1800)

#ifndef FLT_RADIX
#define FLT_RADIX 2
#endif

__inline long double scalbn (long double x, int exp)
{
  return x * (pow ((long double) FLT_RADIX, exp));
}

__inline float scalbnf (float x, int exp)
{
  return x * (pow ((float) FLT_RADIX, exp));
}
#endif

#endif
