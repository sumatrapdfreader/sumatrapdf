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

#include "mupdf/fitz.h"

#include <float.h>

fz_display_list *
fz_new_display_list_from_page(fz_context *ctx, fz_page *page)
{
	fz_display_list *list;
	fz_device *dev = NULL;

	fz_var(dev);

	list = fz_new_display_list(ctx, fz_bound_page(ctx, page));
	fz_try(ctx)
	{
		dev = fz_new_list_device(ctx, list);
		fz_run_page(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_display_list(ctx, list);
		fz_rethrow(ctx);
	}

	return list;
}

fz_display_list *
fz_new_display_list_from_page_number(fz_context *ctx, fz_document *doc, int number)
{
	fz_page *page;
	fz_display_list *list = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		list = fz_new_display_list_from_page(ctx, page);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return list;
}

fz_display_list *
fz_new_display_list_from_page_contents(fz_context *ctx, fz_page *page)
{
	fz_display_list *list;
	fz_device *dev = NULL;

	fz_var(dev);

	list = fz_new_display_list(ctx, fz_bound_page(ctx, page));
	fz_try(ctx)
	{
		dev = fz_new_list_device(ctx, list);
		fz_run_page_contents(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_display_list(ctx, list);
		fz_rethrow(ctx);
	}

	return list;
}

fz_pixmap *
fz_new_pixmap_from_display_list(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, int alpha)
{
	return fz_new_pixmap_from_display_list_with_separations(ctx, list, ctm, cs, NULL, alpha);
}

fz_pixmap *
fz_new_pixmap_from_display_list_with_separations(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha)
{
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;

	rect = fz_bound_display_list(ctx, list);
	rect = fz_transform_rect(rect, ctm);
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, bbox, seps, alpha);
	if (alpha)
		fz_clear_pixmap(ctx, pix);
	else
		fz_clear_pixmap_with_value(ctx, pix, 0xFF);

	return fz_fill_pixmap_from_display_list(ctx, list, ctm, pix);
}

fz_pixmap *
fz_fill_pixmap_from_display_list(fz_context *ctx, fz_display_list *list, fz_matrix ctm, fz_pixmap *pix)
{
	fz_device *dev = NULL;

	fz_var(dev);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, ctm, pix);
		fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page_contents(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha)
{
	return fz_new_pixmap_from_page_contents_with_separations(ctx, page, ctm, cs, NULL, alpha);
}

fz_pixmap *
fz_new_pixmap_from_page_contents_with_separations(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha)
{
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;
	fz_device *dev = NULL;

	fz_var(dev);

	rect = fz_bound_page(ctx, page);
	rect = fz_transform_rect(rect, ctm);
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, bbox, seps, alpha);
	if (alpha)
		fz_clear_pixmap(ctx, pix);
	else
		fz_clear_pixmap_with_value(ctx, pix, 0xFF);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, ctm, pix);
		fz_run_page_contents(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, int alpha)
{
	return fz_new_pixmap_from_page_with_separations(ctx, page, ctm, cs, NULL, alpha);
}

fz_pixmap *
fz_new_pixmap_from_page_with_separations(fz_context *ctx, fz_page *page, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha)
{
	fz_rect rect;
	fz_irect bbox;
	fz_pixmap *pix;
	fz_device *dev = NULL;

	fz_var(dev);

	rect = fz_bound_page(ctx, page);
	rect = fz_transform_rect(rect, ctm);
	bbox = fz_round_rect(rect);

	pix = fz_new_pixmap_with_bbox(ctx, cs, bbox, seps, alpha);

	fz_try(ctx)
	{
		if (alpha)
			fz_clear_pixmap(ctx, pix);
		else
			fz_clear_pixmap_with_value(ctx, pix, 0xFF);

		dev = fz_new_draw_device(ctx, ctm, pix);
		fz_run_page(ctx, page, dev, fz_identity, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, pix);
		fz_rethrow(ctx);
	}

	return pix;
}

fz_pixmap *
fz_new_pixmap_from_page_number(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, int alpha)
{
	return fz_new_pixmap_from_page_number_with_separations(ctx, doc, number, ctm, cs, NULL, alpha);
}

fz_pixmap *
fz_new_pixmap_from_page_number_with_separations(fz_context *ctx, fz_document *doc, int number, fz_matrix ctm, fz_colorspace *cs, fz_separations *seps, int alpha)
{
	fz_page *page;
	fz_pixmap *pix = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		pix = fz_new_pixmap_from_page_with_separations(ctx, page, ctm, cs, seps, alpha);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return pix;
}

fz_stext_page *
fz_new_stext_page_from_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options)
{
	fz_stext_page *text;
	fz_device *dev = NULL;

	fz_var(dev);

	if (list == NULL)
		return NULL;

	text = fz_new_stext_page(ctx, fz_bound_display_list(ctx, list));
	fz_try(ctx)
	{
		dev = fz_new_stext_device(ctx, text, options);
		fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_stext_page(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

fz_stext_page *
fz_new_stext_page_from_page_with_cookie(fz_context *ctx, fz_page *page, const fz_stext_options *options, fz_cookie *cookie)
{
	fz_stext_page *text;
	fz_device *dev = NULL;

	fz_var(dev);

	if (page == NULL)
		return NULL;

	text = fz_new_stext_page(ctx, fz_bound_page(ctx, page));
	fz_try(ctx)
	{
		dev = fz_new_stext_device(ctx, text, options);
		fz_run_page_contents(ctx, page, dev, fz_identity, cookie);
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_drop_stext_page(ctx, text);
		fz_rethrow(ctx);
	}

	return text;
}

fz_stext_page *
fz_new_stext_page_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options)
{
	return fz_new_stext_page_from_page_with_cookie(ctx, page, options, NULL);
}

fz_stext_page *
fz_new_stext_page_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options)
{
	fz_page *page;
	fz_stext_page *text = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		text = fz_new_stext_page_from_page(ctx, page, options);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return text;
}

fz_stext_page *
fz_new_stext_page_from_chapter_page_number(fz_context *ctx, fz_document *doc, int chapter, int number, const fz_stext_options *options)
{
	fz_page *page;
	fz_stext_page *text = NULL;

	page = fz_load_chapter_page(ctx, doc, chapter, number);
	fz_try(ctx)
		text = fz_new_stext_page_from_page(ctx, page, options);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return text;
}

int
fz_search_display_list(fz_context *ctx, fz_display_list *list, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max)
{
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_display_list(ctx, list, NULL);
	fz_try(ctx)
		count = fz_search_stext_page(ctx, text, needle, hit_mark, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_display_list_cb(fz_context *ctx, fz_display_list *list, const char *needle, fz_search_callback_fn *cb, void *opaque)
{
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_display_list(ctx, list, NULL);
	fz_try(ctx)
		count = fz_search_stext_page_cb(ctx, text, needle, cb, opaque);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_page(fz_context *ctx, fz_page *page, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max)
{
	fz_stext_options opts = { FZ_STEXT_DEHYPHENATE };
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_page(ctx, page, &opts);
	fz_try(ctx)
		count = fz_search_stext_page(ctx, text, needle, hit_mark, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_page_cb(fz_context *ctx, fz_page *page, const char *needle, fz_search_callback_fn *cb, void *opaque)
{
	fz_stext_options opts = { FZ_STEXT_DEHYPHENATE };
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_page(ctx, page, &opts);
	fz_try(ctx)
		count = fz_search_stext_page_cb(ctx, text, needle, cb, opaque);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_page_number(fz_context *ctx, fz_document *doc, int number, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max)
{
	fz_page *page;
	int count = 0;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		count = fz_search_page(ctx, page, needle, hit_mark, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_page_number_cb(fz_context *ctx, fz_document *doc, int number, const char *needle, fz_search_callback_fn *cb, void *opaque)
{
	fz_page *page;
	int count = 0;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		count = fz_search_page_cb(ctx, page, needle, cb, opaque);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_chapter_page_number(fz_context *ctx, fz_document *doc, int chapter, int number, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max)
{
	fz_page *page;
	int count = 0;

	page = fz_load_chapter_page(ctx, doc, chapter, number);
	fz_try(ctx)
		count = fz_search_page(ctx, page, needle, hit_mark, hit_bbox, hit_max);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_search_chapter_page_number_cb(fz_context *ctx, fz_document *doc, int chapter, int number, const char *needle, fz_search_callback_fn *cb, void *opaque)
{
	fz_page *page;
	int count = 0;

	page = fz_load_chapter_page(ctx, doc, chapter, number);
	fz_try(ctx)
		count = fz_search_page_cb(ctx, page, needle, cb, opaque);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_match_display_list(fz_context *ctx, fz_display_list *list, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max, fz_search_options options)
{
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_display_list(ctx, list, NULL);
	fz_try(ctx)
		count = fz_match_stext_page(ctx, text, needle, hit_mark, hit_bbox, hit_max, options);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_match_display_list_cb(fz_context *ctx, fz_display_list *list, const char *needle, fz_match_callback_fn *cb, void *opaque, fz_search_options options)
{
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_display_list(ctx, list, NULL);
	fz_try(ctx)
		count = fz_match_stext_page_cb(ctx, text, needle, cb, opaque, options);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_match_page(fz_context *ctx, fz_page *page, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max, fz_search_options options)
{
	fz_stext_options opts = { FZ_STEXT_DEHYPHENATE };
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_page(ctx, page, &opts);
	fz_try(ctx)
		count = fz_match_stext_page(ctx, text, needle, hit_mark, hit_bbox, hit_max, options);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_match_page_cb(fz_context *ctx, fz_page *page, const char *needle, fz_match_callback_fn *cb, void *opaque, fz_search_options options)
{
	fz_stext_options opts = { FZ_STEXT_DEHYPHENATE };
	fz_stext_page *text;
	int count = 0;

	text = fz_new_stext_page_from_page(ctx, page, &opts);
	fz_try(ctx)
		count = fz_match_stext_page_cb(ctx, text, needle, cb, opaque, options);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_match_page_number(fz_context *ctx, fz_document *doc, int number, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max, fz_search_options options)
{
	fz_page *page;
	int count = 0;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		count = fz_match_page(ctx, page, needle, hit_mark, hit_bbox, hit_max, options);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_match_page_number_cb(fz_context *ctx, fz_document *doc, int number, const char *needle, fz_match_callback_fn *cb, void *opaque, fz_search_options options)
{
	fz_page *page;
	int count = 0;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		count = fz_match_page_cb(ctx, page, needle, cb, opaque, options);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_match_chapter_page_number(fz_context *ctx, fz_document *doc, int chapter, int number, const char *needle, int *hit_mark, fz_quad *hit_bbox, int hit_max, fz_search_options options)
{
	fz_page *page;
	int count = 0;

	page = fz_load_chapter_page(ctx, doc, chapter, number);
	fz_try(ctx)
		count = fz_match_page(ctx, page, needle, hit_mark, hit_bbox, hit_max, options);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

int
fz_match_chapter_page_number_cb(fz_context *ctx, fz_document *doc, int chapter, int number, const char *needle, fz_match_callback_fn *cb, void *opaque, fz_search_options options)
{
	fz_page *page;
	int count = 0;

	page = fz_load_chapter_page(ctx, doc, chapter, number);
	fz_try(ctx)
		count = fz_match_page_cb(ctx, page, needle, cb, opaque, options);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return count;
}

fz_buffer *
fz_new_buffer_from_stext_page(fz_context *ctx, fz_stext_page *page)
{
	return fz_new_buffer_from_flattened_stext_page(ctx, page, FZ_TEXT_FLATTEN_KEEP_PARAGRAPHS, NULL);
}

static int
inhibit_space_after_line_break(int c)
{
	return fz_is_unicode_whitespace(c) || fz_is_unicode_hyphen(c);
}

static int
do_flatten(fz_context *ctx, fz_buffer *buf, fz_stext_position **map, fz_stext_page *page, fz_stext_struct *parent, fz_stext_block *block, fz_text_flatten flatten, int *ws)
{
	fz_stext_line *line;
	fz_stext_char *ch;
	int n = 0;

	#define EMIT(X,Y) \
	{ \
		if (map && *map) *(*map)++ = (fz_stext_position){ page, parent, block, line, ch }; \
		if (buf) fz_append_rune(ctx, buf, Y); \
		++n; \
	}

	for (; block != NULL; block = block->next)
		{
			if (block->type == FZ_STEXT_BLOCK_TEXT)
			{
			int join_line = 0;
				for (line = block->u.t.first_line; line; line = line->next)
				{
				join_line = 0;
					for (ch = line->first_char; ch; ch = ch->next)
				{
					/* Last character of a line where we aren't keeping hyphens; check for dehyphenation. */
					if (ch == line->last_char && (flatten & FZ_TEXT_FLATTEN_KEEP_HYPHENS) == 0)
					{
						/* Soft hyphens are always removed. */
						if (ch->c == 0xad)
						{
							join_line = 1;
							continue;
						}
						/* Non-soft hyphens are only broken if we extracted with dehyphenation. */
						if ((line->flags & FZ_STEXT_LINE_FLAGS_JOINED) != 0 && fz_is_unicode_hyphen(ch->c))
						{
							join_line = 1;
							continue;
						}
					}

					/* Soft hyphens at the beginning or in the middle of a line are always removed. */
					if (ch != line->last_char && ch->c == 0xad)
					{
						continue;
					}

					EMIT(ch, ch->c);

					*ws = inhibit_space_after_line_break(ch->c);
				}

				if (join_line)
				{
					/* No whitespace, no linebreak. */
				}
				else if (flatten & FZ_TEXT_FLATTEN_KEEP_LINES)
				{
					EMIT(NULL, '\n');
					*ws = 1;
				}
				else
				{
					if (!*ws)
						EMIT(NULL, ' ');
					*ws = 1;
				}
			}

			if (flatten & FZ_TEXT_FLATTEN_KEEP_PARAGRAPHS)
			{
				EMIT(NULL, '\n');
				*ws = 1;
			}
			else if (!join_line)
			{
				EMIT(NULL, '\n');
				*ws = 1;
			}
		}
		else if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down)
		{
			n += do_flatten(ctx, buf, map, page, block->u.s.down, block->u.s.down->first_block, flatten, ws);
				}
			}

	return n;
}

fz_buffer *
fz_new_buffer_from_flattened_stext_page(fz_context *ctx, fz_stext_page *page, fz_text_flatten flatten, fz_stext_position **mapp)
{
	fz_stext_position *map = NULL;
	fz_buffer *buf = NULL;
	int ws, len;

	fz_var(map);
	fz_var(buf);

	if (mapp)
	{
		ws = 0;
		len = do_flatten(ctx, NULL, NULL, page, NULL, page->first_block, flatten, &ws);
		}

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 256);
		if (mapp)
			map = *mapp = fz_malloc_array(ctx, len, fz_stext_position);
		ws = 0;
		do_flatten(ctx, buf, &map, page, NULL, page->first_block, flatten, &ws);
	}
	fz_catch(ctx)
	{
		if (mapp)
			fz_free(ctx, *mapp);
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}

	return buf;
}

fz_buffer *
fz_new_buffer_from_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options)
{
	return fz_new_buffer_from_flattened_display_list(ctx, list, options, FZ_TEXT_FLATTEN_KEEP_PARAGRAPHS);
}

fz_buffer *
fz_new_buffer_from_flattened_display_list(fz_context *ctx, fz_display_list *list, const fz_stext_options *options, fz_text_flatten flatten)
{
	fz_stext_page *text;
	fz_buffer *buf = NULL;

	text = fz_new_stext_page_from_display_list(ctx, list, options);
	fz_try(ctx)
		buf = fz_new_buffer_from_flattened_stext_page(ctx, text, flatten, NULL);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return buf;
}

fz_buffer *
fz_new_buffer_from_page(fz_context *ctx, fz_page *page, const fz_stext_options *options)
{
	return fz_new_buffer_from_flattened_page(ctx, page, options, FZ_TEXT_FLATTEN_KEEP_PARAGRAPHS);
}

fz_buffer *
fz_new_buffer_from_flattened_page(fz_context *ctx, fz_page *page, const fz_stext_options *options, fz_text_flatten flatten)
{
	fz_stext_page *text;
	fz_buffer *buf = NULL;

	text = fz_new_stext_page_from_page(ctx, page, options);
	fz_try(ctx)
		buf = fz_new_buffer_from_flattened_stext_page(ctx, text, flatten, NULL);
	fz_always(ctx)
		fz_drop_stext_page(ctx, text);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return buf;
}

fz_buffer *
fz_new_buffer_from_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options)
{
	return fz_new_buffer_from_flattened_page_number(ctx, doc, number, options, FZ_TEXT_FLATTEN_KEEP_PARAGRAPHS);
}

fz_buffer *
fz_new_buffer_from_flattened_page_number(fz_context *ctx, fz_document *doc, int number, const fz_stext_options *options, fz_text_flatten flatten)
{
	fz_page *page;
	fz_buffer *buf = NULL;

	page = fz_load_page(ctx, doc, number);
	fz_try(ctx)
		buf = fz_new_buffer_from_flattened_page(ctx, page, options, flatten);
	fz_always(ctx)
		fz_drop_page(ctx, page);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return buf;
}

void
fz_write_image_as_data_uri(fz_context *ctx, fz_output *out, fz_image *image)
{
	fz_compressed_buffer *cbuf;
	fz_buffer *buf;

	cbuf = fz_compressed_image_buffer(ctx, image);

	if (cbuf && cbuf->params.type == FZ_IMAGE_JPEG)
	{
		int type = fz_colorspace_type(ctx, image->colorspace);
		if (type == FZ_COLORSPACE_GRAY || type == FZ_COLORSPACE_RGB)
		{
			fz_write_string(ctx, out, "data:image/jpeg;base64,");
			fz_write_base64_buffer(ctx, out, cbuf->buffer, 1);
			return;
		}
	}
	if (cbuf && cbuf->params.type == FZ_IMAGE_PNG)
	{
		fz_write_string(ctx, out, "data:image/png;base64,");
		fz_write_base64_buffer(ctx, out, cbuf->buffer, 1);
		return;
	}

	buf = fz_new_buffer_from_image_as_png(ctx, image, fz_default_color_params);
	fz_try(ctx)
	{
		fz_write_string(ctx, out, "data:image/png;base64,");
		fz_write_base64_buffer(ctx, out, buf, 1);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static uint32_t read16(const uint8_t *d, size_t *pos, size_t len, int order)
{
	size_t p = *pos;
	uint32_t v;

	if (p+1 >= len)
	{
		*pos = len;
		return 0;
	}

	if (order)
	{
		v = d[p++]<<8; /* BE */
		v |= d[p++];
	}
	else
	{
		v = d[p++]; /* LE */
		v |= d[p++]<<8;
	}

	*pos = p;

	return v;
}

static uint32_t read32(const uint8_t *d, size_t *pos, size_t len, int order)
{
	size_t p = *pos;
	uint32_t v;

	if (p+3 >= len)
	{
		*pos = len;
		return 0;
	}

	if (order)
	{
		v = d[p++]<<24; /* BE */
		v |= d[p++]<<16;
		v |= d[p++]<<8;
		v |= d[p++];
	}
	else
	{
		v = d[p++];
		v |= d[p++]<<8; /* LE */
		v |= d[p++]<<16;
		v |= d[p++]<<24;
	}

	*pos = p;

	return v;
}

static void write16(uint8_t *d, size_t *pos, size_t len, int order, uint32_t v)
{
	size_t p = *pos;

	if (p+1 >= len)
	{
		*pos = len;
		return;
	}

	if (order)
	{
		d[p++] = (v>>8);
		d[p++] = v;
	}
	else
	{
		d[p++] = v;
		d[p++] = (v>>8);
	}

	*pos = p;
}

static void write32( uint8_t *d, size_t *pos, size_t len, int order, uint32_t v)
{
	size_t p = *pos;

	if (p+3 >= len)
	{
		*pos = len;
		return;
	}

	if (order)
	{
		d[p++] = (v>>24);
		d[p++] = (v>>16);
		d[p++] = (v>>8);
		d[p++] = v;
	}
	else
	{
		d[p++] = v;
		d[p++] = (v>>8);
		d[p++] = (v>>16);
		d[p++] = (v>>24);
	}

	*pos = p;
}

fz_buffer *
fz_sanitize_jpeg_buffer(fz_context *ctx, fz_buffer *in)
{
	fz_buffer *out = fz_clone_buffer(ctx, in);
	size_t len = out->len;
	size_t pos = 0;
	uint8_t *d = out->data;

	/* We need at least 4 data bytes. */
	while (pos+4 < len)
	{
		uint8_t m;
		/* We should be on a marker. If not, inch forwards until we are. */
		if (d[pos++] != 0xff)
			continue;
		m = d[pos++];
		if (m == 0xDA)
			break; /* Start Of Scan. All our rewriting happens before this. */
		if (m == 0xE1)
		{
			uint8_t order;
			uint32_t tmp;
			size_t body_start;
			/* APP1 tag. This is where the EXIF data lives. */
			/* Read and discard the marker length. We're not continuing after this anyway. */
			(void)read16(d, &pos, len, 0);
			tmp = read32(d, &pos, len, 0);
			if (tmp != 0x66697845) /* Exif */
				break; /* Not exif - nothing to rewrite. */
			tmp = read16(d, &pos, len, 0);
			if (tmp != 0) /* Terminator + Pad */
				break; /* Not exif - nothing to rewrite. */
			/* Now we're at the APP1 Body. */
			body_start = pos;
			tmp = read16(d, &pos, len, 0);
			if (tmp == 0x4949)
				order = 0; /* LE */
			else if (tmp == 0x4d4d)
				order = 1; /* BE */
			else
				break; /* Bad TIFF type. Bale. */
			tmp = read16(d, &pos, len, order);
			if (tmp != 0x002a) /* 42 */
				break; /* Bad version field.  Bale. */
			do
			{
				uint32_t i, n;
				tmp = read32(d, &pos, len, order);
				pos = body_start + tmp;
				if (tmp == 0 || pos >= len)
					break;
				n = read16(d, &pos, len, order);
				for (i = 0; i < n; i++)
				{
					if (read16(d, &pos, len, order) == 0x112)
					{
						/* Orientation tag! */
						write16(d, &pos, len, order, 3); /* 3 = short */
						write32(d, &pos, len, order, 1); /* Count = 1 */
						write16(d, &pos, len, order, 1); /* Value = 1 */
						write16(d, &pos, len, order, 0); /* padding */
						i = n;
						pos = len; /* Done! */
					}
					else
						pos += 10;
				}
			}
			while (pos+4 < len);
			break;
		}
		else if (m >= 0xD0 && m <= 0xD7)
		{
			/* RSTm - no length code. But we shouldn't hit this! */
		}
		else if (m == 0x01)
		{
			/* TEM - temporary private use in arithmetic coding - shouldn't hit this either. */
		}
		else if (m == 0xD8)
		{
			/* SOI - start of image. */
		}
		else if (m == 0x01)
		{
			/* EOI - end of image. */
		}
		else
		{
			/* All other markers have a length. */
			size_t marker_len = d[pos]*256 + d[pos+1];
			pos += marker_len; /* The 2 length bytes are included in the marker_len */
		}
	}

	return out;
}

void
fz_append_image_as_data_uri(fz_context *ctx, fz_buffer *out, fz_image *image)
{
	fz_compressed_buffer *cbuf;
	fz_buffer *buf;
	const char *mime;

	cbuf = fz_compressed_image_buffer(ctx, image);

	if (cbuf && cbuf->params.type == FZ_IMAGE_JPEG)
	{
		int type = fz_colorspace_type(ctx, image->colorspace);
		if (type == FZ_COLORSPACE_GRAY || type == FZ_COLORSPACE_RGB)
		{
			fz_buffer *new_buf = fz_sanitize_jpeg_buffer(ctx, cbuf->buffer);
			fz_append_string(ctx, out, "data:image/jpeg;base64,");
			fz_try(ctx)
				fz_append_base64_buffer(ctx, out, new_buf, 1);
			fz_always(ctx)
				fz_drop_buffer(ctx, new_buf);
			fz_catch(ctx)
				fz_rethrow(ctx);
			return;
		}
	}

	if (cbuf && cbuf->params.type == FZ_IMAGE_PNG)
	{
		fz_append_string(ctx, out, "data:image/png;base64,");
		fz_append_base64_buffer(ctx, out, cbuf->buffer, 1);
		return;
	}

	if (fz_is_lossy_image(ctx, image))
	{
		/* Convert lossy image formats to JPEG */
		buf = fz_new_buffer_from_image_as_jpeg(ctx, image, fz_default_color_params, 90, 0);
		mime = "data:image/jpeg;base64,";
	}
	else
	{
	buf = fz_new_buffer_from_image_as_png(ctx, image, fz_default_color_params);
		mime = "data:image/png;base64,";
	}

	fz_try(ctx)
	{
		fz_append_string(ctx, out, mime);
		fz_append_base64_buffer(ctx, out, buf, 1);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_write_pixmap_as_data_uri(fz_context *ctx, fz_output *out, fz_pixmap *pixmap)
{
	fz_buffer *buf = fz_new_buffer_from_pixmap_as_png(ctx, pixmap, fz_default_color_params);
	fz_try(ctx)
	{
		fz_write_string(ctx, out, "data:image/png;base64,");
		fz_write_base64_buffer(ctx, out, buf, 1);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_append_pixmap_as_data_uri(fz_context *ctx, fz_buffer *out, fz_pixmap *pixmap)
{
	fz_buffer *buf = fz_new_buffer_from_pixmap_as_png(ctx, pixmap, fz_default_color_params);
	fz_try(ctx)
	{
		fz_append_string(ctx, out, "data:image/png;base64,");
		fz_append_base64_buffer(ctx, out, buf, 1);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

fz_document *
fz_new_xhtml_document_from_document(fz_context *ctx, fz_document *old_doc, const fz_stext_options *opts)
{
	fz_stext_options default_opts = { FZ_STEXT_PRESERVE_IMAGES | FZ_STEXT_DEHYPHENATE };
	fz_document *new_doc;
	fz_buffer *buf = NULL;
	fz_output *out = NULL;
	fz_stream *stm = NULL;
	fz_stext_page *text = NULL;
	int i;

	fz_var(buf);
	fz_var(out);
	fz_var(stm);
	fz_var(text);

	if (!opts)
		opts = &default_opts;

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 8192);
		out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_header_as_xhtml(ctx, out);

		for (i = 0; i < fz_count_pages(ctx, old_doc); ++i)
		{
			text = fz_new_stext_page_from_page_number(ctx, old_doc, i, opts);
			fz_print_stext_page_as_xhtml(ctx, out, text, i+1);
			fz_drop_stext_page(ctx, text);
			text = NULL;
		}

		fz_print_stext_trailer_as_xhtml(ctx, out);
		fz_close_output(ctx, out);
		fz_terminate_buffer(ctx, buf);

		stm = fz_open_buffer(ctx, buf);
		new_doc = fz_open_document_with_stream(ctx, "application/xhtml+xml", stm);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
		fz_drop_buffer(ctx, buf);
		fz_drop_output(ctx, out);
		fz_drop_stext_page(ctx, text);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return new_doc;
}

fz_buffer *
fz_new_buffer_from_page_with_format(fz_context *ctx, fz_page *page, const char *format, const char *options, fz_matrix transform, fz_cookie *cookie)
{
	fz_buffer *buf = NULL;
	fz_output *out;
	fz_document_writer *writer = NULL;
	fz_device *dev = NULL;

	fz_var(buf);
	fz_var(writer);
	fz_var(dev);

	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 0);
		out = fz_new_output_with_buffer(ctx, buf);
		writer = fz_new_document_writer_with_output(ctx, out, format, options);
		dev = fz_begin_page(ctx, writer, fz_bound_page(ctx, page));
		fz_run_page(ctx, page, dev, transform, cookie);
		fz_end_page(ctx, writer);
		fz_close_document_writer(ctx, writer);
	}
	fz_always(ctx)
		fz_drop_document_writer(ctx, writer);
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
	return buf;
}
