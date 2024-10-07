// Copyright (C) 2024 Artifex Software, Inc.
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

#include "mupdf/pdf.h"

typedef struct {
	fz_colorspace *outcs;
	pdf_obj *outcs_obj;
} recolor_data;

static void
color_rewrite(fz_context *ctx, void *opaque, pdf_obj **cs_obj, int *n, float color[FZ_MAX_COLORS])
{
	recolor_data *rd = (recolor_data *)opaque;
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
			fz_convert_color(ctx, cs, color, rd->outcs, cols, NULL, fz_default_color_params);
		}
		fz_always(ctx)
			fz_drop_colorspace(ctx, cs);
		fz_catch(ctx)
			fz_rethrow(ctx);
		*n = rd->outcs->n;
	}

	pdf_drop_obj(ctx, *cs_obj);
	*cs_obj = rd->outcs_obj;
	memcpy(color, cols, sizeof(color[0])*4);
}

static void
image_rewrite(fz_context *ctx, void *opaque, fz_image **image, fz_matrix ctm, pdf_obj *im_obj)
{
	recolor_data *rd = (recolor_data *)opaque;
	fz_image *orig = *image;
	fz_pixmap *pix = NULL;
	fz_colorspace* dst_cs;

	fz_var(pix);

	if ((*image)->imagemask)
		return;

	dst_cs = rd->outcs;
	pix = fz_get_unscaled_pixmap_from_image(ctx, orig);

	fz_try(ctx)
	{
		if (pix->colorspace != dst_cs)
		{
			fz_pixmap *pix2 = fz_convert_pixmap(ctx, pix, dst_cs, NULL, NULL, fz_default_color_params, 1);
			fz_drop_pixmap(ctx, pix);
			pix = pix2;
		}

		*image = fz_new_image_from_pixmap(ctx, pix, orig->mask);
		fz_drop_image(ctx, orig);
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pix);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
vertex_rewrite(fz_context *ctx, void *opaque, fz_colorspace *dst_cs, float *d, fz_colorspace *src_cs, const float *s)
{
	recolor_data *rd = (recolor_data *)opaque;

	fz_convert_color(ctx, src_cs, s, rd->outcs, d, NULL, fz_default_color_params);
}

static pdf_recolor_vertex *
shade_rewrite(fz_context *ctx, void *opaque, fz_colorspace *src_cs, fz_colorspace **dst_cs)
{
	recolor_data *rd = (recolor_data *)opaque;

	*dst_cs = rd->outcs;

	return vertex_rewrite;
}

static void
rewrite_page_streams(fz_context *ctx, pdf_document *doc, int page_num, recolor_data *rd)
{
	pdf_page *page = pdf_load_page(ctx, doc, page_num);
	pdf_filter_options options = { 0 };
	pdf_filter_factory list[2] = { 0 };
	pdf_color_filter_options copts = { 0 };
	pdf_annot *annot;

	copts.opaque = rd;
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


void pdf_recolor_page(fz_context *ctx, pdf_document *doc, int pagenum, const pdf_recolor_options *opts)
{
	recolor_data rd = { 0 };

	if (opts == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Options must be supplied");

	switch (opts->num_comp)
	{
	case 1:
		rd.outcs = fz_device_gray(ctx);
		rd.outcs_obj = PDF_NAME(DeviceGray);
		break;
	case 3:
		rd.outcs = fz_device_rgb(ctx);
		rd.outcs_obj = PDF_NAME(DeviceRGB);
		break;
	case 4:
		rd.outcs = fz_device_cmyk(ctx);
		rd.outcs_obj = PDF_NAME(DeviceCMYK);
		break;
	default:
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unsupported number of components");
	}

	rewrite_page_streams(ctx, doc, pagenum, &rd);
}

void pdf_remove_output_intents(fz_context *ctx, pdf_document *doc)
{
	pdf_dict_del(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)), PDF_NAME(OutputIntents));
}
