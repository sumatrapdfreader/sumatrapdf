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

#ifndef OUTPUT_OPTIONS_HH
#define OUTPUT_OPTIONS_HH

#include "options.hh"

template <bool default_stdout = true>
struct output_options_t
{
  ~output_options_t ()
  {
    g_free (output_file);
    g_free (output_format);
    if (out_fp && out_fp != stdout)
      fclose (out_fp);
  }

  void add_options (option_parser_t *parser,
		    const char **supported_formats = nullptr)
  {
    const char *text = nullptr;

    if (supported_formats)
    {
      char *items = g_strjoinv ("/", const_cast<char **> (supported_formats));
      text = g_strdup_printf ("Set output format\n\n    Supported output formats are: %s", items);
      g_free (items);
      parser->free_later ((char *) text);
    }

    GOptionEntry entries[] =
    {
      {"output-file",   'o', 0, G_OPTION_ARG_STRING,	&this->output_file,		"Set output file-name (default: stdout)","filename"},
      {"output-format", 'O', supported_formats ? 0 : G_OPTION_FLAG_HIDDEN,
				G_OPTION_ARG_STRING,	&this->output_format,		text,					"format"},
      {nullptr}
    };
    parser->add_group (entries,
		       "output",
		       "Output destination & format options:",
		       "Options for the destination & form of the output",
		       this);
  }

  void post_parse (GError **error)
  {
    if (output_format)
      explicit_output_format = true;

    if (output_file && !output_format)
    {
      output_format = strrchr (output_file, '.');
      if (output_format)
      {
	  output_format++; /* skip the dot */
	  output_format = g_strdup (output_format);
      }
    }

    if (output_file && 0 != strcmp (output_file, "-"))
      out_fp = fopen (output_file, "wb");
    else
    {
      if (!default_stdout && !output_file)
      {
	g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		     "No output file was specified");
        return;
      }

#if defined(_WIN32) || defined(__CYGWIN__)
      setmode (fileno (stdout), O_BINARY);
#endif
      out_fp = stdout;
    }
    if (!out_fp)
    {
      g_set_error (error, G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
		   "Cannot open output file `%s': %s",
		   g_filename_display_name (output_file), strerror (errno));
      return;
    }
  }

  char *output_file = nullptr;
  char *output_format = nullptr;

  bool explicit_output_format = false;
  FILE *out_fp = nullptr;
};

#endif
