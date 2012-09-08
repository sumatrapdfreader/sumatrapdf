#include "muxps-internal.h"

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
	xml_element *root;
	void *user;
	void (*func)(xps_document*, fz_matrix, fz_rect, char*, xps_resource*, xml_element*, void*);
};

static void
xps_paint_tiling_brush_clipped(xps_document *doc, fz_matrix ctm, fz_rect viewbox, struct closure *c)
{
	fz_path *path = fz_new_path(doc->ctx);
	fz_moveto(doc->ctx, path, viewbox.x0, viewbox.y0);
	fz_lineto(doc->ctx, path, viewbox.x0, viewbox.y1);
	fz_lineto(doc->ctx, path, viewbox.x1, viewbox.y1);
	fz_lineto(doc->ctx, path, viewbox.x1, viewbox.y0);
	fz_closepath(doc->ctx, path);
	fz_clip_path(doc->dev, path, NULL, 0, ctm);
	fz_free_path(doc->ctx, path);
	c->func(doc, ctm, viewbox, c->base_uri, c->dict, c->root, c->user);
	fz_pop_clip(doc->dev);
}

static void
xps_paint_tiling_brush(xps_document *doc, fz_matrix ctm, fz_rect viewbox, int tile_mode, struct closure *c)
{
	fz_matrix ttm;

	xps_paint_tiling_brush_clipped(doc, ctm, viewbox, c);

	if (tile_mode == TILE_FLIP_X || tile_mode == TILE_FLIP_X_Y)
	{
		ttm = fz_concat(fz_translate(viewbox.x1 * 2, 0), ctm);
		ttm = fz_concat(fz_scale(-1, 1), ttm);
		xps_paint_tiling_brush_clipped(doc, ttm, viewbox, c);
	}

	if (tile_mode == TILE_FLIP_Y || tile_mode == TILE_FLIP_X_Y)
	{
		ttm = fz_concat(fz_translate(0, viewbox.y1 * 2), ctm);
		ttm = fz_concat(fz_scale(1, -1), ttm);
		xps_paint_tiling_brush_clipped(doc, ttm, viewbox, c);
	}

	if (tile_mode == TILE_FLIP_X_Y)
	{
		ttm = fz_concat(fz_translate(viewbox.x1 * 2, viewbox.y1 * 2), ctm);
		ttm = fz_concat(fz_scale(-1, -1), ttm);
		xps_paint_tiling_brush_clipped(doc, ttm, viewbox, c);
	}
}

void
xps_parse_tiling_brush(xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, xml_element *root,
	void (*func)(xps_document*, fz_matrix, fz_rect, char*, xps_resource*, xml_element*, void*), void *user)
{
	xml_element *node;
	struct closure c;

	char *opacity_att;
	char *transform_att;
	char *viewbox_att;
	char *viewport_att;
	char *tile_mode_att;
	char *viewbox_units_att;
	char *viewport_units_att;

	xml_element *transform_tag = NULL;

	fz_matrix transform;
	fz_rect viewbox;
	fz_rect viewport;
	float xstep, ystep;
	float xscale, yscale;
	int tile_mode;

	opacity_att = xml_att(root, "Opacity");
	transform_att = xml_att(root, "Transform");
	viewbox_att = xml_att(root, "Viewbox");
	viewport_att = xml_att(root, "Viewport");
	tile_mode_att = xml_att(root, "TileMode");
	viewbox_units_att = xml_att(root, "ViewboxUnits");
	viewport_units_att = xml_att(root, "ViewportUnits");

	c.base_uri = base_uri;
	c.dict = dict;
	c.root = root;
	c.user = user;
	c.func = func;

	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "ImageBrush.Transform"))
			transform_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "VisualBrush.Transform"))
			transform_tag = xml_down(node);
	}

	xps_resolve_resource_reference(doc, dict, &transform_att, &transform_tag, NULL);

	transform = fz_identity;
	if (transform_att)
		xps_parse_render_transform(doc, transform_att, &transform);
	if (transform_tag)
		xps_parse_matrix_transform(doc, transform_tag, &transform);
	ctm = fz_concat(transform, ctm);

	viewbox = fz_unit_rect;
	if (viewbox_att)
		xps_parse_rectangle(doc, viewbox_att, &viewbox);

	viewport = fz_unit_rect;
	if (viewport_att)
		xps_parse_rectangle(doc, viewport_att, &viewport);

	if (fabsf(viewport.x1 - viewport.x0) < 0.01f || fabsf(viewport.y1 - viewport.y0) < 0.01f)
		fz_warn(doc->ctx, "not drawing tile for viewport size %.4f x %.4f", viewport.x1 - viewport.x0, viewport.y1 - viewport.y0);
	else if (fabsf(viewbox.x1 - viewbox.x0) < 0.01f || fabsf(viewbox.y1 - viewbox.y0) < 0.01f)
		fz_warn(doc->ctx, "not drawing tile for viewbox size %.4f x %.4f", viewbox.x1 - viewbox.x0, viewbox.y1 - viewbox.y0);

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

	xps_begin_opacity(doc, ctm, area, base_uri, dict, opacity_att, NULL);

	ctm = fz_concat(fz_translate(viewport.x0, viewport.y0), ctm);
	ctm = fz_concat(fz_scale(xscale, yscale), ctm);
	ctm = fz_concat(fz_translate(-viewbox.x0, -viewbox.y0), ctm);

	if (tile_mode != TILE_NONE)
	{
		int x0, y0, x1, y1;
		fz_matrix invctm = fz_invert_matrix(ctm);
		area = fz_transform_rect(invctm, area);
		/* SumatraPDF: make sure that the intended area is covered */
		{
			fz_point tl;
			fz_bbox bbox;
			fz_rect bigview = viewbox;
			bigview.x1 = bigview.x0 + xstep;
			bigview.y1 = bigview.y0 + ystep;
			bbox = fz_bbox_covering_rect(fz_transform_rect(ctm, bigview));
			tl.x = bbox.x0;
			tl.y = bbox.y0;
			tl = fz_transform_point(invctm, tl);
			area.x0 -= fz_max(tl.x, 0); area.x1 += xstep - fz_max(tl.x, 0);
			area.y0 -= fz_max(tl.y, 0); area.y1 += ystep - fz_max(tl.y, 0);
		}
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
			fz_begin_tile(doc->dev, area, bigview, xstep, ystep, ctm);
			xps_paint_tiling_brush(doc, ctm, viewbox, tile_mode, &c);
			fz_end_tile(doc->dev);
		}
		else
		{
			int x, y;
			for (y = y0; y < y1; y++)
			{
				for (x = x0; x < x1; x++)
				{
					fz_matrix ttm = fz_concat(fz_translate(xstep * x, ystep * y), ctm);
					xps_paint_tiling_brush(doc, ttm, viewbox, tile_mode, &c);
				}
			}
		}
	}
	else
	{
		xps_paint_tiling_brush(doc, ctm, viewbox, tile_mode, &c);
	}

	xps_end_opacity(doc, base_uri, dict, opacity_att, NULL);
}

static void
xps_paint_visual_brush(xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, xml_element *root, void *visual_tag)
{
	xps_parse_element(doc, ctm, area, base_uri, dict, (xml_element *)visual_tag);
}

void
xps_parse_visual_brush(xps_document *doc, fz_matrix ctm, fz_rect area,
	char *base_uri, xps_resource *dict, xml_element *root)
{
	xml_element *node;

	char *visual_uri;
	char *visual_att;
	xml_element *visual_tag = NULL;

	visual_att = xml_att(root, "Visual");

	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "VisualBrush.Visual"))
			visual_tag = xml_down(node);
	}

	visual_uri = base_uri;
	xps_resolve_resource_reference(doc, dict, &visual_att, &visual_tag, &visual_uri);

	if (visual_tag)
	{
		xps_parse_tiling_brush(doc, ctm, area,
			visual_uri, dict, root, xps_paint_visual_brush, visual_tag);
	}
}

void
xps_parse_canvas(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *root)
{
	xps_resource *new_dict = NULL;
	xml_element *node;
	char *opacity_mask_uri;

	char *transform_att;
	char *clip_att;
	char *opacity_att;
	char *opacity_mask_att;
	char *navigate_uri_att;

	xml_element *transform_tag = NULL;
	xml_element *clip_tag = NULL;
	xml_element *opacity_mask_tag = NULL;

	fz_matrix transform;

	transform_att = xml_att(root, "RenderTransform");
	clip_att = xml_att(root, "Clip");
	opacity_att = xml_att(root, "Opacity");
	opacity_mask_att = xml_att(root, "OpacityMask");
	navigate_uri_att = xml_att(root, "FixedPage.NavigateUri");

	for (node = xml_down(root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "Canvas.Resources") && xml_down(node))
		{
			if (new_dict)
			{
				fz_warn(doc->ctx, "ignoring follow-up resource dictionaries");
			}
			else
			{
				new_dict = xps_parse_resource_dictionary(doc, base_uri, xml_down(node));
				if (new_dict)
				{
					new_dict->parent = dict;
					dict = new_dict;
				}
			}
		}

		if (!strcmp(xml_tag(node), "Canvas.RenderTransform"))
			transform_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "Canvas.Clip"))
			clip_tag = xml_down(node);
		if (!strcmp(xml_tag(node), "Canvas.OpacityMask"))
			opacity_mask_tag = xml_down(node);
	}

	opacity_mask_uri = base_uri;
	xps_resolve_resource_reference(doc, dict, &transform_att, &transform_tag, NULL);
	xps_resolve_resource_reference(doc, dict, &clip_att, &clip_tag, NULL);
	xps_resolve_resource_reference(doc, dict, &opacity_mask_att, &opacity_mask_tag, &opacity_mask_uri);

	transform = fz_identity;
	if (transform_att)
		xps_parse_render_transform(doc, transform_att, &transform);
	if (transform_tag)
		xps_parse_matrix_transform(doc, transform_tag, &transform);
	ctm = fz_concat(transform, ctm);

	/* SumatraPDF: extended link support */
	xps_extract_anchor_info(doc, fz_empty_rect, navigate_uri_att, NULL, 1);
	navigate_uri_att = NULL;

	if (navigate_uri_att)
		xps_add_link(doc, area, base_uri, navigate_uri_att);

	if (clip_att || clip_tag)
		xps_clip(doc, ctm, dict, clip_att, clip_tag);

	xps_begin_opacity(doc, ctm, area, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);

	for (node = xml_down(root); node; node = xml_next(node))
	{
		xps_parse_element(doc, ctm, area, base_uri, dict, node);
	}

	xps_end_opacity(doc, opacity_mask_uri, dict, opacity_att, opacity_mask_tag);

	/* SumatraPDF: extended link support */
	xps_extract_anchor_info(doc, area, NULL, xml_att(root, "Name"), 2);

	if (clip_att || clip_tag)
		fz_pop_clip(doc->dev);

	if (new_dict)
		xps_free_resource_dictionary(doc, new_dict);
}

void
xps_parse_fixed_page(xps_document *doc, fz_matrix ctm, xps_page *page)
{
	xml_element *node;
	xps_resource *dict;
	char base_uri[1024];
	fz_rect area;
	char *s;

	fz_strlcpy(base_uri, page->name, sizeof base_uri);
	s = strrchr(base_uri, '/');
	if (s)
		s[1] = 0;

	dict = NULL;

	doc->opacity_top = 0;
	doc->opacity[0] = 1;

	if (!page->root)
		return;

	area = fz_transform_rect(fz_scale(page->width, page->height), fz_unit_rect);

	for (node = xml_down(page->root); node; node = xml_next(node))
	{
		if (!strcmp(xml_tag(node), "FixedPage.Resources") && xml_down(node))
		{
			if (dict)
				fz_warn(doc->ctx, "ignoring follow-up resource dictionaries");
			else
				dict = xps_parse_resource_dictionary(doc, base_uri, xml_down(node));
		}
		xps_parse_element(doc, ctm, area, base_uri, dict, node);
	}

	if (dict)
		xps_free_resource_dictionary(doc, dict);
}

void
xps_run_page(xps_document *doc, xps_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie)
{
	fz_matrix page_ctm;

	page_ctm = fz_scale(72.0f / 96.0f, 72.0f / 96.0f);
	ctm = fz_concat(page_ctm, ctm);

	doc->cookie = cookie;
	doc->dev = dev;
	xps_parse_fixed_page(doc, ctm, page);
	doc->cookie = NULL;
	doc->dev = NULL;
	page->links_resolved = 1;
}
