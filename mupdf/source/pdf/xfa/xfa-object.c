// Copyright (C) 2004-2026 Artifex Software, Inc.
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

#include "xfa-imp.h"

#include <string.h>

static int pdf_xfa_uid;

pdf_xfa_object *
pdf_xfa_new_object(fz_context *ctx, fz_pool *pool, pdf_xfa_ns_id ns, const char *name, int has_children)
{
	pdf_xfa_object *node = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_object));
	char uidbuf[64];

	memset(node, 0, sizeof(*node));
	node->ns = ns;
	node->name = fz_pool_strdup(ctx, pool, name);
	node->has_children = has_children;
	node->is_data_value = -1;

	fz_snprintf(uidbuf, sizeof uidbuf, "%s%d", name, pdf_xfa_uid++);
	node->uid = fz_pool_strdup(ctx, pool, uidbuf);

	return node;
}

void
pdf_xfa_object_add_attr(fz_context *ctx, fz_pool *pool, pdf_xfa_object *node, const char *name, const char *value)
{
	pdf_xfa_attr *attr = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_attr));
	const char *colon;
	pdf_xfa_attr *scan;

	attr->name = fz_pool_strdup(ctx, pool, name);
	attr->value = fz_pool_strdup(ctx, pool, value ? value : "");
	attr->next = node->first_attr;
	node->first_attr = attr;

	if (strcmp(name, "id") == 0)
		node->template_id = attr->value;

	colon = strchr(name, ':');
	if (colon && strcmp(colon + 1, "dataNode") == 0)
	{
		if (strcmp(attr->value, "dataValue") == 0)
			node->is_data_value = 1;
		else if (strcmp(attr->value, "dataGroup") == 0)
			node->is_data_value = 0;
	}

	/* Keep attributes in stable order for dumps */
	if (attr->next)
	{
		pdf_xfa_attr *prev = NULL;
		for (scan = node->first_attr; scan && scan != attr; scan = scan->next)
			prev = scan;
		if (prev)
		{
			prev->next = attr->next;
			attr->next = node->first_attr;
			node->first_attr = attr;
		}
	}
}

void
pdf_xfa_object_append_child(fz_context *ctx, fz_pool *pool, pdf_xfa_object *parent, pdf_xfa_object *child)
{
	pdf_xfa_object *last;

	(void)pool;

	child->parent = parent;
	if (!parent->first_child)
	{
		parent->first_child = child;
		return;
	}

	for (last = parent->first_child; last->next_sibling; last = last->next_sibling)
		;
	last->next_sibling = child;

	if (!child->global && parent->global)
		child->global = parent->global;
}

void
pdf_xfa_object_set_id(fz_context *ctx, fz_hash_table *ids, pdf_xfa_object *node)
{
	if (node->template_id && node->ns == PDF_XFA_NS_TEMPLATE && ids)
		fz_hash_insert(ctx, ids, node->template_id, node);
}

void
pdf_xfa_object_finalize(fz_context *ctx, fz_pool *pool, pdf_xfa_object *node)
{
	pdf_xfa_object *text_child;

	(void)ctx;

	if (!node->content || !node->first_child)
		return;

	text_child = pdf_xfa_new_object(ctx, pool, node->ns, "#text", 0);
	text_child->content = node->content;
	node->content = NULL;
	pdf_xfa_object_append_child(ctx, pool, node, text_child);
}