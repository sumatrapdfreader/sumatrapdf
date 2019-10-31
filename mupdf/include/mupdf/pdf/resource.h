#ifndef MUPDF_PDF_RESOURCE_H
#define MUPDF_PDF_RESOURCE_H

void pdf_store_item(fz_context *ctx, pdf_obj *key, void *val, size_t itemsize);
void *pdf_find_item(fz_context *ctx, fz_store_drop_fn *drop, pdf_obj *key);
void pdf_remove_item(fz_context *ctx, fz_store_drop_fn *drop, pdf_obj *key);
void pdf_empty_store(fz_context *ctx, pdf_document *doc);

/*
 * Structures used for managing resource locations and avoiding multiple
 * occurrences when resources are added to the document. The search for existing
 * resources will be performed when we are first trying to add an item. Object
 * refs are stored in a fz_hash_table structure using a hash of the md5 sum of
 * the data, enabling rapid lookup.
 */

enum { PDF_SIMPLE_FONT_RESOURCE=1, PDF_CID_FONT_RESOURCE=2, PDF_CJK_FONT_RESOURCE=3 };
enum { PDF_SIMPLE_ENCODING_LATIN, PDF_SIMPLE_ENCODING_GREEK, PDF_SIMPLE_ENCODING_CYRILLIC };

pdf_obj *pdf_find_font_resource(fz_context *ctx, pdf_document *doc, int type, int encoding, fz_font *item, unsigned char md5[16]);
pdf_obj *pdf_insert_font_resource(fz_context *ctx, pdf_document *doc, unsigned char md5[16], pdf_obj *obj);
pdf_obj *pdf_find_image_resource(fz_context *ctx, pdf_document *doc, fz_image *item, unsigned char md5[16]);
pdf_obj *pdf_insert_image_resource(fz_context *ctx, pdf_document *doc, unsigned char md5[16], pdf_obj *obj);
void pdf_drop_resource_tables(fz_context *ctx, pdf_document *doc);

typedef struct pdf_function_s pdf_function;

void pdf_eval_function(fz_context *ctx, pdf_function *func, const float *in, int inlen, float *out, int outlen);
pdf_function *pdf_keep_function(fz_context *ctx, pdf_function *func);
void pdf_drop_function(fz_context *ctx, pdf_function *func);
size_t pdf_function_size(fz_context *ctx, pdf_function *func);
pdf_function *pdf_load_function(fz_context *ctx, pdf_obj *ref, int in, int out);

fz_colorspace *pdf_document_output_intent(fz_context *ctx, pdf_document *doc);
fz_colorspace *pdf_load_colorspace(fz_context *ctx, pdf_obj *obj);
int pdf_is_tint_colorspace(fz_context *ctx, fz_colorspace *cs);

fz_shade *pdf_load_shading(fz_context *ctx, pdf_document *doc, pdf_obj *obj);

fz_image *pdf_load_inline_image(fz_context *ctx, pdf_document *doc, pdf_obj *rdb, pdf_obj *dict, fz_stream *file);
int pdf_is_jpx_image(fz_context *ctx, pdf_obj *dict);

fz_image *pdf_load_image(fz_context *ctx, pdf_document *doc, pdf_obj *obj);

pdf_obj *pdf_add_image(fz_context *ctx, pdf_document *doc, fz_image *image);

typedef struct pdf_pattern_s pdf_pattern;

struct pdf_pattern_s
{
	fz_storable storable;
	int ismask;
	float xstep;
	float ystep;
	fz_matrix matrix;
	fz_rect bbox;
	pdf_document *document;
	pdf_obj *resources;
	pdf_obj *contents;
	int id; /* unique ID for caching rendered tiles */
};

pdf_pattern *pdf_load_pattern(fz_context *ctx, pdf_document *doc, pdf_obj *obj);
pdf_pattern *pdf_keep_pattern(fz_context *ctx, pdf_pattern *pat);
void pdf_drop_pattern(fz_context *ctx, pdf_pattern *pat);

pdf_obj *pdf_new_xobject(fz_context *ctx, pdf_document *doc, fz_rect bbox, fz_matrix matrix, pdf_obj *res, fz_buffer *buffer);
void pdf_update_xobject(fz_context *ctx, pdf_document *doc, pdf_obj *xobj, fz_rect bbox, fz_matrix mat, pdf_obj *res, fz_buffer *buffer);

pdf_obj *pdf_xobject_resources(fz_context *ctx, pdf_obj *xobj);
fz_rect pdf_xobject_bbox(fz_context *ctx, pdf_obj *xobj);
fz_matrix pdf_xobject_matrix(fz_context *ctx, pdf_obj *xobj);
int pdf_xobject_isolated(fz_context *ctx, pdf_obj *xobj);
int pdf_xobject_knockout(fz_context *ctx, pdf_obj *xobj);
int pdf_xobject_transparency(fz_context *ctx, pdf_obj *xobj);
fz_colorspace *pdf_xobject_colorspace(fz_context *ctx, pdf_obj *xobj);

#endif
