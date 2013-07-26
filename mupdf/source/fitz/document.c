#include "mupdf/fitz.h"

/* Yuck! Promiscuous we are. */
extern struct pdf_document *pdf_open_document(fz_context *ctx, const char *filename);
extern struct xps_document *xps_open_document(fz_context *ctx, const char *filename);
extern struct cbz_document *cbz_open_document(fz_context *ctx, const char *filename);
extern struct image_document *image_open_document(fz_context *ctx, const char *filename);

extern struct pdf_document *pdf_open_document_with_stream(fz_context *ctx, fz_stream *file);
extern struct xps_document *xps_open_document_with_stream(fz_context *ctx, fz_stream *file);
extern struct cbz_document *cbz_open_document_with_stream(fz_context *ctx, fz_stream *file);
extern struct image_document *image_open_document_with_stream(fz_context *ctx, fz_stream *file);

extern int pdf_js_supported(void);

static inline int fz_tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + 32;
	return c;
}

static inline int fz_strcasecmp(const char *a, const char *b)
{
	while (fz_tolower(*a) == fz_tolower(*b))
	{
		if (*a++ == 0)
			return 0;
		b++;
	}
	return fz_tolower(*a) - fz_tolower(*b);
}

fz_document *
fz_open_document_with_stream(fz_context *ctx, const char *magic, fz_stream *stream)
{
	char *ext = strrchr(magic, '.');

	if (ext)
	{
		if (!fz_strcasecmp(ext, ".xps") || !fz_strcasecmp(ext, ".rels") || !fz_strcasecmp(ext, ".oxps"))
			return (fz_document*) xps_open_document_with_stream(ctx, stream);
		if (!fz_strcasecmp(ext, ".cbz") || !fz_strcasecmp(ext, ".zip"))
			return (fz_document*) cbz_open_document_with_stream(ctx, stream);
		if (!fz_strcasecmp(ext, ".pdf"))
			return (fz_document*) pdf_open_document_with_stream(ctx, stream);
		if (!fz_strcasecmp(ext, ".png") || !fz_strcasecmp(ext, ".jpg") ||
			!fz_strcasecmp(ext, ".jpeg") || !fz_strcasecmp(ext, ".jfif") ||
			!fz_strcasecmp(ext, ".jfif-tbnl") || !fz_strcasecmp(ext, ".jpe") ||
			!fz_strcasecmp(ext, ".tif") || !fz_strcasecmp(ext, ".tiff"))
			return (fz_document*) image_open_document_with_stream(ctx, stream);
	}

	if (!strcmp(magic, "cbz") || !strcmp(magic, "application/x-cbz"))
		return (fz_document*) cbz_open_document_with_stream(ctx, stream);
	if (!strcmp(magic, "xps") || !strcmp(magic, "oxps") ||
		!strcmp(magic, "application/vnd.ms-xpsdocument") ||
		!strcmp(magic, "application/oxps"))
		return (fz_document*) xps_open_document_with_stream(ctx, stream);
	if (!strcmp(magic, "pdf") || !strcmp(magic, "application/pdf"))
		return (fz_document*) pdf_open_document_with_stream(ctx, stream);
	if (!strcmp(magic, "png") || !strcmp(magic, "image/png") ||
		!strcmp(magic, "jpg") || !strcmp(magic, "image/jpeg") ||
		!strcmp(magic, "jpeg") || !strcmp(magic, "image/pjpeg") ||
		!strcmp(magic, "jpe") || !strcmp(magic, "jfif") ||
		!strcmp(magic, "tif") || !strcmp(magic, "image/tiff") ||
		!strcmp(magic, "tiff") || !strcmp(magic, "image/x-tiff"))
		return (fz_document*) image_open_document_with_stream(ctx, stream);

	/* last guess: pdf */
	return (fz_document*) pdf_open_document_with_stream(ctx, stream);
}

fz_document *
fz_open_document(fz_context *ctx, const char *filename)
{
	char *ext = strrchr(filename, '.');

	if (ext)
	{
		if (!fz_strcasecmp(ext, ".xps") || !fz_strcasecmp(ext, ".rels") || !fz_strcasecmp(ext, ".oxps"))
			return (fz_document*) xps_open_document(ctx, filename);
		if (!fz_strcasecmp(ext, ".cbz") || !fz_strcasecmp(ext, ".zip"))
			return (fz_document*) cbz_open_document(ctx, filename);
		if (!fz_strcasecmp(ext, ".pdf"))
			return (fz_document*) pdf_open_document(ctx, filename);
		if (!fz_strcasecmp(ext, ".png") || !fz_strcasecmp(ext, ".jpg") ||
			!fz_strcasecmp(ext, ".jpeg") || !fz_strcasecmp(ext, ".jpe") ||
			!fz_strcasecmp(ext, ".jfif") || !fz_strcasecmp(ext, ".jfif-tbnl") ||
			!fz_strcasecmp(ext, ".tif") || !fz_strcasecmp(ext, ".tiff"))
			return (fz_document*) image_open_document(ctx, filename);
	}

	/* last guess: pdf */
	return (fz_document*) pdf_open_document(ctx, filename);
}

void
fz_close_document(fz_document *doc)
{
	if (doc && doc->close)
		doc->close(doc);
}

int
fz_needs_password(fz_document *doc)
{
	if (doc && doc->needs_password)
		return doc->needs_password(doc);
	return 0;
}

int
fz_authenticate_password(fz_document *doc, char *password)
{
	if (doc && doc->authenticate_password)
		return doc->authenticate_password(doc, password);
	return 1;
}

fz_outline *
fz_load_outline(fz_document *doc)
{
	if (doc && doc->load_outline)
		return doc->load_outline(doc);
	return NULL;
}

int
fz_count_pages(fz_document *doc)
{
	if (doc && doc->count_pages)
		return doc->count_pages(doc);
	return 0;
}

fz_page *
fz_load_page(fz_document *doc, int number)
{
	if (doc && doc->load_page)
		return doc->load_page(doc, number);
	return NULL;
}

fz_link *
fz_load_links(fz_document *doc, fz_page *page)
{
	if (doc && doc->load_links && page)
		return doc->load_links(doc, page);
	return NULL;
}

fz_rect *
fz_bound_page(fz_document *doc, fz_page *page, fz_rect *r)
{
	if (doc && doc->bound_page && page && r)
		return doc->bound_page(doc, page, r);
	if (r)
		*r = fz_empty_rect;
	return r;
}

fz_annot *
fz_first_annot(fz_document *doc, fz_page *page)
{
	if (doc && doc->first_annot && page)
		return doc->first_annot(doc, page);
	return NULL;
}

fz_annot *
fz_next_annot(fz_document *doc, fz_annot *annot)
{
	if (doc && doc->next_annot && annot)
		return doc->next_annot(doc, annot);
	return NULL;
}

fz_rect *
fz_bound_annot(fz_document *doc, fz_annot *annot, fz_rect *rect)
{
	if (doc && doc->bound_annot && annot && rect)
		return doc->bound_annot(doc, annot, rect);
	if (rect)
		*rect = fz_empty_rect;
	return rect;
}

void
fz_run_page_contents(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie)
{
	if (doc && doc->run_page_contents && page)
		doc->run_page_contents(doc, page, dev, transform, cookie);
}

void
fz_run_annot(fz_document *doc, fz_page *page, fz_annot *annot, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie)
{
	if (doc && doc->run_annot && page && annot)
		doc->run_annot(doc, page, annot, dev, transform, cookie);
}

void
fz_run_page(fz_document *doc, fz_page *page, fz_device *dev, const fz_matrix *transform, fz_cookie *cookie)
{
	fz_annot *annot;
	fz_rect mediabox;

	fz_bound_page(doc, page, &mediabox);
	fz_begin_page(dev, &mediabox, transform);

	fz_run_page_contents(doc, page, dev, transform, cookie);

	if (cookie && cookie->progress_max != -1)
	{
		int count = 1;
		for (annot = fz_first_annot(doc, page); annot; annot = fz_next_annot(doc, annot))
			count++;
		cookie->progress_max += count;
	}

	for (annot = fz_first_annot(doc, page); annot; annot = fz_next_annot(doc, annot))
	{
		/* Check the cookie for aborting */
		if (cookie)
		{
			if (cookie->abort)
				break;
			cookie->progress++;
		}

		fz_run_annot(doc, page, annot, dev, transform, cookie);
	}

	fz_end_page(dev);
}

void
fz_free_page(fz_document *doc, fz_page *page)
{
	if (doc && doc->free_page && page)
		doc->free_page(doc, page);
}

int
fz_meta(fz_document *doc, int key, void *ptr, int size)
{
	if (doc && doc->meta)
		return doc->meta(doc, key, ptr, size);
	return FZ_META_UNKNOWN_KEY;
}

fz_transition *
fz_page_presentation(fz_document *doc, fz_page *page, float *duration)
{
	float dummy;
	if (duration)
		*duration = 0;
	else
		duration = &dummy;
	if (doc && doc->page_presentation && page)
		return doc->page_presentation(doc, page, duration);
	return NULL;
}

int fz_javascript_supported(void)
{
	return pdf_js_supported();
}

void
fz_write_document(fz_document *doc, char *filename, fz_write_options *opts)
{
	if (doc && doc->write)
		doc->write(doc, filename, opts);
}
