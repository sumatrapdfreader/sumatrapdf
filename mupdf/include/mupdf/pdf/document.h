#ifndef MUPDF_PDF_DOCUMENT_H
#define MUPDF_PDF_DOCUMENT_H

typedef struct pdf_xref pdf_xref;
typedef struct pdf_ocg_descriptor pdf_ocg_descriptor;

typedef struct pdf_page pdf_page;
typedef struct pdf_annot pdf_annot;
typedef struct pdf_annot pdf_widget;
typedef struct pdf_js pdf_js;

enum
{
	PDF_LEXBUF_SMALL = 256,
	PDF_LEXBUF_LARGE = 65536
};

typedef struct
{
	size_t size;
	size_t base_size;
	size_t len;
	int64_t i;
	float f;
	char *scratch;
	char buffer[PDF_LEXBUF_SMALL];
} pdf_lexbuf;

typedef struct
{
	pdf_lexbuf base;
	char buffer[PDF_LEXBUF_LARGE - PDF_LEXBUF_SMALL];
} pdf_lexbuf_large;

/*
	Document event structures are mostly opaque to the app. Only the type
	is visible to the app.
*/
typedef struct pdf_doc_event pdf_doc_event;

/*
	the type of function via which the app receives
	document events.
*/
typedef void (pdf_doc_event_cb)(fz_context *ctx, pdf_document *doc, pdf_doc_event *event, void *data);

/*
	Open a PDF document.

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
	Opens a PDF document.

	Same as pdf_open_document, but takes a stream instead of a
	filename to locate the PDF document to open. Increments the
	reference count of the stream. See fz_open_file,
	fz_open_file_w or fz_open_fd for opening a stream, and
	fz_drop_stream for closing an open stream.
*/
pdf_document *pdf_open_document_with_stream(fz_context *ctx, fz_stream *file);

/*
	Closes and frees an opened PDF document.

	The resource store in the context associated with pdf_document
	is emptied.
*/
void pdf_drop_document(fz_context *ctx, pdf_document *doc);

pdf_document *pdf_keep_document(fz_context *ctx, pdf_document *doc);

/*
	down-cast a fz_document to a pdf_document.
	Returns NULL if underlying document is not PDF
*/
pdf_document *pdf_specifics(fz_context *ctx, fz_document *doc);

/*
	Down-cast generic fitz objects into pdf specific variants.
	Returns NULL if the objects are not from a PDF document.
*/
pdf_document *pdf_document_from_fz_document(fz_context *ctx, fz_document *ptr);
pdf_page *pdf_page_from_fz_page(fz_context *ctx, fz_page *ptr);

int pdf_needs_password(fz_context *ctx, pdf_document *doc);

/*
	Attempt to authenticate a
	password.

	Returns 0 for failure, non-zero for success.

	In the non-zero case:
		bit 0 set => no password required
		bit 1 set => user password authenticated
		bit 2 set => owner password authenticated
*/
int pdf_authenticate_password(fz_context *ctx, pdf_document *doc, const char *pw);

int pdf_has_permission(fz_context *ctx, pdf_document *doc, fz_permission p);
int pdf_lookup_metadata(fz_context *ctx, pdf_document *doc, const char *key, char *ptr, int size);

fz_outline *pdf_load_outline(fz_context *ctx, pdf_document *doc);

/*
	Get the number of layer configurations defined in this document.

	doc: The document in question.
*/
int pdf_count_layer_configs(fz_context *ctx, pdf_document *doc);

void pdf_invalidate_xfa(fz_context *ctx, pdf_document *doc);


typedef struct
{
	const char *name;
	const char *creator;
} pdf_layer_config;

/*
	Fetch the name (and optionally creator) of the given layer config.

	doc: The document in question.

	config_num: A value in the 0..n-1 range, where n is the
	value returned from pdf_count_layer_configs.

	info: Pointer to structure to fill in. Pointers within
	this structure may be set to NULL if no information is
	available.
*/
void pdf_layer_config_info(fz_context *ctx, pdf_document *doc, int config_num, pdf_layer_config *info);

/*
	Set the current configuration.
	This updates the visibility of the optional content groups
	within the document.

	doc: The document in question.

	config_num: A value in the 0..n-1 range, where n is the
	value returned from pdf_count_layer_configs.
*/
void pdf_select_layer_config(fz_context *ctx, pdf_document *doc, int config_num);

/*
	Returns the number of entries in the 'UI' for this layer configuration.

	doc: The document in question.
*/
int pdf_count_layer_config_ui(fz_context *ctx, pdf_document *doc);

/*
	Select a checkbox/radiobox within the 'UI' for this layer
	configuration.

	Selecting a UI entry that is a radiobox may disable
	other UI entries.

	doc: The document in question.

	ui: A value in the 0..m-1 range, where m is the value
	returned by pdf_count_layer_config_ui.
*/
void pdf_select_layer_config_ui(fz_context *ctx, pdf_document *doc, int ui);

/*
	Select a checkbox/radiobox within the 'UI' for this layer configuration.

	doc: The document in question.

	ui: A value in the 0..m-1 range, where m is the value
	returned by pdf_count_layer_config_ui.
*/
void pdf_deselect_layer_config_ui(fz_context *ctx, pdf_document *doc, int ui);

/*
	Toggle a checkbox/radiobox within the 'UI' for this layer configuration.

	Toggling a UI entry that is a radiobox may disable
	other UI entries.

	doc: The document in question.

	ui: A value in the 0..m-1 range, where m is the value
	returned by pdf_count_layer_config_ui.
*/
void pdf_toggle_layer_config_ui(fz_context *ctx, pdf_document *doc, int ui);

typedef enum
{
	PDF_LAYER_UI_LABEL = 0,
	PDF_LAYER_UI_CHECKBOX = 1,
	PDF_LAYER_UI_RADIOBOX = 2
} pdf_layer_config_ui_type;

typedef struct
{
	const char *text;
	int depth;
	pdf_layer_config_ui_type type;
	int selected;
	int locked;
} pdf_layer_config_ui;

/*
	Get the info for a given entry in the layer config ui.

	doc: The document in question.

	ui: A value in the 0..m-1 range, where m is the value
	returned by pdf_count_layer_config_ui.

	info: Pointer to a structure to fill in with information
	about the requested ui entry.
*/
void pdf_layer_config_ui_info(fz_context *ctx, pdf_document *doc, int ui, pdf_layer_config_ui *info);

/*
	Write the current layer config back into the document as the default state.
*/
void pdf_set_layer_config_as_default(fz_context *ctx, pdf_document *doc);

/*
	Determine whether changes have been made since the
	document was opened or last saved.
*/
int pdf_has_unsaved_changes(fz_context *ctx, pdf_document *doc);

/*
	Determine if this PDF has been repaired since opening.
*/
int pdf_was_repaired(fz_context *ctx, pdf_document *doc);

/* Object that can perform the cryptographic operation necessary for document signing */
typedef struct pdf_pkcs7_signer pdf_pkcs7_signer;

/* Unsaved signature fields */
typedef struct pdf_unsaved_sig
{
	pdf_obj *field;
	size_t byte_range_start;
	size_t byte_range_end;
	size_t contents_start;
	size_t contents_end;
	pdf_pkcs7_signer *signer;
	struct pdf_unsaved_sig *next;
} pdf_unsaved_sig;

typedef struct
{
	int page;
	int object;
} pdf_rev_page_map;

typedef struct
{
	int number; /* Page object number */
	int64_t offset; /* Offset of page object */
	int64_t index; /* Index into shared hint_shared_ref */
} pdf_hint_page;

typedef struct
{
	int number; /* Object number of first object */
	int64_t offset; /* Offset of first object */
} pdf_hint_shared;

struct pdf_document
{
	fz_document super;

	fz_stream *file;

	int version;
	int64_t startxref;
	int64_t file_size;
	pdf_crypt *crypt;
	pdf_ocg_descriptor *ocg;
	fz_colorspace *oi;

	int max_xref_len;
	int num_xref_sections;
	int saved_num_xref_sections;
	int num_incremental_sections;
	int xref_base;
	int disallow_new_increments;

	/* The local_xref is only active, if local_xref_nesting >= 0 */
	pdf_xref *local_xref;
	int local_xref_nesting;

	pdf_xref *xref_sections;
	pdf_xref *saved_xref_sections;
	int *xref_index;
	int save_in_progress;
	int has_xref_streams;
	int has_old_style_xrefs;
	int has_linearization_object;

	int rev_page_count;
	pdf_rev_page_map *rev_page_map;

	int repair_attempted;

	/* State indicating which file parsing method we are using */
	int file_reading_linearly;
	int64_t file_length;

	int linear_page_count;
	pdf_obj *linear_obj; /* Linearized object (if used) */
	pdf_obj **linear_page_refs; /* Page objects for linear loading */
	int linear_page1_obj_num;

	/* The state for the pdf_progressive_advance parser */
	int64_t linear_pos;
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
	pdf_hint_page *hint_page;
	int *hint_shared_ref;
	pdf_hint_shared *hint_shared;
	int hint_obj_offsets_max;
	int64_t *hint_obj_offsets;

	int resources_localised;

	pdf_lexbuf_large lexbuf;

	pdf_js *js;

	int recalculate;
	int dirty;
	int redacted;

	pdf_doc_event_cb *event_cb;
	void *event_cb_data;

	int num_type3_fonts;
	int max_type3_fonts;
	fz_font **type3_fonts;

	struct {
		fz_hash_table *fonts;
	} resources;

	int orphans_max;
	int orphans_count;
	pdf_obj **orphans;

	fz_xml_doc *xfa;

	pdf_journal *journal;
};

pdf_document *pdf_create_document(fz_context *ctx);

typedef struct pdf_graft_map pdf_graft_map;

/*
	Return a deep copied object equivalent to the
	supplied object, suitable for use within the given document.

	dst: The document in which the returned object is to be used.

	obj: The object deep copy.

	Note: If grafting multiple objects, you should use a pdf_graft_map
	to avoid potential duplication of target objects.
*/
pdf_obj *pdf_graft_object(fz_context *ctx, pdf_document *dst, pdf_obj *obj);

/*
	Prepare a graft map object to allow objects
	to be deep copied from one document to the given one, avoiding
	problems with duplicated child objects.

	dst: The document to copy objects to.

	Note: all the source objects must come from the same document.
*/
pdf_graft_map *pdf_new_graft_map(fz_context *ctx, pdf_document *dst);

pdf_graft_map *pdf_keep_graft_map(fz_context *ctx, pdf_graft_map *map);
void pdf_drop_graft_map(fz_context *ctx, pdf_graft_map *map);

/*
	Return a deep copied object equivalent
	to the supplied object, suitable for use within the target
	document of the map.

	map: A map targeted at the document in which the returned
	object is to be used.

	obj: The object to be copied.

	Note: Copying multiple objects via the same graft map ensures
	that any shared children are not copied more than once.
*/
pdf_obj *pdf_graft_mapped_object(fz_context *ctx, pdf_graft_map *map, pdf_obj *obj);

/*
	Graft a page (and its resources) from the src document to the
	destination document of the graft. This involves a deep copy
	of the objects in question.

	map: A map targetted at the document into which the page should
	be inserted.

	page_to: The position within the destination document at which
	the page should be inserted (pages numbered from 0, with -1
	meaning "at the end").

	src: The document from which the page should be copied.

	page_from: The page number which should be copied from the src
	document (pages numbered from 0, with -1 meaning "at the end").
*/
void pdf_graft_page(fz_context *ctx, pdf_document *dst, int page_to, pdf_document *src, int page_from);
void pdf_graft_mapped_page(fz_context *ctx, pdf_graft_map *map, int page_to, pdf_document *src, int page_from);

/*
	Create a device that will record the
	graphical operations given to it into a sequence of
	pdf operations, together with a set of resources. This
	sequence/set pair can then be used as the basis for
	adding a page to the document (see pdf_add_page).

	doc: The document for which these are intended.

	mediabox: The bbox for the created page.

	presources: Pointer to a place to put the created
	resources dictionary.

	pcontents: Pointer to a place to put the created
	contents buffer.
*/
fz_device *pdf_page_write(fz_context *ctx, pdf_document *doc, fz_rect mediabox, pdf_obj **presources, fz_buffer **pcontents);

/*
	Create a pdf_obj within a document that
	represents a page, from a previously created resources
	dictionary and page content stream. This should then be
	inserted into the document using pdf_insert_page.

	After this call the page exists within the document
	structure, but is not actually ever displayed as it is
	not linked into the PDF page tree.

	doc: The document to which to add the page.

	mediabox: The mediabox for the page (should be identical
	to that used when creating the resources/contents).

	rotate: 0, 90, 180 or 270. The rotation to use for the
	page.

	resources: The resources dictionary for the new page
	(typically created by pdf_page_write).

	contents: The page contents for the new page (typically
	create by pdf_page_write).
*/
pdf_obj *pdf_add_page(fz_context *ctx, pdf_document *doc, fz_rect mediabox, int rotate, pdf_obj *resources, fz_buffer *contents);

/*
	Insert a page previously created by
	pdf_add_page into the pages tree of the document.

	doc: The document to insert into.

	at: The page number to insert at. 0 inserts at the start.
	negative numbers, or INT_MAX insert at the end. Otherwise
	n inserts after page n.

	page: The page to insert.
*/
void pdf_insert_page(fz_context *ctx, pdf_document *doc, int at, pdf_obj *page);

/*
	Delete a page from the page tree of
	a document. This does not remove the page contents
	or resources from the file.

	doc: The document to operate on.

	number: The page to remove (numbered from 0)
*/
void pdf_delete_page(fz_context *ctx, pdf_document *doc, int number);

/*
	Delete a range of pages from the
	page tree of a document. This does not remove the page
	contents or resources from the file.

	doc: The document to operate on.

	start, end: The range of pages (numbered from 0)
	(inclusive, exclusive) to remove. If end is negative or
	greater than the number of pages in the document, it
	will be taken to be the end of the document.
*/
void pdf_delete_page_range(fz_context *ctx, pdf_document *doc, int start, int end);

fz_text_language pdf_document_language(fz_context *ctx, pdf_document *doc);
void pdf_set_document_language(fz_context *ctx, pdf_document *doc, fz_text_language lang);

/*
	In calls to fz_save_document, the following options structure can be used
	to control aspects of the writing process. This structure may grow
	in the future, and should be zero-filled to allow forwards compatibility.
*/
typedef struct
{
	int do_incremental; /* Write just the changed objects. */
	int do_pretty; /* Pretty-print dictionaries and arrays. */
	int do_ascii; /* ASCII hex encode binary streams. */
	int do_compress; /* Compress streams. */
	int do_compress_images; /* Compress (or leave compressed) image streams. */
	int do_compress_fonts; /* Compress (or leave compressed) font streams. */
	int do_decompress; /* Decompress streams (except when compressing images/fonts). */
	int do_garbage; /* Garbage collect objects before saving; 1=gc, 2=re-number, 3=de-duplicate. */
	int do_linear; /* Write linearised. */
	int do_clean; /* Clean content streams. */
	int do_sanitize; /* Sanitize content streams. */
	int do_appearance; /* (Re)create appearance streams. */
	int do_encrypt; /* Encryption method to use: keep, none, rc4-40, etc. */
	int dont_regenerate_id; /* Don't regenerate ID if set (used for clean) */
	int permissions; /* Document encryption permissions. */
	char opwd_utf8[128]; /* Owner password. */
	char upwd_utf8[128]; /* User password. */
} pdf_write_options;

extern const pdf_write_options pdf_default_write_options;

/*
	Parse option string into a pdf_write_options struct.
	Matches the command line options to 'mutool clean':
		g: garbage collect
		d, i, f: expand all, fonts, images
		l: linearize
		a: ascii hex encode
		z: deflate
		c: clean content streams
		s: sanitize content streams
*/
pdf_write_options *pdf_parse_write_options(fz_context *ctx, pdf_write_options *opts, const char *args);

/*
	Returns true if there are digital signatures waiting to
	to updated on save.
*/
int pdf_has_unsaved_sigs(fz_context *ctx, pdf_document *doc);

/*
	Write out the document to an output stream with all changes finalised.
*/
void pdf_write_document(fz_context *ctx, pdf_document *doc, fz_output *out, pdf_write_options *opts);

/*
	Write out the document to a file with all changes finalised.
*/
void pdf_save_document(fz_context *ctx, pdf_document *doc, const char *filename, pdf_write_options *opts);

char *pdf_format_write_options(fz_context *ctx, char *buffer, size_t buffer_len, const pdf_write_options *opts);

/*
	Return true if the document can be saved incrementally. Applying
	redactions or having a repaired document make incremental saving
	impossible.
*/
int pdf_can_be_saved_incrementally(fz_context *ctx, pdf_document *doc);

#endif
