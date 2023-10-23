// Copyright (C) 2004-2021 Artifex Software, Inc.
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

#include "mupdf/fitz.h"
#include "xps-imp.h"

#include <string.h>

static void xps_init_document(fz_context *ctx, xps_document *doc);

static xps_part *
xps_new_part(fz_context *ctx, xps_document *doc, char *name, fz_buffer *data)
{
	xps_part *part;

	part = fz_malloc_struct(ctx, xps_part);
	fz_try(ctx)
	{
		part->name = fz_strdup(ctx, name);
		part->data = data; /* take ownership of buffer */
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, data);
		fz_free(ctx, part);
		fz_rethrow(ctx);
	}

	return part;
}

void
xps_drop_part(fz_context *ctx, xps_document *doc, xps_part *part)
{
	fz_free(ctx, part->name);
	fz_drop_buffer(ctx, part->data);
	fz_free(ctx, part);
}

xps_part *
xps_read_part(fz_context *ctx, xps_document *doc, char *partname)
{
	fz_archive *zip = doc->zip;
	fz_buffer *buf = NULL;
	fz_buffer *tmp = NULL;
	char path[2048];
	int count;
	char *name;
	int seen_last;

	fz_var(buf);
	fz_var(tmp);

	name = partname;
	if (name[0] == '/')
		name ++;

	fz_try(ctx)
	{
		/* All in one piece */
		if (fz_has_archive_entry(ctx, zip, name))
		{
			buf = fz_read_archive_entry(ctx, zip, name);
		}

		/* Assemble all the pieces */
		else
		{
			buf = fz_new_buffer(ctx, 512);
			seen_last = 0;
			for (count = 0; !seen_last; ++count)
			{
				fz_snprintf(path, sizeof path, "%s/[%d].piece", name, count);
				if (fz_has_archive_entry(ctx, zip, path))
				{
					tmp = fz_read_archive_entry(ctx, zip, path);
					fz_append_buffer(ctx, buf, tmp);
					fz_drop_buffer(ctx, tmp);
					tmp = NULL;
				}
				else
				{
					fz_snprintf(path, sizeof path, "%s/[%d].last.piece", name, count);
					if (fz_has_archive_entry(ctx, zip, path))
					{
						tmp = fz_read_archive_entry(ctx, zip, path);
						fz_append_buffer(ctx, buf, tmp);
						fz_drop_buffer(ctx, tmp);
						tmp = NULL;
						seen_last = 1;
					}
					else
						fz_throw(ctx, FZ_ERROR_GENERIC, "cannot find all pieces for part '%s'", partname);
				}
			}
		}

	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, tmp);
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}

	return xps_new_part(ctx, doc, partname, buf);
}

int
xps_has_part(fz_context *ctx, xps_document *doc, char *name)
{
	char buf[2048];
	if (name[0] == '/')
		name++;
	if (fz_has_archive_entry(ctx, doc->zip, name))
		return 1;
	fz_snprintf(buf, sizeof buf, "%s/[0].piece", name);
	if (fz_has_archive_entry(ctx, doc->zip, buf))
		return 1;
	fz_snprintf(buf, sizeof buf, "%s/[0].last.piece", name);
	if (fz_has_archive_entry(ctx, doc->zip, buf))
		return 1;
	return 0;
}

static fz_document *
xps_open_document_with_directory(fz_context *ctx, const char *directory)
{
	xps_document *doc;

	doc = fz_malloc_struct(ctx, xps_document);
	xps_init_document(ctx, doc);

	fz_try(ctx)
	{
		doc->zip = fz_open_directory(ctx, directory);
		xps_read_page_list(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_drop_document(ctx, &doc->super);
		fz_rethrow(ctx);
	}

	return (fz_document*)doc;
}

fz_document *
xps_open_document_with_stream(fz_context *ctx, fz_stream *file)
{
	xps_document *doc;

	doc = fz_malloc_struct(ctx, xps_document);
	xps_init_document(ctx, doc);

	fz_try(ctx)
	{
		doc->zip = fz_open_zip_archive_with_stream(ctx, file);
		xps_read_page_list(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_drop_document(ctx, &doc->super);
		fz_rethrow(ctx);
	}

	return (fz_document*)doc;
}

fz_document *
xps_open_document(fz_context *ctx, const char *filename)
{
	fz_stream *file;
	char *p;
	fz_document *doc = NULL;

	p = strstr(filename, "/_rels/.rels");
	if (p == NULL)
		p = strstr(filename, "\\_rels\\.rels");
	if (p)
	{
		char *buf = fz_strdup(ctx, filename);
		buf[p-filename] = 0;
		fz_try(ctx)
			doc = xps_open_document_with_directory(ctx, buf);
		fz_always(ctx)
			fz_free(ctx, buf);
		fz_catch(ctx)
			fz_rethrow(ctx);
		return doc;
	}

	file = fz_open_file(ctx, filename);

	fz_try(ctx)
		doc = xps_open_document_with_stream(ctx, file);
	fz_always(ctx)
		fz_drop_stream(ctx, file);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return (fz_document*)doc;
}

static void
xps_drop_document(fz_context *ctx, fz_document *doc_)
{
	xps_document *doc = (xps_document*)doc_;
	xps_font_cache *font, *next;

	if (doc->zip)
		fz_drop_archive(ctx, doc->zip);

	font = doc->font_table;
	while (font)
	{
		next = font->next;
		fz_drop_font(ctx, font->font);
		fz_free(ctx, font->name);
		fz_free(ctx, font);
		font = next;
	}

	xps_drop_page_list(ctx, doc);

	fz_free(ctx, doc->start_part);
}

static int
xps_lookup_metadata(fz_context *ctx, fz_document *doc_, const char *key, char *buf, size_t size)
{
	if (!strcmp(key, FZ_META_FORMAT))
		return 1 + (int)fz_strlcpy(buf, "XPS", size);
	return -1;
}

static void
xps_init_document(fz_context *ctx, xps_document *doc)
{
	doc->super.refs = 1;
	doc->super.drop_document = xps_drop_document;
	doc->super.load_outline = xps_load_outline;
	doc->super.resolve_link_dest = xps_lookup_link_target;
	doc->super.count_pages = xps_count_pages;
	doc->super.load_page = xps_load_page;
	doc->super.lookup_metadata = xps_lookup_metadata;
}
