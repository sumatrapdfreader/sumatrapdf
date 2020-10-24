#ifdef HAVE_EXTRACT

#include "extract.h"

#include "mupdf/fitz.h"

#include <errno.h>
#include <string.h>


typedef struct
{
	fz_document_writer super;
	fz_context *ctx;
	fz_buffer *intermediate_buffer;
	fz_output *intermediate_output;
	fz_output *output;
	int we_own_output;
	char output_cache[1024];
} fz_docx_writer;

static fz_device *s_begin_page(fz_context *ctx, fz_document_writer *writer_, fz_rect mediabox)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_write_string(ctx, writer->intermediate_output, "\n<page>\n");
	return fz_new_xmltext_device(ctx, writer->intermediate_output);
}

static void s_end_page(fz_context *ctx, fz_document_writer *writer_, fz_device *dev)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_write_string(ctx, writer->intermediate_output, "\n</page>\n");
	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static int s_buffer_to_output_write(void *handle, const void *source, size_t numbytes, size_t *o_actual)
/* Callback for extract_buffer_t. <source> will be zip/docx content. */
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

static int s_buffer_to_output_cache(void *handle, void **o_cache, size_t *o_numbytes)
{
	fz_docx_writer *writer = handle;
	*o_cache = writer->output_cache;
	*o_numbytes = sizeof(writer->output_cache);
	return 0;
}

static void s_close(fz_context *ctx, fz_document_writer *writer_)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	extract_document_t *extract_document = NULL;
	char *extract_content = NULL;
	size_t extract_content_length;
	extract_buffer_t *extract_buffer_intermediate = NULL;
	extract_buffer_t *extract_buffer_output = NULL;

	fz_var(extract_document);
	fz_var(extract_content);
	fz_var(extract_buffer_intermediate);
	fz_var(extract_buffer_output);
	fz_var(writer);

	fz_try(ctx)
	{
		unsigned char *data;
		size_t data_size;
		writer->ctx = ctx;	/* For s_buffer_to_output_write() callback. */

		/*
		 * Load intermediate data. Intermediate data from xmltext device is
		 * (via * writer->intermediate_output) in writer->intermediate_buffer. We
		 * need to create * an extract_buffer_t that reads this intermediate
		 * data, in order to call * extract_intermediate_to_document_buffer().
		 */
		fz_close_output(ctx, writer->intermediate_output);
		data_size = fz_buffer_storage(ctx, writer->intermediate_buffer, &data);
		if (extract_buffer_open_simple(data, data_size, NULL /*handle*/, NULL /*fn_close*/,
				&extract_buffer_intermediate))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create extract_buffer_intermediate.");
		if (extract_intermediate_to_document(extract_buffer_intermediate, 0 /*autosplit*/, &extract_document))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to extract intermediate data.");
		if (extract_buffer_close(&extract_buffer_intermediate))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to close extract_buffer_intermediate.");

		/* Join spans into lines and paragraphs. */
		if (extract_document_join(extract_document))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to join spans: %s", strerror(errno));

		/* Convert paragraphs into docx content. */
		if (extract_document_to_docx_content(extract_document, 1 /*spacing*/, 1 /*rotation*/, 1 /*images*/, &extract_content,
				&extract_content_length))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to generate docx content: %s", strerror(errno));

		/*
		 * Write docx to writer->output. Need to create an
		 * extract_buffer_t that * writes to writer->output, for use by
		 * extract_docx_content_to_docx().
		 */
		if (extract_buffer_open(writer, NULL /*fn_read*/, s_buffer_to_output_write,
				s_buffer_to_output_cache, NULL /*fn_close*/, &extract_buffer_output))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create extract_buffer_output: %s", strerror(errno));
		if (extract_docx_content_to_docx(extract_content, extract_content_length, extract_document, extract_buffer_output))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create .docx file: %s", strerror(errno));
		if (extract_buffer_close(&extract_buffer_output))
			fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to close extract_buffer: %s", strerror(errno));
	}
	fz_always(ctx)
	{
		writer->ctx = NULL;
		fz_close_output(ctx, writer->intermediate_output);
		extract_buffer_close(&extract_buffer_intermediate);
		free(extract_content);
		extract_document_free(&extract_document);
		extract_buffer_close(&extract_buffer_output);
		fz_close_output(ctx, writer->output);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void s_drop(fz_context *ctx, fz_document_writer *writer_)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_drop_output(ctx, writer->intermediate_output);
	fz_drop_buffer(ctx, writer->intermediate_buffer);
	if (writer->we_own_output)
	{
		fz_drop_output(ctx, writer->output);
		writer->output = NULL;
	}
}

static fz_document_writer *fz_new_docx_writer_internal(fz_context *ctx, fz_output *out, const char *options, int we_own_output)
{
	fz_docx_writer *writer = NULL;
	fz_var(writer);
	fz_try(ctx)
	{
		writer = fz_new_derived_document_writer(ctx, fz_docx_writer, s_begin_page, s_end_page, s_close, s_drop);
		writer->intermediate_output = NULL;
		writer->intermediate_buffer = NULL;
		writer->intermediate_buffer = fz_new_buffer(ctx, 0 /*capacity*/);
		writer->intermediate_output = fz_new_output_with_buffer(ctx, writer->intermediate_buffer);
		writer->output = out;
		writer->we_own_output = we_own_output;
	}
	fz_catch(ctx)
	{
		fz_drop_document_writer(ctx, &writer->super);
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
