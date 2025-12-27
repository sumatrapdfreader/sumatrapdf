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

/* #define DEBUG_WRITE_AS_PS */

/* #define DEBUG_TABLE_STRUCTURE */

/* #define DEBUG_TABLE_HUNT */

/*
 * The algorithm.
 *
 *	The goal of the algorithm is to identify tables on a page.
 *	First we have to find the tables on a page, then we have to
 *	figure out where the columns/rows are, and then how the
 *	cells span them.
 *
 *	We do this as a series of steps.
 *
 *	To illustrate what's going on, let's use an example page
 *	that we can follow through all the steps.
 *
 *	+---------------------------+
 *	|                           |
 *	|      #### ## ### ##       |	<- Title
 *	|                           |
 *	|    ##### ##### #### ##    |		\
 *	|    ## ###### ###### ##    |    |
 *	|    #### ####### ######    |    |- Abstract
 *	|    ####### #### ## ###    |    |
 *	|    ### ##### ######       |   /
 *	|                           |
 *	|   #########   #########   |   2 Columns of text
 *	|   #########   #########   |
 *	|   ########    #########   |
 *	|               #########   |
 *	|   +-------+   #######     |   <- With an image on the left
 *	|   |       |               |
 *	|   |       |   ## ## # #   |   <- And a table on the right
 *	|   +-------+   ## ## # #   |
 *	|               ## ## # #   |
 *	|   #########   ## ## # #   |
 *	|   #########   ## ## # #   |
 *	|   #########   ## ## # #   |
 *	|                           |
 *	+---------------------------+
 *
 *
 * Step 1: Segmentation.
 *
 *	First, we segment the page, trying to break it down into a
 *	series of non-overlapping rectangles. We do this (in stext-boxer.c)
 *	by looking for where the content isn't. If we can identify breaks
 *	that run through the page (either from top to bottom or from left
 *	to right), then we can split the page there, and recursively consider
 *	the two halves of the page.
 *
 *	It's not a perfect algorithm, but it manages to in many cases.
 *
 *	After segmenting the above example, first we'll find the horizontal
 *	splits, giving:
 *
 *	+---------------------------+
 *	|                           |
 *	|      #### ## ### ##       |
 *	+---------------------------+
 *	|    ##### ##### #### ##    |
 *	|    ## ###### ###### ##    |
 *	|    #### ####### ######    |
 *	|    ####### #### ## ###    |
 *	|    ### ##### ######       |
 *	+---------------------------+
 *	|   #########   #########   |
 *	|   #########   #########   |
 *	|   ########    #########   |
 *	|               #########   |
 *	|   +-------+   #######     |
 *	|   |       |               |
 *	|   |       |   ## ## # #   |
 *	|   +-------+   ## ## # #   |
 *	|               ## ## # #   |
 *	|   #########   ## ## # #   |
 *	|   #########   ## ## # #   |
 *	|   #########   ## ## # #   |
 *	|                           |
 *	+---------------------------+
 *
 *	Then we'll recurse and find the vertical split between
 *	the columns:
 *
 *	+---------------------------+
 *	|                           |
 *	|      #### ## ### ##       |
 *	+---------------------------+
 *	|    ##### ##### #### ##    |
 *	|    ## ###### ###### ##    |
 *	|    #### ####### ######    |
 *	|    ####### #### ## ###    |
 *	|    ### ##### ######       |
 *	+-------------+-------------+
 *	|   ######### | #########   |
 *	|   ######### | #########   |
 *	|   ########  | #########   |
 *	|             | #########   |
 *	|   +-------+ | #######     |
 *	|   |       | |             |
 *	|   |       | | ## ## # #   |
 *	|   +-------+ | ## ## # #   |
 *	|             | ## ## # #   |
 *	|   ######### | ## ## # #   |
 *	|   ######### | ## ## # #   |
 *	|   ######### | ## ## # #   |
 *	|             |             |
 *	+-------------+-------------+
 *
 *	Then we recurse again and find the horizontal splits
 *	within the columns:
 *
 *	+---------------------------+
 *	|                           |
 *	|      #### ## ### ##       |
 *	+---------------------------+
 *	|    ##### ##### #### ##    |
 *	|    ## ###### ###### ##    |
 *	|    #### ####### ######    |
 *	|    ####### #### ## ###    |
 *	|    ### ##### ######       |
 *	+-------------+-------------+
 *	|   ######### | #########   |
 *	|   ######### | #########   |
 *	|   ########  | #########   |
 *	+-------------+ #########   |
 *	|   +-------+ | #######     |
 *	|   |       | +-------------+
 *	|   |       | | ## ## # #   |
 *	|   +-------+ | ## ## # #   |
 *	+-------------+ ## ## # #   |
 *	|   ######### | ## ## # #   |
 *	|   ######### | ## ## # #   |
 *	|   ######### | ## ## # #   |
 *	|             |             |
 *	+-------------+-------------+
 *
 *	We recurse a fixed maximum number of times (currently
 *	6, IIRC) or until we fail to find any suitable splits.
 *
 *	This completes the page segmentation step.
 *
 * Step 2: Grid finding
 *
 *	Next, we look at each of those segments and try to identify
 *	where grids might be.
 *
 *	Imagine the bottom right section of that page as
 *	a board with lego blocks on where there is text.
 *	Now imagine viewing that from the bottom of the page.
 *	The gaps between the columns of the table are where you
 *	can see through to the top between the blocks.
 *
 *	Similarly, if you view it from the side, the gaps between the
 *	rows of the page are where you can see through to the other
 *	side.
 *
 *	So, how do we code that? Well, we run through the page content
 *	(obviously, restricted to the content that falls into this
 *	segment of the page - that'll go without saying from here on
 *	in). For each bit of content, we look at the "x extent" of that
 *	content - for instance a given string might start at position
 *	10 and continue to position 100. We build a list of all these
 *	start, and stop positions, and keep them in a sorted list.
 *
 *	Then we walk this list from left to right, keeping a sum. I
 *	call this sum "wind", because it's very similar to the winding
 *	number that you get when doing scan conversion of bezier shapes.
 *
 *	wind starts out as 0. We increment it whenever we pass a 'start'
 *	position, and decrement it whenever we pass a 'stop' position.
 *	So at any given x position along the line wind tells us the
 *	number of pieces of content that overlap that x position.
 *	So wind(left) = 0 = wind(right), and wind(x) >= x for all x.
 *
 *	So, if we walk from left to right, the trace of wind might
 *	look something like:
 *
 *	             __
 *	  ___       /  \  _        __
 *	 /   \     /    \/ \     _/  \_
 *	/     \___/         \___/      \
 *
 *	The left and right edges of the table are pretty clear.
 *	The regions where wind drops to 0 represent the column dividers.
 *	The left and right hand side of those regions gives us the min
 *	and max values for that divider.
 *
 *	We can then repeat this process for Y ranges instead of X ranges
 *	to get the row dividers.
 *
 *	BUT, this only works for pure grid tables. It falls down for
 *	cases where we have merged cells (which is very common due to
 *	titles etc).
 *
 *	We can modify the algorithm slightly to allow for this.
 *
 *	Consider the following table:
 *
 *	+-----------------------------------+
 *	|  Long Table title across the top  |
 *	+---------------+---------+---------+
 *	| Name          | Result1 | Result2 |
 *	+---------------+----+----+----+----+
 *	| Homer Simpson |  1 | 23 |  4 | 56 |
 *	| Barney Gumble |  1 | 23 |  4 | 56 |
 *	| Moe           |  1 | 23 |  4 | 56 |
 *	| Apu           |  1 | 23 |  4 | 56 |
 *	| Ned Flanders  |  1 | 23 |  4 | 56 |
 *	+---------------+----+----+----+----+
 *
 *	The wind trace for that looks something like
 *	(with a certain degree of artistic license for the
 *	limitations of ascii art):
 *
 *	   ________
 *	  /        \      _   __     _   _
 *	 /          \____/ \_/  \___/ \_/ \
 *	/                                  \
 *
 *	So, the trace never quite drops back to zero in the
 *	middle due to the spanning of the top title.
 *
 *	So, instead of just looking for points where the trace
 *	drops to zero, we instead look for local minima. Each local
 *	minima represents a place where there might be a grid divider.
 *	The value of wind at such points can be considered the
 *	"uncertainty" with which there might be a divider there.
 *	Clear dividers (with a wind value of 0) have no uncertainty.
 *	Places where cells are spanned have a higher value of uncertainty.
 *
 *	The output from this step is the list of possible grid positions
 *	(X and Y), with uncertainty values.
 *
 *	We have skipped over the handling of spaces in the above
 *	description. On the one hand, we'd quite like to treat
 *	sentences as long unbroken regions of content. But sometimes
 *	we can get awkward content like:
 *
 *	P subtelomeric                 24.38 8.15  11.31 0.94   1.46
 *	                               11.08 6.93  15.34 0.00   0.73
 *	Pericentromeric region; C band 20.42 9.26  13.64 0.81   0.81
 *	                               16.50 7.03  14.45 0.17   5.15

 *	where the content is frequently sent with spaces instead of
 *	small gaps (particular in tables of digits, because numerals
 *	are often the same width even in proportional fonts).
 *
 *	To cope with this, as we send potential edges into the start/stop
 *	positions, we send 'weak' edges for the start and stops of runs
 *	of spaces. We then post process the edges to remove any local
 *	minima regions in the 'wind' values that are bounded purely by
 *	'weak' edges.
 *
 * Step 3: Cell analysis
 *
 *	So, armed with the output from step 2, we can examine each grid
 *	found. If we have W x-dividers and H y-dividers, we know we have
 *	a potential table with (W-1) x (H-1) cells in it.
 *
 *	We represent this as a W x H grid of cells, each like:
 *
 *	        .       .
 *	        .       .
 *	   . . .+-------+. . .	Each cell holds information about the
 *	        |       .	edges above, and to the left of it.
 *	        |       .
 *	        |       .
 *	   . . .+. . . .+. . .
 *	        .       .
 *	        .       .
 *
 *	h_line: Is there a horizontal divider drawn on the page that
 *	corresponds to the top of this cell (i.e. is there a cell border
 *	here?)
 *	v_line: Is there a vertical divider drawn on the page that
 *	corresponds to the left of this cell (i.e. is there a cell border
 *	here?)
 *	h_crossed: Does content cross this line (i.e. are we merged
 *	with the cell above?)
 *	v_crossed: Does content cross this line (i.e. are we merged
 *	with the cell to the left?)
 *	full: Is there any content in this cell at all?
 *
 *	We need a W x H grid of cells to represent the entire table due
 *	to the potential right and bottom edge lines. The right and
 *	bottom rows of cells should never be full, or be crossed, but
 *	it's easiest just to use a simple representation that copes with
 *	the h_line and v_line values naturally.
 *
 *	So, we start with the cells structure empty, and we run through
 *	the page content, filling in the details as we go.
 *
 *	At the end of the process, we have enough information to draw
 *	an asciiart representation of our table. It might look something
 *	like this (this comes from dotted-gridlines-tables.pdf):
 *
 *	+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *	| | |   | | |#|#|#|#| | | |
 *	+ + + + + + +v+ +v+v+ + + +
 *	| | |   | | |#|#|#|#| | | |
 *	+ + + + + + + +v+ + + + + +
 *	| |#|   | |#|#|#|#|#|#|#|#|
 *	+ +v+ + + +v+v+ +v+v+v+v+v+
 *	| |#|   |#|#|#|#|#|#|#|#|#|
 *	+ + + + +v+ + +v+ + + + + +
 *	|#|#|  #|#|#|#|#|#|#|#|#|#|
 *	+v+v+ +v+ +v+v+ +v+v+v+v+v+
 *	|#|#|  #|#|#|#|#|#|#|#|#|#|
 *	+ + + + +v+ + +v+ + + + + +
 *	| |#|   |#|#|#|#|#|#|#|#|#|
 *	+ +v+ + + +v+v+ +v+v+v+v+v+
 *	| |#|   | |#|#|#|#|#|#|#|#|
 *	+ + + + + + + +v+ + + + + +
 *	| | |   | | |#|#|#|#| | | |
 *	+ + + + + + +v+ +v+v+ + + +
 *	| | |   | | |#|#|#|#| | | |
 *	+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#  |#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#  |#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#>#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#>#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#>#|#| |#| | | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#  |#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#  |#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#  |#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#  |#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *	  |#    |#|#|#|#|#|#|#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *	This shows where lines are detected ( - and | ),
 *	where they are crossed ( > and v) and where cells
 *	are full ( # ).
 *
 * Step 4: Row and column merging.
 *
 *	Based on the information above, we then try to merge
 *	cells and columns to simplify the table.
 *
 *	The best rules I've come up with this so far are:
 *	We can merge two adjacent columns if all the pairs of
 *	cells in the two columns are mergeable.
 *
 *	Cells are held to be mergeable or not based upon the following
 *	rules:
 *		If there is a line between 2 cells - not mergeable.
 *		else if the uncertainty between 2 cells is 0 - not mergeable.
 *		else if the line between the 2 cells is crossed - mergeable.
 *		else if strictly one of the cells is full - mergeable.
 *		else not mergeable.
 *
 *	So in the above example, column 2 (numbered from 0) can be merged
 *	with column 3.
 *
 *	This gives:
 *
 *	+-+-+-+-+-+-+-+-+-+-+-+-+
 *	| | | | | |#|#|#|#| | | |
 *	+ + + + + +v+ +v+v+ + + +
 *	| | | | | |#|#|#|#| | | |
 *	+ + + + + + +v+ + + + + +
 *	| |#| | |#|#|#|#|#|#|#|#|
 *	+ +v+ + +v+v+ +v+v+v+v+v+
 *	| |#| |#|#|#|#|#|#|#|#|#|
 *	+ + + +v+ + +v+ + + + + +
 *	|#|#|#|#|#|#|#|#|#|#|#|#|
 *	+v+v+v+ +v+v+ +v+v+v+v+v+
 *	|#|#|#|#|#|#|#|#|#|#|#|#|
 *	+ + + +v+ + +v+ + + + + +
 *	| |#| |#|#|#|#|#|#|#|#|#|
 *	+ +v+ + +v+v+ +v+v+v+v+v+
 *	| |#| | |#|#|#|#|#|#|#|#|
 *	+ + + + + + +v+ + + + + +
 *	| | | | | |#|#|#|#| | | |
 *	+ + + + + +v+ +v+v+ + + +
 *	| | | | | |#|#|#|#| | | |
 *	+-+-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| |#| | | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  | |#|#| | |#| | |#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *	  |#  |#|#|#|#|#|#|#|#|#|
 *	+ +-+-+-+-+-+-+-+-+-+-+-+
 *
 *	We then perform the same merging process for rows as for
 *	columns - though there are no rows in the above example
 *	that can be merged.
 *
 *	You'll note that, for example, we don't merge row 0 and
 *	row 1 in the above, because we have a pair of cells that
 *	are both full without crossing.
 *
 * Step 5: Cell spanning
 *
 *	Now we actually start to output the table. We keep a 'sent_table'
 *	(a grid of W x H bools) to keep track of whether we've output
 *	the content for a given cell or not yet.
 *
 *	For each cell we reach, assuming sent_table[x,y] is false,
 *	we merge it with as many cells on the right as required,
 *	according to 'v_crossed' values (subject to not passing
 *	v_lines or uncertainty == 0's).
 *
 *	We then try to merge cells below according to 'h_crossed'
 *	values (subject to not passing h_lines or uncertainty == 0's).
 *
 *	In theory this can leave us with some cases where we'd like
 *	to merge some cells (because of crossed) and can't (because
 *	of lines or sent_table[]) values. In the absence of better
 *	cell spanning algorithms we have no choice here.
 *
 *	Then we output the contents and set sent_table[] values as
 *	appropriate.
 *
 *	If a row has no cells in it, we currently omit the TR. If/when
 *	we figure out how to indicate rowspan/colspan in stext, we can
 *	revisit that.
 */


static fz_stext_block *
add_grid_block(fz_context *ctx, fz_stext_page *page, fz_stext_block **first_block, fz_stext_block **last_block, int id)
{
	fz_stext_block *block = fz_pool_alloc(ctx, page->pool, sizeof(**first_block));
	memset(block, 0, sizeof(*block));
	block->type = FZ_STEXT_BLOCK_GRID;
	block->id = id;
	block->bbox = fz_empty_rect; /* Fixes bug 703267. */
	block->next = *first_block;
	if (*first_block)
	{
		(*first_block)->prev = block;
		assert(*last_block);
	}
	else
	{
		assert(*last_block == NULL);
		*last_block = block;
	}
	*first_block = block;
	return block;
}

static void
insert_block_before(fz_stext_block *block, fz_stext_block *before, fz_stext_page *page, fz_stext_struct *dest)
{
	if (before)
	{
		/* We have a block to insert it before, so we know it's not the last. */
		assert(dest ? (dest->first_block != NULL && dest->last_block != NULL) : (page->first_block != NULL && page->last_block != NULL));
		block->next = before;
		block->prev = before->prev;
		if (before->prev)
		{
			assert(before->prev->next == before);
			before->prev->next = block;
		}
		else if (dest)
		{
			assert(dest->first_block == before);
			dest->first_block = block;
		}
		else
		{
			assert(page->first_block == before);
			page->first_block = block;
		}
		before->prev = block;
	}
	else if (dest)
	{
		/* Will be the last block. */
		block->next = NULL;
		block->prev = dest->last_block;
		if (dest->last_block)
		{
			assert(dest->last_block->next == NULL);
			dest->last_block->next = block;
		}
		if (dest->first_block == NULL)
			dest->first_block = block;
		dest->last_block = block;
	}
	else
	{
		/* Will be the last block. */
		block->next = NULL;
		block->prev = page->last_block;
		if (page->last_block)
		{
			assert(page->last_block->next == NULL);
			page->last_block->next = block;
		}
		if (page->first_block == NULL)
			page->first_block = block;
		page->last_block = block;
	}
}

static fz_stext_struct *
add_struct_block_before(fz_context *ctx, fz_stext_block *before, fz_stext_page *page, fz_stext_struct *parent, fz_structure std, const char *raw)
{
	fz_stext_block *block;
	int idx = 0;
	size_t z;
	fz_stext_struct *newstruct;

	if (raw == NULL)
		raw = "";
	z = strlen(raw);

	/* We're going to insert a struct block. We need an idx, so walk the list */
	for (block = parent ? parent->first_block : page->first_block; block != before; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			assert(block->u.s.index >= idx);
			idx = block->u.s.index + 1;
		}
	}
	/* So we'll add our block as idx. But all the other struct blocks that follow us need to have
	 * larger values. */

	/* Update all the subsequent structs to have a higher idx */
	if (before)
	{
		int idx2 = idx+1;
		for (block = before->next; block != NULL; block = block->next)
		{
			if (block->type != FZ_STEXT_BLOCK_STRUCT)
				continue;
			if (block->u.s.index > idx2)
				break;
			block->u.s.index = idx2++;
		}
	}

	/* Now make our new struct block and insert it. */
	block = fz_pool_alloc(ctx, page->pool, sizeof(*block));
	block->type = FZ_STEXT_BLOCK_STRUCT;
	block->id = 0; /* Safe because we only work on single pages */
	block->bbox = fz_empty_rect; /* Fixes bug 703267. */
	insert_block_before(block, before, page, parent);

	block->u.s.down = newstruct = fz_pool_alloc(ctx, page->pool, sizeof(*block->u.s.down) + z);
	block->u.s.index = idx;
	newstruct->parent = parent;
	newstruct->standard = std;
	memcpy(newstruct->raw, raw, z);
	newstruct->raw[z] = 0;
	newstruct->up = block;

	return newstruct;
}

typedef struct
{
	int len;
	int max;
	struct {
		uint8_t left;
		uint8_t weak;
		float pos;
		int freq;
	} *list;
} div_list;

static void
div_list_push(fz_context *ctx, div_list *div, int left, int weak, float pos)
{
	int i;

	/* FIXME: Could be bsearch. */
	for (i = 0; i < div->len; i++)
	{
		if (div->list[i].pos > pos)
			break;
		else if (div->list[i].pos == pos)
		{
			if (div->list[i].left == left)
		{
			div->list[i].freq++;
			div->list[i].weak &= weak;
			return;
		}
			if (div->list[i].left == 0)
				break;
		}
	}

	if (div->len == div->max)
	{
		int newmax = div->max * 2;
		if (newmax == 0)
			newmax = 32;
		div->list = fz_realloc(ctx, div->list, sizeof(div->list[0]) * newmax);
		div->max = newmax;
	}

	if (i < div->len)
		memmove(&div->list[i+1], &div->list[i], sizeof(div->list[0]) * (div->len - i));
	div->len++;
	div->list[i].left = left;
	div->list[i].weak = weak;
	div->list[i].pos = pos;
	div->list[i].freq = 1;
}

static fz_stext_grid_positions *
make_table_positions(fz_context *ctx, div_list *xs, float min, float max)
{
	int wind;
	fz_stext_grid_positions *pos;
	int len = xs->len;
	int i;
	int hi = 0;

	/* Count the number of edges */
	int local_min = 0;
	int edges = 2;

	if (len == 0)
		return NULL;

	assert(xs->list[0].left);
	for (i = 0; i < len; i++)
	{
		if (xs->list[i].pos >= min)
			break;
	}
	for (; i < len; i++)
	{
		if (xs->list[i].pos >= max)
			break;
		if (xs->list[i].left)
		{
			if (local_min)
				edges++;
		}
		else
			local_min = 1;
	}
	assert(!xs->list[len-1].left);

	pos = fz_malloc_flexible(ctx, fz_stext_grid_positions, list, edges);
	pos->len = edges;

	/* Copy the edges in */
	wind = 0;
	local_min = 0;
	edges = 1;
	pos->list[0].pos = min;
	pos->list[0].min = min;
	pos->list[0].max = fz_max(xs->list[0].pos, min);
	pos->list[0].uncertainty = 0;
	pos->list[0].reinforcement = 0;
#ifdef DEBUG_TABLE_HUNT
	printf("|%g ", pos->list[0].pos);
#endif
	/* Skip over entries to the left of min. */
	for (i = 0; i < len; i++)
	{
		if (xs->list[i].pos >= min)
			break;
		if (xs->list[i].left)
			wind += xs->list[i].freq;
		else
			wind -= xs->list[i].freq;
	}
	for (; i < len; i++)
	{
		if (xs->list[i].pos >= max)
			break;
		if (xs->list[i].left)
		{
			if (local_min)
			{
				pos->list[edges].min = xs->list[i-1].pos;
				pos->list[edges].max = xs->list[i].pos;
				pos->list[edges].pos = (xs->list[i-1].pos + xs->list[i].pos)/2;
				pos->list[edges].uncertainty = wind;
#ifdef DEBUG_TABLE_HUNT
				if (wind)
					printf("?%g(%d) ", pos->list[edges].pos, wind);
				else
					printf("|%g ", pos->list[edges].pos);
#endif
				edges++;
			}
			wind += xs->list[i].freq;
			if (wind > hi)
				hi = wind;
		}
		else
		{
			wind -= xs->list[i].freq;
			local_min = 1;
		}
	}
	assert(i < len || wind == 0);
	pos->list[edges].pos = max;
	pos->list[edges].min = fz_min(xs->list[i-1].pos, max);
	pos->list[edges].max = max;
	assert(max >= xs->list[i-1].pos);
	pos->list[edges].uncertainty = 0;
	pos->list[edges].reinforcement = 0;
	pos->max_uncertainty = hi;
#ifdef DEBUG_TABLE_HUNT
	printf("|%g\n", pos->list[edges].pos);
#endif

	return pos;
}

static fz_stext_grid_positions *
copy_grid_positions_to_pool(fz_context *ctx, fz_stext_page *page, fz_stext_grid_positions *xs)
{
	size_t z = offsetof(fz_stext_grid_positions, list) + sizeof(xs->list[0]) * (xs->len);
	fz_stext_grid_positions *xs2 = fz_pool_alloc(ctx, page->pool, z);
	memcpy(xs2, xs, z);
	return xs2;
}

static void
sanitize_positions(fz_context *ctx, div_list *xs)
{
	int i, j, wind, changed;

#ifdef DEBUG_TABLE_HUNT
	printf("OK:\n");
	for (i = 0; i < xs->len; i++)
	{
		if (xs->list[i].left)
			printf("[");
		printf("%g(%d%s)", xs->list[i].pos, xs->list[i].freq, xs->list[i].weak ? "weak" : "");
		if (!xs->list[i].left)
			printf("]");
		printf(" ");
	}
	printf("\n");
#endif

	if (xs->len == 0)
		return;

	do
	{
		/* Now, combine runs of left and right */
		for (i = 0; i < xs->len; i++)
		{
			if (xs->list[i].left)
			{
				j = i;
				while (i < xs->len-1 && xs->list[i+1].left)
				{
					i++;
					xs->list[j].freq += xs->list[i].freq;
					xs->list[j].weak &= xs->list[i].weak;
					xs->list[i].freq = 0;
				}
			}
			else
			{
				while (i < xs->len-1 && !xs->list[i+1].left)
				{
					i++;
					xs->list[i].freq += xs->list[i-1].freq;
					xs->list[i].weak &= xs->list[i-1].weak;
					xs->list[i-1].freq = 0;
				}
			}
		}

#ifdef DEBUG_TABLE_HUNT
		printf("Shrunk:\n");
		for (i = 0; i < xs->len; i++)
		{
			if (xs->list[i].left)
				printf("[");
			printf("%g(%d%s)", xs->list[i].pos, xs->list[i].freq, xs->list[i].weak ? "weak" : "");
			if (!xs->list[i].left)
				printf("]");
			printf(" ");
		}
		printf("\n");
#endif

		/* Now remove the 0 frequency ones. */
		j = 0;
		for (i = 0; i < xs->len; i++)
		{
			if (xs->list[i].freq == 0)
				continue;
			if (i != j)
				xs->list[j] = xs->list[i];
			j++;
		}
		xs->len = j;

		/* Now run across looking for local minima where at least one
		 * edge is 'weak'. If the wind at that point is non-zero, then
		 * remove the weak edges from consideration and retry. */
		wind = 0;
		changed = 0;
		i = 0;
		while (1)
		{
			assert(xs->list[i].left);
			for (; xs->list[i].left; i++)
			{
				wind += xs->list[i].freq;
			}
			assert(i < xs->len);
			for (; xs->list[i].left == 0 && i < xs->len; i++)
			{
				wind -= xs->list[i].freq;
			}
			if (i == xs->len)
				break;
			if (wind != 0 && (xs->list[i-1].weak || xs->list[i].weak))
			{
				int m = fz_mini(xs->list[i-1].freq, xs->list[i].freq);
				assert(m > 0);
				xs->list[i-1].freq -= m;
				xs->list[i].freq -= m;
				changed = 1;
			}
		}
	}
	while (changed);

#ifdef DEBUG_TABLE_HUNT
	printf("Compacted:\n");
	for (i = 0; i < xs->len; i++)
	{
		if (xs->list[i].left)
			printf("[");
		printf("%g(%d%s)", xs->list[i].pos, xs->list[i].freq, xs->list[i].weak ? "weak" : "");
		if (!xs->list[i].left)
			printf("]");
		printf(" ");
	}
	printf("\n");
#endif
}

/* We want to check for whether a DIV that we are about to descend into
 * contains a column of justified text. We will accept some headers in
 * this text, but not JUST headers. */
static int
all_blocks_are_justified_or_headers(fz_context *ctx, fz_stext_block *block)
{
	int just_headers = 1;

	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down == NULL)
				continue;
			if (block->u.s.down->standard == FZ_STRUCTURE_H ||
				block->u.s.down->standard == FZ_STRUCTURE_H1 ||
				block->u.s.down->standard == FZ_STRUCTURE_H2 ||
				block->u.s.down->standard == FZ_STRUCTURE_H3 ||
				block->u.s.down->standard == FZ_STRUCTURE_H4 ||
				block->u.s.down->standard == FZ_STRUCTURE_H5 ||
				block->u.s.down->standard == FZ_STRUCTURE_H6)
				continue;
			if (!all_blocks_are_justified_or_headers(ctx, block->u.s.down->first_block))
				return 0;
		}
		just_headers = 0;
		if (block->type == FZ_STEXT_BLOCK_TEXT && block->u.t.flags != FZ_STEXT_TEXT_JUSTIFY_FULL)
			return 0;
	}

	if (just_headers)
		return 0;

	return 1;
}

#define TWO_INCHES (72*2)

/* Walk through the blocks, finding the bbox. */
static fz_rect
walk_to_find_bounds(fz_context *ctx, fz_stext_block *first_block)
{
	fz_rect bounds = fz_empty_rect;
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	for (block = first_block; block != NULL; block = block->next)
	{
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_STRUCT:
			if (!block->u.s.down)
				continue;
			if (block->u.s.down->standard == FZ_STRUCTURE_H)
			{
				if (block->next != NULL &&
					block->next->type == FZ_STEXT_BLOCK_TEXT &&
					block->next->u.t.flags == FZ_STEXT_TEXT_JUSTIFY_FULL)
					continue;
			}
			bounds = fz_union_rect(bounds, walk_to_find_bounds(ctx, block->u.s.down->first_block));
			break;
		case FZ_STEXT_BLOCK_VECTOR:
			bounds = fz_union_rect(bounds, block->bbox);
			break;
		case FZ_STEXT_BLOCK_TEXT:
			if (block->u.t.flags == FZ_STEXT_TEXT_JUSTIFY_FULL && block->bbox.x1 - block->bbox.x0 >= TWO_INCHES)
				continue;
			for (line = block->u.t.first_line; line != NULL; line = line->next)
			{
				for (ch = line->first_char; ch != NULL; ch = ch->next)
				{
					if (ch->c != ' ')
						bounds = fz_union_rect(bounds, fz_rect_from_quad(ch->quad));
				}
			}
			break;
		}
	}

	return bounds;
}

static void
walk_to_find_content(fz_context *ctx, div_list *xs, div_list *ys, fz_stext_block *first_block, fz_rect bounds)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	for (block = first_block; block != NULL; block = block->next)
	{
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_STRUCT:
			if (block->u.s.down && !fz_is_empty_rect(fz_intersect_rect(bounds, block->bbox)))
				walk_to_find_content(ctx, xs, ys, block->u.s.down->first_block, bounds);
			break;
		case FZ_STEXT_BLOCK_VECTOR:
			break;
		case FZ_STEXT_BLOCK_TEXT:
		{
			fz_rect justified_region = fz_empty_rect;
			for (line = block->u.t.first_line; line != NULL; line = line->next)
			{
				fz_rect region = fz_empty_rect;

				region.y0 = line->bbox.y0;
				region.y1 = line->bbox.y1;

				if (region.y0 < bounds.y0)
					region.y0 = bounds.y0;
				if (region.y1 > bounds.y1)
					region.y1 = bounds.y1;
				if (region.y0 >= region.y1)
					continue;

				/* Skip leading spaces. */
				for (ch = line->first_char; ch != NULL; ch = ch->next)
					if (ch->c != ' ')
						break;

				for (; ch != NULL; ch = ch->next)
				{
					if (ch->c == ' ')
					{
						/* Find the last space char in this run. */
						fz_stext_char *last_space;

						for (last_space = ch; last_space->next != NULL && last_space->next->c == ' '; last_space = last_space->next);

						/* If we're not the last char in the line (i.e. we're not a trailing space,
						 * then send a 'weak' gap for the spaces, assuming it's sane to do so). */
						if (last_space->next != NULL)
						{
							float rpos = fz_min(ch->quad.ll.x, ch->quad.ul.x);
							float lpos = fz_min(last_space->next->quad.ll.x, last_space->next->quad.ll.x);

							/* Clamp these to the bounds */
							rpos = fz_clamp(rpos, bounds.x0, bounds.x1);
							lpos = fz_clamp(lpos, bounds.x0, bounds.x1);

							/* So we have a region (rpos...lpos) to add. */
							/* This can be positioned in various different ways relative to the
							 * current region:
							 *                [region]
							 *   (rpos..lpos)                            OK
							 *        (rpos..lpos)                       OK, but adjust lpos
							 *                     (rpos..lpos)          OK, but adjust rpos
							 *                         (rpos..lpos)      OK
							 *            (rpos  ..  lpos)               OK
							 */
							if (lpos >= region.x1)
							{
								if (rpos >= region.x0 && rpos < region.x1)
									rpos = region.x1;
							}
							else if (rpos <= region.x0)
							{
								if (lpos > region.x0)
									lpos = region.x0;
							}
							else
								rpos = lpos; /* Make it an invalid region */

							if (rpos < lpos)
							{
								/* Send a weak right at the start of the spaces... */
								div_list_push(ctx, xs, 0, 1, rpos);
								/* and a weak left at the end. */
								div_list_push(ctx, xs, 1, 1, lpos);
							}

							/* Expand the region as required. */
							if (rpos < region.x0)
								region.x0 = rpos;
							if (lpos > region.x1)
								region.x1 = lpos;
						}
						/* Jump over the spaces */
						ch = last_space;
					}
					else
					{
						float lpos = fz_min(ch->quad.ll.x, ch->quad.ul.x);
						float rpos = fz_max(ch->quad.lr.x, ch->quad.ur.x);
						if (lpos < region.x0)
							region.x0 = lpos;
						if (rpos > region.x1)
							region.x1 = rpos;
					}
				}
				if (!fz_is_empty_rect(region))
				{
					div_list_push(ctx, xs, 1, 0, region.x0);
					div_list_push(ctx, xs, 0, 0, region.x1);
					/* For justified regions, we don't break after each line, but
					 * rather before/after the region as a whole. */
					if (block->u.t.flags != FZ_STEXT_TEXT_JUSTIFY_FULL)
					{
						div_list_push(ctx, ys, 1, 0, region.y0);
						div_list_push(ctx, ys, 0, 0, region.y1);
					}
					else
						justified_region = fz_union_rect(justified_region, region);
				}
			}
			if (!fz_is_empty_rect(justified_region) && block->u.t.flags == FZ_STEXT_TEXT_JUSTIFY_FULL)
			{
				div_list_push(ctx, ys, 1, 0, justified_region.y0);
				div_list_push(ctx, ys, 0, 0, justified_region.y1);
			}
			break;
		}
		}
	}
}

/* One of our datastructures (cells_t) is an array of details about the
 * cells that make up our table. It's a w * h array of cell_t's. Each
 * cell contains data on one of the cells in the table, as you'd expect.
 *
 *     .       .
 *     .       .
 * - - +-------+ - -
 *     |       .
 *     |       .
 *     |       .
 * - - + - - - + - -
 *     .       .
 *     .       .
 *
 * For any given cell, we store details about the top (lowest y coord)
 * and left (lowest x coord) edges. Specifically we store whether
 * there is a line at this position (h_line and v_line) (i.e. a drawn
 * border), and we also store whether content crosses this edge (h_crossed
 * and y_crossed). Finally, we store whether the cell has any content
 * in it at all (full).
 *
 * A table which has w positions across and h positions vertically, will
 * only really have (w-1) * (h-1) cells. We store w*h though to allow for
 * the right and bottom edges to have their lines represented.
 */

typedef struct
{
	int h_line;
	int v_line;
	int h_crossed;
	int v_crossed;
	int full;
} cell_t;

typedef struct
{
	int w;
	int h;
	cell_t cell[FZ_FLEXIBLE_ARRAY];
} cells_t;

typedef struct
{
	cells_t *cells;
	fz_stext_grid_positions *xpos;
	fz_stext_grid_positions *ypos;
	fz_rect bounds;
	int has_background;
} grid_walker_data;

static fz_stext_grid_info *
copy_grid_info_to_pool(fz_context *ctx, fz_stext_page *page, cells_t *cells)
{
	int i, n = cells->w * cells->h;
	fz_stext_grid_info *info;
	size_t z = offsetof(fz_stext_grid_info, info) + sizeof(info->info[0]) * n;
	cell_t *cell = &cells->cell[0];
	info = fz_pool_alloc(ctx, page->pool, z);

	info->w = cells->w;
	info->h = cells->h;

	for (i = 0; i < n; i++, cell++)
	{
		unsigned int flags = 0;
		if (cell->full)
			flags |= FZ_STEXT_GRID_FULL;
		if (cell->h_crossed)
			flags |= FZ_STEXT_GRID_H_CROSSED;
		if (cell->v_crossed)
			flags |= FZ_STEXT_GRID_V_CROSSED;
		if (cell->h_line)
			flags |= FZ_STEXT_GRID_T_BORDER;
		if (cell->v_line)
			flags |= FZ_STEXT_GRID_L_BORDER;
		info->info[i].flags = flags;
	}

	return info;
}

static cell_t *
get_cell(cells_t *cells, int x, int y)
{
	return &cells->cell[x + y * cells->w];
}

#ifdef DEBUG_TABLE_STRUCTURE
static void
asciiart_table(grid_walker_data *gd);
#endif

static fz_stext_grid_positions *
split_grid_pos(fz_context *ctx, grid_walker_data *gd, int row, int i, int early)
{
	fz_stext_grid_positions **posp = row ? &gd->ypos : &gd->xpos;
	fz_stext_grid_positions *pos = *posp;
	int n = pos->len;
	int x, y, w, h;
	cells_t *cells;

	/* Realloc the required structs */
	*posp = pos = fz_realloc_flexible(ctx, pos, fz_stext_grid_positions, list, n+1);
	cells = gd->cells = fz_realloc_flexible(ctx, gd->cells, cells_t, cell, (gd->cells->w + (1-row)) * (gd->cells->h + row));
	/* If both pass, then we're safe to shuffle the data. */

#ifdef DEBUG_TABLE_STRUCTURE
	printf("Before split %s %d\n", row ? "row" : "col", i);
	asciiart_table(gd);
#endif

	assert(i >= 0 && i < n);
	memmove(&pos->list[i+1], &pos->list[i], sizeof(pos->list[0]) * (n-i));
	pos->len++;

	/* Next expand the cells. Only h_line and v_line are filled in so far. */
	w = cells->w;
	h = cells->h;
	if (early && i > 0)
		i--, early = 0;
	if (row)
	{
		/* Add a row */
		cells->h = h+1;
		/* Expand the table, duplicating row i */
		memmove(&cells->cell[(i+1)*w], &cells->cell[i*w], sizeof(cells->cell[0])*(h-i)*w);

		if (early)
		{
			/* We are splitting row 0 into 0 and 1, with 0 being the new one. */
			for (x = 0; x < w; x++)
			{
				cells->cell[x].h_line = 0;
				cells->cell[x].v_line = 0;
			}
		}
		else
		{
			/* We are splitting row i into i and i+1, with i+1 being the new one. */
			/* v_lines are carried over. h_lines need to be unset. */
			for (x = 0; x < w; x++)
				cells->cell[x + (i+1)*w].h_line = 0;
		}
	}
	else
	{
		/* Add a column */
		cells->w = w+1;
		/* Expand the table, duplicating column i */
		for (y = h-1; y >= 0; y--)
		{
			for (x = w; x > i; x--)
				cells->cell[x + y*(w+1)] = cells->cell[x-1 + y*w];
			for (; x >= 0; x--)
				cells->cell[x + y*(w+1)] = cells->cell[x + y*w];
		}
		if (early)
		{
			/* We are splitting col 0 into 0 and 1, with 0 being the new one. */
			for (y = 0; y < h; y++)
			{
				cells->cell[y*(w+1)].h_line = 0;
				cells->cell[y*(w+1)].v_line = 0;
			}
		}
		else
		{
			/* h_lines are carried over. v_lines need to be reset */
			for (y = 0; y < h; y++)
				cells->cell[i+1 + y*(w+1)].v_line = 0;
		}
	}

#ifdef DEBUG_TABLE_STRUCTURE
	printf("After split\n");
	asciiart_table(gd);
#endif
	return pos;
}

/*	This routine finds (and reinforces) grid positions for lines.
 *
 *	If we have a thin line from (x0, y0) to (x1, y0), then we are
 *	pretty sure that y0 will be on the edge of a cell. We are less
 *	sure that x0 and x1 will match up to the edge of a cell.
 *	Stylistically some tables overrun or underrun such lines.
 *
 *	Similarly from (x0, y0) to (x0, y1), we expect x0 to be accurate
 *	but y0 and y1 less so.
 *
 *	If we have a wider rectangle, from (x0, y0) to (x1, y1) then
 *	we fully expect all sides to be accurate.
 */
static int
find_grid_pos(fz_context *ctx, grid_walker_data *gd, int row, float x, int inaccurate)
{
	const int WIGGLE_ROOM = 2;
	int i;
	fz_stext_grid_positions *pos = row ? gd->ypos : gd->xpos;

	assert(x >= pos->list[0].min && x <= pos->list[pos->len-1].max);

#ifdef DEBUG_TABLE_STRUCTURE
	printf("Looking for %g in %s splits:\n", x, row ? "row" : "col");
	for (i = 0; i < pos->len; i++)
	{
		printf("%d\t%g\t%g\t%g\t%d\n", i, pos->list[i].min, pos->list[i].pos, pos->list[i].max, pos->list[i].reinforcement);
	}
#endif

	while (inaccurate) /* So we can break out */
	{
		float prev = 0;

		/* If we start/finish outside the range of the table, then we
		 * want to extend the table. So ignore 'inaccurate' in this
		 * case. Match the logic below. */
		if (x < pos->list[0].min)
			break;
		if (x < pos->list[0].pos - WIGGLE_ROOM && pos->list[0].reinforcement > 0)
			break;
		if (x > pos->list[pos->len-1].max)
			break;
		if (x > pos->list[pos->len-1].pos + WIGGLE_ROOM && pos->list[pos->len-1].reinforcement > 0)
			break;

		/* Just find the closest one. No reinforcement. */
		for (i = 0; i < pos->len; i++)
		{
			if (x < pos->list[i].min)
			{
				float mid = (prev + pos->list[i].min)/2;
				if (x < mid)
					return i-1;
				return i;
			}
			prev = pos->list[i].max;
			if (x <= prev)
				return i;
		}
		assert("Never happens" == NULL);

		return -1;
	}

	for (i = 0; i < pos->len; i++)
	{
		if (x < pos->list[i].min)
		{
			/* Split i into i and i+1, and make i the new one. */
			assert(i > 0);
#ifdef DEBUG_TABLE_STRUCTURE
			printf("Splitting before %d\n", i);
#endif
			pos = split_grid_pos(ctx, gd, row, i, 1);
			pos->list[i-1].max = pos->list[i].min = (pos->list[i-1].max + x)/2;
			pos->list[i].pos = x;
			pos->list[i].max = pos->list[i+1].min = (pos->list[i+1].pos + x)/2;
			pos->list[i].reinforcement = 1;
			return i;
		}
		else if (x <= pos->list[i].max)
		{
			/* We are in the range for the ith divider. */
			if (i == 0 || i == pos->len-1)
			{
				/* Never move the outermost pos in, because they have been
				 * calculated to be just big enough already. */
				return i;
			}
			if (pos->list[i].reinforcement == 0)
			{
				/* If we've not been reinforced before, reinforce now. */
				pos->list[i].pos = x;
				pos->list[i].reinforcement = 1;
				return i;
			}
			/* We've been reinforced before. This ought to be a pretty good
			 * indication. */
			if (pos->list[i].pos - WIGGLE_ROOM < x && x < pos->list[i].pos + WIGGLE_ROOM)
			{
				/* We are a close match to the previously predicted pos
				 * value. */
				pos->list[i].pos = pos->list[i].pos * pos->list[i].reinforcement + x;
				pos->list[i].pos /= ++pos->list[i].reinforcement;
				return i;
			}
			/* We need to split i into i and i+1. */
			pos = split_grid_pos(ctx, gd, row, i, pos->list[i].pos > x);
			if (pos->list[i].pos > x)
			{
				/* Make i the new one */
#ifdef DEBUG_TABLE_STRUCTURE
				printf("Splitting %d (early)\n", i);
#endif
				pos->list[i].pos = x;
				pos->list[i].max = pos->list[i+1].min = (pos->list[i+1].pos + x)/2;
				pos->list[i].reinforcement = 1;
				return i;
			}
			else
			{
				/* Make i+1 the new one */
#ifdef DEBUG_TABLE_STRUCTURE
				printf("Splitting %d (late)\n", i);
#endif
				pos->list[i+1].pos = x;
				pos->list[i].max = pos->list[i+1].min = (pos->list[i].pos + x)/2;
				pos->list[i].reinforcement = 1;
				return i+1;
			}
		}
	}
	assert("Never happens" == NULL);

	return -1;
}

static int
find_cell_l(fz_stext_grid_positions *pos, float x)
{
	int i;

	for (i = 0; i < pos->len; i++)
		if (x < pos->list[i].pos)
			return i-1;

	return -1;
}

static int
find_cell_r(fz_stext_grid_positions *pos, float x)
{
	int i;

	for (i = 0; i < pos->len; i++)
		if (x <= pos->list[i].pos)
			return i-1;

	return -1;
}

/* Add a horizontal line. Return 1 if the line doesn't seem to be a border line.
 * Record which cells that was a border for. */
static void
add_h_line(fz_context *ctx, grid_walker_data *gd, float x0, float x1, float y0, float y1)
{
	int start = find_grid_pos(ctx, gd, 0, x0, 1);
	int end = find_grid_pos(ctx, gd, 0, x1, 1);
	float y = (y0 + y1) / 2;
	int yidx = find_grid_pos(ctx, gd, 1, y, 0);
	int i;

	for (i = start; i < end; i++)
		get_cell(gd->cells, i, yidx)->h_line++;
}

/* Add a vertical line. Return 1 if the line doesn't seem to be a border line.
 * Record which cells that was a border for. */
static void
add_v_line(fz_context *ctx, grid_walker_data *gd, float y0, float y1, float x0, float x1)
{
	int start = find_grid_pos(ctx, gd, 1, y0, 1);
	int end = find_grid_pos(ctx, gd, 1, y1, 1);
	float x = (x0 + x1) / 2;
	int xidx = find_grid_pos(ctx, gd, 0, x, 0);
	int i;

	for (i = start; i < end; i++)
		get_cell(gd->cells, xidx, i)->v_line++;
}

static void
add_hv_line(fz_context *ctx, grid_walker_data *gd, float x0, float x1, float y0, float y1, int stroked)
{
	int ix0 = find_grid_pos(ctx, gd, 0, x0, 0);
	int ix1 = find_grid_pos(ctx, gd, 0, x1, 0);
	int iy0 = find_grid_pos(ctx, gd, 1, y0, 0);
	int iy1 = find_grid_pos(ctx, gd, 1, y1, 0);
	int i;

	if (stroked)
	{
		for (i = ix0; i < ix1; i++)
		{
			get_cell(gd->cells, i, iy0)->h_line++;
			get_cell(gd->cells, i, iy1)->h_line++;
		}
		for (i = iy0; i < iy1; i++)
		{
			get_cell(gd->cells, ix0, i)->v_line++;
			get_cell(gd->cells, ix1, i)->v_line++;
		}
	}
}

/* Many PDFs with tables in them start by clearing the area of the table to white
 * by one big rectangle. Let's try and spot that. */
enum
{
	KEEP_SEARCHING = 0,
	BACKGROUND_FOUND = 1,
	NON_BACKGROUND_FOUND = 2
};

static fz_rect
whittle_background(fz_context *ctx, fz_stext_block *block0, fz_rect obounds, fz_rect bounds)
{
	fz_stext_block *block;

	for (block = block0; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (!block->u.s.down)
				continue;
			obounds = whittle_background(ctx, block->u.s.down->first_block, obounds, bounds);
			continue;
		}
		else if (block->type == FZ_STEXT_BLOCK_VECTOR ||
			block->type == FZ_STEXT_BLOCK_TEXT)
		{
			fz_rect r = block->bbox;

			if (r.y1 >= bounds.y0 && r.y0 <= bounds.y1)
			{
				if (r.x1 <= bounds.x0)
					obounds.x0 = fz_max(obounds.x0, r.x1);
				if (r.x0 >= bounds.x1)
					obounds.x1 = fz_min(obounds.x1, r.x0);
			}
			if (r.x1 >= bounds.x0 && r.x0 <= bounds.x1)
			{
				if (r.y1 <= bounds.y0)
					obounds.y0 = fz_max(obounds.y0, r.y1);
				if (r.y0 >= bounds.y1)
					obounds.y1 = fz_min(obounds.y1, r.y0);
			}
		}
	}

	return obounds;
}

/* Can we find a fill that covers bounds, but not obounds, before any content impinges on bounds? */
static int
background_hunt(fz_context *ctx, grid_walker_data *gd, fz_stext_block *block0, fz_rect bounds, fz_rect obounds)
{
	int found;
	fz_stext_block *block;

	for (block = block0; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (!block->u.s.down)
				continue;
			found = background_hunt(ctx, gd, block->u.s.down->first_block, bounds, obounds);
			if (found)
				return found;
			continue;
		}
		else if (block->type == FZ_STEXT_BLOCK_VECTOR)
		{
			fz_rect r = block->bbox;
			fz_rect s;

			r = fz_intersect_rect(r, gd->bounds);
			s = fz_intersect_rect(r, bounds);

			if (!fz_is_valid_rect(s))
				continue;

			/* Only rectangular && non-stroked blocks can possibly be background
			 * fills. */
			if ((block->u.v.flags & FZ_STEXT_VECTOR_IS_RECTANGLE) == 0 ||
				(block->u.v.flags & FZ_STEXT_VECTOR_IS_STROKED) != 0)
			{
				if (r.x0 < obounds.x0 || r.x1 > obounds.x1 || r.y0 < obounds.y0 || r.y1 > obounds.y1)
				{
					/* If this extends outside the outer bounds of the table, then maybe it's
					 * a background fill? Just continue. */
				}
				/* Otherwise, this might be content. Stop searching for the background fill. */
				return NON_BACKGROUND_FOUND;
			}

			/* Rectangle */
			if (r.x0 <= bounds.x0 && r.x1 >= bounds.x1 && r.y0 <= bounds.y0 && r.y1 >= bounds.y1)
			{
				/* This rectangle covers the entire table. */
				if (r.x0 < obounds.x0 || r.x1 > obounds.x1 || r.y0 < obounds.y0 || r.y1 > obounds.y1)
				{
					/* But it's too big for the table! Maybe it's a page fill? Just ignore it. */
					continue;
				}
				/* So let's extend the table. */
				gd->xpos->list[0].min = gd->xpos->list[0].pos = gd->xpos->list[0].max = r.x0;
				gd->xpos->list[gd->xpos->len-1].min = gd->xpos->list[gd->xpos->len-1].pos = gd->xpos->list[gd->xpos->len-1].max = r.x1;
				gd->ypos->list[0].min = gd->ypos->list[0].pos = gd->ypos->list[0].max = r.y0;
				gd->ypos->list[gd->ypos->len-1].min = gd->ypos->list[gd->ypos->len-1].pos = gd->ypos->list[gd->ypos->len-1].max = r.y1;

				/* And stop searching */
				return BACKGROUND_FOUND;
			}
			else
			{
				/* No background for the table found. */
				return NON_BACKGROUND_FOUND;
			}
		}
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			fz_rect r = block->bbox;
			fz_rect s;

			s = fz_intersect_rect(r, gd->bounds);
			s = fz_intersect_rect(s, bounds);

			if (!fz_is_valid_rect(s))
				continue;

			return NON_BACKGROUND_FOUND;
		}
	}

	return KEEP_SEARCHING;
}

static int
walk_for_background(fz_context *ctx, grid_walker_data *gd, fz_stext_block *block0)
{
	fz_rect bounds, obounds;

	/* So the smallest covering for our table needs to be this big. */
	bounds.x0 = gd->xpos->list[0].min;
	bounds.x1 = gd->xpos->list[gd->xpos->len-1].max;
	bounds.y0 = gd->ypos->list[0].min;
	bounds.y1 = gd->ypos->list[gd->ypos->len-1].max;

	obounds = fz_infinite_rect;

	/* What is the biggest margin around this table?
	 * Start with an infinite box, and whittle it down by any content that
	 * isn't covered by the bounds. */
	obounds = whittle_background(ctx, block0, obounds, bounds);

	return background_hunt(ctx, gd, block0, bounds, obounds);
}

/* Shared internal routine with stext-boxer.c  */
fz_rect fz_collate_small_vector_run(fz_stext_block **blockp);

static void
walk_grid_lines(fz_context *ctx, grid_walker_data *gd, fz_stext_block *block)
{
	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down)
				walk_grid_lines(ctx, gd, block->u.s.down->first_block);
			continue;
		}
		else if (block->type == FZ_STEXT_BLOCK_VECTOR)
		{
			fz_rect r;
			float w, h;

			/* Only process rectangle blocks. */
			if ((block->u.v.flags & FZ_STEXT_VECTOR_IS_RECTANGLE) == 0)
				continue;

			r = fz_collate_small_vector_run(&block);
			r = fz_intersect_rect(r, gd->bounds);
			if (!fz_is_valid_rect(r))
				continue;

			w = r.x1 - r.x0;
			h = r.y1 - r.y0;
			if (w > h && h < 2)
			{
				/* Thin, wide line */
				(void) add_h_line(ctx, gd, r.x0, r.x1, r.y0, r.y1);
			}
			else if (w < h && w < 2)
			{
				/* Thin, wide line */
				(void) add_v_line(ctx, gd, r.y0, r.y1, r.x0, r.x1);
			}
			else
			{
				/* Rectangle */
				(void) add_hv_line(ctx, gd, r.x0, r.x1, r.y0, r.y1, block->u.v.flags & FZ_STEXT_VECTOR_IS_STROKED);
			}
		}
	}
}

static int
is_numeric(int c)
{
	return (c >= '0' && c <= '9');
}

static int
mark_cells_for_content(fz_context *ctx, grid_walker_data *gd, fz_rect s)
{
	fz_rect r = fz_intersect_rect(gd->bounds, s);
	int x0, x1, y0, y1, x, y;

	/* Check for non-validity rather than empty here, as e.g.
	 * spaces are empty, and we'd still like to account for
	 * their horizontal extent. */
	if (!fz_is_valid_rect(r))
		return 0;

	x0 = find_cell_l(gd->xpos, r.x0);
	x1 = find_cell_r(gd->xpos, r.x1);
	y0 = find_cell_l(gd->ypos, r.y0);
	y1 = find_cell_r(gd->ypos, r.y1);

	if (x0 < 0 || x1 < 0 || y0 < 0 || y1 < 0)
		return 1;
	if (x0 < x1)
	{
		for (y = y0; y <= y1; y++)
			for (x = x0; x < x1; x++)
				get_cell(gd->cells, x+1, y)->v_crossed++;
	}
	if (y0 < y1)
	{
		for (y = y0; y < y1; y++)
			for (x = x0; x <= x1; x++)
				get_cell(gd->cells, x, y+1)->h_crossed++;
	}
	for (y = y0; y <= y1; y++)
		for (x = x0; x <= x1; x++)
			get_cell(gd->cells, x, y)->full++;

	return 0;
}

#define IN_CELL 0
#define IN_BORDER 1
#define IN_UNKNOWN 2

#define SPLIT_MARGIN 4
static int
where_is(fz_stext_grid_positions *pos, float x, int *in)
{
	int i;

	*in = IN_UNKNOWN;

	/* If the table is empty, nothing to do. */
	if (pos->len == 0)
		return -1;

	/* If we are completely outside the table, give up. */
	if (x <= pos->list[0].pos - SPLIT_MARGIN || x >= pos->list[pos->len-1].max + SPLIT_MARGIN)
		return -1;

	for (i = 0; i < pos->len; i++)
	{
		/* Calculate the region (below..above) that counts as being
		 * on the border of position i. */
		float prev = i > 0 ? pos->list[i-1].max : pos->list[0].min;
		float next = i < pos->len-1 ? pos->list[i+1].min : pos->list[i].max;
		float below = pos->list[i].pos - SPLIT_MARGIN;
		float above = pos->list[i].pos + SPLIT_MARGIN;
		/* Find the distance half way back to the previous pos as
		 * a limit to our margin. */
		prev = (prev + pos->list[i].pos)/2;
		next = (next + pos->list[i].pos)/2;
		if (below < prev)
			below = prev;
		if (above > next)
			above = next;

		if (x < below)
		{
			*in = IN_CELL;
			return i-1;
		}
		else if (x <= above)
		{
			*in = IN_BORDER;
			return i;
		}
	}

	*in = IN_BORDER;
	return i-1;
}

enum
{
	VECTOR_IS_CONTENT = 0,
	VECTOR_IS_BORDER = 1,
	VECTOR_IS_UNKNOWN = 2,
	VECTOR_IS_IGNORABLE = 3
};

/* So a vector can either be a border, or contained
 * in some cells, or something completely else. */
static int
classify_vector(fz_context *ctx, grid_walker_data *gd, fz_rect r, int is_rect)
{
	int at_x0, at_x1, at_y0, at_y1;
	int ix0, ix1, iy0, iy1;

	r = fz_intersect_rect(r, gd->bounds);
	if (fz_is_empty_rect(r))
		return VECTOR_IS_IGNORABLE;
	ix0 = where_is(gd->xpos, r.x0, &at_x0);
	ix1 = where_is(gd->xpos, r.x1, &at_x1);
	iy0 = where_is(gd->ypos, r.y0, &at_y0);
	iy1 = where_is(gd->ypos, r.y1, &at_y1);

	/* No idea, just treat it as a border. */
	if (at_x0 == IN_UNKNOWN || at_x1 == IN_UNKNOWN || at_y0 == IN_UNKNOWN || at_y1 == IN_UNKNOWN)
		return VECTOR_IS_IGNORABLE;

	if (at_x0 == IN_BORDER && at_x1 == IN_BORDER)
	{
		/* Vector is aligned along sides of cells. */
		return is_rect ? VECTOR_IS_BORDER : VECTOR_IS_CONTENT;
	}
	if (at_y0 == IN_BORDER && at_y1 == IN_BORDER)
	{
		/* Vector is aligned along sides of cells. */
		return is_rect ? VECTOR_IS_BORDER : VECTOR_IS_CONTENT;
	}
	if (at_x0 == IN_CELL && at_x1 == IN_CELL)
	{
		/* Content within a cell (or 1d range of cells). */
		return VECTOR_IS_CONTENT;
	}
	if (at_y0 == IN_CELL && at_y1 == IN_CELL)
	{
		/* Content within a cell (or 1d range of cells). */
		return VECTOR_IS_CONTENT;
	}
	if (at_x0 == IN_BORDER && at_x1 == IN_CELL && ix0 == ix1)
	{
		return is_rect ? VECTOR_IS_BORDER : VECTOR_IS_CONTENT;
	}
	if (at_x0 == IN_CELL && at_x1 == IN_BORDER && ix0+1 == ix1)
	{
		return is_rect ? VECTOR_IS_BORDER : VECTOR_IS_CONTENT;
	}
	if (at_y0 == IN_BORDER && at_y1 == IN_CELL && iy0 == iy1)
	{
		return is_rect ? VECTOR_IS_BORDER : VECTOR_IS_CONTENT;
	}
	if (at_y0 == IN_CELL && at_y1 == IN_BORDER && iy0+1 == iy1)
	{
		return is_rect ? VECTOR_IS_BORDER : VECTOR_IS_CONTENT;
	}

	if (is_rect)
	{
		return VECTOR_IS_IGNORABLE;
	}

	/* unknown - take this as indication that this maybe isn't
	 * table. */
	return VECTOR_IS_UNKNOWN;
}

#undef IN_CELL
#undef IN_BORDER
#undef IN_UNKNOWN


/* Walk through the content, looking at how it spans our grid.
 * Record gridlines, which cells have content that cross into
 * neighbours, and which cells have any content at all.
 * Return a count of vector graphics that are found that don't
 * look plausibly like cell contents. */
static int
calculate_spanned_content(fz_context *ctx, grid_walker_data *gd, fz_stext_block *block)
{
	int duff = 0;
	fz_rect bounds = {
		gd->xpos->list[0].pos,
		gd->ypos->list[0].pos,
		gd->xpos->list[gd->xpos->len-1].pos,
		gd->ypos->list[gd->ypos->len-1].pos };

	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down)
				duff += calculate_spanned_content(ctx, gd, block->u.s.down->first_block);
			continue;
		}
		else if (block->type == FZ_STEXT_BLOCK_VECTOR)
		{
			switch (classify_vector(ctx, gd, block->bbox, !!(block->u.v.flags & FZ_STEXT_VECTOR_IS_RECTANGLE)))
			{
			case VECTOR_IS_CONTENT:
				mark_cells_for_content(ctx, gd, block->bbox);
				break;
			case VECTOR_IS_BORDER:
			case VECTOR_IS_IGNORABLE:
				break;
			default:
				duff++;
				break;
			}
		}
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			fz_stext_line *line;

			if (block->bbox.x0 >= bounds.x1 || block->bbox.y0 >= bounds.y1 ||
				block->bbox.x1 <= bounds.x0 || block->bbox.y1 <= bounds.y0)
				continue;

			for (line = block->u.t.first_line; line != NULL; line = line->next)
			{
				fz_stext_char *ch = line->first_char;
				int was_numeric = 0;

				/* Skip leading spaces */
				while (ch != NULL && ch->c == ' ')
					ch = ch->next;

				for (; ch != NULL; ch = ch->next)
				{
					if (ch->c == 32)
					{
						/* Trailing space, skip it. */
						if (ch->next == NULL)
							break;
						if (ch->next->c == 32)
						{
							/* Run of spaces. Skip 'em. */
							while (ch->next && ch->next->c == 32)
								ch = ch->next;
							was_numeric = 0;
							continue;
						}
						if (was_numeric || is_numeric(ch->next->c))
						{
							/* Single spaces around numbers are ignored. */
							was_numeric = 0;
							continue;
						}
						if (ch->flags & FZ_STEXT_SYNTHETIC_LARGE)
						{
							/* Break on large synthetic spaces */
							continue;
						}
						/* A single space. Accept it. */
						was_numeric = 0;
					}
					else
						was_numeric = is_numeric(ch->c);
					duff += mark_cells_for_content(ctx, gd, fz_rect_from_quad(ch->quad));
				}
			}
		}
	}

	return duff;
}

static cells_t *new_cells(fz_context *ctx, int w, int h)
{
	cells_t *cells = fz_malloc_flexible(ctx, cells_t, cell, w * h);
	cells->w = w;
	cells->h = h;

	return cells;
}

#ifdef DEBUG_TABLE_STRUCTURE
static void
asciiart_table(grid_walker_data *gd)
{
	int w = gd->xpos->len;
	int h = gd->ypos->len;
	int x, y;

	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w-1; x++)
		{
			cell_t *cell = get_cell(gd->cells, x, y);
			int line = cell->h_line;
			int erase = cell->h_crossed;
			printf("+");
			if (line && !erase)
			{
				printf("-");
			}
			else if (!line && erase)
			{
				printf("v");
			}
			else if (line && erase)
			{
				printf("*");
			}
			else
			{
				printf(" ");
			}
		}
		printf("+\n");
		if (y == h-1)
			break;
		for (x = 0; x < w; x++)
		{
			cell_t *cell = get_cell(gd->cells, x, y);
			int line = cell->v_line;
			int erase = cell->v_crossed;
			if (line && !erase)
			{
				printf("|");
			}
			else if (!line && erase)
			{
				printf(">");
			}
			else if (line && erase)
			{
				printf("*");
			}
			else
			{
				printf(" ");
			}
			if (x < w-1)
			{
				if (cell->full)
					printf("#");
				else
					printf(" ");
			}
			else
				printf("\n");
		}
	}
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

static void
unlink_line_from_block(fz_stext_line *line, fz_stext_block *block)
{
	fz_stext_line *next_line = line->next;

	if (line->prev)
	{
		assert(line->prev->next == line);
		line->prev->next = next_line;
	}
	else
	{
		assert(block->u.t.first_line == line);
		block->u.t.first_line = next_line;
	}
	if (next_line)
	{
		assert(next_line->prev == line);
		next_line->prev = line->prev;
	}
	else
	{
		assert(block->u.t.last_line == line);
		block->u.t.last_line = line->prev;
	}
}

static void
append_line_to_block(fz_stext_line *line, fz_stext_block *block)
{
	if (block->u.t.last_line == NULL)
	{
		assert(block->u.t.first_line == NULL);
		block->u.t.first_line = block->u.t.last_line = line;
		line->prev = NULL;
	}
	else
	{
		assert(block->u.t.last_line->next == NULL);
		line->prev = block->u.t.last_line;
		block->u.t.last_line->next = line;
		block->u.t.last_line = line;
	}
	line->next = NULL;
}

static void
unlink_block(fz_stext_block *block, fz_stext_block **first, fz_stext_block **last)
{
	if (block->prev)
	{
		assert(block->prev->next == block);
		block->prev->next = block->next;
	}
	else
	{
		assert(*first == block);
		*first = block->next;
	}
	if (block->next)
	{
		assert(block->next->prev == block);
		block->next->prev = block->prev;
	}
	else
	{
		assert(*last == block);
		*last = block->prev;
	}
}

#ifndef NDEBUG
static int
verify_stext(fz_context *ctx, fz_stext_page *page, fz_stext_struct *src)
{
	fz_stext_block *block;
	fz_stext_block **first = src ? &src->first_block : &page->first_block;
	fz_stext_block **last = src ? &src->last_block : &page->last_block;
	int max = 0;

	assert((*first == NULL) == (*last == NULL));

	for (block = *first; block != NULL; block = block->next)
	{
		fz_stext_line *line;

		if (block->prev == NULL)
			assert(*first == block);
		else
			assert(block->prev->next == block);
		if (block->next == NULL)
			assert(*last == block);
		else
			assert(block->next->prev == block);

		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down)
			{
				int m = verify_stext(ctx, page, block->u.s.down);
				if (m > max)
					max = m;
			}
			continue;
		}
		if (block->type != FZ_STEXT_BLOCK_TEXT)
			continue;
		assert((block->u.t.first_line == NULL) == (block->u.t.last_line == NULL));
		for (line = block->u.t.first_line; line != NULL; line = line->next)
		{
			fz_stext_char *ch;

			if (line->next == NULL)
				assert(block->u.t.last_line == line);
			else
				assert(line->next->prev == line);

			assert((line->first_char == NULL) == (line->last_char == NULL));

			for (ch = line->first_char; ch != NULL; ch = ch->next)
				assert(ch->next != NULL || line->last_char == ch);
		}
	}

	return max+1;
}
#endif

static fz_rect
move_contained_content(fz_context *ctx, fz_stext_page *page, fz_stext_struct *dest, fz_stext_struct *src, fz_rect r)
{
	fz_stext_block *before = dest ? dest->first_block : page->first_block;
	fz_stext_block **sfirst = src ? &src->first_block : &page->first_block;
	fz_stext_block **slast = src ? &src->last_block : &page->last_block;
	fz_stext_block *block, *next_block;

	for (block = *sfirst; block != NULL; block = next_block)
	{
		fz_rect bbox = fz_intersect_rect(block->bbox, r);
		next_block = block->next;
		/* Don't use fz_is_empty_rect here, as that will exclude zero height areas like spaces. */
		if (bbox.x0 > bbox.x1 || bbox.y0 > bbox.y1)
			continue; /* Trivially excluded */
		if (bbox.x0 == block->bbox.x0 && bbox.y0 == block->bbox.y0 && bbox.x1 == block->bbox.x1 && bbox.y1 == block->bbox.y1)
		{
			/* Trivially included */
			if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down)
			{
				if (block->u.s.down->standard == FZ_STRUCTURE_TD)
				{
					/* Don't copy TDs! Just copy the contents */
					move_contained_content(ctx, page, dest, block->u.s.down, r);
					continue;
				}
			}
			unlink_block(block, sfirst, slast);
			insert_block_before(block, before, page, dest);
			assert(before == block->next);
			continue;
		}
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down)
				move_contained_content(ctx, page, dest, block->u.s.down, r);
		}
		if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			/* Partially included text block */
			fz_stext_line *line, *next_line;
			fz_stext_block *newblock = NULL;

			for (line = block->u.t.first_line; line != NULL; line = next_line)
			{
				fz_rect lrect = fz_intersect_rect(line->bbox, r);
				next_line = line->next;

				/* Don't use fz_is_empty_rect here, as that will exclude zero height areas like spaces. */
				if (lrect.x0 > lrect.x1 || lrect.y0 > lrect.y1)
					continue; /* Trivial exclusion */
				if (line->bbox.x0 == lrect.x0 && line->bbox.y0 == lrect.y0 && line->bbox.x1 == lrect.x1 && line->bbox.y1 == lrect.y1)
				{
					/* Trivial inclusion */
					if (newblock == NULL)
					{
						newblock = fz_pool_alloc(ctx, page->pool, sizeof(fz_stext_block));
						insert_block_before(newblock, before, page, dest);
						newblock->type = FZ_STEXT_BLOCK_TEXT;
						newblock->id = 0;
						newblock->u.t.flags = block->u.t.flags;
						assert(before == newblock->next);
					}

					unlink_line_from_block(line, block);
					append_line_to_block(line, newblock);
				}
				else
				{
					/* Need to walk the line and just take parts */
					fz_stext_line *newline = NULL;
					fz_stext_char *ch, *next_ch, *prev_ch = NULL;

					for (ch = line->first_char; ch != NULL; ch = next_ch)
					{
						fz_rect crect = fz_rect_from_quad(ch->quad);
						float x = (crect.x0 + crect.x1)/2;
						float y = (crect.y0 + crect.y1)/2;
						next_ch = ch->next;
						if (r.x0 > x || r.x1 < x || r.y0 > y || r.y1 < y)
						{
							prev_ch = ch;
							continue;
						}
						/* Take this char */
						if (newline == NULL)
						{
							newline = fz_pool_alloc(ctx, page->pool, sizeof(*newline));
							newline->dir = line->dir;
							newline->wmode = line->wmode;
							newline->bbox = fz_empty_rect;
						}
						/* Unlink char */
						if (prev_ch == NULL)
							line->first_char = next_ch;
						else
							prev_ch->next = next_ch;
						if (next_ch == NULL)
							line->last_char = prev_ch;
						/* Relink char */
						ch->next = NULL;
						if (newline->last_char == NULL)
							newline->first_char = ch;
						else
							newline->last_char->next = ch;
						newline->last_char = ch;
						newline->bbox = fz_union_rect(newline->bbox, crect);
					}
					if (line->first_char == NULL)
					{
						/* We've removed all the chars from this line.
						 * Better remove the line too. */
						if (line->prev)
							line->prev->next = next_line;
						else
							block->u.t.first_line = next_line;
						if (next_line)
							next_line->prev = line->prev;
						else
							block->u.t.last_line = line->prev;
					}
					if (newline)
					{
						if (newblock == NULL)
						{
							newblock = fz_pool_alloc(ctx, page->pool, sizeof(fz_stext_block));

							/* Add the block onto our target list */
							insert_block_before(newblock, before, page, dest);
							newblock->type = FZ_STEXT_BLOCK_TEXT;
							newblock->id = 0;
							newblock->u.t.flags = block->u.t.flags;
							before = newblock->next;
						}
						append_line_to_block(newline, newblock);
					}
				}
			}
			if (newblock)
			{
				recalc_bbox(block);
				recalc_bbox(newblock);
			}
			if (block->u.t.first_line == NULL)
			{
				/* We've removed all the lines from the block. Should remove that too! */
				if (block->prev)
				{
					assert(block->prev->next == block);
					block->prev->next = next_block;
				}
				else
				{
					assert(*sfirst == block);
					*sfirst = block->next;
				}
				if (next_block)
				{
					assert(next_block->prev == block);
					next_block->prev = block->prev;
				}
				else
				{
					assert(*slast == block);
					*slast = block->prev;
				}
			}
		}
	}

	return r;
}

typedef struct
{
	fz_stext_block *block;
	fz_stext_struct *parent;
} tree_pos;

/* This is still not perfect, but it's better! */
static tree_pos
find_table_insertion_point(fz_context *ctx, fz_rect r, tree_pos current, tree_pos best)
{
	fz_stext_block *block;

	for (block = current.block; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down != NULL && fz_is_rect_inside_rect(r, block->bbox))
			{
				tree_pos down;

				down.parent = block->u.s.down;
				down.block = block->u.s.down->first_block;
				best = find_table_insertion_point(ctx, r, down, best);
			}
		}
		else
		{
			/* Is block a better precursor than best? (Or a valid precursor, if best.block == NULL) */
			if (block->bbox.y1 < r.y0 && (best.block == NULL || best.block->bbox.y1 < block->bbox.y1))
			{
				best.block = block;
				best.parent = current.parent;
			}
		}
	}

	return best;
}

static int
tr_is_empty(fz_context *ctx, fz_stext_block *block)
{
	for (; block != NULL; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_STRUCT)
			return 0;
		if (!block->u.s.down)
		{
		}
		else if (block->u.s.down->standard != FZ_STRUCTURE_TD)
			return 0;
		else if (block->u.s.down->first_block != NULL)
			return 0;
	}

	return 1;
}

static int
table_is_empty(fz_context *ctx, fz_stext_block *block)
{
	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_GRID)
			continue;
		if (block->type != FZ_STEXT_BLOCK_STRUCT)
			return 0;
		if (!block->u.s.down)
		{
		}
		else if (block->u.s.down->standard != FZ_STRUCTURE_TR)
			return 0;
		else if (!tr_is_empty(ctx, block->u.s.down->first_block))
			return 0;
	}

	return 1;
}

static void
tidy_td_divs(fz_context *ctx, fz_stext_struct *parent)
{
	while (1)
	{
		fz_stext_block *block = parent->first_block;

		if (block == NULL || block->next != NULL || block->type != FZ_STEXT_BLOCK_STRUCT || block->u.s.down == NULL || block->u.s.down->standard != FZ_STRUCTURE_DIV)
			return;

		parent->first_block = block->u.s.down->first_block;
		parent->last_block = block->u.s.down->last_block;
	}
}

static void
tidy_tr_divs(fz_context *ctx, fz_stext_block *block)
{
	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down && block->u.s.down->standard == FZ_STRUCTURE_TD)
			tidy_td_divs(ctx, block->u.s.down);
	}
}

static void
tidy_table_divs(fz_context *ctx, fz_stext_block *block)
{
	for (; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_GRID)
			continue;
		if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down && block->u.s.down->standard == FZ_STRUCTURE_TR)
			tidy_tr_divs(ctx, block->u.s.down->first_block);
	}
}

static int
struct_is_empty(fz_context *ctx, fz_stext_block *block)
{
	for (; block != NULL; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_STRUCT)
			return 0;
		if (!block->u.s.down)
		{
		}
		else if (!struct_is_empty(ctx, block->u.s.down->first_block))
			return 0;
	}

	return 1;
}

static int
div_is_empty(fz_context *ctx, fz_stext_block *block)
{
	for (; block != NULL; block = block->next)
	{
		if (block->type != FZ_STEXT_BLOCK_STRUCT)
			return 0;
		if (!block->u.s.down)
		{
		}
		else if (block->u.s.down->standard == FZ_STRUCTURE_TABLE)
		{
			tidy_table_divs(ctx, block->u.s.down->first_block);
			return table_is_empty(ctx, block->u.s.down->first_block);
		}
		else if (block->u.s.down->standard != FZ_STRUCTURE_DIV)
		{
			if (!struct_is_empty(ctx, block->u.s.down->first_block))
				return 0;
		}
		else if (!div_is_empty(ctx, block->u.s.down->first_block))
			return 0;
	}

	return 1;
}

static void
tidy_orphaned_tables(fz_context *ctx, fz_stext_page *page, fz_stext_struct *parent)
{
	fz_stext_block **first_blockp = parent ? &parent->first_block : &page->first_block;
	fz_stext_block **last_blockp = parent ? &parent->last_block : &page->last_block;
	fz_stext_block *block, *next_block;

	for (block = *first_blockp; block != NULL; block = next_block)
	{
		next_block = block->next;
		if (block->type == FZ_STEXT_BLOCK_STRUCT && block->u.s.down)
		{
			if (block->u.s.down->standard == FZ_STRUCTURE_TABLE)
			{
				tidy_table_divs(ctx, block->u.s.down->first_block);
				if (table_is_empty(ctx, block->u.s.down->first_block))
				{
					/* Remove block */
					if (block->prev)
					{
						assert(block->prev->next == block);
						block->prev->next = next_block;
					}
					else
					{
						assert(*first_blockp == block);
						*first_blockp = next_block;
					}
					if (next_block)
					{
						assert(next_block->prev == block);
						next_block->prev = block->prev;
					}
					else
					{
						assert(*last_blockp == block);
						*last_blockp = block->prev;
					}
				}
				else
				{
					tidy_orphaned_tables(ctx, page, block->u.s.down);
				}
			}
			if (block->u.s.down->standard == FZ_STRUCTURE_DIV)
			{
				tidy_orphaned_tables(ctx, page, block->u.s.down);
				if (div_is_empty(ctx, block->u.s.down->first_block))
				{
					/* Remove block */
					if (block->prev)
					{
						assert(block->prev->next == block);
						block->prev->next = next_block;
					}
					else
					{
						assert(*first_blockp == block);
						*first_blockp = next_block;
					}
					if (next_block)
					{
						assert(next_block->prev == block);
						next_block->prev = block->prev;
					}
					else
					{
						assert(*last_blockp == block);
						*last_blockp = block->prev;
					}
				}
			}
		}
	}
}

static fz_stext_struct *
transcribe_table(fz_context *ctx, grid_walker_data *gd, fz_stext_page *page, fz_stext_struct *parent)
{
	int w = gd->xpos->len;
	int h = gd->ypos->len;
	int x, y;
	char *sent_tab = fz_calloc(ctx, 1, w*(size_t)h);
	fz_stext_block **first_block = parent ? &parent->first_block : &page->first_block;
	fz_stext_struct *table, *tr, *td;
	fz_rect r;
	tree_pos insert, top;

	/* Where should we insert the table in the data? */
	r.x0 = gd->xpos->list[0].pos;
	r.x1 = gd->xpos->list[w-1].pos;
	r.y0 = gd->ypos->list[0].pos;
	r.y1 = gd->ypos->list[h-1].pos;
#ifdef DEBUG_TABLE_STRUCTURE
	fz_print_stext_page_as_xml(ctx, fz_stddbg(ctx), page, 0);
#endif
	top.block = *first_block;
	top.parent = parent;
	insert.block = NULL;
	insert.parent = NULL;
	insert = find_table_insertion_point(ctx, r, top, insert);

	/* Convert to 'before' */
	if (insert.block)
		insert.block = insert.block->next;
	else
	{
		insert.block = parent ? parent->first_block : page->first_block;
		insert.parent = parent;
	}

	/* Make table */
	table = add_struct_block_before(ctx, insert.block, page, insert.parent, FZ_STRUCTURE_TABLE, "Table");

	/* Run through the cells, and guess at spanning. */
	for (y = 0; y < h-1; y++)
	{
		/* Have we sent this entire row before? */
		for (x = 0; x < w-1; x++)
		{
			if (!sent_tab[x+y*w])
				break;
		}
		if (x == w-1)
			continue; /* No point in sending a row with nothing in it! */

		/* Make TR */
		tr = add_struct_block_before(ctx, NULL, page, table, FZ_STRUCTURE_TR, "TR");

		for (x = 0; x < w-1; x++)
		{
			int x2, y2;
			int cellw = 1;
			int cellh = 1;

			/* Have we sent this cell already? */
			if (sent_tab[x+y*w])
				continue;

			/* Find the width of the cell */
			for (x2 = x+1; x2 < w-1; x2++)
			{
				cell_t *cell = get_cell(gd->cells, x2, y);
				if (cell->v_line)
					break; /* Can't go past a line */
				if (gd->xpos->list[x2].uncertainty == 0)
					break; /* An uncertainty of 0 is as good as a line. */
				if (!cell->v_crossed)
					break;
				cellw++;
			}
			/* Find the height of the cell */
			for (y2 = y+1; y2 < h-1; y2++)
			{
				cell_t *cell;
				int h_crossed = 0;
				if (gd->ypos->list[y2].uncertainty == 0)
					break; /* An uncertainty of 0 is as good as a line. */

				cell = get_cell(gd->cells, x, y2);
				if (cell->h_line)
					break; /* Can't extend down through a line. */
				if (cell->h_crossed)
					h_crossed = 1;
				for (x2 = x+1; x2 < x+cellw; x2++)
				{
					cell = get_cell(gd->cells, x2, y2);
					if (cell->h_line)
						break;
					if (cell->v_line)
						break; /* Can't go past a line */
					if (gd->xpos->list[x2].uncertainty == 0)
						break; /* An uncertainty of 0 is as good as a line. */
					if (!cell->v_crossed)
						break;
					if (cell->h_crossed)
						h_crossed = 1;
				}
				if (x2 == x+cellw && h_crossed)
					cellh++;
				else
					break;
			}
			/* Make TD */
			td = add_struct_block_before(ctx, NULL, page, tr, FZ_STRUCTURE_TD, "TD");
			r.x0 = gd->xpos->list[x].pos;
			r.x1 = gd->xpos->list[x+cellw].pos;
			r.y0 = gd->ypos->list[y].pos;
			r.y1 = gd->ypos->list[y+cellh].pos;
			/* Use r, not REAL contents bbox, as otherwise spanned rows
			 * can end up empty. */
			td->up->bbox = r;
			move_contained_content(ctx, page, td, parent, r);
#ifdef DEBUG_TABLE_STRUCTURE
			printf("(%d,%d) + (%d,%d)\n", x, y, cellw, cellh);
#endif
			for (y2 = y; y2 < y+cellh; y2++)
				for (x2 = x; x2 < x+cellw; x2++)
					sent_tab[x2+y2*w] = 1;
		}
		r.x0 = gd->xpos->list[0].pos;
		r.x1 = gd->xpos->list[gd->xpos->len-1].pos;
		r.y0 = gd->ypos->list[y].pos;
		r.y1 = gd->ypos->list[y+1].pos;
		tr->up->bbox = r;
		table->up->bbox = fz_union_rect(table->up->bbox, tr->up->bbox);
	}
	fz_free(ctx, sent_tab);

	{
		fz_stext_block *block;
		fz_stext_grid_positions *xps2 = copy_grid_positions_to_pool(ctx, page, gd->xpos);
		fz_stext_grid_positions *yps2 = copy_grid_positions_to_pool(ctx, page, gd->ypos);
		fz_stext_grid_info *info = copy_grid_info_to_pool(ctx, page, gd->cells);
		block = add_grid_block(ctx, page, &table->first_block, &table->last_block, table->up->id);
		block->u.b.xs = xps2;
		block->u.b.ys = yps2;
		block->u.b.info = info;
		block->bbox.x0 = block->u.b.xs->list[0].pos;
		block->bbox.y0 = block->u.b.ys->list[0].pos;
		block->bbox.x1 = block->u.b.xs->list[block->u.b.xs->len-1].pos;
		block->bbox.y1 = block->u.b.ys->list[block->u.b.ys->len-1].pos;
	}
	tidy_orphaned_tables(ctx, page, parent);

	return table;
}

static void
merge_column(grid_walker_data *gd, int x)
{
	int y;
	for (y = 0; y < gd->cells->h; y++)
	{
		cell_t *d = &gd->cells->cell[x + y * (gd->cells->w-1)];
		cell_t *s = &gd->cells->cell[x + y * gd->cells->w];

		if (x > 0)
			memmove(d-x, s-x, sizeof(*d) * x);
		d->full = s[0].full || s[1].full;
		d->h_crossed = s[0].h_crossed || s[1].h_crossed;
		d->h_line = s[0].h_line; /* == s[1].h_line */
		d->v_crossed = s[0].v_crossed;
		d->v_line = s[0].v_line;
		if (x < gd->cells->w - 2)
			memmove(d+1, s+2, sizeof(*d) * (gd->cells->w - 2 - x));
	}
	gd->cells->w--;

	if (x < gd->xpos->len - 2)
		memmove(&gd->xpos->list[x+1], &gd->xpos->list[x+2], sizeof(gd->xpos->list[0]) * (gd->xpos->len - 2 - x));
	gd->xpos->len--;
}

static void
merge_columns(grid_walker_data *gd)
{
	int x, y;

	for (x = gd->cells->w-3; x >= 0; x--)
	{
		/* Can column x be merged with column x+1? */
		/* An empty column can certainly be merged if the h_lines are the same,
		 * and there is no v_line. */
		for (y = 0; y < gd->cells->h-1; y++)
		{
			cell_t *a = get_cell(gd->cells, x, y);
			cell_t *b = get_cell(gd->cells, x+1, y);
			if (a->full || !!a->h_line != !!b->h_line || b->v_line)
				break;
		}
		if (y == gd->cells->h-1)
			goto merge_column;
		/* An empty column can certainly be merged if the h_lines are the same. */
		for (y = 0; y < gd->cells->h-1; y++)
		{
			cell_t *a = get_cell(gd->cells, x, y);
			cell_t *b = get_cell(gd->cells, x+1, y);
			if (b->full || !!a->h_line != !!b->h_line || b->v_line)
				break;
		}
		if (y == gd->cells->h-1)
			goto merge_column;
		/* We only ever want to merge columns if content crossed between them somewhere.
		 * Don't use uncertainty for this, because uncertainty doesn't allow for
		 * whitespace. */
		for (y = 0; y < gd->cells->h-1; y++)
			if (get_cell(gd->cells, x+1, y)->v_crossed == 1)
				break;
		if (y == gd->cells->h-1)
			continue;
		/* This requires all the pairs of cells in those 2 columns to be mergeable. */
		for (y = 0; y < gd->cells->h-1; y++)
		{
			cell_t *a = get_cell(gd->cells, x, y);
			cell_t *b = get_cell(gd->cells, x+1, y);
			/* If there is a divider, we can't merge. */
			if (b->v_line)
				break;
			/* If either is empty, we can merge. */
			if (!a->full || !b->full)
				continue;
			/* If we differ in h linedness, we can't merge */
			if (!!a->h_line != !!b->h_line)
				break;
			/* If both are full, we can only merge if we cross. */
			if (a->full && b->full && b->v_crossed)
				continue;
			/* Otherwise we can't merge */
			break;
		}
		if (y == gd->cells->h-1)
		{
			/* Merge the column! */
merge_column:
#ifdef DEBUG_TABLE_STRUCTURE
			printf("Merging column %d\n", x);
#endif
			merge_column(gd, x);
#ifdef DEBUG_TABLE_STRUCTURE
			asciiart_table(gd);
#endif
		}
	}
}

static void
merge_row(grid_walker_data *gd, int y)
{
	int x;
	int w = gd->cells->w;
	cell_t *d = &gd->cells->cell[y * w];
	for (x = 0; x < gd->cells->w-1; x++)
	{
		if (d->full == 0)
			d->full = d[w].full;
		if (d->v_crossed == 0)
			d->v_crossed = d[w].v_crossed;
		d++;
	}
	if (y < gd->cells->h - 2)
		memmove(d, d+w, sizeof(*d) * (gd->cells->h - 2 - y) * w);
	gd->cells->h--;

	if (y < gd->ypos->len - 2)
		memmove(&gd->ypos->list[y+1], &gd->ypos->list[y+2], sizeof(gd->ypos->list[0]) * (gd->ypos->len - 2 - y));
	gd->ypos->len--;
}

static void
merge_rows(grid_walker_data *gd)
{
	int x, y;

	for (y = gd->cells->h-3; y >= 0; y--)
	{
		/* Can row y be merged with row y+1? */
		/* An empty row can certainly be merged if the v_lines are the same,
		 * and there is no h_line. */
		for (x = 0; x < gd->cells->w-1; x++)
		{
			cell_t *a = get_cell(gd->cells, x, y);
			cell_t *b = get_cell(gd->cells, x, y+1);
			if (a->full || !!a->v_line != !!b->v_line || b->h_line)
				break;
		}
		if (x == gd->cells->w-1)
			goto merge_row;
		/* An empty row can certainly be merged if the v_lines are the same. */
		for (x = 0; x < gd->cells->w-1; x++)
		{
			cell_t *a = get_cell(gd->cells, x, y);
			cell_t *b = get_cell(gd->cells, x, y+1);
			if (b->full || !!a->v_line != !!b->v_line || b->h_line)
				break;
		}
		if (x == gd->cells->w-1)
			goto merge_row;
		/* We only ever want to merge rows if content crossed between them somewhere.
		 * Don't use uncertainty for this, because uncertainty doesn't allow for
		 * whitespace. */
		for (x = 0; x < gd->cells->w-1; x++)
			if (get_cell(gd->cells, x, y+1)->h_crossed == 1)
				break;
		if (x == gd->cells->w-1)
			continue;
		/* This requires all the pairs of cells in those 2 rows to be mergeable. */
		for (x = 0; x < gd->cells->w-1; x++)
		{
			cell_t *a = get_cell(gd->cells, x, y);
			cell_t *b = get_cell(gd->cells, x, y+1);
			/* If there is a divider, we can't merge. */
			if (b->h_line)
				goto cant_merge;
			/* If either is empty, we can merge. */
			if (!a->full || !b->full)
				continue;
			/* If we differ in v linedness, we can't merge */
			if (!!a->v_line != !!b->v_line)
				goto cant_merge;
			/* If both are full, we can only merge if we cross. */
			if (a->full && b->full && b->h_crossed)
				continue;
			/* Otherwise we can't merge */
			break;
		}
		if (x == gd->cells->w-1)
		{
			/* Merge the row! */
merge_row:
#ifdef DEBUG_TABLE_STRUCTURE
			printf("Merging row %d\n", y);
#endif
			merge_row(gd, y);
#ifdef DEBUG_TABLE_STRUCTURE
			asciiart_table(gd);
#endif
			continue;
		}

		/* OK, so we failed to be able to merge y and y+1. But if we get here, we
		 * know this wasn't because of any lines being in the way. So we can try
		 * a different set of criteria. If every non-empty cell in line y+1 is either
		 * into from line y, or crosses into line y+2, then it's probably a 'straddling'
		 * line. Just remove it. */
		if (y+2 >= gd->cells->h)
			continue;
		for (x = 0; x < gd->cells->w-1; x++)
		{
			cell_t *b = get_cell(gd->cells, x, y+1);
			cell_t *c = get_cell(gd->cells, x, y+2);
			if (!b->full)
				continue;
			if (!b->h_crossed && !c->h_crossed)
				break;
		}
		if (x == gd->cells->w-1)
		{
			/* Merge the row! */
#ifdef DEBUG_TABLE_STRUCTURE
			printf("Merging row %d (case 2)\n", y);
#endif
			merge_row(gd, y);
#ifdef DEBUG_TABLE_STRUCTURE
			asciiart_table(gd);
#endif
		}


cant_merge:
		{}
	}
}

static int
remove_bordered_empty_cells(grid_walker_data *gd)
{
	int x, y, x1, y1, x2, y2;
	int changed = 0;

	/* We are looking for a region of cells that's bordered,
	 * and empty, and where the borders don't extend...
	 *
	 *      '    '
	 *      '    '
	 * . . .'____'. . .
	 *      |    |
	 *      |    |
	 * . . .|____|. . .
	 *      '    '
	 *      '    '
	 *      '    '
	 */

	for (y = 0; y < gd->cells->h-1; y++)
	{
		for (x = 0; x < gd->cells->w-1; x++)
		{
			cell_t *u = y > 0 ? get_cell(gd->cells, x, y-1) : NULL;
			cell_t *l = x > 0 ? get_cell(gd->cells, x-1, y) : NULL;
			cell_t *c = get_cell(gd->cells, x, y);

			if (c->full)
				continue;
			if (!c->h_line || !c->v_line)
				continue;
			if (l != NULL && l->h_line)
				continue;
			if (u != NULL && u->v_line)
				continue;
			/* So c (x, y) is a reasonable top left of the bounded region. */
			for (x1 = x+1; x1 < gd->cells->w; x1++)
			{
				u = y > 0 ? get_cell(gd->cells, x1, y-1) : NULL;
				c = get_cell(gd->cells, x1, y);

				if (u != NULL && u->v_line)
					goto bad_region;
				if (c->h_line && !c->v_line)
					continue;
				if (c->h_line || !c->v_line)
					goto bad_region;
				break;
			}
			if (x1 == gd->cells->w)
				goto bad_region;
			/* If we get here, then we have the top row of a bounded region
			 * that runs from (x,y) to (x1-1, y). So, can we extend that region
			 * downwards? */
			for (y1 = y+1; y1 < gd->cells->h; y1++)
			{
				c = get_cell(gd->cells, x, y1);

				if (!c->h_line && c->v_line)
				{
					/* This could be the start of a line. */
					for (x2 = x+1; x2 < x1; x2++)
					{
						c = get_cell(gd->cells, x2, y1);
						if (c->full || c->h_line || c->v_line)
							goto bad_region;
					}
					c = get_cell(gd->cells, x1, y1);
					if (c->h_line || !c->v_line)
						goto bad_region;
					/* That line was fine! Region is extended. */
				}
				else if (c->h_line && !c->v_line)
				{
					/* This might be the bottom line of the bounded region. */
					for (x2 = x+1; x2 < x1; x2++)
					{
						c = get_cell(gd->cells, x2, y1);
						if (!c->h_line || c->v_line)
							goto bad_region;
					}
					c = get_cell(gd->cells, x1, y1);
					if (c->h_line || c->v_line)
						goto bad_region;
					break;
				}
				else
					goto bad_region;
			}
			if (y1 == gd->cells->h)
				goto bad_region;
			/* So we have a bounded region from x,y to x1-1,y1-1 */
			for (y2 = y; y2 < y1; y2++)
			{
				for (x2 = x; x2 < x1; x2++)
				{
					c = get_cell(gd->cells, x2, y2);
					c->h_line = 0;
					c->v_line = 0;
					c->full = 1;
					if (x2 > x)
						c->v_crossed = 1;
					if (y2 > y)
						c->h_crossed = 1;
				}
				c = get_cell(gd->cells, x2, y2);
				c->v_line = 0;
			}
			for (x2 = x; x2 < x1; x2++)
				get_cell(gd->cells, x2, y2)->h_line = 0;
			changed = 1;
bad_region:
			{}
		}
	}

#ifdef DEBUG_TABLE_STRUCTURE
	if (changed)
	{
		printf("Simplified empty bounded cells to give:\n");
		asciiart_table(gd);
	}
#endif

	return changed;
}

static int
find_table_within_bounds(fz_context *ctx, grid_walker_data *gd, fz_stext_block *content, fz_rect bounds)
{
	div_list xs = { 0 };
	div_list ys = { 0 };
	int failed = 1;
	int bg;

	fz_try(ctx)
	{
		walk_to_find_content(ctx, &xs, &ys, content, bounds);

		sanitize_positions(ctx, &xs);
		sanitize_positions(ctx, &ys);

		/* Run across the line, counting 'winding' */
		/* If we don't have at least 2 rows and 2 columns, give up. */
		if (xs.len <= 2 || ys.len <= 2)
			break;

		gd->xpos = make_table_positions(ctx, &xs, bounds.x0, bounds.x1);
		gd->ypos = make_table_positions(ctx, &ys, bounds.y0, bounds.y1);
		gd->cells = new_cells(ctx, gd->xpos->len, gd->ypos->len);

		bg = walk_for_background(ctx, gd, content);
		if (bg == BACKGROUND_FOUND)
			gd->has_background = 1;

		/* Walk the content looking for grid lines. These
		 * lines refine our positions. */
		walk_grid_lines(ctx, gd, content);
		/* Now, we walk the content looking for content that crosses
		 * these grid lines. This allows us to spot spanned cells. */
		if (calculate_spanned_content(ctx, gd, content))
			break; /* Unlikely to be a table. */

#ifdef DEBUG_TABLE_STRUCTURE
		asciiart_table(gd);
#endif
		/* Now, can we remove some columns or rows? i.e. have we oversegmented? */
		do
		{
			merge_columns(gd);
			merge_rows(gd);
		}
		while (remove_bordered_empty_cells(gd));

		/* Did we shrink the table so much it's not a table any more? */
		if (gd->xpos->len <= 2 || gd->ypos->len <= 2)
			break;

		failed = 0;
	}
	fz_always(ctx)
	{
		fz_free(ctx, xs.list);
		fz_free(ctx, ys.list);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return failed;
}

/* The score for a table can be thought of as a judgement of
 * how 'awkward' a table is.
 *
 *  + Score 1 for every empty cell that doesn't have "supporting"
 *    borders.
 *  + Score 1 for every 'crossing' between cells.
 */
static float
score_table(fz_context *ctx, grid_walker_data *gd)
{
	int x, y;
	int w = gd->cells->w;
	int h = gd->cells->h;
	int score = 0;
	int num_cells = (w-1)*(h-1);

	assert(num_cells > 0);

	for (y = 0; y < h-1; y++)
	{
		for (x = 0; x < w-1; x++)
		{
			cell_t *cell = get_cell(gd->cells, x, y);
			cell_t *right = get_cell(gd->cells, x+1, y);
			cell_t *below = get_cell(gd->cells, x, y+1);
			score += cell->h_crossed + cell->v_crossed;
			if (cell->full)
			{
				/* We have content. */
			}
			else if (cell->h_line && cell->v_line && right->v_line && below->h_line)
			{
				/* The cell is properly bordered. */
			}
			else
				score++;
		}
	}

	return score / (float)num_cells;
}

static int
do_table_hunt(fz_context *ctx, fz_stext_page *page, fz_stext_struct *parent, int *has_background, fz_rect top_bounds, float *subtable_score, float score_threshold)
{
	fz_stext_block *block;
	int count;
	fz_stext_block **first_block = parent ? &parent->first_block : &page->first_block;
	int num_subtables = 0;
	grid_walker_data gd = { 0 };
	float score;

	*subtable_score = 0;

	/* No content? Just bale. */
	if (*first_block == NULL)
		return 0;

	/* If all the content here looks like a column of text, don't
	 * hunt for a table within it. */
	if (all_blocks_are_justified_or_headers(ctx, *first_block))
		return num_subtables;

	gd.bounds = top_bounds;

	/* First off, descend into any children to see if those look like tables. */
	count = 0;
	for (block = *first_block; block != NULL; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_STRUCT)
		{
			if (block->u.s.down)
			{
				int background = 0;
				num_subtables += do_table_hunt(ctx, page, block->u.s.down, &background, top_bounds, subtable_score, score_threshold);
				if (background)
					*has_background = 1;
				count++;
			}
		}
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
			count++;
	}

	/* If we don't have at least a single child, no more to hunt. */
	/* We only need a single block, because a single text block can
	 * contain an entire unbordered table. */
	if (count < 1)
		return num_subtables;

	/* If at least one of the subtables we found has a background on it, then
	 * probably they all do, and we've probably got a good match. */
	if (*has_background)
		return num_subtables;

	/* We only look for a table at this level if we either at the top
	 * or on a div. This saves us looking for tables within an 'H'
	 * for example. */
	if (parent != NULL && parent->standard != FZ_STRUCTURE_DIV)
		return num_subtables;


	fz_var(gd);

	fz_try(ctx)
	{
		/* Now see whether the content looks like tables. */
		fz_rect bounds = walk_to_find_bounds(ctx, *first_block);

		if (find_table_within_bounds(ctx, &gd, *first_block, bounds))
			break;

		score = score_table(ctx, &gd);
#ifdef DEBUG_TABLE_STRUCTURE
		printf("Bounds: %g %g %g %g Score: %g\n",
			bounds.x0, bounds.y0, bounds.x1, bounds.y1, score);
#endif

		*has_background = gd.has_background;

		if (num_subtables > 0)
		{
			/* Our table's score needs to be lower than the sum of the scores
			 * for the subtables for us to accept it. */
			int x, y;
			if (score > *subtable_score)
				break;
			/* We are risking throwing away a table we've already found for this
			 * one. Only do it if this one is really convincing. */
			for (x = 0; x < gd.xpos->len; x++)
				if (gd.xpos->list[x].uncertainty != 0)
					break;
			if (x != gd.xpos->len)
				break;
			for (y = 0; y < gd.ypos->len; y++)
				if (gd.ypos->list[y].uncertainty != 0)
					break;
			if (y != gd.ypos->len)
				break;
		}

		if (score > score_threshold)
			break;

		*subtable_score = score;

#ifdef DEBUG_TABLE_STRUCTURE
		printf("Transcribing table: (%g,%g)->(%g,%g)\n",
			gd.xpos->list[0].pos,
			gd.ypos->list[0].pos,
			gd.xpos->list[gd.xpos->len-1].pos,
			gd.ypos->list[gd.ypos->len-1].pos);
#endif

		(void)transcribe_table(ctx, &gd, page, parent);
		num_subtables = 1;
#ifdef DEBUG_WRITE_AS_PS
		{
			int i;
			printf("%% TABLE\n");
			for (i = 0; i < block->u.b.xs->len; i++)
			{
				if (block->u.b.xs->list[i].uncertainty)
					printf("0 1 0 setrgbcolor\n");
				else
					printf("0 0.5 0 setrgbcolor\n");
				printf("%g %g moveto %g %g lineto stroke\n",
					block->u.b.xs->list[i].pos, block->bbox.y0,
					block->u.b.xs->list[i].pos, block->bbox.y1);
			}
			for (i = 0; i < block->u.b.ys->len; i++)
			{
				if (block->u.b.ys->list[i].uncertainty)
					printf("0 1 0 setrgbcolor\n");
				else
				printf("0 0.5 0 setrgbcolor\n");
				printf("%g %g moveto %g %g lineto stroke\n",
					block->bbox.x0, block->u.b.ys->list[i].pos,
					block->bbox.x1, block->u.b.ys->list[i].pos);
			}
		}
#endif
	}
	fz_always(ctx)
	{
		fz_free(ctx, gd.xpos);
		fz_free(ctx, gd.ypos);
		fz_free(ctx, gd.cells);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return num_subtables;
}

void
fz_table_hunt(fz_context *ctx, fz_stext_page *page)
{
	int has_background = 0;
	float score;

	if (page == NULL)
		return;

	assert(verify_stext(ctx, page, NULL));

	/* FIXME: Nasty heuristic threshold. */
	do_table_hunt(ctx, page, NULL, &has_background, fz_infinite_rect, &score, 0.3f);

	assert(verify_stext(ctx, page, NULL));
}

void
fz_table_hunt_within_bounds(fz_context *ctx, fz_stext_page *page, fz_rect rect)
{
	int has_background = 0;
	float score;

	if (page == NULL)
		return;

	assert(verify_stext(ctx, page, NULL));

	fz_segment_stext_rect(ctx, page, rect);

	do_table_hunt(ctx, page, NULL, &has_background, rect, &score, 1);

	assert(verify_stext(ctx, page, NULL));
}

fz_stext_block *
fz_find_table_within_bounds(fz_context *ctx, fz_stext_page *page, fz_rect bounds)
{
	fz_stext_struct *table = NULL;
	grid_walker_data gd = { 0 };

	/* No content? Just bale. */
	if (page == NULL || page->first_block == NULL)
		return NULL;

	fz_var(gd);

	fz_try(ctx)
	{
		gd.bounds = bounds;
		if (find_table_within_bounds(ctx, &gd, page->first_block, bounds))
			break;

#ifdef DEBUG_TABLE_STRUCTURE
		printf("Transcribing table: (%g,%g)->(%g,%g)\n",
			gd.xpos->list[0].pos,
			gd.ypos->list[0].pos,
			gd.xpos->list[gd.xpos->len-1].pos,
			gd.ypos->list[gd.ypos->len-1].pos);
#endif

		/* Now we should have the entire table calculated. */
		table = transcribe_table(ctx, &gd, page, NULL);
#ifdef DEBUG_WRITE_AS_PS
		if (table && table->first_block && table->first_block->type == FZ_STEXT_BLOCK_GRID)
		{
			int i;
			fz_stext_block *block = table->first_block;
			printf("%% TABLE\n");
			for (i = 0; i < block->u.b.xs->len; i++)
			{
				if (block->u.b.xs->list[i].uncertainty)
					printf("0 1 0 setrgbcolor\n");
				else
					printf("0 0.5 0 setrgbcolor\n");
				printf("%g %g moveto %g %g lineto stroke\n",
					block->u.b.xs->list[i].pos, block->bbox.y0,
					block->u.b.xs->list[i].pos, block->bbox.y1);
			}
			for (i = 0; i < block->u.b.ys->len; i++)
			{
				if (block->u.b.ys->list[i].uncertainty)
					printf("0 1 0 setrgbcolor\n");
				else
				printf("0 0.5 0 setrgbcolor\n");
				printf("%g %g moveto %g %g lineto stroke\n",
					block->bbox.x0, block->u.b.ys->list[i].pos,
					block->bbox.x1, block->u.b.ys->list[i].pos);
			}
		}
#endif
	}
	fz_always(ctx)
	{
		fz_free(ctx, gd.xpos);
		fz_free(ctx, gd.ypos);
		fz_free(ctx, gd.cells);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return table ? table->first_block : NULL;
}
