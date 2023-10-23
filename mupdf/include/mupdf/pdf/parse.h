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

#ifndef MUPDF_PDF_PARSE_H
#define MUPDF_PDF_PARSE_H

#include "mupdf/pdf/document.h"

typedef enum
{
	PDF_TOK_ERROR, PDF_TOK_EOF,
	PDF_TOK_OPEN_ARRAY, PDF_TOK_CLOSE_ARRAY,
	PDF_TOK_OPEN_DICT, PDF_TOK_CLOSE_DICT,
	PDF_TOK_OPEN_BRACE, PDF_TOK_CLOSE_BRACE,
	PDF_TOK_NAME, PDF_TOK_INT, PDF_TOK_REAL, PDF_TOK_STRING, PDF_TOK_KEYWORD,
	PDF_TOK_R, PDF_TOK_TRUE, PDF_TOK_FALSE, PDF_TOK_NULL,
	PDF_TOK_OBJ, PDF_TOK_ENDOBJ,
	PDF_TOK_STREAM, PDF_TOK_ENDSTREAM,
	PDF_TOK_XREF, PDF_TOK_TRAILER, PDF_TOK_STARTXREF,
	PDF_TOK_NEWOBJ,
	PDF_NUM_TOKENS
} pdf_token;

void pdf_lexbuf_init(fz_context *ctx, pdf_lexbuf *lexbuf, int size);
void pdf_lexbuf_fin(fz_context *ctx, pdf_lexbuf *lexbuf);
ptrdiff_t pdf_lexbuf_grow(fz_context *ctx, pdf_lexbuf *lexbuf);

pdf_token pdf_lex(fz_context *ctx, fz_stream *f, pdf_lexbuf *lexbuf);
pdf_token pdf_lex_no_string(fz_context *ctx, fz_stream *f, pdf_lexbuf *lexbuf);

pdf_obj *pdf_parse_array(fz_context *ctx, pdf_document *doc, fz_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_dict(fz_context *ctx, pdf_document *doc, fz_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_stm_obj(fz_context *ctx, pdf_document *doc, fz_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_ind_obj(fz_context *ctx, pdf_document *doc, fz_stream *f, int *num, int *gen, int64_t *stm_ofs, int *try_repair);
pdf_obj *pdf_parse_journal_obj(fz_context *ctx, pdf_document *doc, fz_stream *stm, int *onum, fz_buffer **ostm, int *newobj);

/*
	print a lexed token to a buffer, growing if necessary
*/
void pdf_append_token(fz_context *ctx, fz_buffer *buf, int tok, pdf_lexbuf *lex);

#endif
