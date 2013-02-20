#ifndef MUPDF_H
#define MUPDF_H

#include "fitz.h"

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
unsigned short *pdf_to_ucs2(pdf_document *xref, pdf_obj *src); /* sumatrapdf */
pdf_obj *pdf_to_utf8_name(pdf_document *xref, pdf_obj *src);
char *pdf_from_ucs2(pdf_document *xref, unsigned short *str);
void pdf_to_ucs2_buf(unsigned short *buffer, pdf_obj *src);

fz_rect *pdf_to_rect(fz_context *ctx, pdf_obj *array, fz_rect *rect);
fz_matrix *pdf_to_matrix(fz_context *ctx, pdf_obj *array, fz_matrix *mat);

int pdf_count_objects(pdf_document *doc);
pdf_obj *pdf_resolve_indirect(pdf_obj *ref);
pdf_obj *pdf_load_object(pdf_document *doc, int num, int gen);

fz_buffer *pdf_load_raw_stream(pdf_document *doc, int num, int gen);
fz_buffer *pdf_load_stream(pdf_document *doc, int num, int gen);
fz_stream *pdf_open_raw_stream(pdf_document *doc, int num, int gen);
fz_stream *pdf_open_stream(pdf_document *doc, int num, int gen);

fz_image *pdf_load_image(pdf_document *doc, pdf_obj *obj);

fz_outline *pdf_load_outline(pdf_document *doc);

/*
	pdf_create_object: Allocate a slot in the xref table and return a fresh unused object number.
*/
int pdf_create_object(pdf_document *xref);

/*
	pdf_delete_object: Remove object from xref table, marking the slot as free.
*/
void pdf_delete_object(pdf_document *xref, int num);

/*
	pdf_update_object: Replace object in xref table with the passed in object.
*/
void pdf_update_object(pdf_document *xref, int num, pdf_obj *obj);

/*
	pdf_update_stream: Replace stream contents for object in xref table with the passed in buffer.

	The buffer contents must match the /Filter setting.
	If storing uncompressed data, make sure to delete the /Filter key from
	the stream dictionary. If storing deflated data, make sure to set the
	/Filter value to /FlateDecode.
*/
void pdf_update_stream(pdf_document *xref, int num, fz_buffer *buf);

/*
	pdf_new_pdf_device: Create a pdf device. Rendering to the device creates
	new pdf content. WARNING: this device is work in progress. It doesn't
	currently support all rendering cases.
*/
fz_device *pdf_new_pdf_device(pdf_document *doc, pdf_obj *contents, pdf_obj *resources, const fz_matrix *ctm);

/*
	pdf_write_document: Write out the document to a file with all changes finalised.
*/
void pdf_write_document(pdf_document *doc, char *filename, fz_write_options *opts);

/*
	pdf_open_document: Open a PDF document.

	Open a PDF document by reading its cross reference table, so
	MuPDF can locate PDF objects inside the file. Upon an broken
	cross reference table or other parse errors MuPDF will restart
	parsing the file from the beginning to try to rebuild a
	(hopefully correct) cross reference table to allow further
	processing of the file.

	The returned pdf_document should be used when calling most
	other PDF functions. Note that it wraps the context, so those
	functions implicitly get access to the global state in
	context.

	filename: a path to a file as it would be given to open(2).
*/
pdf_document *pdf_open_document(fz_context *ctx, const char *filename);

/*
	pdf_open_document_with_stream: Opens a PDF document.

	Same as pdf_open_document, but takes a stream instead of a
	filename to locate the PDF document to open. Increments the
	reference count of the stream. See fz_open_file,
	fz_open_file_w or fz_open_fd for opening a stream, and
	fz_close for closing an open stream.
*/
pdf_document *pdf_open_document_with_stream(fz_context *ctx, fz_stream *file);

/*
	pdf_close_document: Closes and frees an opened PDF document.

	The resource store in the context associated with pdf_document
	is emptied.

	Does not throw exceptions.
*/
void pdf_close_document(pdf_document *doc);

int pdf_needs_password(pdf_document *doc);
int pdf_authenticate_password(pdf_document *doc, char *pw);

enum
{
	PDF_PERM_PRINT = 1 << 2,
	PDF_PERM_CHANGE = 1 << 3,
	PDF_PERM_COPY = 1 << 4,
	PDF_PERM_NOTES = 1 << 5,
	PDF_PERM_FILL_FORM = 1 << 8,
	PDF_PERM_ACCESSIBILITY = 1 << 9,
	PDF_PERM_ASSEMBLE = 1 << 10,
	PDF_PERM_HIGH_RES_PRINT = 1 << 11,
	PDF_DEFAULT_PERM_FLAGS = 0xfffc
};

int pdf_has_permission(pdf_document *doc, int p);

typedef struct pdf_page_s pdf_page;

int pdf_lookup_page_number(pdf_document *doc, pdf_obj *pageobj);
int pdf_count_pages(pdf_document *doc);

/*
	pdf_load_page: Load a page and its resources.

	Locates the page in the PDF document and loads the page and its
	resources. After pdf_load_page is it possible to retrieve the size
	of the page using pdf_bound_page, or to render the page using
	pdf_run_page_*.

	number: page number, where 0 is the first page of the document.
*/
pdf_page *pdf_load_page(pdf_document *doc, int number);

fz_link *pdf_load_links(pdf_document *doc, pdf_page *page);

/*
	pdf_bound_page: Determine the size of a page.

	Determine the page size in user space units, taking page rotation
	into account. The page size is taken to be the crop box if it
	exists (visible area after cropping), otherwise the media box will
	be used (possibly including printing marks).

	Does not throw exceptions.
*/
fz_rect *pdf_bound_page(pdf_document *doc, pdf_page *page, fz_rect *);

/*
	pdf_free_page: Frees a page and its resources.

	Does not throw exceptions.
*/
void pdf_free_page(pdf_document *doc, pdf_page *page);

typedef struct pdf_annot_s pdf_annot;

/*
	pdf_first_annot: Return the first annotation on a page.

	Does not throw exceptions.
*/
pdf_annot *pdf_first_annot(pdf_document *doc, pdf_page *page);

/*
	pdf_next_annot: Return the next annotation on a page.

	Does not throw exceptions.
*/
pdf_annot *pdf_next_annot(pdf_document *doc, pdf_annot *annot);

/*
	pdf_bound_annot: Return the rectangle for an annotation on a page.

	Does not throw exceptions.
*/
fz_rect *pdf_bound_annot(pdf_document *doc, pdf_annot *annot, fz_rect *rect);

/*
	pdf_run_page: Interpret a loaded page and render it on a device.

	page: A page loaded by pdf_load_page.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_page(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

void pdf_run_page_with_usage(pdf_document *doc, pdf_page *page, fz_device *dev, const fz_matrix *ctm, char *event, fz_cookie *cookie);

/*
	pdf_run_page_contents: Interpret a loaded page and render it on a device.
	Just the main page contents without the annotations

	page: A page loaded by pdf_load_page.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_page_contents(pdf_document *xref, pdf_page *page, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

/*
	pdf_run_annot: Interpret an annotation and render it on a device.

	page: A page loaded by pdf_load_page.

	annot: an annotation.

	dev: Device used for rendering, obtained from fz_new_*_device.

	ctm: A transformation matrix applied to the objects on the page,
	e.g. to scale or rotate the page contents as desired.
*/
void pdf_run_annot(pdf_document *xref, pdf_page *page, pdf_annot *annot, fz_device *dev, const fz_matrix *ctm, fz_cookie *cookie);

/*
	Metadata interface.
*/
int pdf_meta(pdf_document *doc, int key, void *ptr, int size);

/*
	Presentation interface.
*/
fz_transition *pdf_page_presentation(pdf_document *doc, pdf_page *page, float *duration);

#endif
