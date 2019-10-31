#include "mupdf/fitz.h"
#include "xps-imp.h"

#include <math.h>
#include <string.h>

#define TILE

/*
 * Parse a tiling brush (visual and image brushes at this time) common
 * properties. Use the callback to draw the individual tiles.
 */

enum { TILE_NONE, TILE_TILE, TILE_FLIP_X, TILE_FLIP_Y, TILE_FLIP_X_Y };

struct closure
{
	char *base_uri;
	xps_resource *dict;
	fz_xml *root;
	void *user;
	void (*func)(fz_context *ctx, xps_document*, fz_matrix, fz_rect, char*, xps_resource*, fz_xml*, void*);
};

static void
xps_paint_tiling_brush_clipped(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect viewbox, struct closure *c)
{
	fz_device *dev = doc->dev;
	fz_path *path;

	path = fz_new_path(ctx);
	fz_try(ctx)
	{
		fz_moveto(ctx, path, viewbox.x0, viewbox.y0);
		fz_lineto(ctx, path, viewbox.x0, viewbox.y1);
		fz_lineto(ctx, path, viewbox.x1, viewbox.y1);
		fz_lineto(ctx, path, viewbox.x1, viewbox.y0);
		fz_closepath(ctx, path);
		fz_clip_path(ctx, dev, path, 0, ctm, fz_infinite_rect);
	}
	fz_always(ctx)
		fz_drop_path(ctx, path);
	fz_catch(ctx)
		fz_rethrow(ctx);

	c->func(ctx, doc, ctm, viewbox, c->base_uri, c->dict, c->root, c->user);
	fz_pop_clip(ctx, dev);
}

static void
xps_paint_tiling_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect viewbox, int tile_mode, struct closure *c)
{
	fz_matrix ttm;

	xps_paint_tiling_brush_clipped(ctx, doc, ctm, viewbox, c);

	if (tile_mode == TILE_FLIP_X || tile_mode == TILE_FLIP_X_Y)
	{
		ttm = fz_pre_scale(fz_pre_translate(ctm, viewbox.x1 * 2, 0), -1, 1);
		xps_paint_tiling_brush_clipped(ctx, doc, ttm, viewbox, c);
	}

	if (tile_mode == TILE_FLIP_Y || tile_mode == TILE_FLIP_X_Y)
	{
		ttm = fz_pre_scale(fz_pre_translate(ctm, 0, viewbox.y1 * 2), 1, -1);
		xps_paint_tiling_brush_clipped(ctx, doc, ttm, viewbox, c);
	}

	if (tile_mode == TILE_FLIP_X_Y)
	{
		ttm = fz_pre_scale(fz_pre_translate(ctm, viewbox.x1 * 2, viewbox.y1 * 2), -1, -1);
		xps_paint_tiling_brush_clipped(ctx, doc, ttm, viewbox, c);
	}
}

void
xps_parse_tiling_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, fz_xml *root,
	void (*func)(fz_context *ctx, xps_document*, fz_matrix, fz_rect, char*, xps_resource*, fz_xml*, void*), void *user)
{
	fz_device *dev = doc->dev;
	fz_xml *node;
	struct closure c;

	char *opacity_att;
	char *transform_att;
	char *viewbox_att;
	char *viewport_att;
	char *tile_mode_att;

	fz_xml *transform_tag = NULL;

	fz_rect viewbox;
	fz_rect viewport;
	float xstep, ystep;
	float xscale, yscale;
	int tile_mode;

	opacity_att = fz_xml_att(root, "Opacity");
	transform_att = fz_xml_att(root, "Transform");
	viewbox_att = fz_xml_att(root, "Viewbox");
	viewport_att = fz_xml_att(root, "Viewport");
	tile_mode_att = fz_xml_att(root, "TileMode");

	c.base_uri = base_uri;
	c.dict = dict;
	c.root = root;
	c.user = user;
	c.func = func;

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "ImageBrush.Transform"))
			transform_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "VisualBrush.Transform"))
			transform_tag = fz_xml_down(node);
	}

	xps_resolve_resource_reference(ctx, doc, dict, &transform_att, &transform_tag, NULL);

	ctm = xps_parse_transform(ctx, doc, transform_att, transform_tag, ctm);

	viewbox = fz_unit_rect;
	if (viewbox_att)
		viewbox = xps_parse_rectangle(ctx, doc, viewbox_att);

	viewport = fz_unit_rect;
	if (viewport_att)
		viewport = xps_parse_rectangle(ctx, doc, viewport_att);

	if (fabsf(viewport.x1 - viewport.x0) < 0.01f || fabsf(viewport.y1 - viewport.y0) < 0.01f)
		fz_warn(ctx, "not drawing tile for viewport size %.4f x %.4f", viewport.x1 - viewport.x0, viewport.y1 - viewport.y0);
	else if (fabsf(viewbox.x1 - viewbox.x0) < 0.01f || fabsf(viewbox.y1 - viewbox.y0) < 0.01f)
		fz_warn(ctx, "not drawing tile for viewbox size %.4f x %.4f", viewbox.x1 - viewbox.x0, viewbox.y1 - viewbox.y0);

	/* some sanity checks on the viewport/viewbox size */
	if (fabsf(viewport.x1 - viewport.x0) < 0.01f) return;
	if (fabsf(viewport.y1 - viewport.y0) < 0.01f) return;
	if (fabsf(viewbox.x1 - viewbox.x0) < 0.01f) return;
	if (fabsf(viewbox.y1 - viewbox.y0) < 0.01f) return;

	xstep = viewbox.x1 - viewbox.x0;
	ystep = viewbox.y1 - viewbox.y0;

	xscale = (viewport.x1 - viewport.x0) / xstep;
	yscale = (viewport.y1 - viewport.y0) / ystep;

	tile_mode = TILE_NONE;
	if (tile_mode_att)
	{
		if (!strcmp(tile_mode_att, "None"))
			tile_mode = TILE_NONE;
		if (!strcmp(tile_mode_att, "Tile"))
			tile_mode = TILE_TILE;
		if (!strcmp(tile_mode_att, "FlipX"))
			tile_mode = TILE_FLIP_X;
		if (!strcmp(tile_mode_att, "FlipY"))
			tile_mode = TILE_FLIP_Y;
		if (!strcmp(tile_mode_att, "FlipXY"))
			tile_mode = TILE_FLIP_X_Y;
	}

	if (tile_mode == TILE_FLIP_X || tile_mode == TILE_FLIP_X_Y)
		xstep *= 2;
	if (tile_mode == TILE_FLIP_Y || tile_mode == TILE_FLIP_X_Y)
		ystep *= 2;

	xps_begin_opacity(ctx, doc, ctm, area, base_uri, dict, opacity_att, NULL);

	ctm = fz_pre_translate(ctm, viewport.x0, viewport.y0);
	ctm = fz_pre_scale(ctm, xscale, yscale);
	ctm = fz_pre_translate(ctm, -viewbox.x0, -viewbox.y0);

	if (tile_mode != TILE_NONE)
	{
		int x0, y0, x1, y1;
		fz_matrix invctm;
		invctm = fz_invert_matrix(ctm);
		area = fz_transform_rect(area, invctm);
		x0 = floorf(area.x0 / xstep);
		y0 = floorf(area.y0 / ystep);
		x1 = ceilf(area.x1 / xstep);
		y1 = ceilf(area.y1 / ystep);

#ifdef TILE
		if ((x1 - x0) * (y1 - y0) > 1)
#else
		if (0)
#endif
		{
			fz_rect bigview = viewbox;
			bigview.x1 = bigview.x0 + xstep;
			bigview.y1 = bigview.y0 + ystep;
			fz_begin_tile(ctx, dev, area, bigview, xstep, ystep, ctm);
			xps_paint_tiling_brush(ctx, doc, ctm, viewbox, tile_mode, &c);
			fz_end_tile(ctx, dev);
		}
		else
		{
			int x, y;
			for (y = y0; y < y1; y++)
			{
				for (x = x0; x < x1; x++)
				{
					fz_matrix ttm = fz_pre_translate(ctm, xstep * x, ystep * y);
					xps_paint_tiling_brush(ctx, doc, ttm, viewbox, tile_mode, &c);
				}
			}
		}
	}
	else
	{
		xps_paint_tiling_brush(ctx, doc, ctm, viewbox, tile_mode, &c);
	}

	xps_end_opacity(ctx, doc, base_uri, dict, opacity_att, NULL);
}

static void
xps_paint_visual_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, fz_xml *root, void *visual_tag)
{
	xps_parse_element(ctx, doc, ctm, area, base_uri, dict, (fz_xml *)visual_tag);
}

void
xps_parse_visual_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, fz_xml *root)
{
	fz_xml *node;

	char *visual_uri;
	char *visual_att;
	fz_xml *visual_tag = NULL;

	visual_att = fz_xml_att(root, "Visual");

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "VisualBrush.Visual"))
			visual_tag = fz_xml_down(node);
	}

	visual_uri = base_uri;
	xps_resolve_resource_reference(ctx, doc, dict, &visual_att, &visual_tag, &visual_uri);

	if (visual_tag)
	{
		xps_parse_tiling_brush(ctx, doc, ctm, area,
			visual_uri, dict, root, xps_paint_visual_brush, visual_tag);
	}
}

void
xps_parse_canvas(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *root)
{
	fz_device *dev = doc->dev;
	xps_resource *new_dict = NULL;
	fz_xml *node;
	char *opacity_mask_uri;

	char *transform_att;
	char *clip_att;
	char *opacity_att;
	char *opacity_mask_att;

	fz_xml *transform_tag = NULL;
	fz_xml *clip_tag = NULL;
	fz_xml *opacity_mask_tag = NULL;

	transform_att = fz_xml_att(root, "RenderTransform");
	clip_att = fz_xml_att(root, "Clip");
	opacity_att = fz_xml_att(root, "Opacity");
	opacity_mask_att = fz_xml_att(root, "OpacityMask");

	for (node = fz_xml_down(root); node; node = fz_xml_next(node))
	{
		if (fz_xml_is_tag(node, "Canvas.Resources") && fz_xml_down(node))
		{
			if (new_dict)
			{
				fz_warn(ctx, "ignoring follow-up resource dictionaries");
			}
			else
			{
				new_dict = xps_parse_resource_dictionary(ctx, doc, base_uri, fz_xml_down(node));
				if (new_dict)
				{
					new_dict->parent = dict;
					dict = new_dict;
				}
			}
		}

		if (fz_xml_is_tag(node, "Canvas.RenderTransform"))
			transform_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "Canvas.Clip"))
			clip_tag = fz_xml_down(node);
		if (fz_xml_is_tag(node, "Canvas.OpacityMask"))
			opacity_mask_tag = fz_xml_down(node);
	}

	fz_try(ctx)
	{
		opacity_mask_uri = base_uri;
		xps_resolve_resource_reference(ctx, doc, dict, &transform_att, &transform_tag, NULL);
		xps_resolve_resource_reference(ctx, doc, dict, &clip_att, &clip_tag, NULL);
		xps_resolve_resource_reference(ctx, doc, dict, &opacity_mask_att, &opacity_mask_tag, &opacity_mask_uri);

		ctm = xps_parse_transform(ctx, doc, transform_att, transform_tag, ctm);

		if (clip_att || clip_tag)
			xps_clip(ctx, doc, ctm, dict, clip_att, clip_tag);

		xps_begin_opacity(ctx, doc, ctm, area, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);

		for (node = fz_xml_down(root); node; node = fz_xml_next(node))
			xps_parse_element(ctx, doc, ctm, area, base_uri, dict, node);

		xps_end_opacity(ctx, doc, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);

		if (clip_att || clip_tag)
			fz_pop_clip(ctx, dev);
	}
	fz_always(ctx)
		xps_drop_resource_dictionary(ctx, doc, new_dict);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
xps_parse_fixed_page(fz_context *ctx, xps_document *doc, fz_matrix ctm, xps_page *page)
{
	fz_xml *root, *node;
	xps_resource *dict;
	char base_uri[1024];
	fz_rect area;
	char *s;

	fz_strlcpy(base_uri, page->fix->name, sizeof base_uri);
	s = strrchr(base_uri, '/');
	if (s)
		s[1] = 0;

	dict = NULL;

	doc->opacity_top = 0;
	doc->opacity[0] = 1;

	root = fz_xml_root(page->xml);
	if (!root)
		return;

	area = fz_transform_rect(fz_unit_rect, fz_scale(page->fix->width, page->fix->height));

	fz_try(ctx)
	{
		for (node = fz_xml_down(root); node; node = fz_xml_next(node))
		{
			if (fz_xml_is_tag(node, "FixedPage.Resources") && fz_xml_down(node))
			{
				if (dict)
					fz_warn(ctx, "ignoring follow-up resource dictionaries");
				else
					dict = xps_parse_resource_dictionary(ctx, doc, base_uri, fz_xml_down(node));
			}
			xps_parse_element(ctx, doc, ctm, area, base_uri, dict, node);
		}
	}
	fz_always(ctx)
		xps_drop_resource_dictionary(ctx, doc, dict);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
xps_run_page(fz_context *ctx, fz_page *page_, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	xps_page *page = (xps_page*)page_;
	xps_document *doc = page->doc;
	fz_matrix page_ctm;

	page_ctm = fz_pre_scale(ctm, 72.0f / 96.0f, 72.0f / 96.0f);

	doc->cookie = cookie;
	doc->dev = dev;
	xps_parse_fixed_page(ctx, doc, page_ctm, page);
	doc->cookie = NULL;
	doc->dev = NULL;
}
