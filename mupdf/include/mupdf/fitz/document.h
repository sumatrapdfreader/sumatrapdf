// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_DOCUMENT_H
#define MUPDF_FITZ_DOCUMENT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/types.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/device.h"
#include "mupdf/fitz/transition.h"
#include "mupdf/fitz/link.h"
#include "mupdf/fitz/outline.h"
#include "mupdf/fitz/separation.h"

typedef struct fz_document_handler fz_document_handler;
typedef struct fz_page fz_page;
typedef intptr_t fz_bookmark;

typedef enum
{
	FZ_MEDIA_BOX,
	FZ_CROP_BOX,
	FZ_BLEED_BOX,
	FZ_TRIM_BOX,
	FZ_ART_BOX,
	FZ_UNKNOWN_BOX
} fz_box_type;

fz_box_type fz_box_type_from_string(const char *name);
const char *fz_string_from_box_type(fz_box_type box);

/**
	Simple constructor for fz_locations.
*/
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
	FZ_PERMISSION_FORM = 'f',
	FZ_PERMISSION_ACCESSIBILITY = 'y',
	FZ_PERMISSION_ASSEMBLE = 'a',
	FZ_PERMISSION_PRINT_HQ = 'h',
}
fz_permission;

/**
	Type for a function to be called when
	the reference count for the fz_document drops to 0. The
	implementation should release any resources held by the
	document. The actual document pointer will be freed by the
	caller.
*/
typedef void (fz_document_drop_fn)(fz_context *ctx, fz_document *doc);

/**
	Type for a function to be
	called to enquire whether the document needs a password
	or not. See fz_needs_password for more information.
*/
typedef int (fz_document_needs_password_fn)(fz_context *ctx, fz_document *doc);

/**
	Type for a function to be
	called to attempt to authenticate a password. See
	fz_authenticate_password for more information.
*/
typedef int (fz_document_authenticate_password_fn)(fz_context *ctx, fz_document *doc, const char *password);

/**
	Type for a function to be
	called to see if a document grants a certain permission. See
	fz_document_has_permission for more information.
*/
typedef int (fz_document_has_permission_fn)(fz_context *ctx, fz_document *doc, fz_permission permission);

/**
	Type for a function to be called to
	load the outlines for a document. See fz_document_load_outline
	for more information.
*/
typedef fz_outline *(fz_document_load_outline_fn)(fz_context *ctx, fz_document *doc);

/**
	Type for a function to be called to obtain an outline iterator
	for a document. See fz_document_outline_iterator for more information.
*/
typedef fz_outline_iterator *(fz_document_outline_iterator_fn)(fz_context *ctx, fz_document *doc);

/**
	Type for a function to be called to lay
	out a document. See fz_layout_document for more information.
*/
typedef void (fz_document_layout_fn)(fz_context *ctx, fz_document *doc, float w, float h, float em);

/**
	Type for a function to be called to
	resolve an internal link to a location (chapter/page number
	tuple). See fz_resolve_link_dest for more information.
*/
typedef fz_link_dest (fz_document_resolve_link_dest_fn)(fz_context *ctx, fz_document *doc, const char *uri);

/**
	Type for a function to be called to
	create an internal link to a destination (chapter/page/x/y/w/h/zoom/type
	tuple). See fz_resolve_link_dest for more information.
*/
typedef char * (fz_document_format_link_uri_fn)(fz_context *ctx, fz_document *doc, fz_link_dest dest);

/**
	Type for a function to be called to
	count the number of chapters in a document. See
	fz_count_chapters for more information.
*/
typedef int (fz_document_count_chapters_fn)(fz_context *ctx, fz_document *doc);

/**
	Type for a function to be called to
	count the number of pages in a document. See fz_count_pages for
	more information.
*/
typedef int (fz_document_count_pages_fn)(fz_context *ctx, fz_document *doc, int chapter);

/**
	Type for a function to load a given
	page from a document. See fz_load_page for more information.
*/
typedef fz_page *(fz_document_load_page_fn)(fz_context *ctx, fz_document *doc, int chapter, int page);

/**
	Type for a function to get the page label of a page in the document.
	See fz_page_label for more information.
*/
typedef void (fz_document_page_label_fn)(fz_context *ctx, fz_document *doc, int chapter, int page, char *buf, size_t size);

/**
	Type for a function to query
	a document's metadata. See fz_lookup_metadata for more
	information.
*/
typedef int (fz_document_lookup_metadata_fn)(fz_context *ctx, fz_document *doc, const char *key, char *buf, size_t size);

/**
	Type for a function to set
	a document's metadata. See fz_set_metadata for more
	information.
*/
typedef int (fz_document_set_metadata_fn)(fz_context *ctx, fz_document *doc, const char *key, const char *value);

/**
	Return output intent color space if it exists
*/
typedef fz_colorspace *(fz_document_output_intent_fn)(fz_context *ctx, fz_document *doc);

/**
	Write document accelerator data
*/
typedef void (fz_document_output_accelerator_fn)(fz_context *ctx, fz_document *doc, fz_output *out);

/**
	Type for a function to make
	a bookmark. See fz_make_bookmark for more information.
*/
typedef fz_bookmark (fz_document_make_bookmark_fn)(fz_context *ctx, fz_document *doc, fz_location loc);

/**
	Type for a function to lookup a bookmark.
	See fz_lookup_bookmark for more information.
*/
typedef fz_location (fz_document_lookup_bookmark_fn)(fz_context *ctx, fz_document *doc, fz_bookmark mark);

/**
	Type for a function to release all the
	resources held by a page. Called automatically when the
	reference count for that page reaches zero.
*/
typedef void (fz_page_drop_page_fn)(fz_context *ctx, fz_page *page);

/**
	Type for a function to return the
	bounding box of a page. See fz_bound_page for more
	information.
*/
typedef fz_rect (fz_page_bound_page_fn)(fz_context *ctx, fz_page *page, fz_box_type box);

/**
	Type for a function to run the
	contents of a page. See fz_run_page_contents for more
	information.
*/
typedef void (fz_page_run_page_fn)(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);

/**
	Type for a function to load the links
	from a page. See fz_load_links for more information.
*/
typedef fz_link *(fz_page_load_links_fn)(fz_context *ctx, fz_page *page);

/**
	Type for a function to
	obtain the details of how this page should be presented when
	in presentation mode. See fz_page_presentation for more
	information.
*/
typedef fz_transition *(fz_page_page_presentation_fn)(fz_context *ctx, fz_page *page, fz_transition *transition, float *duration);

/**
	Type for a function to enable/
	disable separations on a page. See fz_control_separation for
	more information.
*/
typedef void (fz_page_control_separation_fn)(fz_context *ctx, fz_page *page, int separation, int disable);

/**
	Type for a function to detect
	whether a given separation is enabled or disabled on a page.
	See FZ_SEPARATION_DISABLED for more information.
*/
typedef int (fz_page_separation_disabled_fn)(fz_context *ctx, fz_page *page, int separation);

/**
	Type for a function to retrieve
	details of separations on a page. See fz_get_separations
	for more information.
*/
typedef fz_separations *(fz_page_separations_fn)(fz_context *ctx, fz_page *page);

/**
	Type for a function to retrieve
	whether or not a given page uses overprint.
*/
typedef int (fz_page_uses_overprint_fn)(fz_context *ctx, fz_page *page);


/**
	Type for a function to create a link on a page.
*/
typedef fz_link *(fz_page_create_link_fn)(fz_context *ctx, fz_page *page, fz_rect bbox, const char *uri);

/**
	Type for a function to delete a link on a page.
*/
typedef void (fz_page_delete_link_fn)(fz_context *ctx, fz_page *page, fz_link *link);

/**
	Function type to open a document from a file.

	filename: file to open

	Pointer to opened document. Throws exception in case of error.
*/
typedef fz_document *(fz_document_open_fn)(fz_context *ctx, const char *filename);

/**
	Function type to open a
	document from a file.

	stream: fz_stream to read document data from. Must be
	seekable for formats that require it.

	Pointer to opened document. Throws exception in case of error.
*/
typedef fz_document *(fz_document_open_with_stream_fn)(fz_context *ctx, fz_stream *stream);

/**
	Function type to open a document from a
	file, with accelerator data.

	filename: file to open

	accel: accelerator file

	Pointer to opened document. Throws exception in case of error.
*/
typedef fz_document *(fz_document_open_accel_fn)(fz_context *ctx, const char *filename, const char *accel);

/**
	Function type to open a document from a file,
	with accelerator data.

	stream: fz_stream to read document data from. Must be
	seekable for formats that require it.

	accel: fz_stream to read accelerator data from. Must be
	seekable for formats that require it.

	Pointer to opened document. Throws exception in case of error.
*/
typedef fz_document *(fz_document_open_accel_with_stream_fn)(fz_context *ctx, fz_stream *stream, fz_stream *accel);

/**
	Recognize a document type from
	a magic string.

	magic: string to recognise - typically a filename or mime
	type.

	Returns a number between 0 (not recognized) and 100
	(fully recognized) based on how certain the recognizer
	is that this is of the required type.
*/
typedef int (fz_document_recognize_fn)(fz_context *ctx, const char *magic);

/**
	Recognize a document type from stream contents.

	stream: stream contents to recognise.

	Returns a number between 0 (not recognized) and 100
	(fully recognized) based on how certain the recognizer
	is that this is of the required type.
*/
typedef int (fz_document_recognize_content_fn)(fz_context *ctx, fz_stream *stream);

/**
	Type for a function to be called when processing an already opened page.
	See fz_process_opened_pages.
*/
typedef void *(fz_process_opened_page_fn)(fz_context *ctx, fz_page *page, void *state);

/**
	Register a handler for a document type.

	handler: The handler to register.
*/
void fz_register_document_handler(fz_context *ctx, const fz_document_handler *handler);

/**
	Register handlers
	for all the standard document types supported in
	this build.
*/
void fz_register_document_handlers(fz_context *ctx);

/**
	Given a magic find a document handler that can handle a
	document of this type.

	magic: Can be a filename extension (including initial period) or
	a mimetype.
*/
const fz_document_handler *fz_recognize_document(fz_context *ctx, const char *magic);

/**
	Given a filename find a document handler that can handle a
	document of this type.

	filename: The filename of the document. This will be opened and sampled
	to check data.
*/
const fz_document_handler *fz_recognize_document_content(fz_context *ctx, const char *filename);

/**
	Given a magic find a document handler that can handle a
	document of this type.

	stream: the file stream to sample.

	magic: Can be a filename extension (including initial period) or
	a mimetype.
*/
const fz_document_handler *fz_recognize_document_stream_content(fz_context *ctx, fz_stream *stream, const char *magic);

/**
	Open a document file and read its basic structure so pages and
	objects can be located. MuPDF will try to repair broken
	documents (without actually changing the file contents).

	The returned fz_document is used when calling most other
	document related functions.

	filename: a path to a file as it would be given to open(2).
*/
fz_document *fz_open_document(fz_context *ctx, const char *filename);

/**
	Open a document file and read its basic structure so pages and
	objects can be located. MuPDF will try to repair broken
	documents (without actually changing the file contents).

	The returned fz_document is used when calling most other
	document related functions.

	filename: a path to a file as it would be given to open(2).
*/
fz_document *fz_open_accelerated_document(fz_context *ctx, const char *filename, const char *accel);

/**
	Open a document using the specified stream object rather than
	opening a file on disk.

	magic: a string used to detect document type; either a file name
	or mime-type.
*/
fz_document *fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream);

/**
	Open a document using a buffer rather than opening a file on disk.
*/
fz_document *fz_open_document_with_buffer(fz_context *ctx, const char *magic, fz_buffer *buffer);

/**
	Open a document using the specified stream object rather than
	opening a file on disk.

	magic: a string used to detect document type; either a file name
	or mime-type.
*/
fz_document *fz_open_accelerated_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream, fz_stream *accel);

/**
	Query if the document supports the saving of accelerator data.
*/
int fz_document_supports_accelerator(fz_context *ctx, fz_document *doc);

/**
	Save accelerator data for the document to a given file.
*/
void fz_save_accelerator(fz_context *ctx, fz_document *doc, const char *accel);

/**
	Output accelerator data for the document to a given output
	stream.
*/
void fz_output_accelerator(fz_context *ctx, fz_document *doc, fz_output *accel);

/**
	New documents are typically created by calls like
	foo_new_document(fz_context *ctx, ...). These work by
	deriving a new document type from fz_document, for instance:
	typedef struct { fz_document base; ...extras... } foo_document;
	These are allocated by calling
	fz_new_derived_document(ctx, foo_document)
*/
void *fz_new_document_of_size(fz_context *ctx, int size);
#define fz_new_derived_document(C,M) ((M*)Memento_label(fz_new_document_of_size(C, sizeof(M)), #M))

/**
	Increment the document reference count. The same pointer is
	returned.

	Never throws exceptions.
*/
fz_document *fz_keep_document(fz_context *ctx, fz_document *doc);

/**
	Decrement the document reference count. When the reference
	count reaches 0, the document and all it's references are
	freed.

	Never throws exceptions.
*/
void fz_drop_document(fz_context *ctx, fz_document *doc);

/**
	Check if a document is encrypted with a
	non-blank password.
*/
int fz_needs_password(fz_context *ctx, fz_document *doc);

/**
	Test if the given password can decrypt the document.

	password: The password string to be checked. Some document
	specifications do not specify any particular text encoding, so
	neither do we.

	Returns 0 for failure to authenticate, non-zero for success.

	For PDF documents, further information can be given by examining
	the bits in the return code.

		Bit 0 => No password required
		Bit 1 => User password authenticated
		Bit 2 => Owner password authenticated
*/
int fz_authenticate_password(fz_context *ctx, fz_document *doc, const char *password);

/**
	Load the hierarchical document outline.

	Should be freed by fz_drop_outline.
*/
fz_outline *fz_load_outline(fz_context *ctx, fz_document *doc);

/**
	Get an iterator for the document outline.

	Should be freed by fz_drop_outline_iterator.
*/
fz_outline_iterator *fz_new_outline_iterator(fz_context *ctx, fz_document *doc);

/**
	Is the document reflowable.

	Returns 1 to indicate reflowable documents, otherwise 0.
*/
int fz_is_document_reflowable(fz_context *ctx, fz_document *doc);

/**
	Layout reflowable document types.

	w, h: Page size in points.
	em: Default font size in points.
*/
void fz_layout_document(fz_context *ctx, fz_document *doc, float w, float h, float em);

/**
	Create a bookmark for the given page, which can be used to find
	the same location after the document has been laid out with
	different parameters.
*/
fz_bookmark fz_make_bookmark(fz_context *ctx, fz_document *doc, fz_location loc);

/**
	Find a bookmark and return its page number.
*/
fz_location fz_lookup_bookmark(fz_context *ctx, fz_document *doc, fz_bookmark mark);

/**
	Return the number of pages in document

	May return 0 for documents with no pages.
*/
int fz_count_pages(fz_context *ctx, fz_document *doc);

/**
	Resolve an internal link to a page number, location, and possible viewing parameters.

	Returns location (-1,-1) if the URI cannot be resolved.
*/
fz_link_dest fz_resolve_link_dest(fz_context *ctx, fz_document *doc, const char *uri);

/**
	Format an internal link to a page number, location, and possible viewing parameters,
	suitable for use with fz_create_link.

	Returns a newly allocated string that the caller must free.
*/
char *fz_format_link_uri(fz_context *ctx, fz_document *doc, fz_link_dest dest);

/**
	Resolve an internal link to a page number.

	xp, yp: Pointer to store coordinate of destination on the page.

	Returns (-1,-1) if the URI cannot be resolved.
*/
fz_location fz_resolve_link(fz_context *ctx, fz_document *doc, const char *uri, float *xp, float *yp);

/**
	Function to get the location for the last page in the document.
	Using this can be far more efficient in some cases than calling
	fz_count_pages and using the page number.
*/
fz_location fz_last_page(fz_context *ctx, fz_document *doc);

/**
	Function to get the location of the next page (allowing for the
	end of chapters etc). If at the end of the document, returns the
	current location.
*/
fz_location fz_next_page(fz_context *ctx, fz_document *doc, fz_location loc);

/**
	Function to get the location of the previous page (allowing for
	the end of chapters etc). If already at the start of the
	document, returns the current page.
*/
fz_location fz_previous_page(fz_context *ctx, fz_document *doc, fz_location loc);

/**
	Clamps a location into valid chapter/page range. (First clamps
	the chapter into range, then the page into range).
*/
fz_location fz_clamp_location(fz_context *ctx, fz_document *doc, fz_location loc);

/**
	Converts from page number to chapter+page. This may cause many
	chapters to be laid out in order to calculate the number of
	pages within those chapters.
*/
fz_location fz_location_from_page_number(fz_context *ctx, fz_document *doc, int number);

/**
	Converts from chapter+page to page number. This may cause many
	chapters to be laid out in order to calculate the number of
	pages within those chapters.
*/
int fz_page_number_from_location(fz_context *ctx, fz_document *doc, fz_location loc);

/**
	Load a given page number from a document. This may be much less
	efficient than loading by location (chapter+page) for some
	document types.
*/
fz_page *fz_load_page(fz_context *ctx, fz_document *doc, int number);

/**
	Return the number of chapters in the document.
	At least 1.
*/
int fz_count_chapters(fz_context *ctx, fz_document *doc);

/**
	Return the number of pages in a chapter.
	May return 0.
*/
int fz_count_chapter_pages(fz_context *ctx, fz_document *doc, int chapter);

/**
	Load a page.

	After fz_load_page is it possible to retrieve the size of the
	page using fz_bound_page, or to render the page using
	fz_run_page_*. Free the page by calling fz_drop_page.

	chapter: chapter number, 0 is the first chapter of the document.
	number: page number, 0 is the first page of the chapter.
*/
fz_page *fz_load_chapter_page(fz_context *ctx, fz_document *doc, int chapter, int page);

/**
	Load the list of links for a page.

	Returns a linked list of all the links on the page, each with
	its clickable region and link destination. Each link is
	reference counted so drop and free the list of links by
	calling fz_drop_link on the pointer return from fz_load_links.

	page: Page obtained from fz_load_page.
*/
fz_link *fz_load_links(fz_context *ctx, fz_page *page);

/**
	Different document types will be implemented by deriving from
	fz_page. This macro allocates such derived structures, and
	initialises the base sections.
*/
fz_page *fz_new_page_of_size(fz_context *ctx, int size, fz_document *doc);
#define fz_new_derived_page(CTX,TYPE,DOC) \
	((TYPE *)Memento_label(fz_new_page_of_size(CTX,sizeof(TYPE),DOC),#TYPE))

/**
	Determine the size of a page at 72 dpi.
*/
fz_rect fz_bound_page(fz_context *ctx, fz_page *page);
fz_rect fz_bound_page_box(fz_context *ctx, fz_page *page, fz_box_type box);

/**
	Run a page through a device.

	page: Page obtained from fz_load_page.

	dev: Device obtained from fz_new_*_device.

	transform: Transform to apply to page. May include for example
	scaling and rotation, see fz_scale, fz_rotate and fz_concat.
	Set to fz_identity if no transformation is desired.

	cookie: Communication mechanism between caller and library
	rendering the page. Intended for multi-threaded applications,
	while single-threaded applications set cookie to NULL. The
	caller may abort an ongoing rendering of a page. Cookie also
	communicates progress information back to the caller. The
	fields inside cookie are continually updated while the page is
	rendering.
*/
void fz_run_page(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);

/**
	Run a page through a device. Just the main
	page content, without the annotations, if any.

	page: Page obtained from fz_load_page.

	dev: Device obtained from fz_new_*_device.

	transform: Transform to apply to page. May include for example
	scaling and rotation, see fz_scale, fz_rotate and fz_concat.
	Set to fz_identity if no transformation is desired.

	cookie: Communication mechanism between caller and library
	rendering the page. Intended for multi-threaded applications,
	while single-threaded applications set cookie to NULL. The
	caller may abort an ongoing rendering of a page. Cookie also
	communicates progress information back to the caller. The
	fields inside cookie are continually updated while the page is
	rendering.
*/
void fz_run_page_contents(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);

/**
	Run the annotations on a page through a device.
*/
void fz_run_page_annots(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);

/**
	Run the widgets on a page through a device.
*/
void fz_run_page_widgets(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);

/**
	Increment the reference count for the page. Returns the same
	pointer.

	Never throws exceptions.
*/
fz_page *fz_keep_page(fz_context *ctx, fz_page *page);

/**
	Increment the reference count for the page. Returns the same
	pointer. Must only be used when the alloc lock is already taken.

	Never throws exceptions.
*/
fz_page *fz_keep_page_locked(fz_context *ctx, fz_page *page);

/**
	Decrements the reference count for the page. When the reference
	count hits 0, the page and its references are freed.

	Never throws exceptions.
*/
void fz_drop_page(fz_context *ctx, fz_page *page);

/**
	Get the presentation details for a given page.

	transition: A pointer to a transition struct to fill out.

	duration: A pointer to a place to set the page duration in
	seconds. Will be set to 0 if no transition is specified for the
	page.

	Returns: a pointer to the transition structure, or NULL if there
	is no transition specified for the page.
*/
fz_transition *fz_page_presentation(fz_context *ctx, fz_page *page, fz_transition *transition, float *duration);

/**
	Get page label for a given page.
*/
const char *fz_page_label(fz_context *ctx, fz_page *page, char *buf, int size);

/**
	Check permission flags on document.
*/
int fz_has_permission(fz_context *ctx, fz_document *doc, fz_permission p);

/**
	Retrieve document meta data strings.

	doc: The document to query.

	key: Which meta data key to retrieve...

	Basic information:
		'format'	-- Document format and version.
		'encryption'	-- Description of the encryption used.

	From the document information dictionary:
		'info:Title'
		'info:Author'
		'info:Subject'
		'info:Keywords'
		'info:Creator'
		'info:Producer'
		'info:CreationDate'
		'info:ModDate'

	buf: The buffer to hold the results (a nul-terminated UTF-8
	string).

	size: Size of 'buf'.

	Returns the number of bytes need to store the string plus terminator
	(will be larger than 'size' if the output was truncated), or -1 if the
	key is not recognized or found.
*/
int fz_lookup_metadata(fz_context *ctx, fz_document *doc, const char *key, char *buf, int size);

#define FZ_META_FORMAT "format"
#define FZ_META_ENCRYPTION "encryption"

#define FZ_META_INFO "info:"
#define FZ_META_INFO_TITLE "info:Title"
#define FZ_META_INFO_AUTHOR "info:Author"
#define FZ_META_INFO_SUBJECT "info:Subject"
#define FZ_META_INFO_KEYWORDS "info:Keywords"
#define FZ_META_INFO_CREATOR "info:Creator"
#define FZ_META_INFO_PRODUCER "info:Producer"
#define FZ_META_INFO_CREATIONDATE "info:CreationDate"
#define FZ_META_INFO_MODIFICATIONDATE "info:ModDate"

void fz_set_metadata(fz_context *ctx, fz_document *doc, const char *key, const char *value);

/**
	Find the output intent colorspace if the document has defined
	one.

	Returns a borrowed reference that should not be dropped, unless
	it is kept first.
*/
fz_colorspace *fz_document_output_intent(fz_context *ctx, fz_document *doc);

/**
	Get the separations details for a page.
	This will be NULL, unless the format specifically supports
	separations (such as PDF files). May be NULL even
	so, if there are no separations on a page.

	Returns a reference that must be dropped.
*/
fz_separations *fz_page_separations(fz_context *ctx, fz_page *page);

/**
	Query if a given page requires overprint.
*/
int fz_page_uses_overprint(fz_context *ctx, fz_page *page);

/**
	Create a new link on a page.
*/
fz_link *fz_create_link(fz_context *ctx, fz_page *page, fz_rect bbox, const char *uri);

/**
	Delete an existing link on a page.
*/
void fz_delete_link(fz_context *ctx, fz_page *page, fz_link *link);

/**
	Iterates over all opened pages of the document, calling the
	provided callback for each page for processing. If the callback
	returns non-NULL then the iteration stops and that value is returned
	to the called of fz_process_opened_pages().

	The state pointer provided to fz_process_opened_pages() is
	passed on to the callback but is owned by the caller.

	Returns the first non-NULL value returned by the callback,
	or NULL if the callback returned NULL for all opened pages.
*/
void *fz_process_opened_pages(fz_context *ctx, fz_document *doc, fz_process_opened_page_fn *process_openend_page, void *state);

/* Implementation details: subject to change. */

/**
	Structure definition is public so other classes can
	derive from it. Do not access the members directly.
*/
struct fz_page
{
	int refs;
	fz_document *doc; /* kept reference to parent document. Guaranteed non-NULL. */
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
	fz_page_create_link_fn *create_link;
	fz_page_delete_link_fn *delete_link;

	/* linked list of currently open pages. This list is maintained
	 * by fz_load_chapter_page and fz_drop_page. All pages hold a
	 * kept reference to the document, so the document cannot disappear
	 * while pages exist. 'Incomplete' pages are NOT kept in this
	 * list. */
	fz_page **prev, *next;
};

/**
	Structure definition is public so other classes can
	derive from it. Callers should not access the members
	directly, though implementations will need initialize
	functions directly.
*/
struct fz_document
{
	int refs;
	fz_document_drop_fn *drop_document;
	fz_document_needs_password_fn *needs_password;
	fz_document_authenticate_password_fn *authenticate_password;
	fz_document_has_permission_fn *has_permission;
	fz_document_load_outline_fn *load_outline;
	fz_document_outline_iterator_fn *outline_iterator;
	fz_document_layout_fn *layout;
	fz_document_make_bookmark_fn *make_bookmark;
	fz_document_lookup_bookmark_fn *lookup_bookmark;
	fz_document_resolve_link_dest_fn *resolve_link_dest;
	fz_document_format_link_uri_fn *format_link_uri;
	fz_document_count_chapters_fn *count_chapters;
	fz_document_count_pages_fn *count_pages;
	fz_document_load_page_fn *load_page;
	fz_document_page_label_fn *page_label;
	fz_document_lookup_metadata_fn *lookup_metadata;
	fz_document_set_metadata_fn *set_metadata;
	fz_document_output_intent_fn *get_output_intent;
	fz_document_output_accelerator_fn *output_accelerator;
	int did_layout;
	int is_reflowable;

	/* Linked list of currently open pages. These are not
	 * references, but just a linked list of open pages,
	 * maintained by fz_load_chapter_page, and fz_drop_page.
	 * Every page holds a kept reference to the document, so
	 * the document cannot be destroyed while a page exists.
	 * Incomplete pages are NOT inserted into this list, but
	 * do still hold a real document reference. */
	fz_page *open;
};

struct fz_document_handler
{
	fz_document_recognize_fn *recognize;
	fz_document_open_fn *open;
	fz_document_open_with_stream_fn *open_with_stream;
	const char **extensions;
	const char **mimetypes;
	fz_document_open_accel_fn *open_accel;
	fz_document_open_accel_with_stream_fn *open_accel_with_stream;
	fz_document_recognize_content_fn *recognize_content;
};

#endif
