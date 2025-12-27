// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#include "mupdf/fitz.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int usage(void)
{
	fprintf(stderr,
		"usage: mutool trace [options] file [pages]\n"
		"\t-p -\tpassword\n"
		"\n"
		"\t-b -\tuse named page box (MediaBox, CropBox, BleedBox, TrimBox, or ArtBox)\n"
		"\n"
		"\t-W -\tpage width for EPUB layout\n"
		"\t-H -\tpage height for EPUB layout\n"
		"\t-S -\tfont size for EPUB layout\n"
		"\t-U -\tfile name of user stylesheet for EPUB layout\n"
		"\t-X\tdisable document styles for EPUB layout\n"
		"\n"
		"\t-d\tuse display list\n"
		"\n"
		"\tpages\tcomma separated list of page numbers and ranges\n"
		);
	return 1;
}

static float layout_w = FZ_DEFAULT_LAYOUT_W;
static float layout_h = FZ_DEFAULT_LAYOUT_H;
static float layout_em = FZ_DEFAULT_LAYOUT_EM;
static char *layout_css = NULL;
static int layout_use_doc_css = 1;
static int page_box = FZ_CROP_BOX;

static int use_display_list = 0;

static void runpage(fz_context *ctx, fz_document *doc, int number)
{
	fz_page *page = NULL;
	fz_display_list *list = NULL;
	fz_device *dev = NULL;
	fz_rect mediabox;

	fz_var(page);
	fz_var(list);
	fz_var(dev);
	fz_try(ctx)
	{
		page = fz_load_page(ctx, doc, number - 1);
		mediabox = fz_bound_page_box(ctx, page, page_box);
		printf("<page number=\"%d\" mediabox=\"%g %g %g %g\">\n",
				number, mediabox.x0, mediabox.y0, mediabox.x1, mediabox.y1);
		dev = fz_new_trace_device(ctx, fz_stdout(ctx));
		if (use_display_list)
		{
			list = fz_new_display_list_from_page(ctx, page);
			fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
		}
		else
		{
			fz_run_page(ctx, page, dev, fz_identity, NULL);
		}
		printf("</page>\n");
	}
	fz_always(ctx)
	{
		fz_drop_display_list(ctx, list);
		fz_drop_page(ctx, page);
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void runrange(fz_context *ctx, fz_document *doc, int count, const char *range)
{
	int start, end, i;

	while ((range = fz_parse_page_range(ctx, range, &start, &end, count)))
	{
		if (start < end)
			for (i = start; i <= end; ++i)
				runpage(ctx, doc, i);
		else
			for (i = start; i >= end; --i)
				runpage(ctx, doc, i);
	}
}

int mutrace_main(int argc, char **argv)
{
	fz_context *ctx;
	fz_document *doc = NULL;
	char *password = "";
	int i, c, count;

	while ((c = fz_getopt(argc, argv, "p:b:W:H:S:U:Xd")) != -1)
	{
		switch (c)
		{
		default: return usage();
		case 'p': password = fz_optarg; break;

		case 'W': layout_w = fz_atof(fz_optarg); break;
		case 'H': layout_h = fz_atof(fz_optarg); break;
		case 'S': layout_em = fz_atof(fz_optarg); break;
		case 'U': layout_css = fz_optarg; break;
		case 'X': layout_use_doc_css = 0; break;

		case 'd': use_display_list = 1; break;

		case 'b':
			page_box = fz_box_type_from_string(fz_optarg);
			if (page_box == FZ_UNKNOWN_BOX)
			{
				fprintf(stderr, "Invalid box type: %s\n", fz_optarg);
				return 1;
			}
			break;
		}
	}

	if (fz_optind == argc)
		return usage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return EXIT_FAILURE;
	}

	fz_try(ctx)
	{
		fz_register_document_handlers(ctx);
		if (layout_css)
			fz_load_user_css(ctx, layout_css);
		fz_set_use_document_css(ctx, layout_use_doc_css);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot initialize mupdf\n");
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	fz_var(doc);
	fz_try(ctx)
	{
		printf("<?xml version=\"1.0\"?>\n");
		for (i = fz_optind; i < argc; ++i)
		{
			doc = fz_open_document(ctx, argv[i]);
			if (fz_needs_password(ctx, doc))
				if (!fz_authenticate_password(ctx, doc, password))
					fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot authenticate password: %s", argv[i]);
			fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);
			printf("<document filename=\"%s\">\n", argv[i]);
			count = fz_count_pages(ctx, doc);
			if (i+1 < argc && fz_is_page_range(ctx, argv[i+1]))
				runrange(ctx, doc, count, argv[++i]);
			else
				runrange(ctx, doc, count, "1-N");
			printf("</document>\n");
			fz_drop_document(ctx, doc);
			doc = NULL;
		}
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot run document\n");
		fz_drop_document(ctx, doc);
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	fz_drop_context(ctx);
	return EXIT_SUCCESS;
}
