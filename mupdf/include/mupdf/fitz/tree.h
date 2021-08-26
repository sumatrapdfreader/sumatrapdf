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

#ifndef MUPDF_FITZ_TREE_H
#define MUPDF_FITZ_TREE_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/**
	AA-tree to look up things by strings.
*/

typedef struct fz_tree fz_tree;

/**
	Look for the value of a node in the tree with the given key.

	Simple pointer equivalence is used for key.

	Returns NULL for no match.
*/
void *fz_tree_lookup(fz_context *ctx, fz_tree *node, const char *key);

/**
	Insert a new key/value pair and rebalance the tree.
	Return the new root of the tree after inserting and rebalancing.
	May be called with a NULL root to create a new tree.

	No data is copied into the tree structure; key and value are
	merely kept as pointers.
*/
fz_tree *fz_tree_insert(fz_context *ctx, fz_tree *root, const char *key, void *value);

/**
	Drop the tree.

	The storage used by the tree is freed, and each value has
	dropfunc called on it.
*/
void fz_drop_tree(fz_context *ctx, fz_tree *node, void (*dropfunc)(fz_context *ctx, void *value));

#endif
