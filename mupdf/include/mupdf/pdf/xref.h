#ifndef MUPDF_PDF_XREF_H
#define MUPDF_PDF_XREF_H

/*
	pdf_create_object: Allocate a slot in the xref table and return a fresh unused object number.
*/
int pdf_create_object(pdf_document *doc);

/*
	pdf_delete_object: Remove object from xref table, marking the slot as free.
*/
void pdf_delete_object(pdf_document *doc, int num);

/*
	pdf_update_object: Replace object in xref table with the passed in object.
*/
void pdf_update_object(pdf_document *doc, int num, pdf_obj *obj);

/*
	pdf_update_stream: Replace stream contents for object in xref table with the passed in buffer.

	The buffer contents must match the /Filter setting.
	If storing uncompressed data, make sure to delete the /Filter key from
	the stream dictionary. If storing deflated data, make sure to set the
	/Filter value to /FlateDecode.
*/
void pdf_update_stream(pdf_document *doc, int num, fz_buffer *buf);

/*
 * xref and object / stream api
 */

typedef struct pdf_xref_entry_s pdf_xref_entry;

struct pdf_xref_entry_s
{
	char type;	/* 0=unset (f)ree i(n)use (o)bjstm */
	int ofs;	/* file offset / objstm object number */
	int gen;	/* generation / objstm index */
	int stm_ofs;	/* on-disk stream */
	fz_buffer *stm_buf; /* in-memory stream (for updated objects) */
	pdf_obj *obj;	/* stored/cached object */
};

struct pdf_xref_s
{
	int len;
	pdf_xref_entry *table;
	pdf_obj *trailer;
};

void pdf_cache_object(pdf_document *doc, int num, int gen);

int pdf_count_objects(pdf_document *doc);
pdf_obj *pdf_resolve_indirect(pdf_obj *ref);
pdf_obj *pdf_load_object(pdf_document *doc, int num, int gen);

fz_buffer *pdf_load_raw_stream(pdf_document *doc, int num, int gen);
fz_buffer *pdf_load_stream(pdf_document *doc, int num, int gen);
fz_stream *pdf_open_raw_stream(pdf_document *doc, int num, int gen);
fz_stream *pdf_open_stream(pdf_document *doc, int num, int gen);

fz_stream *pdf_open_inline_stream(pdf_document *doc, pdf_obj *stmobj, int length, fz_stream *chain, fz_compression_params *params);
fz_compressed_buffer *pdf_load_compressed_stream(pdf_document *doc, int num, int gen);
fz_stream *pdf_open_stream_with_offset(pdf_document *doc, int num, int gen, pdf_obj *dict, int stm_ofs);
fz_stream *pdf_open_compressed_stream(fz_context *ctx, fz_compressed_buffer *);
fz_stream *pdf_open_contents_stream(pdf_document *doc, pdf_obj *obj);
fz_buffer *pdf_load_raw_renumbered_stream(pdf_document *doc, int num, int gen, int orig_num, int orig_gen);
fz_buffer *pdf_load_renumbered_stream(pdf_document *doc, int num, int gen, int orig_num, int orig_gen, int *truncated);
fz_stream *pdf_open_raw_renumbered_stream(pdf_document *doc, int num, int gen, int orig_num, int orig_gen);

pdf_obj *pdf_trailer(pdf_document *doc);
void pdf_set_populating_xref_trailer(pdf_document *doc, pdf_obj *trailer);
int pdf_xref_len(pdf_document *doc);
pdf_xref_entry *pdf_get_populating_xref_entry(pdf_document *doc, int i);
pdf_xref_entry *pdf_get_xref_entry(pdf_document *doc, int i);
void pdf_replace_xref(pdf_document *doc, pdf_xref_entry *entries, int n);
void pdf_xref_ensure_incremental_object(pdf_document *doc, int num);
int pdf_xref_is_incremental(pdf_document *doc, int num);

void pdf_repair_xref(pdf_document *doc, pdf_lexbuf *buf);
void pdf_repair_obj_stms(pdf_document *doc);
pdf_obj *pdf_new_ref(pdf_document *doc, pdf_obj *obj);

int pdf_repair_obj(pdf_document *doc, pdf_lexbuf *buf, int *stmofsp, int *stmlenp, pdf_obj **encrypt, pdf_obj **id, pdf_obj **page, int *tmpofs);

pdf_obj *pdf_progressive_advance(pdf_document *doc, int pagenum);

void pdf_print_xref(pdf_document *);

#endif
