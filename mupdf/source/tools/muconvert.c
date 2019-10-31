/*
 * muconvert -- command line tool for converting documents
 */

#include "mupdf/fitz.h"

#include <stdlib.h>
#include <stdio.h>

/* input options */
static const char *password = "";
static int alphabits = 8;
static float layout_w = FZ_DEFAULT_LAYOUT_W;
static float layout_h = FZ_DEFAULT_LAYOUT_H;
static float layout_em = FZ_DEFAULT_LAYOUT_EM;
static char *layout_css = NULL;
static int layout_use_doc_css = 1;

/* output options */
static const char *output = NULL;
static const char *format = NULL;
static const char *options = "";

static fz_context *ctx;
static fz_document *doc;
static fz_document_writer *out;
static int count;

static void usage(void)
{
	fprintf(stderr,
		"mutool convert version " FZ_VERSION "\n"
		"Usage: mutool convert [options] file [pages]\n"
		"\t-p -\tpassword\n"
		"\n"
		"\t-A -\tnumber of bits of antialiasing (0 to 8)\n"
		"\t-W -\tpage width for EPUB layout\n"
		"\t-H -\tpage height for EPUB layout\n"
		"\t-S -\tfont size for EPUB layout\n"
		"\t-U -\tfile name of user stylesheet for EPUB layout\n"
		"\t-X\tdisable document styles for EPUB layout\n"
		"\n"
		"\t-o -\toutput file name (%%d for page number)\n"
		"\t-F -\toutput format (default inferred from output file name)\n"
		"\t\t\traster: cbz, png, pnm, pgm, ppm, pam, pbm, pkm.\n"
		"\t\t\tprint-raster: pcl, pclm, ps, pwg.\n"
		"\t\t\tvector: pdf, svg.\n"
		"\t\t\ttext: html, xhtml, text, stext.\n"
		"\t-O -\tcomma separated list of options for output format\n"
		"\n"
		"\tpages\tcomma separated list of page ranges (N=last page)\n"
		"\n"
		);
	fputs(fz_draw_options_usage, stderr);
	fputs(fz_pcl_write_options_usage, stderr);
	fputs(fz_pclm_write_options_usage, stderr);
	fputs(fz_pwg_write_options_usage, stderr);
	fputs(fz_stext_options_usage, stderr);
#if FZ_ENABLE_PDF
	fputs(fz_pdf_write_options_usage, stderr);
#endif
	fputs(fz_svg_write_options_usage, stderr);
	exit(1);
}

static void runpage(int number)
{
	fz_rect mediabox;
	fz_page *page;
	fz_device *dev = NULL;

	page = fz_load_page(ctx, doc, number - 1);

	fz_var(dev);

	fz_try(ctx)
	{
		mediabox = fz_bound_page(ctx, page);
		dev = fz_begin_page(ctx, out, mediabox);
		fz_run_page(ctx, page, dev, fz_identity, NULL);
		fz_end_page(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_page(ctx, page);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void runrange(const char *range)
{
	int start, end, i;

	while ((range = fz_parse_page_range(ctx, range, &start, &end, count)))
	{
		if (start < end)
			for (i = start; i <= end; ++i)
				runpage(i);
		else
			for (i = start; i >= end; --i)
				runpage(i);
	}
}

int muconvert_main(int argc, char **argv)
{
	int i, c;

	while ((c = fz_getopt(argc, argv, "p:A:W:H:S:U:Xo:F:O:")) != -1)
	{
		switch (c)
		{
		default: usage(); break;

		case 'p': password = fz_optarg; break;
		case 'A': alphabits = atoi(fz_optarg); break;
		case 'W': layout_w = fz_atof(fz_optarg); break;
		case 'H': layout_h = fz_atof(fz_optarg); break;
		case 'S': layout_em = fz_atof(fz_optarg); break;
		case 'U': layout_css = fz_optarg; break;
		case 'X': layout_use_doc_css = 0; break;

		case 'o': output = fz_optarg; break;
		case 'F': format = fz_optarg; break;
		case 'O': options = fz_optarg; break;
		}
	}

	if (fz_optind == argc || (!format && !output))
		usage();

	/* Create a context to hold the exception stack and various caches. */
	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return EXIT_FAILURE;
	}

	/* Register the default file types to handle. */
	fz_try(ctx)
		fz_register_document_handlers(ctx);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	fz_set_aa_level(ctx, alphabits);

	if (layout_css)
	{
		fz_buffer *buf = fz_read_file(ctx, layout_css);
		fz_set_user_css(ctx, fz_string_from_buffer(ctx, buf));
		fz_drop_buffer(ctx, buf);
	}

	fz_set_use_document_css(ctx, layout_use_doc_css);

	/* Open the output document. */
	fz_try(ctx)
		out = fz_new_document_writer(ctx, output, format, options);
	fz_catch(ctx)
	{
		fprintf(stderr, "cannot create document: %s\n", fz_caught_message(ctx));
		fz_drop_context(ctx);
		return EXIT_FAILURE;
	}

	for (i = fz_optind; i < argc; ++i)
	{
		doc = fz_open_document(ctx, argv[i]);
		if (fz_needs_password(ctx, doc))
			if (!fz_authenticate_password(ctx, doc, password))
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", argv[i]);
		fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);
		count = fz_count_pages(ctx, doc);

		if (i+1 < argc && fz_is_page_range(ctx, argv[i+1]))
			runrange(argv[++i]);
		else
			runrange("1-N");

		fz_drop_document(ctx, doc);
	}

	fz_close_document_writer(ctx, out);

	fz_drop_document_writer(ctx, out);
	fz_drop_context(ctx);
	return EXIT_SUCCESS;
}
