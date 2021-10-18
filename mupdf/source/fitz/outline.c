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

#include "mupdf/fitz.h"

fz_outline_item *fz_outline_iterator_item(fz_context *ctx, fz_outline_iterator *iter)
{
	if (iter->item == NULL)
		return NULL;
	return iter->item(ctx, iter);
}

int fz_outline_iterator_next(fz_context *ctx, fz_outline_iterator *iter)
{
	if (iter->next == NULL)
		return -1;
	return iter->next(ctx, iter);
}

int fz_outline_iterator_prev(fz_context *ctx, fz_outline_iterator *iter)
{
	if (iter->prev == NULL)
		return -1;
	return iter->prev(ctx, iter);
}

int fz_outline_iterator_up(fz_context *ctx, fz_outline_iterator *iter)
{
	if (iter->up == NULL)
		return -1;
	return iter->up(ctx, iter);
}

int fz_outline_iterator_down(fz_context *ctx, fz_outline_iterator *iter)
{
	if (iter->down == NULL)
		return -1;
	return iter->down(ctx, iter);
}

int fz_outline_iterator_insert(fz_context *ctx, fz_outline_iterator *iter, fz_outline_item *item)
{
	if (iter->insert == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Document type does not support Outline editing");
	return iter->insert(ctx, iter, item);
}

int fz_outline_iterator_delete(fz_context *ctx, fz_outline_iterator *iter)
{
	if (iter->del == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Document type does not support Outline editing");
	return iter->del(ctx, iter);
}

void fz_outline_iterator_update(fz_context *ctx, fz_outline_iterator *iter, fz_outline_item *item)
{
	if (iter->update == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Document type does not support Outline editing");
	iter->update(ctx, iter, item);
}

void fz_drop_outline_iterator(fz_context *ctx, fz_outline_iterator *iter)
{
	if (iter == NULL)
		return;
	iter->drop(ctx, iter);
	fz_drop_document(ctx, iter->doc);
	fz_free(ctx, iter);
}

static void
load_outline_sub(fz_context *ctx, fz_outline_iterator *iter, fz_outline **tail, char **t, char **u)
{
	fz_outline_item *item;
	fz_outline *node, *onode;
	int res;

	do {
		item = fz_outline_iterator_item(ctx, iter);
		if (item == NULL)
			return;
		/* Duplicate title and uri first so we can recurse with limited try/catch. */
		*t = item->title == NULL ? NULL : fz_strdup(ctx, item->title);
		*u = item->uri == NULL ? NULL : fz_strdup(ctx, item->uri);
		node = fz_malloc_struct(ctx, fz_outline);
		node->is_open = item->is_open;
		node->refs = 1;
		node->title = *t;
		node->uri = *u;
		*t = NULL;
		*u = NULL;
		node->page = fz_resolve_link(ctx, iter->doc, node->uri, &node->x, &node->y);
		*tail = node;
		tail = &node->next;
		onode = node;
		node = NULL;

		res = fz_outline_iterator_down(ctx, iter);
		if (res == 0)
			load_outline_sub(ctx, iter, &onode->down, t, u);
		if (res >= 0)
			fz_outline_iterator_up(ctx, iter);
	}
	while (fz_outline_iterator_next(ctx, iter) == 0);
}

fz_outline *
fz_load_outline_from_iterator(fz_context *ctx, fz_outline_iterator *iter)
{
	fz_outline *head = NULL;
	fz_outline **tail = &head;
	char *title = NULL;
	char *uri = NULL;

	if (iter == NULL)
		return NULL;

	fz_try(ctx)
		load_outline_sub(ctx, iter, tail, &title, &uri);
	fz_always(ctx)
		fz_drop_outline_iterator(ctx, iter);
	fz_catch(ctx)
	{
		fz_free(ctx, title);
		fz_free(ctx, uri);
		fz_rethrow(ctx);
	}

	return head;
}

fz_outline_iterator *fz_new_outline_iterator_of_size(fz_context *ctx, size_t size, fz_document *doc)
{
	fz_outline_iterator *iter = fz_calloc(ctx, size, 1);

	iter->doc = fz_keep_document(ctx, doc);

	return iter;
}

fz_outline *
fz_new_outline(fz_context *ctx)
{
	fz_outline *outline = fz_malloc_struct(ctx, fz_outline);
	outline->refs = 1;
	return outline;
}

fz_outline *
fz_keep_outline(fz_context *ctx, fz_outline *outline)
{
	return fz_keep_imp(ctx, outline, &outline->refs);
}

void
fz_drop_outline(fz_context *ctx, fz_outline *outline)
{
	while (fz_drop_imp(ctx, outline, &outline->refs))
	{
		fz_outline *next = outline->next;
		fz_drop_outline(ctx, outline->down);
		fz_free(ctx, outline->title);
		fz_free(ctx, outline->uri);
		fz_free(ctx, outline);
		outline = next;
	}
}

typedef struct {
	fz_outline_iterator super;
	fz_outline *outline;
	fz_outline *current;
	fz_outline_item item;
	int down_max;
	int down_len;
	fz_outline **down_array;
} fz_outline_iter_std;

static int
iter_std_down(fz_context *ctx, fz_outline_iterator *iter_)
{
	fz_outline_iter_std *iter = (fz_outline_iter_std *)iter_;

	if (iter->current == NULL)
		return -1;
	if (iter->current->down == NULL)
		return -1;

	if (iter->down_max == iter->down_len)
	{
		int new_max = iter->down_max ? iter->down_max * 2 : 32;
		iter->down_array = fz_realloc_array(ctx, iter->down_array, new_max, fz_outline *);
		iter->down_max = new_max;
	}
	iter->down_array[iter->down_len++] = iter->current;

	iter->current = iter->current->down;
	return 0;
}

static int
iter_std_up(fz_context *ctx, fz_outline_iterator *iter_)
{
	fz_outline_iter_std *iter = (fz_outline_iter_std *)iter_;

	if (iter->current == NULL)
		return -1;
	if (iter->down_len == 0)
		return -1;

	iter->current = iter->down_array[--iter->down_len];

	return 0;
}

static int
iter_std_next(fz_context *ctx, fz_outline_iterator *iter_)
{
	fz_outline_iter_std *iter = (fz_outline_iter_std *)iter_;

	if (iter->current == NULL)
		return -1;
	if (iter->current->next == NULL)
		return -1;

	iter->current = iter->current->next;

	return 0;
}

static int
iter_std_prev(fz_context *ctx, fz_outline_iterator *iter_)
{
	fz_outline_iter_std *iter = (fz_outline_iter_std *)iter_;
	fz_outline *first;

	if (iter->current == NULL)
		return -1;
	first = iter->down_len == 0 ? iter->outline : iter->down_array[iter->down_len-1];
	if (iter->current == first)
		return -1;

	while (first->next != iter->current)
		first = first->next;

	iter->current = first;

	return 0;
}

static fz_outline_item *
iter_std_item(fz_context *ctx, fz_outline_iterator *iter_)
{
	fz_outline_iter_std *iter = (fz_outline_iter_std *)iter_;

	if (iter->current == NULL)
		return NULL;

	iter->item.is_open = iter->current->is_open;
	iter->item.title = iter->current->title;
	iter->item.uri = iter->current->uri;

	return &iter->item;
}

static void
iter_std_drop(fz_context *ctx, fz_outline_iterator *iter_)
{
	fz_outline_iter_std *iter = (fz_outline_iter_std *)iter_;

	if (iter == NULL)
		return;

	fz_drop_outline(ctx, iter->outline);
	fz_free(ctx, iter->down_array);
}

fz_outline_iterator *fz_outline_iterator_from_outline(fz_context *ctx, fz_outline *outline)
{
	fz_outline_iter_std *iter;

	fz_try(ctx)
	{
		iter = fz_malloc_struct(ctx, fz_outline_iter_std);
		iter->super.down = iter_std_down;
		iter->super.up = iter_std_up;
		iter->super.next = iter_std_next;
		iter->super.prev = iter_std_prev;
		iter->super.item = iter_std_item;
		iter->super.drop = iter_std_drop;
		iter->outline = outline;
		iter->current = outline;
	}
	fz_catch(ctx)
	{
		fz_drop_outline(ctx, outline);
		fz_rethrow(ctx);
	}

	return &iter->super;
}
