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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <assert.h>

static int
pdf_make_hash_key(fz_context *ctx, fz_store_hash *hash, void *key_)
{
	pdf_obj *key = (pdf_obj *)key_;

	if (!pdf_is_indirect(ctx, key))
		return 0;
	hash->u.pi.i = pdf_to_num(ctx, key);
	hash->u.pi.ptr = pdf_get_indirect_document(ctx, key);
	return 1;
}

static void *
pdf_keep_key(fz_context *ctx, void *key)
{
	return (void *)pdf_keep_obj(ctx, (pdf_obj *)key);
}

static void
pdf_drop_key(fz_context *ctx, void *key)
{
	pdf_drop_obj(ctx, (pdf_obj *)key);
}

static int
pdf_cmp_key(fz_context *ctx, void *k0, void *k1)
{
	return pdf_objcmp(ctx, (pdf_obj *)k0, (pdf_obj *)k1);
}

static void
pdf_format_key(fz_context *ctx, char *s, size_t n, void *key_)
{
	pdf_obj *key = (pdf_obj *)key_;
	if (pdf_is_indirect(ctx, key))
		fz_snprintf(s, n, "(%d 0 R)", pdf_to_num(ctx, key));
	else
	{
		size_t t;
		char *p = pdf_sprint_obj(ctx, s, n, &t, key, 1, 0);
		if (p != s) {
			fz_strlcpy(s, p, n);
			fz_free(ctx, p);
		}
	}
}

static const fz_store_type pdf_obj_store_type =
{
	"pdf_obj",
	pdf_make_hash_key,
	pdf_keep_key,
	pdf_drop_key,
	pdf_cmp_key,
	pdf_format_key,
	NULL
};

void
pdf_store_item(fz_context *ctx, pdf_obj *key, void *val, size_t itemsize)
{
	void *existing;

	assert(pdf_is_name(ctx, key) || pdf_is_array(ctx, key) || pdf_is_dict(ctx, key) || pdf_is_indirect(ctx, key));
	existing = fz_store_item(ctx, key, val, itemsize, &pdf_obj_store_type);
	if (existing)
		fz_warn(ctx, "unexpectedly replacing entry in PDF store");
}

void *
pdf_find_item(fz_context *ctx, fz_store_drop_fn *drop, pdf_obj *key)
{
	return fz_find_item(ctx, drop, key, &pdf_obj_store_type);
}

void
pdf_remove_item(fz_context *ctx, fz_store_drop_fn *drop, pdf_obj *key)
{
	fz_remove_item(ctx, drop, key, &pdf_obj_store_type);
}

static int
pdf_filter_store(fz_context *ctx, void *doc_, void *key)
{
	pdf_document *doc = (pdf_document *)doc_;
	pdf_obj *obj = (pdf_obj *)key;
	pdf_document *key_doc = pdf_get_bound_document(ctx, obj);

	return (doc == key_doc);
}

void
pdf_empty_store(fz_context *ctx, pdf_document *doc)
{
	fz_filter_store(ctx, pdf_filter_store, doc, &pdf_obj_store_type);
	fz_drop_drawn_tiles_for_document(ctx, (fz_document *)doc);
}

static int
pdf_filter_locals(fz_context *ctx, void *doc_, void *key)
{
	pdf_document *doc = (pdf_document *)doc_;
	pdf_obj *obj = (pdf_obj *)key;
	pdf_document *key_doc = pdf_get_bound_document(ctx, obj);

	return (doc == key_doc && pdf_is_local_object(ctx, doc, obj));
}

void pdf_purge_locals_from_store(fz_context *ctx, pdf_document *doc)
{
	fz_filter_store(ctx, pdf_filter_locals, doc, &pdf_obj_store_type);
}

struct doc_num_info {
	pdf_document *doc;
	int num;
};

static int
pdf_filter_object_number(fz_context *ctx, void *ref_, void *key_)
{
	struct doc_num_info *ref = ref_;
	pdf_obj *key_obj = (pdf_obj *)key_;
	pdf_document *key_doc = pdf_get_bound_document(ctx, key_obj);
	return (ref->doc == key_doc && ref->num == pdf_to_num(ctx, key_obj));
}

void pdf_purge_object_from_store(fz_context *ctx, pdf_document *doc, int num)
{
	struct doc_num_info ref = { doc, num };
	fz_filter_store(ctx, pdf_filter_object_number, &ref, &pdf_obj_store_type);
}
