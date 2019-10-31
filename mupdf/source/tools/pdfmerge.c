/*
 * PDF merge tool: Tool for merging pdf content.
 *
 * Simple test bed to work with merging pages from multiple PDFs into a single PDF.
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdlib.h>
#include <stdio.h>

static void usage(void)
{
	fprintf(stderr,
		"usage: mutool merge [-o output.pdf] [-O options] input.pdf [pages] [input2.pdf] [pages2] ...\n"
		"\t-o -\tname of PDF file to create\n"
		"\t-O -\tcomma separated list of output options\n"
		"\tinput.pdf\tname of input file from which to copy pages\n"
		"\tpages\tcomma separated list of page numbers and ranges\n\n"
		);
	fputs(fz_pdf_write_options_usage, stderr);
	exit(1);
}

static fz_context *ctx = NULL;
static pdf_document *doc_des = NULL;
static pdf_document *doc_src = NULL;

static void page_merge(int page_from, int page_to, pdf_graft_map *graft_map)
{
	pdf_obj *page_ref;
	pdf_obj *page_dict = NULL;
	pdf_obj *obj;
	pdf_obj *ref = NULL;
	int i;

	/* Copy as few key/value pairs as we can. Do not include items that reference other pages. */
	static pdf_obj * const copy_list[] = {
		PDF_NAME(Contents),
		PDF_NAME(Resources),
		PDF_NAME(MediaBox),
		PDF_NAME(CropBox),
		PDF_NAME(BleedBox),
		PDF_NAME(TrimBox),
		PDF_NAME(ArtBox),
		PDF_NAME(Rotate),
		PDF_NAME(UserUnit)
	};

	fz_var(ref);
	fz_var(page_dict);

	fz_try(ctx)
	{
		page_ref = pdf_lookup_page_obj(ctx, doc_src, page_from - 1);
		pdf_flatten_inheritable_page_items(ctx, page_ref);

		/* Make a new page object dictionary to hold the items we copy from the source page. */
		page_dict = pdf_new_dict(ctx, doc_des, 4);

		pdf_dict_put(ctx, page_dict, PDF_NAME(Type), PDF_NAME(Page));

		for (i = 0; i < (int)nelem(copy_list); i++)
		{
			obj = pdf_dict_get(ctx, page_ref, copy_list[i]);
			if (obj != NULL)
				pdf_dict_put_drop(ctx, page_dict, copy_list[i], pdf_graft_mapped_object(ctx, graft_map, obj));
		}

		/* Add the page object to the destination document. */
		ref = pdf_add_object(ctx, doc_des, page_dict);

		/* Insert it into the page tree. */
		pdf_insert_page(ctx, doc_des, page_to - 1, ref);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, page_dict);
		pdf_drop_obj(ctx, ref);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
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
		default: usage(); break;
		}
	}

	if (fz_optind == argc)
		usage();

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
		input = argv[fz_optind++];
		doc_src = pdf_open_document(ctx, input);

		fz_try(ctx)
		{
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
