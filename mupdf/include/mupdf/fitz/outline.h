// Copyright (C) 2004-2021 Artifex Software, Inc.
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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#ifndef MUPDF_FITZ_OUTLINE_H
#define MUPDF_FITZ_OUTLINE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/link.h"
#include "mupdf/fitz/output.h"

/* Outline */

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
*/
typedef struct fz_outline
{
	int refs;
	char *title;
	char *uri;
	int page;
	float x, y;
	struct fz_outline *next;
	struct fz_outline *down;
	int is_open;

	/* sumatrapdf: support color and flags */
	int flags;
	int n_color;
	float color[4];
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

#endif
