// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#include <string.h>

fz_device *
fz_new_device_of_size(fz_context *ctx, int size)
{
	fz_device *dev = Memento_label(fz_calloc(ctx, 1, size), "fz_device");
	dev->refs = 1;
	return dev;
}

static void
fz_disable_device(fz_context *ctx, fz_device *dev)
{
	dev->close_device = NULL;
	dev->fill_path = NULL;
	dev->stroke_path = NULL;
	dev->clip_path = NULL;
	dev->clip_stroke_path = NULL;
	dev->fill_text = NULL;
	dev->stroke_text = NULL;
	dev->clip_text = NULL;
	dev->clip_stroke_text = NULL;
	dev->ignore_text = NULL;
	dev->fill_shade = NULL;
	dev->fill_image = NULL;
	dev->fill_image_mask = NULL;
	dev->clip_image_mask = NULL;
	dev->pop_clip = NULL;
	dev->begin_mask = NULL;
	dev->end_mask = NULL;
	dev->begin_group = NULL;
	dev->end_group = NULL;
	dev->begin_tile = NULL;
	dev->end_tile = NULL;
	dev->render_flags = NULL;
	dev->set_default_colorspaces = NULL;
	dev->begin_layer = NULL;
	dev->end_layer = NULL;
	dev->begin_structure = NULL;
	dev->end_structure = NULL;
	dev->begin_metatext = NULL;
	dev->end_metatext = NULL;
}

void
fz_close_device(fz_context *ctx, fz_device *dev)
{
	if (dev == NULL)
		return;

	fz_try(ctx)
	{
		if (dev->close_device)
			dev->close_device(ctx, dev);
	}
	fz_always(ctx)
		fz_disable_device(ctx, dev);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

fz_device *
fz_keep_device(fz_context *ctx, fz_device *dev)
{
	return fz_keep_imp(ctx, dev, &dev->refs);
}

void
fz_drop_device(fz_context *ctx, fz_device *dev)
{
	if (fz_drop_imp(ctx, dev, &dev->refs))
	{
		if (dev->close_device)
			fz_warn(ctx, "dropping unclosed device");
		if (dev->drop_device)
			dev->drop_device(ctx, dev);
		fz_free(ctx, dev->container);
		fz_free(ctx, dev);
	}
}

void
fz_enable_device_hints(fz_context *ctx, fz_device *dev, int hints)
{
	dev->hints |= hints;
}

void
fz_disable_device_hints(fz_context *ctx, fz_device *dev, int hints)
{
	dev->hints &= ~hints;
}

static void
push_clip_stack(fz_context *ctx, fz_device *dev, fz_rect rect, int type)
{
	if (dev->container_len == dev->container_cap)
	{
		int newmax = dev->container_cap * 2;
		if (newmax == 0)
			newmax = 4;
		dev->container = fz_realloc_array(ctx, dev->container, newmax, fz_device_container_stack);
		dev->container_cap = newmax;
	}
	if (dev->container_len == 0)
		dev->container[0].scissor = rect;
	else
	{
		dev->container[dev->container_len].scissor = fz_intersect_rect(dev->container[dev->container_len-1].scissor, rect);
	}
	dev->container[dev->container_len].type = type;
	dev->container[dev->container_len].user = 0;
	dev->container_len++;
}

static void
pop_clip_stack(fz_context *ctx, fz_device *dev, int type)
{
	if (dev->container_len == 0 || dev->container[dev->container_len-1].type != type)
	{
		fz_disable_device(ctx, dev);
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "device calls unbalanced");
	}
	dev->container_len--;
}

static void
pop_push_clip_stack(fz_context *ctx, fz_device *dev, int pop_type, int push_type)
{
	if (dev->container_len == 0 || dev->container[dev->container_len-1].type != pop_type)
	{
		fz_disable_device(ctx, dev);
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "device calls unbalanced");
	}
	dev->container[dev->container_len-1].type = push_type;
}

void
fz_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->fill_path)
	{
		fz_try(ctx)
			dev->fill_path(ctx, dev, path, even_odd, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->stroke_path)
	{
		fz_try(ctx)
			dev->stroke_path(ctx, dev, path, stroke, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_bound_path(ctx, path, NULL, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_path)
	{
		fz_try(ctx)
			dev->clip_path(ctx, dev, path, even_odd, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_bound_path(ctx, path, stroke, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_stroke_path)
	{
		fz_try(ctx)
			dev->clip_stroke_path(ctx, dev, path, stroke, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->fill_text)
	{
		fz_try(ctx)
			dev->fill_text(ctx, dev, text, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->stroke_text)
	{
		fz_try(ctx)
			dev->stroke_text(ctx, dev, text, stroke, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_bound_text(ctx, text, NULL, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_text)
	{
		fz_try(ctx)
			dev->clip_text(ctx, dev, text, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_bound_text(ctx, text, stroke, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_stroke_text)
	{
		fz_try(ctx)
			dev->clip_stroke_text(ctx, dev, text, stroke, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	if (dev->ignore_text)
	{
		fz_try(ctx)
			dev->ignore_text(ctx, dev, text, ctm);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_pop_clip(fz_context *ctx, fz_device *dev)
{
	pop_clip_stack(ctx, dev, fz_device_container_stack_is_clip);

	if (dev->pop_clip)
	{
		fz_try(ctx)
			dev->pop_clip(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	if (dev->fill_shade)
	{
		fz_try(ctx)
			dev->fill_shade(ctx, dev, shade, ctm, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_fill_image(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	if (image->colorspace == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "argument to fill image must be a color image");
	if (dev->fill_image)
	{
		fz_try(ctx)
			dev->fill_image(ctx, dev, image, ctm, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	if (dev->fill_image_mask)
	{
		fz_try(ctx)
			dev->fill_image_mask(ctx, dev, image, ctm, colorspace, color, alpha, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, fz_rect scissor)
{
	fz_rect bbox = fz_transform_rect(fz_unit_rect, ctm);
	bbox = fz_intersect_rect(bbox, scissor);
	push_clip_stack(ctx, dev, bbox, fz_device_container_stack_is_clip);

	if (dev->clip_image_mask)
	{
		fz_try(ctx)
			dev->clip_image_mask(ctx, dev, image, ctm, scissor);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_begin_mask(fz_context *ctx, fz_device *dev, fz_rect area, int luminosity, fz_colorspace *colorspace, const float *bc, fz_color_params color_params)
{
	push_clip_stack(ctx, dev, area, fz_device_container_stack_is_mask);

	if (dev->begin_mask)
	{
		fz_try(ctx)
			dev->begin_mask(ctx, dev, area, luminosity, colorspace, bc, color_params);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_end_mask(fz_context *ctx, fz_device *dev)
{
	pop_push_clip_stack(ctx, dev, fz_device_container_stack_is_mask, fz_device_container_stack_is_clip);

	if (dev->end_mask)
	{
		fz_try(ctx)
			dev->end_mask(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_begin_group(fz_context *ctx, fz_device *dev, fz_rect area, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	push_clip_stack(ctx, dev, area, fz_device_container_stack_is_group);

	if (dev->begin_group)
	{
		fz_try(ctx)
			dev->begin_group(ctx, dev, area, cs, isolated, knockout, blendmode, alpha);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_end_group(fz_context *ctx, fz_device *dev)
{
	pop_clip_stack(ctx, dev, fz_device_container_stack_is_group);

	if (dev->end_group)
	{
		fz_try(ctx)
			dev->end_group(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	(void)fz_begin_tile_id(ctx, dev, area, view, xstep, ystep, ctm, 0);
}

int
fz_begin_tile_id(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	int result = 0;

	push_clip_stack(ctx, dev, area, fz_device_container_stack_is_tile);

	if (xstep < 0)
		xstep = -xstep;
	if (ystep < 0)
		ystep = -ystep;
	if (dev->begin_tile)
	{
		fz_try(ctx)
			result = dev->begin_tile(ctx, dev, area, view, xstep, ystep, ctm, id);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}

	return result;
}

void
fz_end_tile(fz_context *ctx, fz_device *dev)
{
	pop_clip_stack(ctx, dev, fz_device_container_stack_is_tile);

	if (dev->end_tile)
	{
		fz_try(ctx)
			dev->end_tile(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_render_flags(fz_context *ctx, fz_device *dev, int set, int clear)
{
	if (dev->render_flags)
	{
		fz_try(ctx)
			dev->render_flags(ctx, dev, set, clear);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void
fz_set_default_colorspaces(fz_context *ctx, fz_device *dev, fz_default_colorspaces *default_cs)
{
	if (dev->set_default_colorspaces)
	{
		fz_try(ctx)
			dev->set_default_colorspaces(ctx, dev, default_cs);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void fz_begin_layer(fz_context *ctx, fz_device *dev, const char *layer_name)
{
	if (dev->begin_layer)
	{
		fz_try(ctx)
			dev->begin_layer(ctx, dev, layer_name);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void fz_end_layer(fz_context *ctx, fz_device *dev)
{
	if (dev->end_layer)
	{
		fz_try(ctx)
			dev->end_layer(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void fz_begin_structure(fz_context *ctx, fz_device *dev, fz_structure str, const char *raw, int uid)
{
	if (dev->begin_structure)
	{
		fz_try(ctx)
			dev->begin_structure(ctx, dev, str, raw, uid);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void fz_end_structure(fz_context *ctx, fz_device *dev)
{
	if (dev->end_structure)
	{
		fz_try(ctx)
			dev->end_structure(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void fz_begin_metatext(fz_context *ctx, fz_device *dev, fz_metatext meta, const char *meta_text)
{
	if (dev->begin_metatext)
	{
		fz_try(ctx)
			dev->begin_metatext(ctx, dev, meta, meta_text);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

void fz_end_metatext(fz_context *ctx, fz_device *dev)
{
	if (dev->end_metatext)
	{
		fz_try(ctx)
			dev->end_metatext(ctx, dev);
		fz_catch(ctx)
		{
			fz_disable_device(ctx, dev);
			fz_rethrow(ctx);
		}
	}
}

fz_rect
fz_device_current_scissor(fz_context *ctx, fz_device *dev)
{
	if (dev->container_len > 0)
		return dev->container[dev->container_len-1].scissor;
	return fz_infinite_rect;
}

const char *
fz_structure_to_string(fz_structure type)
{
	switch (type)
	{
	default:
		return "Invalid";
	case FZ_STRUCTURE_DOCUMENT:
		return "Document";
	case FZ_STRUCTURE_PART:
		return "Part";
	case FZ_STRUCTURE_ART:
		return "Art";
	case FZ_STRUCTURE_SECT:
		return "Sect";
	case FZ_STRUCTURE_DIV:
		return "Div";
	case FZ_STRUCTURE_BLOCKQUOTE:
		return "BlockQuote";
	case FZ_STRUCTURE_CAPTION:
		return "Caption";
	case FZ_STRUCTURE_TOC:
		return "TOC";
	case FZ_STRUCTURE_TOCI:
		return "TOCI";
	case FZ_STRUCTURE_INDEX:
		return "Index";
	case FZ_STRUCTURE_NONSTRUCT:
		return "NonDtruct";
	case FZ_STRUCTURE_PRIVATE:
		return "Private";
	/* Grouping elements (PDF 2.0 - Table 364) */
	case FZ_STRUCTURE_DOCUMENTFRAGMENT:
		return "DocumentFragment";
	/* Grouping elements (PDF 2.0 - Table 365) */
	case FZ_STRUCTURE_ASIDE:
		return "Aside";
	/* Grouping elements (PDF 2.0 - Table 366) */
	case FZ_STRUCTURE_TITLE:
		return "Title";
	case FZ_STRUCTURE_FENOTE:
		return "FENote";
	/* Grouping elements (PDF 2.0 - Table 367) */
	case FZ_STRUCTURE_SUB:
		return "Sub";

	/* Paragraphlike elements (PDF 1.7 - Table 10.21) */
	case FZ_STRUCTURE_P:
		return "P";
	case FZ_STRUCTURE_H:
		return "H";
	case FZ_STRUCTURE_H1:
		return "H1";
	case FZ_STRUCTURE_H2:
		return "H2";
	case FZ_STRUCTURE_H3:
		return "H3";
	case FZ_STRUCTURE_H4:
		return "H4";
	case FZ_STRUCTURE_H5:
		return "H5";
	case FZ_STRUCTURE_H6:
		return "H6";

	/* List elements (PDF 1.7 - Table 10.23) */
	case FZ_STRUCTURE_LIST:
		return "List";
	case FZ_STRUCTURE_LISTITEM:
		return "LI";
	case FZ_STRUCTURE_LABEL:
		return "Lbl";
	case FZ_STRUCTURE_LISTBODY:
		return "LBody";

	/* Table elements (PDF 1.7 - Table 10.24) */
	case FZ_STRUCTURE_TABLE:
		return "Table";
	case FZ_STRUCTURE_TR:
		return "TR";
	case FZ_STRUCTURE_TH:
		return "TH";
	case FZ_STRUCTURE_TD:
		return "TD";
	case FZ_STRUCTURE_THEAD:
		return "THead";
	case FZ_STRUCTURE_TBODY:
		return "TBody";
	case FZ_STRUCTURE_TFOOT:
		return "TFoot";

	/* Inline elements (PDF 1.7 - Table 10.25) */
	case FZ_STRUCTURE_SPAN:
		return "Span";
	case FZ_STRUCTURE_QUOTE:
		return "Quote";
	case FZ_STRUCTURE_NOTE:
		return "Note";
	case FZ_STRUCTURE_REFERENCE:
		return "Reference";
	case FZ_STRUCTURE_BIBENTRY:
		return "BibEntry";
	case FZ_STRUCTURE_CODE:
		return "Code";
	case FZ_STRUCTURE_LINK:
		return "Link";
	case FZ_STRUCTURE_ANNOT:
		return "Annot";
	/* Inline elements (PDF 2.0 - Table 368) */
	case FZ_STRUCTURE_EM:
		return "Em";
	case FZ_STRUCTURE_STRONG:
		return "Strong";

	/* Ruby inline element (PDF 1.7 - Table 10.26) */
	case FZ_STRUCTURE_RUBY:
		return "Ruby";
	case FZ_STRUCTURE_RB:
		return "RB";
	case FZ_STRUCTURE_RT:
		return "RT";
	case FZ_STRUCTURE_RP:
		return "RP";

	/* Warichu inline element (PDF 1.7 - Table 10.26) */
	case FZ_STRUCTURE_WARICHU:
		return "Warichu";
	case FZ_STRUCTURE_WT:
		return "WT";
	case FZ_STRUCTURE_WP:
		return "WP";

	/* Illustration elements (PDF 1.7 - Table 10.27) */
	case FZ_STRUCTURE_FIGURE:
		return "Figure";
	case FZ_STRUCTURE_FORMULA:
		return "Formula";
	case FZ_STRUCTURE_FORM:
		return "Form";

	/* Artifact structure type (PDF 2.0 - Table 375) */
	case FZ_STRUCTURE_ARTIFACT:
		return "Artifact";
	}

	return NULL;
}

fz_structure
fz_structure_from_string(const char *str)
{
	if (!strcmp(str, "Document")) return FZ_STRUCTURE_DOCUMENT;
	if (!strcmp(str, "Part")) return FZ_STRUCTURE_PART;
	if (!strcmp(str, "Art")) return FZ_STRUCTURE_ART;
	if (!strcmp(str, "Sect")) return FZ_STRUCTURE_SECT;
	if (!strcmp(str, "Div")) return FZ_STRUCTURE_DIV;
	if (!strcmp(str, "BlockQuote")) return FZ_STRUCTURE_BLOCKQUOTE;
	if (!strcmp(str, "Caption")) return FZ_STRUCTURE_CAPTION;
	if (!strcmp(str, "TOC")) return FZ_STRUCTURE_TOC;
	if (!strcmp(str, "TOCI")) return FZ_STRUCTURE_TOCI;
	if (!strcmp(str, "Index")) return FZ_STRUCTURE_INDEX;
	if (!strcmp(str, "NonStruct")) return FZ_STRUCTURE_NONSTRUCT;
	if (!strcmp(str, "Private")) return FZ_STRUCTURE_PRIVATE;
	if (!strcmp(str, "P")) return FZ_STRUCTURE_P;
	if (!strcmp(str, "H")) return FZ_STRUCTURE_H;
	if (!strcmp(str, "H1")) return FZ_STRUCTURE_H1;
	if (!strcmp(str, "H2")) return FZ_STRUCTURE_H2;
	if (!strcmp(str, "H3")) return FZ_STRUCTURE_H3;
	if (!strcmp(str, "H4")) return FZ_STRUCTURE_H4;
	if (!strcmp(str, "H5")) return FZ_STRUCTURE_H5;
	if (!strcmp(str, "H6")) return FZ_STRUCTURE_H6;
	if (!strcmp(str, "L")) return FZ_STRUCTURE_LIST;
	if (!strcmp(str, "LI")) return FZ_STRUCTURE_LISTITEM;
	if (!strcmp(str, "Lbl")) return FZ_STRUCTURE_LABEL;
	if (!strcmp(str, "LBody")) return FZ_STRUCTURE_LISTBODY;
	if (!strcmp(str, "Table")) return FZ_STRUCTURE_TABLE;
	if (!strcmp(str, "TR")) return FZ_STRUCTURE_TR;
	if (!strcmp(str, "TH")) return FZ_STRUCTURE_TH;
	if (!strcmp(str, "TD")) return FZ_STRUCTURE_TD;
	if (!strcmp(str, "THead")) return FZ_STRUCTURE_THEAD;
	if (!strcmp(str, "TBody")) return FZ_STRUCTURE_TBODY;
	if (!strcmp(str, "TFoot")) return FZ_STRUCTURE_TFOOT;
	if (!strcmp(str, "Span")) return FZ_STRUCTURE_SPAN;
	if (!strcmp(str, "Quote")) return FZ_STRUCTURE_QUOTE;
	if (!strcmp(str, "Note")) return FZ_STRUCTURE_NOTE;
	if (!strcmp(str, "Reference")) return FZ_STRUCTURE_REFERENCE;
	if (!strcmp(str, "BibEntry")) return FZ_STRUCTURE_BIBENTRY;
	if (!strcmp(str, "Code")) return FZ_STRUCTURE_CODE;
	if (!strcmp(str, "Link")) return FZ_STRUCTURE_LINK;
	if (!strcmp(str, "Annot")) return FZ_STRUCTURE_ANNOT;
	if (!strcmp(str, "Ruby")) return FZ_STRUCTURE_RUBY;
	if (!strcmp(str, "RB")) return FZ_STRUCTURE_RB;
	if (!strcmp(str, "RT")) return FZ_STRUCTURE_RT;
	if (!strcmp(str, "RP")) return FZ_STRUCTURE_RP;
	if (!strcmp(str, "Warichu")) return FZ_STRUCTURE_WARICHU;
	if (!strcmp(str, "WT")) return FZ_STRUCTURE_WT;
	if (!strcmp(str, "WP")) return FZ_STRUCTURE_WP;
	if (!strcmp(str, "Figure")) return FZ_STRUCTURE_FIGURE;
	if (!strcmp(str, "Formula")) return FZ_STRUCTURE_FORMULA;
	if (!strcmp(str, "Form")) return FZ_STRUCTURE_FORM;
	return FZ_STRUCTURE_INVALID;
}
