// Copyright (C) 2025 Artifex Software, Inc.
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

#undef DEBUG_WRITE_AS_PS
#undef DEBUG_STRUCT

typedef struct boxer_s boxer_t;

typedef struct {
	int len;
	int max;
	double fudge;
	fz_rect list[FZ_FLEXIBLE_ARRAY];
} rectlist_t;

struct boxer_s {
	fz_rect mediabox;
	rectlist_t *list;
	int tight;
};

static rectlist_t *
rectlist_create(fz_context *ctx, int max, double fudge)
{
	rectlist_t *list = fz_malloc_flexible(ctx, rectlist_t, list, max);

	list->len = 0;
	list->max = max;
	list->fudge = fudge;

	return list;
}

/* Push box onto rectlist, unless it is completely enclosed by
 * another box, or completely encloses others (in which case they
 * are replaced by it). */
static void
rectlist_append(rectlist_t *list, fz_rect *box)
{
	int i;
	/* We allow ourselves a fudge factor when checking for inclusion.
	 * This is either 4 points, or 0 points, depending on whether
	 * we are running in 'tight' mode or not. */
	double r_fudge = list->fudge;

	for (i = 0; i < list->len; i++)
	{
		fz_rect *r = &list->list[i];
		fz_rect smaller, larger;

		smaller.x0 = r->x0 + r_fudge;
		larger. x0 = r->x0 - r_fudge;
		smaller.y0 = r->y0 + r_fudge;
		larger. y0 = r->y0 - r_fudge;
		smaller.x1 = r->x1 - r_fudge;
		larger. x1 = r->x1 + r_fudge;
		smaller.y1 = r->y1 - r_fudge;
		larger. y1 = r->y1 + r_fudge;

		if (fz_contains_rect(larger, *box))
			return; /* box is enclosed! Nothing to do. */
		if (fz_contains_rect(*box, smaller))
		{
			/* box encloses r. Ditch r. */
			/* Shorten the list */
			--list->len;
			/* If the one that just got chopped off wasn't r, move it down. */
			if (i < list->len)
			{
				memmove(r, &list->list[list->len], sizeof(*r));
				i--; /* Reconsider this entry next time. */
			}
		}
	}

	assert(list->len < list->max);
	memcpy(&list->list[list->len], box, sizeof(*box));
	list->len++;
}

static boxer_t *
boxer_create_length(fz_context *ctx, fz_rect *mediabox, int len, int tight)
{
	boxer_t *boxer = fz_malloc_struct(ctx, boxer_t);

	if (boxer == NULL)
		return NULL;

	memcpy(&boxer->mediabox, mediabox, sizeof(*mediabox));
	boxer->list = rectlist_create(ctx, len, tight ? 0 : 4);
	boxer->tight = tight;

	return boxer;
}

/* Create a boxer structure for a page of size mediabox. */
static boxer_t *
boxer_create(fz_context *ctx, fz_rect *mediabox, int tight)
{
	boxer_t *boxer = boxer_create_length(ctx, mediabox, 1, tight);

	if (boxer == NULL)
		return NULL;
	rectlist_append(boxer->list, mediabox);

	return boxer;
}

static void
push_if_intersect_suitable(rectlist_t *dst, const fz_rect *a, const fz_rect *b)
{
	fz_rect c;

	/* Intersect a and b. */
	c = fz_intersect_rect(*a, *b);
	/* If no intersection, nothing to push. */
	if (!fz_is_valid_rect(c))
		return;

	rectlist_append(dst, &c);
}

static void
boxlist_feed_intersect(rectlist_t *dst, const rectlist_t *src, const fz_rect *box)
{
	int i;

	for (i = 0; i < src->len; i++)
		push_if_intersect_suitable(dst, &src->list[i], box);
}

/* Mark a given box as being occupied (typically by a glyph) */
static void boxer_feed(fz_context *ctx, boxer_t *boxer, fz_rect *bbox)
{
	fz_rect box;
	/* When we feed a box into a the boxer, we can never make
	* the list more than 4 times as long. */
	rectlist_t *newlist = rectlist_create(ctx, boxer->list->len * 4, boxer->list->fudge);

#ifdef DEBUG_WRITE_AS_PS
	printf("0 0 1 setrgbcolor\n");
	printf("%g %g moveto %g %g lineto %g %g lineto %g %g lineto closepath fill\n",
		bbox->x0, bbox->y0,
		bbox->x0, bbox->y1,
		bbox->x1, bbox->y1,
		bbox->x1, bbox->y0
	);
#endif

	/* Left (0,0) (x0,H) */
	box.x0 = boxer->mediabox.x0;
	box.y0 = boxer->mediabox.y0;
	box.x1 = bbox->x0;
	box.y1 = boxer->mediabox.y1;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	/* Right (x1,0) (W,H) */
	box.x0 = bbox->x1;
	box.y0 = boxer->mediabox.y0;
	box.x1 = boxer->mediabox.x1;
	box.y1 = boxer->mediabox.y1;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	/* Bottom (0,0) (W,y0) */
	box.x0 = boxer->mediabox.x0;
	box.y0 = boxer->mediabox.y0;
	box.x1 = boxer->mediabox.x1;
	box.y1 = bbox->y0;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	/* Top (0,y1) (W,H) */
	box.x0 = boxer->mediabox.x0;
	box.y0 = bbox->y1;
	box.x1 = boxer->mediabox.x1;
	box.y1 = boxer->mediabox.y1;
	boxlist_feed_intersect(newlist, boxer->list, &box);

	fz_free(ctx, boxer->list);
	boxer->list = newlist;
}

#ifdef DEBUG_WRITE_AS_PS
static int
compare_areas(const void *a_, const void *b_)
{
	const fz_rect *a = (const fz_rect *)a_;
	const fz_rect *b = (const fz_rect *)b_;
	double area_a = (a->x1-a->x0) * (a->y1-a->y0);
	double area_b = (b->x1-b->x0) * (b->y1-b->y0);

	if (area_a < area_b)
		return 1;
	else if (area_a > area_b)
		return -1;
	else
		return 0;
}

/* Sort the rectangle list to be largest area first. For ease of humans
 * reading debug output. */
static void boxer_sort(boxer_t *boxer)
{
	qsort(boxer->list->list, boxer->list->len, sizeof(fz_rect), compare_areas);
}

/* Get the rectangle list for a given boxer. Return value is the length of
 * the list. Lifespan is until the boxer is modified or freed. */
static int boxer_results(boxer_t *boxer, fz_rect **list)
{
	*list = boxer->list->list;
	return boxer->list->len;
}
#endif

/* Currently unused debugging routine */
#if 0
static void
boxer_dump(fz_context *ctx, boxer_t *boxer)
{
	int i;

	printf("bbox = %g %g %g %g\n", boxer->mediabox.x0, boxer->mediabox.y0, boxer->mediabox.x1, boxer->mediabox.y1);
	for (i = 0; i < boxer->list->len; i++)
	{
		printf("%d %g %g %g %g\n", i, boxer->list->list[i].x0, boxer->list->list[i].y0, boxer->list->list[i].x1, boxer->list->list[i].y1);
	}
}
#endif

/* Destroy a boxer. */
static void boxer_destroy(fz_context *ctx, boxer_t *boxer)
{
	if (!boxer)
		return;

	fz_free(ctx, boxer->list);
	fz_free(ctx, boxer);
}

/* Find the margins for a given boxer. */
static fz_rect boxer_margins(boxer_t *boxer)
{
	rectlist_t *list = boxer->list;
	int i;
	fz_rect margins = boxer->mediabox;

	for (i = 0; i < list->len; i++)
	{
		fz_rect *r = &list->list[i];
		if (r->x0 <= margins.x0 && r->y0 <= margins.y0 && r->y1 >= margins.y1)
			margins.x0 = r->x1; /* Left Margin */
		else if (r->x1 >= margins.x1 && r->y0 <= margins.y0 && r->y1 >= margins.y1)
			margins.x1 = r->x0; /* Right Margin */
		else if (r->x0 <= margins.x0 && r->x1 >= margins.x1 && r->y0 <= margins.y0)
			margins.y0 = r->y1; /* Top Margin */
		else if (r->x0 <= margins.x0 && r->x1 >= margins.x1 && r->y1 >= margins.y1)
			margins.y1 = r->y0; /* Bottom Margin */
	}

	return margins;
}

/* Create a new boxer from a subset of an old one. */
static boxer_t *boxer_subset(fz_context *ctx, boxer_t *boxer, fz_rect rect)
{
	boxer_t *new_boxer = boxer_create_length(ctx, &rect, boxer->list->len, boxer->tight);
	int i;

	if (new_boxer == NULL)
		return NULL;

	for (i = 0; i < boxer->list->len; i++)
	{
		fz_rect r = fz_intersect_rect(boxer->list->list[i], rect);

		if (fz_is_empty_rect(r))
			continue;
		rectlist_append(new_boxer->list, &r);
	}

	return new_boxer;
}

static int analyse_sub(fz_context *ctx, fz_stext_page *page, fz_stext_struct *parent, boxer_t *big_boxer, int depth);

static void
analyse_subset(fz_context *ctx, fz_stext_page *page, fz_stext_struct *parent, boxer_t *boxer, fz_rect r, int depth)
{
	boxer_t *sub_box = boxer_subset(ctx, boxer, r);

	fz_try(ctx)
		(void)analyse_sub(ctx, page, parent, sub_box, depth);
	fz_always(ctx)
		boxer_destroy(ctx, sub_box);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* Consider a boxer for subdivision.
 * Returns 0 if no suitable subdivision point found.
 * Returns 1 if a subdivision point is found.*/
static int
boxer_subdivide(fz_context *ctx, fz_stext_page *page, fz_stext_struct *parent, boxer_t *boxer, int depth)
{
	rectlist_t *list = boxer->list;
	double max_size = 0;
	int i;
	int horiz = 0;
	int largest = -1;
	fz_rect r;

	for (i = 0; i < list->len; i++)
	{
		r = list->list[i];

		if (r.x0 <= boxer->mediabox.x0 && r.x1 >= boxer->mediabox.x1)
		{
			/* Horizontal divider */
			double size = r.y1 - r.y0;
			if (size > max_size)
			{
				max_size = size;
				largest = i;
				horiz = 1;
			}
		}
		if (r.y0 <= boxer->mediabox.y0 && r.y1 >= boxer->mediabox.y1)
		{
			/* Vertical divider */
			double size = r.x1 - r.x0;
			if (size > max_size)
			{
				max_size = size;
				largest = i;
				horiz = 0;
			}
		}
	}

	if (largest == -1)
	{
#ifdef DEBUG_WRITE_AS_PS
		{
			printf("%% SUBSET\n");
			printf("0 1 1 setrgbcolor\n");
			printf("%g %g moveto\n%g %g lineto\n%g %g lineto\n%g %g lineto\nclosepath\nstroke\n\n",
			boxer->mediabox.x0, boxer->mediabox.y0,
			boxer->mediabox.x0, boxer->mediabox.y1,
			boxer->mediabox.x1, boxer->mediabox.y1,
			boxer->mediabox.x1, boxer->mediabox.y0);
				}
#endif
		return 0;
			}

			r = boxer->mediabox;
	if (horiz)
	{
		/* Divider runs horizontally. */
#ifdef DEBUG_WRITE_AS_PS
		{
			printf("%% H DIVIDER\n");
			printf("1 0 1 setrgbcolor\n");
			printf("%g %g moveto\n%g %g lineto\nstroke\n\n",
			list->list[largest].x0, (list->list[largest].y0 + list->list[largest].y1) * 0.5f,
			list->list[largest].x1, (list->list[largest].y0 + list->list[largest].y1) * 0.5f);
		}
#endif
		r.y1 = list->list[largest].y0;
			analyse_subset(ctx, page, parent, boxer, r, depth);

		r.y0 = list->list[largest].y1;
		r.y1 = boxer->mediabox.y1;
		analyse_subset(ctx, page, parent, boxer, r, depth);
	}
	else
	{
		/* Divider runs vertically. */
#ifdef DEBUG_WRITE_AS_PS
		{
			printf("%% V DIVIDER\n");
			printf("1 0 1 setrgbcolor\n");
			printf("%g %g moveto\n%g %g lineto\nstroke\n\n",
			(list->list[largest].x0 + list->list[largest].x1) * 0.5f, list->list[largest].y0,
			(list->list[largest].x0 + list->list[largest].x1) * 0.5f, list->list[largest].y1);
					}
#endif
		r.x1 = list->list[largest].x0;
		analyse_subset(ctx, page, parent, boxer, r, depth);

		r.x0 = list->list[largest].x1;
		r.x1 = boxer->mediabox.x1;
			analyse_subset(ctx, page, parent, boxer, r, depth);
		}

		return 1;
	}

#ifdef DEBUG_STRUCT
static void
do_dump_stext(fz_stext_block *block, int depth)
{
	int d;
	int idx = -1;

	while (block)
	{
		for (d = 0; d < depth; d++)
			printf(" ");
		switch(block->type)
		{
			case FZ_STEXT_BLOCK_TEXT:
				printf("TEXT %p\n", block);
				break;
			case FZ_STEXT_BLOCK_IMAGE:
				printf("IMAGE %p\n", block);
				break;
			case FZ_STEXT_BLOCK_VECTOR:
				printf("VECTOR %p\n", block);
				break;
			case FZ_STEXT_BLOCK_STRUCT:
				printf("STRUCT %p (idx=%d)\n", block, block->u.s.index);
				assert(block->u.s.index > idx);
				idx = block->u.s.index;
				do_dump_stext(block->u.s.down->first_block, depth+1);
				break;
		}
		block = block->next;
	}
}

static void
dump_stext(char *str, fz_stext_block *block)
{
	printf("%s\n", str);

	do_dump_stext(block, 0);
}
#endif

static void
recalc_bbox(fz_stext_block *block)
{
	fz_rect bbox = fz_empty_rect;
	fz_stext_line *line;

	for (line = block->u.t.first_line; line != NULL; line = line->next)
		bbox = fz_union_rect(bbox, line->bbox);

	block->bbox = bbox;
}

static fz_stext_struct *
page_subset(fz_context *ctx, fz_stext_page *page, fz_stext_struct *parent, fz_rect mediabox)
{
	fz_stext_block *block, *next_block;
	fz_stext_block *target = NULL; /* The first block in our target list */
	fz_stext_block *last = NULL; /* The last block in our target list */
	fz_stext_struct *target_parent = NULL;
	fz_stext_block *after = NULL; /* The block we want to insert after (NULL=start of list) */
	fz_stext_block *newblock;
	int idx = 0;
	int idx2;
#ifdef DEBUG_STRUCT
	dump_stext("BEFORE", parent ? parent->first_block : page->first_block);
#endif

	block = parent ? parent->first_block : page->first_block;
	while (block != NULL)
	{
		fz_rect bbox;

		next_block = block->next;

		bbox = block->bbox;

		/* Can we take the whole block? */
		if (bbox.x0 >= mediabox.x0 && bbox.y0 >= mediabox.y0 && bbox.x1 <= mediabox.x1 && bbox.y1 <= mediabox.y1)
		{
			/* Unlink block from the current list. */
			if (block->prev)
				block->prev->next = next_block;
			else if (parent)
				parent->first_block = next_block;
			else
				page->first_block = next_block;
			if (next_block)
				next_block->prev = block->prev;
			else if (parent)
				parent->last_block = block->prev;
			else
				page->last_block = block->prev;

			/* Add block onto our target list */
			if (target == NULL)
			{
				target = block;
				target_parent = parent;
				after = block->prev;
				block->prev = NULL;
			}
			else
			{
				last->next = block;
				block->prev = last;
			}
			last = block;
			block->next = NULL;
		}
		else if (fz_is_empty_rect(fz_intersect_rect(bbox, mediabox)))
		{
		}
		else if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down)
		{
			parent = block->u.s.down;
			next_block = parent->first_block;
		}
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			/* Need to look at the parts. */
			fz_stext_line *line, *next_line;

			newblock = NULL;
			for (line = block->u.t.first_line; line != NULL; line = next_line)
			{
				next_line = line->next;
				if (line->bbox.x0 >= mediabox.x0 && line->bbox.y0 >= mediabox.y0 && line->bbox.x1 <= mediabox.x1 && line->bbox.y1 <= mediabox.y1)
				{
					/* We need to take this line */
					if (newblock == NULL)
					{
						newblock = fz_pool_alloc(ctx, page->pool, sizeof(fz_stext_block));

						/* Add the block onto our target list */
						if (target == NULL)
						{
							target = newblock;
							target_parent = parent;
							if (line == block->u.t.first_line)
								after = block->prev;
							else
								after = block;
						}
						else
						{
							last->next = newblock;
							newblock->prev = last;
						}
						last = newblock;
						newblock->id = block->id;
					}

					/* Unlink line from the current list. */
					if (line->prev)
						line->prev->next = next_line;
					else
						block->u.t.first_line = next_line;
					if (next_line)
						next_line->prev = line->prev;
					else
						block->u.t.last_line = line->prev;

					/* Add line onto our new block */
					if (newblock->u.t.last_line == NULL)
					{
						newblock->u.t.first_line = newblock->u.t.last_line = line;
						line->prev = NULL;
					}
					else
					{
						line->prev = newblock->u.t.last_line;
						newblock->u.t.last_line->next = line;
						newblock->u.t.last_line = line;
					}
					line->next = NULL;
				}
			}
			if (newblock)
			{
				recalc_bbox(block);
				recalc_bbox(newblock);
			}
		}

		/* Step onwards (or upwards) */
		block = next_block;
		while (block == NULL)
		{
			if (parent == NULL)
				break;
			block = parent->up->next;
			parent = parent->parent;
		}
	}

	/* If no content to add, bale! */
	if (target == NULL)
		return NULL;

	/* We want to insert a structure node that contains target after 'after'. */
	block = target_parent ? target_parent->first_block : page->first_block;
	if (after != NULL)
	{
		while (1)
		{
			if (block->type == FZ_STEXT_BLOCK_STRUCT)
				idx = block->u.s.index+1;
			if (block == after)
				break;
			block = block->next;
		}
		block = block->next;
	}
	/* So we want to insert a structure node with index 'idx' after 'after' */
	/* Ensure that the following structure nodes have sane index values */
	idx2 = idx+1;
	for (; block != NULL; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_STRUCT)
			continue;
		if (block->u.s.index >= idx2)
			break;
		block->u.s.index = idx2;
		idx2++;
	}

	/* Convert from 'after' to 'before'. */
	if (after)
		block = after->next;
	else if (target_parent)
		block = target_parent->first_block;
	else
		block = page->first_block;

	/* So we want to insert just before block, with index 'idx'. */

	/* We are going to need to create a new block. Create a complete unlinked one here. */
	newblock = fz_new_stext_struct(ctx, page, FZ_STRUCTURE_DIV, "Split", idx);
	if (block)
		newblock->prev = block->prev;
	else if (target_parent)
		newblock->prev = target_parent->last_block;
	else
		newblock->prev = page->last_block;
	newblock->next = block;
	newblock->id = target->id;

	/* Now insert newblock just before block */
	/* If block was first, now we are. */
	if (target_parent)
	{
		if (target_parent->first_block == block)
			target_parent->first_block = newblock;
	}
	else if (page->first_block == block)
		page->first_block = newblock;
	if (block == NULL)
	{
		/* Inserting at the end! */
		if (target_parent)
		{
			if (target_parent->last_block)
				target_parent->last_block->next = newblock;
			target_parent->last_block = newblock;
		}
		else
		{
			if (page->last_block)
				page->last_block->next = newblock;
			page->last_block = newblock;
		}
	}
	else
	{
		if (block->prev)
			block->prev->next = newblock;
		block->prev = newblock;
	}

	newblock->u.s.down->first_block = target;
	newblock->u.s.down->last_block = last;
	target->prev = NULL;

	for (block = target; block->next != NULL; block = block->next)
		newblock->bbox = fz_union_rect(newblock->bbox, block->bbox);
	newblock->bbox = fz_union_rect(newblock->bbox, block->bbox);
	newblock->u.s.down->last_block = block;

#ifdef DEBUG_STRUCT
	dump_stext("AFTER", parent ? parent->first_block : page->first_block);
#endif

	return newblock->u.s.down;
}

enum {
	MAX_ANALYSIS_DEPTH = 6
};

static int
analyse_sub(fz_context *ctx, fz_stext_page *page, fz_stext_struct *parent, boxer_t *big_boxer, int depth)
{
	fz_rect margins;
	boxer_t *boxer;
	fz_stext_struct *div;
	int ret = 0;

	/* Find the margins in the enclosing boxer. This returns
	 * a subset of the bbox of the original. */
	margins = boxer_margins(big_boxer);
#ifdef DEBUG_WRITE_AS_PS
	printf("\n\n%% MARGINS %g %g %g %g\n", margins.x0, margins.y0, margins.x1, margins.y1);
#endif

	/* Now subset the rectangles just to include those that are in our bbox. */
	boxer = boxer_subset(ctx, big_boxer, margins);

	fz_try(ctx)
	{
		div = page_subset(ctx, page, parent, boxer->mediabox);
		/* If nothing subsetted (no textual content in that region), give up. */
		if (div == NULL)
			break;

		ret = 1;

		if (depth < MAX_ANALYSIS_DEPTH)
		{
			/* Can we subdivide that region any more? */
			if (boxer_subdivide(ctx, page, div, boxer, depth+1))
				break;
		}
	}
	fz_always(ctx)
	{
		boxer_destroy(ctx, boxer);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

static int
line_isnt_all_spaces(fz_context *ctx, fz_stext_line *line)
{
	fz_stext_char *ch;
	for (ch = line->first_char; ch != NULL; ch = ch->next)
		if (ch->c != 32 && ch->c != 160)
			return 1;
	return 0;
}

static void
feed_line(fz_context *ctx, boxer_t *boxer, fz_stext_line *line)
{
	fz_stext_char *ch;

	for (ch = line->first_char; ch != NULL; ch = ch->next)
	{
		fz_rect r = fz_empty_rect;

		if (ch->c == ' ')
			continue;

		do
		{
			fz_rect bbox = fz_rect_from_quad(ch->quad);
			float margin = boxer->tight ? 0 : ch->size/4;
			bbox.x0 -= margin;
			bbox.y0 -= margin;
			bbox.x1 += margin;
			bbox.y1 += margin;
			r = fz_union_rect(r, bbox);
			ch = ch->next;
		}
		while (ch != NULL && ch->c != ' ');
		boxer_feed(ctx, boxer, &r);
		if (ch == NULL)
			break;
	}
}

/* Internal, non-API function, shared with stext-table. */
fz_rect
fz_collate_small_vector_run(fz_stext_block **blockp)
{
	fz_stext_block *block = *blockp;
	fz_stext_block *next;
	fz_rect r = block->bbox;
	int MAX_SIZE = 2;
	int MAX_GAP = 2;

	float w = r.x1 - r.x0;
	float h = r.y1 - r.y0;

	if (w < MAX_SIZE)
	{
		while ((next = block->next) != NULL &&
			next->type == FZ_STEXT_BLOCK_VECTOR &&
			next->bbox.x0 == r.x0 &&
			next->bbox.x1 == r.x1 &&
			((next->bbox.y1 > r.y1 && next->bbox.y0 <= r.y1 + MAX_GAP) ||
			(next->bbox.y0 < r.y0 && next->bbox.y1 >= r.y0 - MAX_GAP)))
		{
			r = fz_union_rect(r, next->bbox);
			block = next;
		}
	}
	if (h < MAX_SIZE)
	{
		while ((next = block->next) != NULL &&
			next->type == FZ_STEXT_BLOCK_VECTOR &&
			next->bbox.y0 == r.y0 &&
			next->bbox.y1 == r.y1 &&
			((next->bbox.x1 > r.x1 && next->bbox.x0 <= r.x1 + MAX_GAP) ||
			(next->bbox.x0 < r.x0 && next->bbox.x1 >= r.x0 - MAX_GAP)))
		{
			r = fz_union_rect(r, next->bbox);
			block = next;
		}
	}

	*blockp = block;

	return r;
}

static void
recurse_and_feed(fz_context *ctx, boxer_t *boxer, fz_stext_block *block)
	{
	for (; block != NULL; block = block->next)
		{
			fz_stext_line *line;
			switch (block->type)
			{
			case FZ_STEXT_BLOCK_TEXT:
				for (line = block->u.t.first_line; line != NULL; line = line->next)
					if (line_isnt_all_spaces(ctx, line))
						feed_line(ctx, boxer, line);
				break;
			case FZ_STEXT_BLOCK_VECTOR:
			{
				/* Allow a 1 point margin around vectors to avoid hairline
				 * cracks between supposedly abutting things. */
				int VECTOR_MARGIN = 1;
				fz_rect r = fz_collate_small_vector_run(&block);

				r.x0 -= VECTOR_MARGIN;
				r.y0 -= VECTOR_MARGIN;
				r.x1 += VECTOR_MARGIN;
				r.y1 += VECTOR_MARGIN;
				boxer_feed(ctx, boxer, &r);
			break;
		}
		case FZ_STEXT_BLOCK_STRUCT:
			if(block->u.s.down)
				recurse_and_feed(ctx, boxer, block->u.s.down->first_block);
			break;
		default:
			boxer_feed(ctx, boxer, &block->bbox);
			break;
			}
			}
		}

static int
segment_rect(fz_context *ctx, fz_rect box, fz_stext_page *page, fz_stext_struct *parent, int tight)
{
	boxer_t *boxer;
	int ret = 0;

	boxer = boxer_create(ctx, &box, tight);

	fz_try(ctx)
	{
		recurse_and_feed(ctx, boxer, parent ? parent->first_block : page->first_block);

		ret = analyse_sub(ctx, page, parent, boxer, 0);
	}
	fz_always(ctx)
		boxer_destroy(ctx, boxer);
	fz_catch(ctx)
		fz_rethrow(ctx);

#ifdef DEBUG_WRITE_AS_PS
	printf("showpage\n");
#endif

	return ret;
}

int fz_segment_stext_rect(fz_context *ctx, fz_stext_page *page, fz_rect rect)
{
#ifdef DEBUG_WRITE_AS_PS
	printf("1 -1 scale 0 -%g translate\n", rect.y1-rect.y0);
#endif

	return segment_rect(ctx, rect, page, NULL, 1);
}

int fz_segment_stext_page(fz_context *ctx, fz_stext_page *page)
{
	fz_stext_block *block;

	fz_stext_remove_page_fill(ctx, page);

	/* If we have structure already, give up. We can't hope to beat
	 * proper structure! */
	for (block = page->first_block; block != NULL; block = block->next)
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
			return 0;

#ifdef DEBUG_WRITE_AS_PS
	printf("1 -1 scale 0 -%g translate\n", page->mediabox.y1-page->mediabox.y0);
#endif

	return segment_rect(ctx, page->mediabox, page, NULL, 0);
}

int
fz_stext_remove_page_fill(fz_context *ctx, fz_stext_page *page)
{
	fz_stext_page_block_iterator iter;
	int dropped = 0;
	fz_rect coverage = fz_empty_rect;

	/* First, find the area actually covered on the page. */
	for (iter = fz_stext_page_block_iterator_begin(page); !fz_stext_page_block_iterator_eod_dfs(iter); iter = fz_stext_page_block_iterator_next_dfs(iter))
	{
		/* Try to ignore stuff that's completely off screen */
		fz_rect bbox = fz_intersect_rect(page->mediabox, iter.block->bbox);

		coverage = fz_union_rect(coverage, bbox);
	}

	/* Iterate across all the blocks in the page in a depth first order. We'll break out
	 * when we find the first one that is not a white (or transparent) rectangle fill
	 * that covers a significant amount of the page. This therefore copes with several
	 * page fills on the same page. */
	for (iter = fz_stext_page_block_iterator_begin(page); !fz_stext_page_block_iterator_eod_dfs(iter); iter = fz_stext_page_block_iterator_next_dfs(iter))
	{
		fz_rect bbox;

		/* Stop searching when we find something that's not a vector */
		if (iter.block->type != FZ_STEXT_BLOCK_VECTOR)
			break;

		/* Stop searching when we find a vector that's not a rectangle */
		if ((iter.block->u.v.flags & FZ_STEXT_VECTOR_IS_RECTANGLE) == 0)
			break;

		/* Stop searching when we find a vector that's stroked */
		if ((iter.block->u.v.flags & FZ_STEXT_VECTOR_IS_STROKED) != 0)
			break;

		/* Stop searching when we find a vector that's not white (or invisible) */
		if ((iter.block->u.v.argb & 0xff000000) != 0 && (iter.block->u.v.argb & 0xffffff) != 0xffffff)
			break;

		/* If we don't cover the coverage area, then we can't be a background. */
		bbox = fz_expand_rect(iter.block->bbox, 0.1f); /* Allow for rounding */
		if (!fz_contains_rect(bbox, coverage))
			break;

		/* If we don't cover at least 90% of the height/width, then give up. */
		if (fz_is_infinite_rect(page->mediabox))
		{
			/* Can't judge. Skip this check. */
		}
		else if ((iter.block->bbox.y1 - iter.block->bbox.y0) < 0.9f * (page->mediabox.y1 - page->mediabox.y0) ||
			(iter.block->bbox.x1 - iter.block->bbox.x0) < 0.9f * (page->mediabox.x1 - page->mediabox.x0))
		{
			break;
		}

		/* This is a background block. Remove it. This will be the first block
		 * in the list, so it's simpler. */
		if (iter.parent)
		{
			iter.parent->first_block = iter.block->next;
			if (iter.block->next)
				iter.block->next->prev = NULL;
			else
				iter.parent->last_block = NULL;
		}
		else
		{
			iter.page->first_block = iter.block->next;
			if (iter.block->next)
				iter.block->next->prev = NULL;
			else
				iter.page->last_block = NULL;
		}

		dropped = 1;
	}

	return dropped;
}
