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

#include <string.h>

/* AA-tree */

struct fz_tree
{
	char *key;
	void *value;
	fz_tree *left, *right;
	int level;
};

static fz_tree tree_sentinel = { "", NULL, &tree_sentinel, &tree_sentinel, 0 };

static fz_tree *fz_tree_new_node(fz_context *ctx, const char *key, void *value)
{
	fz_tree *node = fz_malloc_struct(ctx, fz_tree);
	fz_try(ctx)
	{
		node->key = fz_strdup(ctx, key);
		node->value = value;
		node->left = node->right = &tree_sentinel;
		node->level = 1;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, node);
		fz_rethrow(ctx);
	}
	return node;
}

void *fz_tree_lookup(fz_context *ctx, fz_tree *node, const char *key)
{
	if (node)
	{
		while (node != &tree_sentinel)
		{
			int c = strcmp(key, node->key);
			if (c == 0)
				return node->value;
			else if (c < 0)
				node = node->left;
			else
				node = node->right;
		}
	}
	return NULL;
}

static fz_tree *fz_tree_skew(fz_tree *node)
{
	if (node->level != 0)
	{
		if (node->left->level == node->level)
		{
			fz_tree *save = node;
			node = node->left;
			save->left = node->right;
			node->right = save;
		}
		node->right = fz_tree_skew(node->right);
	}
	return node;
}

static fz_tree *fz_tree_split(fz_tree *node)
{
	if (node->level != 0 && node->right->right->level == node->level)
	{
		fz_tree *save = node;
		node = node->right;
		save->right = node->left;
		node->left = save;
		node->level++;
		node->right = fz_tree_split(node->right);
	}
	return node;
}

fz_tree *fz_tree_insert(fz_context *ctx, fz_tree *node, const char *key, void *value)
{
	if (node && node != &tree_sentinel)
	{
		int c = strcmp(key, node->key);
		if (c < 0)
			node->left = fz_tree_insert(ctx, node->left, key, value);
		else
			node->right = fz_tree_insert(ctx, node->right, key, value);
		node = fz_tree_skew(node);
		node = fz_tree_split(node);
		return node;
	}
	else
	{
		return fz_tree_new_node(ctx, key, value);
	}
}

void fz_drop_tree(fz_context *ctx, fz_tree *node, void (*dropfunc)(fz_context *ctx, void *value))
{
	if (node)
	{
		if (node->left != &tree_sentinel)
			fz_drop_tree(ctx, node->left, dropfunc);
		if (node->right != &tree_sentinel)
			fz_drop_tree(ctx, node->right, dropfunc);
		fz_free(ctx, node->key);
		if (dropfunc)
			dropfunc(ctx, node->value);
		fz_free(ctx, node);
	}
}
