// Copyright (C) 2023-2026 Artifex Software, Inc.
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
#include "html-imp.h"

#ifdef FZ_ENABLE_MD

#include "cmark-gfm.h"
#include "cmark-gfm-core-extensions.h"
#include "registry.h"

#include <ctype.h>

/* Defaults are all 0's. FIXME: Very subject to change. Possibly might be removed entirely. */
typedef struct
{
	int dummy;
}
fz_md_to_html_opts;

static void
add_extension(fz_context *ctx, cmark_parser *parser, const char *ext)
{
	cmark_syntax_extension *syntax_extension = cmark_find_syntax_extension(ext);
	if (!syntax_extension)
		fz_throw(ctx, FZ_ERROR_LIBRARY, "cmark %s extension not found", ext);
	cmark_parser_attach_syntax_extension(parser, syntax_extension);
}

static void
register_plugins(fz_context *ctx)
{
	static int cmark_plugin_registration_once = 0;

	// Abuse the freetype lock here.
	fz_lock(ctx, FZ_LOCK_FREETYPE);
	if (cmark_plugin_registration_once)
	{
		fz_unlock(ctx, FZ_LOCK_FREETYPE);
	}
	else
	{
		fz_try(ctx)
		{
			cmark_gfm_core_extensions_ensure_registered();
			cmark_plugin_registration_once = 1;
			atexit(cmark_release_plugins);
		}
		fz_always(ctx)
			fz_unlock(ctx, FZ_LOCK_FREETYPE);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
}

static fz_buffer *
fz_md_to_html(fz_context *ctx, fz_html_font_set *set, fz_buffer *buffer_in, fz_archive *dir, fz_md_to_html_opts *opts)
{
	fz_buffer *buffer_out = NULL;
	size_t i, len;
	char *src, *out = NULL;
	cmark_parser *parser = NULL;
	cmark_node *document = NULL;
	/* CMark provides a way to redirect allocation, but
	 * stupidly, provides no way to pass in any opaque
	 * data, so we can't pass an fz_context. So might
	 * as well live with the defaults for now. */
	cmark_mem *mem = cmark_get_default_mem_allocator();

	fz_var(buffer_out);
	fz_var(out);
	fz_var(parser);
	fz_var(document);

	fz_terminate_buffer(ctx, buffer_in);
	len = buffer_in->len-1;
	src = (char *)buffer_in->data;
	for (i = 0; i < len; i++)
		if (src[i] == 0)
			src[i] = '\n';

	fz_try(ctx)
	{
		int options = CMARK_OPT_UNSAFE | CMARK_OPT_LIBERAL_HTML_TAG | CMARK_OPT_FOOTNOTES;

		register_plugins(ctx);

		parser = cmark_parser_new_with_mem(options, mem);
		add_extension(ctx, parser, "table");
		add_extension(ctx, parser, "strikethrough");
		add_extension(ctx, parser, "autolink");
		add_extension(ctx, parser, "tagfilter");
		add_extension(ctx, parser, "tasklist");
		add_extension(ctx, parser, "autoheaderid");

		cmark_parser_feed(parser, src, len);

		document = cmark_parser_finish(parser);

		out = cmark_render_html_with_mem(document, options, cmark_parser_get_syntax_extensions(parser), mem);

		buffer_out = fz_new_buffer_from_copied_data(ctx, (unsigned char *)out, strlen(out)+1);
	}
	fz_always(ctx)
	{
		if (parser)
			cmark_parser_free(parser);
		if (document)
			cmark_node_free(document);
		mem->free(out);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

#ifndef NDEBUG
	if (fz_atoi(getenv("FZ_DEBUG_MARKDOWN")))
		fz_write_buffer(ctx, fz_stdout(ctx), buffer_out);
#endif

	return buffer_out;
}

/* MD document handler */

static fz_buffer *
md_to_html(fz_context *ctx, fz_html_font_set *set, fz_buffer *buf, fz_archive *zip)
{
	fz_md_to_html_opts opts = { 0 };

	return fz_md_to_html(ctx, set, buf, zip, &opts);
}

static const fz_htdoc_format_t fz_htdoc_md =
{
	"Markdown document",
	md_to_html,
	0, 1,
	FZ_HTML_FLAVOR_MARKDOWN
};

static fz_document *
md_open_document(fz_context *ctx, const fz_document_handler *handler, fz_stream *file, fz_stream *accel, fz_archive *zip, void *state)
{
	return fz_htdoc_open_document_with_stream_and_dir(ctx, file, zip, &fz_htdoc_md);
}

static const char *md_extensions[] =
{
	"md",
	NULL
};

static const char *md_mimetypes[] =
{
	"text/markdown",
	NULL
};

/* We are only ever 75% sure here, to allow a 'better' handler, such as sodochandler
 * to override us by returning 100. */
static int
md_recognize_doc_content(fz_context *ctx, const fz_document_handler *handler, fz_stream *stream, fz_archive *zip, void **state, fz_document_recognize_state_free_fn **free_state)
{
	int ret = 0;

	if (state)
		*state = NULL;
	if (free_state)
		*free_state = NULL;

	if (stream == NULL)
		return 0;

	fz_var(ret);

	fz_try(ctx)
	{
		// Really crap markdown detector.
		// Assume the first line of the file will be a heading,
		// so will be <whitespace>#+<whitespace>.
		int c = fz_read_byte(ctx, stream);

		if (c == EOF)
			break;

		while (c != EOF && isspace(c))
			c = fz_read_byte(ctx, stream);

		if (c != '#')
			break;

		while (c != EOF && c == '#')
			c = fz_read_byte(ctx, stream);

		if (c == EOF || !isspace(c))
			break;

		ret = 50;
	}
	fz_always(ctx)
	{
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

fz_document_handler md_document_handler =
{
	NULL,
	md_open_document,
	md_extensions,
	md_mimetypes,
	md_recognize_doc_content
};

#endif // FZ_ENABLE_MD
