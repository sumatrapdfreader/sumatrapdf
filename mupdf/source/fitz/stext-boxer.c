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
	fz_rect list[FZ_FLEXIBLE_ARRAY];
} rectlist_t;

struct boxer_s {
	fz_rect mediabox;
	rectlist_t *list;
};

static int fz_rect_contains_rect(fz_rect a, fz_rect b)
{
	if (a.x0 > b.x0)
		return 0;
	if (a.y0 > b.y0)
		return 0;
	if (a.x1 < b.x1)
		return 0;
	if (a.y1 < b.y1)
		return 0;

	return 1;
}

static rectlist_t *
rectlist_create(fz_context *ctx, int max)
{
	rectlist_t *list = fz_malloc_flexible(ctx, rectlist_t, list, max);

	list->len = 0;
	list->max = max;

	return list;
}

/* Push box onto rectlist, unless it is completely enclosed by
 * another box, or completely encloses others (in which case they
 * are replaced by it). */
static void
rectlist_append(rectlist_t *list, fz_rect *box)
{
	int i;

	for (i = 0; i < list->len; i++)
	{
		fz_rect *r = &list->list[i];
		fz_rect smaller, larger;
		/* We allow ourselves a fudge factor of 4 points when checking for inclusion. */
		double r_fudge = 4;

		smaller.x0 = r->x0 + r_fudge;
		larger. x0 = r->x0 - r_fudge;
		smaller.y0 = r->y0 + r_fudge;
		larger. y0 = r->y0 - r_fudge;
		smaller.x1 = r->x1 - r_fudge;
		larger. x1 = r->x1 + r_fudge;
		smaller.y1 = r->y1 - r_fudge;
		larger. y1 = r->y1 + r_fudge;

		if (fz_rect_contains_rect(larger, *box))
			return; /* box is enclosed! Nothing to do. */
		if (fz_rect_contains_rect(*box, smaller))
		{
			/* box encloses r. Ditch r. */
			/* Shorten the list */
			--list->len;
			/* If the one that just got chopped off wasn't r, move it down. */
			if (i < list->len)
			{
				memcpy(r, &list->list[list->len], sizeof(*r));
				i--; /* Reconsider this entry next time. */
			}
		}
	}

	assert(list->len < list->max);
	memcpy(&list->list[list->len], box, sizeof(*box));
	list->len++;
}

static boxer_t *
boxer_create_length(fz_context *ctx, fz_rect *mediabox, int len)
{
	boxer_t *boxer = fz_malloc_struct(ctx, boxer_t);

	if (boxer == NULL)
		return NULL;

	memcpy(&boxer->mediabox, mediabox, sizeof(*mediabox));
	boxer->list = rectlist_create(ctx, len);

	return boxer;
}

/* Create a boxer structure for a page of size mediabox. */
static boxer_t *
boxer_create(fz_context *ctx, fz_rect *mediabox)
{
	boxer_t *boxer = boxer_create_length(ctx, mediabox, 1);

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
	rectlist_t *newlist = rectlist_create(ctx, boxer->list->len * 4);

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
	boxer_t *new_boxer = boxer_create_length(ctx, &rect, boxer->list->len);
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

static int analyse_sub(fz_context *ctx, fz_stext_page *page, fz_stext_block **first_block, fz_stext_block **last_block, boxer_t *big_boxer, int depth);

static void
analyse_subset(fz_context *ctx, fz_stext_page *page, fz_stext_block **first_block, fz_stext_block **last_block, boxer_t *boxer, fz_rect r, int depth)
{
	boxer_t *sub_box = boxer_subset(ctx, boxer, r);

	fz_try(ctx)
		(void)analyse_sub(ctx, page, first_block, last_block, sub_box, depth);
	fz_always(ctx)
		boxer_destroy(ctx, sub_box);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

/* Consider a boxer for subdivision.
 * Returns 0 if no suitable subdivision point found.
 * Returns 1 if a subdivision point is found.*/
static int
boxer_subdivide(fz_context *ctx, fz_stext_page *page, fz_stext_block **first_block, fz_stext_block **last_block, boxer_t *boxer, int depth)
{
	rectlist_t *list = boxer->list;
	double max_h = 0, max_v = 0;
	int i;

	for (i = 0; i < list->len; i++)
	{
		fz_rect r = boxer->list->list[i];

		if (r.x0 <= boxer->mediabox.x0 && r.x1 >= boxer->mediabox.x1)
		{
			/* Horizontal divider */
			double size = r.y1 - r.y0;
			if (size > max_h)
			{
				max_h = size;
			}
		}
		if (r.y0 <= boxer->mediabox.y0 && r.y1 >= boxer->mediabox.y1)
		{
			/* Vertical divider */
			double size = r.x1 - r.x0;
			if (size > max_v)
			{
				max_v = size;
			}
		}
	}

	if (max_h > max_v)
	{
		fz_rect r;
		float min_gap;
		float top;

		/* Divider runs horizontally. */

		/* We want to list out all the horizontal subregions that are separated
		 * by an appropriate gap, from top to bottom. */

		/* Any gap larger than the the gap we ignored will do. */
		min_gap = max_v;

		/* We're going to need to run through the data multiple times to find the
		 * topmost block each time. We'll use 'top' to cull against, and gradually
		 * lower that after each successful pass. */
		top = boxer->mediabox.y0;
		while (1)
		{
			/* Find the topmost divider below 'top' of height at least min_gap */
			float found_top;
			int found = -1;
			for (i = 0; i < list->len; i++)
			{
				fz_rect *b = &list->list[i];
				if (b->x0 <= boxer->mediabox.x0 && boxer->mediabox.x1 <= b->x1 && b->y0 > top && b->y1 - b->y0 >= min_gap)
				{
					if (found == -1 || b->y0 < found_top)
					{
						found = i;
						found_top = b->y0;
					}
				}
			}

			/* If we failed to find one, we're done. */
			if (found == -1)
				break;

			/* So we have a region from top to found_top */
			r = boxer->mediabox;
			r.y0 = top;
			r.y1 = found_top;

			analyse_subset(ctx, page, first_block, last_block, boxer, r, depth);

			/* Now move top down for the next go. */
			top = list->list[found].y1;
		}

		/* One final region, from top to bottom */
		r = boxer->mediabox;
		r.y0 = top;
		analyse_subset(ctx, page, first_block, last_block, boxer, r, depth);

		return 1;
	}
	else if (max_v > 0)
	{
		fz_rect r;
		float min_gap;
		float left;

		/* Divider runs vertically. */

		/* We want to list out all the vertical subregions that are separated
		 * by an appropriate gap, from left to right. */

		/* Any gap larger than the the gap we ignored will do. */
		min_gap = max_h;

		/* We're going to need to run through the data multiple times to find the
		 * leftmost block each time. We'll use 'left' to cull against, and gradually
		 * lower that after each successful pass. */
		left = boxer->mediabox.x0;
		while (1)
		{
			/* Find the leftmost divider to the right of 'left' of width at least min_gap */
			float found_left;
			int found = -1;
			for (i = 0; i < list->len; i++)
			{
				fz_rect *b = &list->list[i];
				if (b->y0 <= boxer->mediabox.y0 && boxer->mediabox.y1 <= b->y1 && b->x0 > left && b->x1 - b->x0 >= min_gap)
				{
					if (found == -1 || b->x0 < found_left)
					{
						found = i;
						found_left = b->x0;
					}
				}
			}

			/* If we failed to find one, we're done. */
			if (found == -1)
				break;

			/* So we have a region from top to found_top */
			r = boxer->mediabox;
			r.x0 = left;
			r.x1 = found_left;
			analyse_subset(ctx, page, first_block, last_block, boxer, r, depth);

			/* Now move left right for the next go. */
			left = list->list[found].x1;
		}

		/* One final region, from left to right */
		r = boxer->mediabox;
		r.x0 = left;
		analyse_subset(ctx, page, first_block, last_block, boxer, r, depth);

		return 1;
	}

	return 0;
}

static void
new_stext_struct(fz_context *ctx, fz_stext_page *page, fz_stext_block *block, fz_structure standard, const char *raw)
{
	fz_stext_struct *str;
	size_t z;

	if (raw == NULL)
		raw = "";
	z = strlen(raw);

	str = fz_pool_alloc(ctx, page->pool, offsetof(fz_stext_struct, raw) + z + 1);
	str->first_block = NULL;
	str->last_block = NULL;
	str->standard = standard;
	str->parent = page->last_struct;
	str->up = block;
	memcpy(str->raw, raw, z+1);

	block->u.s.down = str;
}

#ifdef DEBUG_STRUCT
static void
do_dump_stext(fz_stext_block *block, int depth)
{
	int d;

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
				printf("STRUCT %p\n", block);
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
page_subset(fz_context *ctx, fz_stext_page *page, fz_stext_block **first_block, fz_stext_block **last_block, fz_rect mediabox)
{
	fz_stext_block *block, *next_block;
	fz_stext_block *target = NULL;
	fz_stext_block *last = NULL;
	fz_stext_block *newblock;
	int idx = 0;

#ifdef DEBUG_STRUCT
	dump_stext("BEFORE", *first_block);
#endif

	for (block = *first_block; block != NULL; block = next_block)
	{
		fz_rect bbox;

		next_block = block->next;
		if (block->type != FZ_STEXT_BLOCK_TEXT && block->type != FZ_STEXT_BLOCK_VECTOR)
			continue;

		bbox = block->bbox;

		/* Can we take the whole block? */
		if (bbox.x0 >= mediabox.x0 && bbox.y0 >= mediabox.y0 && bbox.x1 <= mediabox.x1 && bbox.y1 <= mediabox.y1)
		{
			/* Unlink block from the current list. */
			if (block->prev)
				block->prev->next = next_block;
			else
				*first_block = next_block;
			if (next_block)
				next_block->prev = block->prev;
			else
				*last_block = block->prev;

			/* Add block onto our target list */
			if (target == NULL)
			{
				target = block;
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
		else if (block->type == FZ_STEXT_BLOCK_TEXT && !fz_is_empty_rect(fz_intersect_rect(bbox, mediabox)))
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
						}
						else
						{
							last->next = newblock;
							newblock->prev = last;
						}
						last = newblock;
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
	}

	/* If no content to add, bale! */
	if (target == NULL)
		return NULL;

	/* We want to insert a structure node that contains target as the last structure
	 * node on this blocklist. Find the first block that's not a structure block. */
	for (block = *first_block; block != NULL; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_STRUCT)
			break;
		idx++;
	}

	/* So we want to insert just before block. */

	/* We are going to need to create a new block. Create a complete unlinked one here. */
	newblock = fz_pool_alloc(ctx, page->pool, sizeof *newblock);
	newblock->bbox = fz_empty_rect;
	newblock->prev = block ? block->prev : *last_block;
	newblock->next = block;
	newblock->type = FZ_STEXT_BLOCK_STRUCT;
	newblock->u.s.index = idx;
	newblock->u.s.down = NULL;
	/* If this throws, we leak newblock but it's within the pool, so it doesn't matter. */
	/* And create a new struct and have newblock point to it. */
	new_stext_struct(ctx, page, newblock, FZ_STRUCTURE_DIV, "Split");

	/* Now insert newblock just before block */
	/* If block was first, now we are. */
	if (*first_block == block)
		*first_block = newblock;
	if (block == NULL)
	{
		/* Inserting at the end! */
		if (*last_block)
			(*last_block)->next = newblock;
		*last_block = newblock;
	}
	else
	{
		if (block->prev)
			block->prev->next = newblock;
		block->prev = newblock;
	}

	newblock->u.s.down->first_block = target;
	target->prev = NULL;

	for (block = target; block->next != NULL; block = block->next)
		newblock->bbox = fz_union_rect(newblock->bbox, block->bbox);
	newblock->bbox = fz_union_rect(newblock->bbox, block->bbox);
	newblock->u.s.down->last_block = block;

#ifdef DEBUG_STRUCT
	dump_stext("AFTER", *first_block);
#endif

	return newblock->u.s.down;
}

enum {
	MAX_ANALYSIS_DEPTH = 6
};

static int
analyse_sub(fz_context *ctx, fz_stext_page *page, fz_stext_block **first_block, fz_stext_block **last_block, boxer_t *big_boxer, int depth)
{
	fz_rect margins;
	boxer_t *boxer;
	boxer_t *boxer1 = NULL;
	boxer_t *boxer2 = NULL;
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

	fz_var(boxer1);
	fz_var(boxer2);

	fz_try(ctx)
	{
		div = page_subset(ctx, page, first_block, last_block, boxer->mediabox);
		/* If nothing subsetted (no textual content in that region), give up. */
		if (div == NULL)
			break;

		ret = 1;

		if (depth < MAX_ANALYSIS_DEPTH)
		{
			/* Can we subdivide that region any more? */
			if (boxer_subdivide(ctx, page, &div->first_block, &div->last_block, boxer, depth+1))
				break;
		}

#ifdef DEBUG_WRITE_AS_PS
		{
			int i, n;
			fz_rect *list;
			boxer_sort(boxer);
			n = boxer_results(boxer, &list);

			printf("%% SUBDIVISION\n");
			for (i = 0; i < n; i++)
			{
				printf("%% %g %g %g %g\n",
					list[i].x0, list[i].y0, list[i].x1, list[i].y1);
			}

			printf("0 0 0 setrgbcolor\n");
			for (i = 0; i < n; i++) {
				printf("%g %g moveto\n%g %g lineto\n%g %g lineto\n%g %g lineto\nclosepath\nstroke\n\n",
					list[i].x0, list[i].y0,
					list[i].x0, list[i].y1,
					list[i].x1, list[i].y1,
					list[i].x1, list[i].y0);
			}

			printf("1 0 0 setrgbcolor\n");
			printf("%g %g moveto\n%g %g lineto\n%g %g lineto\n%g %g lineto\nclosepath\nstroke\n\n",
				margins.x0, margins.y0,
				margins.x0, margins.y1,
				margins.x1, margins.y1,
				margins.x1, margins.y0);
		}
#endif
	}
	fz_always(ctx)
	{
		boxer_destroy(ctx, boxer1);
		boxer_destroy(ctx, boxer2);
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
			float margin = ch->size/2;
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

int fz_segment_stext_page(fz_context *ctx, fz_stext_page *page)
{
	boxer_t *boxer;
	fz_stext_block *block;
	int ret = 0;

	/* If we have structure already, give up. We can't hope to beat
	 * proper structure! */
	for (block = page->first_block; block != NULL; block = block->next)
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
			return 0;

#ifdef DEBUG_WRITE_AS_PS
	printf("1 -1 scale 0 -%g translate\n", page->mediabox.y1-page->mediabox.y0);
#endif

	boxer = boxer_create(ctx, &page->mediabox);

	fz_try(ctx)
	{
		/* Just walking the blocks is safe as we're assuming no structure here. */
		for (block = page->first_block; block != NULL; block = block->next)
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
			}
			}
		}

		ret = analyse_sub(ctx, page, &page->first_block, &page->last_block, boxer, 0);
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
