#include "mupdf/fitz.h"

#include <string.h>

/* AA-tree */

struct fz_tree_s
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

/*
	Insert a new key/value pair and rebalance the tree.
	Return the new root of the tree after inserting and rebalancing.
	May be called with a NULL root to create a new tree.
*/
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
