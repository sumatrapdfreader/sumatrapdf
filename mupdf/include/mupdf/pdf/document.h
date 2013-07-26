#ifndef MUPDF_PDF_DOCUMENT_H
#define MUPDF_PDF_DOCUMENT_H

typedef struct pdf_lexbuf_s pdf_lexbuf;
typedef struct pdf_lexbuf_large_s pdf_lexbuf_large;
typedef struct pdf_xref_s pdf_xref;
typedef struct pdf_crypt_s pdf_crypt;
typedef struct pdf_ocg_descriptor_s pdf_ocg_descriptor;

typedef struct pdf_page_s pdf_page;
typedef struct pdf_annot_s pdf_annot;
typedef struct pdf_widget_s pdf_widget;
typedef struct pdf_hotspot_s pdf_hotspot;
typedef struct pdf_js_s pdf_js;

enum
{
	PDF_LEXBUF_SMALL = 256,
	PDF_LEXBUF_LARGE = 65536
};

struct pdf_lexbuf_s
{
	fz_context *ctx;
	int size;
	int base_size;
	int len;
	int i;
	float f;
	char *scratch;
	char buffer[PDF_LEXBUF_SMALL];
};

struct pdf_lexbuf_large_s
{
	pdf_lexbuf base;
	char buffer[PDF_LEXBUF_LARGE - PDF_LEXBUF_SMALL];
};

struct pdf_hotspot_s
{
	int num;
	int gen;
	int state;
};

/*
	Document event structures are mostly opaque to the app. Only the type
	is visible to the app.
*/
typedef struct pdf_doc_event_s pdf_doc_event;

/*
	pdf_doc_event_cb: the type of function via which the app receives
	document events.
*/
typedef void (pdf_doc_event_cb)(pdf_doc_event *event, void *data);

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

pdf_document *pdf_open_document_no_run(fz_context *ctx, const char *filename);
pdf_document *pdf_open_document_no_run_with_stream(fz_context *ctx, fz_stream *file);

/*
	pdf_close_document: Closes and frees an opened PDF document.

	The resource store in the context associated with pdf_document
	is emptied.

	Does not throw exceptions.
*/
void pdf_close_document(pdf_document *doc);

/*
	pdf_specific: down-cast an fz_document to a pdf_document.
	Returns NULL if underlying document is not PDF
*/
pdf_document *pdf_specifics(fz_document *doc);

int pdf_needs_password(pdf_document *doc);
int pdf_authenticate_password(pdf_document *doc, const char *pw);

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

/*
	Metadata interface.
*/
int pdf_meta(pdf_document *doc, int key, void *ptr, int size);

fz_outline *pdf_load_outline(pdf_document *doc);

typedef struct pdf_ocg_entry_s pdf_ocg_entry;

struct pdf_ocg_entry_s
{
	int num;
	int gen;
	int state;
};

struct pdf_ocg_descriptor_s
{
	int len;
	pdf_ocg_entry *ocgs;
	pdf_obj *intent;
};

/*
	pdf_update_page: update a page for the sake of changes caused by a call
	to pdf_pass_event. pdf_update_page regenerates any appearance streams that
	are out of date, checks for cases where different appearance streams
	should be selected because of state changes, and records internally
	each annotation that has changed appearance. The list of changed annotations
	is then available via pdf_poll_changed_annot. Note that a call to
	pdf_pass_event for one page may lead to changes on any other, so an app
	should call pdf_update_page for every page it currently displays. Also
	it is important that the pdf_page object is the one used to last render
	the page. If instead the app were to drop the page and reload it then
	a call to pdf_update_page would not reliably be able to report all changed
	areas.
*/
void pdf_update_page(pdf_document *doc, pdf_page *page);

/*
	Determine whether changes have been made since the
	document was opened or last saved.
*/
int pdf_has_unsaved_changes(pdf_document *doc);

typedef struct pdf_obj_read_state_s pdf_obj_read_state;

struct
{
	int offset;
	int num;
	int numofs;
	int gen;
	int genofs;
}
pdf_obj_read_state_s;

struct pdf_document_s
{
	fz_document super;

	fz_context *ctx;
	fz_stream *file;

	int version;
	int startxref;
	int file_size;
	pdf_crypt *crypt;
	pdf_ocg_descriptor *ocg;
	pdf_hotspot hotspot;

	int num_xref_sections;
	pdf_xref *xref_sections;
	int xref_altered;
	int freeze_updates;

	int page_count;

	/* State indicating which file parsing method we are using */
	int file_reading_linearly;
	int file_length;

	pdf_obj *linear_obj; /* Linearized object (if used) */
	pdf_obj **linear_page_refs; /* Page objects for linear loading */
	int linear_page1_obj_num;

	/* The state for the pdf_progressive_advance parser */
	int linear_pos;
	int linear_page_num;

	int hint_object_offset;
	int hint_object_length;
	int hints_loaded; /* Set to 1 after the hints loading has completed,
			   * whether successful or not! */
	/* Page n references shared object references:
	 *   hint_shared_ref[i]
	 * where
	 *      i = s to e-1
	 *	s = hint_page[n]->index
	 *	e = hint_page[n+1]->index
	 * Shared object reference r accesses objects:
	 *   rs to re-1
	 * where
	 *   rs = hint_shared[r]->number
	 *   re = hint_shared[r]->count + rs
	 * These are guaranteed to lie within the region starting at
	 * hint_shared[r]->offset of length hint_shared[r]->length
	 */
	struct
	{
		int number; /* Page object number */
		int offset; /* Offset of page object */
		int index; /* Index into shared hint_shared_ref */
	} *hint_page;
	int *hint_shared_ref;
	struct
	{
		int number; /* Object number of first object */
		int offset; /* Offset of first object */
	} *hint_shared;
	int hint_obj_offsets_max;
	int *hint_obj_offsets;

	int resources_localised;

	pdf_lexbuf_large lexbuf;

	pdf_annot *focus;
	pdf_obj *focus_obj;

	pdf_js *js;
	int recalculating;
	int dirty;
	void (*update_appearance)(pdf_document *doc, pdf_annot *annot);

	pdf_doc_event_cb *event_cb;
	void *event_cb_data;

	int num_type3_fonts;
	int max_type3_fonts;
	fz_font **type3_fonts;
};

/*
	PDF creation
*/

/*
	pdf_create_document: Create a blank PDF document
*/
pdf_document *pdf_create_document(fz_context *ctx);

pdf_page *pdf_create_page(pdf_document *doc, fz_rect rect, int res, int rotate);

void pdf_insert_page(pdf_document *doc, pdf_page *page, int at);

void pdf_delete_page(pdf_document *doc, int number);

void pdf_delete_page_range(pdf_document *doc, int start, int end);

fz_device *pdf_page_write(pdf_document *doc, pdf_page *page);

void pdf_finish_edit(pdf_document *doc);

#endif
