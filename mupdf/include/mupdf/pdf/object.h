#ifndef MUPDF_PDF_OBJECT_H
#define MUPDF_PDF_OBJECT_H

typedef struct pdf_document_s pdf_document;
typedef struct pdf_crypt_s pdf_crypt;

/* Defined in PDF 1.7 according to Acrobat limit. */
#define PDF_MAX_OBJECT_NUMBER 8388607
#define PDF_MAX_GEN_NUMBER 65535

/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filters and the mupdf parser.
 */

typedef struct pdf_obj_s pdf_obj;

pdf_obj *pdf_new_int(fz_context *ctx, int64_t i);
pdf_obj *pdf_new_real(fz_context *ctx, float f);
pdf_obj *pdf_new_name(fz_context *ctx, const char *str);
pdf_obj *pdf_new_string(fz_context *ctx, const char *str, size_t len);
pdf_obj *pdf_new_text_string(fz_context *ctx, const char *s);
pdf_obj *pdf_new_indirect(fz_context *ctx, pdf_document *doc, int num, int gen);
pdf_obj *pdf_new_array(fz_context *ctx, pdf_document *doc, int initialcap);
pdf_obj *pdf_new_dict(fz_context *ctx, pdf_document *doc, int initialcap);
pdf_obj *pdf_new_rect(fz_context *ctx, pdf_document *doc, fz_rect rect);
pdf_obj *pdf_new_matrix(fz_context *ctx, pdf_document *doc, fz_matrix mtx);
pdf_obj *pdf_copy_array(fz_context *ctx, pdf_obj *array);
pdf_obj *pdf_copy_dict(fz_context *ctx, pdf_obj *dict);
pdf_obj *pdf_deep_copy_obj(fz_context *ctx, pdf_obj *obj);

pdf_obj *pdf_keep_obj(fz_context *ctx, pdf_obj *obj);
void pdf_drop_obj(fz_context *ctx, pdf_obj *obj);

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
int pdf_obj_num_is_stream(fz_context *ctx, pdf_document *doc, int num);
int pdf_is_stream(fz_context *ctx, pdf_obj *obj);
pdf_obj *pdf_resolve_obj(fz_context *ctx, pdf_obj *a);
int pdf_objcmp(fz_context *ctx, pdf_obj *a, pdf_obj *b);
int pdf_objcmp_resolve(fz_context *ctx, pdf_obj *a, pdf_obj *b);
int pdf_name_eq(fz_context *ctx, pdf_obj *a, pdf_obj *b);

int pdf_obj_marked(fz_context *ctx, pdf_obj *obj);
int pdf_mark_obj(fz_context *ctx, pdf_obj *obj);
void pdf_unmark_obj(fz_context *ctx, pdf_obj *obj);

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
pdf_obj *pdf_dict_put_array(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int initial);
pdf_obj *pdf_dict_put_dict(fz_context *ctx, pdf_obj *dict, pdf_obj *key, int initial);
pdf_obj *pdf_dict_puts_dict(fz_context *ctx, pdf_obj *dict, const char *key, int initial);

int pdf_dict_get_bool(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
int pdf_dict_get_int(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
float pdf_dict_get_real(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
const char *pdf_dict_get_name(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
const char *pdf_dict_get_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key, size_t *sizep);
const char *pdf_dict_get_text_string(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
fz_rect pdf_dict_get_rect(fz_context *ctx, pdf_obj *dict, pdf_obj *key);
fz_matrix pdf_dict_get_matrix(fz_context *ctx, pdf_obj *dict, pdf_obj *key);

void pdf_array_push_bool(fz_context *ctx, pdf_obj *array, int x);
void pdf_array_push_int(fz_context *ctx, pdf_obj *array, int64_t x);
void pdf_array_push_real(fz_context *ctx, pdf_obj *array, double x);
void pdf_array_push_name(fz_context *ctx, pdf_obj *array, const char *x);
void pdf_array_push_string(fz_context *ctx, pdf_obj *array, const char *x, size_t n);
void pdf_array_push_text_string(fz_context *ctx, pdf_obj *array, const char *x);
pdf_obj *pdf_array_push_array(fz_context *ctx, pdf_obj *array, int initial);
pdf_obj *pdf_array_push_dict(fz_context *ctx, pdf_obj *array, int initial);

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

char *pdf_new_utf8_from_pdf_string(fz_context *ctx, const char *srcptr, size_t srclen);
char *pdf_new_utf8_from_pdf_string_obj(fz_context *ctx, pdf_obj *src);
char *pdf_new_utf8_from_pdf_stream_obj(fz_context *ctx, pdf_obj *src);
char *pdf_load_stream_or_string_as_utf8(fz_context *ctx, pdf_obj *src);

fz_quad pdf_to_quad(fz_context *ctx, pdf_obj *array, int offset);
fz_rect pdf_to_rect(fz_context *ctx, pdf_obj *array);
fz_matrix pdf_to_matrix(fz_context *ctx, pdf_obj *array);

pdf_document *pdf_get_indirect_document(fz_context *ctx, pdf_obj *obj);
pdf_document *pdf_get_bound_document(fz_context *ctx, pdf_obj *obj);
void pdf_set_str_len(fz_context *ctx, pdf_obj *obj, size_t newlen);
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

#endif
