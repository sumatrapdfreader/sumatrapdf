/*
 * Copyright © 2021  Behdad Esfahbod
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
 */

#ifndef BATCH_HH
#define BATCH_HH

#include "options.hh"

typedef int (*main_func_t) (int argc, char **argv);

template <typename main_t, bool report_status=false>
int
batch_main (int argc, char **argv)
{
  if (argc == 2 && !strcmp (argv[1], "--batch"))
  {
    int ret = 0;
    char buf[4092];
    while (fgets (buf, sizeof (buf), stdin))
    {
      size_t l = strlen (buf);
      if (l && buf[l - 1] == '\n') buf[l - 1] = '\0';

      char *args[64];
      argc = 0;
      args[argc++] = argv[0];
      char *p = buf, *e;
      args[argc++] = p;
      while ((e = strchr (p, ';')) && argc < (int) ARRAY_LENGTH (args))
      {
	*e++ = '\0';
	while (*e == ';')
	  e++;
	args[argc++] = p = e;
      }

      int result = main_t () (argc, args);

      if (report_status)
	fprintf (stdout, result == 0 ? "success\n" : "failure\n");
      fflush (stdout);

      ret = MAX (ret, result);
    }
    return ret;
  }

  int ret = main_t () (argc, argv);
  if (report_status && ret != 0)
    fprintf (stdout, "error: Operation failed. Probably a bug. File github issue.\n");
  return ret;
}

#endif
