// Copyright (C) 2004-2022 Artifex Software, Inc.
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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#include "emscripten.h"
#include "mupdf/fitz.h"
#include <string.h>
#include <math.h>

static fz_context *ctx;

void wasm_rethrow(fz_context *ctx)
{
	if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
		EM_ASM({ throw "trylater"; });
	else
		EM_ASM({ throw new Error(UTF8ToString($0)); }, fz_caught_message(ctx));
}

EMSCRIPTEN_KEEPALIVE
void initContext(void)
{
	ctx = fz_new_context(NULL, NULL, 100<<20);
	if (!ctx)
		EM_ASM({ throw new Error("Cannot create MuPDF context!"); });
	fz_register_document_handlers(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_document *openDocumentFromBuffer(unsigned char *data, int size, char *magic)
{
	fz_document *document = NULL;
	fz_buffer *buf = NULL;
	fz_stream *stm = NULL;

	fz_var(buf);
	fz_var(stm);

	/* NOTE: We take ownership of input data! */

	fz_try(ctx)
	{
		buf = fz_new_buffer_from_data(ctx, data, size);
		stm = fz_open_buffer(ctx, buf);
		document = fz_open_document_with_stream(ctx, magic, stm);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stm);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, data);
		wasm_rethrow(ctx);
	}
	return document;
}

EMSCRIPTEN_KEEPALIVE
fz_document *openDocumentFromStream(fz_stream *stm, char *magic)
{
	fz_document *document = NULL;
	fz_try(ctx)
		document = fz_open_document_with_stream(ctx, magic, stm);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return document;
}

EMSCRIPTEN_KEEPALIVE
void freeDocument(fz_document *doc)
{
	fz_drop_document(ctx, doc);
}

EMSCRIPTEN_KEEPALIVE
int countPages(fz_document *doc)
{
	int n = 1;
	fz_try(ctx)
		n = fz_count_pages(ctx, doc);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return n;
}

static fz_page *lastPage = NULL;

static void loadPage(fz_document *doc, int number)
{
	static fz_document *lastPageDoc = NULL;
	static int lastPageNumber = -1;
	if (lastPageNumber != number || lastPageDoc != doc)
	{
		if (lastPage)
		{
			fz_drop_page(ctx, lastPage);
			lastPage = NULL;
			lastPageDoc = NULL;
			lastPageNumber = -1;
		}
		lastPage = fz_load_page(ctx, doc, number-1);
		lastPageDoc = doc;
		lastPageNumber = number;
	}
}

EMSCRIPTEN_KEEPALIVE
char *pageText(fz_document *doc, int number, float dpi)
{
	static unsigned char *data = NULL;
	fz_stext_page *text = NULL;
	fz_buffer *buf = NULL;
	fz_output *out = NULL;

	fz_var(buf);
	fz_var(out);
	fz_var(text);

	fz_stext_options opts = { FZ_STEXT_PRESERVE_SPANS };

	fz_free(ctx, data);
	data = NULL;

	fz_try(ctx)
	{
		loadPage(doc, number);

		buf = fz_new_buffer(ctx, 0);
		out = fz_new_output_with_buffer(ctx, buf);
		text = fz_new_stext_page_from_page(ctx, lastPage, &opts);

		fz_print_stext_page_as_json(ctx, out, text, dpi / 72);
		fz_close_output(ctx, out);
		fz_terminate_buffer(ctx, buf);

		fz_buffer_extract(ctx, buf, &data);
	}
	fz_always(ctx)
	{
		fz_drop_stext_page(ctx, text);
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		wasm_rethrow(ctx);
	}

	return (char*)data;
}

static fz_buffer *lastDrawBuffer = NULL;

EMSCRIPTEN_KEEPALIVE
void doDrawPageAsPNG(fz_document *doc, int number, float dpi)
{
	float zoom = dpi / 72;
	fz_pixmap *pix = NULL;

	fz_var(pix);

	if (lastDrawBuffer)
		fz_drop_buffer(ctx, lastDrawBuffer);
	lastDrawBuffer = NULL;

	fz_try(ctx)
	{
		loadPage(doc, number);
		pix = fz_new_pixmap_from_page(ctx, lastPage, fz_scale(zoom, zoom), fz_device_rgb(ctx), 0);
		lastDrawBuffer = fz_new_buffer_from_pixmap_as_png(ctx, pix, fz_default_color_params);
	}
	fz_always(ctx)
		fz_drop_pixmap(ctx, pix);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
unsigned char *getLastDrawData(void)
{
	return lastDrawBuffer ? lastDrawBuffer->data : 0;
}

EMSCRIPTEN_KEEPALIVE
int getLastDrawSize(void)
{
	return lastDrawBuffer ? lastDrawBuffer->len : 0;
}

static fz_irect pageBounds(fz_document *doc, int number, float dpi)
{
	fz_irect bbox = fz_empty_irect;
	fz_try(ctx)
	{
		loadPage(doc, number);
		bbox = fz_round_rect(fz_transform_rect(fz_bound_page(ctx, lastPage), fz_scale(dpi/72, dpi/72)));
	}
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return bbox;
}

EMSCRIPTEN_KEEPALIVE
int pageWidth(fz_document *doc, int number, float dpi)
{
	fz_irect bbox = fz_empty_irect;
	fz_try(ctx)
	{
		loadPage(doc, number);
		bbox = pageBounds(doc, number, dpi);
	}
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return bbox.x1 - bbox.x0;
}

EMSCRIPTEN_KEEPALIVE
int pageHeight(fz_document *doc, int number, float dpi)
{
	fz_irect bbox = fz_empty_irect;
	fz_try(ctx)
	{
		loadPage(doc, number);
		bbox = pageBounds(doc, number, dpi);
	}
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return bbox.y1 - bbox.y0;
}

EMSCRIPTEN_KEEPALIVE
char *pageLinks(fz_document *doc, int number, float dpi)
{
	static unsigned char *data = NULL;
	fz_buffer *buf = NULL;
	fz_link *links = NULL;
	fz_link *link;

	fz_var(buf);
	fz_var(links);

	fz_free(ctx, data);
	data = NULL;

	fz_try(ctx)
	{
		loadPage(doc, number);

		links = fz_load_links(ctx, lastPage);

		buf = fz_new_buffer(ctx, 0);

		fz_append_string(ctx, buf, "[");
		for (link = links; link; link = link->next)
		{
			fz_irect bbox = fz_round_rect(fz_transform_rect(link->rect, fz_scale(dpi/72, dpi/72)));
			fz_append_string(ctx, buf, "{");
			fz_append_printf(ctx, buf, "%q:%d,", "x", bbox.x0);
			fz_append_printf(ctx, buf, "%q:%d,", "y", bbox.y0);
			fz_append_printf(ctx, buf, "%q:%d,", "w", bbox.x1 - bbox.x0);
			fz_append_printf(ctx, buf, "%q:%d,", "h", bbox.y1 - bbox.y0);
			if (fz_is_external_link(ctx, link->uri))
			{
				fz_append_printf(ctx, buf, "%q:%q", "href", link->uri);
			}
			else
			{
				fz_location link_loc = fz_resolve_link(ctx, doc, link->uri, NULL, NULL);
				int link_page = fz_page_number_from_location(ctx, doc, link_loc);
				fz_append_printf(ctx, buf, "%q:\"#page%d\"", "href", link_page+1);
			}
			fz_append_string(ctx, buf, "}");
			if (link->next)
				fz_append_string(ctx, buf, ",");
		}
		fz_append_string(ctx, buf, "]");
		fz_terminate_buffer(ctx, buf);

		fz_buffer_extract(ctx, buf, &data);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_drop_link(ctx, links);
	}
	fz_catch(ctx)
	{
		wasm_rethrow(ctx);
	}

	return (char*)data;
}

EMSCRIPTEN_KEEPALIVE
char *search(fz_document *doc, int number, float dpi, const char *needle)
{
	static unsigned char *data = NULL;
	fz_buffer *buf = NULL;
	fz_quad hits[500];
	int i, n;

	fz_var(buf);

	fz_free(ctx, data);
	data = NULL;

	fz_try(ctx)
	{
		loadPage(doc, number);

		n = fz_search_page(ctx, lastPage, needle, NULL, hits, nelem(hits));

		buf = fz_new_buffer(ctx, 0);

		fz_append_string(ctx, buf, "[");
		for (i = 0; i < n; ++i)
		{
			fz_rect rect = fz_rect_from_quad(hits[i]);
			fz_irect bbox = fz_round_rect(fz_transform_rect(rect, fz_scale(dpi/72, dpi/72)));
			if (i > 0) fz_append_string(ctx, buf, ",");
			fz_append_printf(ctx, buf, "{%q:%d,", "x", bbox.x0);
			fz_append_printf(ctx, buf, "%q:%d,", "y", bbox.y0);
			fz_append_printf(ctx, buf, "%q:%d,", "w", bbox.x1 - bbox.x0);
			fz_append_printf(ctx, buf, "%q:%d}", "h", bbox.y1 - bbox.y0);
		}
		fz_append_string(ctx, buf, "]");
		fz_terminate_buffer(ctx, buf);

		fz_buffer_extract(ctx, buf, &data);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
	}
	fz_catch(ctx)
	{
		wasm_rethrow(ctx);
	}

	return (char*)data;
}

EMSCRIPTEN_KEEPALIVE
char *documentTitle(fz_document *doc)
{
	static char buf[100], *result = NULL;
	fz_try(ctx)
	{
		if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_TITLE, buf, sizeof buf) > 0)
			result = buf;
	}
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return result;
}

EMSCRIPTEN_KEEPALIVE
fz_outline *loadOutline(fz_document *doc)
{
	fz_outline *outline = NULL;
	fz_var(outline);
	fz_try(ctx)
	{
		outline = fz_load_outline(ctx, doc);
	}
	fz_catch(ctx)
	{
		fz_drop_outline(ctx, outline);
		wasm_rethrow(ctx);
	}
	return outline;
}

EMSCRIPTEN_KEEPALIVE
void freeOutline(fz_outline *outline)
{
	fz_drop_outline(ctx, outline);
}

EMSCRIPTEN_KEEPALIVE
char *outlineTitle(fz_outline *node)
{
	return node->title;
}

EMSCRIPTEN_KEEPALIVE
int outlinePage(fz_document *doc, fz_outline *node)
{
	return fz_page_number_from_location(ctx, doc, node->page);
}

EMSCRIPTEN_KEEPALIVE
fz_outline *outlineDown(fz_outline *node)
{
	return node->down;
}

EMSCRIPTEN_KEEPALIVE
fz_outline *outlineNext(fz_outline *node)
{
	return node->next;
}

/* PROGRESSIVE FETCH STREAM */

struct fetch_state
{
	int block_shift;
	int block_size;
	int content_length; // Content-Length in bytes
	int map_length; // Content-Length in blocks
	uint8_t *content; // Array buffer with bytes
	uint8_t *map; // Map of which blocks have been requested and loaded.
};

static void fetch_close(fz_context *ctx, void *state_)
{
	struct fetch_state *state = state_;
	fz_free(ctx, state->content);
	fz_free(ctx, state->map);
	state->content = NULL;
	state->map = NULL;
	// TODO: wait for all outstanding requests to complete, then free state
	// fz_free(ctx, state);
	EM_ASM({
		fetcher.postMessage(['CLOSE', $0]);
	}, state);
}

static void fetch_seek(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
	struct fetch_state *state = stm->state;
	stm->wp = stm->rp = state->content;
	if (whence == SEEK_END)
		stm->pos = state->content_length + offset;
	else if (whence == SEEK_CUR)
		stm->pos += offset;
	else
		stm->pos = offset;
	if (stm->pos < 0)
		stm->pos = 0;
	if (stm->pos > state->content_length)
		stm->pos = state->content_length;
}

static int fetch_next(fz_context *ctx, fz_stream *stm, size_t len)
{
	struct fetch_state *state = stm->state;

	int block = stm->pos >> state->block_shift;
	int start = block << state->block_shift;
	int end = start + state->block_size;
	if (end > state->content_length)
		end = state->content_length;

	if (state->map[block] == 0) {
		state->map[block] = 1;
		EM_ASM({
			fetcher.postMessage(['READ', $0, $1]);
		}, state, block);
		fz_throw(ctx, FZ_ERROR_TRYLATER, "waiting for data");
	}

	if (state->map[block] == 1) {
		fz_throw(ctx, FZ_ERROR_TRYLATER, "waiting for data");
	}

	stm->rp = state->content + stm->pos;
	stm->wp = state->content + end;
	stm->pos = end;

	if (stm->rp < stm->wp)
		return *stm->rp++;
	return -1;
}

EM_JS(void, js_init_fetch, (struct fetch_state *state, char *url, int content_length, int block_shift), {
	fetcher.postMessage(['OPEN', state, [UTF8ToString(url), content_length, block_shift]]);
});

EMSCRIPTEN_KEEPALIVE
void onFetchData(struct fetch_state *state, int block, uint8_t *data, int size)
{
	if (state->content) {
		memcpy(state->content + (block << state->block_shift), data, size);
		state->map[block] = 2;
	}
}

EMSCRIPTEN_KEEPALIVE
fz_stream *openURL(char *url, int content_length, int block_size)
{
	fz_stream *stm = NULL;
	fz_var(stm);
	fz_try (ctx)
	{
		struct fetch_state *state;
		int block_shift = (int)log2(block_size);

		if (block_shift < 10 || block_shift > 24)
			fz_throw(ctx, FZ_ERROR_GENERIC, "invalid block shift: %d", block_shift);

		state = fz_malloc(ctx, sizeof *state);
		state->block_shift = block_shift;
		state->block_size = 1 << block_shift;
		state->content_length = content_length;
		state->content = fz_malloc(ctx, state->content_length);
		state->map_length = content_length / state->block_size + 1;
		state->map = fz_malloc(ctx, state->map_length);
		memset(state->map, 0, state->map_length);

		stm = fz_new_stream(ctx, state, fetch_next, fetch_close);
		// stm->progressive = 1;
		stm->seek = fetch_seek;

		js_init_fetch(state, url, content_length, block_shift);
	}
	fz_catch(ctx)
	{
		if (state)
		{
			fz_free(ctx, state->content);
			fz_free(ctx, state->map);
			fz_free(ctx, state);
		}
		wasm_rethrow(ctx);
	}
	return stm;
}
