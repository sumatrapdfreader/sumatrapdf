#ifndef MUPDF_PDF_PARSE_H
#define MUPDF_PDF_PARSE_H

/*
 * tokenizer and low-level object parser
 */

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
	PDF_NUM_TOKENS
} pdf_token;

void pdf_lexbuf_init(fz_context *ctx, pdf_lexbuf *lexbuf, int size);
void pdf_lexbuf_fin(pdf_lexbuf *lexbuf);
ptrdiff_t pdf_lexbuf_grow(pdf_lexbuf *lexbuf);

pdf_token pdf_lex(fz_stream *f, pdf_lexbuf *lexbuf);

pdf_obj *pdf_parse_array(pdf_document *doc, fz_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_dict(pdf_document *doc, fz_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_stm_obj(pdf_document *doc, fz_stream *f, pdf_lexbuf *buf);
pdf_obj *pdf_parse_ind_obj(pdf_document *doc, fz_stream *f, pdf_lexbuf *buf, int *num, int *gen, int *stm_ofs);

/*
	pdf_print_token: print a lexed token to a buffer, growing if necessary
*/
void pdf_print_token(fz_context *ctx, fz_buffer *buf, int tok, pdf_lexbuf *lex);

#endif
