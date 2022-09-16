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
#include "mupdf/pdf.h"
#include <string.h>
#include <math.h>

static fz_context *ctx;

static fz_rect out_rect;
static fz_irect out_irect;
static fz_matrix out_matrix;
static fz_point out_points[2];

// TODO - instrument fz_throw to include call stack
void wasm_rethrow(fz_context *ctx)
{
	if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
		EM_ASM({ throw new libmupdf.MupdfTryLaterError("operation in progress"); });
	else
		EM_ASM({ throw new libmupdf.MupdfError(UTF8ToString($0)); }, fz_caught_message(ctx));
}

EMSCRIPTEN_KEEPALIVE
void wasm_init_context(void)
{
	ctx = fz_new_context(NULL, NULL, 100<<20);
	if (!ctx)
		EM_ASM({ throw new Error("Cannot create MuPDF context!"); });
	fz_register_document_handlers(ctx);
}

EMSCRIPTEN_KEEPALIVE
void *wasm_malloc(size_t size)
{
	void *pointer;
	fz_try(ctx)
		pointer = fz_malloc(ctx, size);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return pointer;
}

EMSCRIPTEN_KEEPALIVE
void *wasm_malloc_no_throw(size_t size)
{
	return fz_malloc_no_throw(ctx, size);
}

EMSCRIPTEN_KEEPALIVE
void *wasm_calloc(size_t count, size_t size)
{
	void *pointer;
	fz_try(ctx)
		pointer = fz_calloc(ctx, count, size);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return pointer;
}

EMSCRIPTEN_KEEPALIVE
void *wasm_calloc_no_throw(size_t count, size_t size)
{
	return fz_calloc_no_throw(ctx, count, size);
}

EMSCRIPTEN_KEEPALIVE
void *wasm_realloc(void *p, size_t size)
{
	void *pointer;
	fz_try(ctx)
		pointer = fz_realloc(ctx, p, size);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return pointer;
}

EMSCRIPTEN_KEEPALIVE
void *wasm_realloc_no_throw(void *p, size_t size)
{
	return fz_realloc_no_throw(ctx, p, size);
}

EMSCRIPTEN_KEEPALIVE
void wasm_free(void *p)
{
	fz_free(ctx, p);
}

EMSCRIPTEN_KEEPALIVE
fz_matrix *wasm_scale(float scale_x, float scale_y) {
	out_matrix = fz_scale(scale_x, scale_y);
	return &out_matrix;
}

EMSCRIPTEN_KEEPALIVE
fz_rect *wasm_transform_rect(
	float r_0, float r_1, float r_2, float r_3,
	float tr_0, float tr_1, float tr_2, float tr_3, float tr_4, float tr_5
) {
	fz_rect rect = fz_make_rect(r_0, r_1, r_2, r_3);
	fz_matrix transform = fz_make_matrix(tr_0, tr_1, tr_2, tr_3, tr_4, tr_5);
	out_rect = fz_transform_rect(rect, transform);
	return &out_rect;
}

EMSCRIPTEN_KEEPALIVE
fz_document *wasm_open_document_with_buffer(fz_buffer *buffer, char *magic)
{
	fz_stream *stream = NULL;
	fz_document *document;

	fz_var(buffer);
	fz_var(stream);

	fz_try(ctx)
	{
		stream = fz_open_buffer(ctx, buffer);
		document = fz_open_document_with_stream(ctx, magic, stream);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, stream);
	}
	fz_catch(ctx)
	{
		wasm_rethrow(ctx);
	}
	return document;
}

EMSCRIPTEN_KEEPALIVE
fz_document *wasm_open_document_with_stream(fz_stream* stream, char *magic)
{
	fz_document *document;

	fz_try(ctx)
		document = fz_open_document_with_stream(ctx, magic, stream);
	fz_catch(ctx)
		wasm_rethrow(ctx);

	return document;
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_document(fz_document *doc)
{
	fz_drop_document(ctx, doc);
}

EMSCRIPTEN_KEEPALIVE
char *wasm_document_title(fz_document *doc)
{
	static char buf[100], *result;
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
int wasm_count_pages(fz_document *doc)
{
	int n = 1;
	fz_try(ctx)
		n = fz_count_pages(ctx, doc);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return n;
}

EMSCRIPTEN_KEEPALIVE
fz_page *wasm_load_page(fz_document *doc, int number)
{
	fz_page *page;
	fz_try(ctx)
		page = fz_load_page(ctx, doc, number);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return page;
}

EMSCRIPTEN_KEEPALIVE
fz_outline *wasm_load_outline(fz_document *doc)
{
	fz_outline *outline;
	fz_try(ctx)
		outline = fz_load_outline(ctx, doc);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return outline;
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_page(fz_page *page)
{
	fz_drop_page(ctx, page);
}

EMSCRIPTEN_KEEPALIVE
fz_rect *wasm_bound_page(fz_page *page)
{
	fz_try(ctx)
		out_rect = fz_bound_page(ctx, page);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return &out_rect;
}

EMSCRIPTEN_KEEPALIVE
void wasm_run_page(
	fz_page *page,
	fz_device *dev,
	float ctm_0, float ctm_1, float ctm_2, float ctm_3, float ctm_4, float ctm_5,
	fz_cookie *cookie
) {
	fz_matrix ctm = fz_make_matrix(ctm_0, ctm_1, ctm_2, ctm_3, ctm_4, ctm_5);
	fz_try(ctx)
		fz_run_page(ctx, page, dev, ctm, cookie);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_run_page_contents(
	fz_page *page,
	fz_device *dev,
	float ctm_0, float ctm_1, float ctm_2, float ctm_3, float ctm_4, float ctm_5,
	fz_cookie *cookie
) {
	fz_matrix ctm = fz_make_matrix(ctm_0, ctm_1, ctm_2, ctm_3, ctm_4, ctm_5);
	fz_try(ctx)
		fz_run_page_contents(ctx, page, dev, ctm, cookie);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_run_page_annots(
	fz_page *page,
	fz_device *dev,
	float ctm_0, float ctm_1, float ctm_2, float ctm_3, float ctm_4, float ctm_5,
	fz_cookie *cookie
) {
	fz_matrix ctm = fz_make_matrix(ctm_0, ctm_1, ctm_2, ctm_3, ctm_4, ctm_5);
	fz_try(ctx)
		fz_run_page_annots(ctx, page, dev, ctm, cookie);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_run_page_widgets(
	fz_page *page,
	fz_device *dev,
	float ctm_0, float ctm_1, float ctm_2, float ctm_3, float ctm_4, float ctm_5,
	fz_cookie *cookie
) {
	fz_matrix ctm = fz_make_matrix(ctm_0, ctm_1, ctm_2, ctm_3, ctm_4, ctm_5);
	fz_try(ctx)
		fz_run_page_widgets(ctx, page, dev, ctm, cookie);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_device *wasm_new_draw_device(
	float ctm_0, float ctm_1, float ctm_2, float ctm_3, float ctm_4, float ctm_5,
	fz_pixmap *dest
) {
	fz_matrix ctm = fz_make_matrix(ctm_0, ctm_1, ctm_2, ctm_3, ctm_4, ctm_5);
	fz_device *device;
	fz_try(ctx)
		device = fz_new_draw_device(ctx, ctm, dest);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return device;
}

EMSCRIPTEN_KEEPALIVE
void wasm_close_device(fz_device *dev)
{
	fz_try(ctx)
		fz_close_device(ctx, dev);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_device(fz_device *dev)
{
	fz_try(ctx)
		fz_drop_device(ctx, dev);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_cookie *wasm_new_cookie() {
	fz_cookie *cookie;
	fz_try(ctx)
		cookie = fz_malloc_struct(ctx, fz_cookie);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return cookie;
}

EMSCRIPTEN_KEEPALIVE
void wasm_free_cookie(fz_cookie *cookie) {
	fz_free(ctx, cookie);
}

EMSCRIPTEN_KEEPALIVE
int wasm_cookie_aborted(fz_cookie *cookie) {
	return cookie->abort;
}

EMSCRIPTEN_KEEPALIVE
fz_stext_page *wasm_new_stext_page_from_page(fz_page *page) {
	fz_stext_page *stext_page;
	// FIXME
	const fz_stext_options options = { FZ_STEXT_PRESERVE_SPANS };

	fz_try(ctx)
		stext_page = fz_new_stext_page_from_page(ctx, page, &options);
	fz_catch(ctx)
		wasm_rethrow(ctx);

	return stext_page;
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_stext_page(fz_stext_page *page) {
	fz_try(ctx)
		fz_drop_stext_page(ctx, page);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE void wasm_print_stext_page_as_json(fz_output *out, fz_stext_page *page, float scale) {
	fz_try(ctx)
		fz_print_stext_page_as_json(ctx, out, page, scale);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_link *wasm_load_links(fz_page *page)
{
	fz_link *links;
	fz_try(ctx)
		links = fz_load_links(ctx, page);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return links;
}

EMSCRIPTEN_KEEPALIVE
pdf_page *wasm_pdf_page_from_fz_page(fz_page *page) {
	return pdf_page_from_fz_page(ctx, page);
}

EMSCRIPTEN_KEEPALIVE
fz_link* wasm_next_link(fz_link *link) {
	return link->next;
}

EMSCRIPTEN_KEEPALIVE
fz_rect *wasm_link_rect(fz_link *link) {
	return &link->rect;
}

EMSCRIPTEN_KEEPALIVE
int wasm_is_external_link(fz_link *link) {
	return fz_is_external_link(ctx, link->uri);
}

EMSCRIPTEN_KEEPALIVE
char* wasm_link_uri(fz_link *link) {
	return link->uri;
}

EMSCRIPTEN_KEEPALIVE
int wasm_resolve_link_chapter(fz_document *doc, const char *uri) {
	int chapter;
	fz_try(ctx)
		chapter = fz_resolve_link(ctx, doc, uri, NULL, NULL).chapter;
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return chapter;
}

EMSCRIPTEN_KEEPALIVE
int wasm_resolve_link_page(fz_document *doc, const char *uri) {
	int page;
	fz_try(ctx)
		page = fz_resolve_link(ctx, doc, uri, NULL, NULL).page;
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return page;
}

EMSCRIPTEN_KEEPALIVE
int wasm_page_number_from_location(fz_document *doc, int chapter, int page) {
	fz_location link_loc = { chapter, page };
	int page_number;
	fz_try(ctx)
		page_number = fz_page_number_from_location(ctx, doc, link_loc);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return page_number;
}

EMSCRIPTEN_KEEPALIVE
char *wasm_outline_title(fz_outline *node)
{
	return node->title;
}

EMSCRIPTEN_KEEPALIVE
int wasm_outline_page(fz_document *doc, fz_outline *node)
{
	int pageNumber;
	fz_try(ctx)
		pageNumber = fz_page_number_from_location(ctx, doc, node->page);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return pageNumber;
}

EMSCRIPTEN_KEEPALIVE
fz_outline *wasm_outline_down(fz_outline *node)
{
	return node->down;
}

EMSCRIPTEN_KEEPALIVE
fz_outline *wasm_outline_next(fz_outline *node)
{
	return node->next;
}

EMSCRIPTEN_KEEPALIVE
int wasm_search_page(fz_page *page, const char *needle, fz_quad *hit_bbox, int hit_max)
{
	int hitCount;
	fz_try(ctx)
		hitCount = fz_search_page(ctx, page, needle, NULL, hit_bbox, hit_max);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return hitCount;
}

EMSCRIPTEN_KEEPALIVE
size_t wasm_size_of_quad() {
	return sizeof(fz_quad);
}

EMSCRIPTEN_KEEPALIVE
fz_rect *wasm_rect_from_quad(fz_quad *quad)
{
	out_rect = fz_rect_from_quad(*quad);
	return &out_rect;
}

EMSCRIPTEN_KEEPALIVE
fz_colorspace *wasm_device_gray(void)
{
	return fz_device_rgb(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_colorspace *wasm_device_rgb(void)
{
	return fz_device_rgb(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_colorspace *wasm_device_bgr(void)
{
	return fz_device_bgr(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_colorspace *wasm_device_cmyk(void)
{
	return fz_device_cmyk(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_colorspace(fz_colorspace *cs)
{
	fz_drop_colorspace(ctx, cs);
}

EMSCRIPTEN_KEEPALIVE
fz_pixmap *wasm_new_pixmap_from_page(fz_page *page,
	float ctm_0, float ctm_1, float ctm_2, float ctm_3, float ctm_4, float ctm_5,
	fz_colorspace *colorspace,
	int alpha)
{
	fz_matrix ctm = fz_make_matrix(ctm_0, ctm_1, ctm_2, ctm_3, ctm_4, ctm_5);
	fz_pixmap *pix;
	fz_try(ctx)
		pix = fz_new_pixmap_from_page(ctx, page, ctm, colorspace, alpha);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return pix;
}

EMSCRIPTEN_KEEPALIVE
fz_pixmap *wasm_new_pixmap_with_bbox(
	fz_colorspace *colorspace,
	int bbox_x0, int bbox_y0, int bbox_x1, int bbox_y1,
	fz_separations *seps,
	int alpha
)
{
	fz_irect bbox = fz_make_irect(bbox_x0, bbox_y0, bbox_x1, bbox_y1);
	fz_pixmap *pix;
	fz_try(ctx)
		pix = fz_new_pixmap_with_bbox(ctx, colorspace, bbox, seps, alpha);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return pix;
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_pixmap(fz_pixmap *pix)
{
	fz_drop_pixmap(ctx, pix);
}

EMSCRIPTEN_KEEPALIVE
fz_irect *wasm_pixmap_bbox(fz_pixmap *pix)
{
	fz_try(ctx)
		out_irect = fz_pixmap_bbox(ctx, pix);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return &out_irect;
}

EMSCRIPTEN_KEEPALIVE
void wasm_clear_pixmap_rect_with_value(fz_pixmap *pix, int value, int rect_x0, int rect_y0, int rect_x1, int rect_y1)
{
	fz_irect rect = fz_make_irect(rect_x0, rect_y0, rect_x1, rect_y1);
	// never throws
	fz_clear_pixmap_rect_with_value(ctx, pix, value, rect);
}

EMSCRIPTEN_KEEPALIVE
void wasm_clear_pixmap(fz_pixmap *pix)
{
	// never throws
	fz_clear_pixmap(ctx, pix);
}

EMSCRIPTEN_KEEPALIVE
void wasm_clear_pixmap_with_value(fz_pixmap *pix, int value)
{
	// never throws
	fz_clear_pixmap_with_value(ctx, pix, value);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pixmap_stride(fz_pixmap *pix)
{
	int stride;
	fz_try(ctx)
		stride = fz_pixmap_stride(ctx, pix);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return stride;
}

EMSCRIPTEN_KEEPALIVE
unsigned char *wasm_pixmap_samples(fz_pixmap *pix)
{
	unsigned char *samples;
	fz_try(ctx)
		samples = fz_pixmap_samples(ctx, pix);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return samples;
}

EMSCRIPTEN_KEEPALIVE
fz_buffer *wasm_new_buffer(size_t capacity)
{
	fz_buffer *buf;
	fz_try(ctx)
		buf = fz_new_buffer(ctx, capacity);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return buf;
}

EMSCRIPTEN_KEEPALIVE
fz_buffer *wasm_new_buffer_from_data(unsigned char *data, size_t size)
{
	fz_buffer *buf;
	fz_try(ctx)
		buf = fz_new_buffer_from_data(ctx, data, size);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return buf;
}

EMSCRIPTEN_KEEPALIVE
fz_buffer *wasm_new_buffer_from_pixmap_as_png(fz_pixmap *pix)
{
	fz_buffer *buf;
	fz_try(ctx)
		buf = fz_new_buffer_from_pixmap_as_png(ctx, pix, fz_default_color_params);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return buf;
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_buffer(fz_buffer *buf)
{
	fz_drop_buffer(ctx, buf);
}

EMSCRIPTEN_KEEPALIVE
unsigned char *wasm_buffer_data(fz_buffer *buf)
{
	return buf->data;
}

EMSCRIPTEN_KEEPALIVE
size_t wasm_buffer_size(fz_buffer *buf)
{
	return buf->len;
}

EMSCRIPTEN_KEEPALIVE
size_t wasm_buffer_capacity(fz_buffer *buf)
{
	return buf->cap;
}

EMSCRIPTEN_KEEPALIVE
void wasm_resize_buffer(fz_buffer *buf, size_t capacity) {
	fz_try(ctx)
		fz_resize_buffer(ctx, buf, capacity);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_grow_buffer(fz_buffer *buf) {
	fz_try(ctx)
		fz_grow_buffer(ctx, buf);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_trim_buffer(fz_buffer *buf) {
	fz_try(ctx)
		fz_trim_buffer(ctx, buf);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_clear_buffer(fz_buffer *buf) {
	fz_try(ctx)
		fz_clear_buffer(ctx, buf);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
int wasm_buffers_eq(fz_buffer *buf1, fz_buffer *buf2) {
	if (buf1->len != buf2->len)
		return 0;
	else
		return memcmp(buf1->data, buf2->data, buf1->len) == 0;
}

EMSCRIPTEN_KEEPALIVE
fz_output *wasm_new_output_with_buffer(fz_buffer *buf) {
	fz_output *output;
	fz_try(ctx)
		output = fz_new_output_with_buffer(ctx, buf);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return output;
}

EMSCRIPTEN_KEEPALIVE
void wasm_close_output(fz_output *output) {
	fz_try(ctx)
		fz_close_output(ctx, output);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_output(fz_output *output)
{
	fz_drop_output(ctx, output);
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

EM_JS(void, js_open_fetch, (struct fetch_state *state, char *url, int content_length, int block_shift, int prefetch), {
	libmupdf.fetchOpen(state, UTF8ToString(url), content_length, block_shift, prefetch);
});

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
		libmupdf.fetchClose($0);
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
			libmupdf.fetchRead($0, $1);
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

EMSCRIPTEN_KEEPALIVE
fz_stream *wasm_open_stream_from_url(char *url, int content_length, int block_size, int prefetch)
{
	fz_stream *stream = NULL;
	struct fetch_state *state = NULL;

	fz_var(stream);
	fz_var(state);

	fz_try (ctx)
	{
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

		stream = fz_new_stream(ctx, state, fetch_next, fetch_close);
		// stream->progressive = 1;
		stream->seek = fetch_seek;

		js_open_fetch(state, url, content_length, block_shift, prefetch);
	}
	fz_catch(ctx)
	{
		if (state)
		{
			fz_free(ctx, state->content);
			fz_free(ctx, state->map);
			fz_free(ctx, state);
		}
		fz_drop_stream(ctx, stream);
		wasm_rethrow(ctx);
	}
	return stream;
}

EMSCRIPTEN_KEEPALIVE
void wasm_on_data_fetched(struct fetch_state *state, int block, uint8_t *data, int size)
{
	if (state->content) {
		memcpy(state->content + (block << state->block_shift), data, size);
		state->map[block] = 2;
	}
}

EMSCRIPTEN_KEEPALIVE
fz_stream *wasm_new_stream_from_buffer(fz_buffer *buf)
{
	fz_stream *stream;
	fz_try(ctx)
		stream = fz_open_buffer(ctx, buf);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return stream;
}

EMSCRIPTEN_KEEPALIVE
fz_stream *wasm_new_stream_from_data(unsigned char *data, size_t size)
{
	fz_stream *stream;
	fz_try(ctx)
		stream = fz_open_memory(ctx, data, size);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return stream;
}

EMSCRIPTEN_KEEPALIVE
void wasm_drop_stream(fz_stream *stream)
{
	fz_drop_stream(ctx, stream);
}

EMSCRIPTEN_KEEPALIVE
fz_buffer *wasm_read_all(fz_stream *stream, size_t initial) {
	fz_buffer *buffer;
	fz_try(ctx)
		buffer = fz_read_all(ctx, stream, initial);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return buffer;
}

// ANNOTATION HANDLING

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_update_page(pdf_page *page)
{
	pdf_update_page(ctx, page);
}

EMSCRIPTEN_KEEPALIVE
pdf_annot *wasm_pdf_keep_annot(pdf_annot *annot)
{
	// never throws
	return pdf_keep_annot(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_drop_annot(pdf_annot *annot)
{
	// never throws
	pdf_drop_annot(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_active(pdf_annot *annot)
{
	// never throws
	return pdf_annot_active(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_active(pdf_annot *annot, int active)
{
	// never throws
	pdf_set_annot_active(ctx, annot, active);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_hot(pdf_annot *annot)
{
	// never throws
	return pdf_annot_hot(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_hot(pdf_annot *annot, int hot)
{
	// never throws
	pdf_set_annot_hot(ctx, annot, hot);
}

EMSCRIPTEN_KEEPALIVE
fz_matrix* wasm_pdf_annot_transform(pdf_annot *annot)
{
	// never throws
	out_matrix = pdf_annot_transform(ctx, annot);
	return &out_matrix;
}

EMSCRIPTEN_KEEPALIVE
pdf_annot *wasm_pdf_first_annot(pdf_page *page)
{
	// never throws
	return pdf_first_annot(ctx, page);
}

EMSCRIPTEN_KEEPALIVE
pdf_annot *wasm_pdf_next_annot(pdf_annot *annot)
{
	// never throws
	return pdf_next_annot(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
pdf_obj *wasm_pdf_annot_obj(pdf_annot *annot)
{
	// never throws
	return pdf_annot_obj(ctx, annot);
}

// Unused
// TODO - remove?
EMSCRIPTEN_KEEPALIVE
pdf_page *wasm_pdf_annot_page(pdf_annot *annot)
{
	// never throws
	return pdf_annot_page(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
fz_rect *wasm_pdf_bound_annot(pdf_annot *annot)
{
	fz_try(ctx)
		out_rect = pdf_bound_annot(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return &out_rect;
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_needs_resynthesis(pdf_annot* annot)
{
	// never throws
	return pdf_annot_needs_resynthesis(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_resynthesised(pdf_annot* annot)
{
	// never throws
	pdf_set_annot_resynthesised(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_dirty_annot(pdf_annot* annot)
{
	// never throws
	pdf_dirty_annot(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
pdf_annot *wasm_pdf_create_annot_raw(pdf_page *page, enum pdf_annot_type type)
{
	pdf_annot *annot;
	fz_try(ctx)
		annot = pdf_create_annot_raw(ctx, page, type);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return annot;
}

EMSCRIPTEN_KEEPALIVE
fz_link *wasm_pdf_create_link(pdf_page *page, float bbox_x0, float bbox_y0, float bbox_x1, float bbox_y1, const char *uri)
{
	fz_rect bbox = fz_make_rect(bbox_x0, bbox_y0, bbox_x1, bbox_y1);
	fz_link *link;
	fz_try(ctx)
		link = pdf_create_link(ctx, page, bbox, uri);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return link;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_delete_link(pdf_page *page, fz_link *link)
{
	fz_try(ctx)
		pdf_delete_link(ctx, page, link);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_popup(pdf_annot *annot, float rect_x0, float rect_y0, float rect_x1, float rect_y1)
{
	fz_rect rect = fz_make_rect(rect_x0, rect_y0, rect_x1, rect_y1);
	fz_try(ctx)
		pdf_set_annot_popup(ctx, annot, rect);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_rect* wasm_pdf_annot_popup(pdf_annot *annot)
{
	fz_try(ctx)
		out_rect = pdf_annot_popup(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return &out_rect;
}

// TODO - Expand
EMSCRIPTEN_KEEPALIVE
pdf_annot *wasm_pdf_create_annot(pdf_page *page, enum pdf_annot_type type)
{
	pdf_annot *annot;
	fz_try(ctx)
		annot = pdf_create_annot(ctx, page, type);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return annot;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_delete_annot(pdf_page *page, pdf_annot *annot)
{
	fz_try(ctx)
		pdf_delete_annot(ctx, page, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
const char *wasm_pdf_annot_type_string(pdf_annot *annot)
{
	const char *type_string;
	fz_try(ctx)
		type_string = pdf_string_from_annot_type(ctx, pdf_annot_type(ctx, annot));
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return type_string;
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_flags(pdf_annot *annot)
{
	int flags;
	fz_try(ctx)
		flags = pdf_annot_flags(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return flags;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_flags(pdf_annot *annot, int flags)
{
	fz_try(ctx)
		pdf_set_annot_flags(ctx, annot, flags);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
fz_rect* wasm_pdf_annot_rect(pdf_annot *annot)
{
	// never throws
	out_rect = pdf_annot_rect(ctx, annot);
	return &out_rect;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_rect(pdf_annot *annot, float rect_x0, float rect_y0, float rect_x1, float rect_y1)
{
	fz_rect rect = fz_make_rect(rect_x0, rect_y0, rect_x1, rect_y1);
	fz_try(ctx)
		pdf_set_annot_rect(ctx, annot, rect);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

// Returned string must be freed
EMSCRIPTEN_KEEPALIVE
const char *wasm_pdf_annot_contents(pdf_annot *annot)
{
	const char* contents_str;
	fz_try(ctx)
		contents_str = pdf_annot_contents(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return contents_str;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_contents(pdf_annot *annot, const char *text)
{
	fz_try(ctx)
		pdf_set_annot_contents(ctx, annot, text);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_has_open(pdf_annot *annot)
{
	int res;
	fz_try(ctx)
		res = pdf_annot_has_open(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return res;
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_is_open(pdf_annot *annot)
{
	int res;
	fz_try(ctx)
		res = pdf_annot_is_open(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return res;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_is_open(pdf_annot *annot, int is_open)
{
	fz_try(ctx)
		pdf_set_annot_is_open(ctx, annot, is_open);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_has_icon_name(pdf_annot *annot)
{
	int res;
	fz_try(ctx)
		res = pdf_annot_has_icon_name(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return res;
}

EMSCRIPTEN_KEEPALIVE
const char *wasm_pdf_annot_icon_name(pdf_annot *annot)
{
	const char* name;
	fz_try(ctx)
		name = pdf_annot_icon_name(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	// Returns a static string, doesn't need to be freed.
	return name;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_icon_name(pdf_annot *annot, const char *name)
{
	fz_try(ctx)
		pdf_set_annot_icon_name(ctx, annot, name);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

// TODO - line endings styles
// !!!
// !!!

EMSCRIPTEN_KEEPALIVE
float wasm_pdf_annot_border(pdf_annot *annot)
{
	float border_width;
	fz_try(ctx)
		border_width = pdf_annot_border(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return border_width;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_border(pdf_annot *annot, float w)
{
	fz_try(ctx)
		pdf_set_annot_border(ctx, annot, w);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

// TODO - fz_document_language

EMSCRIPTEN_KEEPALIVE
const char* wasm_pdf_annot_language(pdf_annot *annot)
{
	static char returned_string[8];

	fz_try(ctx)
	{
		fz_text_language language = pdf_annot_language(ctx, annot);
		fz_string_from_text_language(returned_string, language);
	}
	fz_catch(ctx)
		wasm_rethrow(ctx);

	return returned_string;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_language(pdf_annot *annot, const char* lang)
{
	fz_try(ctx)
	{
		fz_text_language lang_code = fz_text_language_from_string(lang);

		if (lang_code == FZ_LANG_UNSET)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot set language: language %s not recognized", lang);

		pdf_set_annot_language(ctx, annot, lang_code);
	}
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_quadding(pdf_annot *annot)
{
	// never throws
	return pdf_annot_quadding(ctx, annot);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_quadding(pdf_annot *annot, int q)
{
	fz_try(ctx)
		pdf_set_annot_quadding(ctx, annot, q);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
float wasm_pdf_annot_opacity(pdf_annot *annot)
{
	float opacity;
	fz_try(ctx)
		opacity = pdf_annot_opacity(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return opacity;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_opacity(pdf_annot *annot, float opacity)
{
	fz_try(ctx)
		pdf_set_annot_opacity(ctx, annot, opacity);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

// pdf_annot_MK_BG
// pdf_set_annot_color
// pdf_annot_interior_color
// !!!
// !!!

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_has_line(pdf_annot *annot)
{
	int res;
	fz_try(ctx)
		res = pdf_annot_has_line(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return res;
}

EMSCRIPTEN_KEEPALIVE
fz_point *wasm_pdf_annot_line(pdf_annot *annot)
{
	fz_try(ctx)
		pdf_annot_line(ctx, annot, &out_points[0], &out_points[1]);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return out_points;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_line(pdf_annot *annot, float x0, float y0, float x1, float y1)
{
	fz_point a = fz_make_point(x0, y0);
	fz_point b = fz_make_point(x1, y1);
	fz_try(ctx)
		pdf_set_annot_line(ctx, annot, a, b);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_has_vertices(pdf_annot *annot)
{
	int res;
	fz_try(ctx)
		res = pdf_annot_has_vertices(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return res;
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_vertex_count(pdf_annot *annot)
{
	int count;
	fz_try(ctx)
		count = pdf_annot_vertex_count(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return count;
}

fz_point *wasm_pdf_annot_vertex(pdf_annot *annot, int i)
{
	fz_try(ctx)
		out_points[0] = pdf_annot_vertex(ctx, annot, i);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return &out_points[0];
}

// TODO
// void pdf_set_annot_vertices(fz_context *ctx, pdf_annot *annot, int n, const fz_point *v)

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_clear_annot_vertices(pdf_annot *annot)
{
	fz_try(ctx)
		pdf_clear_annot_vertices(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_add_annot_vertex(pdf_annot *annot, float x, float y)
{
	fz_point p = fz_make_point(x, y);
	fz_try(ctx)
		pdf_add_annot_vertex(ctx, annot, p);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_vertex(pdf_annot *annot, int i, float x, float y)
{
	fz_point p = fz_make_point(x, y);
	fz_try(ctx)
		pdf_set_annot_vertex(ctx, annot, i, p);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_has_quad_points(pdf_annot *annot)
{
	int res;
	fz_try(ctx)
		res = pdf_annot_has_quad_points(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return res;
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_quad_point_count(pdf_annot *annot)
{
	int count;
	fz_try(ctx)
		count = pdf_annot_quad_point_count(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return count;
}

// TODO
/*
fz_quad pdf_annot_quad_point(fz_context *ctx, pdf_annot *annot, int idx)
void pdf_set_annot_quad_points(fz_context *ctx, pdf_annot *annot, int n, const fz_quad *q)
*/

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_clear_annot_quad_points(pdf_annot *annot)
{
	fz_try(ctx)
		pdf_clear_annot_quad_points(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

// TODO
/*
void wasm_pdf_add_annot_quad_point(pdf_annot *annot, fz_quad quad)
*/

// TODO - ink list

EMSCRIPTEN_KEEPALIVE
double wasm_pdf_annot_modification_date(pdf_annot *annot)
{
	double seconds_since_epoch;
	fz_try(ctx)
		seconds_since_epoch = pdf_annot_modification_date(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return seconds_since_epoch;
}

EMSCRIPTEN_KEEPALIVE
double wasm_pdf_annot_creation_date(pdf_annot *annot)
{
	double seconds_since_epoch;
	fz_try(ctx)
		seconds_since_epoch = pdf_annot_creation_date(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return seconds_since_epoch;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_modification_date(pdf_annot *annot, double secs)
{
	fz_try(ctx)
		pdf_set_annot_modification_date(ctx, annot, secs);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_creation_date(pdf_annot *annot, double secs)
{
	fz_try(ctx)
		pdf_set_annot_creation_date(ctx, annot, secs);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_has_author(pdf_annot *annot)
{
	int res;
	fz_try(ctx)
		res = pdf_annot_has_author(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return res;
}

// Returned string must be freed
EMSCRIPTEN_KEEPALIVE
const char *wasm_pdf_annot_author(pdf_annot *annot)
{
	const char* author;
	fz_try(ctx)
		author = pdf_annot_author(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return author;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_author(pdf_annot *annot, const char *author)
{
	fz_try(ctx)
		pdf_set_annot_author(ctx, annot, author);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}

// TODO - default appearance

EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_field_flags(pdf_annot *annot)
{
	int flags;
	fz_try(ctx)
		flags = pdf_annot_field_flags(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return flags;
}

// Returned string must be freed
EMSCRIPTEN_KEEPALIVE
const char *wasm_pdf_annot_field_value(pdf_annot *widget)
{
	const char* value;
	fz_try(ctx)
		value = pdf_annot_field_value(ctx, widget);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return value;
}

// Returned string must be freed
EMSCRIPTEN_KEEPALIVE
const char *wasm_pdf_annot_field_label(pdf_annot *widget)
{
	const char* label;
	fz_try(ctx)
		label = pdf_annot_field_label(ctx, widget);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return label;
}

// TODO
//int pdf_set_annot_field_value(fz_context *ctx, pdf_document *doc, pdf_annot *annot, const char *text, int ignore_trigger_events)
// void pdf_set_annot_appearance(fz_context *ctx, pdf_annot *annot, const char *appearance, const char *state, fz_matrix ctm, fz_rect bbox, pdf_obj *res, fz_buffer *contents)
// void pdf_set_annot_appearance_from_display_list(fz_context *ctx, pdf_annot *annot, const char *appearance, const char *state, fz_matrix ctm, fz_display_list *list)


EMSCRIPTEN_KEEPALIVE
int wasm_pdf_annot_has_filespec(pdf_annot *annot)
{
	int res;
	fz_try(ctx)
		res = pdf_annot_has_filespec(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return res;
}

EMSCRIPTEN_KEEPALIVE
pdf_obj* wasm_pdf_annot_filespec(pdf_annot *annot)
{
	pdf_obj* filespec;
	fz_try(ctx)
		filespec = pdf_annot_filespec(ctx, annot);
	fz_catch(ctx)
		wasm_rethrow(ctx);
	return filespec;
}

EMSCRIPTEN_KEEPALIVE
void wasm_pdf_set_annot_filespec(pdf_annot *annot, pdf_obj *fs)
{
	fz_try(ctx)
		pdf_set_annot_filespec(ctx, annot, fs);
	fz_catch(ctx)
		wasm_rethrow(ctx);
}
