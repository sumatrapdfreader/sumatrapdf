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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef struct pdf_object_labels pdf_object_labels;
typedef struct pdf_object_label_node pdf_object_label_node;

struct pdf_object_label_node
{
	int num;
	char *path;
	pdf_object_label_node *next;
};

struct pdf_object_labels
{
	fz_pool *pool;
	int object_count;
	int root, info, encrypt;
	unsigned short *pages;
	char *seen;
	pdf_object_label_node **nodes;
};

static void
add_object_label(fz_context *ctx, pdf_object_labels *g, char *path, int a, int b)
{
	pdf_object_label_node *node, **root;

	if (a < 0 || a >= g->object_count)
		return;

	node = fz_pool_alloc(ctx, g->pool, sizeof(pdf_object_label_node));
	node->path = fz_pool_strdup(ctx, g->pool, path);
	node->num = b;

	root = &g->nodes[a];
	node->next = *root;
	*root = node;
}

static void
scan_object_label_rec(fz_context *ctx, pdf_object_labels *g, char *root_path, pdf_obj *obj, int top)
{
	char path[100];
	int i, n;
	if (pdf_is_indirect(ctx, obj))
		;
	else if (pdf_is_dict(ctx, obj))
	{
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; ++i)
		{
			pdf_obj *key = pdf_dict_get_key(ctx, obj, i);
			pdf_obj *val = pdf_dict_get_val(ctx, obj, i);
			if (val && key != PDF_NAME(Parent) && key != PDF_NAME(P) && key != PDF_NAME(Prev) && key != PDF_NAME(Last))
			{
				if (pdf_is_indirect(ctx, val))
				{
					fz_snprintf(path, sizeof path, "%s/%s", root_path, pdf_to_name(ctx, key));
					add_object_label(ctx, g, path, pdf_to_num(ctx, val), top);
				}
				else if (pdf_is_dict(ctx, val) || pdf_is_array(ctx, val))
				{
					fz_snprintf(path, sizeof path, "%s/%s", root_path, pdf_to_name(ctx, key));
					scan_object_label_rec(ctx, g, path, val, top);
				}
			}
		}
	}
	else if (pdf_is_array(ctx, obj))
	{
		n = pdf_array_len(ctx, obj);
		for (i = 0; i < n; ++i)
		{
			pdf_obj *val = pdf_array_get(ctx, obj, i);
			if (val)
			{
				if (pdf_is_indirect(ctx, val))
				{
					fz_snprintf(path, sizeof path, "%s/%d", root_path, i+1);
					add_object_label(ctx, g, path, pdf_to_num(ctx, val), top);
				}
				else if (pdf_is_dict(ctx, val) || pdf_is_array(ctx, val))
				{
					fz_snprintf(path, sizeof path, "%s/%d", root_path, i+1);
					scan_object_label_rec(ctx, g, path, val, top);
				}
			}
		}
	}
}

static void
scan_object_label(fz_context *ctx, pdf_document *doc, pdf_object_labels *g, int num)
{
	pdf_obj *obj = pdf_load_object(ctx, doc, num);
	fz_try(ctx)
		scan_object_label_rec(ctx, g, "", obj, num);
	fz_always(ctx)
		pdf_drop_obj(ctx, obj);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

pdf_object_labels *
pdf_load_object_labels(fz_context *ctx, pdf_document *doc)
{
	pdf_object_labels *g = NULL;
	fz_pool *pool;
	int i, n, page_count;

	n = pdf_count_objects(ctx, doc);

	pool = fz_new_pool(ctx);
	fz_try(ctx)
	{
		g = fz_pool_alloc(ctx, pool, sizeof(pdf_object_labels));
		g->pool = pool;
		g->object_count = n;
		g->root = pdf_to_num(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root)));
		g->info = pdf_to_num(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Info)));
		g->encrypt = pdf_to_num(ctx, pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Encrypt)));
		g->seen = fz_pool_alloc(ctx, pool, n);
		g->nodes = fz_pool_alloc(ctx, pool, g->object_count * sizeof(pdf_object_label_node*));
		g->pages = fz_pool_alloc(ctx, pool, g->object_count * sizeof(unsigned short));

		page_count = pdf_count_pages(ctx, doc);
		for (i = 0; i < page_count; ++i)
			g->pages[pdf_to_num(ctx, pdf_lookup_page_obj(ctx, doc, i))] = i+1;

		for (i = 1; i < n; ++i)
			scan_object_label(ctx, doc, g, i);
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, pool);
	}
	return g;
}

void
pdf_drop_object_labels(fz_context *ctx, pdf_object_labels *g)
{
	if (g)
		fz_drop_pool(ctx, g->pool);
}

static char *
prepend(char *path_buffer, char *path, const char *fmt, ...)
{
	char buf[256];
	size_t z;
	va_list args;

	va_start(args, fmt);
	z = fz_vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	/* We always want to leave ourselves at least 3 chars for
	 * a future "..." */
	if (path_buffer + z + 3 <= path)
	{
		path -= z;
		memcpy(path, buf, z);
		return path;
	}

	/* Just put ... in now. */
	path -= 3;
	path[0] = '.';
	path[1] = '.';
	path[2] = '.';

	return path;
}

static void
find_paths(fz_context *ctx, pdf_object_labels *g, int here, char *path_buffer, char *leaf_path, pdf_label_object_fn *callback, void *arg)
{
	pdf_object_label_node *node;
	int next;
	if (here == g->root)
	{
		prepend(path_buffer, leaf_path, "trailer/Root");
		callback(ctx, arg, prepend(path_buffer, leaf_path, "trailer/Root"));
		return;
	}
	if (here == g->info)
	{
		callback(ctx, arg, prepend(path_buffer, leaf_path, "trailer/Info"));
		return;
	}
	if (here == g->encrypt)
	{
		callback(ctx, arg, prepend(path_buffer, leaf_path, "trailer/Encrypt"));
		return;
	}
	if (g->pages[here])
	{
		callback(ctx, arg, prepend(path_buffer, leaf_path, "pages/%d", g->pages[here]));
	}
	for (node = g->nodes[here]; node; node = node->next)
	{
		next = node->num;
		if (next < 1 || next >= g->object_count)
			continue;
		if (g->seen[next])
			continue;
		if (g->pages[next])
		{
			callback(ctx, arg, prepend(path_buffer, leaf_path, "pages/%d%s", g->pages[next], node->path));
		}
		else
		{
			char *p = prepend(path_buffer, leaf_path, "%s", node->path);
			g->seen[next] = 1;
			// if we've run out of room in the path buffer, send this and stop.
			if (p[0] == '.' && p[1] == '.' && p[2] == '.')
				callback(ctx, arg, p);
			else
				find_paths(ctx, g, next, path_buffer, p, callback, arg);
			g->seen[next] = 0;
		}
	}
}

void
pdf_label_object(fz_context *ctx, pdf_object_labels *g, int num, pdf_label_object_fn *callback, void *arg)
{
	int i;
	char path[4096];

	if (num < 1 || num >= g->object_count)
		return;
	for (i = 1; i < g->object_count; ++i)
		g->seen[i] = 0;
	path[sizeof(path)-1] = 0;
	find_paths(ctx, g, num, path, &path[sizeof(path)-1], callback, arg);
}
