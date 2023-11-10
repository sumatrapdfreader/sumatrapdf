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

/* PDF content trimming tool. */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct
{
	fz_rect cullbox;
	int exclude;
} culler_data_t;

static int
culler(fz_context *ctx, void *opaque, fz_rect r, fz_cull_type type)
{
	culler_data_t *cd = (culler_data_t *)opaque;

	r = fz_intersect_rect(r, cd->cullbox);
	if (cd->exclude)
	{
		if (!fz_is_empty_rect(r))
			return 1;
	}
	else
	{
		if (fz_is_empty_rect(r))
			return 1;
	}

	return 0;
}

static void
rewrite_page_streams(fz_context *ctx, pdf_document *doc, int page_num, fz_box_type box, float *margins, int exclude, int fallback)
{
	pdf_page *page = pdf_load_page(ctx, doc, page_num);
	pdf_filter_options options = { 0 };
	pdf_filter_factory list[2] = { 0 };
	pdf_sanitize_filter_options sopts = { 0 };
	pdf_annot *annot;
	culler_data_t cd;

	cd.exclude = exclude;
	sopts.opaque = &cd;
	sopts.culler = culler;
	options.filters = list;
	options.recurse = 1;
	list[0].filter = pdf_new_sanitize_filter;
	list[0].options = &sopts;

	fz_try(ctx)
	{
		switch (box)
		{
		default:
		case FZ_MEDIA_BOX:
			cd.cullbox = pdf_dict_get_rect(ctx, page->obj, PDF_NAME(MediaBox));
			break;
		case FZ_BLEED_BOX:
			cd.cullbox = pdf_dict_get_rect(ctx, page->obj, PDF_NAME(BleedBox));
			break;
		case FZ_CROP_BOX:
			cd.cullbox = pdf_dict_get_rect(ctx, page->obj, PDF_NAME(CropBox));
			break;
		case FZ_TRIM_BOX:
			cd.cullbox = pdf_dict_get_rect(ctx, page->obj, PDF_NAME(TrimBox));
			break;
		case FZ_ART_BOX:
			cd.cullbox = pdf_dict_get_rect(ctx, page->obj, PDF_NAME(ArtBox));
			break;
		}

		cd.cullbox.x0 += margins[3];
		cd.cullbox.y0 += margins[2];
		cd.cullbox.x1 -= margins[1];
		cd.cullbox.y1 -= margins[0];

		if (fz_is_empty_rect(cd.cullbox) && fallback && box != FZ_MEDIA_BOX)
		{
			fprintf(stderr, "Falling back to Mediabox for page %d\n", page_num);
			cd.cullbox = pdf_dict_get_rect(ctx, page->obj, PDF_NAME(MediaBox));
		}
		if (fz_is_empty_rect(cd.cullbox))
		{
			fprintf(stderr, "No box found for page %d\n", page_num);
			break;
		}

		pdf_filter_page_contents(ctx, doc, page, &options);

		for (annot = pdf_first_annot(ctx, page); annot != NULL; annot = pdf_next_annot(ctx, annot))
			pdf_filter_annot_contents(ctx, doc, annot, &options);
	}
	fz_always(ctx)
		fz_drop_page(ctx, &page->super);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static char *
skip_comma(char *s)
{
	while (isspace(*s))
		s++;
	if (*s == ',')
		s++;
	while (isspace(*s))
		s++;
	return s;
}

static void
read_margins(float *margin, char *arg)
{
	char *e;

	/* A single one reads for all margins. */
	margin[0] = fz_strtof(arg, &e);
	margin[1] = margin[2] = margin[3] = margin[0];
	e = skip_comma(e);
	if (*e == 0)
		return;
	/* 2 entries reads for V,H. */
	margin[1] = fz_strtof(e, &e);
	margin[3] = margin[1];
	e = skip_comma(e);
	if (*e == 0)
		return;
	/* 4 entries reads for T,R,B,L. */
	margin[2] = fz_strtof(e, &e);
	margin[3] = 0;
	e = skip_comma(e);
	if (*e == 0)
		return;
	margin[3] = fz_strtof(e, &e);
}

static int
usage(void)
{
	fprintf(stderr, "usage: mutool trim [options] <input filename>\n");
	fprintf(stderr, "\t-b -\tWhich box to trim to (MediaBox(default), CropBox, BleedBox, TrimBox, ArtBox)\n");
	fprintf(stderr, "\t-m -\tAdd margins to box (+ve for inwards, -ve outwards).\n");
	fprintf(stderr, "\t\t\t<All> or <V>,<H> or <T>,<R>,<B>,<L>\n");
	fprintf(stderr, "\t-e\tExclude contents of box, rather than include them\n");
	fprintf(stderr, "\t-f\tFallback to mediabox if specified box not available\n");
	fprintf(stderr, "\t-o -\tOutput file\n");
	return 1;
}

int pdftrim_main(int argc, char **argv)
{
	fz_context *ctx = NULL;
	pdf_document *pdf = NULL;
	fz_document *doc = NULL;
	pdf_write_options opts = pdf_default_write_options;
	int n, i;
	char *infile = NULL;
	char *outputfile = NULL;
	int code = EXIT_SUCCESS;
	int exclude = 0;
	const char *boxname = NULL;
	fz_box_type box = FZ_CROP_BOX;
	int fallback = 0;
	float margins[4] = { 0 };
	int c;

	while ((c = fz_getopt(argc, argv, "b:o:efm:")) != -1)
	{
		switch (c)
		{
		default: return usage();

		case 'b': boxname = fz_optarg; break;
		case 'o': outputfile = fz_optarg; break;
		case 'e': exclude = 1; break;
		case 'f': fallback = 1; break;
		case 'm': read_margins(margins, fz_optarg); break;
		}
	}

	if (fz_optind == argc)
		return usage();

	infile = argv[fz_optind];

	if (boxname)
	{
		box = fz_box_type_from_string(boxname);
		if (box == FZ_UNKNOWN_BOX)
		{
			fprintf(stderr, "Unknown box %s specified!\n", boxname);
			return 1;
		}
	}

	/* Set up the options for the file saving. */
#if 1
	opts.do_compress = 1;
	opts.do_compress_images = 1;
	opts.do_compress_fonts = 1;
	opts.do_garbage = 3;
#else
	opts.do_compress = 0;
	opts.do_pretty = 1;
	opts.do_compress = 0;
	opts.do_compress_images = 1;
	opts.do_compress_fonts = 0;
	opts.do_garbage = 0;
	opts.do_clean = 1;
#endif

	/* Create a MuPDF library context. */
	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "Could not create global context.\n");
		return EXIT_FAILURE;
	}

	/* Register the document handlers (only really need PDF, but this is
	 * the simplest way. */
	fz_register_document_handlers(ctx);

	fz_try(ctx)
	{
		/* Load the input document. */
		doc = fz_open_document(ctx, infile);

		/* Get a PDF specific pointer, and count the pages. */
		pdf = pdf_document_from_fz_document(ctx, doc);
		n = fz_count_pages(ctx, doc);

		for (i = 0; i < n; i++)
			rewrite_page_streams(ctx, pdf, i, box, margins, exclude, fallback);

		pdf_save_document(ctx, pdf, outputfile, &opts);
	}
	fz_always(ctx)
	{
		fz_drop_document(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		code = EXIT_FAILURE;
	}
	fz_drop_context(ctx);

	return code;
}
