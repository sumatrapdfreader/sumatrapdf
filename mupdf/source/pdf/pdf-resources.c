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
#include "mupdf/pdf.h"

#include <string.h>

static void pdf_drop_obj_as_void(fz_context *ctx, void *obj)
{
	pdf_drop_obj(ctx, obj);
}

/* We do need to come up with an effective way to see what is already in the
 * file to avoid adding to what is already there. This is avoided for pdfwrite
 * as we check as we add each font. For adding text to an existing file though
 * it may be more problematic. */

pdf_obj *
pdf_find_font_resource(fz_context *ctx, pdf_document *doc, int type, int encoding, fz_font *item, pdf_font_resource_key *key)
{
	pdf_obj *res;

	if (!doc->resources.fonts)
		doc->resources.fonts = fz_new_hash_table(ctx, 4096, sizeof(*key), -1, pdf_drop_obj_as_void);

	memset(key, 0, sizeof(*key));
	fz_font_digest(ctx, item, key->digest);

	key->type = type;
	key->encoding = encoding;
	key->local_xref = doc->local_xref_nesting > 0;

	res = fz_hash_find(ctx, doc->resources.fonts, (void *)key);
	if (res)
		pdf_keep_obj(ctx, res);
	return res;
}

pdf_obj *
pdf_find_colorspace_resource(fz_context *ctx, pdf_document *doc, fz_colorspace *item, pdf_colorspace_resource_key *key)
{
	pdf_obj *res;

	if (!doc->resources.colorspaces)
		doc->resources.colorspaces = fz_new_hash_table(ctx, 4096, sizeof(*key), -1, pdf_drop_obj_as_void);

	memset(key, 0, sizeof(*key));
	fz_colorspace_digest(ctx, item, key->digest);

	key->local_xref = doc->local_xref_nesting > 0;

	res = fz_hash_find(ctx, doc->resources.colorspaces, (void *)key);
	if (res)
		pdf_keep_obj(ctx, res);
	return res;
}

pdf_obj *
pdf_insert_font_resource(fz_context *ctx, pdf_document *doc, pdf_font_resource_key *key, pdf_obj *obj)
{
	pdf_obj *res = fz_hash_insert(ctx, doc->resources.fonts, (void *)key, obj);
	if (res)
		fz_warn(ctx, "warning: font resource already present");
	else
		res = pdf_keep_obj(ctx, obj);
	return pdf_keep_obj(ctx, res);
}

pdf_obj *
pdf_insert_colorspace_resource(fz_context *ctx, pdf_document *doc, pdf_colorspace_resource_key *key, pdf_obj *obj)
{
	pdf_obj *res = fz_hash_insert(ctx, doc->resources.colorspaces, (void *)key, obj);
	if (res)
		fz_warn(ctx, "warning: colorspace resource already present");
	else
		res = pdf_keep_obj(ctx, obj);
	return pdf_keep_obj(ctx, res);
}

static int purge_local_font_resource(fz_context *ctx, void *state, void *key_, int keylen, void *val)
{
	pdf_font_resource_key *key = key_;
	if (key->local_xref)
	{
		pdf_drop_obj(ctx, val);
		return 1;
	}
	return 0;
}

static int purge_local_colorspace_resource(fz_context *ctx, void *state, void *key_, int keylen, void *val)
{
	pdf_colorspace_resource_key *key = key_;
	if (key->local_xref)
	{
		pdf_drop_obj(ctx, val);
		return 1;
	}
	return 0;
}

void
pdf_purge_local_resources(fz_context *ctx, pdf_document *doc)
{
	if (doc->resources.fonts)
		fz_hash_filter(ctx, doc->resources.fonts, NULL, purge_local_font_resource);
	if (doc->resources.colorspaces)
		fz_hash_filter(ctx, doc->resources.colorspaces, NULL, purge_local_colorspace_resource);
}

void
pdf_drop_resource_tables(fz_context *ctx, pdf_document *doc)
{
	if (doc)
	{
		fz_drop_hash_table(ctx, doc->resources.colorspaces);
		fz_drop_hash_table(ctx, doc->resources.fonts);
	}
}
