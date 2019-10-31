#ifndef MUPDF_FITZ_TREE_H
#define MUPDF_FITZ_TREE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/*
	AA-tree to look up things by strings.
*/

typedef struct fz_tree_s fz_tree;

void *fz_tree_lookup(fz_context *ctx, fz_tree *node, const char *key);

fz_tree *fz_tree_insert(fz_context *ctx, fz_tree *root, const char *key, void *value);

void fz_drop_tree(fz_context *ctx, fz_tree *node, void (*dropfunc)(fz_context *ctx, void *value));

#endif
