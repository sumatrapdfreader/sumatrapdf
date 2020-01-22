#ifndef MUPDF_PDF_XREF_H
#define MUPDF_PDF_XREF_H

int pdf_create_object(fz_context *ctx, pdf_document *doc);

void pdf_delete_object(fz_context *ctx, pdf_document *doc, int num);

void pdf_update_object(fz_context *ctx, pdf_document *doc, int num, pdf_obj *obj);

void pdf_update_stream(fz_context *ctx, pdf_document *doc, pdf_obj *ref, fz_buffer *buf, int compressed);

pdf_obj *pdf_add_object(fz_context *ctx, pdf_document *doc, pdf_obj *obj);
pdf_obj *pdf_add_object_drop(fz_context *ctx, pdf_document *doc, pdf_obj *obj);
pdf_obj *pdf_add_stream(fz_context *ctx, pdf_document *doc, fz_buffer *buf, pdf_obj *obj, int compressed);

pdf_obj *pdf_add_new_dict(fz_context *ctx, pdf_document *doc, int initial);
pdf_obj *pdf_add_new_array(fz_context *ctx, pdf_document *doc, int initial);

typedef struct pdf_xref_entry_s pdf_xref_entry;

struct pdf_xref_entry_s
{
	char type;		/* 0=unset (f)ree i(n)use (o)bjstm */
	unsigned char marked;	/* marked to keep alive with pdf_mark_xref */
	unsigned short gen;	/* generation / objstm index */
	int num;		/* original object number (for decryption after renumbering) */
	int64_t ofs;		/* file offset / objstm object number */
	int64_t stm_ofs;	/* on-disk stream */
	fz_buffer *stm_buf;	/* in-memory stream (for updated objects) */
	pdf_obj *obj;		/* stored/cached object */
};

typedef struct pdf_xref_subsec_s pdf_xref_subsec;

struct pdf_xref_subsec_s
{
	pdf_xref_subsec *next;
	int len;
	int start;
	pdf_xref_entry *table;
};

struct pdf_xref_s
{
	int num_objects;
	pdf_xref_subsec *subsec;
	pdf_obj *trailer;
	pdf_obj *pre_repair_trailer;
	pdf_unsaved_sig *unsaved_sigs;
	pdf_unsaved_sig **unsaved_sigs_end;
	int64_t end_ofs; /* file offset to end of xref */
};

pdf_xref_entry *pdf_cache_object(fz_context *ctx, pdf_document *doc, int num);

int pdf_count_objects(fz_context *ctx, pdf_document *doc);
pdf_obj *pdf_resolve_indirect(fz_context *ctx, pdf_obj *ref);
pdf_obj *pdf_resolve_indirect_chain(fz_context *ctx, pdf_obj *ref);
pdf_obj *pdf_load_object(fz_context *ctx, pdf_document *doc, int num);
pdf_obj *pdf_load_unencrypted_object(fz_context *ctx, pdf_document *doc, int num);

fz_buffer *pdf_load_raw_stream_number(fz_context *ctx, pdf_document *doc, int num);
fz_buffer *pdf_load_raw_stream(fz_context *ctx, pdf_obj *ref);
fz_buffer *pdf_load_stream_number(fz_context *ctx, pdf_document *doc, int num);
fz_buffer *pdf_load_stream(fz_context *ctx, pdf_obj *ref);
fz_stream *pdf_open_raw_stream_number(fz_context *ctx, pdf_document *doc, int num);
fz_stream *pdf_open_raw_stream(fz_context *ctx, pdf_obj *ref);
fz_stream *pdf_open_stream_number(fz_context *ctx, pdf_document *doc, int num);
fz_stream *pdf_open_stream(fz_context *ctx, pdf_obj *ref);

fz_stream *pdf_open_inline_stream(fz_context *ctx, pdf_document *doc, pdf_obj *stmobj, int length, fz_stream *chain, fz_compression_params *params);
fz_compressed_buffer *pdf_load_compressed_stream(fz_context *ctx, pdf_document *doc, int num);
void pdf_load_compressed_inline_image(fz_context *ctx, pdf_document *doc, pdf_obj *dict, int length, fz_stream *cstm, int indexed, fz_compressed_image *image);
fz_stream *pdf_open_stream_with_offset(fz_context *ctx, pdf_document *doc, int num, pdf_obj *dict, int64_t stm_ofs);
fz_stream *pdf_open_compressed_stream(fz_context *ctx, fz_compressed_buffer *);
fz_stream *pdf_open_contents_stream(fz_context *ctx, pdf_document *doc, pdf_obj *obj);

int pdf_version(fz_context *ctx, pdf_document *doc);
pdf_obj *pdf_trailer(fz_context *ctx, pdf_document *doc);
void pdf_set_populating_xref_trailer(fz_context *ctx, pdf_document *doc, pdf_obj *trailer);
int pdf_xref_len(fz_context *ctx, pdf_document *doc);
pdf_xref_entry *pdf_get_populating_xref_entry(fz_context *ctx, pdf_document *doc, int i);
pdf_xref_entry *pdf_get_xref_entry(fz_context *ctx, pdf_document *doc, int i);
void pdf_replace_xref(fz_context *ctx, pdf_document *doc, pdf_xref_entry *entries, int n);
void pdf_forget_xref(fz_context *ctx, pdf_document *doc);
void pdf_xref_ensure_incremental_object(fz_context *ctx, pdf_document *doc, int num);
int pdf_xref_is_incremental(fz_context *ctx, pdf_document *doc, int num);
void pdf_xref_store_unsaved_signature(fz_context *ctx, pdf_document *doc, pdf_obj *field, pdf_pkcs7_signer *signer);
int pdf_xref_obj_is_unsaved_signature(pdf_document *doc, pdf_obj *obj);

void pdf_repair_xref(fz_context *ctx, pdf_document *doc);
void pdf_repair_obj_stms(fz_context *ctx, pdf_document *doc);
void pdf_ensure_solid_xref(fz_context *ctx, pdf_document *doc, int num);
void pdf_mark_xref(fz_context *ctx, pdf_document *doc);
void pdf_clear_xref(fz_context *ctx, pdf_document *doc);
void pdf_clear_xref_to_mark(fz_context *ctx, pdf_document *doc);

int pdf_repair_obj(fz_context *ctx, pdf_document *doc, pdf_lexbuf *buf, int64_t *stmofsp, int *stmlenp, pdf_obj **encrypt, pdf_obj **id, pdf_obj **page, int64_t *tmpofs, pdf_obj **root);

pdf_obj *pdf_progressive_advance(fz_context *ctx, pdf_document *doc, int pagenum);

int pdf_count_incremental_updates(fz_context *ctx, pdf_document *doc);
int pdf_validate_changes(fz_context *ctx, pdf_document *doc, int version);
int pdf_doc_was_linearized(fz_context *ctx, pdf_document *doc);

typedef struct pdf_locked_fields_s pdf_locked_fields;
int pdf_is_field_locked(fz_context *ctx, pdf_locked_fields *locked, const char *name);
void pdf_drop_locked_fields(fz_context *ctx, pdf_locked_fields *locked);
pdf_locked_fields *pdf_find_locked_fields(fz_context *ctx, pdf_document *doc, int version);
pdf_locked_fields *pdf_find_locked_fields_for_sig(fz_context *ctx, pdf_document *doc, pdf_obj *sig);
int pdf_validate_change_history(fz_context *ctx, pdf_document *doc);

int pdf_find_version_for_obj(fz_context *ctx, pdf_document *doc, pdf_obj *obj);
int pdf_validate_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget);

#endif
