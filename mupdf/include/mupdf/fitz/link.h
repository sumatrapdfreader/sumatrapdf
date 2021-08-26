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

#ifndef MUPDF_FITZ_LINK_H
#define MUPDF_FITZ_LINK_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"

/**
	fz_link is a list of interactive links on a page.

	There is no relation between the order of the links in the
	list and the order they appear on the page. The list of links
	for a given page can be obtained from fz_load_links.

	A link is reference counted. Dropping a reference to a link is
	done by calling fz_drop_link.

	rect: The hot zone. The area that can be clicked in
	untransformed coordinates.

	uri: Link destinations come in two forms: internal and external.
	Internal links refer to other pages in the same document.
	External links are URLs to other documents.

	next: A pointer to the next link on the same page.
*/
typedef struct fz_link
{
	int refs;
	struct fz_link *next;
	fz_rect rect;
	char *uri;
} fz_link;

/**
	Create a new link record.

	next is set to NULL with the expectation that the caller will
	handle the linked list setup.

	Internal function.
*/
fz_link *fz_new_link(fz_context *ctx, fz_rect bbox, const char *uri);

/**
	Increment the reference count for a link. The same pointer is
	returned.

	Never throws exceptions.
*/
fz_link *fz_keep_link(fz_context *ctx, fz_link *link);

/**
	Decrement the reference count for a link. When the reference
	count reaches zero, the link is destroyed.

	When a link is freed, the reference for any linked link (next)
	is dropped too, thus an entire linked list of fz_link's can be
	freed by just dropping the head.
*/
void fz_drop_link(fz_context *ctx, fz_link *link);

/**
	Query whether a link is external to a document (determined by
	uri containing a ':', intended to match with '://' which
	separates the scheme from the scheme specific parts in URIs).
*/
int fz_is_external_link(fz_context *ctx, const char *uri);

#endif
