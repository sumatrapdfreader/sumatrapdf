#ifndef MUPDF_PDF_OBJECT_H
#define MUPDF_PDF_OBJECT_H

typedef struct pdf_document_s pdf_document;

/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filters and the mupdf parser.
 */

typedef struct pdf_obj_s pdf_obj;

pdf_obj *pdf_new_null(fz_context *ctx);
pdf_obj *pdf_new_bool(fz_context *ctx, int b);
pdf_obj *pdf_new_int(fz_context *ctx, int i);
pdf_obj *pdf_new_real(fz_context *ctx, float f);
pdf_obj *pdf_new_name(fz_context *ctx, const char *str);
pdf_obj *pdf_new_string(fz_context *ctx, const char *str, int len);
pdf_obj *pdf_new_indirect(fz_context *ctx, int num, int gen, void *doc);
pdf_obj *pdf_new_array(fz_context *ctx, int initialcap);
pdf_obj *pdf_new_dict(fz_context *ctx, int initialcap);
pdf_obj *pdf_new_rect(fz_context *ctx, const fz_rect *rect);
pdf_obj *pdf_new_matrix(fz_context *ctx, const fz_matrix *mtx);
pdf_obj *pdf_copy_array(fz_context *ctx, pdf_obj *array);
pdf_obj *pdf_copy_dict(fz_context *ctx, pdf_obj *dict);

pdf_obj *pdf_new_obj_from_str(fz_context *ctx, const char *src);

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
int pdf_obj_mark(pdf_obj *obj);
void pdf_obj_unmark(pdf_obj *obj);

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
void pdf_array_insert(pdf_obj *array, pdf_obj *obj);
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

int pdf_fprint_obj(FILE *fp, pdf_obj *obj, int tight);

#ifndef NDEBUG
void pdf_print_obj(pdf_obj *obj);
void pdf_print_ref(pdf_obj *obj);
#endif

char *pdf_to_utf8(pdf_document *xref, pdf_obj *src);
unsigned short *pdf_to_ucs2(pdf_document *xref, pdf_obj *src);
pdf_obj *pdf_to_utf8_name(pdf_document *xref, pdf_obj *src);
char *pdf_from_ucs2(pdf_document *xref, unsigned short *str);
void pdf_to_ucs2_buf(unsigned short *buffer, pdf_obj *src);

fz_rect *pdf_to_rect(fz_context *ctx, pdf_obj *array, fz_rect *rect);
fz_matrix *pdf_to_matrix(fz_context *ctx, pdf_obj *array, fz_matrix *mat);

pdf_document *pdf_get_indirect_document(pdf_obj *obj);
void pdf_set_str_len(pdf_obj *obj, int newlen);
void pdf_set_int(pdf_obj *obj, int i);

#endif
