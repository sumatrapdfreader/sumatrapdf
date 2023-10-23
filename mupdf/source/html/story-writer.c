// Copyright (C) 2022 Artifex Software, Inc.
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


#include "mupdf/fitz/story-writer.h"

#include <string.h>

/*
 * Internal state for fz_write_story() to allow passing of page_num to
 * positionfn.
 */
typedef struct
{
	fz_write_story_positionfn *positionfn;
	void *positionfn_ref;
	int page_num;
} positionfn_state;

/*
 * Intermediate callback for fz_write_story() which calls the user-supplied
 * callback.
 */
static void positionfn_state_fn(fz_context *ctx, void *arg, const fz_story_element_position *position)
{
	positionfn_state *state = arg;
	fz_write_story_position do_position;
	do_position.element = *position;
	do_position.page_num = state->page_num;
	state->positionfn(ctx, state->positionfn_ref, &do_position);
}

void fz_write_story(
		fz_context *ctx,
		fz_document_writer *writer,
		fz_story *story,
		fz_write_story_rectfn rectfn,
		void *rectfn_ref,
		fz_write_story_positionfn positionfn,
		void *positionfn_ref,
		fz_write_story_pagefn pagefn,
		void *pagefn_ref
		)
{
	fz_rect filled = {0};
	int rect_num = 0;
	positionfn_state positionfn_state;

	positionfn_state.positionfn = positionfn;
	positionfn_state.positionfn_ref = positionfn_ref;
	positionfn_state.page_num = 0;

	fz_var(positionfn_state);

	fz_try(ctx)
	{
		fz_device *dev = NULL;
		for(;;)
		{
			fz_matrix ctm = fz_identity;
			fz_rect rect;
			fz_rect mediabox;
			int newpage;
			int more;

			newpage = rectfn(ctx, rectfn_ref, rect_num, filled, &rect, &ctm, &mediabox);
			rect_num += 1;
			if (newpage)
				positionfn_state.page_num += 1;

			more = fz_place_story(ctx, story, rect, &filled);

			if (positionfn)
			{
				fz_story_positions(ctx, story, positionfn_state_fn, &positionfn_state /*ref*/);
			}

			if (writer)
			{
				if (newpage)
				{
					if (dev)
					{
						if (pagefn)
							pagefn(ctx, pagefn_ref, positionfn_state.page_num, mediabox, dev, 1 /*after*/);
						fz_end_page(ctx, writer);
					}
					dev = fz_begin_page(ctx, writer, mediabox);
					if (pagefn)
						pagefn(ctx, pagefn_ref, positionfn_state.page_num, mediabox, dev, 0 /*after*/);
				}
				assert(dev);
				fz_draw_story(ctx, story, dev, ctm);
				if (!more)
				{
					if (pagefn)
						pagefn(ctx, pagefn_ref, positionfn_state.page_num, mediabox, dev, 1 /*after*/);
					fz_end_page(ctx, writer);
				}
			}
			else
			{
				fz_draw_story(ctx, story, NULL /*dev*/, ctm);
			}
			if (!more)
				break;
		}
	}
	fz_always(ctx)
	{
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/*
 * Copies a fz_write_story_position, taking care to use fz_strdup() for
 * strings.
 */
static void do_position_copy(fz_context *ctx, fz_write_story_position *to, const fz_write_story_position *from)
{
	*to = *from;
	to->element.id = NULL;
	to->element.text = NULL;
	if (from->element.id) to->element.id = fz_strdup(ctx, from->element.id);
	if (from->element.text) to->element.text = fz_strdup(ctx, from->element.text);
}

static void positions_clear(fz_context *ctx, fz_write_story_positions *positions)
{
	int i;
	/* positions_clear() will have used fz_strdup() for strings, so free
	them first: */
	for (i=0; i<positions->num; ++i)
	{
		fz_free(ctx, (void*) positions->positions[i].element.id);
		fz_free(ctx, (void*) positions->positions[i].element.text);
	}
	fz_free(ctx, positions->positions);
	positions->positions = NULL;
	positions->num = 0;
}

static int buffers_identical(fz_context *ctx, fz_buffer *a, fz_buffer *b)
{
	size_t a_len;
	size_t b_len;
	unsigned char *a_data;
	unsigned char *b_data;
	a_len = fz_buffer_storage(ctx, a, &a_data);
	b_len = fz_buffer_storage(ctx, b, &b_data);
	return a_len == b_len && !memcmp(a_data, b_data, a_len);
}


static void stabilize_positionfn(fz_context *ctx, void *ref, const fz_write_story_position *position)
{
	fz_write_story_positions *positions = ref;
	/* Append <element> plus page_num to items->items[]. */
	positions->positions = fz_realloc(ctx, positions->positions, sizeof(*positions->positions) * (positions->num + 1));
	do_position_copy(ctx, &positions->positions[positions->num], position);
	positions->num += 1;
}


void fz_write_stabilized_story(
		fz_context *ctx,
		fz_document_writer *writer,
		const char *user_css,
		float em,
		fz_write_story_contentfn contentfn,
		void *contentfn_ref,
		fz_write_story_rectfn rectfn,
		void *rectfn_ref,
		fz_write_story_pagefn pagefn,
		void *pagefn_ref,
		fz_archive *zip
		)
{
	fz_write_story_positions positions = {0};
	fz_story *story = NULL;
	fz_buffer *content = NULL;
	fz_buffer *content_prev = NULL;
	int stable = 0;

	positions.positions = NULL;
	positions.num = 0;

	fz_var(positions);
	fz_var(story);
	fz_var(content);

	fz_try(ctx)
	{
		content = fz_new_buffer(ctx, 0 /*capacity*/);
		content_prev = fz_new_buffer(ctx, 0 /*capacity*/);

		/* Iterate until stable. */
		for(;;)
		{
			/* Move <content> to <content_prev> and make <content>
			contain new html from contentfn(). */
			{
				fz_buffer *content_tmp = content;
				content = content_prev;
				content_prev = content_tmp;
			}
			fz_clear_buffer(ctx, content);
			contentfn(ctx, contentfn_ref, &positions, content);

			if (buffers_identical(ctx, content, content_prev))
			{
				/* Content is unchanged, so this is the last iteration and we
				will use <writer> not NULL. */
				stable = 1;
			}

			/* Create story from new content. */
			fz_drop_story(ctx, story);
			story = NULL;
			story = fz_new_story(ctx, content, user_css, em, zip);

			/* Layout the story, gathering toc information as we go. */
			positions_clear(ctx, &positions);
			fz_write_story(
					ctx,
					(stable) ? writer : NULL,
					story,
					rectfn,
					rectfn_ref,
					stabilize_positionfn,
					&positions /*positionfn_ref*/,
					pagefn,
					pagefn_ref
					);

			if (stable)
				break;
		}
	}
	fz_always(ctx)
	{
		fz_drop_story(ctx, story);
		fz_drop_buffer(ctx, content);
		fz_drop_buffer(ctx, content_prev);
		positions_clear(ctx, &positions);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
