// Copyright (C) 2025 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_HYPHEN_H
#define MUPDF_FITZ_HYPHEN_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/types.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/text.h"

typedef struct fz_hyphenator fz_hyphenator;
typedef struct fz_hyph_trie fz_hyph_trie;

struct fz_hyphenator {
	fz_pool *pool;
	int node_count;
	int pattern_count;
	fz_hyph_trie *trie;
};

struct fz_hyph_trie
{
	char *patval; /* null unless leaf */
	short patlen; /* num values - 1 in pattern */
	int ch; /* char to branch on (not used for leaves) */
	fz_hyph_trie *child;
	fz_hyph_trie *next;
};

fz_hyphenator *fz_new_hyphenator_from_stream(fz_context *ctx, fz_stream *stm);
void fz_register_hyphenator(fz_context *ctx, fz_text_language lang, fz_hyphenator *hyph);

void fz_hyphenate_word(fz_context *ctx, fz_hyphenator *hyph, const char *input, int input_size, char *output, int output_size);
void fz_drop_hyphenator(fz_context *ctx, fz_hyphenator *hyph);

fz_hyphenator *fz_lookup_hyphenator(fz_context *ctx, fz_text_language lang);

#endif
