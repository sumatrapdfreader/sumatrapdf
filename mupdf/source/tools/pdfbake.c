// Copyright (C) 2004-2024 Artifex Software, Inc.
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
 * PDF baking tool: Bake interactive form and/or annotation content into static graphics.
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int usage(void)
{
	fprintf(stderr,
		"usage: mutool bake [options] input.pdf [output.pdf]\n"
		"\t-A\tkeep annotations\n"
		"\t-F\tkeep forms\n"
		"\t-O -\tcomma separated list of output options\n"
	);
	return 1;
}

int pdfbake_main(int argc, char **argv)
{
	fz_context *ctx;
	pdf_document *doc;
	pdf_write_options opts = pdf_default_write_options;
	int bake_annots = 1;
	int bake_widgets = 1;
	char *output = "out.pdf";
	char *flags = "garbage";
	char *input;
	int c;

	while ((c = fz_getopt(argc, argv, "AFO:")) != -1)
	{
		switch (c)
		{
		case 'A':
			bake_annots = 0;
			break;
		case 'F':
			bake_widgets = 0;
			break;
		case 'O':
			flags = fz_optarg;
			break;
		default:
			return usage();
		}
	}

	if (argc - fz_optind < 1)
		return usage();

	input = argv[fz_optind++];
	if (argc - fz_optind > 0)
		output = fz_optpath(argv[fz_optind++]);

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "error: Cannot initialize MuPDF context.\n");
		exit(1);
	}

	fz_try(ctx)
	{
		doc = pdf_open_document(ctx, input);

		pdf_bake_document(ctx, doc, bake_annots, bake_widgets);

		pdf_parse_write_options(ctx, &opts, flags);
		pdf_save_document(ctx, doc, output, &opts);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		return 1;
	}

	pdf_drop_document(ctx, doc);
	fz_flush_warnings(ctx);
	fz_drop_context(ctx);
	return 0;
}
