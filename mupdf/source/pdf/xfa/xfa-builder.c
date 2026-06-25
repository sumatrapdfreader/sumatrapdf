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

typedef struct
{
	pdf_xfa_ns_id *stack;
	int len;
	int cap;
} pdf_xfa_prefix_stack;

static void
pdf_xfa_prefix_stack_push(fz_context *ctx, fz_pool *pool, pdf_xfa_prefix_stack *ps, pdf_xfa_ns_id ns)
{
	if (ps->len == ps->cap)
	{
		int newcap = ps->cap ? ps->cap * 2 : 4;
		pdf_xfa_ns_id *newstack = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_ns_id) * newcap);
		if (ps->stack)
			memcpy(newstack, ps->stack, sizeof(pdf_xfa_ns_id) * ps->len);
		ps->stack = newstack;
		ps->cap = newcap;
	}
	ps->stack[ps->len++] = ns;
}

static void
pdf_xfa_prefix_stack_pop(pdf_xfa_prefix_stack *ps)
{
	if (ps->len > 0)
		ps->len--;
}

static pdf_xfa_ns_id
pdf_xfa_prefix_stack_top(pdf_xfa_prefix_stack *ps)
{
	if (ps->len == 0)
		return PDF_XFA_NS_UNKNOWN;
	return ps->stack[ps->len - 1];
}

static void
pdf_xfa_prefix_stack_drop(fz_context *ctx, void *val)
{
	(void)ctx;
	(void)val;
}

pdf_xfa_builder *
pdf_xfa_builder_new(fz_context *ctx, fz_pool *pool)
{
	pdf_xfa_builder *builder = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_builder));

	builder->current_ns = PDF_XFA_NS_UNKNOWN;
	builder->next_unknown_ns = PDF_XFA_NS_COUNT;
	builder->namespace_stack_cap = 8;
	builder->namespace_stack = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_ns_id) * builder->namespace_stack_cap);
	builder->prefix_stacks = fz_new_hash_table(ctx, 16, FZ_HASH_TABLE_KEY_LENGTH, -1, pdf_xfa_prefix_stack_drop);
	builder->uri_namespaces = fz_new_hash_table(ctx, 16, FZ_HASH_TABLE_KEY_LENGTH, -1, NULL);

	return builder;
}

void
pdf_xfa_builder_drop(fz_context *ctx, pdf_xfa_builder *builder)
{
	if (!builder)
		return;
	fz_drop_hash_table(ctx, builder->prefix_stacks);
	fz_drop_hash_table(ctx, builder->uri_namespaces);
}

pdf_xfa_object *
pdf_xfa_builder_build_root(fz_context *ctx, fz_pool *pool, fz_hash_table *ids)
{
	pdf_xfa_object *root = pdf_xfa_new_object(ctx, pool, PDF_XFA_NS_UNKNOWN, "root", 1);
	(void)ids;
	return root;
}

static pdf_xfa_ns_id
pdf_xfa_builder_get_ns_for_prefix(fz_context *ctx, pdf_xfa_builder *builder, const char *prefix)
{
	pdf_xfa_prefix_stack *ps;

	if (!prefix || !*prefix)
		return builder->current_ns;

	ps = fz_hash_find(ctx, builder->prefix_stacks, prefix);
	if (ps && ps->len > 0)
		return pdf_xfa_prefix_stack_top(ps);

	return PDF_XFA_NS_UNKNOWN;
}

static int
pdf_xfa_is_xdp_child_ns(pdf_xfa_ns_id ns, const char *name)
{
	/* Xdp[$onChildCheck] from pdf.js */
	if (strcmp(name, "config") == 0) return ns == PDF_XFA_NS_CONFIG;
	if (strcmp(name, "connectionSet") == 0) return ns == PDF_XFA_NS_CONNECTION_SET;
	if (strcmp(name, "datasets") == 0) return ns == PDF_XFA_NS_DATASETS;
	if (strcmp(name, "localeSet") == 0) return ns == PDF_XFA_NS_LOCALE_SET;
	if (strcmp(name, "stylesheet") == 0) return ns == PDF_XFA_NS_STYLESHEET;
	if (strcmp(name, "template") == 0) return ns == PDF_XFA_NS_TEMPLATE;
	if (strcmp(name, "xdp") == 0) return ns == PDF_XFA_NS_XDP;
	return 1;
}

pdf_xfa_object *
pdf_xfa_builder_build(fz_context *ctx, fz_pool *pool, pdf_xfa_builder *builder,
		const char *ns_prefix, const char *name, pdf_xfa_attr *attrs,
		const char *xmlns, pdf_xfa_attr *prefixes)
{
	pdf_xfa_ns_id ns;
	pdf_xfa_object *node;
	pdf_xfa_attr *attr;
	int has_children = 1;

	if (xmlns)
	{
		if (builder->namespace_stack_len == builder->namespace_stack_cap)
		{
			int newcap = builder->namespace_stack_cap * 2;
			pdf_xfa_ns_id *newstack = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_ns_id) * newcap);
			memcpy(newstack, builder->namespace_stack, sizeof(pdf_xfa_ns_id) * builder->namespace_stack_len);
			builder->namespace_stack = newstack;
			builder->namespace_stack_cap = newcap;
		}
		builder->namespace_stack[builder->namespace_stack_len++] = builder->current_ns;
		builder->current_ns = pdf_xfa_ns_from_uri(ctx, builder, xmlns);
	}

	if (prefixes)
	{
		for (attr = prefixes; attr; attr = attr->next)
		{
			pdf_xfa_prefix_stack *ps = fz_hash_find(ctx, builder->prefix_stacks, attr->name);
			if (!ps)
			{
				ps = fz_pool_alloc(ctx, pool, sizeof(pdf_xfa_prefix_stack));
				fz_hash_insert(ctx, builder->prefix_stacks, attr->name, ps);
			}
			pdf_xfa_prefix_stack_push(ctx, pool, ps, pdf_xfa_ns_from_uri(ctx, builder, attr->value));
		}
	}

	ns = pdf_xfa_builder_get_ns_for_prefix(ctx, builder, ns_prefix);
	if (ns == PDF_XFA_NS_UNKNOWN)
		ns = builder->current_ns;

	/* Phase 1: generic nodes.  Typed builders (template.js et al.) plug in here. */
	if (!pdf_xfa_is_xdp_child_ns(ns, name))
		has_children = 0;

	node = pdf_xfa_new_object(ctx, pool, ns, name, has_children);

	for (attr = attrs; attr; attr = attr->next)
		pdf_xfa_object_add_attr(ctx, pool, node, attr->name, attr->value);

	return node;
}

void
pdf_xfa_builder_clean(fz_context *ctx, pdf_xfa_builder *builder, int has_namespace, pdf_xfa_attr *prefixes, int ns_agnostic)
{
	pdf_xfa_attr *attr;

	if (has_namespace && builder->namespace_stack_len > 0)
		builder->current_ns = builder->namespace_stack[--builder->namespace_stack_len];

	for (attr = prefixes; attr; attr = attr->next)
	{
		pdf_xfa_prefix_stack *ps = fz_hash_find(ctx, builder->prefix_stacks, attr->name);
		if (ps)
			pdf_xfa_prefix_stack_pop(ps);
	}

	if (ns_agnostic && builder->ns_agnostic_level > 0)
		builder->ns_agnostic_level--;
}

int
pdf_xfa_builder_is_ns_agnostic(fz_context *ctx, pdf_xfa_builder *builder)
{
	(void)ctx;
	return builder->ns_agnostic_level > 0;
}