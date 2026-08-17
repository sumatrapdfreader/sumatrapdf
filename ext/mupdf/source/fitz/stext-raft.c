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

#include <assert.h>

static int
raftable(fz_stext_block *pos, fz_image_raft_options *options, fz_rect r)
{
	int w, h;
	float FUDGE = 0.1f;

	if (pos->u.i.image == NULL)
		return 0;

	w = pos->u.i.image->w;
	h = pos->u.i.image->h;

	if (w > options->max_size && h > options->max_size)
		return 0;

	return (pos->bbox.x1 >= r.x0 - FUDGE &&
		pos->bbox.x0 <= r.x1 + FUDGE &&
		pos->bbox.y1 >= r.y0 - FUDGE &&
		pos->bbox.y0 <= r.y1 + FUDGE);
}

static float
guess_res(fz_stext_block *block)
{
	/* unit square * T =  extent of image in dest space.
	 * So (1/w, 1/h) * T = extent of pixel in dest space.
	 */
	int w, h;
	fz_point one_pix;

	if (block->u.i.image == NULL)
		return 72.0f; /* No image, just a placeholder. */

	w = block->u.i.image->w;
	h = block->u.i.image->h;
	one_pix = fz_make_point(1.0f/w, 1.0f/h);

	one_pix = fz_transform_vector(one_pix, block->u.i.transform);

	if (one_pix.x < 0)
		one_pix.x = -one_pix.x;
	if (one_pix.y < 0)
		one_pix.y = -one_pix.y;

	if (one_pix.x < one_pix.y)
		one_pix.x = one_pix.y;

	return 72.0f/one_pix.x;
}

static fz_image *
combine_images(fz_context *ctx, fz_stext_page_block_iterator start, fz_stext_page_block_iterator end, float res, fz_matrix *inv, int mkimg, fz_colorspace *cs)
{
	fz_matrix scale = { res/72.0f, 0, 0, res/72.0f, 0, 0 };
	fz_stext_page_block_iterator pos;
	fz_rect r = fz_empty_rect;
	fz_pixmap *p;
	fz_device *dev = NULL;
	fz_image *im = NULL;
	fz_rect unit = {0, 0, 1, 1};

	for (pos = start;
		!(pos.block == end.block && pos.parent == end.parent);
		pos = fz_stext_page_block_iterator_next_dfs(pos))
	{
		fz_matrix m = fz_concat(pos.block->u.i.transform, scale);
		r = fz_union_rect(r, fz_transform_rect(unit, m));
	}

	scale.e -= r.x0;
	scale.f -= r.y0;

	*inv = fz_invert_matrix(scale);

	if (!mkimg)
		return NULL;

	p = fz_new_pixmap(ctx, cs, r.x1 - r.x0, r.y1 - r.y0, NULL, 1);

	fz_var(dev);
	fz_var(im);

	fz_try(ctx)
	{
		dev = fz_new_draw_device(ctx, scale, p);

		for (pos = start;
			!(pos.block == end.block && pos.parent == end.parent);
			pos = fz_stext_page_block_iterator_next_dfs(pos))
		{
			fz_fill_image(ctx, dev, pos.block->u.i.image, pos.block->u.i.transform, 1, fz_default_color_params);
		}

		fz_close_device(ctx, dev);

		im = fz_new_image_from_pixmap(ctx, p, NULL);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_pixmap(ctx, p);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return im;
}

void
fz_stext_raft_images(fz_context *ctx, fz_stext_page *stext, fz_image_raft_options *options)
{
	fz_stext_page_block_iterator iter, pos, end, next;
	fz_matrix inv;
	fz_colorspace *cs = NULL;

	for (iter = fz_stext_page_block_iterator_begin_dfs(stext);
		!fz_stext_page_block_iterator_eod_dfs(iter);
		iter = end)
	{
		int n;
		fz_rect r;
		float max_res, res;
		fz_image *im;
		int no_image;

		end = fz_stext_page_block_iterator_next_dfs(iter);

		if (iter.block->type != FZ_STEXT_BLOCK_IMAGE)
			continue;
		r = iter.block->bbox;
		if (!raftable(iter.block, options, r))
			continue;

		no_image = (iter.block->u.i.image == NULL);
		cs = no_image ? NULL : iter.block->u.i.image->colorspace;
		max_res = guess_res(iter.block);

		/* So we have a plausible starting position. Do we have stuff we can
		 * raft with it? */
		for (n = 0; !fz_stext_page_block_iterator_eod_dfs(end);
			n++, end = fz_stext_page_block_iterator_next_dfs(end))
		{
			if (end.block->type != FZ_STEXT_BLOCK_IMAGE)
				break;
			if (!raftable(end.block, options, r))
				break;
			if (no_image ^ (end.block->u.i.image == NULL))
			{
				/* Don't mix blocks with images and no-images. */
				break;
			}
			else if (no_image)
			{
				/* Neither has any colorspace, so they match. */
			}
			else if (cs != end.block->u.i.image->colorspace)
			{
				/* Colorspaces differ. */
				if (cs == NULL || end.block->u.i.image->colorspace == NULL)
				{
					/* Don't mix colorspaceless and colorspaced images together. */
					break;
				}
				/* Otherwise, prefer colorspaces with higher numbers of components.
				 * It's envisaged this will primarily allow mixing rafts of images
				 * with rgb and gray, or cmyk and gray, rather than rgb and cmyk.
				 * We might tweak this in future. */
				if (cs->n < end.block->u.i.image->colorspace->n)
					cs = end.block->u.i.image->colorspace;
			}
			r = fz_union_rect(r, end.block->bbox);
			res = guess_res(end.block);
			if (res > max_res)
				max_res = res;
		}

		/* If we didn't find any to raft, nothing to do. */
		if (n == 0)
			continue;

		/* So we need to raft everything from iter to end. */
		im = combine_images(ctx, iter, end, max_res, &inv, !no_image && options->combine_image, cs);

		/* Unlink all but the first. */
		for (pos = fz_stext_page_block_iterator_next_dfs(iter);
			!(pos.block == end.block && pos.parent == end.parent);
			pos = next)
		{
			next = fz_stext_page_block_iterator_next_dfs(pos);

			if (pos.block->prev)
				pos.block->prev->next = pos.block->next;
			else if (pos.parent)
				pos.parent->first_block = pos.block->next;
			else
				pos.page->first_block = pos.block->next;
			if (pos.block->next)
				pos.block->next->prev = pos.block->prev;
			else if (pos.parent)
				pos.parent->last_block = pos.block->prev;
			else
				pos.page->last_block = pos.block->next;
		}

		/* Now rewrite the first one */
		iter.block->bbox = r;
		iter.block->u.i.image = im;
		iter.block->u.i.transform = inv;
	}
}

/* A bunch of rafts makes a flotilla. */
typedef struct
{
	fz_rect area;
} fz_raft;

struct fz_flotilla
{
	int len;
	int max;
	fz_raft *rafts;
};

fz_flotilla *
fz_new_flotilla(fz_context *ctx)
{
	return fz_malloc_struct(ctx, fz_flotilla);
}

void
fz_drop_flotilla(fz_context *ctx, fz_flotilla *f)
{
	if (!f)
		return;
	fz_free(ctx, f->rafts);
	fz_free(ctx, f);
}

static void
add_raft_to_flotilla(fz_context *ctx, fz_flotilla *f, fz_raft r)
{
	if (f->len == f->max)
	{
		int newmax = f->max * 2;
		if (newmax == 0)
			newmax = 8;
		f->rafts = (fz_raft *) fz_realloc(ctx, f->rafts, sizeof(f->rafts[0]) * newmax);
		f->max = newmax;
	}

	f->rafts[f->len++] = r;
}

/* Without FUDGE this would be equivalent to:
 * fz_is_valid_rect(fz_intersect_rect(r, s))
 * but faster.
 *
 * With FUDGE it is slightly forgiving to allow for FP inaccuracies.
 */
 #define FUDGE 0.1f
static int
overlap_or_abut(fz_rect r, fz_rect s)
{
	return (r.x1 >= s.x0 - FUDGE && r.x0 <= s.x1 + FUDGE && r.y1 >= s.y0 - FUDGE && r.y0 <= s.y1 + FUDGE);
}

#ifdef DEBUG_RAFT
static void
verify(fz_context *ctx, fz_flotilla *f)
{
	int i, j;

	printf("Dump: len=%d\n", f->len);
	for (i = 0; i < f->len; i++)
	{
		printf("%d: %g %g %g %g\n", i, f->rafts[i].area.x0, f->rafts[i].area.y0, f->rafts[i].area.x1, f->rafts[i].area.y1);
	}

	for (i = 0; i < f->len-1; i++)
	{
		for (j = i+1; j < f->len; j++)
		{
			if (overlap_or_abut(f->rafts[i].area, f->rafts[j].area))
			{
				printf("%d and %d overlap!\n", i, j);
				assert(i == j);
			}
		}
	}
}
#endif

static void
add_plank_to_flotilla(fz_context *ctx, fz_flotilla *f, fz_rect rect)
{
	int i, j;

#ifdef DEBUG_RAFT
	verify(ctx, f);

	printf("%g %g %g %g\n", rect.x0, rect.y0, rect.x1, rect.y1);
#endif

	/* Does the plank extend any of the existing rafts? */
	for (i = f->len-1; i >= 0; i--)
	{
		if (overlap_or_abut(rect, f->rafts[i].area))
		{
			/* We overlap. */
			fz_rect r = fz_union_rect(f->rafts[i].area, rect);
			if (r.x0 == f->rafts[i].area.x0 &&
				r.y0 == f->rafts[i].area.y0 &&
				r.x1 == f->rafts[i].area.x1 &&
				r.y1 == f->rafts[i].area.y1)
			{
				/* We were entirely contained. Nothing more to do. */
#ifdef DEBUG_RAFT
				printf("Contained\n");
#endif
				return;
			}
			f->rafts[i].area = r;
#ifdef DEBUG_RAFT
			printf("Overlap %d -> %g %g %g %g\n", i, r.x0, r.y0, r.x1, r.y1);
#endif
			break;
		}
	}

	if (i >= 0)
	{
		/* We've extended raft[i]. We now need to check if any other raft overlaps
		 * with the extended one. */

		/* But our new one might have bridged between two (or more!) existing rafts. */
		/* Unfortunately, if we bridge between 2 rafts, that new larger raft might now
		 * intersect with more rafts. So we need to repeatedly scan. */
		while (1)
		{
			int changed = 0;

			for (j = f->len-1; j > i; j--)
			{
				if (overlap_or_abut(f->rafts[j].area, f->rafts[i].area))
				{
					/* Update raft i to be the union of the two. */
					f->rafts[i].area = fz_union_rect(f->rafts[j].area, f->rafts[i].area);
#ifdef DEBUG_RAFT
					printf("Bridge %d -> %g %g %g %g\n", j, f->rafts[i].area.x0, f->rafts[i].area.y0, f->rafts[i].area.x1, f->rafts[i].area.y1);
#endif
					/* Shorten the list by moving the end one down to be the ith one. */
					f->len--;
					if (j != f->len)
					{
						f->rafts[j] = f->rafts[f->len];
					}
					changed = 1;
				}
			}

			for (j = i-1; j >= 0; j--)
			{
				if (overlap_or_abut(f->rafts[j].area, f->rafts[i].area))
				{
					/* Update raft j to be the union of the two. */
					f->rafts[j].area = fz_union_rect(f->rafts[j].area, f->rafts[i].area);
#ifdef DEBUG_RAFT
					printf("Bridge %d -> %g %g %g %g\n", j, f->rafts[j].area.x0, f->rafts[j].area.y0, f->rafts[j].area.x1, f->rafts[j].area.y1);
#endif
					/* Shorten the list by moving the end one down to be the ith one. */
					f->len--;
					if (i != f->len)
					{
						f->rafts[i] = f->rafts[f->len];
					}
					i = j;
					changed = 1;
				}
			}

			if (!changed)
				break;
		}
	}
	else
	{
		/* This didn't overlap anything else. Make a new raft. */
		fz_raft raft = { rect };
		add_raft_to_flotilla(ctx, f, raft);
	}
}

fz_flotilla *
fz_new_flotilla_from_stext_page_vectors(fz_context *ctx, fz_stext_page *page)
{
	fz_flotilla *flot = fz_new_flotilla(ctx);
	fz_stext_page_block_iterator it;

	fz_try(ctx)
	{
		for (it = fz_stext_page_block_iterator_begin_dfs(page);
			!fz_stext_page_block_iterator_eod_dfs(it);
			it = fz_stext_page_block_iterator_next_dfs(it))
		{
			if (it.block->type != FZ_STEXT_BLOCK_VECTOR)
				continue;
			if (!(it.block->u.v.flags & FZ_STEXT_VECTOR_IS_RECTANGLE))
				continue;
			add_plank_to_flotilla(ctx, flot, it.block->bbox);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_flotilla(ctx, flot);
		fz_rethrow(ctx);
	}

	return flot;
}

int
fz_flotilla_size(fz_context *ctx, fz_flotilla *flot)
{
	return flot ? flot->len : 0;
}

fz_rect
fz_flotilla_raft_area(fz_context *ctx, fz_flotilla *flot, int i)
{
	if (flot == NULL || i < 0 || i >= flot->len)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "out of range raft in flotilla");
	return flot->rafts[i].area;
}
