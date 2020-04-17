#ifndef MUPDF_FITZ_TREE_H
#define MUPDF_FITZ_TREE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/*
	AA-tree to look up things by strings.
*/

typedef struct fz_tree fz_tree;

/*
	Look for the value of a node in the tree with the given key.

	Simple pointer equivalence is used for key.

	Returns NULL for no match.
*/
void *fz_tree_lookup(fz_context *ctx, fz_tree *node, const char *key);

/*
	Insert a new key/value pair and rebalance the tree.
	Return the new root of the tree after inserting and rebalancing.
	May be called with a NULL root to create a new tree.

	No data is copied into the tree structure; key and value are
	merely kept as pointers.
*/
fz_tree *fz_tree_insert(fz_context *ctx, fz_tree *root, const char *key, void *value);

/*
	Drop the tree.

	The storage used by the tree is freed, and each value has
	dropfunc called on it.
*/
void fz_drop_tree(fz_context *ctx, fz_tree *node, void (*dropfunc)(fz_context *ctx, void *value));

#endif
