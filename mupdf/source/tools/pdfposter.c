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

/*
 * PDF posteriser; split pages within a PDF file into smaller lumps.
 */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int x_factor = 0;
static int y_factor = 0;
static float margin = 0;
static int margin_is_percent = 0;
static int x_dir = 1;

static int usage(void)
{
	fprintf(stderr,
		"usage: mutool poster [options] input.pdf [output.pdf]\n"
		"\t-p -\tpassword\n"
		"\t-m -\tmargin (overlap) between pages (pts, or %%)\n"
		"\t-x\tx decimation factor\n"
		"\t-y\ty decimation factor\n"
		"\t-r\tsplit right-to-left\n"
		);
	return 1;
}

static void
intersect_box(fz_context *ctx, pdf_document *doc, pdf_obj *page, pdf_obj *box_name, fz_rect mb)
{
	pdf_obj *box = pdf_dict_get(ctx, page, box_name);
	pdf_obj *newbox;
	fz_rect old_rect;

	if (box == NULL)
		return;

	old_rect.x0 = pdf_array_get_real(ctx, box, 0);
	old_rect.y0 = pdf_array_get_real(ctx, box, 1);
	old_rect.x1 = pdf_array_get_real(ctx, box, 2);
	old_rect.y1 = pdf_array_get_real(ctx, box, 3);

	if (old_rect.x0 < mb.x0)
		old_rect.x0 = mb.x0;
	if (old_rect.y0 < mb.y0)
		old_rect.y0 = mb.y0;
	if (old_rect.x1 > mb.x1)
		old_rect.x1 = mb.x1;
	if (old_rect.y1 > mb.y1)
		old_rect.y1 = mb.y1;

	newbox = pdf_new_array(ctx, doc, 4);
	pdf_array_push_real(ctx, newbox, old_rect.x0);
	pdf_array_push_real(ctx, newbox, old_rect.y0);
	pdf_array_push_real(ctx, newbox, old_rect.x1);
	pdf_array_push_real(ctx, newbox, old_rect.y1);
	pdf_dict_put_drop(ctx, page, box_name, newbox);
}

/*
 * Recreate page tree with our posterised pages in.
 */

static void decimatepages(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *oldroot, *root, *pages, *kids;
	int num_pages = pdf_count_pages(ctx, doc);
	int page, kidcount;
	fz_rect mediabox, cropbox;
	int rotate;

	oldroot = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	pages = pdf_dict_get(ctx, oldroot, PDF_NAME(Pages));

	root = pdf_new_dict(ctx, doc, 2);
	pdf_dict_put(ctx, root, PDF_NAME(Type), pdf_dict_get(ctx, oldroot, PDF_NAME(Type)));
	pdf_dict_put(ctx, root, PDF_NAME(Pages), pdf_dict_get(ctx, oldroot, PDF_NAME(Pages)));

	pdf_update_object(ctx, doc, pdf_to_num(ctx, oldroot), root);

	pdf_drop_obj(ctx, root);

	/* Create a new kids array with our new pages in */
	kids = pdf_new_array(ctx, doc, 1);

	kidcount = 0;
	for (page=0; page < num_pages; page++)
	{
		pdf_obj *page_obj = pdf_lookup_page_obj(ctx, doc, page);
		int xf = x_factor, yf = y_factor;
		float w, h;
		int x, y;
		int xd, yd, y0, y1;
		float xmargin, ymargin;

		rotate = pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, page_obj, PDF_NAME(Rotate)));
		mediabox = pdf_to_rect(ctx, pdf_dict_get_inheritable(ctx, page_obj, PDF_NAME(MediaBox)));
		cropbox = pdf_to_rect(ctx, pdf_dict_get_inheritable(ctx, page_obj, PDF_NAME(CropBox)));
		if (fz_is_empty_rect(mediabox))
			mediabox = fz_make_rect(0, 0, 612, 792);
		if (!fz_is_empty_rect(cropbox))
			mediabox = fz_intersect_rect(mediabox, cropbox);

		w = mediabox.x1 - mediabox.x0;
		h = mediabox.y1 - mediabox.y0;

		switch (rotate)
		{
		default:
		case 0:
			xf = x_factor;
			yf = y_factor;
			xd = x_dir;
			yd = -x_dir;
			break;
		case 90:
			xf = y_factor;
			yf = x_factor;
			yd = x_dir;
			xd = x_dir;
			break;
		case 180:
			xf = x_factor;
			yf = y_factor;
			xd = -x_dir;
			yd = x_dir;
			break;
		case 270:
			xf = y_factor;
			yf = x_factor;
			yd = -x_dir;
			xd = -x_dir;
			break;
		}

		if (xf == 0 && yf == 0)
		{
			/* Nothing specified, so split along the long edge */
			if (w > h)
				xf = 2, yf = 1;
			else
				xf = 1, yf = 2;
		}
		else if (xf == 0)
			xf = 1;
		else if (yf == 0)
			yf = 1;

		xmargin = xf == 1 ? 0 : margin * (margin_is_percent ? .001 * w/xf : 1);
		ymargin = yf == 1 ? 0 : margin * (margin_is_percent ? .001 * h/yf : 1);

		y0 = (yd > 0) ? 0 : yf-1;
		y1 = (yd > 0) ? yf : -1;
		for (y = y0; y != y1; y += yd)
		{
			int x0 = (xd > 0) ? 0 : xf-1;
			int x1 = (xd > 0) ? xf : -1;
			for (x = x0; x != x1; x += xd)
			{
				pdf_obj *newpageobj, *newpageref, *newmediabox;
				fz_rect mb;

				newpageobj = pdf_copy_dict(ctx, pdf_lookup_page_obj(ctx, doc, page));
				pdf_flatten_inheritable_page_items(ctx, newpageobj);
				newpageref = pdf_add_object(ctx, doc, newpageobj);

				newmediabox = pdf_new_array(ctx, doc, 4);

				mb.x0 = mediabox.x0 + (w/xf)*x;
				if (x == xf-1)
					mb.x1 = mediabox.x1;
				else
					mb.x1 = mb.x0 + (w/xf) + xmargin;
				mb.y0 = mediabox.y0 + (h/yf)*y;
				if (y == yf-1)
					mb.y1 = mediabox.y1;
				else
					mb.y1 = mb.y0 + (h/yf) + ymargin;

				pdf_array_push_real(ctx, newmediabox, mb.x0);
				pdf_array_push_real(ctx, newmediabox, mb.y0);
				pdf_array_push_real(ctx, newmediabox, mb.x1);
				pdf_array_push_real(ctx, newmediabox, mb.y1);

				pdf_dict_put(ctx, newpageobj, PDF_NAME(Parent), pages);
				pdf_dict_put_drop(ctx, newpageobj, PDF_NAME(MediaBox), newmediabox);

				intersect_box(ctx, doc, newpageobj, PDF_NAME(CropBox), mb);
				intersect_box(ctx, doc, newpageobj, PDF_NAME(BleedBox), mb);
				intersect_box(ctx, doc, newpageobj, PDF_NAME(TrimBox), mb);
				intersect_box(ctx, doc, newpageobj, PDF_NAME(ArtBox), mb);

				/* Store page object in new kids array */
				pdf_drop_obj(ctx, newpageobj);
				pdf_array_push_drop(ctx, kids, newpageref);

				kidcount++;
			}
		}
	}

	/* Update page count and kids array */
	pdf_dict_put_int(ctx, pages, PDF_NAME(Count), kidcount);
	pdf_dict_put_drop(ctx, pages, PDF_NAME(Kids), kids);
}

int pdfposter_main(int argc, char **argv)
{
	char *infile;
	char *outfile = "out.pdf";
	char *password = "";
	int c;
	pdf_write_options opts = pdf_default_write_options;
	pdf_document *doc = NULL;
	fz_context *ctx;
	int ret = 0;

	while ((c = fz_getopt(argc, argv, "m:x:y:p:r")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'm': margin = atof(fz_optarg); margin_is_percent = (strchr(fz_optarg, '%') != NULL); break;
		case 'x': x_factor = atoi(fz_optarg); break;
		case 'y': y_factor = atoi(fz_optarg); break;
		case 'r': x_dir = -1; break;
		default: return usage();
		}
	}

	if (argc - fz_optind < 1)
		return usage();

	infile = argv[fz_optind++];

	if (argc - fz_optind > 0 &&
		(strstr(argv[fz_optind], ".pdf") || strstr(argv[fz_optind], ".PDF")))
	{
		outfile = fz_optpath(argv[fz_optind++]);
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	fz_var(doc);

	fz_try(ctx)
	{
		doc = pdf_open_document(ctx, infile);
		if (pdf_needs_password(ctx, doc))
			if (!pdf_authenticate_password(ctx, doc, password))
				fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot authenticate password: %s", infile);

		decimatepages(ctx, doc);

		pdf_save_document(ctx, doc, outfile, &opts);
	}
	fz_always(ctx)
		pdf_drop_document(ctx, doc);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		ret = 1;
	}
	fz_drop_context(ctx);

	return ret;
}
