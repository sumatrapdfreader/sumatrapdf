/*
 * Information tool.
 * Print information about pages of a pdf.
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdlib.h>
#include <stdio.h>

static void
infousage(void)
{
	fprintf(stderr,
		"usage: mutool pages [options] file.pdf [pages]\n"
		"\t-p -\tpassword for decryption\n"
		"\tpages\tcomma separated list of page numbers and ranges\n"
		);
	exit(1);
}

static int
showbox(fz_context *ctx, fz_output *out, pdf_obj *page, char *text, pdf_obj *name)
{
	fz_rect bbox;
	pdf_obj *obj;
	int failed = 0;

	fz_try(ctx)
	{
		obj = pdf_dict_get(ctx, page, name);
		if (!pdf_is_array(ctx, obj))
			break;

		bbox = pdf_to_rect(ctx, obj);

		fz_write_printf(ctx, out, "<%s l=\"%g\" b=\"%g\" r=\"%g\" t=\"%g\" />\n", text, bbox.x0, bbox.y0, bbox.x1, bbox.y1);
	}
	fz_catch(ctx)
	{
		failed = 1;
	}

	return failed;
}

static int
shownum(fz_context *ctx, fz_output *out, pdf_obj *page, char *text, pdf_obj *name)
{
	pdf_obj *obj;
	int failed = 0;

	fz_try(ctx)
	{
		obj = pdf_dict_get(ctx, page, name);
		if (!pdf_is_number(ctx, obj))
			break;

		fz_write_printf(ctx, out, "<%s v=\"%g\" />\n", text, pdf_to_real(ctx, obj));
	}
	fz_catch(ctx)
	{
		failed = 1;
	}

	return failed;
}

static int
showpage(fz_context *ctx, pdf_document *doc, fz_output *out, int page)
{
	pdf_obj *pageref;
	int failed = 0;

	fz_write_printf(ctx, out, "<page pagenum=\"%d\">\n", page);
	fz_try(ctx)
	{
		pageref = pdf_lookup_page_obj(ctx, doc, page-1);
		if (!pageref)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot retrieve info from page %d", page);
	}
	fz_catch(ctx)
	{
		fz_write_printf(ctx, out, "Failed to gather information for page %d\n", page);
		failed = 1;
	}

	if (!failed)
	{
		failed |= showbox(ctx, out, pageref, "MediaBox", PDF_NAME(MediaBox));
		failed |= showbox(ctx, out, pageref, "CropBox", PDF_NAME(CropBox));
		failed |= showbox(ctx, out, pageref, "ArtBox", PDF_NAME(ArtBox));
		failed |= showbox(ctx, out, pageref, "BleedBox", PDF_NAME(BleedBox));
		failed |= showbox(ctx, out, pageref, "TrimBox", PDF_NAME(TrimBox));
		failed |= shownum(ctx, out, pageref, "Rotate", PDF_NAME(Rotate));
		failed |= shownum(ctx, out, pageref, "UserUnit", PDF_NAME(UserUnit));
	}

	fz_write_printf(ctx, out, "</page>\n");

	return failed;
}

static int
showpages(fz_context *ctx, pdf_document *doc, fz_output *out, const char *pagelist)
{
	int page, spage, epage;
	int pagecount;
	int ret = 0;

	if (!doc)
		infousage();

	pagecount = pdf_count_pages(ctx, doc);
	while ((pagelist = fz_parse_page_range(ctx, pagelist, &spage, &epage, pagecount)))
	{
		if (spage > epage)
			page = spage, spage = epage, epage = page;
		for (page = spage; page <= epage; page++)
			ret |= showpage(ctx, doc, out, page);
	}

	return ret;
}

static int
pdfpages_pages(fz_context *ctx, fz_output *out, char *filename, char *password, char *argv[], int argc)
{
	enum { NO_FILE_OPENED, NO_INFO_GATHERED, INFO_SHOWN } state;
	int argidx = 0;
	pdf_document *doc = NULL;
	int ret = 0;

	state = NO_FILE_OPENED;
	while (argidx < argc)
	{
		if (state == NO_FILE_OPENED || !fz_is_page_range(ctx, argv[argidx]))
		{
			if (state == NO_INFO_GATHERED)
			{
				showpages(ctx, doc, out, "1-N");
			}

			pdf_drop_document(ctx, doc);

			filename = argv[argidx];
			fz_write_printf(ctx, out, "%s:\n", filename);
			doc = pdf_open_document(ctx, filename);
			if (pdf_needs_password(ctx, doc))
				if (!pdf_authenticate_password(ctx, doc, password))
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot authenticate password: %s", filename);

			state = NO_INFO_GATHERED;
		}
		else
		{
			ret |= showpages(ctx, doc, out, argv[argidx]);
			state = INFO_SHOWN;
		}

		argidx++;
	}

	if (state == NO_INFO_GATHERED)
		showpages(ctx, doc, out, "1-N");

	pdf_drop_document(ctx, doc);

	return ret;
}

int pdfpages_main(int argc, char **argv)
{
	char *filename = "";
	char *password = "";
	int c;
	int ret;
	fz_context *ctx;

	while ((c = fz_getopt(argc, argv, "p:")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		default:
			infousage();
			break;
		}
	}

	if (fz_optind == argc)
		infousage();

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	ret = 0;
	fz_try(ctx)
		ret = pdfpages_pages(ctx, fz_stdout(ctx), filename, password, &argv[fz_optind], argc-fz_optind);
	fz_catch(ctx)
		ret = 1;
	fz_drop_context(ctx);
	return ret;
}
