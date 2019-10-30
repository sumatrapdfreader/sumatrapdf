/*
 * Copyright © 2002 Keith Packard
 * Copyright © 2014  Google, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Google Author(s): Behdad Esfahbod
 */

#define HAVE_GETOPT_LONG 1 /* XXX */

#include "hb-fc.h"

#include <fontconfig/fontconfig.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#ifdef linux
#define HAVE_GETOPT_LONG 1
#endif
#define HAVE_GETOPT 1
#endif

#ifndef HAVE_GETOPT
#define HAVE_GETOPT 0
#endif
#ifndef HAVE_GETOPT_LONG
#define HAVE_GETOPT_LONG 0
#endif

#if HAVE_GETOPT_LONG
#undef  _GNU_SOURCE
#define _GNU_SOURCE
#include <getopt.h>
const struct option longopts[] = {
    {"verbose", 0, 0, 'v'},
    {"format", 1, 0, 'f'},
    {"quiet", 0, 0, 'q'},
    {"version", 0, 0, 'V'},
    {"help", 0, 0, 'h'},
    {NULL,0,0,0},
};
#else
#if HAVE_GETOPT
extern char *optarg;
extern int optind, opterr, optopt;
#endif
#endif

static void
usage (char *program, int error)
{
    FILE *file = error ? stderr : stdout;
#if HAVE_GETOPT_LONG
    fprintf (file, "usage: %s [-vqVh] [-f FORMAT] [--verbose] [--format=FORMAT] [--quiet] [--version] [--help] text [pattern] {element ...} \n",
	     program);
#else
    fprintf (file, "usage: %s [-vqVh] [-f FORMAT] text [pattern] {element ...} \n",
	     program);
#endif
    fprintf (file, "List fonts matching [pattern] that can render [text]\n");
    fprintf (file, "\n");
#if HAVE_GETOPT_LONG
    fprintf (file, "  -v, --verbose        display entire font pattern verbosely\n");
    fprintf (file, "  -f, --format=FORMAT  use the given output format\n");
    fprintf (file, "  -q, --quiet          suppress all normal output, exit 1 if no fonts matched\n");
    fprintf (file, "  -V, --version        display font config version and exit\n");
    fprintf (file, "  -h, --help           display this help and exit\n");
#else
    fprintf (file, "  -v         (verbose) display entire font pattern verbosely\n");
    fprintf (file, "  -f FORMAT  (format)  use the given output format\n");
    fprintf (file, "  -q,        (quiet)   suppress all normal output, exit 1 if no fonts matched\n");
    fprintf (file, "  -V         (version) display HarfBuzz version and exit\n");
    fprintf (file, "  -h         (help)    display this help and exit\n");
#endif
    exit (error);
}

int
main (int argc, char **argv)
{
    int			verbose = 0;
    int			quiet = 0;
    const FcChar8	*format = NULL;
    int			nfont = 0;
    int			i;
    FcObjectSet		*os = 0;
    FcFontSet		*fs;
    FcPattern		*pat;
    const char		*text;
#if HAVE_GETOPT_LONG || HAVE_GETOPT
    int			c;

#if HAVE_GETOPT_LONG
    while ((c = getopt_long (argc, argv, "vf:qVh", longopts, NULL)) != -1)
#else
    while ((c = getopt (argc, argv, "vf:qVh")) != -1)
#endif
    {
	switch (c) {
	case 'v':
	    verbose = 1;
	    break;
	case 'f':
	    format = (FcChar8 *) strdup (optarg);
	    break;
	case 'q':
	    quiet = 1;
	    break;
	case 'V':
	    fprintf (stderr, "fontconfig version %d.%d.%d\n",
		     FC_MAJOR, FC_MINOR, FC_REVISION);
	    exit (0);
	case 'h':
	    usage (argv[0], 0);
	default:
	    usage (argv[0], 1);
	}
    }
    i = optind;
#else
    i = 1;
#endif

    if (!argv[i])
	usage (argv[0], 1);

    text = argv[i];
    i++;

    if (argv[i])
    {
	pat = FcNameParse ((FcChar8 *) argv[i]);
	if (!pat)
	{
	    fputs ("Unable to parse the pattern\n", stderr);
	    return 1;
	}
	while (argv[++i])
	{
	    if (!os)
		os = FcObjectSetCreate ();
	    FcObjectSetAdd (os, argv[i]);
	}
    }
    else
	pat = FcPatternCreate ();
    if (quiet && !os)
	os = FcObjectSetCreate ();
    if (!verbose && !format && !os)
	os = FcObjectSetBuild (FC_FAMILY, FC_STYLE, FC_FILE, (char *) 0);
    FcObjectSetAdd (os, FC_CHARSET);
    if (!format)
        format = (const FcChar8 *) "%{=fclist}\n";
    fs = FcFontList (0, pat, os);
    if (os)
	FcObjectSetDestroy (os);
    if (pat)
	FcPatternDestroy (pat);

    if (!quiet && fs)
    {
	int	j;

	for (j = 0; j < fs->nfont; j++)
	{
	    hb_font_t *font = hb_fc_font_create (fs->fonts[j]);
	    hb_bool_t can_render = hb_fc_can_render (font, text);
	    hb_font_destroy (font);

	    if (!can_render)
		continue;

	    FcPatternDel (fs->fonts[j], FC_CHARSET);

	    if (verbose)
	    {
		FcPatternPrint (fs->fonts[j]);
	    }
	    else
	    {
	        FcChar8 *s;

		s = FcPatternFormat (fs->fonts[j], format);
		if (s)
		{
		    printf ("%s", s);
		    FcStrFree (s);
		}
	    }
	}
    }

    if (fs) {
	nfont = fs->nfont;
	FcFontSetDestroy (fs);
    }

    FcFini ();

    return quiet ? (nfont == 0 ? 1 : 0) : 0;
}
