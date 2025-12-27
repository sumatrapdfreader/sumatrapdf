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

/*
	We use this operation twice below, hence it's a separate function,
	but it seems a bit odd, so it's not exposed as an API one yet.
*/
static fz_stext_page_block_iterator
fz_stext_page_block_iterator_next_or_up(fz_stext_page_block_iterator pos)
{
	while (1)
	{
		pos = fz_stext_page_block_iterator_next(pos);
		/* If we hit the end, and we still have further to go, step up. */
		if (pos.block == NULL && pos.parent != NULL)
		{
			pos = fz_stext_page_block_iterator_up(pos);
			continue;
		}
		break;
	}

	return pos;
}

fz_stext_block *
insert_new_struct(fz_context *ctx, fz_stext_page_block_iterator pos, fz_structure classification)
{
	fz_stext_block *newblock;
	fz_stext_block *b;
	int index = 0;

	/* Horrible. Calculate an index value. */
	b = pos.parent ? pos.parent->first_block : pos.page->first_block;
	while (b != pos.block)
	{
		if (b->type == FZ_STEXT_BLOCK_STRUCT)
			index++;
		b = b->next;
	}

	b = b->next;
	while (b)
	{
		if (b->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (b->u.s.index >= index)
				break;
			b->u.s.index = ++index;
		}
		b = b->next;
	}

	/* We are going to need to create a new block. Create a complete unlinked one here. */
	newblock = fz_new_stext_struct(ctx, pos.page, classification, fz_structure_to_string(classification), index);
	newblock->id = pos.block->id;

	/* Insert the newblock before pos.block. */
	newblock->prev = pos.block->prev;
	if (pos.block->prev)
		pos.block->prev->next = newblock;
	else if (pos.parent)
		pos.parent->first_block = newblock;
	else
		pos.page->first_block = newblock;
	newblock->next = pos.block;
	pos.block->prev = newblock;

	return newblock;
}

static fz_stext_block *
relink_block_and_next(fz_stext_page_block_iterator pos, fz_stext_block *newblock)
{
	fz_stext_block *b = pos.block;
	fz_stext_block *next = b->next;

	/* Unlink b (pos.block) from it's current position. */
	if (b->prev)
		b->prev->next = b->next;
	else if (pos.parent)
		pos.parent->first_block = b->next;
	else
		pos.page->first_block = b->next;

	if (b->next)
		b->next->prev = b->prev;
	else if (pos.parent)
		pos.parent->last_block = b->prev;
	else
		pos.page->last_block = b->prev;

	/* Now relink b at the tail of newblock's children */

	if (newblock->u.s.down->first_block == NULL)
	{
		newblock->u.s.down->first_block = b;
		newblock->u.s.down->last_block = b;
		b->prev = NULL;
		b->next = NULL;
	}
	else
	{
		b->prev = newblock->u.s.down->last_block;
		newblock->u.s.down->last_block->next = b;
		newblock->u.s.down->last_block = b;
		b->next = NULL;
	}

	return next;
}

enum
{
	SPLIT_OK = 0,
	NO_SPLIT_ALL_IN = 1,
	NO_SPLIT_ALL_OUT = 2
};

static int
split_text_by_rect(fz_context *ctx, fz_stext_page_block_iterator pos, fz_rect rect)
{
	fz_stext_block *b = pos.block;
	fz_stext_line *line;
	fz_stext_char *ch;
	int all_in = 1;
	int all_out = 1;
	int first_char_is_in = -1;
	fz_stext_block *newblock;
	fz_stext_line *next_line;

	/* Walk through the block, looking for if we have stuff that falls significantly outside the rect. */
	for (line = b->u.t.first_line; line != NULL; line = line->next)
	{
		if (fz_contains_rect(rect, line->bbox))
		{
			/* All this line is inside */
			all_out = 0;
			if (first_char_is_in == -1)
				first_char_is_in = 1;
			break;
		}
		if (!fz_overlaps_rect(rect, line->bbox))
		{
			/* None of this line is inside */
			if (first_char_is_in == -1)
				first_char_is_in = 0;
			all_in = 0;
			continue;
		}
		for (ch = line->first_char; ch != NULL; ch = ch->next)
		{
			fz_rect r = fz_rect_from_quad(ch->quad);
			if (r.x0 < rect.x0 || r.y0 < rect.y0 || r.x1 > rect.x1 || r.y1 > rect.y1)
			{
				/* This char is at least partly outside rect. */
				fz_rect intersect = fz_intersect_rect(r, rect);
				float iarea = (intersect.x1 - intersect.x0) * (intersect.y1 - intersect.y0);
				float area = (r.x1 - r.x0) * (r.y1 - r.y0);
				if (iarea * 2 < area)
				{
					/* Less than half the char is inside. */
					all_in = 0;
					if (first_char_is_in == -1)
						first_char_is_in = 0;
				}
				else
				{
					/* At least half the char is inside. */
					all_out = 0;
					if (first_char_is_in == -1)
						first_char_is_in = 1;
				}
			}
		}
	}

	/* If all the content is in, or all the content is out, no need to split. */
	if (all_in == 1)
		return NO_SPLIT_ALL_IN;
	if (all_out == 1)
		return NO_SPLIT_ALL_OUT;
	assert(first_char_is_in != -1);

	newblock = fz_pool_alloc(ctx, pos.page->pool, sizeof *pos.page->first_block);
	newblock->bbox = fz_empty_rect;
	newblock->id = pos.block->id;
	newblock->prev = NULL;
	newblock->next = NULL;
	newblock->type = FZ_STEXT_BLOCK_TEXT;
	newblock->u.t.first_line = NULL;
	newblock->u.t.flags = pos.block->u.t.flags;
	newblock->u.t.last_line = NULL;

	/* We are going to move chars/lines that fall entirely within the rectangle into a
	 * new block. */
	if (first_char_is_in)
	{
		/* The very first char goes into our new block; our new block should go first. */
		newblock->prev = pos.block->prev;
		if (pos.block->prev)
			pos.block->prev->next = newblock;
		else if (pos.parent)
			pos.parent->first_block = newblock;
		else
			pos.page->first_block = newblock;
		newblock->next = pos.block;
		pos.block->prev = newblock;
	}
	else
	{
		/* The very first char will not be moved; our new block should go after pos.block. */
		newblock->prev = pos.block;
		newblock->next = pos.block->next;
		pos.block->next = newblock;
		if (newblock->next)
			newblock->next->prev = newblock;
		else if (pos.parent)
			pos.parent->last_block = newblock;
		else
			pos.page->last_block = newblock;
	}

	/* Walk through the block, copying stuff. */
	for (line = b->u.t.first_line; line != NULL; line = next_line)
	{
		fz_stext_line *new_line;
		fz_stext_char **next_chp;
		fz_stext_char **chp;
		next_line = line->next;
		if (fz_contains_rect(rect, line->bbox))
		{
			/* All this line is inside. */
			/* Unlink line */
			if (line->prev)
				line->prev->next = line->next;
			else
				b->u.t.first_line = line->next;
			if (line->next)
				line->next->prev = line->prev;
			else
				b->u.t.last_line = line->prev;

			/* Relink line. */
			line->prev = newblock->u.t.last_line;
			newblock->u.t.last_line = line;
			if (line->prev)
				line->prev->next = line;
			else
				newblock->u.t.first_line = line;
			line->next = NULL;

			continue;
		}
		if (!fz_overlaps_rect(rect, line->bbox))
		{
			/* None of this line is inside */
			continue;
		}
		/* Some of this line might be inside. */
		new_line = NULL;
		for (chp = &line->first_char; *chp != NULL; chp = next_chp)
		{
			fz_rect r;
			ch = (*chp);
			r = fz_rect_from_quad(ch->quad);
			next_chp = &ch->next;
			if (r.x0 < rect.x0 || r.y0 < rect.y0 || r.x1 > rect.x1 || r.y1 > rect.y1)
			{
				/* This char is at least partly outside rect. */
				fz_rect intersect = fz_intersect_rect(r, rect);
				float iarea = (intersect.x1 - intersect.x0) * (intersect.y1 - intersect.y0);
				float area = (r.x1 - r.x0) * (r.y1 - r.y0);
				if (iarea * 2 < area)
					continue; /* Less than half the char is inside. Leave that alone. */
			}
			/* We want to move this char to our new block. */
			if (new_line == NULL)
			{
				new_line = fz_pool_alloc(ctx, pos.page->pool, sizeof(*new_line));
				new_line->bbox = fz_empty_rect;
				new_line->dir = line->dir;
				new_line->flags = line->flags;
				new_line->wmode = line->wmode;
			}
			/* Unlink char */
			*chp = ch->next;
			next_chp = chp;
			/* Relink char */
			if (new_line->last_char == NULL)
				new_line->first_char = ch;
			else
				new_line->last_char->next = ch;
			new_line->last_char = ch;
			ch->next = NULL;
			new_line->bbox = fz_union_rect(new_line->bbox, fz_rect_from_quad(ch->quad));
		}
		if (new_line)
		{
			/* Insert our new line. */
			newblock->bbox = fz_union_rect(newblock->bbox, new_line->bbox);
			if (newblock->u.t.last_line)
				newblock->u.t.last_line->next = new_line;
			else
				newblock->u.t.first_line = new_line;
			new_line->prev = newblock->u.t.last_line;
			newblock->u.t.last_line = new_line;
			if (line->last_char == NULL)
			{
				/* All the chars were moved. Rare, but not impossible. */
				line->first_char = NULL;
				if (line->prev)
					line->prev->next = line->next;
				else
					b->u.t.first_line = line->next;
				if (line->next)
					line->next->prev = line->prev;
				else
					b->u.t.last_line = line->prev;
				/* line is lost, but in the pool, so will be freed properly. */
			}
			else
			{
				/* At least some chars were moved. Must recalculate line->bbox. */
				line->bbox = fz_empty_rect;
				for (ch = line->first_char; ch != NULL; ch = ch->next)
					line->bbox = fz_union_rect(line->bbox, fz_rect_from_quad(ch->quad));
			}
		}
	}

	return SPLIT_OK;
}

void
fz_classify_stext_rect(fz_context *ctx, fz_stext_page *page, fz_structure classification, fz_rect rect)
{
	fz_stext_page_block_iterator pos = fz_stext_page_block_iterator_begin(page);
	fz_stext_block *newblock;

	while (pos.block != NULL)
	{
		/* Walk the tree until we find a block that overlaps rect. */
		while (pos.block != NULL)
		{
			if (fz_contains_rect(rect, pos.block->bbox))
			{
				/* This node is entirely contained by rect. */
				break;
			}
			else if (fz_overlaps_rect(pos.block->bbox, rect))
			{
				/* rect overlaps with the current nodes bbox. (This includes the
				 * case where rect is contained by this node, but is not restricted
				 * to it) */
				/* Note: "if", not "switch" here, to allow us to break out of the while! */
				if (pos.block->type == FZ_STEXT_BLOCK_STRUCT)
				{
					/* It's a struct node. */
					pos = fz_stext_page_block_iterator_down(pos);
					/* Can't possibly step down any further, so this is it. */
					if (pos.block == NULL)
						break;
					continue;
				}
				else if (pos.block->type == FZ_STEXT_BLOCK_TEXT)
				{
					/* Split pos.block by rect, and then continue to make sure we pick the right fragment. */
					int split = split_text_by_rect(ctx, pos, rect);
					if (split == SPLIT_OK)
						continue; /* We split the node successfully. Loop back to consider the 2 halves again. */
					if (split == NO_SPLIT_ALL_IN)
						break; /* We didn't need to split, because all the content is actually included. */
					assert(split == NO_SPLIT_ALL_OUT);
					/* Nothing split, as it was all excluded. Keep walking. */
				}
				else
				{
					/* Any other type of node needs to be captured. */
					/* FIXME: Maybe do an area test here, to only take 50% covered stuff? */
					break;
				}
			}
			/* Step to the next block (maybe moving up, but never down) */
			pos = fz_stext_page_block_iterator_next_or_up(pos);
		}

		/* If we've hit the end, we have nothing left to do. */
		if (pos.block == NULL)
			return;

		/* So pos.block overlaps our rect. We need to make a struct node and put stuff
		 * (starting with pos.block, but maybe continuing after it) inside that. */
		newblock = insert_new_struct(ctx, pos, classification);

		/* Walk the tree appending stuff onto newblock until we find a block that doesn't overlap rect. */
		while (pos.block)
		{
			fz_rect overlap;
			if (fz_contains_rect(rect, pos.block->bbox))
			{
				/* This node is entirely contained by rect. */
				pos.block = relink_block_and_next(pos, newblock);
				continue;
			}

			overlap = fz_intersect_rect(rect, pos.block->bbox);

			if (!fz_is_empty_rect(overlap))
			{
				float area, area2;
				/* rect overlaps this node */
				if (pos.block->type == FZ_STEXT_BLOCK_TEXT)
				{
					/* Split pos.block by rect, and then continue to make sure we pick the right fragment. */
					int split = split_text_by_rect(ctx, pos, rect);
					if (split == SPLIT_OK)
						continue; /* We split the node successfully. Loop back to reconsider the fragments. */
					/* Nothing split off, as everything was excluded. Stop the search. */
					if (split == NO_SPLIT_ALL_OUT)
						break;
					/* No split, as everything was included. */
					assert(split == NO_SPLIT_ALL_IN);
				}
				area = (overlap.x1 - overlap.x0) * (overlap.y1 - overlap.y0);
				area2 = (pos.block->bbox.x1 - pos.block->bbox.x0) * (pos.block->bbox.y1 - pos.block->bbox.y0);
				if (area2 < 2 * area)
				{
					/* More than 50% of this block is overlapped. We'll take it. */
					pos.block = relink_block_and_next(pos, newblock);
					continue;
				}
			}
			break;
		}

		/* We've hit the end of this run. */
		/* Step to the next block (maybe moving up, but never down) */
		pos = fz_stext_page_block_iterator_next_or_up(pos);
	}
}
