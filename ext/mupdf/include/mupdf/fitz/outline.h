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

#ifndef MUPDF_FITZ_OUTLINE_H
#define MUPDF_FITZ_OUTLINE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/types.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/link.h"
#include "mupdf/fitz/output.h"

/* Outline */

typedef struct {
	char *title;
	char *uri;
	int is_open;
	int flags;
	float r;
	float g;
	float b;
} fz_outline_item;

enum
{
	FZ_OUTLINE_FLAG_BOLD = 1,
	FZ_OUTLINE_FLAG_ITALIC = 2,
};

enum
{
	FZ_OUTLINE_ITERATOR_DID_NOT_MOVE = -1,
	FZ_OUTLINE_ITERATOR_AT_ITEM = 0,
	FZ_OUTLINE_ITERATOR_AT_EMPTY = 1,
};

typedef struct fz_outline_iterator fz_outline_iterator;

/**
	Call to get the current outline item.

	Can return NULL. The item is only valid until the next call.
*/
fz_outline_item *fz_outline_iterator_item(fz_context *ctx, fz_outline_iterator *iter);

/**
	Calls to move the iterator position.

	A negative return value means we could not move as requested. Otherwise:
	0 = the final position has a valid item.
	1 = not a valid item, but we can insert an item here.
*/
int fz_outline_iterator_next(fz_context *ctx, fz_outline_iterator *iter);
int fz_outline_iterator_prev(fz_context *ctx, fz_outline_iterator *iter);
int fz_outline_iterator_up(fz_context *ctx, fz_outline_iterator *iter);
int fz_outline_iterator_down(fz_context *ctx, fz_outline_iterator *iter);

/**
	Call to insert a new item BEFORE the current point.

	Ownership of pointers are retained by the caller. The item data will be copied.

	After an insert, we do not change where we are pointing.
	The return code is the same as for next, it indicates the current iterator position.

	Note that for PDF documents at least, the is_open field is ignored. All childless
	nodes are considered closed by PDF, hence (given every newly inserted node is
	childless by definition) all new nodes are inserted with is_open == false.
*/
int fz_outline_iterator_insert(fz_context *ctx, fz_outline_iterator *iter, fz_outline_item *item);

/**
	Delete the current item.

	This implicitly moves us to the 'next' item, and the return code is as for fz_outline_iterator_next.
*/
int fz_outline_iterator_delete(fz_context *ctx, fz_outline_iterator *iter);

/**
	Update the current item properties according to the given item.
*/
void fz_outline_iterator_update(fz_context *ctx, fz_outline_iterator *iter, fz_outline_item *item);

/**
	Drop the current iterator.
*/
void fz_drop_outline_iterator(fz_context *ctx, fz_outline_iterator *iter);


/** Structure based API */

/**
	fz_outline is a tree of the outline of a document (also known
	as table of contents).

	title: Title of outline item using UTF-8 encoding. May be NULL
	if the outline item has no text string.

	uri: Destination in the document to be displayed when this
	outline item is activated. May be an internal or external
	link, or NULL if the outline item does not have a destination.

	page: The page number of an internal link, or -1 for external
	links or links with no destination.

	next: The next outline item at the same level as this outline
	item. May be NULL if no more outline items exist at this level.

	down: The outline items immediate children in the hierarchy.
	May be NULL if no children exist.

	is_open: If zero, the outline element is closed in the UI. If
	1, it should be open, showing any child elements.

	flags: Bit 0 set -> Bold, Bit 1 set -> Italic. All other bits
	reserved.

	r, g, b: The RGB components of the color of this entry.
*/
typedef struct fz_outline
{
	int refs;
	char *title;
	char *uri;
	fz_location page;
	float x, y;
	struct fz_outline *next;
	struct fz_outline *down;
	unsigned int is_open : 1;
	unsigned int flags : 7;
	unsigned int r : 8;
	unsigned int g : 8;
	unsigned int b : 8;
} fz_outline;

/**
	Create a new outline entry with zeroed fields for the caller
	to fill in.
*/
fz_outline *fz_new_outline(fz_context *ctx);

/**
	Increment the reference count. Returns the same pointer.

	Never throws exceptions.
*/
fz_outline *fz_keep_outline(fz_context *ctx, fz_outline *outline);

/**
	Decrements the reference count. When the reference point
	reaches zero, the outline is freed.

	When freed, it will drop linked	outline entries (next and down)
	too, thus a whole outline structure can be dropped by dropping
	the top entry.

	Never throws exceptions.
*/
void fz_drop_outline(fz_context *ctx, fz_outline *outline);

/**
	Routine to implement the old Structure based API from an iterator.
*/
fz_outline *
fz_load_outline_from_iterator(fz_context *ctx, fz_outline_iterator *iter);


/**
	Implementation details.
	Of use to people coding new document handlers.
*/

/**
	Function type for getting the current item.

	Can return NULL. The item is only valid until the next call.
*/
typedef fz_outline_item *(fz_outline_iterator_item_fn)(fz_context *ctx, fz_outline_iterator *iter);

/**
	Function types for moving the iterator position.

	A negative return value means we could not move as requested. Otherwise:
	0 = the final position has a valid item.
	1 = not a valid item, but we can insert an item here.
*/
typedef int (fz_outline_iterator_next_fn)(fz_context *ctx, fz_outline_iterator *iter);
typedef int (fz_outline_iterator_prev_fn)(fz_context *ctx, fz_outline_iterator *iter);
typedef int (fz_outline_iterator_up_fn)(fz_context *ctx, fz_outline_iterator *iter);
typedef int (fz_outline_iterator_down_fn)(fz_context *ctx, fz_outline_iterator *iter);

/**
	Function type for inserting a new item BEFORE the current point.

	Ownership of pointers are retained by the caller. The item data will be copied.

	After an insert, we implicitly do a next, so that a successive insert operation
	would insert after the item inserted here. The return code is therefore as for next.
*/
typedef int (fz_outline_iterator_insert_fn)(fz_context *ctx, fz_outline_iterator *iter, fz_outline_item *item);

/**
	Function type for deleting the current item.

	This implicitly moves us to the 'next' item, and the return code is as for fz_outline_iterator_next.
*/
typedef int (fz_outline_iterator_delete_fn)(fz_context *ctx, fz_outline_iterator *iter);

/**
	Function type for updating the current item properties according to the given item.
*/
typedef void (fz_outline_iterator_update_fn)(fz_context *ctx, fz_outline_iterator *iter, fz_outline_item *item);

/**
	Function type for dropping the current iterator.
*/
typedef void (fz_outline_iterator_drop_fn)(fz_context *ctx, fz_outline_iterator *iter);

#define fz_new_derived_outline_iter(CTX, TYPE, DOC)\
	((TYPE *)Memento_label(fz_new_outline_iterator_of_size(ctx,sizeof(TYPE),DOC),#TYPE))

fz_outline_iterator *fz_new_outline_iterator_of_size(fz_context *ctx, size_t size, fz_document *doc);

fz_outline_iterator *fz_outline_iterator_from_outline(fz_context *ctx, fz_outline *outline);

struct fz_outline_iterator {
	/* Functions */
	fz_outline_iterator_drop_fn *drop;
	fz_outline_iterator_item_fn *item;
	fz_outline_iterator_next_fn *next;
	fz_outline_iterator_prev_fn *prev;
	fz_outline_iterator_up_fn *up;
	fz_outline_iterator_down_fn *down;
	fz_outline_iterator_insert_fn *insert;
	fz_outline_iterator_update_fn *update;
	fz_outline_iterator_delete_fn *del;
	/* Common state */
	fz_document *doc;
};

#endif
