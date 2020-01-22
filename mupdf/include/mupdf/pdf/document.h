#ifndef MUPDF_PDF_DOCUMENT_H
#define MUPDF_PDF_DOCUMENT_H

typedef struct pdf_lexbuf_s pdf_lexbuf;
typedef struct pdf_lexbuf_large_s pdf_lexbuf_large;
typedef struct pdf_xref_s pdf_xref;
typedef struct pdf_ocg_descriptor_s pdf_ocg_descriptor;

typedef struct pdf_page_s pdf_page;
typedef struct pdf_annot_s pdf_annot;
typedef struct pdf_annot_s pdf_widget;
typedef struct pdf_js_s pdf_js;

enum
{
	PDF_LEXBUF_SMALL = 256,
	PDF_LEXBUF_LARGE = 65536
};

struct pdf_lexbuf_s
{
	size_t size;
	size_t base_size;
	size_t len;
	int64_t i;
	float f;
	char *scratch;
	char buffer[PDF_LEXBUF_SMALL];
};

struct pdf_lexbuf_large_s
{
	pdf_lexbuf base;
	char buffer[PDF_LEXBUF_LARGE - PDF_LEXBUF_SMALL];
};

/*
	Document event structures are mostly opaque to the app. Only the type
	is visible to the app.
*/
typedef struct pdf_doc_event_s pdf_doc_event;

/*
	the type of function via which the app receives
	document events.
*/
typedef void (pdf_doc_event_cb)(fz_context *ctx, pdf_document *doc, pdf_doc_event *event, void *data);

pdf_document *pdf_open_document(fz_context *ctx, const char *filename);

pdf_document *pdf_open_document_with_stream(fz_context *ctx, fz_stream *file);

void pdf_drop_document(fz_context *ctx, pdf_document *doc);

pdf_document *pdf_keep_document(fz_context *ctx, pdf_document *doc);

pdf_document *pdf_specifics(fz_context *ctx, fz_document *doc);

pdf_document *pdf_document_from_fz_document(fz_context *ctx, fz_document *ptr);
pdf_page *pdf_page_from_fz_page(fz_context *ctx, fz_page *ptr);

int pdf_needs_password(fz_context *ctx, pdf_document *doc);

int pdf_authenticate_password(fz_context *ctx, pdf_document *doc, const char *pw);

int pdf_has_permission(fz_context *ctx, pdf_document *doc, fz_permission p);
int pdf_lookup_metadata(fz_context *ctx, pdf_document *doc, const char *key, char *ptr, int size);

fz_outline *pdf_load_outline(fz_context *ctx, pdf_document *doc);

int pdf_count_layer_configs(fz_context *ctx, pdf_document *doc);

typedef struct
{
	const char *name;
	const char *creator;
} pdf_layer_config;

void pdf_layer_config_info(fz_context *ctx, pdf_document *doc, int config_num, pdf_layer_config *info);

void pdf_select_layer_config(fz_context *ctx, pdf_document *doc, int config_num);

int pdf_count_layer_config_ui(fz_context *ctx, pdf_document *doc);

void pdf_select_layer_config_ui(fz_context *ctx, pdf_document *doc, int ui);

void pdf_deselect_layer_config_ui(fz_context *ctx, pdf_document *doc, int ui);

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

void pdf_layer_config_ui_info(fz_context *ctx, pdf_document *doc, int ui, pdf_layer_config_ui *info);

void pdf_set_layer_config_as_default(fz_context *ctx, pdf_document *doc);

int pdf_has_unsaved_changes(fz_context *ctx, pdf_document *doc);

typedef enum
{
	PDF_SIGNATURE_ERROR_OKAY,
	PDF_SIGNATURE_ERROR_NO_SIGNATURES,
	PDF_SIGNATURE_ERROR_NO_CERTIFICATE,
	PDF_SIGNATURE_ERROR_DIGEST_FAILURE,
	PDF_SIGNATURE_ERROR_SELF_SIGNED,
	PDF_SIGNATURE_ERROR_SELF_SIGNED_IN_CHAIN,
	PDF_SIGNATURE_ERROR_NOT_TRUSTED,
	PDF_SIGNATURE_ERROR_UNKNOWN
} pdf_signature_error;

typedef struct pdf_pkcs7_designated_name_s
{
	char *cn;
	char *o;
	char *ou;
	char *email;
	char *c;
}
pdf_pkcs7_designated_name;

/* Object that can perform the cryptographic operation necessary for document signing */
typedef struct pdf_pkcs7_signer_s pdf_pkcs7_signer;

/* Increment the reference count for a signer object */
typedef pdf_pkcs7_signer *(pdf_pkcs7_keep_fn)(pdf_pkcs7_signer *signer);

/* Drop a reference for a signer object */
typedef void (pdf_pkcs7_drop_fn)(pdf_pkcs7_signer *signer);

/* Obtain the designated name information from a signer object */
typedef pdf_pkcs7_designated_name *(pdf_pkcs7_designated_name_fn)(pdf_pkcs7_signer *signer);

/* Free the resources associated with previously obtained designated name information */
typedef void (pdf_pkcs7_drop_designated_name_fn)(pdf_pkcs7_signer *signer, pdf_pkcs7_designated_name *name);

/* Predict the size of the digest. The actual digest returned by create_digest will be no greater in size */
typedef size_t (pdf_pkcs7_max_digest_size_fn)(pdf_pkcs7_signer *signer);

/* Create a signature based on ranges of bytes drawn from a stream */
typedef int (pdf_pkcs7_create_digest_fn)(pdf_pkcs7_signer *signer, fz_stream *in, unsigned char *digest, size_t *digest_len);

struct pdf_pkcs7_signer_s
{
	pdf_pkcs7_keep_fn *keep;
	pdf_pkcs7_drop_fn *drop;
	pdf_pkcs7_designated_name_fn *designated_name;
	pdf_pkcs7_drop_designated_name_fn *drop_designated_name;
	pdf_pkcs7_max_digest_size_fn *max_digest_size;
	pdf_pkcs7_create_digest_fn *create_digest;
};

/* Unsaved signature fields */
typedef struct pdf_unsaved_sig_s pdf_unsaved_sig;

struct pdf_unsaved_sig_s
{
	pdf_obj *field;
	size_t byte_range_start;
	size_t byte_range_end;
	size_t contents_start;
	size_t contents_end;
	pdf_pkcs7_signer *signer;
	pdf_unsaved_sig *next;
};

typedef struct pdf_rev_page_map_s pdf_rev_page_map;
struct pdf_rev_page_map_s
{
	int page;
	int object;
};

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

struct pdf_document_s
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
		fz_hash_table *images;
		fz_hash_table *fonts;
	} resources;

	int orphans_max;
	int orphans_count;
	pdf_obj **orphans;
};

pdf_document *pdf_create_document(fz_context *ctx);

typedef struct pdf_graft_map_s pdf_graft_map;

pdf_obj *pdf_graft_object(fz_context *ctx, pdf_document *dst, pdf_obj *obj);

pdf_graft_map *pdf_new_graft_map(fz_context *ctx, pdf_document *dst);

pdf_graft_map *pdf_keep_graft_map(fz_context *ctx, pdf_graft_map *map);
void pdf_drop_graft_map(fz_context *ctx, pdf_graft_map *map);

pdf_obj *pdf_graft_mapped_object(fz_context *ctx, pdf_graft_map *map, pdf_obj *obj);

fz_device *pdf_page_write(fz_context *ctx, pdf_document *doc, fz_rect mediabox, pdf_obj **presources, fz_buffer **pcontents);

pdf_obj *pdf_add_page(fz_context *ctx, pdf_document *doc, fz_rect mediabox, int rotate, pdf_obj *resources, fz_buffer *contents);

void pdf_insert_page(fz_context *ctx, pdf_document *doc, int at, pdf_obj *page);

void pdf_delete_page(fz_context *ctx, pdf_document *doc, int number);

void pdf_delete_page_range(fz_context *ctx, pdf_document *doc, int start, int end);

void pdf_finish_edit(fz_context *ctx, pdf_document *doc);

int pdf_recognize(fz_context *doc, const char *magic);

typedef struct pdf_write_options_s pdf_write_options;

/*
	In calls to fz_save_document, the following options structure can be used
	to control aspects of the writing process. This structure may grow
	in the future, and should be zero-filled to allow forwards compatibility.
*/
struct pdf_write_options_s
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
	int permissions; /* Document encryption permissions. */
	char opwd_utf8[128]; /* Owner password. */
	char upwd_utf8[128]; /* User password. */
};

extern const pdf_write_options pdf_default_write_options;

pdf_write_options *pdf_parse_write_options(fz_context *ctx, pdf_write_options *opts, const char *args);

int pdf_has_unsaved_sigs(fz_context *ctx, pdf_document *doc);

void pdf_write_document(fz_context *ctx, pdf_document *doc, fz_output *out, pdf_write_options *opts);

void pdf_save_document(fz_context *ctx, pdf_document *doc, const char *filename, pdf_write_options *opts);

int pdf_can_be_saved_incrementally(fz_context *ctx, pdf_document *doc);

#endif
