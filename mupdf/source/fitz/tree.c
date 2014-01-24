#include "mupdf/fitz.h"

/* AA-tree */

struct fz_tree_s
{
	char *key;
	void *value;
	fz_tree *left, *right;
	int level;
};

static fz_tree sentinel = { "", NULL, &sentinel, &sentinel, 0 };

static fz_tree *fz_tree_new_node(fz_context *ctx, const char *key, void *value)
{
	fz_tree *node = fz_malloc_struct(ctx, fz_tree);
	node->key = fz_strdup(ctx, key);
	node->value = value;
	node->left = node->right = &sentinel;
	node->level = 1;
	return node;
}

void *fz_tree_lookup(fz_context *ctx, fz_tree *node, const char *key)
{
	if (node)
	{
		while (node != &sentinel)
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
	if (node && node != &sentinel)
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

void fz_free_tree(fz_context *ctx, fz_tree *node, void (*freefunc)(fz_context *ctx, void *value))
{
	if (node)
	{
		if (node->left != &sentinel)
			fz_free_tree(ctx, node->left, freefunc);
		if (node->right != &sentinel)
			fz_free_tree(ctx, node->right, freefunc);
		fz_free(ctx, node->key);
		if (freefunc)
			freefunc(ctx, node->value);
	}
}

static void print_tree_imp(fz_context *ctx, fz_tree *node, int level)
{
	int i;
	if (node->left != &sentinel)
		print_tree_imp(ctx, node->left, level + 1);
	for (i = 0; i < level; i++)
		putchar(' ');
	printf("%s = %p (%d)\n", node->key, node->value, node->level);
	if (node->right != &sentinel)
		print_tree_imp(ctx, node->right, level + 1);
}

void fz_debug_tree(fz_context *ctx, fz_tree *root)
{
	printf("--- tree dump ---\n");
	if (root && root != &sentinel)
		print_tree_imp(ctx, root, 0);
	printf("---\n");
}
