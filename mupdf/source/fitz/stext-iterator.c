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

fz_stext_page_block_iterator fz_stext_page_block_iterator_begin(fz_stext_page *page)
{
	fz_stext_page_block_iterator pos;

	pos.page = page;
	pos.parent = NULL;
	pos.block = page ? page->first_block : NULL;

	return pos;
}

/* Iterates along, stopping at every block. Stops at the end of the run. */
fz_stext_page_block_iterator fz_stext_page_block_iterator_next(fz_stext_page_block_iterator pos)
{
	/* If page == NULL, then this iterator can never go anywhere */
	if (pos.page == NULL)
		return pos;

	/* If we've hit EOF, then nowhere else to go. */
	if (pos.block == NULL)
		return pos;

	pos.block = pos.block->next;
	return pos;
}

fz_stext_page_block_iterator fz_stext_page_block_iterator_down(fz_stext_page_block_iterator pos)
{
	/* Can't throw here, so trying to move down on illegal nodes
	 * will just do nothing. */
	if (pos.block == NULL)
		return pos;
	if (pos.block->type != FZ_STEXT_BLOCK_STRUCT)
		return pos;

	pos.parent = pos.block->u.s.down;
	pos.block = pos.block->u.s.down->first_block;

	return pos;
}

fz_stext_page_block_iterator fz_stext_page_block_iterator_up(fz_stext_page_block_iterator pos)
{
	if (pos.parent == NULL)
		return pos;

	/* pos.parent->up is the struct block we are currently traversing the
	 * children of. So it's where we want to do 'next' from. */
	pos.block = pos.parent->up;
	/* pos.parent->parent is the struct that owns the new pos.block */
	pos.parent = pos.parent->parent;

	return pos;
}

/* Iterates along, and automatically (silently) goes down at structure
 * nodes and up at the end of runs. */
fz_stext_page_block_iterator fz_stext_page_block_iterator_next_dfs(fz_stext_page_block_iterator pos)
{
	while (1)
	{
		pos = fz_stext_page_block_iterator_next(pos);

		if (pos.block)
		{
			if (pos.block->type != FZ_STEXT_BLOCK_STRUCT)
				return pos;

			/* Move down. And loop. */
			pos.parent = pos.block->u.s.down;
			pos.block = pos.block->u.s.down->first_block;
			continue;
		}

		/* We've hit the end of the row. Move up. */
		/* If no parent, we've really hit the EOD. */
		if (pos.parent == NULL)
			return pos; /* EOF */
		/* pos.parent->up is the struct block we are currently traversing the
		 * children of. So it's where we want to do 'next' from. */
		pos.block = pos.parent->up;
		/* pos.parent->parent is the struct that owns the new pos.block */
		pos.parent = pos.parent->parent;
	}
}

int fz_stext_page_block_iterator_eod(fz_stext_page_block_iterator pos)
{
	return (pos.block == NULL);
}

int fz_stext_page_block_iterator_eod_dfs(fz_stext_page_block_iterator pos)
{
	while (1)
	{
		if (pos.block)
			return 0;
		if (pos.parent == NULL)
			return 1;
		pos.block = pos.parent->up;
		pos.parent = pos.parent->parent;
	}
}
