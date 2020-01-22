#include "mupdf/fitz.h"

#include <string.h>

/* Return non-null terminated pointers to key/value entries in comma separated
 * option string. A plain key has the default value 'yes'. Use strncmp to compare
 * key/value strings. */
static const char *
fz_get_option(fz_context *ctx, const char **key, const char **val, const char *opts)
{
	if (!opts || *opts == 0)
		return NULL;

	if (*opts == ',')
		++opts;

	*key = opts;
	while (*opts != 0 && *opts != ',' && *opts != '=')
		++opts;

	if (*opts == '=')
	{
		*val = ++opts;
		while (*opts != 0 && *opts != ',')
			++opts;
	}
	else
	{
		*val = "yes";
	}

	return opts;
}

int
fz_has_option(fz_context *ctx, const char *opts, const char *key, const char **val)
{
	const char *straw;
	size_t n = strlen(key);
	while ((opts = fz_get_option(ctx, &straw, val, opts)))
		if (!strncmp(straw, key, n) && (straw[n] == '=' || straw[n] == ',' || straw[n] == 0))
			return 1;
	return 0;
}

int
fz_option_eq(const char *a, const char *b)
{
	size_t n = strlen(b);
	return !strncmp(a, b, n) && (a[n] == ',' || a[n] == 0);
}

size_t
fz_copy_option(fz_context *ctx, const char *val, char *dest, size_t maxlen)
{
	const char *e = val;
	size_t len, len2;

	if (val == NULL) {
		if (maxlen)
			*dest = 0;
		return 0;
	}

	while (*e != ',' && *e != 0)
		e++;

	len = e-val;
	len2 = len+1; /* Allow for terminator */
	if (len > maxlen)
		len = maxlen;
	memcpy(dest, val, len);
	if (len < maxlen)
		memset(dest+len, 0, maxlen-len);

	return len2 >= maxlen ? len2 - maxlen : 0;
}

/*
	Internal function to allocate a
	block for a derived document_writer structure, with the base
	structure's function pointers populated correctly, and the extra
	space zero initialised.
*/
fz_document_writer *fz_new_document_writer_of_size(fz_context *ctx, size_t size, fz_document_writer_begin_page_fn *begin_page,
	fz_document_writer_end_page_fn *end_page, fz_document_writer_close_writer_fn *close, fz_document_writer_drop_writer_fn *drop)
{
	fz_document_writer *wri = Memento_label(fz_calloc(ctx, 1, size), "fz_document_writer");

	wri->begin_page = begin_page;
	wri->end_page = end_page;
	wri->close_writer = close;
	wri->drop_writer = drop;

	return wri;
}

fz_document_writer *fz_new_png_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.png", 0, fz_save_pixmap_as_png);
}

fz_document_writer *fz_new_pam_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pam", 0, fz_save_pixmap_as_pam);
}

fz_document_writer *fz_new_pnm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pnm", 0, fz_save_pixmap_as_pnm);
}

fz_document_writer *fz_new_pgm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pgm", 1, fz_save_pixmap_as_pnm);
}

fz_document_writer *fz_new_ppm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.ppm", 3, fz_save_pixmap_as_pnm);
}

fz_document_writer *fz_new_pbm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pbm", 1, fz_save_pixmap_as_pbm);
}

fz_document_writer *fz_new_pkm_pixmap_writer(fz_context *ctx, const char *path, const char *options)
{
	return fz_new_pixmap_writer(ctx, path, options, "out-%04d.pkm", 4, fz_save_pixmap_as_pkm);
}

/*
	Create a new fz_document_writer, for a
	file of the given type.

	path: The document name to write (or NULL for default)

	format: Which format to write (currently cbz, html, pdf, pam, pbm,
	pgm, pkm, png, ppm, pnm, svg, text, xhtml)

	options: NULL, or pointer to comma separated string to control
	file generation.
*/
fz_document_writer *
fz_new_document_writer(fz_context *ctx, const char *path, const char *format, const char *options)
{
	if (!format)
	{
		format = strrchr(path, '.');
		if (!format)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot detect document format");
		format += 1; /* skip the '.' */
	}

	if (!fz_strcasecmp(format, "cbz"))
		return fz_new_cbz_writer(ctx, path, options);
#if FZ_ENABLE_PDF
	if (!fz_strcasecmp(format, "pdf"))
		return fz_new_pdf_writer(ctx, path, options);
#endif
	if (!fz_strcasecmp(format, "svg"))
		return fz_new_svg_writer(ctx, path, options);

	if (!fz_strcasecmp(format, "png"))
		return fz_new_png_pixmap_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "pam"))
		return fz_new_pam_pixmap_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "pnm"))
		return fz_new_pnm_pixmap_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "pgm"))
		return fz_new_pgm_pixmap_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "ppm"))
		return fz_new_ppm_pixmap_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "pbm"))
		return fz_new_pbm_pixmap_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "pkm"))
		return fz_new_pkm_pixmap_writer(ctx, path, options);

	if (!fz_strcasecmp(format, "pcl"))
		return fz_new_pcl_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "pclm"))
		return fz_new_pclm_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "ps"))
		return fz_new_ps_writer(ctx, path, options);
	if (!fz_strcasecmp(format, "pwg"))
		return fz_new_pwg_writer(ctx, path, options);

	if (!fz_strcasecmp(format, "txt") || !fz_strcasecmp(format, "text"))
		return fz_new_text_writer(ctx, "text", path, options);
	if (!fz_strcasecmp(format, "html"))
		return fz_new_text_writer(ctx, format, path, options);
	if (!fz_strcasecmp(format, "xhtml"))
		return fz_new_text_writer(ctx, format, path, options);
	if (!fz_strcasecmp(format, "stext"))
		return fz_new_text_writer(ctx, format, path, options);

	fz_throw(ctx, FZ_ERROR_GENERIC, "unknown output document format: %s", format);
}

/*
 * Like fz_new_document_writer but takes a fz_output for writing the result.
 * Only works for multi-page formats.
 */
fz_document_writer *
fz_new_document_writer_with_output(fz_context *ctx, fz_output *out, const char *format, const char *options)
{
	if (!fz_strcasecmp(format, "cbz"))
		return fz_new_cbz_writer_with_output(ctx, out, options);
#if FZ_ENABLE_PDF
	if (!fz_strcasecmp(format, "pdf"))
		return fz_new_pdf_writer_with_output(ctx, out, options);
#endif

	if (!fz_strcasecmp(format, "pcl"))
		return fz_new_pcl_writer_with_output(ctx, out, options);
	if (!fz_strcasecmp(format, "pclm"))
		return fz_new_pclm_writer_with_output(ctx, out, options);
	if (!fz_strcasecmp(format, "ps"))
		return fz_new_ps_writer_with_output(ctx, out, options);
	if (!fz_strcasecmp(format, "pwg"))
		return fz_new_pwg_writer_with_output(ctx, out, options);

	if (!fz_strcasecmp(format, "txt") || !fz_strcasecmp(format, "text"))
		return fz_new_text_writer_with_output(ctx, "text", out, options);
	if (!fz_strcasecmp(format, "html"))
		return fz_new_text_writer_with_output(ctx, format, out, options);
	if (!fz_strcasecmp(format, "xhtml"))
		return fz_new_text_writer_with_output(ctx, format, out, options);
	if (!fz_strcasecmp(format, "stext"))
		return fz_new_text_writer_with_output(ctx, format, out, options);

	fz_throw(ctx, FZ_ERROR_GENERIC, "unknown output document format: %s", format);
}

/*
	Called to end the process of writing
	pages to a document.

	This writes any file level trailers required. After this
	completes successfully the file is up to date and complete.
*/
void
fz_close_document_writer(fz_context *ctx, fz_document_writer *wri)
{
	if (wri->close_writer)
		wri->close_writer(ctx, wri);
	wri->close_writer = NULL;
}

/*
	Called to discard a fz_document_writer.
	This may be called at any time during the process to release all
	the resources owned by the writer.

	Calling drop without having previously called close may leave
	the file in an inconsistent state.
*/
void
fz_drop_document_writer(fz_context *ctx, fz_document_writer *wri)
{
	if (!wri)
		return;

	if (wri->close_writer)
		fz_warn(ctx, "dropping unclosed document writer");
	if (wri->drop_writer)
		wri->drop_writer(ctx, wri);
	if (wri->dev)
		fz_drop_device(ctx, wri->dev);
	fz_free(ctx, wri);
}

/*
	Called to start the process of writing a page to
	a document.

	mediabox: page size rectangle in points.

	Returns a fz_device to write page contents to.
*/
fz_device *
fz_begin_page(fz_context *ctx, fz_document_writer *wri, fz_rect mediabox)
{
	if (!wri)
		return NULL;
	if (wri->dev)
		fz_throw(ctx, FZ_ERROR_GENERIC, "called begin page without ending the previous page");
	wri->dev = wri->begin_page(ctx, wri, mediabox);
	return wri->dev;
}

/*
	Called to end the process of writing a page to a
	document.
*/
void
fz_end_page(fz_context *ctx, fz_document_writer *wri)
{
	fz_device *dev;

	if (!wri)
		return;
	dev = wri->dev;
	wri->dev = NULL;
	wri->end_page(ctx, wri, dev);
}
