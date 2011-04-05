#include "fitz.h"

enum { ISOLATED = 1, KNOCKOUT = 2 };

static fz_display_node *
fz_new_display_node(fz_display_command cmd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_display_node *node;
	int i;

	node = fz_malloc(sizeof(fz_display_node));
	node->cmd = cmd;
	node->next = NULL;
	node->rect = fz_empty_rect;
	node->item.path = NULL;
	node->stroke = NULL;
	node->flag = 0;
	node->ctm = ctm;
	if (colorspace)
	{
		node->colorspace = fz_keep_colorspace(colorspace);
		if (color)
		{
			for (i = 0; i < node->colorspace->n; i++)
				node->color[i] = color[i];
		}
	}
	else
	{
		node->colorspace = NULL;
	}
	node->alpha = alpha;

	return node;
}

static fz_stroke_state *
fz_clone_stroke_state(fz_stroke_state *stroke)
{
	fz_stroke_state *newstroke = fz_malloc(sizeof(fz_stroke_state));
	*newstroke = *stroke;
	return newstroke;
}

static void
fz_append_display_node(fz_display_list *list, fz_display_node *node)
{
	if (!list->first)
	{
		list->first = node;
		list->last = node;
	}
	else
	{
		list->last->next = node;
		list->last = node;
	}
}

static void
fz_free_display_node(fz_display_node *node)
{
	switch (node->cmd)
	{
	case FZ_CMD_FILL_PATH:
	case FZ_CMD_STROKE_PATH:
	case FZ_CMD_CLIP_PATH:
	case FZ_CMD_CLIP_STROKE_PATH:
		fz_free_path(node->item.path);
		break;
	case FZ_CMD_FILL_TEXT:
	case FZ_CMD_STROKE_TEXT:
	case FZ_CMD_CLIP_TEXT:
	case FZ_CMD_CLIP_STROKE_TEXT:
	case FZ_CMD_IGNORE_TEXT:
		fz_free_text(node->item.text);
		break;
	case FZ_CMD_FILL_SHADE:
		fz_drop_shade(node->item.shade);
		break;
	case FZ_CMD_FILL_IMAGE:
	case FZ_CMD_FILL_IMAGE_MASK:
	case FZ_CMD_CLIP_IMAGE_MASK:
		fz_drop_pixmap(node->item.image);
		break;
	case FZ_CMD_POP_CLIP:
	case FZ_CMD_BEGIN_MASK:
	case FZ_CMD_END_MASK:
	case FZ_CMD_BEGIN_GROUP:
	case FZ_CMD_END_GROUP:
	case FZ_CMD_BEGIN_TILE:
	case FZ_CMD_END_TILE:
		break;
	}
	if (node->stroke)
		fz_free(node->stroke);
	if (node->colorspace)
		fz_drop_colorspace(node->colorspace);
	fz_free(node);
}

static void
fz_list_fill_path(void *user, fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_FILL_PATH, ctm, colorspace, color, alpha);
	node->rect = fz_bound_path(path, NULL, ctm);
	node->item.path = fz_clone_path(path);
	node->flag = even_odd;
	fz_append_display_node(user, node);
}

static void
fz_list_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_STROKE_PATH, ctm, colorspace, color, alpha);
	node->rect = fz_bound_path(path, stroke, ctm);
	node->item.path = fz_clone_path(path);
	node->stroke = fz_clone_stroke_state(stroke);
	fz_append_display_node(user, node);
}

static void
fz_list_clip_path(void *user, fz_path *path, int even_odd, fz_matrix ctm)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_CLIP_PATH, ctm, NULL, NULL, 0);
	node->rect = fz_bound_path(path, NULL, ctm);
	node->item.path = fz_clone_path(path);
	node->flag = even_odd;
	fz_append_display_node(user, node);
}

static void
fz_list_clip_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_CLIP_STROKE_PATH, ctm, NULL, NULL, 0);
	node->rect = fz_bound_path(path, stroke, ctm);
	node->item.path = fz_clone_path(path);
	node->stroke = fz_clone_stroke_state(stroke);
	fz_append_display_node(user, node);
}

static void
fz_list_fill_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_FILL_TEXT, ctm, colorspace, color, alpha);
	node->rect = fz_bound_text(text, ctm);
	node->item.text = fz_clone_text(text);
	fz_append_display_node(user, node);
}

static void
fz_list_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_STROKE_TEXT, ctm, colorspace, color, alpha);
	node->rect = fz_bound_text(text, ctm);
	node->item.text = fz_clone_text(text);
	node->stroke = fz_clone_stroke_state(stroke);
	fz_append_display_node(user, node);
}

static void
fz_list_clip_text(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_CLIP_TEXT, ctm, NULL, NULL, 0);
	node->rect = fz_bound_text(text, ctm);
	node->item.text = fz_clone_text(text);
	node->flag = accumulate;
	/* when accumulating, be conservative about culling */
	if (accumulate)
		node->rect = fz_infinite_rect;
	fz_append_display_node(user, node);
}

static void
fz_list_clip_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_CLIP_STROKE_TEXT, ctm, NULL, NULL, 0);
	node->rect = fz_bound_text(text, ctm);
	node->item.text = fz_clone_text(text);
	node->stroke = fz_clone_stroke_state(stroke);
	fz_append_display_node(user, node);
}

static void
fz_list_ignore_text(void *user, fz_text *text, fz_matrix ctm)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_IGNORE_TEXT, ctm, NULL, NULL, 0);
	node->rect = fz_bound_text(text, ctm);
	node->item.text = fz_clone_text(text);
	fz_append_display_node(user, node);
}

static void
fz_list_pop_clip(void *user)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_POP_CLIP, fz_identity, NULL, NULL, 0);
	/* TODO: scan back for matching pushclip and calculate bbox of contents */
	fz_append_display_node(user, node);
}

static void
fz_list_fill_shade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_FILL_SHADE, ctm, NULL, NULL, alpha);
	node->rect = fz_bound_shade(shade, ctm);
	node->item.shade = fz_keep_shade(shade);
	fz_append_display_node(user, node);
}

static void
fz_list_fill_image(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_FILL_IMAGE, ctm, NULL, NULL, alpha);
	node->rect = fz_transform_rect(ctm, fz_unit_rect);
	node->item.image = fz_keep_pixmap(image);
	fz_append_display_node(user, node);
}

static void
fz_list_fill_image_mask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_FILL_IMAGE_MASK, ctm, colorspace, color, alpha);
	node->rect = fz_transform_rect(ctm, fz_unit_rect);
	node->item.image = fz_keep_pixmap(image);
	fz_append_display_node(user, node);
}

static void
fz_list_clip_image_mask(void *user, fz_pixmap *image, fz_matrix ctm)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_CLIP_IMAGE_MASK, ctm, NULL, NULL, 0);
	node->rect = fz_transform_rect(ctm, fz_unit_rect);
	node->item.image = fz_keep_pixmap(image);
	fz_append_display_node(user, node);
}

static void
fz_list_begin_mask(void *user, fz_rect rect, int luminosity, fz_colorspace *colorspace, float *color)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_BEGIN_MASK, fz_identity, colorspace, color, 0);
	node->rect = rect;
	node->flag = luminosity;
	fz_append_display_node(user, node);
}

static void
fz_list_end_mask(void *user)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_END_MASK, fz_identity, NULL, NULL, 0);
	fz_append_display_node(user, node);
}

static void
fz_list_begin_group(void *user, fz_rect rect, int isolated, int knockout, fz_blendmode blendmode, float alpha)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_BEGIN_GROUP, fz_identity, NULL, NULL, alpha);
	node->rect = rect;
	node->item.blendmode = blendmode;
	node->flag |= isolated ? ISOLATED : 0;
	node->flag |= knockout ? KNOCKOUT : 0;
	fz_append_display_node(user, node);
}

static void
fz_list_end_group(void *user)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_END_GROUP, fz_identity, NULL, NULL, 0);
	fz_append_display_node(user, node);
}

static void
fz_list_begin_tile(void *user, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_BEGIN_TILE, ctm, NULL, NULL, 0);
	node->rect = area;
	node->color[0] = xstep;
	node->color[1] = ystep;
	node->color[2] = view.x0;
	node->color[3] = view.y0;
	node->color[4] = view.x1;
	node->color[5] = view.y1;
	fz_append_display_node(user, node);
}

static void
fz_list_end_tile(void *user)
{
	fz_display_node *node;
	node = fz_new_display_node(FZ_CMD_END_TILE, fz_identity, NULL, NULL, 0);
	fz_append_display_node(user, node);
}

fz_device *
fz_new_list_device(fz_display_list *list)
{
	fz_device *dev = fz_new_device(list);

	dev->fill_path = fz_list_fill_path;
	dev->stroke_path = fz_list_stroke_path;
	dev->clip_path = fz_list_clip_path;
	dev->clip_stroke_path = fz_list_clip_stroke_path;

	dev->fill_text = fz_list_fill_text;
	dev->stroke_text = fz_list_stroke_text;
	dev->clip_text = fz_list_clip_text;
	dev->clip_stroke_text = fz_list_clip_stroke_text;
	dev->ignore_text = fz_list_ignore_text;

	dev->fill_shade = fz_list_fill_shade;
	dev->fill_image = fz_list_fill_image;
	dev->fill_image_mask = fz_list_fill_image_mask;
	dev->clip_image_mask = fz_list_clip_image_mask;

	dev->pop_clip = fz_list_pop_clip;

	dev->begin_mask = fz_list_begin_mask;
	dev->end_mask = fz_list_end_mask;
	dev->begin_group = fz_list_begin_group;
	dev->end_group = fz_list_end_group;

	dev->begin_tile = fz_list_begin_tile;
	dev->end_tile = fz_list_end_tile;

	return dev;
}

fz_display_list *
fz_new_display_list(void)
{
	fz_display_list *list = fz_malloc(sizeof(fz_display_list));
	list->first = NULL;
	list->last = NULL;
	return list;
}

void
fz_free_display_list(fz_display_list *list)
{
	fz_display_node *node = list->first;
	while (node)
	{
		fz_display_node *next = node->next;
		fz_free_display_node(node);
		node = next;
	}
	fz_free(list);
}

void
fz_execute_display_list(fz_display_list *list, fz_device *dev, fz_matrix top_ctm, fz_bbox bounds)
{
	fz_display_node *node;
	fz_rect bbox;
	int clipped = 0;
	int tiled = 0;

	if (!fz_is_infinite_bbox(bounds))
	{
		/* add some fuzz at the edges, as especially glyph rects
		 * are sometimes not actually completely bounding the glyph */
		bounds.x0 -= 20; bounds.y0 -= 20;
		bounds.x1 += 20; bounds.y1 += 20;
	}

	for (node = list->first; node; node = node->next)
	{
		fz_matrix ctm = fz_concat(node->ctm, top_ctm);
		fz_rect rect = fz_transform_rect(top_ctm, node->rect);

		/* never skip tiles */
		if (tiled)
			goto visible;

		/* cull objects to draw using a quick visibility test */
		if (clipped || fz_is_empty_bbox(fz_intersect_bbox(fz_round_rect(rect), bounds)))
		{
			switch (node->cmd)
			{
			case FZ_CMD_BEGIN_TILE:
				tiled++;
				goto visible;
			case FZ_CMD_END_TILE:
				tiled--;
				goto visible;
			case FZ_CMD_CLIP_PATH:
			case FZ_CMD_CLIP_STROKE_PATH:
			case FZ_CMD_CLIP_TEXT:
			case FZ_CMD_CLIP_STROKE_TEXT:
			case FZ_CMD_CLIP_IMAGE_MASK:
			case FZ_CMD_BEGIN_MASK:
			case FZ_CMD_BEGIN_GROUP:
				clipped++;
				continue;
			case FZ_CMD_POP_CLIP:
			case FZ_CMD_END_GROUP:
				if (!clipped)
					goto visible;
				clipped--;
				continue;
			case FZ_CMD_END_MASK:
				if (!clipped)
					goto visible;
				continue;
			default:
				continue;
			}
		}

visible:
		switch (node->cmd)
		{
		case FZ_CMD_FILL_PATH:
			fz_fill_path(dev, node->item.path, node->flag, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMD_STROKE_PATH:
			fz_stroke_path(dev, node->item.path, node->stroke, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMD_CLIP_PATH:
			fz_clip_path(dev, node->item.path, node->flag, ctm);
			break;
		case FZ_CMD_CLIP_STROKE_PATH:
			fz_clip_stroke_path(dev, node->item.path, node->stroke, ctm);
			break;
		case FZ_CMD_FILL_TEXT:
			fz_fill_text(dev, node->item.text, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMD_STROKE_TEXT:
			fz_stroke_text(dev, node->item.text, node->stroke, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMD_CLIP_TEXT:
			fz_clip_text(dev, node->item.text, ctm, node->flag);
			break;
		case FZ_CMD_CLIP_STROKE_TEXT:
			fz_clip_stroke_text(dev, node->item.text, node->stroke, ctm);
			break;
		case FZ_CMD_IGNORE_TEXT:
			fz_ignore_text(dev, node->item.text, ctm);
			break;
		case FZ_CMD_FILL_SHADE:
			fz_fill_shade(dev, node->item.shade, ctm, node->alpha);
			break;
		case FZ_CMD_FILL_IMAGE:
			fz_fill_image(dev, node->item.image, ctm, node->alpha);
			break;
		case FZ_CMD_FILL_IMAGE_MASK:
			fz_fill_image_mask(dev, node->item.image, ctm,
				node->colorspace, node->color, node->alpha);
			break;
		case FZ_CMD_CLIP_IMAGE_MASK:
			fz_clip_image_mask(dev, node->item.image, ctm);
			break;
		case FZ_CMD_POP_CLIP:
			fz_pop_clip(dev);
			break;
		case FZ_CMD_BEGIN_MASK:
			bbox = fz_transform_rect(top_ctm, node->rect);
			fz_begin_mask(dev, bbox, node->flag, node->colorspace, node->color);
			break;
		case FZ_CMD_END_MASK:
			fz_end_mask(dev);
			break;
		case FZ_CMD_BEGIN_GROUP:
			bbox = fz_transform_rect(top_ctm, node->rect);
			fz_begin_group(dev, bbox,
				node->flag & ISOLATED, node->flag & KNOCKOUT,
				node->item.blendmode, node->alpha);
			break;
		case FZ_CMD_END_GROUP:
			fz_end_group(dev);
			break;
		case FZ_CMD_BEGIN_TILE:
			bbox.x0 = node->color[2];
			bbox.y0 = node->color[3];
			bbox.x1 = node->color[4];
			bbox.y1 = node->color[5];
			fz_begin_tile(dev, node->rect, bbox,
				node->color[0], node->color[1], ctm);
			break;
		case FZ_CMD_END_TILE:
			fz_end_tile(dev);
			break;
		}
	}
}
