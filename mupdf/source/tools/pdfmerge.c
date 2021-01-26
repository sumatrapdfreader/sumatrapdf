/*
 * PDF merge tool: Tool for merging pdf content.
 *
 * Simple test bed to work with merging pages from multiple PDFs into a single PDF.
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdlib.h>
#include <stdio.h>

static int usage(void)
{
	fprintf(stderr,
		"usage: mutool merge [-o output.pdf] [-O options] input.pdf [pages] [input2.pdf] [pages2] ...\n"
		"\t-o -\tname of PDF file to create\n"
		"\t-O -\tcomma separated list of output options\n"
		"\tinput.pdf\tname of input file from which to copy pages\n"
		"\tpages\tcomma separated list of page numbers and ranges\n\n"
		);
	fputs(fz_pdf_write_options_usage, stderr);
	return 1;
}

static fz_context *ctx = NULL;
static pdf_document *doc_des = NULL;
static pdf_document *doc_src = NULL;

static void page_merge(int page_from, int page_to, pdf_graft_map *graft_map)
{
	pdf_graft_mapped_page(ctx, graft_map, page_to - 1, doc_src, page_from - 1);
}

static void merge_range(const char *range)
{
	int start, end, i, count;
	pdf_graft_map *graft_map;

	count = pdf_count_pages(ctx, doc_src);
	graft_map = pdf_new_graft_map(ctx, doc_des);

	fz_try(ctx)
	{
		while ((range = fz_parse_page_range(ctx, range, &start, &end, count)))
		{
			if (start < end)
				for (i = start; i <= end; ++i)
					page_merge(i, -1, graft_map);
			else
				for (i = start; i >= end; --i)
					page_merge(i, -1, graft_map);
		}
	}
	fz_always(ctx)
	{
		pdf_drop_graft_map(ctx, graft_map);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

int pdfmerge_main(int argc, char **argv)
{
	pdf_write_options opts = pdf_default_write_options;
	char *output = "out.pdf";
	char *flags = "";
	char *input;
	int c;

	while ((c = fz_getopt(argc, argv, "o:O:")) != -1)
	{
		switch (c)
		{
		case 'o': output = fz_optarg; break;
		case 'O': flags = fz_optarg; break;
		default: return usage();
		}
	}

	if (fz_optind == argc)
		return usage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "error: Cannot initialize MuPDF context.\n");
		exit(1);
	}

	pdf_parse_write_options(ctx, &opts, flags);

	fz_try(ctx)
	{
		doc_des = pdf_create_document(ctx);
	}
	fz_catch(ctx)
	{
		fprintf(stderr, "error: Cannot create destination document.\n");
		fz_flush_warnings(ctx);
		fz_drop_context(ctx);
		exit(1);
	}

	/* Step through the source files */
	while (fz_optind < argc)
	{
		doc_src = NULL;
		input = argv[fz_optind++];

		fz_try(ctx)
		{
		doc_src = pdf_open_document(ctx, input);
			if (fz_optind == argc || !fz_is_page_range(ctx, argv[fz_optind]))
				merge_range("1-N");
			else
				merge_range(argv[fz_optind++]);
		}
		fz_always(ctx)
			pdf_drop_document(ctx, doc_src);
		fz_catch(ctx)
			fprintf(stderr, "error: Cannot merge document '%s'.\n", input);
	}

	if (fz_optind == argc)
	{
		fz_try(ctx)
			pdf_save_document(ctx, doc_des, output, &opts);
		fz_catch(ctx)
			fprintf(stderr, "error: Cannot save output file: '%s'.\n", output);
	}

	pdf_drop_document(ctx, doc_des);
	fz_flush_warnings(ctx);
	fz_drop_context(ctx);
	return 0;
}
