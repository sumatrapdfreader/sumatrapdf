// Copyright (C) 2004-2023 Artifex Software, Inc.
//
// This file is part of MuPDF WASM Library.
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

// TODO: Story
// TODO: DOM

// TODO: PDFWidget
// TODO: PDFGraftMap

// TODO: WASMDevice with callbacks
// TODO: PDFPage.process with callbacks

#include "emscripten.h"
#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include <string.h>
#include <math.h>

static fz_context *ctx;

static fz_matrix out_matrix;
static fz_point out_point;
static fz_rect out_rect;
static fz_quad out_quad;

#define EXPORT EMSCRIPTEN_KEEPALIVE

#define TRY(CODE) { fz_try(ctx) CODE fz_catch(ctx) wasm_rethrow(ctx); }

// Simple wrappers for one-line functions...
#define POINTER(F, ...) void* p; TRY({ p = (void*)F(ctx, ## __VA_ARGS__); }) return p;
#define INTEGER(F, ...) int p; TRY({ p = F(ctx, ## __VA_ARGS__); }) return p;
#define NUMBER(F, ...) float p; TRY({ p = F(ctx, ## __VA_ARGS__); }) return p;
#define MATRIX(F, ...) TRY({ out_matrix = F(ctx, ## __VA_ARGS__); }) return &out_matrix;
#define POINT(F, ...) TRY({ out_point = F(ctx, ## __VA_ARGS__); }) return &out_point;
#define RECT(F, ...) TRY({ out_rect = F(ctx, ## __VA_ARGS__); }) return &out_rect;
#define QUAD(F, ...) TRY({ out_quad = F(ctx, ## __VA_ARGS__); }) return &out_quad;
#define VOID(F, ...) TRY({ F(ctx, ## __VA_ARGS__); })

__attribute__((noinline)) void
wasm_rethrow(fz_context *ctx)
{
	int code;
	const char *message = fz_convert_error(ctx, &code);
	if (code == FZ_ERROR_TRYLATER)
		EM_ASM({ throw new libmupdf.TryLaterError(UTF8ToString($0)); }, message);
	else if (code == FZ_ERROR_ABORT)
		EM_ASM({ throw new libmupdf.AbortError(UTF8ToString($0)); }, message);
	else
		EM_ASM({ throw new Error(UTF8ToString($0)); }, message);
}

EXPORT
void wasm_init_context(void)
{
	ctx = fz_new_context(NULL, NULL, 100<<20);
	if (!ctx)
		EM_ASM({ throw new Error("Cannot create MuPDF context!"); });
	fz_register_document_handlers(ctx);
}

EXPORT
void * wasm_malloc(size_t size)
{
	POINTER(fz_malloc, size)
}

EXPORT
void wasm_free(void *p)
{
	fz_free(ctx, p);
}

// --- REFERENCE COUNTING ---

#define KEEP_(WNAME,FNAME) EXPORT void * WNAME(void *p) { return FNAME(ctx, p); }
#define DROP_(WNAME,FNAME) EXPORT void WNAME(void *p) { FNAME(ctx, p); }

#define KEEP(NAME) KEEP_(wasm_keep_ ## NAME, fz_keep_ ## NAME)
#define DROP(NAME) DROP_(wasm_drop_ ## NAME, fz_drop_ ## NAME)

#define REFS(NAME) KEEP(NAME) DROP(NAME)

#define PDF_REFS(NAME) \
        KEEP_(wasm_pdf_keep_ ## NAME, pdf_keep_ ## NAME) \
        DROP_(wasm_pdf_drop_ ## NAME, pdf_drop_ ## NAME)

REFS(buffer)
REFS(stream)

REFS(colorspace)
REFS(pixmap)
REFS(font)
REFS(stroke_state)
REFS(image)
REFS(shade)
REFS(path)
REFS(text)
REFS(device)
REFS(display_list)
DROP(stext_page)
DROP(document_writer)
DROP(outline_iterator)

REFS(document)
REFS(page)
REFS(link)
REFS(outline)

PDF_REFS(annot)
PDF_REFS(obj)

// --- PLAIN STRUCT ACCESSORS ---

#define GETP(S,T,F) EXPORT T* wasm_ ## S ## _get_ ## F (fz_ ## S *p) { return &p->F; }
#define GETU(S,T,F,U) EXPORT T wasm_ ## S ## _get_ ## F (fz_ ## S *p) { return p->U; }
#define GET(S,T,F) EXPORT T wasm_ ## S ## _get_ ## F (fz_ ## S *p) { return p->F; }
#define SET(S,T,F) EXPORT void wasm_ ## S ## _set_ ## F (fz_ ## S *p, T v) { p->F = v; }
#define GETSET(S,T,F) GET(S,T,F) SET(S,T,F)

#define PDF_GET(S,T,F) EXPORT T wasm_pdf_ ## S ## _get_ ## F (pdf_ ## S *p) { return p->F; }
#define PDF_SET(S,T,F) EXPORT void wasm_pdf_ ## S ## _set_ ## F (pdf_ ## S *p, T v) { p->F = v; }

GET(buffer, void*, data)
GET(buffer, int, len)

GET(colorspace, int, type)
GET(colorspace, int, n)
GET(colorspace, char*, name)

GET(pixmap, int, w)
GET(pixmap, int, h)
GET(pixmap, int, x)
GET(pixmap, int, y)
GET(pixmap, int, n)
GET(pixmap, int, stride)
GET(pixmap, int, alpha)
GET(pixmap, int, xres)
GET(pixmap, int, yres)
GET(pixmap, fz_colorspace*, colorspace)
GET(pixmap, unsigned char*, samples)

SET(pixmap, int, xres)
SET(pixmap, int, yres)

GET(font, char*, name)

GETSET(stroke_state, int, start_cap)
GETSET(stroke_state, int, dash_cap)
GETSET(stroke_state, int, end_cap)
GETSET(stroke_state, int, linejoin)
GETSET(stroke_state, float, linewidth)
GETSET(stroke_state, float, miterlimit)
GETSET(stroke_state, float, dash_phase)
// TODO: dash_len, dash_array

GET(image, int, w)
GET(image, int, h)
GET(image, int, n)
GET(image, int, bpc)
GET(image, int, xres)
GET(image, int, yres)
GET(image, int, imagemask)
GET(image, fz_colorspace*, colorspace)
GET(image, fz_image*, mask)

GET(outline, char*, title)
GET(outline, char*, uri)
GET(outline, fz_outline*, next)
GET(outline, fz_outline*, down)
GET(outline, int, is_open)

GET(outline_item, char*, title)
GET(outline_item, char*, uri)
GET(outline_item, int, is_open)

GETP(link, fz_rect, rect)
GET(link, char*, uri)
GET(link, fz_link*, next)

GETP(stext_page, fz_rect, mediabox)
GET(stext_page, fz_stext_block*, first_block)

GET(stext_block, fz_stext_block*, next)
GET(stext_block, int, type)
GETP(stext_block, fz_rect, bbox)
GETU(stext_block, fz_stext_line*, first_line, u.t.first_line)

GET(stext_line, fz_stext_line*, next)
GET(stext_line, int, wmode)
GETP(stext_line, fz_point, dir)
GETP(stext_line, fz_rect, bbox)
GET(stext_line, fz_stext_char*, first_char)

GET(stext_char, fz_stext_char*, next)
GET(stext_char, int, c)
GETP(stext_char, fz_point, origin)
GETP(stext_char, fz_quad, quad)
GET(stext_char, float, size)
GET(stext_char, fz_font*, font)

PDF_GET(embedded_file_params, const char*, filename)
PDF_GET(embedded_file_params, const char*, mimetype)
PDF_GET(embedded_file_params, int, size)
PDF_GET(embedded_file_params, int, created)
PDF_GET(embedded_file_params, int, modified)

// --- Buffer ---

EXPORT
fz_buffer * wasm_new_buffer(size_t capacity)
{
	POINTER(fz_new_buffer, capacity)
}

EXPORT
fz_buffer * wasm_new_buffer_from_data(unsigned char *data, size_t size)
{
	POINTER(fz_new_buffer_from_data, data, size)
}

EXPORT
void wasm_append_string(fz_buffer *buf, char *str)
{
	VOID(fz_append_string, buf, str)
}

EXPORT
void wasm_append_byte(fz_buffer *buf, int c)
{
	VOID(fz_append_byte, buf, c)
}

EXPORT
void wasm_append_buffer(fz_buffer *buf, fz_buffer *src)
{
	VOID(fz_append_buffer, buf, src)
}

EXPORT
char * wasm_string_from_buffer(fz_buffer *buf)
{
	POINTER(fz_string_from_buffer, buf)
}

// --- ColorSpace ---

EXPORT fz_colorspace * wasm_device_gray(void) { return fz_device_gray(ctx); }
EXPORT fz_colorspace * wasm_device_rgb(void) { return fz_device_rgb(ctx); }
EXPORT fz_colorspace * wasm_device_bgr(void) { return fz_device_bgr(ctx); }
EXPORT fz_colorspace * wasm_device_cmyk(void) { return fz_device_cmyk(ctx); }
EXPORT fz_colorspace * wasm_device_lab(void) { return fz_device_lab(ctx); }

EXPORT
fz_colorspace * wasm_new_icc_colorspace(char *name, fz_buffer *buffer)
{
	POINTER(fz_new_icc_colorspace, FZ_COLORSPACE_NONE, 0, name, buffer)
}

// --- StrokeState ---

EXPORT
fz_stroke_state * wasm_new_stroke_state(void)
{
	fz_stroke_state *p = NULL;
	TRY({
		p = fz_new_stroke_state(ctx);
	})
	return p;
}

// --- Font ---

EXPORT
fz_font * wasm_new_base14_font(char *name)
{
	POINTER(fz_new_base14_font, name)
}

EXPORT
fz_font * wasm_new_font_from_buffer(char *name, fz_buffer *buf, int subfont)
{
	POINTER(fz_new_font_from_buffer, name, buf, subfont, 0)
}

// --- Image ---

EXPORT
fz_image * wasm_new_image_from_pixmap(fz_pixmap *pix, fz_image *mask)
{
	POINTER(fz_new_image_from_pixmap, pix, mask)
}

EXPORT
fz_image * wasm_new_image_from_buffer(fz_buffer *buf)
{
	POINTER(fz_new_image_from_buffer, buf)
}

// --- Pixmap ---

EXPORT
fz_pixmap * wasm_get_pixmap_from_image(fz_image *image)
{
	POINTER(fz_get_pixmap_from_image, image, NULL, NULL, NULL, NULL)
}

EXPORT
fz_pixmap * wasm_new_pixmap_from_page(fz_page *page, fz_matrix *ctm, fz_colorspace *colorspace, int alpha)
{
	POINTER(fz_new_pixmap_from_page, page, *ctm, colorspace, alpha)
}

EXPORT
fz_pixmap * wasm_pdf_new_pixmap_from_page_with_usage(pdf_page *page, fz_matrix *ctm, fz_colorspace *colorspace, int alpha, char *usage, fz_box_type box)
{
	POINTER(pdf_new_pixmap_from_page_with_usage, page, *ctm, colorspace, alpha, usage, box)
}

EXPORT
fz_pixmap * wasm_pdf_new_pixmap_from_page_contents_with_usage(pdf_page *page, fz_matrix *ctm, fz_colorspace *colorspace, int alpha, char *usage, fz_box_type box)
{
	POINTER(pdf_new_pixmap_from_page_contents_with_usage, page, *ctm, colorspace, alpha, usage, box)
}

EXPORT
fz_pixmap * wasm_new_pixmap_with_bbox(fz_colorspace *colorspace, fz_rect *bbox, int alpha)
{
	POINTER(fz_new_pixmap_with_bbox, colorspace, fz_irect_from_rect(*bbox), NULL, alpha)
}

EXPORT
void wasm_clear_pixmap(fz_pixmap *pix)
{
	VOID(fz_clear_pixmap, pix)
}

EXPORT
void wasm_clear_pixmap_with_value(fz_pixmap *pix, int value)
{
	VOID(fz_clear_pixmap_with_value, pix, value)
}

EXPORT
void wasm_invert_pixmap(fz_pixmap *pix)
{
	VOID(fz_invert_pixmap, pix)
}

EXPORT
void wasm_invert_pixmap_luminance(fz_pixmap *pix)
{
	VOID(fz_invert_pixmap_luminance, pix)
}

EXPORT
void wasm_gamma_pixmap(fz_pixmap *pix, float gamma)
{
	VOID(fz_gamma_pixmap, pix, gamma)
}

EXPORT
void wasm_tint_pixmap(fz_pixmap *pix, int black_hex_color, int white_hex_color)
{
	VOID(fz_tint_pixmap, pix, black_hex_color, white_hex_color)
}

EXPORT
fz_buffer * wasm_new_buffer_from_pixmap_as_png(fz_pixmap *pix)
{
	POINTER(fz_new_buffer_from_pixmap_as_png, pix, fz_default_color_params)
}

EXPORT
fz_buffer * wasm_new_buffer_from_pixmap_as_pam(fz_pixmap *pix)
{
	POINTER(fz_new_buffer_from_pixmap_as_pam, pix, fz_default_color_params)
}

EXPORT
fz_buffer * wasm_new_buffer_from_pixmap_as_psd(fz_pixmap *pix)
{
	POINTER(fz_new_buffer_from_pixmap_as_psd, pix, fz_default_color_params)
}

EXPORT
fz_buffer * wasm_new_buffer_from_pixmap_as_jpeg(fz_pixmap *pix, int quality, int invert_cmyk)
{
	POINTER(fz_new_buffer_from_pixmap_as_jpeg, pix, fz_default_color_params, quality, invert_cmyk)
}

EXPORT
fz_pixmap * wasm_convert_pixmap(fz_pixmap *pixmap, fz_colorspace *colorspace, int keep_alpha)
{
	POINTER(fz_convert_pixmap, pixmap, colorspace, NULL, NULL, fz_default_color_params, keep_alpha)
}

// --- Shade ---

EXPORT
fz_rect * wasm_bound_shade(fz_shade *shade)
{
	RECT(fz_bound_shade, shade, fz_identity)
}

// --- DisplayList ---

EXPORT
fz_display_list * wasm_new_display_list(fz_rect *mediabox)
{
	POINTER(fz_new_display_list, *mediabox)
}

EXPORT
fz_rect * wasm_bound_display_list(fz_display_list *list)
{
	RECT(fz_bound_display_list, list)
}

EXPORT
void wasm_run_display_list(fz_display_list *display_list, fz_device *dev, fz_matrix *ctm)
{
	VOID(fz_run_display_list, display_list, dev, *ctm, fz_infinite_rect, NULL)
}

EXPORT
fz_pixmap * wasm_new_pixmap_from_display_list(fz_display_list *display_list, fz_matrix *ctm, fz_colorspace *colorspace, int alpha)
{
	POINTER(fz_new_pixmap_from_display_list, display_list, *ctm, colorspace, alpha)
}

EXPORT
fz_stext_page * wasm_new_stext_page_from_display_list(fz_display_list *display_list)
{
	// TODO: parse options
	fz_stext_options options = { FZ_STEXT_PRESERVE_SPANS };
	POINTER(fz_new_stext_page_from_display_list, display_list, &options)
}

EXPORT
int wasm_search_display_list(fz_display_list *display_list, char *needle, int *marks, fz_quad *hits, int hit_max)
{
	INTEGER(fz_search_display_list, display_list, needle, marks, hits, hit_max)
}

// --- Path ---

EXPORT
fz_path * wasm_new_path(void)
{
	POINTER(fz_new_path)
}

EXPORT
void wasm_moveto(fz_path *path, float x, float y)
{
	VOID(fz_moveto, path, x, y)
}

EXPORT
void wasm_lineto(fz_path *path, float x, float y)
{
	VOID(fz_lineto, path, x, y)
}

EXPORT
void wasm_curveto(fz_path *path, float x1, float y1, float x2, float y2, float x3, float y3)
{
	VOID(fz_curveto, path, x1, y1, x2, y2, x3, y3)
}

EXPORT
void wasm_curvetov(fz_path *path, float x1, float y1, float x2, float y2)
{
	VOID(fz_curvetov, path, x1, y1, x2, y2)
}

EXPORT
void wasm_curvetoy(fz_path *path, float x1, float y1, float x2, float y2)
{
	VOID(fz_curvetoy, path, x1, y1, x2, y2)
}

EXPORT
void wasm_closepath(fz_path *path)
{
	VOID(fz_closepath, path)
}

EXPORT
void wasm_rectto(fz_path *path, float x1, float y1, float x2, float y2)
{
	VOID(fz_rectto, path, x1, y1, x2, y2)
}

EXPORT
void wasm_transform_path(fz_path *path, fz_matrix *ctm)
{
	VOID(fz_transform_path, path, *ctm)
}

EXPORT
fz_rect * wasm_bound_path(fz_path *path, fz_stroke_state *stroke, fz_matrix *ctm)
{
	RECT(fz_bound_path, path, stroke, *ctm)
}

// --- Text ---

EXPORT
fz_text * wasm_new_text(void)
{
	POINTER(fz_new_text)
}

EXPORT
fz_rect * wasm_bound_text(fz_text *text, fz_stroke_state *stroke, fz_matrix *ctm)
{
	RECT(fz_bound_text, text, stroke, *ctm)
}

EXPORT
void wasm_show_glyph(fz_text *text, fz_font *font, fz_matrix *trm, int gid, int ucs, int wmode)
{
	VOID(fz_show_glyph, text, font, *trm, gid, ucs, wmode, 0, 0, FZ_LANG_UNSET)
}

EXPORT
void wasm_show_string(fz_text *text, fz_font *font, fz_matrix *trm, char *string, int wmode)
{
	VOID(fz_show_string, text, font, *trm, string, wmode, 0, 0, FZ_LANG_UNSET)
}

// --- Device ---

EXPORT
fz_device * wasm_new_draw_device(fz_matrix *ctm, fz_pixmap *dest)
{
	POINTER(fz_new_draw_device, *ctm, dest)
}

EXPORT
fz_device * wasm_new_display_list_device(fz_display_list *list)
{
	POINTER(fz_new_list_device, list)
}

EXPORT
void wasm_close_device(fz_device *dev)
{
	VOID(fz_close_device, dev)
}

EXPORT
void wasm_fill_path(fz_device *dev, fz_path *path, int evenOdd, fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	VOID(fz_fill_path, dev, path, evenOdd, *ctm, colorspace, color, alpha, fz_default_color_params)
}

EXPORT
void wasm_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	VOID(fz_stroke_path, dev, path, stroke, *ctm, colorspace, color, alpha, fz_default_color_params)
}

EXPORT
void wasm_clip_path(fz_device *dev, fz_path *path, int evenOdd, fz_matrix *ctm)
{
	VOID(fz_clip_path, dev, path, evenOdd, *ctm, fz_infinite_rect)
}

EXPORT
void wasm_clip_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, fz_matrix *ctm)
{
	VOID(fz_clip_stroke_path, dev, path, stroke, *ctm, fz_infinite_rect)
}

EXPORT
void wasm_fill_text(fz_device *dev, fz_text *text, fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	VOID(fz_fill_text, dev, text, *ctm, colorspace, color, alpha, fz_default_color_params)
}

EXPORT
void wasm_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	VOID(fz_stroke_text, dev, text, stroke, *ctm, colorspace, color, alpha, fz_default_color_params)
}

EXPORT
void wasm_clip_text(fz_device *dev, fz_text *text, fz_matrix *ctm)
{
	VOID(fz_clip_text, dev, text, *ctm, fz_infinite_rect)
}

EXPORT
void wasm_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix *ctm)
{
	VOID(fz_clip_stroke_text, dev, text, stroke, *ctm, fz_infinite_rect)
}

EXPORT
void wasm_ignore_text(fz_device *dev, fz_text *text, fz_matrix *ctm)
{
	VOID(fz_ignore_text, dev, text, *ctm)
}

EXPORT
void wasm_fill_shade(fz_device *dev, fz_shade *shade, fz_matrix *ctm, float alpha)
{
	VOID(fz_fill_shade, dev, shade, *ctm, alpha, fz_default_color_params)
}

EXPORT
void wasm_fill_image(fz_device *dev, fz_image *image, fz_matrix *ctm, float alpha)
{
	VOID(fz_fill_image, dev, image, *ctm, alpha, fz_default_color_params)
}

EXPORT
void wasm_fill_image_mask(fz_device *dev, fz_image *image, fz_matrix *ctm, fz_colorspace *colorspace, float *color, float alpha)
{
	VOID(fz_fill_image_mask, dev, image, *ctm, colorspace, color, alpha, fz_default_color_params)
}

EXPORT
void wasm_clip_image_mask(fz_device *dev, fz_image *image, fz_matrix *ctm)
{
	VOID(fz_clip_image_mask, dev, image, *ctm, fz_infinite_rect)
}

EXPORT
void wasm_pop_clip(fz_device *dev)
{
	VOID(fz_pop_clip, dev)
}

EXPORT
void wasm_begin_mask(fz_device *dev, fz_rect *area, int luminosity, fz_colorspace *colorspace, float *color)
{
	VOID(fz_begin_mask, dev, *area, luminosity, colorspace, color, fz_default_color_params)
}

EXPORT
void wasm_end_mask(fz_device *dev)
{
	VOID(fz_end_mask, dev)
}

EXPORT
void wasm_begin_group(fz_device *dev, fz_rect *area, fz_colorspace *colorspace, int isolated, int knockout, int blendmode, float alpha)
{
	VOID(fz_begin_group, dev, *area, colorspace, isolated, knockout, blendmode, alpha)
}

EXPORT
void wasm_end_group(fz_device *dev)
{
	VOID(fz_end_group, dev)
}

EXPORT
int wasm_begin_tile(fz_device *dev, fz_rect *area, fz_rect *view, float xstep, float ystep, fz_matrix *ctm, int id)
{
	INTEGER(fz_begin_tile_id, dev, *area, *view, xstep, ystep, *ctm, id)
}

EXPORT
void wasm_end_tile(fz_device *dev)
{
	VOID(fz_end_tile, dev)
}

EXPORT
void wasm_begin_layer(fz_device *dev, char *name)
{
	VOID(fz_begin_layer, dev, name)
}

EXPORT
void wasm_end_layer(fz_device *dev)
{
	VOID(fz_end_layer, dev)
}

// --- DocumentWriter ---

EXPORT
fz_document_writer * wasm_new_document_writer_with_buffer(fz_buffer *buf, char *format, char *options)
{
	POINTER(fz_new_document_writer_with_buffer, buf, format, options)
}

EXPORT
fz_device * wasm_begin_page(fz_document_writer *wri, fz_rect *mediabox)
{
	POINTER(fz_begin_page, wri, *mediabox)
}

EXPORT
void wasm_end_page(fz_document_writer *wri)
{
	VOID(fz_end_page, wri)
}

EXPORT
void wasm_close_document_writer(fz_document_writer *wri)
{
	VOID(fz_close_document_writer, wri)
}

// --- StructuredText ---

EXPORT
unsigned char * wasm_print_stext_page_as_json(fz_stext_page *page, float scale)
{
	unsigned char *data = NULL;
	TRY ({
		fz_buffer *buf = fz_new_buffer(ctx, 1024);
		fz_output *out = fz_new_output_with_buffer(ctx, buf);
		fz_print_stext_page_as_json(ctx, out, page, scale);
		fz_close_output(ctx, out);
		fz_drop_output(ctx, out);
		fz_terminate_buffer(ctx, buf);
		fz_buffer_extract(ctx, buf, &data);
	})
	return data;
}

EXPORT
int wasm_search_stext_page(fz_stext_page *text, char *needle, int *marks, fz_quad *hits, int hit_max)
{
	INTEGER(fz_search_stext_page, text, needle, marks, hits, hit_max)
}

// --- Document ---

EXPORT
fz_document * wasm_open_document_with_buffer(char *magic, fz_buffer *buffer)
{
	POINTER(fz_open_document_with_buffer, magic, buffer)
}

EXPORT
fz_document * wasm_open_document_with_stream(char *magic, fz_stream *stream)
{
	POINTER(fz_open_document_with_stream, magic, stream)
}

EXPORT
char * wasm_format_link_uri(fz_document *doc, int ch, int pg, int ty, float x, float y, float w, float h, float z)
{
	fz_link_dest dest = { ch, pg, ty, x, y, w, h, z };
	POINTER(fz_format_link_uri, doc, dest)
}

EXPORT
int wasm_needs_password(fz_document *doc)
{
	INTEGER(fz_needs_password, doc)
}

EXPORT
int wasm_authenticate_password(fz_document *doc, char *password)
{
	INTEGER(fz_authenticate_password, doc, password)
}

EXPORT
int wasm_has_permission(fz_document *doc, int perm)
{
	INTEGER(fz_has_permission, doc, perm)
}

EXPORT
int wasm_count_pages(fz_document *doc)
{
	INTEGER(fz_count_pages, doc)
}

EXPORT
fz_page * wasm_load_page(fz_document *doc, int number)
{
	POINTER(fz_load_page, doc, number)
}

EXPORT
char * wasm_lookup_metadata(fz_document *doc, char *key)
{
	static char buf[500];
	char *result = NULL;
	TRY ({
		if (fz_lookup_metadata(ctx, doc, key, buf, sizeof buf) > 0)
			result = buf;
	})
	return result;
}

EXPORT
void wasm_set_metadata(fz_document *doc, char *key, char *value)
{
	VOID(fz_set_metadata, doc, key, value)
}

EXPORT
int wasm_resolve_link(fz_document *doc, const char *uri)
{
	INTEGER(fz_page_number_from_location, doc, fz_resolve_link(ctx, doc, uri, NULL, NULL))
}

EXPORT
fz_outline * wasm_load_outline(fz_document *doc)
{
	POINTER(fz_load_outline, doc)
}

EXPORT
int wasm_outline_get_page(fz_document *doc, fz_outline *outline)
{
	INTEGER(fz_page_number_from_location, doc, outline->page)
}

EXPORT
void wasm_layout_document(fz_document *doc, float w, float h, float em)
{
	VOID(fz_layout_document, doc, w, h, em)
}

// --- Page ---

EXPORT
fz_rect * wasm_bound_page(fz_page *page)
{
	RECT(fz_bound_page, page)
}

EXPORT
fz_link * wasm_load_links(fz_page *page)
{
	POINTER(fz_load_links, page)
}

EXPORT
fz_link * wasm_create_link(fz_page *page, fz_rect *bbox, char *uri)
{
	POINTER(fz_create_link, page, *bbox, uri)
}

EXPORT
void wasm_delete_link(fz_page *page, fz_link *link)
{
	VOID(fz_delete_link, page, link)
}

EXPORT
void wasm_run_page(fz_page *page, fz_device *dev, fz_matrix *ctm)
{
	VOID(fz_run_page, page, dev, *ctm, NULL)
}

EXPORT
void wasm_run_page_contents(fz_page *page, fz_device *dev, fz_matrix *ctm)
{
	VOID(fz_run_page_contents, page, dev, *ctm, NULL)
}

EXPORT
void wasm_run_page_annots(fz_page *page, fz_device *dev, fz_matrix *ctm)
{
	VOID(fz_run_page_annots, page, dev, *ctm, NULL)
}

EXPORT
void wasm_run_page_widgets(fz_page *page, fz_device *dev, fz_matrix *ctm)
{
	VOID(fz_run_page_widgets, page, dev, *ctm, NULL)
}

EXPORT
fz_stext_page * wasm_new_stext_page_from_page(fz_page *page)
{
	// TODO: parse options
	fz_stext_options options = { FZ_STEXT_PRESERVE_SPANS };
	POINTER(fz_new_stext_page_from_page, page, &options)
}

EXPORT
fz_display_list * wasm_new_display_list_from_page(fz_page *page)
{
	POINTER(fz_new_display_list_from_page, page)
}

EXPORT
fz_display_list * wasm_new_display_list_from_page_contents(fz_page *page)
{
	POINTER(fz_new_display_list_from_page_contents, page)
}

EXPORT
char * wasm_page_label(fz_page *page)
{
	static char buf[100];
	POINTER(fz_page_label, page, buf, sizeof buf)
}

EXPORT
int wasm_search_page(fz_page *page, char *needle, int *marks, fz_quad *hits, int hit_max)
{
	INTEGER(fz_search_page, page, needle, marks, hits, hit_max)
}

// --- DocumentIterator ---

EXPORT
fz_outline_iterator * wasm_new_outline_iterator(fz_document *doc)
{
	POINTER(fz_new_outline_iterator, doc)
}

EXPORT
int wasm_outline_iterator_next(fz_outline_iterator *iter)
{
	INTEGER(fz_outline_iterator_next, iter)
}

EXPORT
int wasm_outline_iterator_prev(fz_outline_iterator *iter)
{
	INTEGER(fz_outline_iterator_prev, iter)
}

EXPORT
int wasm_outline_iterator_up(fz_outline_iterator *iter)
{
	INTEGER(fz_outline_iterator_up, iter)
}

EXPORT
int wasm_outline_iterator_down(fz_outline_iterator *iter)
{
	INTEGER(fz_outline_iterator_down, iter)
}

EXPORT
int wasm_outline_iterator_delete(fz_outline_iterator *iter)
{
	INTEGER(fz_outline_iterator_delete, iter)
}

EXPORT
fz_outline_item * wasm_outline_iterator_item(fz_outline_iterator *iter)
{
	POINTER(fz_outline_iterator_item, iter)
}

EXPORT
int wasm_outline_iterator_insert(fz_outline_iterator *iter, char *title, char *uri, int is_open)
{
	fz_outline_item item = { title, uri, is_open };
	INTEGER(fz_outline_iterator_insert, iter, &item)
}

EXPORT
void wasm_outline_iterator_update(fz_outline_iterator *iter, char *title, char *uri, int is_open)
{
	fz_outline_item item = { title, uri, is_open };
	VOID(fz_outline_iterator_update, iter, &item)
}

// --- PDFDocument --

EXPORT
pdf_document * wasm_pdf_document_from_fz_document(fz_document *document)
{
	return pdf_document_from_fz_document(ctx, document);
}

EXPORT
pdf_page * wasm_pdf_page_from_fz_page(fz_page *page)
{
	return pdf_page_from_fz_page(ctx, page);
}

EXPORT
pdf_document * wasm_pdf_create_document(void)
{
	POINTER(pdf_create_document)
}

EXPORT
int wasm_pdf_version(pdf_document *doc)
{
	INTEGER(pdf_version, doc)
}

EXPORT
int wasm_pdf_was_repaired(pdf_document *doc)
{
	INTEGER(pdf_was_repaired, doc)
}

EXPORT
int wasm_pdf_has_unsaved_changes(pdf_document *doc)
{
	INTEGER(pdf_has_unsaved_changes, doc)
}

EXPORT
int wasm_pdf_can_be_saved_incrementally(pdf_document *doc)
{
	INTEGER(pdf_can_be_saved_incrementally, doc)
}

EXPORT
int wasm_pdf_count_versions(pdf_document *doc)
{
	INTEGER(pdf_count_versions, doc)
}

EXPORT
int wasm_pdf_count_unsaved_versions(pdf_document *doc)
{
	INTEGER(pdf_count_unsaved_versions, doc)
}

EXPORT
int wasm_pdf_validate_change_history(pdf_document *doc)
{
	INTEGER(pdf_validate_change_history, doc)
}

EXPORT
void wasm_pdf_enable_journal(pdf_document *doc)
{
	VOID(pdf_enable_journal, doc)
}

EXPORT
int wasm_pdf_undoredo_state_position(pdf_document *doc)
{
	int position, count;
	TRY ({
		position = pdf_undoredo_state(ctx, doc, &count);
	})
	return position;
}

EXPORT
int wasm_pdf_undoredo_state_count(pdf_document *doc)
{
	int position, count;
	TRY ({
		position = pdf_undoredo_state(ctx, doc, &count);
	})
	return count;
}

EXPORT
char * wasm_pdf_undoredo_step(pdf_document *doc, int step)
{
	POINTER(pdf_undoredo_step, doc, step)
}

EXPORT
void wasm_pdf_begin_operation(pdf_document *doc, char *op)
{
	VOID(pdf_begin_operation, doc, op)
}

EXPORT
void wasm_pdf_begin_implicit_operation(pdf_document *doc)
{
	VOID(pdf_begin_implicit_operation, doc)
}

EXPORT
void wasm_pdf_end_operation(pdf_document *doc)
{
	VOID(pdf_end_operation, doc)
}

EXPORT
void wasm_pdf_undo(pdf_document *doc)
{
	VOID(pdf_undo, doc)
}

EXPORT
void wasm_pdf_redo(pdf_document *doc)
{
	VOID(pdf_redo, doc)
}

EXPORT
int wasm_pdf_can_undo(pdf_document *doc)
{
	INTEGER(pdf_can_undo, doc)
}

EXPORT
int wasm_pdf_can_redo(pdf_document *doc)
{
	INTEGER(pdf_can_redo, doc)
}

EXPORT
char * wasm_pdf_document_language(pdf_document *doc)
{
	static char str[8];
	TRY ({
		fz_string_from_text_language(str, pdf_document_language(ctx, doc));
	})
	return str;
}

EXPORT
void wasm_pdf_set_document_language(pdf_document *doc, char *str)
{
	VOID(pdf_set_document_language, doc, fz_text_language_from_string(str))
}

EXPORT
pdf_obj * wasm_pdf_trailer(pdf_document *doc)
{
	POINTER(pdf_trailer, doc)
}

EXPORT
int wasm_pdf_xref_len(pdf_document *doc)
{
	INTEGER(pdf_xref_len, doc)
}

EXPORT
fz_image * wasm_pdf_lookup_page_obj(pdf_document *doc, int index)
{
	POINTER(pdf_lookup_page_obj, doc, index)
}

EXPORT
pdf_obj * wasm_pdf_add_object(pdf_document *doc, pdf_obj *obj)
{
	POINTER(pdf_add_object, doc, obj)
}

EXPORT
int wasm_pdf_create_object(pdf_document *doc)
{
	INTEGER(pdf_create_object, doc)
}

EXPORT
void wasm_pdf_delete_object(pdf_document *doc, int num)
{
	VOID(pdf_delete_object, doc, num)
}

EXPORT
pdf_obj * wasm_pdf_add_stream(pdf_document *doc, fz_buffer *buf, pdf_obj *obj, int compress)
{
	POINTER(pdf_add_stream, doc, buf, obj, compress)
}

EXPORT
pdf_obj * wasm_pdf_add_simple_font(pdf_document *doc, fz_font *font, int encoding)
{
	POINTER(pdf_add_simple_font, doc, font, encoding)
}

EXPORT
pdf_obj * wasm_pdf_add_cjk_font(pdf_document *doc, fz_font *font, int ordering, int wmode, int serif)
{
	POINTER(pdf_add_cjk_font, doc, font, ordering, wmode, serif)
}

EXPORT
pdf_obj * wasm_pdf_add_cid_font(pdf_document *doc, fz_font *font)
{
	POINTER(pdf_add_cid_font, doc, font)
}

EXPORT
pdf_obj * wasm_pdf_add_image(pdf_document *doc, fz_image *image)
{
	POINTER(pdf_add_image, doc, image)
}

EXPORT
fz_image * wasm_pdf_load_image(pdf_document *doc, pdf_obj *ref)
{
	POINTER(pdf_load_image, doc, ref)
}

EXPORT
pdf_obj * wasm_pdf_add_page(pdf_document *doc, fz_rect *mediabox, int rotate, pdf_obj *resources, fz_buffer *contents)
{
	POINTER(pdf_add_page, doc, *mediabox, rotate, resources, contents)
}

EXPORT
void wasm_pdf_insert_page(pdf_document *doc, int index, pdf_obj *obj)
{
	VOID(pdf_insert_page, doc, index, obj)
}

EXPORT
void wasm_pdf_delete_page(pdf_document *doc, int index)
{
	VOID(pdf_delete_page, doc, index)
}

EXPORT
void wasm_pdf_set_page_labels(pdf_document *doc, int index, int style, char *prefix, int start)
{
	VOID(pdf_set_page_labels, doc, index, style, prefix, start)
}

EXPORT
void wasm_pdf_delete_page_labels(pdf_document *doc, int index)
{
	VOID(pdf_delete_page_labels, doc, index)
}

EXPORT
int wasm_pdf_is_embedded_file(pdf_obj *ref)
{
	INTEGER(pdf_is_embedded_file, ref)
}

EXPORT
pdf_embedded_file_params * wasm_pdf_get_embedded_file_params(pdf_obj *ref)
{
	static pdf_embedded_file_params out;
	TRY ({
		pdf_get_embedded_file_params(ctx, ref, &out);
	})
	return &out;
}

EXPORT
pdf_obj * wasm_pdf_add_embedded_file(pdf_document *doc, char *filename, char *mimetype, fz_buffer *contents, int created, int modified, int checksum)
{
	POINTER(pdf_add_embedded_file, doc, filename, mimetype, contents, created, modified, checksum)
}

EXPORT
fz_buffer * wasm_pdf_load_embedded_file_contents(pdf_obj *fs)
{
	POINTER(pdf_load_embedded_file_contents, fs)
}

EXPORT
fz_buffer * wasm_pdf_write_document_buffer(pdf_document *doc, char *options)
{
	fz_buffer *buffer;
	fz_output *output;
	pdf_write_options pwo;
	TRY ({
		buffer = fz_new_buffer(ctx, 32 << 10);
		output = fz_new_output_with_buffer(ctx, buffer);
		pdf_parse_write_options(ctx, &pwo, options);
		pdf_write_document(ctx, doc, output, &pwo);
		fz_close_output(ctx, output);
		fz_drop_output(ctx, output);
	})
	return buffer;
}

// --- PDFPage ---

EXPORT
fz_matrix * wasm_pdf_page_transform(pdf_page *page)
{
	TRY ({
		pdf_page_transform(ctx, page, NULL, &out_matrix);
	})
	return &out_matrix;
}

EXPORT
pdf_annot * wasm_pdf_first_annot(pdf_page *page)
{
	POINTER(pdf_first_annot, page)
}

EXPORT
pdf_annot * wasm_pdf_next_annot(pdf_annot *annot)
{
	POINTER(pdf_next_annot, annot)
}

EXPORT
pdf_annot * wasm_pdf_first_widget(pdf_page *page)
{
	POINTER(pdf_first_widget, page)
}

EXPORT
pdf_annot * wasm_pdf_next_widget(pdf_annot *annot)
{
	POINTER(pdf_next_widget, annot)
}

EXPORT
pdf_annot * wasm_pdf_create_annot(pdf_page *page, int type)
{
	POINTER(pdf_create_annot, page, type)
}

EXPORT
void wasm_pdf_delete_annot(pdf_page *page, pdf_annot *annot)
{
	VOID(pdf_delete_annot, page, annot)
}

EXPORT
int wasm_pdf_update_page(pdf_page *page)
{
	INTEGER(pdf_update_page, page)
}

EXPORT
void wasm_pdf_redact_page(pdf_page *page, int black_boxes, int image_method)
{
	pdf_redact_options opts = { black_boxes, image_method };
	VOID(pdf_redact_page, page->doc, page, &opts)
}

// --- PDFAnnotation ---

EXPORT
fz_rect * wasm_pdf_bound_annot(pdf_annot *annot)
{
	RECT(pdf_bound_annot, annot)
}

EXPORT
void wasm_pdf_run_annot(pdf_annot *annot, fz_device *dev, fz_matrix *ctm)
{
	VOID(pdf_run_annot, annot, dev, *ctm, NULL)
}

EXPORT
fz_pixmap * wasm_pdf_new_pixmap_from_annot(pdf_annot *annot, fz_matrix *ctm, fz_colorspace *colorspace, int alpha)
{
	POINTER(pdf_new_pixmap_from_annot, annot, *ctm, colorspace, NULL, alpha)
}

EXPORT
fz_display_list * wasm_pdf_new_display_list_from_annot(pdf_annot *annot)
{
	POINTER(pdf_new_display_list_from_annot, annot)
}

EXPORT
int wasm_pdf_update_annot(pdf_annot *annot)
{
	INTEGER(pdf_update_annot, annot)
}

#define PDF_ANNOT_GET(T,R,N) EXPORT T wasm_pdf_annot_ ## N (pdf_annot *annot) { R(pdf_annot_ ## N, annot) }
#define PDF_ANNOT_GET1(T,R,N) EXPORT T wasm_pdf_annot_ ## N (pdf_annot *annot, int idx) { R(pdf_annot_ ## N, annot, idx) }
#define PDF_ANNOT_GET2(T,R,N) EXPORT T wasm_pdf_annot_ ## N (pdf_annot *annot, int a, int b) { R(pdf_annot_ ## N, annot, a, b) }
#define PDF_ANNOT_SET(T,N) EXPORT void wasm_pdf_set_annot_ ## N (pdf_annot *annot, T v) { VOID(pdf_set_annot_ ## N, annot, v) }
#define PDF_ANNOT_GETSET(T,R,N) PDF_ANNOT_GET(T,R,N) PDF_ANNOT_SET(T,N)

PDF_ANNOT_GET(pdf_obj*, POINTER, obj)
PDF_ANNOT_GET(int, INTEGER, type)

PDF_ANNOT_GETSET(int, INTEGER, flags)
PDF_ANNOT_GETSET(char*, POINTER, contents)
PDF_ANNOT_GETSET(char*, POINTER, author)
PDF_ANNOT_GETSET(int, INTEGER, creation_date)
PDF_ANNOT_GETSET(int, INTEGER, modification_date)
PDF_ANNOT_GETSET(float, NUMBER, border_width)
PDF_ANNOT_GETSET(int, INTEGER, border_style)
PDF_ANNOT_GETSET(int, INTEGER, border_effect)
PDF_ANNOT_GETSET(float, NUMBER, border_effect_intensity)
PDF_ANNOT_GETSET(float, NUMBER, opacity)
PDF_ANNOT_GETSET(pdf_obj*, POINTER, filespec)
PDF_ANNOT_GETSET(int, INTEGER, quadding)
PDF_ANNOT_GETSET(int, INTEGER, is_open)
PDF_ANNOT_GETSET(char*, POINTER, icon_name)

PDF_ANNOT_GET(fz_rect*, RECT, rect)
PDF_ANNOT_GET(fz_rect*, RECT, popup)

PDF_ANNOT_GET(int, INTEGER, quad_point_count)
PDF_ANNOT_GET1(fz_quad*, QUAD, quad_point)

PDF_ANNOT_GET(int, INTEGER, vertex_count)
PDF_ANNOT_GET1(fz_point*, POINT, vertex)

PDF_ANNOT_GET(int, INTEGER, ink_list_count)
PDF_ANNOT_GET1(int, INTEGER, ink_list_stroke_count)
PDF_ANNOT_GET2(fz_point*, POINT, ink_list_stroke_vertex)

PDF_ANNOT_GET(int, INTEGER, border_dash_count)
PDF_ANNOT_GET1(float, NUMBER, border_dash_item)

EXPORT
char * wasm_pdf_annot_language(pdf_annot *doc)
{
	static char str[8];
	TRY ({
		fz_string_from_text_language(str, pdf_annot_language(ctx, doc));
	})
	return str;
}

EXPORT
void wasm_pdf_set_annot_language(pdf_annot *doc, char *str)
{
	VOID(pdf_set_annot_language, doc, fz_text_language_from_string(str))
}

EXPORT
void wasm_pdf_set_annot_popup(pdf_annot *annot, fz_rect *rect)
{
	VOID(pdf_set_annot_popup, annot, *rect)
}

EXPORT
void wasm_pdf_set_annot_rect(pdf_annot *annot, fz_rect *rect)
{
	VOID(pdf_set_annot_rect, annot, *rect)
}

EXPORT
void wasm_pdf_clear_annot_quad_points(pdf_annot *annot)
{
	VOID(pdf_clear_annot_quad_points, annot)
}

EXPORT
void wasm_pdf_clear_annot_vertices(pdf_annot *annot)
{
	VOID(pdf_clear_annot_vertices, annot)
}

EXPORT
void wasm_pdf_clear_annot_ink_list(pdf_annot *annot)
{
	VOID(pdf_clear_annot_ink_list, annot)
}

EXPORT
void wasm_pdf_clear_annot_border_dash(pdf_annot *annot)
{
	VOID(pdf_clear_annot_border_dash, annot)
}

EXPORT
void wasm_pdf_add_annot_quad_point(pdf_annot *annot, fz_quad *quad)
{
	VOID(pdf_add_annot_quad_point, annot, *quad)
}

EXPORT
void wasm_pdf_add_annot_vertex(pdf_annot *annot, fz_point *point)
{
	VOID(pdf_add_annot_vertex, annot, *point)
}

EXPORT
void wasm_pdf_add_annot_ink_list_stroke(pdf_annot *annot)
{
	VOID(pdf_add_annot_ink_list_stroke, annot)
}

EXPORT
void wasm_pdf_add_annot_ink_list_stroke_vertex(pdf_annot *annot, fz_point *point)
{
	VOID(pdf_add_annot_ink_list_stroke_vertex, annot, *point)
}

EXPORT
void wasm_pdf_add_annot_border_dash_item(pdf_annot *annot, float v)
{
	VOID(pdf_add_annot_border_dash_item, annot, v)
}

EXPORT
int wasm_pdf_annot_line_ending_styles_start(pdf_annot *annot)
{
	enum pdf_line_ending start;
	enum pdf_line_ending end;
	TRY ({
		pdf_annot_line_ending_styles(ctx, annot, &start, &end);
	})
	return start;
}

EXPORT
fz_point * wasm_pdf_annot_line_1(pdf_annot *annot)
{
	fz_point tmp;
	TRY ({
		pdf_annot_line(ctx, annot, &out_point, &tmp);
	})
	return &out_point;
}

EXPORT
fz_point * wasm_pdf_annot_line_2(pdf_annot *annot)
{
	fz_point tmp;
	TRY ({
		pdf_annot_line(ctx, annot, &tmp, &out_point);
	})
	return &out_point;
}

EXPORT
void wasm_pdf_set_annot_line(pdf_annot *annot, fz_point *a, fz_point *b)
{
	VOID(pdf_set_annot_line, annot, *a, *b)
}

EXPORT
int wasm_pdf_annot_line_ending_styles_end(pdf_annot *annot)
{
	enum pdf_line_ending start;
	enum pdf_line_ending end;
	TRY ({
		pdf_annot_line_ending_styles(ctx, annot, &start, &end);
	})
	return end;
}

EXPORT
void wasm_pdf_set_annot_line_ending_styles(pdf_annot *annot, int start, int end)
{
	VOID(pdf_set_annot_line_ending_styles, annot, start, end)
}

EXPORT
int wasm_pdf_annot_color(pdf_annot *annot, float *color)
{
	int n;
	TRY ({
		pdf_annot_color(ctx, annot, &n, color);
	})
	return n;
}

EXPORT
int wasm_pdf_annot_interior_color(pdf_annot *annot, float *color)
{
	int n;
	TRY ({
		pdf_annot_interior_color(ctx, annot, &n, color);
	})
	return n;
}

EXPORT
void wasm_pdf_set_annot_color(pdf_annot *annot, int n, float *color)
{
	VOID(pdf_set_annot_color, annot, n, color);
}

EXPORT
void wasm_pdf_set_annot_interior_color(pdf_annot *annot, int n, float *color)
{
	VOID(pdf_set_annot_interior_color, annot, n, color);
}

EXPORT
void wasm_pdf_set_annot_default_appearance(pdf_annot *annot, char *font, float size, int ncolor, float *color)
{
	VOID(pdf_set_annot_default_appearance, annot, font, size, ncolor, color)
}

EXPORT
const char * wasm_pdf_annot_default_appearance_font(pdf_annot *annot)
{
	const char *font;
	float size, color[4];
	int n;
	TRY({
		pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);
	})
	return font;
}

EXPORT
float wasm_pdf_annot_default_appearance_size(pdf_annot *annot)
{
	const char *font;
	float size, color[4];
	int n;
	TRY({
		pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);
	})
	return size;
}

EXPORT
int wasm_pdf_annot_default_appearance_color(pdf_annot *annot, float *color)
{
	const char *font;
	float size;
	int n;
	TRY({
		pdf_annot_default_appearance(ctx, annot, &font, &size, &n, color);
	})
	return n;
}

EXPORT
void wasm_pdf_set_annot_appearance_from_display_list(pdf_annot *annot,
	char *appearance, char *state, fz_matrix *ctm, fz_display_list *list)
{
	VOID(pdf_set_annot_appearance_from_display_list, annot, appearance, state, *ctm, list)
}

EXPORT
void wasm_pdf_set_annot_appearance(pdf_annot *annot,
	char *appearance, char *state, fz_matrix *ctm, fz_rect *bbox, pdf_obj *resources, fz_buffer *contents)
{
	VOID(pdf_set_annot_appearance, annot, appearance, state, *ctm, *bbox, resources, contents)
}

EXPORT
void wasm_pdf_apply_redaction(pdf_annot *annot, int black_boxes, int image_method)
{
	pdf_redact_options opts = { black_boxes, image_method };
	VOID(pdf_apply_redaction, annot, &opts)
}

// --- PDFWidget ---

EXPORT
int wasm_pdf_annot_field_type(pdf_annot *widget)
{
	INTEGER(pdf_field_type, pdf_annot_obj(ctx, widget))
}

EXPORT
int wasm_pdf_annot_field_flags(pdf_annot *widget)
{
	INTEGER(pdf_field_flags, pdf_annot_obj(ctx, widget))
}

PDF_ANNOT_GET(char*, POINTER, field_label)
PDF_ANNOT_GET(char*, POINTER, field_value)

EXPORT
char * wasm_pdf_load_field_name(pdf_annot *widget)
{
	POINTER(pdf_load_field_name, pdf_annot_obj(ctx, widget))
}

EXPORT
int wasm_pdf_annot_text_widget_max_len(pdf_annot *widget)
{
	INTEGER(pdf_text_widget_max_len, widget)
}

EXPORT
int wasm_pdf_set_annot_text_field_value(pdf_annot *widget, char *value)
{
	INTEGER(pdf_set_text_field_value, widget, value)
}

EXPORT
int wasm_pdf_set_annot_choice_field_value(pdf_annot *widget, char *value)
{
	INTEGER(pdf_set_choice_field_value, widget, value)
}

EXPORT
int wasm_pdf_annot_choice_field_option_count(pdf_annot *widget)
{
	INTEGER(pdf_choice_field_option_count, pdf_annot_obj(ctx, widget))
}

EXPORT
char * wasm_pdf_annot_choice_field_option(pdf_annot *widget, int export, int i)
{
	POINTER(pdf_choice_field_option, pdf_annot_obj(ctx, widget), export, i)
}

EXPORT
int wasm_pdf_toggle_widget(pdf_annot *widget)
{
	INTEGER(pdf_toggle_widget, widget)
}

// --- PDFObject ---

#define PDF_IS(N) EXPORT int wasm_pdf_is_ ## N (pdf_obj *obj) { INTEGER(pdf_is_ ## N, obj) }
#define PDF_TO(T,R,N) EXPORT T wasm_pdf_to_ ## N (pdf_obj *obj) { R(pdf_to_ ## N, obj) }
#define PDF_NEW(N,T) EXPORT pdf_obj* wasm_pdf_new_ ## N (pdf_document *doc, T v) { POINTER(pdf_new_ ## N, doc, v) }
#define PDF_NEW2(N,T1,T2) EXPORT pdf_obj* wasm_pdf_new_ ## N (pdf_document *doc, T1 v1, T2 v2) { POINTER(pdf_new_ ## N, doc, v1, v2) }

PDF_IS(indirect)
PDF_IS(bool)
PDF_IS(number)
PDF_IS(name)
PDF_IS(string)

PDF_IS(array)
PDF_IS(dict)
PDF_IS(stream)

PDF_TO(int, INTEGER, num)
PDF_TO(int, INTEGER, bool)
PDF_TO(double, NUMBER, real)
PDF_TO(char*, POINTER, name)
PDF_TO(char*, POINTER, text_string)

EXPORT
pdf_obj * wasm_pdf_new_indirect(pdf_document *doc, int num)
{
	POINTER(pdf_new_indirect, doc, num, 0)
}

EXPORT
pdf_obj * wasm_pdf_new_array(pdf_document *doc, int cap)
{
	POINTER(pdf_new_array, doc, cap)
}

EXPORT
pdf_obj * wasm_pdf_new_dict(pdf_document *doc, int cap)
{
	POINTER(pdf_new_dict, doc, cap)
}

EXPORT
pdf_obj * wasm_pdf_new_bool(int v)
{
	return v ? PDF_TRUE : PDF_FALSE;
}

EXPORT
pdf_obj * wasm_pdf_new_int(int v)
{
	POINTER(pdf_new_int, v)
}

EXPORT
pdf_obj * wasm_pdf_new_real(float v)
{
	POINTER(pdf_new_real, v)
}

EXPORT
pdf_obj * wasm_pdf_new_name(char *v)
{
	POINTER(pdf_new_name, v)
}

EXPORT
pdf_obj * wasm_pdf_new_text_string(char *v)
{
	POINTER(pdf_new_text_string, v)
}

EXPORT
pdf_obj * wasm_pdf_resolve_indirect(pdf_obj *obj)
{
	POINTER(pdf_resolve_indirect, obj)
}

EXPORT
pdf_obj * wasm_pdf_array_len(pdf_obj *obj)
{
	POINTER(pdf_array_len, obj)
}

EXPORT
pdf_obj * wasm_pdf_array_get(pdf_obj *obj, int idx)
{
	POINTER(pdf_array_get, obj, idx)
}

EXPORT
pdf_obj * wasm_pdf_dict_get(pdf_obj *obj, pdf_obj *key)
{
	POINTER(pdf_dict_get, obj, key)
}

EXPORT
pdf_obj * wasm_pdf_dict_gets(pdf_obj *obj, char *key)
{
	POINTER(pdf_dict_gets, obj, key)
}

EXPORT
void wasm_pdf_dict_put(pdf_obj *obj, pdf_obj *key, pdf_obj *val)
{
	VOID(pdf_dict_put, obj, key, val)
}

EXPORT
void wasm_pdf_dict_puts(pdf_obj *obj, char *key, pdf_obj *val)
{
	VOID(pdf_dict_puts, obj, key, val)
}

EXPORT
void wasm_pdf_dict_del(pdf_obj *obj, pdf_obj *key)
{
	VOID(pdf_dict_del, obj, key)
}

EXPORT
void wasm_pdf_dict_dels(pdf_obj *obj, char *key)
{
	VOID(pdf_dict_dels, obj, key)
}

EXPORT
void wasm_pdf_array_put(pdf_obj *obj, int key, pdf_obj *val)
{
	VOID(pdf_array_put, obj, key, val)
}

EXPORT
void wasm_pdf_array_push(pdf_obj *obj, pdf_obj *val)
{
	VOID(pdf_array_push, obj, val)
}

EXPORT
void wasm_pdf_array_delete(pdf_obj *obj, int key)
{
	VOID(pdf_array_delete, obj, key)
}

EXPORT
char * wasm_pdf_sprint_obj(pdf_obj *obj, int tight, int ascii)
{
	size_t len;
	POINTER(pdf_sprint_obj, NULL, 0, &len, obj, tight, ascii)
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

EXPORT
void wasm_on_data_fetched(struct fetch_state *state, int block, uint8_t *data, int size)
{
	if (state->content) {
		memcpy(state->content + (block << state->block_shift), data, size);
		state->map[block] = 2;
	}
}

EXPORT
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
