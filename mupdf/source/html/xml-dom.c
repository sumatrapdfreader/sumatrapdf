// Copyright (C) 2022 Artifex Software, Inc.
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

#include "html-imp.h"

#include "string.h"

fz_xml *fz_story_document(fz_context *ctx, fz_story *story)
{
	if (story == NULL || story->dom == NULL)
		return NULL;

	return story->dom;
}

fz_xml *fz_dom_body(fz_context *ctx, fz_xml *dom)
{
	if (dom == NULL)
		return NULL;

	return fz_xml_find_dfs(dom, "body", NULL, NULL);
}

fz_xml *fz_dom_document_element(fz_context *ctx, fz_xml *dom)
{
	if (dom == NULL)
		return NULL;

	while (dom->up)
		dom = dom->up;

	return dom->down;
}

static fz_xml *
doc_pointer(fz_xml *a)
{
	while (a->up)
		a = a->up;

	return a;
}

static void
check_same_doc(fz_context *ctx, fz_xml *a, fz_xml *b)
{
	/* Sanity check: The child and parent must come from the same doc. */
	if (doc_pointer(a) != doc_pointer(b))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Parent and child must be from the same document");
}

/* Helper function to skip forward if we are passed a
 * doc pointer in circumstances where we should not be. */
static fz_xml *
skip_doc_pointer(fz_xml *x)
{
	return (x == NULL || !FZ_DOCUMENT_ITEM(x)) ? x : x->down;
}

static fz_xml *
new_xml_node(fz_context *ctx, fz_xml *dom, const char *tag)
{
	const char *ns;
	fz_xml *xml;
	size_t size;

	dom = doc_pointer(dom);

	/* skip namespace prefix */
	for (ns = tag; *ns; ++ns)
		if (*ns == ':')
			tag = ns + 1;

	size = offsetof(fz_xml, u.node.u.d.name) + ns-tag+1;

	xml = fz_pool_alloc(ctx, dom->u.doc.pool, size);

	memcpy(xml->u.node.u.d.name, tag, ns-tag+1);
	xml->u.node.u.d.atts = NULL;
	xml->down = NULL;
	xml->up = dom;
	xml->u.node.next = NULL;
	xml->u.node.prev = NULL;
#ifdef FZ_XML_SEQ
	/* We don't have sequence numbers here. */
	xml->seq = 0;
#endif

	return xml;
}

static fz_xml *
new_xml_text_node(fz_context *ctx, fz_xml *dom, const char *text)
{
	fz_xml *xml;
	size_t len = text ? strlen(text) : 0;
	size_t size;

	dom = doc_pointer(dom);

	size = offsetof(fz_xml, u.node.u.text) + len + 1;

	xml = fz_pool_alloc(ctx, dom->u.doc.pool, size);

	if (text)
		memcpy(xml->u.node.u.text, text, len);
	xml->u.node.u.text[len] = 0;
	xml->down = MAGIC_TEXT;
	xml->up = dom;
	xml->u.node.next = NULL;
	xml->u.node.prev = NULL;
#ifdef FZ_XML_SEQ
	/* We don't have sequence numbers here. */
	xml->u.node.seq = 0;
#endif

	return xml;
}

static fz_xml *
clone_xml(fz_context *ctx, fz_xml *dom, fz_xml *node)
{
	fz_xml *clone;
	struct attribute **dst;
	struct attribute *attr;
	fz_xml *child, *prev;

	if (dom == NULL || node == NULL)
		return NULL;

	/* Text nodes are simple. No children. */
	if (FZ_TEXT_ITEM(node))
	{
		return new_xml_text_node(ctx, dom, node->u.node.u.text);
	}

	/* Clone a non-text node. */
	clone = new_xml_node(ctx, dom, node->u.node.u.d.name);

	/* Clone the attributes. */
	attr = node->u.node.u.d.atts;
	dst = &clone->u.node.u.d.atts;
	while (attr)
	{
		size_t len = strlen(attr->name) + 1;
		size_t size = offsetof(struct attribute, name) + len;
		struct attribute *a = fz_pool_alloc(ctx, dom->u.doc.pool, size);
		memcpy(a->name, attr->name, len);
		a->next = NULL;
		a->value = NULL;
		if (attr->value)
		{
			a->value = fz_pool_alloc(ctx, dom->u.doc.pool, strlen(attr->value)+1);
			strcpy(a->value, attr->value);
		}
		*dst = a;
		dst = &a->next;
		attr = attr->next;
	}

	/* If we have no children, we're done. */
	if (node->down == NULL)
		return clone;

	/* Copy the first child. */
	clone->down = clone_xml(ctx, dom, node->down);
	clone->down->up = clone;

	/* And then run along all the successive children. */
	prev = clone->down;
	child = node->down->u.node.next;
	while (child)
	{
		prev->u.node.next = clone_xml(ctx, dom, child);
		prev->u.node.prev = prev;
		prev = prev->u.node.next;
		prev->up = clone;
		child = child->u.node.next;
	}

	return clone;
}

fz_xml *fz_dom_clone(fz_context *ctx, fz_xml *elt)
{
	fz_xml *dom;

	if (elt == NULL)
		return NULL;

	/* We shouldn't be passed a document item really, but
	 * cope. */
	if (FZ_DOCUMENT_ITEM(elt))
		elt = elt->down;

	/* Find the document pointer. */
	dom = elt;
	while (dom->up)
		dom = dom->up;

	return clone_xml(ctx, dom, elt);
}

fz_xml *fz_dom_create_element(fz_context *ctx, fz_xml *dom, const char *tag)
{
	if (dom == NULL || tag == NULL)
		return NULL;

	/* We make a new node, unconnected to anything else.
	 * up will stil point to the dom root though. */
	return new_xml_node(ctx, dom, tag);
}

fz_xml *fz_dom_create_text_node(fz_context *ctx, fz_xml *dom, const char *text)
{
	if (dom == NULL || text == NULL)
		return NULL;

	/* We make a new node, unconnected to anything else. */
	return new_xml_text_node(ctx, dom, text);
}

fz_xml *fz_dom_find(fz_context *ctx, fz_xml *elt, const char *tag, const char *att, const char *match)
{
	if (elt == NULL)
		return NULL;

	return fz_xml_find_dfs(elt, tag, att, match);
}

fz_xml *fz_dom_find_next(fz_context *ctx, fz_xml *elt, const char *tag, const char *att, const char *match)
{
	if (elt == NULL)
		return NULL;

	return fz_xml_find_next_dfs(elt, tag, att, match);
}

void fz_dom_append_child(fz_context *ctx, fz_xml *parent, fz_xml *child)
{
	fz_xml *x;

	child = skip_doc_pointer(child);
	parent = skip_doc_pointer(parent);

	if (parent == NULL || child == NULL)
		return;

	check_same_doc(ctx, parent, child);

	/* Sanity checks: We can't add child to parent if parent is
	 * a child of child. */
	x = parent;
	while (x)
	{
		if (x == child)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Can't add a parent to its child.");
		x = x->up;
	}

	/* First unlink child from anywhere it's currently linked in. */
	if (child->u.node.prev)
		child->u.node.prev->u.node.next = child->u.node.next;
	else if (child->up->down == child && !FZ_DOCUMENT_ITEM(child->up))
		child->up->down = child->u.node.next;
	if (child->u.node.next)
		child->u.node.next->u.node.prev = child->u.node.prev;
	child->u.node.next = NULL;
	child->u.node.prev = NULL;

	/* Now find where to insert the child. */
	if (parent->down == NULL)
	{
		/* Insert as first (and only) child. */
		parent->down = child;
	}
	else
	{
		/* Find x, the current last child. */
		x = parent->down;
		while (x->u.node.next)
			x = x->u.node.next;

		/* And insert xchild after that. */
		x->u.node.next = child;
		child->u.node.prev = x;
	}
	child->up = parent;
}

void fz_dom_insert_before(fz_context *ctx, fz_xml *existing, fz_xml *elt)
{
	fz_xml *x;

	existing = skip_doc_pointer(existing);
	elt = skip_doc_pointer(elt);

	if (existing == NULL || elt == NULL)
		return;

	check_same_doc(ctx, existing, elt);

	/* Sanity check: We can't add elt before existing if existing is
	 * a child of elt. */
	x = existing;
	while (x)
	{
		if (x == elt)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Can't add a node before its child.");
		x = x->up;
	}

	/* First unlink elt from anywhere it's currently linked in. */
	if (elt->u.node.prev)
		elt->u.node.prev->u.node.next = elt->u.node.next;
	else if (elt->up && !FZ_DOCUMENT_ITEM(elt->up))
		elt->up->down = elt->u.node.next;
	if (elt->u.node.next)
		elt->u.node.next->u.node.prev = elt->u.node.prev;
	elt->u.node.next = NULL;
	elt->u.node.prev = NULL;
	elt->up = NULL;

	/* Now insert the element */
	elt->u.node.prev = existing->u.node.prev;
	if (elt->u.node.prev)
		elt->u.node.prev->u.node.next = elt;
	else if (existing->up && !FZ_DOCUMENT_ITEM(existing->up))
		existing->up->down = elt;
	elt->u.node.next = existing;
	existing->u.node.prev = elt;
	elt->up = existing->up;
}

void fz_dom_insert_after(fz_context *ctx, fz_xml *existing, fz_xml *elt)
{
	fz_xml *x;

	existing = skip_doc_pointer(existing);
	elt = skip_doc_pointer(elt);

	if (existing == NULL || elt == NULL)
		return;

	check_same_doc(ctx, existing, elt);

	/* Sanity check: We can't add elt before existing if existing is
	 * a child of elt. */
	x = existing;
	while (x)
	{
		if (x == elt)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Can't add a node after its child.");
		x = x->up;
	}

	/* First unlink child from anywhere it's currently linked in. */
	if (elt->u.node.prev)
		elt->u.node.prev->u.node.next = elt->u.node.next;
	else if (elt->up && !FZ_DOCUMENT_ITEM(elt->up))
		elt->up->down = elt->u.node.next;
	if (elt->u.node.next)
		elt->u.node.next->u.node.prev = elt->u.node.prev;
	elt->u.node.next = NULL;
	elt->u.node.prev = NULL;

	/* Now insert the element */
	elt->u.node.next = existing->u.node.next;
	if (elt->u.node.next)
		elt->u.node.next->u.node.prev = elt;
	elt->u.node.prev = existing;
	existing->u.node.next = elt;
	elt->up = existing->up;
}

void fz_dom_remove(fz_context *ctx, fz_xml *elt)
{
	elt = skip_doc_pointer(elt);

	if (elt == NULL)
		return;

	/* Unlink child from anywhere it's currently linked in. */
	if (elt->u.node.prev)
		elt->u.node.prev->u.node.next = elt->u.node.next;
	else if (elt->up && !FZ_DOCUMENT_ITEM(elt))
		elt->up->down = elt->u.node.next;
	if (elt->u.node.next)
		elt->u.node.next->u.node.prev = elt->u.node.prev;
	elt->u.node.next = NULL;
	elt->u.node.prev = NULL;
	elt->up = doc_pointer(elt);
}

fz_xml *fz_dom_first_child(fz_context *ctx, fz_xml *elt)
{
	elt = skip_doc_pointer(elt);

	if (elt == NULL || FZ_TEXT_ITEM(elt))
		return NULL;

	return elt->down;
}

fz_xml *fz_dom_parent(fz_context *ctx, fz_xml *elt)
{
	elt = skip_doc_pointer(elt);

	if (elt == NULL)
		return NULL;

	if (FZ_DOCUMENT_ITEM(elt->up))
		return NULL;

	return elt->up;
}

fz_xml *fz_dom_next(fz_context *ctx, fz_xml *elt)
{
	elt = skip_doc_pointer(elt);

	if (elt == NULL)
		return NULL;

	return elt->u.node.next;
}

fz_xml *fz_dom_previous(fz_context *ctx, fz_xml *elt)
{
	elt = skip_doc_pointer(elt);

	if (elt == NULL)
		return NULL;

	return elt->u.node.prev;
}

void fz_dom_add_attribute(fz_context *ctx, fz_xml *elt, const char *att, const char *value)
{
	struct attribute *attr;
	size_t len, size;
	char *mvalue = NULL;
	fz_xml *doc;

	elt = skip_doc_pointer(elt);

	if (elt == NULL || att == NULL)
		return;

	if (FZ_TEXT_ITEM(elt))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot add attributes to text node.");

	/* Move value to being a malloced thing, with the entity parsing done. */
	if (value) {
		char *d;
		const char *s = value;
		d = mvalue = fz_malloc(ctx, strlen(value)+1);

		while (*s)
		{
			if (*s == '&') {
				int c;
				s += xml_parse_entity(&c, s);
				d += fz_runetochar(d, c);
			}
			else
				*d++ = *s++;
		}
		*d = 0;
	}

	/* Do we have an attribute we can reuse? */
	attr = elt->u.node.u.d.atts;
	while (attr)
	{
		if (strcmp(att, attr->name) == 0)
		{
			/* Reuse this one. */
			break;
		}
		attr = attr->next;
	}

	if (attr && attr->value)
	{
		if (mvalue == NULL)
		{
			/* Just rewrite the existing value to be NULL. This
			 * 'leaks' the old value within the pool, so it will
			 * be cleaned up at the end. */
			attr->value = NULL;
			return;
		}
		if (strcmp(mvalue, attr->value) == 0)
		{
			/* Old and new values match. Nothing to change. */
			return;
		}
	}

	doc = doc_pointer(elt);
	/* Move mvalue to be an fz_pool thing. */
	if (mvalue)
	{
		char *tmp;
		fz_try(ctx)
		{
			tmp = fz_pool_alloc(ctx, doc->u.doc.pool, strlen(mvalue)+1);
			strcpy(tmp, mvalue);
		}
		fz_always(ctx)
			fz_free(ctx, mvalue);
		fz_catch(ctx)
			fz_rethrow(ctx);
		mvalue = tmp;
	}

	/* Make a new one and prepend it. */
	len = strlen(att) + 1;
	size = offsetof(struct attribute, name) + len;
	attr = fz_pool_alloc(ctx, doc->u.doc.pool, size);
	memcpy(attr->name, att, len);
	attr->next = elt->u.node.u.d.atts;
	elt->u.node.u.d.atts = attr;
	attr->value = mvalue;
}

void fz_dom_remove_attribute(fz_context *ctx, fz_xml *elt, const char *att)
{
	struct attribute **attr;

	elt = skip_doc_pointer(elt);

	if (elt == NULL || att == NULL)
		return;

	if (FZ_TEXT_ITEM(elt))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot add attributes to text node.");

	attr = &elt->u.node.u.d.atts;
	while (*attr)
	{
		if (strcmp(att, (*attr)->name) == 0)
		{
			/* Delete this one. */
			/* The old attr/value are 'leaked' within the pool. */
			*attr = (*attr)->next;
			break;
		}
		attr = &(*attr)->next;
	}
}

const char *fz_dom_attribute(fz_context *ctx, fz_xml *elt, const char *att)
{
	struct attribute *attr;

	elt = skip_doc_pointer(elt);

	if (elt == NULL || att == NULL)
		return NULL;

	/* Text nodes don't have attributes. */
	if (FZ_TEXT_ITEM(elt))
		return NULL;

	attr = elt->u.node.u.d.atts;
	while (attr)
	{
		if (strcmp(att, attr->name) == 0)
		{
			/* Found! */
			return attr->value;
		}
	}
	return NULL;
}

const char *fz_dom_get_attribute(fz_context *ctx, fz_xml *elt, int i, const char **att)
{
	struct attribute *attr;

	if (elt == NULL || att == NULL)
	{
		if (att)
			*att = NULL;
		return NULL;
	}

	/* Text nodes don't have attributes. */
	if (FZ_TEXT_ITEM(elt) || i < 0)
	{
		*att = NULL;
		return NULL;
	}

	attr = elt->u.node.u.d.atts;
	while (attr)
	{
		if (i == 0)
		{
			/* Found! */
			*att = attr->name;
			return attr->value;
		}
		i--;
		attr = attr->next;
	}

	*att = NULL;
	return NULL;
}
