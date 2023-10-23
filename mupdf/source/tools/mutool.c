// Copyright (C) 2004-2023 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

/*
 * mutool -- swiss army knife of pdf manipulation tools
 */

#include "mupdf/fitz.h"

#include <string.h>
#include <stdio.h>

#ifdef _MSC_VER
#define main main_utf8
#endif

int muconvert_main(int argc, char *argv[]);
int mudraw_main(int argc, char *argv[]);
int mutrace_main(int argc, char *argv[]);
int murun_main(int argc, char *argv[]);

int pdfclean_main(int argc, char *argv[]);
int pdfextract_main(int argc, char *argv[]);
int pdfinfo_main(int argc, char *argv[]);
int pdfposter_main(int argc, char *argv[]);
int pdfshow_main(int argc, char *argv[]);
int pdfpages_main(int argc, char *argv[]);
int pdfcreate_main(int argc, char *argv[]);
int pdfmerge_main(int argc, char *argv[]);
int pdfsign_main(int argc, char *argv[]);
int pdfrecolor_main(int argc, char *argv[]);
int pdftrim_main(int argc, char *argv[]);

int cmapdump_main(int argc, char *argv[]);

static struct {
	int (*func)(int argc, char *argv[]);
	char *name;
	char *desc;
} tools[] = {
#if FZ_ENABLE_PDF
	{ pdfclean_main, "clean", "rewrite pdf file" },
#endif
	{ muconvert_main, "convert", "convert document" },
#if FZ_ENABLE_PDF
	{ pdfcreate_main, "create", "create pdf document" },
#endif
	{ mudraw_main, "draw", "convert document" },
	{ mutrace_main, "trace", "trace device calls" },
#if FZ_ENABLE_PDF
	{ pdfextract_main, "extract", "extract font and image resources" },
	{ pdfinfo_main, "info", "show information about pdf resources" },
	{ pdfmerge_main, "merge", "merge pages from multiple pdf sources into a new pdf" },
	{ pdfpages_main, "pages", "show information about pdf pages" },
	{ pdfposter_main, "poster", "split large page into many tiles" },
	{ pdfrecolor_main, "recolor", "Change colorspace of pdf document" },
	{ pdfsign_main, "sign", "manipulate PDF digital signatures" },
	{ pdftrim_main, "trim", "trim PDF page contents" },
#endif
#if FZ_ENABLE_JS
	{ murun_main, "run", "run javascript" },
#endif
#if FZ_ENABLE_PDF
	{ pdfshow_main, "show", "show internal pdf objects" },
#ifndef NDEBUG
	{ cmapdump_main, "cmapdump", "dump CMap resource as C source file" },
#endif
#endif
};

static int
namematch(const char *end, const char *start, const char *match)
{
	size_t len = strlen(match);
	return ((end-len >= start) && (strncmp(end-len, match, len) == 0));
}

#ifdef GPERF
#include "gperftools/profiler.h"

static int profiled_main(int argc, char **argv);

int main(int argc, char **argv)
{
	int ret;
	ProfilerStart("mutool.prof");
	ret = profiled_main(argc, argv);
	ProfilerStop();
	return ret;
}

static int profiled_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	char *start, *end;
	char buf[32];
	int i;

	if (argc == 0)
	{
		fprintf(stderr, "No command name found!\n");
		return 1;
	}

	/* Check argv[0] */

	if (argc > 0)
	{
		end = start = argv[0];
		while (*end)
			end++;
		if ((end-4 >= start) && (end[-4] == '.') && (end[-3] == 'e') && (end[-2] == 'x') && (end[-1] == 'e'))
			end = end-4;
		for (i = 0; i < (int)nelem(tools); i++)
		{
			strcpy(buf, "mupdf");
			strcat(buf, tools[i].name);
			if (namematch(end, start, buf) || namematch(end, start, buf+2))
				return tools[i].func(argc, argv);
			strcpy(buf, "mu");
			strcat(buf, tools[i].name);
			if (namematch(end, start, buf))
				return tools[i].func(argc, argv);
		}
	}

	/* Check argv[1] */

	if (argc > 1)
	{
		for (i = 0; i < (int)nelem(tools); i++)
			if (!strcmp(tools[i].name, argv[1]))
				return tools[i].func(argc - 1, argv + 1);
		if (!strcmp(argv[1], "-v"))
		{
			fprintf(stderr, "mutool version %s\n", FZ_VERSION);
			return 0;
		}
	}

	/* Print usage */

	fprintf(stderr, "mutool version %s\n", FZ_VERSION);
	fprintf(stderr, "usage: mutool <command> [options]\n");

	for (i = 0; i < (int)nelem(tools); i++)
		fprintf(stderr, "\t%s\t-- %s\n", tools[i].name, tools[i].desc);

	return 1;
}

#ifdef _MSC_VER
int wmain(int argc, wchar_t *wargv[])
{
	char **argv = fz_argv_from_wargv(argc, wargv);
	int ret = main(argc, argv);
	fz_free_argv(argc, argv);
	return ret;
}
#endif
