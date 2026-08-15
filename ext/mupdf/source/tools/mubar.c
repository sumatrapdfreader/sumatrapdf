// Copyright (C) 2025 Artifex Software, Inc.
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
 * mudraw -- command line tool for drawing and converting documents
 */

#include "mupdf/fitz.h"

#if FZ_ENABLE_BARCODE

#if FZ_ENABLE_PDF
#include "mupdf/pdf.h" /* for pdf output */
#endif

static int mubar_usage(void)
{
	int i, n, c;
	const char *s;
	fprintf(stderr,
		"usage to decode: mutool barcode -d [options] input [pages]\n"
		"\t-p -\tpassword for encrypted PDF files\n"
		"\t-o -\toutput file (default: stdout)\n"
		"\t-r -\trotation\n"
	);
	fprintf(stderr,
		"usage to create: mutool barcode -c [options] text\n"
		"\t-o -\toutput file (default: out.png)\n"
		"\t-q\tadd quiet zones\n"
		"\t-t\tadd human readable text (when possible)\n"
		"\t-e -\terror correction level (0-8)\n"
		"\t-s -\tsize of barcode image\n"
		"\t-F -\tbarcode format (default: qrcode)\n"
	);
	for (c = 0, i = FZ_BARCODE_NONE + 1; i < FZ_BARCODE__LIMIT; i++)
	{
		s = fz_string_from_barcode_type(i);
		n = (int)strlen(s);
		if (c + 2 + n > 78)
		{
			fprintf(stderr, ",\n\t\t%s", s);
			c = 8 + n;
		}
		else
		{
			if (c == 0)
			{
				fprintf(stderr, "\t\t%s", s);
				c += 8 + n;
			}
			else
			{
				fprintf(stderr, ", %s", s);
				c += n + 2;
			}
		}
	}
	fprintf(stderr, "\n");
	return EXIT_FAILURE;
}

int mubar_create(int argc, char **argv)
{
	fz_context *ctx;
	fz_pixmap *pixmap = NULL;
	int retval = EXIT_SUCCESS;

	const char *output = "out.png";
	const char *format = "png";

	fz_barcode_type bartype = FZ_BARCODE_QRCODE;
	int quiet = 0;
	int hrt = 0;
	int ec_level = 0;
	int size = 256;
	int c;

	while ((c = fz_getopt(argc, argv, "F:ce:o:qs:td:")) != -1)
	{
		switch (c)
		{
		case 'F':
			bartype = fz_barcode_type_from_string(fz_optarg);
			if (bartype == FZ_BARCODE_NONE)
				return mubar_usage();
			break;
		case 'c': break;
		case 'e': ec_level = fz_atoi(fz_optarg); break;
		case 'o': output = fz_optpath(fz_optarg); break;
		case 'q': quiet = 1; break;
		case 's': size = fz_atoi(fz_optarg); break;
		case 't': hrt = 1; break;
		default: return mubar_usage();
		}
	}

	if (fz_optind == argc)
		return mubar_usage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		return EXIT_FAILURE;
	}

	format = strrchr(output, '.');
	if (format == NULL)
		return mubar_usage();

	fz_var(pixmap);

	fz_try(ctx)
	{
		pixmap = fz_new_barcode_pixmap(ctx, bartype, argv[fz_optind], size, ec_level, quiet, hrt);

		if (!fz_strcasecmp(format, ".png"))
		{
			fz_save_pixmap_as_png(ctx, pixmap, output);
		}
		else if (!fz_strcasecmp(format, ".pdf"))
		{
			fz_pclm_options opts = { 0 };
			opts.compress = 1;
			opts.strip_height = pixmap->h;
			fz_save_pixmap_as_pclm(ctx, pixmap, output, 0, &opts);
		}
		else
		{
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid output format (must be PNG or PDF)\n");
		}
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, pixmap);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		retval = EXIT_FAILURE;
	}

	fz_drop_context(ctx);

	return retval;
}

static void mubar_decode_page(fz_context *ctx, fz_output *out, fz_document *doc, int page_no, int rotation)
{
	fz_barcode_type type;
	fz_page *page = fz_load_page(ctx, doc, page_no-1);
	char *text = NULL;

	fz_var(text);

	fz_try(ctx)
	{
		text = fz_decode_barcode_from_page(ctx, &type, page, fz_infinite_rect, rotation);
		if (text && type != FZ_BARCODE_NONE)
			fz_write_printf(ctx, out, "%s: %s\n", fz_string_from_barcode_type(type), text);
		else
			fz_write_string(ctx, out, "none\n");
	}
	fz_always(ctx)
	{
		fz_free(ctx, text);
		fz_drop_page(ctx, page);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

int mubar_decode(int argc, char **argv)
{
	fz_context *ctx;
	fz_document *doc = NULL;
	fz_output *out = NULL;
	int i, c, start, end, p, count;
	const char *range = NULL;
	int retval = EXIT_SUCCESS;

	const char *output = NULL;
	const char *password = NULL;
	int rotation = 0;

	while ((c = fz_getopt(argc, argv, "do:r:p:")) != -1)
	{
		switch (c)
		{
		case 'd': break;
		case 'o': output = fz_optarg; break;
		case 'r': rotation = fz_atof(fz_optarg); break;
		case 'p': password = fz_optarg; break;
		default: return mubar_usage();
		}
	}

	if (fz_optind == argc)
		return mubar_usage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		return EXIT_FAILURE;
	}

	fz_try(ctx)
		fz_register_document_handlers(ctx);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		fprintf(stderr, "cannot register document handlers\n");
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	fz_var(doc);
	fz_var(out);
	fz_try(ctx)
	{
		if (output)
			out = fz_new_output_with_path(ctx, output, 0);
		else
			out = fz_stdout(ctx);

		for (i = fz_optind; i < argc; ++i)
		{
			doc = fz_open_document(ctx, argv[i]);
			if (fz_needs_password(ctx, doc))
				if (!fz_authenticate_password(ctx, doc, password))
					fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot authenticate password: %s", argv[i]);
			count = fz_count_pages(ctx, doc);

			if (i+1 < argc && fz_is_page_range(ctx, argv[i+1]))
				range = argv[++i];
			else
				range = "1-N";

			while ((range = fz_parse_page_range(ctx, range, &start, &end, count)))
			{
				if (start < end)
					for (p = start; p <= end; ++p)
						mubar_decode_page(ctx, out, doc, p, rotation);
				else
					for (p = start; p >= end; --p)
						mubar_decode_page(ctx, out, doc, p, rotation);
			}

			fz_drop_document(ctx, doc);
			doc = NULL;
		}

		if (output)
			fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		if (output)
			fz_drop_output(ctx, out);
		fz_drop_document(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		retval = EXIT_FAILURE;
	}

	fz_drop_context(ctx);

	return retval;
}

int mubar_main(int argc, char **argv)
{
	if (argc > 2 && !strcmp(argv[1], "-c"))
		return mubar_create(argc, argv);
	if (argc > 2 && !strcmp(argv[1], "-d"))
		return mubar_decode(argc, argv);
	return mubar_usage();
}

#else

#include <stdio.h>

int mubar_main(int argc, char **argv)
{
	fprintf(stderr, "barcode support disabled\n");
	return 1;
}

#endif
