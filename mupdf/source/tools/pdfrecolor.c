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

/* PDF recoloring tool. */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
	pdf_recolor_options ropts = { 0 };
	int n, i, c;
	char *infile = NULL;
	char *outputfile = "out.pdf";
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
		case 'o': outputfile = fz_optpath(fz_optarg); break;
		case 'r': remove_oi = 1; break;
		}
	}

	if (fz_optind == argc || !outputfile)
		return usage();

	infile = argv[fz_optind];

	if (colorspace == NULL || !strcmp(colorspace, "gray"))
		ropts.num_comp = 1;
	else if (!strcmp(colorspace, "rgb"))
		ropts.num_comp = 3;
	else if (!strcmp(colorspace, "cmyk"))
		ropts.num_comp = 4;
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
	opts.do_use_objstms = 1;
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

	fz_var(doc);

	fz_try(ctx)
	{
		/* Load the input document. */
		doc = fz_open_document(ctx, infile);

		/* Get a PDF specific pointer, and count the pages. */
		pdf = pdf_document_from_fz_document(ctx, doc);
		n = fz_count_pages(ctx, doc);

		for (i = 0; i < n; i++)
			pdf_recolor_page(ctx, pdf, i, &ropts);

		if (remove_oi)
			pdf_remove_output_intents(ctx, pdf);

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
