#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <string.h>

enum
{
	FZ_DOCUMENT_HANDLER_MAX = 10
};

#define DEFW (450)
#define DEFH (600)
#define DEFEM (12)

struct fz_document_handler_context_s
{
	int refs;
	int count;
	const fz_document_handler *handler[FZ_DOCUMENT_HANDLER_MAX];
};

void fz_new_document_handler_context(fz_context *ctx)
{
	ctx->handler = fz_malloc_struct(ctx, fz_document_handler_context);
	ctx->handler->refs = 1;
}

fz_document_handler_context *fz_keep_document_handler_context(fz_context *ctx)
{
	if (!ctx || !ctx->handler)
		return NULL;
	return fz_keep_imp(ctx, ctx->handler, &ctx->handler->refs);
}

void fz_drop_document_handler_context(fz_context *ctx)
{
	if (!ctx)
		return;

	if (fz_drop_imp(ctx, ctx->handler, &ctx->handler->refs))
	{
		fz_free(ctx, ctx->handler);
		ctx->handler = NULL;
	}
}

/*
	Register a handler
	for a document type.

	handler: The handler to register.
*/
void fz_register_document_handler(fz_context *ctx, const fz_document_handler *handler)
{
	fz_document_handler_context *dc;
	int i;

	if (!handler)
		return;

	dc = ctx->handler;
	if (dc == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Document handler list not found");

	for (i = 0; i < dc->count; i++)
		if (dc->handler[i] == handler)
			return;

	if (dc->count >= FZ_DOCUMENT_HANDLER_MAX)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Too many document handlers");

	dc->handler[dc->count++] = handler;
}

/*
	Given a magic find a document
	handler that can handle a document of this type.

	magic: Can be a filename extension (including initial period) or
	a mimetype.
*/
const fz_document_handler *
fz_recognize_document(fz_context *ctx, const char *magic)
{
	fz_document_handler_context *dc;
	int i, best_score, best_i;
	const char *ext, *needle;

	dc = ctx->handler;
	if (dc->count == 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "No document handlers registered");

	ext = strrchr(magic, '.');
	if (ext)
		needle = ext + 1;
	else
		needle = magic;

	best_score = 0;
	best_i = -1;

	for (i = 0; i < dc->count; i++)
	{
		int score = 0;
		const char **entry;

		if (dc->handler[i]->recognize)
			score = dc->handler[i]->recognize(ctx, magic);

		if (!ext)
		{
			for (entry = &dc->handler[i]->mimetypes[0]; *entry; entry++)
				if (!fz_strcasecmp(needle, *entry) && score < 100)
				{
					score = 100;
					break;
				}
		}

		for (entry = &dc->handler[i]->extensions[0]; *entry; entry++)
			if (!fz_strcasecmp(needle, *entry) && score < 100)
			{
				score = 100;
				break;
			}

		if (best_score < score)
		{
			best_score = score;
			best_i = i;
		}
	}

	if (best_i < 0)
		return NULL;

	return dc->handler[best_i];
}

#if FZ_ENABLE_PDF
extern fz_document_handler pdf_document_handler;
#endif

/*
	Open a PDF, XPS or CBZ document.

	Open a document using the specified stream object rather than
	opening a file on disk.

	magic: a string used to detect document type; either a file name or mime-type.
*/
fz_document *
fz_open_accelerated_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream, fz_stream *accel)
{
	const fz_document_handler *handler;

	if (magic == NULL || stream == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no document to open");

	handler = fz_recognize_document(ctx, magic);
	if (!handler)
#if FZ_ENABLE_PDF
		handler = &pdf_document_handler;
#else
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find document handler for file type: %s", magic);
#endif
	if (handler->open_accel_with_stream)
		if (accel || handler->open_with_stream == NULL)
			return handler->open_accel_with_stream(ctx, stream, accel);
	if (accel)
	{
		/* We've had an accelerator passed to a format that doesn't
		 * handle it. This should never happen, as how did the
		 * accelerator get created? */
		fz_drop_stream(ctx, accel);
	}
	return handler->open_with_stream(ctx, stream);
}

/*
	Open a PDF, XPS or CBZ document.

	Open a document using the specified stream object rather than
	opening a file on disk.

	magic: a string used to detect document type; either a file name or mime-type.
*/
fz_document *
fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream)
{
	return fz_open_accelerated_document_with_stream(ctx, magic, stream, NULL);
}

/*
	Open a PDF, XPS or CBZ document.

	Open a document file and read its basic structure so pages and
	objects can be located. MuPDF will try to repair broken
	documents (without actually changing the file contents).

	The returned fz_document is used when calling most other
	document related functions.

	filename: a path to a file as it would be given to open(2).
*/
fz_document *
fz_open_accelerated_document(fz_context *ctx, const char *filename, const char *accel)
{
	const fz_document_handler *handler;
	fz_stream *file;
	fz_stream *afile = NULL;
	fz_document *doc = NULL;

	fz_var(afile);

	if (filename == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no document to open");

	handler = fz_recognize_document(ctx, filename);
	if (!handler)
#if FZ_ENABLE_PDF
		handler = &pdf_document_handler;
#else
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find document handler for file: %s", filename);
#endif

	if (accel) {
		if (handler->open_accel)
			return handler->open_accel(ctx, filename, accel);
		if (handler->open_accel_with_stream == NULL)
		{
			/* We're not going to be able to use the accelerator - this
			 * should never happen, as how can one have been created? */
			accel = NULL;
		}
	}
	if (!accel && handler->open)
		return handler->open(ctx, filename);

	file = fz_open_file(ctx, filename);

	fz_try(ctx)
	{
		if (accel || handler->open_with_stream == NULL)
		{
			if (accel)
				afile = fz_open_file(ctx, accel);
			doc = handler->open_accel_with_stream(ctx, file, afile);
		}
		else
			doc = handler->open_with_stream(ctx, file);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, afile);
		fz_drop_stream(ctx, file);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return doc;
}

/*
	Open a PDF, XPS or CBZ document.

	Open a document file and read its basic structure so pages and
	objects can be located. MuPDF will try to repair broken
	documents (without actually changing the file contents).

	The returned fz_document is used when calling most other
	document related functions.

	filename: a path to a file as it would be given to open(2).
*/
fz_document *
fz_open_document(fz_context *ctx, const char *filename)
{
	return fz_open_accelerated_document(ctx, filename, NULL);
}

void fz_save_accelerator(fz_context *ctx, fz_document *doc, const char *accel)
{
	if (doc == NULL)
		return;
	if (doc->output_accelerator == NULL)
		return;

	fz_output_accelerator(ctx, doc, fz_new_output_with_path(ctx, accel, 0));
}

void fz_output_accelerator(fz_context *ctx, fz_document *doc, fz_output *accel)
{
	if (doc == NULL || accel == NULL)
		return;
	if (doc->output_accelerator == NULL)
	{
		fz_drop_output(ctx, accel);
		fz_throw(ctx, FZ_ERROR_GENERIC, "Document does not support writing an accelerator");
	}

	doc->output_accelerator(ctx, doc, accel);
}

int fz_document_supports_accelerator(fz_context *ctx, fz_document *doc)
{
	if (doc == NULL)
		return 0;
	return (doc->output_accelerator) != NULL;
}

void *
fz_new_document_of_size(fz_context *ctx, int size)
{
	fz_document *doc = fz_calloc(ctx, 1, size);
	doc->refs = 1;
	return doc;
}

fz_document *
fz_keep_document(fz_context *ctx, fz_document *doc)
{
	return fz_keep_imp(ctx, doc, &doc->refs);
}

void
fz_drop_document(fz_context *ctx, fz_document *doc)
{
	if (fz_drop_imp(ctx, doc, &doc->refs))
	{
		if (doc->drop_document)
			doc->drop_document(ctx, doc);
		fz_free(ctx, doc);
	}
}

static void
fz_ensure_layout(fz_context *ctx, fz_document *doc)
{
	if (doc && doc->layout && !doc->did_layout)
	{
		doc->layout(ctx, doc, DEFW, DEFH, DEFEM);
		doc->did_layout = 1;
	}
}

/*
	Is the document reflowable.

	Returns 1 to indicate reflowable documents, otherwise 0.
*/
int
fz_is_document_reflowable(fz_context *ctx, fz_document *doc)
{
	return doc ? doc->is_reflowable : 0;
}

/*
	Create a bookmark for the given page, which can be used to find the
	same location after the document has been laid out with different
	parameters.
*/
fz_bookmark fz_make_bookmark(fz_context *ctx, fz_document *doc, fz_location loc)
{
	if (doc && doc->make_bookmark)
		return doc->make_bookmark(ctx, doc, loc);
	return (loc.chapter<<16) + loc.page;
}

/*
	Find a bookmark and return its page number.
*/
fz_location fz_lookup_bookmark(fz_context *ctx, fz_document *doc, fz_bookmark mark)
{
	if (doc && doc->lookup_bookmark)
		return doc->lookup_bookmark(ctx, doc, mark);
	return fz_make_location((mark>>16) & 0xffff, mark & 0xffff);
}

/*
	Check if a document is encrypted with a
	non-blank password.
*/
int
fz_needs_password(fz_context *ctx, fz_document *doc)
{
	if (doc && doc->needs_password)
		return doc->needs_password(ctx, doc);
	return 0;
}

/*
	Test if the given password can
	decrypt the document.

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
int
fz_authenticate_password(fz_context *ctx, fz_document *doc, const char *password)
{
	if (doc && doc->authenticate_password)
		return doc->authenticate_password(ctx, doc, password);
	return 1;
}

/*
	Check permission flags on document.
*/
int
fz_has_permission(fz_context *ctx, fz_document *doc, fz_permission p)
{
	if (doc && doc->has_permission)
		return doc->has_permission(ctx, doc, p);
	return 1;
}

/*
	Load the hierarchical document outline.

	Should be freed by fz_drop_outline.
*/
fz_outline *
fz_load_outline(fz_context *ctx, fz_document *doc)
{
	fz_ensure_layout(ctx, doc);
	if (doc && doc->load_outline)
		return doc->load_outline(ctx, doc);
	return NULL;
}

/*
	Resolve an internal link to a page number.

	xp, yp: Pointer to store coordinate of destination on the page.

	Returns (-1,-1) if the URI cannot be resolved.
*/
fz_location
fz_resolve_link(fz_context *ctx, fz_document *doc, const char *uri, float *xp, float *yp)
{
	fz_ensure_layout(ctx, doc);
	if (xp) *xp = 0;
	if (yp) *yp = 0;
	if (doc && doc->resolve_link)
		return doc->resolve_link(ctx, doc, uri, xp, yp);
	return fz_make_location(-1, -1);
}

/*
	Layout reflowable document types.

	w, h: Page size in points.
	em: Default font size in points.
*/
void
fz_layout_document(fz_context *ctx, fz_document *doc, float w, float h, float em)
{
	if (doc && doc->layout)
	{
		doc->layout(ctx, doc, w, h, em);
		doc->did_layout = 1;
	}
}

/*
	Return the number of chapters in the document.
	At least 1.
*/
int
fz_count_chapters(fz_context *ctx, fz_document *doc)
{
	fz_ensure_layout(ctx, doc);
	if (doc && doc->count_chapters)
		return doc->count_chapters(ctx, doc);
	return 1;
}

/*
	Return the number of pages in a chapter.
	May return 0.
*/
int
fz_count_chapter_pages(fz_context *ctx, fz_document *doc, int chapter)
{
	fz_ensure_layout(ctx, doc);
	if (doc && doc->count_pages)
		return doc->count_pages(ctx, doc, chapter);
	return 0;
}


/*
	Return the number of pages in document

	May return 0 for documents with no pages.
*/
int
fz_count_pages(fz_context *ctx, fz_document *doc)
{
	int i, c, n = 0;
	c = fz_count_chapters(ctx, doc);
	for (i = 0; i < c; ++i)
		n += fz_count_chapter_pages(ctx, doc, i);
	return n;
}

fz_page *
fz_load_page(fz_context *ctx, fz_document *doc, int number)
{
	int i, n = fz_count_chapters(ctx, doc);
	int start = 0;
	for (i = 0; i < n; ++i)
	{
		int m = fz_count_chapter_pages(ctx, doc, i);
		if (number < start + m)
			return fz_load_chapter_page(ctx, doc, i, number - start);
		start += m;
	}
	fz_throw(ctx, FZ_ERROR_GENERIC, "Page not found: %d", number+1);
}

fz_location fz_last_page(fz_context *ctx, fz_document *doc)
{
	int nc = fz_count_chapters(ctx, doc);
	int np = fz_count_chapter_pages(ctx, doc, nc-1);
	return fz_make_location(nc-1, np-1);
}

fz_location fz_next_page(fz_context *ctx, fz_document *doc, fz_location loc)
{
	int nc = fz_count_chapters(ctx, doc);
	int np = fz_count_chapter_pages(ctx, doc, loc.chapter);
	if (loc.page + 1 == np)
	{
		if (loc.chapter + 1 < nc)
		{
			return fz_make_location(loc.chapter + 1, 0);
		}
	}
	else
	{
		return fz_make_location(loc.chapter, loc.page + 1);
	}
	return loc;
}

fz_location fz_previous_page(fz_context *ctx, fz_document *doc, fz_location loc)
{
	if (loc.page == 0)
	{
		if (loc.chapter > 0)
		{
			int np = fz_count_chapter_pages(ctx, doc, loc.chapter - 1);
			return fz_make_location(loc.chapter - 1, np - 1);
		}
	}
	else
	{
		return fz_make_location(loc.chapter, loc.page - 1);
	}
	return loc;
}

fz_location fz_clamp_location(fz_context *ctx, fz_document *doc, fz_location loc)
{
	int nc = fz_count_chapters(ctx, doc);
	int np;
	if (loc.chapter < 0) loc.chapter = 0;
	if (loc.chapter >= nc) loc.chapter = nc - 1;
	np = fz_count_chapter_pages(ctx, doc, loc.chapter);
	if (loc.page < 0) loc.page = 0;
	if (loc.page >= np) loc.page = np - 1;
	return loc;
}

fz_location fz_location_from_page_number(fz_context *ctx, fz_document *doc, int number)
{
	int i, m = 0, n = fz_count_chapters(ctx, doc);
	int start = 0;
	if (number < 0)
		number = 0;
	for (i = 0; i < n; ++i)
	{
		m = fz_count_chapter_pages(ctx, doc, i);
		if (number < start + m)
			return fz_make_location(i, number - start);
		start += m;
	}
	return fz_make_location(i-1, m-1);
}

int fz_page_number_from_location(fz_context *ctx, fz_document *doc, fz_location loc)
{
	int i, n, start = 0;
	n = fz_count_chapters(ctx, doc);
	for (i = 0; i < n; ++i)
	{
		if (i == loc.chapter)
			return start + loc.page;
		start += fz_count_chapter_pages(ctx, doc, i);
	}
	return -1;
}

/*
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

	buf: The buffer to hold the results (a nul-terminated UTF-8 string).

	size: Size of 'buf'.

	Returns the size of the output string (may be larger than 'size' if
	the output was truncated), or -1 if the key is not recognized or found.
*/
int
fz_lookup_metadata(fz_context *ctx, fz_document *doc, const char *key, char *buf, int size)
{
	if (buf && size > 0)
		buf[0] = 0;
	if (doc && doc->lookup_metadata)
		return doc->lookup_metadata(ctx, doc, key, buf, size);
	return -1;
}

/*
	Find the output intent colorspace if the document has defined one.
*/
fz_colorspace *
fz_document_output_intent(fz_context *ctx, fz_document *doc)
{
	if (doc && doc->get_output_intent)
		return doc->get_output_intent(ctx, doc);
	return NULL;
}

/*
	Load a page.

	After fz_load_page is it possible to retrieve the size of the
	page using fz_bound_page, or to render the page using
	fz_run_page_*. Free the page by calling fz_drop_page.

	chapter: chapter number, 0 is the first chapter of the document.
	number: page number, 0 is the first page of the chapter.
*/
fz_page *
fz_load_chapter_page(fz_context *ctx, fz_document *doc, int chapter, int number)
{
	fz_page *page;

	fz_ensure_layout(ctx, doc);

	if (doc)
		for (page = doc->open; page; page = page->next)
			if (page->chapter == chapter && page->number == number)
				return fz_keep_page(ctx, page);

	if (doc && doc->load_page)
	{
		page = doc->load_page(ctx, doc, chapter, number);
		page->chapter = chapter;
		page->number = number;

		/* Insert new page at the head of the list of open pages. */
		if (!page->incomplete)
		{
			if ((page->next = doc->open) != NULL)
				doc->open->prev = &page->next;
			doc->open = page;
			page->prev = &doc->open;
		}
		return page;
	}

	return NULL;
}

/*
	Load the list of links for a page.

	Returns a linked list of all the links on the page, each with
	its clickable region and link destination. Each link is
	reference counted so drop and free the list of links by
	calling fz_drop_link on the pointer return from fz_load_links.

	page: Page obtained from fz_load_page.
*/
fz_link *
fz_load_links(fz_context *ctx, fz_page *page)
{
	if (page && page->load_links)
		return page->load_links(ctx, page);
	return NULL;
}

/*
	Determine the size of a page at 72 dpi.
*/
fz_rect
fz_bound_page(fz_context *ctx, fz_page *page)
{
	if (page && page->bound_page)
		return page->bound_page(ctx, page);
	return fz_empty_rect;
}

/*
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
void
fz_run_page_contents(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	if (page && page->run_page_contents)
	{
		fz_try(ctx)
		{
			page->run_page_contents(ctx, page, dev, transform, cookie);
		}
		fz_catch(ctx)
		{
			dev->close_device = NULL; /* aborted run, don't warn about unclosed device */
			if (fz_caught(ctx) != FZ_ERROR_ABORT)
				fz_rethrow(ctx);
		}
	}
}

/*
	Run the annotations on a page through a device.
*/
void
fz_run_page_annots(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	if (page && page->run_page_annots)
	{
		fz_try(ctx)
		{
			page->run_page_annots(ctx, page, dev, transform, cookie);
		}
		fz_catch(ctx)
		{
			dev->close_device = NULL; /* aborted run, don't warn about unclosed device */
			if (fz_caught(ctx) != FZ_ERROR_ABORT)
				fz_rethrow(ctx);
		}
	}
}

/*
	Run the widgets on a page through a device.
*/
void
fz_run_page_widgets(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	if (page && page->run_page_widgets)
	{
		fz_try(ctx)
		{
			page->run_page_widgets(ctx, page, dev, transform, cookie);
		}
		fz_catch(ctx)
		{
			dev->close_device = NULL; /* aborted run, don't warn about unclosed device */
			if (fz_caught(ctx) != FZ_ERROR_ABORT)
				fz_rethrow(ctx);
		}
	}
}

/*
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
void
fz_run_page(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	fz_run_page_contents(ctx, page, dev, transform, cookie);
	fz_run_page_annots(ctx, page, dev, transform, cookie);
	fz_run_page_widgets(ctx, page, dev, transform, cookie);
}

fz_page *
fz_new_page_of_size(fz_context *ctx, int size)
{
	fz_page *page = Memento_label(fz_calloc(ctx, 1, size), "fz_page");
	page->refs = 1;
	return page;
}

fz_page *
fz_keep_page(fz_context *ctx, fz_page *page)
{
	return fz_keep_imp(ctx, page, &page->refs);
}

void
fz_drop_page(fz_context *ctx, fz_page *page)
{
	if (fz_drop_imp(ctx, page, &page->refs))
	{
		/* Remove page from the list of open pages */
		if (page->next != NULL)
			page->next->prev = page->prev;
		if (page->prev != NULL)
			*page->prev = page->next;

		if (page->drop_page)
			page->drop_page(ctx, page);

		fz_free(ctx, page);
	}
}

/*
	Get the presentation details for a given page.

	transition: A pointer to a transition struct to fill out.

	duration: A pointer to a place to set the page duration in seconds.
	Will be set to 0 if no transition is specified for the page.

	Returns: a pointer to the transition structure, or NULL if there is no
	transition specified for the page.
*/
fz_transition *
fz_page_presentation(fz_context *ctx, fz_page *page, fz_transition *transition, float *duration)
{
	float dummy;
	if (duration)
		*duration = 0;
	else
		duration = &dummy;
	if (page && page->page_presentation && page)
		return page->page_presentation(ctx, page, transition, duration);
	return NULL;
}

/*
	Get the separations details for a page.
	This will be NULL, unless the format specifically supports
	separations (such as PDF files). May be NULL even
	so, if there are no separations on a page.

	Returns a reference that must be dropped.
*/
fz_separations *
fz_page_separations(fz_context *ctx, fz_page *page)
{
	if (page && page->separations)
		return page->separations(ctx, page);
	return NULL;
}

int fz_page_uses_overprint(fz_context *ctx, fz_page *page)
{
	if (page && page->overprint)
		return page->overprint(ctx, page);
	return 0;
}
