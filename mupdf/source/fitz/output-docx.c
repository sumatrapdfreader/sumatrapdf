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

#if FZ_ENABLE_DOCX_OUTPUT

#include "glyphbox.h"
#include "extract/extract.h"
#include "extract/buffer.h"

#include <assert.h>
#include <errno.h>
#include <string.h>


typedef struct
{
	fz_document_writer super;
	extract_alloc_t *alloc;

	/*
	 * .ctx is needed for the callbacks we get from the Extract library, for
	 * example s_realloc_fn(). Each of our main device callbacks sets .ctx on
	 * entry, and resets back to NULL before returning.
	 */
	fz_context *ctx;

	fz_output *output;
	extract_t *extract;
	int spacing;
	int rotation;
	int images;
	int mediabox_clip;
	fz_rect mediabox; /* As passed to writer_begin_page(). */
	char output_cache[1024];
} fz_docx_writer;


typedef struct
{
	fz_device super;
	fz_docx_writer *writer;
} fz_docx_device;


static void dev_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_docx_device *dev = (fz_docx_device*) dev_;
	fz_text_span *span;
	assert(!dev->writer->ctx);
	dev->writer->ctx = ctx;
	fz_try(ctx)
	{
		for (span = text->head; span; span = span->next)
		{
			int i;
			fz_matrix combined, trm;
			fz_rect bbox;

			combined = fz_concat(span->trm, ctm);

			bbox = span->font->bbox;
			if (extract_span_begin(
					dev->writer->extract,
					span->font->name,
					span->font->flags.is_bold,
					span->font->flags.is_italic,
					span->wmode,
					combined.a,
					combined.b,
					combined.c,
					combined.d,
					bbox.x0,
					bbox.y0,
					bbox.x1,
					bbox.y1))
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to begin span");
			}

			trm = span->trm;
			for (i=0; i<span->len; ++i)
			{
				fz_text_item *item = &span->items[i];
				float adv = 0;
				fz_rect bounds;
				fz_matrix combined;

				trm.e = item->x;
				trm.f = item->y;
				combined = fz_concat(trm, ctm);

				if (dev->writer->mediabox_clip)
					if (fz_glyph_entirely_outside_box(ctx, &ctm, span, item, &dev->writer->mediabox))
						continue;

				if (span->items[i].gid >= 0)
					adv = fz_advance_glyph(ctx, span->font, span->items[i].gid, span->wmode);

				bounds = fz_bound_glyph(ctx, span->font, span->items[i].gid, combined);
				if (extract_add_char(dev->writer->extract, combined.e, combined.f, item->ucs, adv,
							bounds.x0, bounds.y0, bounds.x1, bounds.y1))
					fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to add char");
			}

			if (extract_span_end(dev->writer->extract))
				fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to end span");
		}
	}
	fz_always(ctx)
	{
		dev->writer->ctx = NULL;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void dev_fill_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	dev_text(ctx, dev_, text, ctm, colorspace, color, alpha, color_params);
}

static void dev_stroke_text(fz_context *ctx, fz_device *dev_, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	dev_text(ctx, dev_, text, ctm, colorspace, color, alpha, color_params);
}

static void dev_clip_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	dev_text(ctx, dev_, text, ctm, NULL, NULL, 0 /*alpha*/, fz_default_color_params);
}

static void dev_clip_stroke_text(fz_context *ctx, fz_device *dev_, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	dev_text(ctx, dev_, text, ctm, NULL, 0, 0, fz_default_color_params);
}

static void
dev_ignore_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm)
{
}

static void writer_image_free(void *handle, void *image_data)
{
	fz_docx_writer *writer = handle;
	fz_free(writer->ctx, image_data);
}

static void dev_fill_image(fz_context *ctx, fz_device *dev_, fz_image *img, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_docx_device *dev = (fz_docx_device*) dev_;
	const char *type = NULL;
	fz_compressed_buffer *compressed = fz_compressed_image_buffer(ctx, img);

	assert(!dev->writer->ctx);
	dev->writer->ctx = ctx;
	fz_try(ctx)
	{
		if (compressed)
		{
			if (0) { /* For alignment */ }
			else if (compressed->params.type == FZ_IMAGE_RAW) type = "raw";
			else if (compressed->params.type == FZ_IMAGE_FAX) type = "fax";
			else if (compressed->params.type == FZ_IMAGE_FLATE) type = "flate";
			else if (compressed->params.type == FZ_IMAGE_LZW) type = "lzw";
			else if (compressed->params.type == FZ_IMAGE_BMP) type = "bmp";
			else if (compressed->params.type == FZ_IMAGE_GIF) type = "gif";
			else if (compressed->params.type == FZ_IMAGE_JBIG2) type = "jbig2";
			else if (compressed->params.type == FZ_IMAGE_JPEG) type = "jpeg";
			else if (compressed->params.type == FZ_IMAGE_JPX) type = "jpx";
			else if (compressed->params.type == FZ_IMAGE_JXR) type = "jxr";
			else if (compressed->params.type == FZ_IMAGE_PNG) type = "png";
			else if (compressed->params.type == FZ_IMAGE_PNM) type = "pnm";
			else if (compressed->params.type == FZ_IMAGE_TIFF) type = "tiff";

			if (type)
			{
				/* Write out raw data. */
				unsigned char *data;
				size_t datasize = fz_buffer_extract(ctx, compressed->buffer, &data);
				if (extract_add_image(
						dev->writer->extract,
						type,
						ctm.e /*x*/,
						ctm.f /*y*/,
						img->w /*w*/,
						img->h /*h*/,
						data,
						datasize,
						writer_image_free,
						dev->writer
						))
				{
					fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to add image type=%s", type);
				}
			}
			else
			{
				/* We don't recognise this image type, so ignore. */
			}
		}
		else
		{
			/*
			 * Compressed data not available, so we could write out
			 * raw pixel values. But for now we ignore.
			 */
		}
	}
	fz_always(ctx)
	{
		dev->writer->ctx = NULL;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/*
 * Support for sending information to Extract when walking stroke/fill path
 * with fz_walk_path().
 */
typedef struct
{
	fz_path_walker walker;
	extract_t *extract;
} walker_info_t;

static void s_moveto(fz_context *ctx, void *arg, float x, float y)
{
	extract_t* extract = arg;
	if (extract_moveto(extract, x, y))
		fz_throw(ctx, FZ_ERROR_GENERIC, "extract_moveto() failed");
}

static void s_lineto(fz_context *ctx, void *arg, float x, float y)
{
	extract_t* extract = arg;
	if (extract_lineto(extract, x, y))
		fz_throw(ctx, FZ_ERROR_GENERIC, "extract_lineto() failed");
}

static void s_curveto(fz_context *ctx, void *arg, float x1, float y1,
		float x2, float y2, float x3, float y3)
{
	/* We simply move to the end point of the curve so that subsequent
	(straight) lines will be handled correctly. */
	extract_t* extract = arg;
	if (extract_moveto(extract, x3, y3))
		fz_throw(ctx, FZ_ERROR_GENERIC, "extract_moveto() failed");
}

static void s_closepath(fz_context *ctx, void *arg)
{
	extract_t* extract = arg;
	if (extract_closepath(extract))
		fz_throw(ctx, FZ_ERROR_GENERIC, "extract_closepath() failed");
}

/*
 * Calls extract_*() path functions on <path> using fz_walk_path() and the
 * above callbacks.
 */
static void s_walk_path(fz_context *ctx, fz_docx_device *dev, extract_t *extract, const fz_path *path)
{
	fz_path_walker walker;
	walker.moveto = s_moveto;
	walker.lineto = s_lineto;
	walker.curveto = s_curveto;
	walker.closepath = s_closepath;
	walker.quadto = NULL;
	walker.curvetov = NULL;
	walker.curvetoy = NULL;
	walker.rectto = NULL;

	assert(dev->writer->ctx == ctx);
	fz_walk_path(ctx, path, &walker, extract /*arg*/);
}

void dev_fill_path(fz_context *ctx, fz_device *dev_, const fz_path *path, int even_odd,
		fz_matrix matrix, fz_colorspace * colorspace, const float *color, float alpha,
		fz_color_params color_params)
{
	fz_docx_device *dev = (fz_docx_device*) dev_;
	extract_t *extract = dev->writer->extract;

	assert(!dev->writer->ctx);
	dev->writer->ctx = ctx;

	fz_try(ctx)
	{
		if (extract_fill_begin(
				extract,
				matrix.a,
				matrix.b,
				matrix.c,
				matrix.d,
				matrix.e,
				matrix.f,
				color[0]
				))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to begin fill");
		s_walk_path(ctx, dev, extract, path);
		if (extract_fill_end(extract))
			fz_throw(ctx, FZ_ERROR_GENERIC, "extract_fill_end() failed");
	}
	fz_always(ctx)
	{
		dev->writer->ctx = NULL;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}


static void
dev_stroke_path(fz_context *ctx, fz_device *dev_, const fz_path *path,
		const fz_stroke_state *stroke, fz_matrix in_ctm,
		fz_colorspace *colorspace_in, const float *color, float alpha,
		fz_color_params color_params)
{
	fz_docx_device *dev = (fz_docx_device*) dev_;
	extract_t *extract = dev->writer->extract;

	assert(!dev->writer->ctx);
	dev->writer->ctx = ctx;
	fz_try(ctx)
	{
		if (extract_stroke_begin(
				extract,
				in_ctm.a,
				in_ctm.b,
				in_ctm.c,
				in_ctm.d,
				in_ctm.e,
				in_ctm.f,
				stroke->linewidth,
				color[0]
				))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to begin stroke");
		s_walk_path(ctx, dev, extract, path);
		if (extract_stroke_end(extract))
			fz_throw(ctx, FZ_ERROR_GENERIC, "extract_stroke_end() failed");
	}
	fz_always(ctx)
	{
		dev->writer->ctx = NULL;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static extract_struct_t
fz_struct_to_extract(fz_structure type)
{
	switch (type)
	{
	default:
		return extract_struct_INVALID;

	case FZ_STRUCTURE_DOCUMENT:
		return extract_struct_DOCUMENT;
	case FZ_STRUCTURE_PART:
		return extract_struct_PART;
	case FZ_STRUCTURE_ART:
		return extract_struct_ART;
	case FZ_STRUCTURE_SECT:
		return extract_struct_SECT;
	case FZ_STRUCTURE_DIV:
		return extract_struct_DIV;
	case FZ_STRUCTURE_BLOCKQUOTE:
		return extract_struct_BLOCKQUOTE;
	case FZ_STRUCTURE_CAPTION:
		return extract_struct_CAPTION;
	case FZ_STRUCTURE_TOC:
		return extract_struct_TOC;
	case FZ_STRUCTURE_TOCI:
		return extract_struct_TOCI;
	case FZ_STRUCTURE_INDEX:
		return extract_struct_INDEX;
	case FZ_STRUCTURE_NONSTRUCT:
		return extract_struct_NONSTRUCT;
	case FZ_STRUCTURE_PRIVATE:
		return extract_struct_PRIVATE;
	/* Grouping elements (PDF 2.0 - Table 364) */
	case FZ_STRUCTURE_DOCUMENTFRAGMENT:
		return extract_struct_DOCUMENTFRAGMENT;
	/* Grouping elements (PDF 2.0 - Table 365) */
	case FZ_STRUCTURE_ASIDE:
		return extract_struct_ASIDE;
	/* Grouping elements (PDF 2.0 - Table 366) */
	case FZ_STRUCTURE_TITLE:
		return extract_struct_TITLE;
	case FZ_STRUCTURE_FENOTE:
		return extract_struct_FENOTE;
	/* Grouping elements (PDF 2.0 - Table 367) */
	case FZ_STRUCTURE_SUB:
		return extract_struct_SUB;

	/* Paragraphlike elements (PDF 1.7 - Table 10.21) */
	case FZ_STRUCTURE_P:
		return extract_struct_P;
	case FZ_STRUCTURE_H:
		return extract_struct_H;
	case FZ_STRUCTURE_H1:
		return extract_struct_H1;
	case FZ_STRUCTURE_H2:
		return extract_struct_H2;
	case FZ_STRUCTURE_H3:
		return extract_struct_H3;
	case FZ_STRUCTURE_H4:
		return extract_struct_H4;
	case FZ_STRUCTURE_H5:
		return extract_struct_H5;
	case FZ_STRUCTURE_H6:
		return extract_struct_H6;

	/* List elements (PDF 1.7 - Table 10.23) */
	case FZ_STRUCTURE_LIST:
		return extract_struct_LIST;
	case FZ_STRUCTURE_LISTITEM:
		return extract_struct_LISTITEM;
	case FZ_STRUCTURE_LABEL:
		return extract_struct_LABEL;
	case FZ_STRUCTURE_LISTBODY:
		return extract_struct_LISTBODY;

	/* Table elements (PDF 1.7 - Table 10.24) */
	case FZ_STRUCTURE_TABLE:
		return extract_struct_TABLE;
	case FZ_STRUCTURE_TR:
		return extract_struct_TR;
	case FZ_STRUCTURE_TH:
		return extract_struct_TH;
	case FZ_STRUCTURE_TD:
		return extract_struct_TD;
	case FZ_STRUCTURE_THEAD:
		return extract_struct_THEAD;
	case FZ_STRUCTURE_TBODY:
		return extract_struct_TBODY;
	case FZ_STRUCTURE_TFOOT:
		return extract_struct_TFOOT;

	/* Inline elements (PDF 1.7 - Table 10.25) */
	case FZ_STRUCTURE_SPAN:
		return extract_struct_SPAN;
	case FZ_STRUCTURE_QUOTE:
		return extract_struct_QUOTE;
	case FZ_STRUCTURE_NOTE:
		return extract_struct_NOTE;
	case FZ_STRUCTURE_REFERENCE:
		return extract_struct_REFERENCE;
	case FZ_STRUCTURE_BIBENTRY:
		return extract_struct_BIBENTRY;
	case FZ_STRUCTURE_CODE:
		return extract_struct_CODE;
	case FZ_STRUCTURE_LINK:
		return extract_struct_LINK;
	case FZ_STRUCTURE_ANNOT:
		return extract_struct_ANNOT;
	/* Inline elements (PDF 2.0 - Table 368) */
	case FZ_STRUCTURE_EM:
		return extract_struct_EM;
	case FZ_STRUCTURE_STRONG:
		return extract_struct_STRONG;

	/* Ruby inline element (PDF 1.7 - Table 10.26) */
	case FZ_STRUCTURE_RUBY:
		return extract_struct_RUBY;
	case FZ_STRUCTURE_RB:
		return extract_struct_RB;
	case FZ_STRUCTURE_RT:
		return extract_struct_RT;
	case FZ_STRUCTURE_RP:
		return extract_struct_RP;

	/* Warichu inline element (PDF 1.7 - Table 10.26) */
	case FZ_STRUCTURE_WARICHU:
		return extract_struct_WARICHU;
	case FZ_STRUCTURE_WT:
		return extract_struct_WT;
	case FZ_STRUCTURE_WP:
		return extract_struct_WP;

	/* Illustration elements (PDF 1.7 - Table 10.27) */
	case FZ_STRUCTURE_FIGURE:
		return extract_struct_FIGURE;
	case FZ_STRUCTURE_FORMULA:
		return extract_struct_FORMULA;
	case FZ_STRUCTURE_FORM:
		return extract_struct_FORM;

	/* Artifact structure type (PDF 2.0 - Table 375) */
	case FZ_STRUCTURE_ARTIFACT:
		return extract_struct_ARTIFACT;
	}
}

static void
dev_begin_structure(fz_context *ctx, fz_device *dev_, fz_structure standard, const char *raw, int uid)
{
	fz_docx_device *dev = (fz_docx_device *)dev_;
	extract_t *extract = dev->writer->extract;

	assert(!dev->writer->ctx);
	dev->writer->ctx = ctx;
	fz_try(ctx)
	{
		if (extract_begin_struct(extract, fz_struct_to_extract(standard), uid, -1))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to begin struct");
	}
	fz_always(ctx)
		dev->writer->ctx = NULL;
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
dev_end_structure(fz_context *ctx, fz_device *dev_)
{
	fz_docx_device *dev = (fz_docx_device *)dev_;
	extract_t *extract = dev->writer->extract;

	assert(!dev->writer->ctx);
	dev->writer->ctx = ctx;
	fz_try(ctx)
	{
		if (extract_end_struct(extract))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to end struct");
	}
	fz_always(ctx)
		dev->writer->ctx = NULL;
	fz_catch(ctx)
		fz_rethrow(ctx);
}


static fz_device *writer_begin_page(fz_context *ctx, fz_document_writer *writer_, fz_rect mediabox)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_docx_device *dev;
	assert(!writer->ctx);
	writer->ctx = ctx;
	writer->mediabox = mediabox;
	fz_var(dev);
	fz_try(ctx)
	{
		if (extract_page_begin(writer->extract, mediabox.x0, mediabox.y0, mediabox.x1, mediabox.y1))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to begin page");
		dev = fz_new_derived_device(ctx, fz_docx_device);
		dev->super.fill_text = dev_fill_text;
		dev->super.stroke_text = dev_stroke_text;
		dev->super.clip_text = dev_clip_text;
		dev->super.clip_stroke_text = dev_clip_stroke_text;
		dev->super.ignore_text = dev_ignore_text;
		dev->super.fill_image = dev_fill_image;
		dev->super.fill_path = dev_fill_path;
		dev->super.stroke_path = dev_stroke_path;
		dev->super.begin_structure = dev_begin_structure;
		dev->super.end_structure = dev_end_structure;
		dev->writer = writer;
	}
	fz_always(ctx)
	{
		writer->ctx = NULL;
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
	return &dev->super;
}

static void writer_end_page(fz_context *ctx, fz_document_writer *writer_, fz_device *dev)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	assert(!writer->ctx);
	writer->ctx = ctx;
	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		if (extract_page_end(writer->extract))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to end page");

		if (extract_process(writer->extract, writer->spacing, writer->rotation, writer->images))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to process page");
	}
	fz_always(ctx)
	{
		writer->ctx = NULL;
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static int buffer_write(void *handle, const void *source, size_t numbytes, size_t *o_actual)
/*
 * extract_buffer_t callback that calls fz_write_data(). <source> will be docx
 * archive data.
 */
{
	int e = 0;
	fz_docx_writer *writer = handle;
	fz_var(e);
	fz_try(writer->ctx)
	{
		fz_write_data(writer->ctx, writer->output, source, numbytes);
		*o_actual = numbytes;
	}
	fz_catch(writer->ctx)
	{
		errno = EIO;
		e = -1;
	}
	return e;
}

static int buffer_cache(void *handle, void **o_cache, size_t *o_numbytes)
/*
 * extract_buffer_t cache function. We simply return writer->output_cache.
 */
{
	fz_docx_writer *writer = handle;
	*o_cache = writer->output_cache;
	*o_numbytes = sizeof(writer->output_cache);
	return 0;
}

static void writer_close(fz_context *ctx, fz_document_writer *writer_)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	extract_buffer_t *extract_buffer_output = NULL;

	fz_var(extract_buffer_output);
	fz_var(writer);
	assert(!writer->ctx);
	writer->ctx = ctx;
	fz_try(ctx)
	{
		/*
		 * Write docx to writer->output. Need to create an
		 * extract_buffer_t that writes to writer->output, for use by
		 * extract_write().
		 */
		if (extract_buffer_open(
				writer->alloc,
				writer,
				NULL /*fn_read*/,
				buffer_write,
				buffer_cache,
				NULL /*fn_close*/,
				&extract_buffer_output
				))
		{
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create extract_buffer_output: %s", strerror(errno));
		}
		if (extract_write(writer->extract, extract_buffer_output))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to generate docx content: %s", strerror(errno));
		if (extract_buffer_close(&extract_buffer_output))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to close extract_buffer: %s", strerror(errno));

		extract_end(&writer->extract);
		fz_close_output(ctx, writer->output);
		writer->ctx = NULL;
	}
	fz_catch(ctx)
	{
		/*
		 * We don't call fz_close_output() because it can throw and in
		 * this error case we can safely leave cleanup to our s_drop()
		 * function's calls to fz_drop_output().
		 */
		extract_buffer_close(&extract_buffer_output);
		extract_end(&writer->extract);
		writer->ctx = NULL;
		fz_rethrow(ctx);
	}
}

static void writer_drop(fz_context *ctx, fz_document_writer *writer_)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_drop_output(ctx, writer->output);
	writer->output = NULL;
	assert(!writer->ctx);
	writer->ctx = ctx;
	extract_end(&writer->extract);
	extract_alloc_destroy(&writer->alloc);
	writer->ctx = NULL;
}


static int get_bool_option(fz_context *ctx, const char *options, const char *name, int default_)
{
	const char *value;
	if (fz_has_option(ctx, options, name, &value))
	{
		if (fz_option_eq(value, "yes")) return 1;
		if (fz_option_eq(value, "no")) return 0;
		else fz_throw(ctx, FZ_ERROR_SYNTAX, "option '%s' should be yes or no in options='%s'", name, options);
	}
	else
		return default_;
}

static void *s_realloc_fn(void *state, void *prev, size_t size)
{
	fz_docx_writer *writer = state;
	assert(writer);
	assert(writer->ctx);
	return fz_realloc_no_throw(writer->ctx, prev, size);
}

/* Will drop <out> if an error occurs. */
static fz_document_writer *fz_new_docx_writer_internal(fz_context *ctx, fz_output *out,
		const char *options, extract_format_t format)
{
	fz_docx_writer *writer = NULL;

	fz_var(writer);

	fz_try(ctx)
	{
		writer = fz_new_derived_document_writer(
				ctx,
				fz_docx_writer,
				writer_begin_page,
				writer_end_page,
				writer_close,
				writer_drop
				);
		writer->ctx = ctx;
		writer->output = out;
		if (get_bool_option(ctx, options, "html", 0)) format = extract_format_HTML;
		if (get_bool_option(ctx, options, "text", 0)) format = extract_format_TEXT;
		if (get_bool_option(ctx, options, "json", 0)) format = extract_format_JSON;
		if (extract_alloc_create(s_realloc_fn, writer, &writer->alloc))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create extract_alloc instance");
		if (extract_begin(writer->alloc, format, &writer->extract))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create extract instance");
		writer->spacing = get_bool_option(ctx, options, "spacing", 0);
		writer->rotation = get_bool_option(ctx, options, "rotation", 1);
		writer->images = get_bool_option(ctx, options, "images", 1);
		writer->mediabox_clip = get_bool_option(ctx, options, "mediabox-clip", 1);
		if (extract_set_layout_analysis(writer->extract, get_bool_option(ctx, options, "analyse", 0)))
			fz_throw(ctx, FZ_ERROR_GENERIC, "extract_enable_analysis failed.");
		{
			const char* v;
			if (fz_has_option(ctx, options, "tables-csv-format", &v))
			{
				size_t len = strlen(v) + 1; /* Might include trailing options. */
				char* formatbuf = fz_malloc(ctx, len);
				fz_copy_option(ctx, v, formatbuf, len);
				fprintf(stderr, "tables-csv-format: %s\n", formatbuf);
				if (extract_tables_csv_format(writer->extract, formatbuf))
				{
					fz_free(ctx, formatbuf);
					fz_throw(ctx, FZ_ERROR_GENERIC, "extract_tables_csv_format() failed.");
				}
				fz_free(ctx, formatbuf);
			}
		}
		writer->ctx = NULL;
	}
	fz_catch(ctx)
	{
		/* fz_drop_document_writer() drops its output so we only need to call
		fz_drop_output() if we failed before creating the writer. */
		if (writer)
		{
			writer->ctx = ctx;
			fz_drop_document_writer(ctx, &writer->super);
			writer->ctx = NULL;
		}
		else
			fz_drop_output(ctx, out);
		fz_rethrow(ctx);
	}
	return &writer->super;
}

fz_document_writer *fz_new_docx_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	return fz_new_docx_writer_internal(ctx, out, options, extract_format_DOCX);
}

fz_document_writer *fz_new_docx_writer(fz_context *ctx, const char *path, const char *options)
{
	/* No need to drop <out> if fz_new_docx_writer_internal() throws, because
	it always drops <out> if it fails. */
	fz_output *out = fz_new_output_with_path(ctx, path, 0 /*append*/);
	return fz_new_docx_writer_internal(ctx, out, options, extract_format_DOCX);
}

#if FZ_ENABLE_ODT_OUTPUT

fz_document_writer *fz_new_odt_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	return fz_new_docx_writer_internal(ctx, out, options, extract_format_ODT);
}

fz_document_writer *fz_new_odt_writer(fz_context *ctx, const char *path, const char *options)
{
	/* No need to drop <out> if fz_new_docx_writer_internal() throws, because
	it always drops <out> if it fails. */
	fz_output *out = fz_new_output_with_path(ctx, path, 0 /*append*/);
	return fz_new_docx_writer_internal(ctx, out, options, extract_format_ODT);
}

#else

fz_document_writer *fz_new_odt_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "ODT writer not enabled");
	return NULL;
}

fz_document_writer *fz_new_odt_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "ODT writer not enabled");
	return NULL;
}

#endif

#else

fz_document_writer *fz_new_odt_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "DOCX/ODT writer not enabled");
	return NULL;
}

fz_document_writer *fz_new_odt_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "DOCX/ODT writer not enabled");
	return NULL;
}

fz_document_writer *fz_new_docx_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "DOCX writer not enabled");
	return NULL;
}

fz_document_writer *fz_new_docx_writer(fz_context *ctx, const char *path, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "DOCX writer not enabled");
	return NULL;
}

#endif
