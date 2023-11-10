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

/* PDF recoloring tool. */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static fz_colorspace *outcs = NULL;
static pdf_obj *outcs_obj = NULL;

static void
color_rewrite(fz_context *ctx, void *opaque, pdf_obj **cs_obj, int *n, float color[FZ_MAX_COLORS])
{
	fz_colorspace *cs;
	float cols[4] = { 0 };

	if (pdf_name_eq(ctx, *cs_obj, PDF_NAME(Pattern)))
		return;
	if (pdf_name_eq(ctx, pdf_dict_get(ctx, *cs_obj, PDF_NAME(Type)), PDF_NAME(Pattern)))
		return;

	if (*n != 0)
	{
		cs = pdf_load_colorspace(ctx, *cs_obj);

		fz_try(ctx)
		{
			fz_convert_color(ctx, cs, color, outcs, cols, NULL, fz_default_color_params);
		}
		fz_always(ctx)
			fz_drop_colorspace(ctx, cs);
		fz_catch(ctx)
			fz_rethrow(ctx);
		*n = outcs->n;
	}

	pdf_drop_obj(ctx, *cs_obj);
	*cs_obj = outcs_obj;
	memcpy(color, cols, sizeof(color[0])*4);
}

static void
image_rewrite(fz_context *ctx, void *opaque, fz_image **image, fz_matrix ctm, pdf_obj *im_obj)
{
	fz_image *orig = *image;
	fz_pixmap *pix;
	fz_colorspace* dst_cs;

	dst_cs = outcs;

	pix = fz_get_unscaled_pixmap_from_image(ctx, orig);

	if (pix->colorspace != dst_cs)
	{
		fz_pixmap *pix2 = fz_convert_pixmap(ctx, pix, dst_cs, NULL, NULL, fz_default_color_params, 1);
		fz_drop_pixmap(ctx, pix);
		pix = pix2;
	}

	*image = fz_new_image_from_pixmap(ctx, pix, orig->mask);
	fz_drop_pixmap(ctx, pix);
	fz_drop_image(ctx, orig);
}

static void
vertex_rewrite(fz_context *ctx, void *opaque, fz_colorspace *dst_cs, float *d, fz_colorspace *src_cs, const float *s)
{
	fz_convert_color(ctx, src_cs, s, outcs, d, NULL, fz_default_color_params);
}

static pdf_recolor_vertex *
shade_rewrite(fz_context *ctx, void *opaque, fz_colorspace *src_cs, fz_colorspace **dst_cs)
{
	*dst_cs = outcs;

	return vertex_rewrite;
}

static void
rewrite_page_streams(fz_context *ctx, pdf_document *doc, int page_num)
{
	pdf_page *page = pdf_load_page(ctx, doc, page_num);
	pdf_filter_options options = { 0 };
	pdf_filter_factory list[2] = { 0 };
	pdf_color_filter_options copts = { 0 };
	pdf_annot *annot;

	copts.opaque = NULL;
	copts.color_rewrite = color_rewrite;
	copts.image_rewrite = image_rewrite;
	copts.shade_rewrite = shade_rewrite;
	options.filters = list;
	options.recurse = 1;
	list[0].filter = pdf_new_color_filter;
	list[0].options = &copts;

	fz_try(ctx)
	{
		pdf_filter_page_contents(ctx, doc, page, &options);

		for (annot = pdf_first_annot(ctx, page); annot != NULL; annot = pdf_next_annot(ctx, annot))
			pdf_filter_annot_contents(ctx, doc, annot, &options);
	}
	fz_always(ctx)
		fz_drop_page(ctx, &page->super);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static int
usage(void)
{
	fprintf(stderr, "usage: mutool recolor [options] <input filename>\n");
	fprintf(stderr, "\t-c -\tOutput colorspace (gray(default), rgb, cmyk)\n");
	fprintf(stderr, "\t-r\tRemove OutputIntent(s)\n");
	fprintf(stderr, "\t-o -\tOutput file\n");
	return 1;
}

int pdfrecolor_main(int argc, char **argv)
{
	fz_context *ctx = NULL;
	pdf_document *pdf = NULL;
	fz_document *doc = NULL;
	pdf_write_options opts = pdf_default_write_options;
	int n, i, c;
	char *infile = NULL;
	char *outputfile = NULL;
	int code = EXIT_SUCCESS;
	const char *colorspace = NULL;
	int remove_oi = 0;

	while ((c = fz_getopt(argc, argv, "c:o:r")) != -1)
	{
		switch (c)
		{
		default: return usage();

		// color convert
		case 'c': colorspace = fz_optarg; break;
		case 'o': outputfile = fz_optarg; break;
		case 'r': remove_oi = 1; break;
		}
	}

	if (fz_optind == argc || !outputfile)
		return usage();

	infile = argv[fz_optind];

	if (colorspace == NULL || !strcmp(colorspace, "gray"))
		colorspace = "gray";
	else if (!strcmp(colorspace, "rgb"))
	{}
	else if (!strcmp(colorspace, "cmyk"))
	{}
	else
	{
		fprintf(stderr, "Unknown colorspace\n");
		return usage();
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

	if (!strcmp(colorspace, "gray"))
	{
		outcs = fz_device_gray(ctx);
		outcs_obj = PDF_NAME(DeviceGray);
	}
	else if (!strcmp(colorspace, "rgb"))
	{
		outcs = fz_device_rgb(ctx);
		outcs_obj = PDF_NAME(DeviceRGB);
	}
	else if (!strcmp(colorspace, "cmyk"))
	{
		outcs = fz_device_cmyk(ctx);
		outcs_obj = PDF_NAME(DeviceCMYK);
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
			rewrite_page_streams(ctx, pdf, i);

		if (remove_oi)
			pdf_dict_del(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, pdf), PDF_NAME(Root)), PDF_NAME(OutputIntents));

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
