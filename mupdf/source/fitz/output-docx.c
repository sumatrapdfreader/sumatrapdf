#ifdef HAVE_EXTRACT

#include "extract.h"

#include "mupdf/fitz.h"

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
	int we_own_output;
	int spacing;
	int rotation;
	int images;
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

			if (extract_span_begin(
					dev->writer->extract,
					span->font->name,
					span->font->flags.is_bold,
					span->font->flags.is_italic,
					span->wmode,
					ctm.a,
					ctm.b,
					ctm.c,
					ctm.d,
					ctm.e,
					ctm.f,
					span->trm.a,
					span->trm.b,
					span->trm.c,
					span->trm.d,
					span->trm.e,
					span->trm.f
					))
			{
				fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to begin span");
			}

			for (i=0; i<span->len; ++i)
			{
				fz_text_item *item = &span->items[i];
				float adv = 0;

				if (span->items[i].gid >= 0)
					adv = fz_advance_glyph(ctx, span->font, span->items[i].gid, span->wmode);

				if (extract_add_char(dev->writer->extract, item->x, item->y, item->ucs, adv, 0 /*autosplit*/))
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
			if (0) {}
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
						0 /*x*/,
						0 /*y*/,
						0 /*w*/,
						0 /*h*/,
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
			 * Compressed data not available, so we could write out raw pixel
			 values. But for * now we ignore.
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


static fz_device *writer_begin_page(fz_context *ctx, fz_document_writer *writer_, fz_rect mediabox)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_docx_device *dev;
	assert(!writer->ctx);
	writer->ctx = ctx;
	fz_var(dev);
	fz_try(ctx)
	{
		if (extract_page_begin(writer->extract))
		{
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to begin page");
		}
		dev = fz_new_derived_device(ctx, fz_docx_device);
		dev->super.fill_text = dev_fill_text;
		dev->super.stroke_text = dev_stroke_text;
		dev->super.clip_text = dev_clip_text;
		dev->super.clip_stroke_text = dev_clip_stroke_text;
		dev->super.ignore_text = dev_ignore_text;
		dev->super.fill_image = dev_fill_image;
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

		extract_alloc_destroy(&writer->alloc);
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
		extract_alloc_destroy(&writer->alloc);
		fz_rethrow(ctx);
	}
}

static void writer_drop(fz_context *ctx, fz_document_writer *writer_)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	if (writer->we_own_output)
	{
		fz_drop_output(ctx, writer->output);
		writer->output = NULL;
	}
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

static fz_document_writer *fz_new_docx_writer_internal(fz_context *ctx, fz_output *out, const char *options, int we_own_output)
{
	fz_docx_writer *writer = fz_new_derived_document_writer(
			ctx,
			fz_docx_writer,
			writer_begin_page,
			writer_end_page,
			writer_close,
			writer_drop
			);
	fz_try(ctx)
	{
		writer->ctx = ctx;
		if (extract_alloc_create(s_realloc_fn, writer, &writer->alloc))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create extract_alloc instance");
		if (extract_begin(writer->alloc, &writer->extract))
		{
			extract_alloc_destroy(&writer->alloc);
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create extract instance");
		}
		writer->output = out;
		writer->we_own_output = we_own_output;
		writer->spacing = get_bool_option(ctx, options, "spacing", 1);
		writer->rotation = get_bool_option(ctx, options, "rotation", 1);
		writer->images = get_bool_option(ctx, options, "images", 1);
		writer->ctx = NULL;
	}
	fz_catch(ctx)
	{
		fz_drop_document_writer(ctx, &writer->super);
		extract_alloc_destroy(&writer->alloc);
		fz_rethrow(ctx);
	}
	return &writer->super;
}

fz_document_writer *fz_new_docx_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	return fz_new_docx_writer_internal(ctx, out, options, 0 /*we_own_output*/);
}

fz_document_writer *fz_new_docx_writer(fz_context *ctx, const char *format, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path, 0 /*append*/);
	return fz_new_docx_writer_internal(ctx, out, options, 1 /*we_own_output*/);
}

#else

#include "mupdf/fitz.h"

fz_document_writer *fz_new_docx_writer(fz_context *ctx, const char *format, const char *path, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "docx output not available in this build.");
}

fz_document_writer *fz_new_docx_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "docx output not available in this build.");
}

#endif
