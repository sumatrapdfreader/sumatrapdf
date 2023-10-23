// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#include <math.h>

fz_link *
fz_new_link_of_size(fz_context *ctx, int size, fz_rect rect, const char *uri)
{
	fz_link *link = Memento_label(fz_calloc(ctx, 1, size), "fz_link");
	link->refs = 1;
	link->rect = rect;

	fz_try(ctx)
		link->uri = fz_strdup(ctx, uri);
	fz_catch(ctx)
	{
		fz_drop_link(ctx, link);
		fz_rethrow(ctx);
	}

	return link;
}

fz_link *
fz_keep_link(fz_context *ctx, fz_link *link)
{
	return fz_keep_imp(ctx, link, &link->refs);
}

void
fz_drop_link(fz_context *ctx, fz_link *link)
{
	while (fz_drop_imp(ctx, link, &link->refs))
	{
		fz_link *next = link->next;

		if (link->drop)
			link->drop(ctx, link);

		fz_free(ctx, link->uri);
		fz_free(ctx, link);
		link = next;
	}
}

int
fz_is_external_link(fz_context *ctx, const char *uri)
{
	const char *mark;
	/* Basically, this function returns true, if the URI starts with
	 * a valid 'scheme' followed by ':'. */

	 if (!uri)
		 return 0;

	/* All schemes must start with a letter; exit if we don't. */
	if ((*uri < 'a' || *uri > 'z') && (*uri < 'A' || *uri > 'Z'))
		return 0;
	uri++;

	mark = uri;

	/* Subsequent characters can be letters, digits, +, -, or . */
	while ((*uri >= 'a' && *uri <= 'z') ||
		(*uri >= 'A' && *uri <= 'Z') ||
		(*uri >= '0' && *uri <= '9') ||
		(*uri == '+') ||
		(*uri == '-') ||
		(*uri == '.'))
		++uri;

	return uri[0] == ':' && (uri - mark) > 1;
}

fz_link_dest fz_make_link_dest_none(void)
{
	fz_link_dest dest = { { -1, -1 }, FZ_LINK_DEST_XYZ, NAN, NAN, NAN, NAN, NAN };
	return dest;
}

fz_link_dest fz_make_link_dest_xyz(int chapter, int page, float x, float y, float z)
{
	fz_link_dest dest = { { chapter, page }, FZ_LINK_DEST_XYZ, x, y, NAN, NAN, z };
	return dest;
}
