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

#ifndef MUPDF_PDF_OBJECT_H
#define MUPDF_PDF_OBJECT_H

#include "mupdf/fitz/stream.h"

typedef struct pdf_document pdf_document;
typedef struct pdf_crypt pdf_crypt;
typedef struct pdf_journal pdf_journal;

/* Defined in PDF 1.7 according to Acrobat limit. */
#define PDF_MAX_OBJECT_NUMBER 8388607
#define PDF_MAX_GEN_NUMBER 65535

/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filters and the mupdf parser.
 */

typedef struct pdf_obj pdf_obj;

pdf_obj *pdf_new_int(fz_context *ctx, int64_t i);
pdf_obj *pdf_new_real(fz_context *ctx, float f);
pdf_obj *pdf_new_name(fz_context *ctx, const char *str);
pdf_obj *pdf_new_string(fz_context *ctx, const char *str, size_t len);

/*
	Create a PDF 'text string' by encoding input string as either ASCII or UTF-16BE.
	In theory, we could also use PDFDocEncoding.
*/
pdf_obj *pdf_new_text_string(fz_context *ctx, const char *s);
pdf_obj *pdf_new_indirect(fz_context *ctx, pdf_document *doc, int num, int gen);
pdf_obj *pdf_new_array(fz_context *ctx, pdf_document *doc, int initialcap);
pdf_obj *pdf_new_dict(fz_context *ctx, pdf_document *doc, int initialcap);
pdf_obj *pdf_new_rect(fz_context *ctx, pdf_document *doc, fz_rect rect);
pdf_obj *pdf_new_matrix(fz_context *ctx, pdf_document *doc, fz_matrix mtx);
pdf_obj *pdf_new_date(fz_context *ctx, pdf_document *doc, int64_t time);
pdf_obj *pdf_copy_array(fz_context *ctx, pdf_obj *array);
pdf_obj *pdf_copy_dict(fz_context *ctx, pdf_obj *dict);
pdf_obj *pdf_deep_copy_obj(fz_context *ctx, pdf_obj *obj);

pdf_obj *pdf_keep_obj(fz_context *ctx, pdf_obj *obj);
void pdf_drop_obj(fz_context *ctx, pdf_obj *obj);
pdf_obj *pdf_drop_singleton_obj(fz_context *ctx, pdf_obj *obj);

int pdf_is_null(fz_context *ctx, pdf_obj *obj);
int pdf_is_bool(fz_context *ctx, pdf_obj *obj);
int pdf_is_int(fz_context *ctx, pdf_obj *obj);
int pdf_is_real(fz_context *ctx, pdf_obj *obj);
int pdf_is_number(fz_context *ctx, pdf_obj *obj);
int pdf_is_name(fz_context *ctx, pdf_obj *obj);
int pdf_is_string(fz_context *ctx, pdf_obj *obj);
int pdf_is_array(fz_context *ctx, pdf_obj *obj);
int pdf_is_dict(fz_context *ctx, pdf_obj *obj);
int pdf_is_indirect(fz_context *ctx, pdf_obj *obj);

/*
	Check if an object is a stream or not.
*/
int pdf_obj_num_is_stream(fz_context *ctx, pdf_document *doc, int num);
int pdf_is_stream(fz_context *ctx, pdf_obj *obj);

/* Compare 2 objects. Returns 0 on match, non-zero on mismatch.
 * Streams always mismatch.
 */
int pdf_objcmp(fz_context *ctx, pdf_obj *a, pdf_obj *b);
int pdf_objcmp_resolve(fz_context *ctx, pdf_obj *a, pdf_obj *b);

/* Compare 2 objects. Returns 0 on match, non-zero on mismatch.
 * Stream contents are explicitly checked.
 */
int pdf_objcmp_deep(fz_context *ctx, pdf_obj *a, pdf_obj *b);

int pdf_name_eq(fz_context *ctx, pdf_obj *a, pdf_obj *b);

int pdf_obj_marked(fz_context *ctx, pdf_obj *obj);
int pdf_mark_obj(fz_context *ctx, pdf_obj *obj);
void pdf_unmark_obj(fz_context *ctx, pdf_obj *obj);

typedef struct pdf_cycle_list pdf_cycle_list;
struct pdf_cycle_list {
	pdf_cycle_list *up;
	int num;
};
int pdf_cycle(fz_context *ctx, pdf_cycle_list *here, pdf_cycle_list *prev, pdf_obj *obj);

typedef struct
{
	int len;
	unsigned char bits[1];
} pdf_mark_bits;

pdf_mark_bits *pdf_new_mark_bits(fz_context *ctx, pdf_document *doc);
void pdf_drop_mark_bits(fz_context *ctx, pdf_mark_bits *marks);
void pdf_mark_bits_reset(fz_context *ctx, pdf_mark_bits *marks);
int pdf_mark_bits_set(fz_context *ctx, pdf_mark_bits *marks, pdf_obj *obj);

typedef struct
{
	int len;
	int max;
	int *list;
	int local_list[8];
} pdf_mark_list;

int pdf_mark_list_push(fz_context *ctx, pdf_mark_list *list, pdf_obj *obj);
void pdf_mark_list_pop(fz_context *ctx, pdf_mark_list *list);
void pdf_mark_list_init(fz_context *ctx, pdf_mark_list *list);
void pdf_mark_list_free(fz_context *ctx, pdf_mark_list *list);

void pdf_set_obj_memo(fz_context *ctx, pdf_obj *obj, int bit, int memo);
int pdf_obj_memo(fz_context *ctx, pdf_obj *obj, int bit, int *memo);

int pdf_obj_is_dirty(fz_context *ctx, pdf_obj *obj);
void pdf_dirty_obj(fz_context *ctx, pdf_obj *obj);
void pdf_clean_obj(fz_context *ctx, pdf_obj *obj);

int pdf_to_bool(fz_context *ctx, pdf_obj *obj);
int pdf_to_int(fz_context *ctx, pdf_obj *obj);
int64_t pdf_to_int64(fz_context *ctx, pdf_obj *obj);
float pdf_to_real(fz_context *ctx, pdf_obj *obj);
const char *pdf_to_name(fz_context *ctx, pdf_obj *obj);
const char *pdf_to_text_string(fz_context *ctx, pdf_obj *obj);
const char *pdf_to_string(fz_context *ctx, pdf_obj *obj, size_t *sizep);
char *pdf_to_str_buf(fz_context *ctx, pdf_obj *obj);
size_t pdf_to_str_len(fz_context *ctx, pdf_obj *obj);
int pdf_to_num(fz_context *ctx, pdf_obj *obj);
int pdf_to_gen(fz_context *ctx, pdf_obj *obj);

int pdf_to_bool_default(fz_context *ctx, pdf_obj *obj, int def);
int pdf_to_int_default(fz_context *ctx, pdf_obj *obj, int def);
float pdf_to_real_default(fz_context *ctx, pdf_obj *obj, float def);

int pdf_array_len(fz_context *ctx, pdf_obj *array);
pdf_obj *pdf_array_get(fz_context *ctx, pdf_obj *array, int i);
void pdf_array_put(fz_context *ctx, pdf_obj *array, int i, pdf_obj *obj);
void pdf_array_put_drop(fz_context *ctx, pdf_obj *array, int i, pdf_obj *obj);
void pdf_array_push(fz_context *ctx, pdf_obj *array, pdf_obj *obj);
void pdf_array_push_drop(fz_context *ctx, pdf_obj *array, pdf_obj *obj);
void pdf_array_insert(fz_context *ctx, pdf_obj *array, pdf_obj *obj, int index);
void pdf_array_insert_drop(fz_context *ctx, pdf_obj *array, pdf_obj *obj, int index);
void pdf_array_delete(fz_context *ctx, pdf_obj *array, int index);
int pdf_array_find(fz_context *ctx, pdf_obj *array, pdf_obj *obj);
int pdf_array_contains(fz_context *ctx, pdf_obj *array, pdf_obj *obj);

int pdf_dict_len(fz_context *ctx, pdf_obj *dict);
pdf_obj *pdf_dict_get_key(fz_context *ctx, pdf_obj *dict, int idx);
pdf_obj *pdf_dict_get_val(fz_context *ctx, pdf_obj *dict, int idx);
void pdf_dict_put_val_null(fz_context *ctx, pdf_obj *obj, int idx);
pdf_obj *pdf_dict_get(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
pdf_obj *pdf_dict_getp(fz_context *ctx, pdf_obj *dict, const char *path);
pdf_obj *pdf_dict_getl(fz_context *ctx, pdf_obj *dict, ...);
pdf_obj *pdf_dict_geta(fz_context *ctx, pdf_obj *dict, pdf_obj *key, pdf_obj *abbrev);
pdf_obj *pdf_dict_gets(fz_context *ctx, pdf_obj *dict, const char *key);
pdf_obj *pdf_dict_getsa(fz_context *ctx, pdf_obj *dict, const char *key, const char *abbrev);
pdf_obj *pdf_dict_get_inheritable(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
pdf_obj *pdf_dict_getp_inheritable(fz_context *ctx, pdf_obj *dict, const char *path);
pdf_obj *pdf_dict_gets_inheritable(fz_context *ctx, pdf_obj *dict, const char *key);
void pdf_dict_put(fz_context *ctx, pdf_obj *dict, pdf_obj *key, pdf_obj *val);
void pdf_dict_put_drop(fz_context *ctx, pdf_obj *dict, pdf_obj *key, pdf_obj *val);
void pdf_dict_get_put_drop(fz_context *ctx, pdf_obj *dict, pdf_obj *key, pdf_obj *val, pdf_obj **old_val);
void pdf_dict_puts(fz_context *ctx, pdf_obj *dict, const char *key, pdf_obj *val);
void pdf_dict_puts_drop(fz_context *ctx, pdf_obj *dict, const char *key, pdf_obj *val);
void pdf_dict_putp(fz_context *ctx, pdf_obj *dict, const char *path, pdf_obj *val);
void pdf_dict_putp_drop(fz_context *ctx, pdf_obj *dict, const char *path, pdf_obj *val);
void pdf_dict_putl(fz_context *ctx, pdf_obj *dict, pdf_obj *val, ...);
void pdf_dict_putl_drop(fz_context *ctx, pdf_obj *dict, pdf_obj *val, ...);
void pdf_dict_del(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
void pdf_dict_dels(fz_context *ctx, pdf_obj *dict, const char *key);
void pdf_sort_dict(fz_context *ctx, pdf_obj *dict);

void pdf_dict_put_bool(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int x);
void pdf_dict_put_int(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int64_t x);
void pdf_dict_put_real(fz_context *ctx, pdf_obj *dict, pdf_obj *key, double x);
void pdf_dict_put_name(fz_context *ctx, pdf_obj *dict, pdf_obj *key, const char *x);
void pdf_dict_put_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, const char *x, size_t n);
void pdf_dict_put_text_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, const char *x);
void pdf_dict_put_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key, fz_rect x);
void pdf_dict_put_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key, fz_matrix x);
void pdf_dict_put_date(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int64_t time);
pdf_obj *pdf_dict_put_array(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int initial);
pdf_obj *pdf_dict_put_dict(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int initial);
pdf_obj *pdf_dict_puts_dict(fz_context *ctx, pdf_obj *dict, const char *key, int initial);

int pdf_dict_get_bool(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
int pdf_dict_get_int(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
int64_t pdf_dict_get_int64(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
float pdf_dict_get_real(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
const char *pdf_dict_get_name(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
const char *pdf_dict_get_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, size_t *sizep);
const char *pdf_dict_get_text_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
fz_rect pdf_dict_get_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
fz_matrix pdf_dict_get_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
int64_t pdf_dict_get_date(fz_context *ctx, pdf_obj *dict, pdf_obj *key);

int pdf_dict_get_bool_default(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int def);
int pdf_dict_get_int_default(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int def);
float pdf_dict_get_real_default(fz_context *ctx, pdf_obj *dict, pdf_obj *key, float def);

int pdf_dict_get_inheritable_bool(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
int pdf_dict_get_inheritable_int(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
int64_t pdf_dict_get_inheritable_int64(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
float pdf_dict_get_inheritable_real(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
const char *pdf_dict_get_inheritable_name(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
const char *pdf_dict_get_inheritable_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, size_t *sizep);
const char *pdf_dict_get_inheritable_text_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
fz_rect pdf_dict_get_inheritable_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
fz_matrix pdf_dict_get_inheritable_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
int64_t pdf_dict_get_inheritable_date(fz_context *ctx, pdf_obj *dict, pdf_obj *key);

void pdf_array_push_bool(fz_context *ctx, pdf_obj *array, int x);
void pdf_array_push_int(fz_context *ctx, pdf_obj *array, int64_t x);
void pdf_array_push_real(fz_context *ctx, pdf_obj *array, double x);
void pdf_array_push_name(fz_context *ctx, pdf_obj *array, const char *x);
void pdf_array_push_string(fz_context *ctx, pdf_obj *array, const char *x, size_t n);
void pdf_array_push_text_string(fz_context *ctx, pdf_obj *array, const char *x);
pdf_obj *pdf_array_push_array(fz_context *ctx, pdf_obj *array, int initial);
pdf_obj *pdf_array_push_dict(fz_context *ctx, pdf_obj *array, int initial);

void pdf_array_put_bool(fz_context *ctx, pdf_obj *array, int i, int x);
void pdf_array_put_int(fz_context *ctx, pdf_obj *array, int i, int64_t x);
void pdf_array_put_real(fz_context *ctx, pdf_obj *array, int i, double x);
void pdf_array_put_name(fz_context *ctx, pdf_obj *array, int i, const char *x);
void pdf_array_put_string(fz_context *ctx, pdf_obj *array, int i, const char *x, size_t n);
void pdf_array_put_text_string(fz_context *ctx, pdf_obj *array, int i, const char *x);
pdf_obj *pdf_array_put_array(fz_context *ctx, pdf_obj *array, int i, int initial);
pdf_obj *pdf_array_put_dict(fz_context *ctx, pdf_obj *array, int i, int initial);

int pdf_array_get_bool(fz_context *ctx, pdf_obj *array, int index);
int pdf_array_get_int(fz_context *ctx, pdf_obj *array, int index);
float pdf_array_get_real(fz_context *ctx, pdf_obj *array, int index);
const char *pdf_array_get_name(fz_context *ctx, pdf_obj *array, int index);
const char *pdf_array_get_string(fz_context *ctx, pdf_obj *array, int index, size_t *sizep);
const char *pdf_array_get_text_string(fz_context *ctx, pdf_obj *array, int index);
fz_rect pdf_array_get_rect(fz_context *ctx, pdf_obj *array, int index);
fz_matrix pdf_array_get_matrix(fz_context *ctx, pdf_obj *array, int index);

void pdf_set_obj_parent(fz_context *ctx, pdf_obj *obj, int num);

int pdf_obj_refs(fz_context *ctx, pdf_obj *ref);

int pdf_obj_parent_num(fz_context *ctx, pdf_obj *obj);

char *pdf_sprint_obj(fz_context *ctx, char *buf, size_t cap, size_t *len, pdf_obj *obj, int tight, int ascii);
void pdf_print_obj(fz_context *ctx, fz_output *out, pdf_obj *obj, int tight, int ascii);
void pdf_print_encrypted_obj(fz_context *ctx, fz_output *out, pdf_obj *obj, int tight, int ascii, pdf_crypt *crypt, int num, int gen);

void pdf_debug_obj(fz_context *ctx, pdf_obj *obj);
void pdf_debug_ref(fz_context *ctx, pdf_obj *obj);

/*
	Convert Unicode/PdfDocEncoding string into utf-8.

	The returned string must be freed by the caller.
*/
char *pdf_new_utf8_from_pdf_string(fz_context *ctx, const char *srcptr, size_t srclen);

/*
	Convert text string object to UTF-8.

	The returned string must be freed by the caller.
*/
char *pdf_new_utf8_from_pdf_string_obj(fz_context *ctx, pdf_obj *src);

/*
	Load text stream and convert to UTF-8.

	The returned string must be freed by the caller.
*/
char *pdf_new_utf8_from_pdf_stream_obj(fz_context *ctx, pdf_obj *src);

/*
	Load text stream or text string and convert to UTF-8.

	The returned string must be freed by the caller.
*/
char *pdf_load_stream_or_string_as_utf8(fz_context *ctx, pdf_obj *src);

fz_quad pdf_to_quad(fz_context *ctx, pdf_obj *array, int offset);
fz_rect pdf_to_rect(fz_context *ctx, pdf_obj *array);
fz_matrix pdf_to_matrix(fz_context *ctx, pdf_obj *array);
int64_t pdf_to_date(fz_context *ctx, pdf_obj *time);

/*
	pdf_get_indirect_document and pdf_get_bound_document are
	now deprecated. Please do not use them in future. They will
	be removed.

	Please use pdf_pin_document instead.
*/
pdf_document *pdf_get_indirect_document(fz_context *ctx, pdf_obj *obj);
pdf_document *pdf_get_bound_document(fz_context *ctx, pdf_obj *obj);

/*
	pdf_pin_document returns a new reference to the document
	to which obj is bound. The caller is responsible for
	dropping this reference once they have finished with it.

	This is a replacement for pdf_get_indirect_document
	and pdf_get_bound_document that are now deprecated. Those
	returned a borrowed reference that did not need to be
	dropped.

	Note that this can validly return NULL in various cases:
	1) When the object is of a simple type (such as a number
	or a string), it contains no reference to the enclosing
	document. 2) When the object has yet to be inserted into
	a PDF document (such as during parsing). 3) And (in
	future versions) when the document has been destroyed
	but the object reference remains.

	It is the caller's responsibility to deal with a NULL
	return here.
*/
pdf_document *pdf_pin_document(fz_context *ctx, pdf_obj *obj);

void pdf_set_int(fz_context *ctx, pdf_obj *obj, int64_t i);

/* Voodoo to create PDF_NAME(Foo) macros from name-table.h */

#define PDF_NAME(X) ((pdf_obj*)(intptr_t)PDF_ENUM_NAME_##X)

#define PDF_MAKE_NAME(STRING,NAME) PDF_ENUM_NAME_##NAME,
enum {
	PDF_ENUM_NULL,
	PDF_ENUM_TRUE,
	PDF_ENUM_FALSE,
#include "mupdf/pdf/name-table.h"
	PDF_ENUM_LIMIT,
};
#undef PDF_MAKE_NAME

#define PDF_NULL ((pdf_obj*)(intptr_t)PDF_ENUM_NULL)
#define PDF_TRUE ((pdf_obj*)(intptr_t)PDF_ENUM_TRUE)
#define PDF_FALSE ((pdf_obj*)(intptr_t)PDF_ENUM_FALSE)
#define PDF_LIMIT ((pdf_obj*)(intptr_t)PDF_ENUM_LIMIT)


/* Implementation details: subject to change. */

/*
	for use by pdf_crypt_obj_imp to decrypt AES string in place
*/
void pdf_set_str_len(fz_context *ctx, pdf_obj *obj, size_t newlen);


/* Journalling */

/* Call this to enable journalling on a given document. */
void pdf_enable_journal(fz_context *ctx, pdf_document *doc);

/* Call this to start an operation. Undo/redo works at 'operation'
 * granularity. Nested operations are all counted within the outermost
 * operation. Any modification performed on a journalled PDF without an
 * operation having been started will throw an error. */
void pdf_begin_operation(fz_context *ctx, pdf_document *doc, const char *operation);

/* Call this to start an implicit operation. Implicit operations are
 * operations that happen as a consequence of things like updating
 * an annotation. They get rolled into the previous operation, because
 * they generally happen as a result of them. */
void pdf_begin_implicit_operation(fz_context *ctx, pdf_document *doc);

/* Call this to end an operation. */
void pdf_end_operation(fz_context *ctx, pdf_document *doc);

/* Call this to abandon an operation. Revert to the state
 * when you began. */
void pdf_abandon_operation(fz_context *ctx, pdf_document *doc);

/* Call this to find out how many undo/redo steps there are, and the
 * current position we are within those. 0 = original document,
 * *steps = final edited version. */
int pdf_undoredo_state(fz_context *ctx, pdf_document *doc, int *steps);

/* Call this to find the title of the operation within the undo state. */
const char *pdf_undoredo_step(fz_context *ctx, pdf_document *doc, int step);

/* Helper functions to identify if we are in a state to be able to undo
 * or redo. */
int pdf_can_undo(fz_context *ctx, pdf_document *doc);
int pdf_can_redo(fz_context *ctx, pdf_document *doc);

/* Move backwards in the undo history. Throws an error if we are at the
 * start. Any edits to the document at this point will discard all
 * subsequent history. */
void pdf_undo(fz_context *ctx, pdf_document *doc);

/* Move forwards in the undo history. Throws an error if we are at the
 * end. */
void pdf_redo(fz_context *ctx, pdf_document *doc);

/* Called to reset the entire history. This is called implicitly when
 * a non-undoable change occurs (such as a pdf repair). */
void pdf_discard_journal(fz_context *ctx, pdf_journal *journal);

/* Internal destructor. */
void pdf_drop_journal(fz_context *ctx, pdf_journal *journal);

/* Internal call as part of saving a snapshot of a PDF document. */
void pdf_serialise_journal(fz_context *ctx, pdf_document *doc, fz_output *out);

/* Internal call as part of loading a snapshot of a PDF document. */
void pdf_deserialise_journal(fz_context *ctx, pdf_document *doc, fz_stream *stm);

/* Internal call as part of creating objects. */
void pdf_add_journal_fragment(fz_context *ctx, pdf_document *doc, int parent, pdf_obj *copy, fz_buffer *copy_stream, int newobj);

char *pdf_format_date(fz_context *ctx, int64_t time, char *s, size_t n);
int64_t pdf_parse_date(fz_context *ctx, const char *s);

#endif
