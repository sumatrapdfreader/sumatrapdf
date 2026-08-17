// Copyright (C) 2026 Artifex Software, Inc.
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

#include "mupdf/fitz.h"

#include "color-imp.h"

typedef struct
{
	fz_device super;
	fz_culling_options opts;
} fz_culling_device;

/* Break an fz_text down into subspans. Consider each subspan for culling. If not culled,
 * pass them into the callback function. */
static int
text_as_spans(fz_context *ctx, fz_culling_device *dev, const fz_text *text, fz_matrix ctm, const fz_stroke_state *stroke,
		void (*callback)(fz_context *ctx, fz_device *thru_dev, fz_text *text, fz_matrix ctm, const fz_stroke_state *stroke, void *args), void *args)
{
	fz_text_span *span, *new_span;
	fz_matrix tm, trm;
	fz_rect bbox;
	int i, sent = 0;
	fz_text *new_text = NULL;

	if (dev->super.passthrough == NULL)
		return 0;

	fz_var(new_text);

	fz_try(ctx)
	{
		new_text = fz_new_text(ctx);

		for (span = text->head; span; span = span->next)
		{
			if (span->len <= 0)
				continue;

			new_span = NULL;

			tm = span->trm;
			for (i = 0; i < span->len; i++)
			{
				if (span->items[i].gid < 0)
					continue;

				tm.e = span->items[i].x;
				tm.f = span->items[i].y;
				trm = fz_concat(tm, ctm);
				bbox = fz_bound_glyph(ctx, span->font, span->items[i].gid, trm);

				if (fz_is_empty_rect(bbox))
					continue;

				/* FIXME: SText bboxes do not allow for stroke sizes. Should they? */
#if 0
				if (stroke)
					bbox = fz_adjust_rect_for_stroke(ctx, bbox, stroke, ctm);
#endif

				if (dev->opts.cull_glyph && dev->opts.cull_glyph(ctx, dev->opts.opaque, bbox))
					continue;

				/* We need to send that glyph through. */
				if (new_span == NULL)
				{
					new_span = fz_malloc_struct(ctx, fz_text_span);
					if (new_text->tail)
						new_text->tail->next = new_span;
					else
						new_text->head = new_span;
					new_text->tail = new_span;
					new_span->items = fz_malloc_array(ctx, span->len, fz_text_item);
					new_span->cap = span->len;
					new_span->bidi_level = span->bidi_level;
					new_span->language = span->language;
					new_span->markup_dir = span->markup_dir;
					new_span->trm = span->trm;
					new_span->wmode = span->wmode;
					new_span->font = fz_keep_font(ctx, span->font);
				}
				new_span->items[new_span->len++] = span->items[i];
			}
		}
		if (new_text->head)
		{
			callback(ctx, dev->super.passthrough, new_text, ctm, stroke, args);
			sent = 1;
		}
	}
	fz_always(ctx)
		fz_drop_text(ctx, new_text);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return sent;
}

typedef struct
{
	fz_colorspace *colorspace;
	const float *color;
	float alpha;
	fz_color_params params;
	fz_rect scissor;
} cb_args;

static void
fill_text_cb(fz_context *ctx, fz_device *dev, fz_text *text, fz_matrix ctm, const fz_stroke_state *stroke, void *args)
{
	cb_args *fa = (cb_args *)args;

	fz_fill_text(ctx, dev, text, ctm, fa->colorspace, fa->color, fa->alpha, fa->params);
}

static void
fz_culling_fill_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_culling_device *dev = (fz_culling_device*)dev_;
	cb_args fa = { colorspace, color, alpha, color_params };

	text_as_spans(ctx, dev, text, ctm, NULL, fill_text_cb, &fa);
}

static void
stroke_text_cb(fz_context *ctx, fz_device *dev, fz_text *text, fz_matrix ctm, const fz_stroke_state *stroke, void *args)
{
	cb_args *fa = (cb_args *)args;

	fz_stroke_text(ctx, dev, text, stroke, ctm, fa->colorspace, fa->color, fa->alpha, fa->params);
}

static void
fz_culling_stroke_text(fz_context *ctx, fz_device *dev_, const fz_text *text, const fz_stroke_state *stroke,
	fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_culling_device *dev = (fz_culling_device*)dev_;
	cb_args fa = { colorspace, color, alpha, color_params };

	text_as_spans(ctx, dev, text, ctm, stroke, stroke_text_cb, &fa);
}

static void
clip_text_cb(fz_context *ctx, fz_device *dev, fz_text *text, fz_matrix ctm, const fz_stroke_state *stroke, void *args)
{
	cb_args *fa = (cb_args *)args;

	fz_clip_text(ctx, dev, text, ctm, fa->scissor);
}

static void
fz_culling_clip_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_culling_device *dev = (fz_culling_device*)dev_;
	cb_args fa = { 0 };

	fa.scissor = scissor;

	if (text_as_spans(ctx, dev, text, ctm, NULL, clip_text_cb, &fa) == 0)
	{
		/* Nothing was sent at all. This will upset the clip stack.
		 * Send an emty clip. */
		fz_clip_path(ctx, dev->super.passthrough, fz_new_path(ctx), 0, ctm, fz_empty_rect);
	}
}

static void
clip_stroke_text_cb(fz_context *ctx, fz_device *dev, fz_text *text, fz_matrix ctm, const fz_stroke_state *stroke, void *args)
{
	cb_args *fa = (cb_args *)args;

	fz_clip_stroke_text(ctx, dev, text, stroke, ctm, fa->scissor);
}

static void
fz_culling_clip_stroke_text(fz_context *ctx, fz_device *dev_, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_culling_device *dev = (fz_culling_device*)dev_;
	cb_args fa = { 0 };

	fa.scissor = scissor;

	if (text_as_spans(ctx, dev, text, ctm, NULL, clip_stroke_text_cb, &fa) == 0)
	{
		/* Nothing was sent at all. This will upset the clip stack.
		 * Send an empty clip. */
		fz_clip_path(ctx, dev->super.passthrough, fz_new_path(ctx), 0, ctm, fz_empty_rect);
	}
}

static void
fz_culling_fill_path(fz_context *ctx, fz_device *dev_, const fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cp)
{
	fz_culling_device *dev = (fz_culling_device*)dev_;

	if (dev->opts.cull_fill_path &&
		dev->opts.cull_fill_path(ctx, dev->opts.opaque, path, even_odd, ctm, cs, color, alpha))
	{
		/* Cull this one */
	}
	else
		fz_fill_path(ctx, dev->super.passthrough, path, even_odd, ctm, cs, color, alpha, cp);
}

static void
fz_culling_stroke_path(fz_context *ctx, fz_device *dev_, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params cp)
{
	fz_culling_device *dev = (fz_culling_device*)dev_;

	if (dev->opts.cull_stroke_path &&
		dev->opts.cull_stroke_path(ctx, dev->opts.opaque, path, stroke, ctm, cs, color, alpha))
	{
		/* Cull this one */
	}
	else
		fz_stroke_path(ctx, dev->super.passthrough, path, stroke, ctm, cs, color, alpha, cp);
}

static void
fz_culling_close_device(fz_context *ctx, fz_device *dev_)
{
	/* Does not pass through */
}

static void
fz_culling_drop_device(fz_context *ctx, fz_device *dev_)
{
	fz_culling_device *dev = (fz_culling_device*)dev_;

	if (dev->opts.drop)
		dev->opts.drop(ctx, dev->opts.opaque);
	fz_drop_device(ctx, dev->super.passthrough); /* Drop my reference */
}

fz_device *
fz_new_culling_device(fz_context *ctx, fz_device *passthrough, const fz_culling_options *opts)
{
	fz_culling_device *dev = fz_new_derived_passthrough_device(ctx, passthrough, fz_culling_device);

	dev->super.fill_text = fz_culling_fill_text;
	dev->super.stroke_text = fz_culling_stroke_text;
	dev->super.clip_text = fz_culling_clip_text;
	dev->super.clip_stroke_text = fz_culling_clip_stroke_text;
	dev->super.fill_path = fz_culling_fill_path;
	dev->super.stroke_path = fz_culling_stroke_path;

	dev->super.close_device = fz_culling_close_device;
	dev->super.drop_device = fz_culling_drop_device;

	dev->opts = *opts;

	return (fz_device*)dev;
}

typedef struct
{
	int n;
	float borders;
	fz_rect rects[FZ_FLEXIBLE_ARRAY];
} cull_rects;

static void
drop_culling_rects(fz_context *ctx, void *opaque)
{
	cull_rects *er = (cull_rects *)opaque;

	fz_free(ctx, er);
}

static int
cull_rect(fz_context *ctx, void *opaque, fz_rect rect)
{
	int i;
	cull_rects *er = (cull_rects *)opaque;
	float area = fz_rect_area(rect)/2;
	float overlapped = 0;

	for (i = 0; i < er->n; i++)
	{
		fz_rect ov = fz_intersect_rect(rect, er->rects[i]);
		overlapped += fz_rect_area(ov);
		/* If more than half of the text box is overlapped, cull it. */
		if (overlapped > area)
			return 1;
	}

	return 0;
}

static int
cull_fill_border(fz_context *ctx, void *opaque, const fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha)
{
	cull_rects *er = (cull_rects *)opaque;
	fz_rect r;
	float borders = er->borders;

	if (borders <= 0)
		return 0;

	if (!fz_path_is_rect_with_bounds(ctx, path, ctm, &r))
		return 0; // We only cull rects.

	if (!fz_is_valid_rect(r))
		return 0;

	if (r.y1 - r.y0 <= borders || r.x1 - r.x0 <= borders)
		return 1;

	return 0;
}

static int
cull_stroke_border(fz_context *ctx, void *opaque, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha)
{
	cull_rects *er = (cull_rects *)opaque;
	fz_rect r;
	float borders = er->borders;

	if (borders <= 0)
		return 0;

	if (!fz_path_is_rect_with_bounds(ctx, path, ctm, &r))
		return 0; // We only cull rects.

	r = fz_adjust_rect_for_stroke(ctx, r, stroke, ctm);

	if (!fz_is_valid_rect(r))
		return 0;

	if (r.y1 - r.y0 <= borders || r.x1 - r.x0 <= borders)
		return 1;

	return 0;
}

fz_device *
fz_new_culling_device_with_rects(fz_context *ctx, fz_device *passthrough, int n, const fz_rect *rects)
{
	cull_rects *er;
	fz_device *dev = NULL;
	fz_culling_options opts = { 0 };

	er = fz_malloc_flexible(ctx, cull_rects, rects, n);
	er->n = n;
	if (n)
		memcpy(er->rects, rects, sizeof(rects[0])*n);

	opts.opaque = er;
	opts.drop = drop_culling_rects;
	opts.cull_glyph = cull_rect;

	fz_var(dev);

	fz_try(ctx)
	{
		dev = fz_new_culling_device(ctx, passthrough, &opts);
	}
	fz_catch(ctx)
	{
		drop_culling_rects(ctx, er);
		fz_rethrow(ctx);
	}

	return dev;
}

fz_pixmap *fz_new_pixmap_from_page_number_culling_text_etc(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, int alpha, int n, const fz_rect *rects, float borders)
{
	fz_pixmap *pix;
	fz_culling_options opts = { 0 };
	cull_rects *er;

	er = fz_malloc_flexible(ctx, cull_rects, rects, n);
	er->borders = borders;
	er->n = n;
	if (n)
		memcpy(er->rects, rects, sizeof(rects[0])*n);

	opts.opaque = er;
	opts.drop = drop_culling_rects;
	opts.cull_glyph = cull_rect;
	if (borders)
	{
		opts.cull_fill_path = cull_fill_border;
		opts.cull_stroke_path = cull_stroke_border;
	}

	fz_try(ctx)
		pix = fz_new_pixmap_from_culled_page_number(ctx, doc, number, ctm, cs, alpha, &opts);
	fz_catch(ctx)
	{
		fz_free(ctx, er);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *fz_new_pixmap_from_culled_page_number(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, int alpha, const fz_culling_options *opts)
{
	fz_page *page;
	fz_pixmap *pix = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		pix = fz_new_pixmap_from_culled_page(ctx, page, ctm, cs, alpha, opts);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page_culling_text_etc(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha, int n, const fz_rect *rects, float borders)
{
	fz_culling_options opts = { 0 };
	fz_pixmap *pix;
	cull_rects *er;

	er = fz_malloc_flexible(ctx, cull_rects, rects, n);
	er->borders = borders;
	er->n = n;
	if (n)
		memcpy(er->rects, rects, sizeof(rects[0])*n);

	opts.opaque = er;
	opts.drop = drop_culling_rects;
	opts.cull_glyph = cull_rect;
	if (borders)
	{
		opts.cull_fill_path = cull_fill_border;
		opts.cull_stroke_path = cull_stroke_border;
	}

	fz_try(ctx)
		pix = fz_new_pixmap_from_culled_page(ctx, page, ctm, cs, alpha, &opts);
	fz_catch(ctx)
	{
		fz_free(ctx, er);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_culled_page(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha, const fz_culling_options *opts)
{
	fz_device *draw_dev = NULL;
	fz_device *cull_dev = NULL;
	fz_rect bounds = fz_bound_page(ctx, page);
	fz_rect tbounds = fz_transform_rect(bounds, ctm);
	fz_irect itbounds = fz_irect_from_rect(tbounds);
	fz_pixmap *pix = fz_new_pixmap_with_bbox(ctx, cs, itbounds, NULL, alpha);

	fz_var(draw_dev);
	fz_var(cull_dev);

	fz_try(ctx)
	{
		fz_clear_pixmap(ctx, pix);
		draw_dev = fz_new_draw_device(ctx, ctm, pix);
		cull_dev = fz_new_culling_device(ctx, draw_dev, opts);

		fz_run_page(ctx, page, cull_dev, fz_identity, NULL);
		fz_close_device(ctx, cull_dev);
		fz_close_device(ctx, draw_dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, cull_dev);
		fz_drop_device(ctx, draw_dev);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_culled_display_list(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, int alpha, const fz_culling_options *opts)
{
	fz_device *draw_dev = NULL;
	fz_device *cull_dev = NULL;
	fz_rect bounds = fz_bound_display_list(ctx, list);
	fz_rect tbounds = fz_transform_rect(bounds, ctm);
	fz_irect itbounds = fz_irect_from_rect(tbounds);
	fz_pixmap *pix = fz_new_pixmap_with_bbox(ctx, cs, itbounds, NULL, alpha);

	fz_var(draw_dev);
	fz_var(cull_dev);

	fz_try(ctx)
	{
		fz_clear_pixmap(ctx, pix);
		draw_dev = fz_new_draw_device(ctx, ctm, pix);
		cull_dev = fz_new_culling_device(ctx, draw_dev, opts);

		fz_run_display_list(ctx, list, cull_dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, cull_dev);
		fz_close_device(ctx, draw_dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, cull_dev);
		fz_drop_device(ctx, draw_dev);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return pix;
}

fz_pixmap *fz_new_pixmap_from_display_list_culling_text_etc(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, int alpha, int n, const fz_rect *rects, float borders)
{
	fz_culling_options opts = { 0 };
	fz_pixmap *pix;
	cull_rects *er;

	er = fz_malloc_flexible(ctx, cull_rects, rects, n);
	er->borders = borders;
	er->n = n;
	if (n)
		memcpy(er->rects, rects, sizeof(rects[0])*n);

	opts.opaque = er;
	opts.drop = drop_culling_rects;
	opts.cull_glyph = cull_rect;
	if (borders)
	{
		opts.cull_fill_path = cull_fill_border;
		opts.cull_stroke_path = cull_stroke_border;
	}

	fz_try(ctx)
		pix = fz_new_pixmap_from_culled_display_list(ctx, list, ctm, cs, alpha, &opts);
	fz_catch(ctx)
	{
		fz_free(ctx, er);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *fz_new_pixmap_from_page_number_culling_text(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, int alpha, int n, const fz_rect *rects)
{
	return fz_new_pixmap_from_page_number_culling_text_etc(ctx, doc, number, ctm, cs, alpha, n, rects, 0);
}

fz_pixmap *fz_new_pixmap_from_page_culling_text(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha, int n, const fz_rect *rects)
{
	return fz_new_pixmap_from_page_culling_text_etc(ctx, page, ctm, cs, alpha, n, rects, 0);
}

fz_pixmap *fz_new_pixmap_from_display_list_culling_text(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, int alpha, int n, const fz_rect *rects)
{
	return fz_new_pixmap_from_display_list_culling_text_etc(ctx, list, ctm, cs, alpha, n, rects, 0);
}
