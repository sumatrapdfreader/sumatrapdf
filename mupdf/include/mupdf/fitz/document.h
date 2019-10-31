#ifndef MUPDF_FITZ_DOCUMENT_H
#define MUPDF_FITZ_DOCUMENT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/device.h"
#include "mupdf/fitz/transition.h"
#include "mupdf/fitz/link.h"
#include "mupdf/fitz/outline.h"
#include "mupdf/fitz/separation.h"

typedef struct fz_document_s fz_document;
typedef struct fz_document_handler_s fz_document_handler;
typedef struct fz_page_s fz_page;
typedef intptr_t fz_bookmark;

typedef struct fz_location_s
{
	int chapter;
	int page;
} fz_location;

static inline fz_location fz_make_location(int chapter, int page)
{
	fz_location loc = { chapter, page };
	return loc;
}

enum
{
	/* 6in at 4:3 */
	FZ_LAYOUT_KINDLE_W = 260,
	FZ_LAYOUT_KINDLE_H = 346,
	FZ_LAYOUT_KINDLE_EM = 9,

	/* 4.25 x 6.87 in */
	FZ_LAYOUT_US_POCKET_W = 306,
	FZ_LAYOUT_US_POCKET_H = 495,
	FZ_LAYOUT_US_POCKET_EM = 10,

	/* 5.5 x 8.5 in */
	FZ_LAYOUT_US_TRADE_W = 396,
	FZ_LAYOUT_US_TRADE_H = 612,
	FZ_LAYOUT_US_TRADE_EM = 11,

	/* 110 x 178 mm */
	FZ_LAYOUT_UK_A_FORMAT_W = 312,
	FZ_LAYOUT_UK_A_FORMAT_H = 504,
	FZ_LAYOUT_UK_A_FORMAT_EM = 10,

	/* 129 x 198 mm */
	FZ_LAYOUT_UK_B_FORMAT_W = 366,
	FZ_LAYOUT_UK_B_FORMAT_H = 561,
	FZ_LAYOUT_UK_B_FORMAT_EM = 10,

	/* 135 x 216 mm */
	FZ_LAYOUT_UK_C_FORMAT_W = 382,
	FZ_LAYOUT_UK_C_FORMAT_H = 612,
	FZ_LAYOUT_UK_C_FORMAT_EM = 11,

	/* 148 x 210 mm */
	FZ_LAYOUT_A5_W = 420,
	FZ_LAYOUT_A5_H = 595,
	FZ_LAYOUT_A5_EM = 11,

	/* Default to A5 */
	FZ_DEFAULT_LAYOUT_W = FZ_LAYOUT_A5_W,
	FZ_DEFAULT_LAYOUT_H = FZ_LAYOUT_A5_H,
	FZ_DEFAULT_LAYOUT_EM = FZ_LAYOUT_A5_EM,
};

typedef enum
{
	FZ_PERMISSION_PRINT = 'p',
	FZ_PERMISSION_COPY = 'c',
	FZ_PERMISSION_EDIT = 'e',
	FZ_PERMISSION_ANNOTATE = 'n',
}
fz_permission;

/*
	Type for a function to be called when
	the reference count for the fz_document drops to 0. The
	implementation should release any resources held by the
	document. The actual document pointer will be freed by the
	caller.
*/
typedef void (fz_document_drop_fn)(fz_context *ctx, fz_document *doc);

/*
	Type for a function to be
	called to enquire whether the document needs a password
	or not. See fz_needs_password for more information.
*/
typedef int (fz_document_needs_password_fn)(fz_context *ctx, fz_document *doc);

/*
	Type for a function to be
	called to attempt to authenticate a password. See
	fz_authenticate_password for more information.
*/
typedef int (fz_document_authenticate_password_fn)(fz_context *ctx, fz_document *doc, const char *password);

/*
	Type for a function to be
	called to see if a document grants a certain permission. See
	fz_document_has_permission for more information.
*/
typedef int (fz_document_has_permission_fn)(fz_context *ctx, fz_document *doc, fz_permission permission);

/*
	Type for a function to be called to
	load the outlines for a document. See fz_document_load_outline
	for more information.
*/
typedef fz_outline *(fz_document_load_outline_fn)(fz_context *ctx, fz_document *doc);

/*
	Type for a function to be called to lay
	out a document. See fz_layout_document for more information.
*/
typedef void (fz_document_layout_fn)(fz_context *ctx, fz_document *doc, float w, float h, float em);

/*
	Type for a function to be called to
	resolve an internal link to a page number. See fz_resolve_link
	for more information.
*/
typedef fz_location (fz_document_resolve_link_fn)(fz_context *ctx, fz_document *doc, const char *uri, float *xp, float *yp);

/*
	Type for a function to be called to
	count the number of chapters in a document. See fz_count_chapters for
	more information.
*/
typedef int (fz_document_count_chapters_fn)(fz_context *ctx, fz_document *doc);

/*
	Type for a function to be called to
	count the number of pages in a document. See fz_count_pages for
	more information.
*/
typedef int (fz_document_count_pages_fn)(fz_context *ctx, fz_document *doc, int chapter);

/*
	Type for a function to load a given
	page from a document. See fz_load_page for more information.
*/
typedef fz_page *(fz_document_load_page_fn)(fz_context *ctx, fz_document *doc, int chapter, int page);

/*
	Type for a function to query
	a documents metadata. See fz_lookup_metadata for more
	information.
*/
typedef int (fz_document_lookup_metadata_fn)(fz_context *ctx, fz_document *doc, const char *key, char *buf, int size);

/*
	Return output intent color space if it exists
*/
typedef fz_colorspace* (fz_document_output_intent_fn)(fz_context *ctx, fz_document *doc);

/*
	Write document accelerator data
*/
typedef void (fz_document_output_accelerator_fn)(fz_context *ctx, fz_document *doc, fz_output *out);

/*
	Type for a function to make
	a bookmark. See fz_make_bookmark for more information.
*/
typedef fz_bookmark (fz_document_make_bookmark_fn)(fz_context *ctx, fz_document *doc, fz_location loc);

/*
	Type for a function to lookup
	a bookmark. See fz_lookup_bookmark for more information.
*/
typedef fz_location (fz_document_lookup_bookmark_fn)(fz_context *ctx, fz_document *doc, fz_bookmark mark);

/*
	Type for a function to release all the
	resources held by a page. Called automatically when the
	reference count for that page reaches zero.
*/
typedef void (fz_page_drop_page_fn)(fz_context *ctx, fz_page *page);

/*
	Type for a function to return the
	bounding box of a page. See fz_bound_page for more
	information.
*/
typedef fz_rect (fz_page_bound_page_fn)(fz_context *ctx, fz_page *page);

/*
	Type for a function to run the
	contents of a page. See fz_run_page_contents for more
	information.
*/
typedef void (fz_page_run_page_fn)(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);

/*
	Type for a function to load the links
	from a page. See fz_load_links for more information.
*/
typedef fz_link *(fz_page_load_links_fn)(fz_context *ctx, fz_page *page);

/*
	Type for a function to
	obtain the details of how this page should be presented when
	in presentation mode. See fz_page_presentation for more
	information.
*/
typedef fz_transition *(fz_page_page_presentation_fn)(fz_context *ctx, fz_page *page, fz_transition *transition, float *duration);

/*
	Type for a function to enable/
	disable separations on a page. See fz_control_separation for
	more information.
*/
typedef void (fz_page_control_separation_fn)(fz_context *ctx, fz_page *page, int separation, int disable);

/*
	Type for a function to detect
	whether a given separation is enabled or disabled on a page.
	See FZ_SEPARATION_DISABLED for more information.
*/
typedef int (fz_page_separation_disabled_fn)(fz_context *ctx, fz_page *page, int separation);

/*
	Type for a function to retrieve
	details of separations on a page. See fz_get_separations
	for more information.
*/
typedef fz_separations *(fz_page_separations_fn)(fz_context *ctx, fz_page *page);

/*
	Type for a function to retrieve
	whether or not a given page uses overprint.
*/
typedef int (fz_page_uses_overprint_fn)(fz_context *ctx, fz_page *page);

/*
	Structure definition is public so other classes can
	derive from it. Do not access the members directly.
*/
struct fz_page_s
{
	int refs;
	int chapter; /* chapter number */
	int number; /* page number in chapter */
	int incomplete; /* incomplete from progressive loading; don't cache! */
	fz_page_drop_page_fn *drop_page;
	fz_page_bound_page_fn *bound_page;
	fz_page_run_page_fn *run_page_contents;
	fz_page_run_page_fn *run_page_annots;
	fz_page_run_page_fn *run_page_widgets;
	fz_page_load_links_fn *load_links;
	fz_page_page_presentation_fn *page_presentation;
	fz_page_control_separation_fn *control_separation;
	fz_page_separation_disabled_fn *separation_disabled;
	fz_page_separations_fn *separations;
	fz_page_uses_overprint_fn *overprint;
	fz_page **prev, *next; /* linked list of currently open pages */
};

/*
	Structure definition is public so other classes can
	derive from it. Callers should not access the members
	directly, though implementations will need initialize
	functions directly.
*/
struct fz_document_s
{
	int refs;
	fz_document_drop_fn *drop_document;
	fz_document_needs_password_fn *needs_password;
	fz_document_authenticate_password_fn *authenticate_password;
	fz_document_has_permission_fn *has_permission;
	fz_document_load_outline_fn *load_outline;
	fz_document_layout_fn *layout;
	fz_document_make_bookmark_fn *make_bookmark;
	fz_document_lookup_bookmark_fn *lookup_bookmark;
	fz_document_resolve_link_fn *resolve_link;
	fz_document_count_chapters_fn *count_chapters;
	fz_document_count_pages_fn *count_pages;
	fz_document_load_page_fn *load_page;
	fz_document_lookup_metadata_fn *lookup_metadata;
	fz_document_output_intent_fn *get_output_intent;
	fz_document_output_accelerator_fn *output_accelerator;
	int did_layout;
	int is_reflowable;
	fz_page *open; /* linked list of currently open pages */
};

/*
	Function type to open a document from a
	file.

	filename: file to open

	Pointer to opened document. Throws exception in case of error.
*/
typedef fz_document *(fz_document_open_fn)(fz_context *ctx, const char *filename);

/*
	Function type to open a
	document from a file.

	stream: fz_stream to read document data from. Must be
	seekable for formats that require it.

	Pointer to opened document. Throws exception in case of error.
*/
typedef fz_document *(fz_document_open_with_stream_fn)(fz_context *ctx, fz_stream *stream);

/*
	Function type to open a document from a
	file, with accelerator data.

	filename: file to open

	accel: accelerator file

	Pointer to opened document. Throws exception in case of error.
*/
typedef fz_document *(fz_document_open_accel_fn)(fz_context *ctx, const char *filename, const char *accel);

/*
	Function type to open a document from a file,
	with accelerator data.

	stream: fz_stream to read document data from. Must be
	seekable for formats that require it.

	accel: fz_stream to read accelerator data from. Must be
	seekable for formats that require it.

	Pointer to opened document. Throws exception in case of error.
*/
typedef fz_document *(fz_document_open_accel_with_stream_fn)(fz_context *ctx, fz_stream *stream, fz_stream *accel);

/*
	Recognize a document type from
	a magic string.

	magic: string to recognise - typically a filename or mime
	type.

	Returns a number between 0 (not recognized) and 100
	(fully recognized) based on how certain the recognizer
	is that this is of the required type.
*/
typedef int (fz_document_recognize_fn)(fz_context *ctx, const char *magic);

struct fz_document_handler_s
{
	fz_document_recognize_fn *recognize;
	fz_document_open_fn *open;
	fz_document_open_with_stream_fn *open_with_stream;
	const char **extensions;
	const char **mimetypes;
	fz_document_open_accel_fn *open_accel;
	fz_document_open_accel_with_stream_fn *open_accel_with_stream;
};

void fz_register_document_handler(fz_context *ctx, const fz_document_handler *handler);

void fz_register_document_handlers(fz_context *ctx);

const fz_document_handler *fz_recognize_document(fz_context *ctx, const char *magic);

fz_document *fz_open_document(fz_context *ctx, const char *filename);

fz_document *fz_open_accelerated_document(fz_context *ctx, const char *filename, const char *accel);

fz_document *fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream);

fz_document *fz_open_accelerated_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream, fz_stream *accel);

int fz_document_supports_accelerator(fz_context *ctx, fz_document *doc);

void fz_save_accelerator(fz_context *ctx, fz_document *doc, const char *accel);

void fz_output_accelerator(fz_context *ctx, fz_document *doc, fz_output *accel);

void *fz_new_document_of_size(fz_context *ctx, int size);
#define fz_new_derived_document(C,M) ((M*)Memento_label(fz_new_document_of_size(C, sizeof(M)), #M))

fz_document *fz_keep_document(fz_context *ctx, fz_document *doc);
void fz_drop_document(fz_context *ctx, fz_document *doc);

int fz_needs_password(fz_context *ctx, fz_document *doc);

int fz_authenticate_password(fz_context *ctx, fz_document *doc, const char *password);

fz_outline *fz_load_outline(fz_context *ctx, fz_document *doc);

int fz_is_document_reflowable(fz_context *ctx, fz_document *doc);

void fz_layout_document(fz_context *ctx, fz_document *doc, float w, float h, float em);

fz_bookmark fz_make_bookmark(fz_context *ctx, fz_document *doc, fz_location loc);

fz_location fz_lookup_bookmark(fz_context *ctx, fz_document *doc, fz_bookmark mark);

int fz_count_pages(fz_context *ctx, fz_document *doc);

fz_location fz_resolve_link(fz_context *ctx, fz_document *doc, const char *uri, float *xp, float *yp);
fz_location fz_last_page(fz_context *ctx, fz_document *doc);
fz_location fz_next_page(fz_context *ctx, fz_document *doc, fz_location loc);
fz_location fz_previous_page(fz_context *ctx, fz_document *doc, fz_location loc);
fz_location fz_clamp_location(fz_context *ctx, fz_document *doc, fz_location loc);
fz_location fz_location_from_page_number(fz_context *ctx, fz_document *doc, int number);
int fz_page_number_from_location(fz_context *ctx, fz_document *doc, fz_location loc);

fz_page *fz_load_page(fz_context *ctx, fz_document *doc, int number);

int fz_count_chapters(fz_context *ctx, fz_document *doc);
int fz_count_chapter_pages(fz_context *ctx, fz_document *doc, int chapter);
fz_page *fz_load_chapter_page(fz_context *ctx, fz_document *doc, int chapter, int page);

fz_link *fz_load_links(fz_context *ctx, fz_page *page);

fz_page *fz_new_page_of_size(fz_context *ctx, int size);
#define fz_new_derived_page(CTX,TYPE) \
	((TYPE *)Memento_label(fz_new_page_of_size(CTX,sizeof(TYPE)),#TYPE))

fz_rect fz_bound_page(fz_context *ctx, fz_page *page);

void fz_run_page(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);

void fz_run_page_contents(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);
void fz_run_page_annots(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);
void fz_run_page_widgets(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);

fz_page *fz_keep_page(fz_context *ctx, fz_page *page);
void fz_drop_page(fz_context *ctx, fz_page *page);

fz_transition *fz_page_presentation(fz_context *ctx, fz_page *page, fz_transition *transition, float *duration);

int fz_has_permission(fz_context *ctx, fz_document *doc, fz_permission p);

int fz_lookup_metadata(fz_context *ctx, fz_document *doc, const char *key, char *buf, int size);

#define FZ_META_FORMAT "format"
#define FZ_META_ENCRYPTION "encryption"

#define FZ_META_INFO_AUTHOR "info:Author"
#define FZ_META_INFO_TITLE "info:Title"

fz_colorspace *fz_document_output_intent(fz_context *ctx, fz_document *doc);

fz_separations *fz_page_separations(fz_context *ctx, fz_page *page);

int fz_page_uses_overprint(fz_context *ctx, fz_page *page);

#endif
