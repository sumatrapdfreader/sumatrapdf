#include "fitz-internal.h"

/* Yuck! Promiscuous we are. */
extern struct pdf_document *pdf_open_document(fz_context *ctx, char *filename);
extern struct xps_document *xps_open_document(fz_context *ctx, char *filename);
extern struct cbz_document *cbz_open_document(fz_context *ctx, char *filename);

static inline int fz_tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + 32;
	return c;
}

static inline int fz_strcasecmp(char *a, char *b)
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
fz_open_document(fz_context *ctx, char *filename)
{
	char *ext = strrchr(filename, '.');
	if (ext && (!fz_strcasecmp(ext, ".xps") || !fz_strcasecmp(ext, ".rels")))
		return (fz_document*) xps_open_document(ctx, filename);
	if (ext && !fz_strcasecmp(ext, ".cbz"))
		return (fz_document*) cbz_open_document(ctx, filename);
#if 0
	/* We used to only open pdf files if they ended in .pdf. For now,
	 * until we move to detecting filetypes by their content, we disable
	 * this code, and assume that any file that hasn't matched an
	 * extension already, is a PDF. */
	if (ext && !fz_strcasecmp(ext, ".pdf"))
		return (fz_document*) pdf_open_document(ctx, filename);
	fz_throw(ctx, "unknown document type: '%s'", filename);
	return NULL;
#else
	return (fz_document*) pdf_open_document(ctx, filename);
#endif
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

fz_rect
fz_bound_page(fz_document *doc, fz_page *page)
{
	if (doc && doc->bound_page && page)
		return doc->bound_page(doc, page);
	return fz_empty_rect;
}

void
fz_run_page(fz_document *doc, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie)
{
	if (doc && doc->run_page && page)
		doc->run_page(doc, page, dev, transform, cookie);
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

void
fz_write_document(fz_document *doc, char *filename, fz_write_options *opts)
{
	if (doc && doc->write)
		doc->write(doc, filename, opts);
}
