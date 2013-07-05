#ifndef MUPDF_PDF_OBJECT_H
#define MUPDF_PDF_OBJECT_H

typedef struct pdf_document_s pdf_document;

/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filters and the mupdf parser.
 */

typedef struct pdf_obj_s pdf_obj;

pdf_obj *pdf_new_null(pdf_document *doc);
pdf_obj *pdf_new_bool(pdf_document *doc, int b);
pdf_obj *pdf_new_int(pdf_document *doc, int i);
pdf_obj *pdf_new_real(pdf_document *doc, float f);
pdf_obj *pdf_new_name(pdf_document *doc, const char *str);
pdf_obj *pdf_new_string(pdf_document *doc, const char *str, int len);
pdf_obj *pdf_new_indirect(pdf_document *doc, int num, int gen);
pdf_obj *pdf_new_array(pdf_document *doc, int initialcap);
pdf_obj *pdf_new_dict(pdf_document *doc, int initialcap);
pdf_obj *pdf_new_rect(pdf_document *doc, const fz_rect *rect);
pdf_obj *pdf_new_matrix(pdf_document *doc, const fz_matrix *mtx);
pdf_obj *pdf_copy_array(pdf_obj *array);
pdf_obj *pdf_copy_dict(pdf_obj *dict);

pdf_obj *pdf_new_obj_from_str(pdf_document *doc, const char *src);

pdf_obj *pdf_keep_obj(pdf_obj *obj);
void pdf_drop_obj(pdf_obj *obj);

/* type queries */
int pdf_is_null(pdf_obj *obj);
int pdf_is_bool(pdf_obj *obj);
int pdf_is_int(pdf_obj *obj);
int pdf_is_real(pdf_obj *obj);
int pdf_is_name(pdf_obj *obj);
int pdf_is_string(pdf_obj *obj);
int pdf_is_array(pdf_obj *obj);
int pdf_is_dict(pdf_obj *obj);
int pdf_is_indirect(pdf_obj *obj);
int pdf_is_stream(pdf_document *doc, int num, int gen);

int pdf_objcmp(pdf_obj *a, pdf_obj *b);

/* obj marking and unmarking functions - to avoid infinite recursions. */
int pdf_obj_marked(pdf_obj *obj);
int pdf_mark_obj(pdf_obj *obj);
void pdf_unmark_obj(pdf_obj *obj);

/* obj memo functions - allows us to secretly remember "a memo" (a bool) in
 * an object, and to read back whether there was a memo, and if so, what it
 * was. */
void pdf_set_obj_memo(pdf_obj *obj, int memo);
int pdf_obj_memo(pdf_obj *obj, int *memo);

/* obj dirty bit support. */
int pdf_obj_is_dirty(pdf_obj *obj);
void pdf_dirty_obj(pdf_obj *obj);
void pdf_clean_obj(pdf_obj *obj);

/* safe, silent failure, no error reporting on type mismatches */
int pdf_to_bool(pdf_obj *obj);
int pdf_to_int(pdf_obj *obj);
float pdf_to_real(pdf_obj *obj);
char *pdf_to_name(pdf_obj *obj);
char *pdf_to_str_buf(pdf_obj *obj);
pdf_obj *pdf_to_dict(pdf_obj *obj);
int pdf_to_str_len(pdf_obj *obj);
int pdf_to_num(pdf_obj *obj);
int pdf_to_gen(pdf_obj *obj);

int pdf_array_len(pdf_obj *array);
pdf_obj *pdf_array_get(pdf_obj *array, int i);
void pdf_array_put(pdf_obj *array, int i, pdf_obj *obj);
void pdf_array_push(pdf_obj *array, pdf_obj *obj);
void pdf_array_push_drop(pdf_obj *array, pdf_obj *obj);
void pdf_array_insert(pdf_obj *array, pdf_obj *obj, int index);
void pdf_array_insert_drop(pdf_obj *array, pdf_obj *obj, int index);
void pdf_array_delete(pdf_obj *array, int index);
int pdf_array_contains(pdf_obj *array, pdf_obj *obj);

int pdf_dict_len(pdf_obj *dict);
pdf_obj *pdf_dict_get_key(pdf_obj *dict, int idx);
pdf_obj *pdf_dict_get_val(pdf_obj *dict, int idx);
pdf_obj *pdf_dict_get(pdf_obj *dict, pdf_obj *key);
pdf_obj *pdf_dict_gets(pdf_obj *dict, const char *key);
pdf_obj *pdf_dict_getp(pdf_obj *dict, const char *key);
pdf_obj *pdf_dict_getsa(pdf_obj *dict, const char *key, const char *abbrev);
void pdf_dict_put(pdf_obj *dict, pdf_obj *key, pdf_obj *val);
void pdf_dict_puts(pdf_obj *dict, const char *key, pdf_obj *val);
void pdf_dict_puts_drop(pdf_obj *dict, const char *key, pdf_obj *val);
void pdf_dict_putp(pdf_obj *dict, const char *key, pdf_obj *val);
void pdf_dict_putp_drop(pdf_obj *dict, const char *key, pdf_obj *val);
void pdf_dict_del(pdf_obj *dict, pdf_obj *key);
void pdf_dict_dels(pdf_obj *dict, const char *key);
void pdf_sort_dict(pdf_obj *dict);

/*
	Recurse through the object structure setting the node's parent_num to num.
	parent_num is used when a subobject is to be changed during a document edit.
	The whole containing hierarchy is moved to the incremental xref section, so
	to be later written out as an incremental file update.
*/
void pdf_set_obj_parent(pdf_obj *obj, int num);

int pdf_fprint_obj(FILE *fp, pdf_obj *obj, int tight);

#ifndef NDEBUG
void pdf_print_obj(pdf_obj *obj);
void pdf_print_ref(pdf_obj *obj);
#endif

char *pdf_to_utf8(pdf_document *doc, pdf_obj *src);
unsigned short *pdf_to_ucs2(pdf_document *doc, pdf_obj *src);
pdf_obj *pdf_to_utf8_name(pdf_document *doc, pdf_obj *src);
char *pdf_from_ucs2(pdf_document *doc, unsigned short *str);
void pdf_to_ucs2_buf(unsigned short *buffer, pdf_obj *src);

fz_rect *pdf_to_rect(fz_context *ctx, pdf_obj *array, fz_rect *rect);
fz_matrix *pdf_to_matrix(fz_context *ctx, pdf_obj *array, fz_matrix *mat);

pdf_document *pdf_get_indirect_document(pdf_obj *obj);
void pdf_set_str_len(pdf_obj *obj, int newlen);
void pdf_set_int(pdf_obj *obj, int i);

#endif
