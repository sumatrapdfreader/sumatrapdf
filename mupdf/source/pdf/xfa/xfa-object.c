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
pdf_xfa_ids_make_key(char *buf, const char *id)
{
	size_t len;

	memset(buf, 0, FZ_HASH_TABLE_KEY_LENGTH);
	if (!id)
		return;
	len = strlen(id);
	if (len >= FZ_HASH_TABLE_KEY_LENGTH)
		len = FZ_HASH_TABLE_KEY_LENGTH - 1;
	memcpy(buf, id, len);
}

void
pdf_xfa_object_set_id(fz_context *ctx, fz_hash_table *ids, pdf_xfa_object *node)
{
	char key[FZ_HASH_TABLE_KEY_LENGTH];

	if (node->template_id && node->ns == PDF_XFA_NS_TEMPLATE && ids)
	{
		pdf_xfa_ids_make_key(key, node->template_id);
		fz_hash_insert(ctx, ids, key, node);
	}
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

char *
pdf_xfa_object_get_attr(fz_context *ctx, pdf_xfa_object *node, const char *name)
{
	pdf_xfa_attr *attr;

	(void)ctx;
	if (!node || !name)
		return NULL;
	for (attr = node->first_attr; attr; attr = attr->next)
		if (strcmp(attr->name, name) == 0)
			return attr->value;
	return NULL;
}

int
pdf_xfa_object_is_data_value(pdf_xfa_object *node)
{
	pdf_xfa_object *child;

	if (!node)
		return 0;
	if (node->is_data_value == 1)
		return 1;
	if (node->is_data_value == 0)
		return 0;
	if (!node->first_child)
		return 1;
	child = node->first_child;
	if (!child->next_sibling && child->ns == PDF_XFA_NS_XHTML)
		return 1;
	return 0;
}

const char *
pdf_xfa_object_get_data_value(fz_context *ctx, pdf_xfa_object *node)
{
	if (!node)
		return "";
	if (!pdf_xfa_object_is_data_value(node))
		return NULL;
	if (!node->first_child)
		return node->content ? node->content : "";
	return pdf_xfa_object_text(ctx, node);
}

const char *
pdf_xfa_object_text(fz_context *ctx, pdf_xfa_object *node)
{
	pdf_xfa_object *child;
	fz_buffer *buf = NULL;
	const char *res = "";

	if (!node)
		return "";
	if (!node->first_child)
		return node->content ? node->content : "";

	fz_var(buf);
	fz_try(ctx)
	{
		buf = fz_new_buffer(ctx, 64);
		for (child = node->first_child; child; child = child->next_sibling)
		{
			const char *t = pdf_xfa_object_text(ctx, child);
			if (t && *t)
				fz_append_string(ctx, buf, t);
		}
		fz_terminate_buffer(ctx, buf);
		res = fz_string_from_buffer(ctx, buf);
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return res;
}

pdf_xfa_object *
pdf_xfa_object_find_child(pdf_xfa_object *node, const char *name)
{
	pdf_xfa_object *child;

	if (!node || !name)
		return NULL;
	for (child = node->first_child; child; child = child->next_sibling)
		if (child->name && strcmp(child->name, name) == 0)
			return child;
	return NULL;
}

static void
pdf_xfa_object_clone_attrs(fz_context *ctx, fz_pool *pool, pdf_xfa_object *dst, pdf_xfa_object *src)
{
	pdf_xfa_attr *attr;

	for (attr = src->first_attr; attr; attr = attr->next)
		pdf_xfa_object_add_attr(ctx, pool, dst, attr->name, attr->value);
}

pdf_xfa_object *
pdf_xfa_object_clone(fz_context *ctx, fz_pool *pool, pdf_xfa_object *node)
{
	pdf_xfa_object *clone, *child, *cchild, *last;

	if (!node)
		return NULL;

	clone = pdf_xfa_new_object(ctx, pool, node->ns, node->name, node->has_children);
	clone->content = node->content ? fz_pool_strdup(ctx, pool, node->content) : NULL;
	clone->is_data_value = node->is_data_value;
	clone->global = node->global;
	pdf_xfa_object_clone_attrs(ctx, pool, clone, node);

	last = NULL;
	for (child = node->first_child; child; child = child->next_sibling)
	{
		cchild = pdf_xfa_object_clone(ctx, pool, child);
		cchild->parent = clone;
		if (!clone->first_child)
			clone->first_child = cchild;
		else
			last->next_sibling = cchild;
		last = cchild;
	}

	return clone;
}