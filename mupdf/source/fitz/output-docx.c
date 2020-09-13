#ifdef HAVE_EXTRACT

#include "extract.h"

#include "mupdf/fitz.h"

#include <errno.h>
#include <string.h>


typedef struct
{
	fz_document_writer super;
	fz_output *intermediate;
	fz_buffer *buffer;
	char *path;
} fz_docx_writer;

static fz_device *s_begin_page(fz_context *ctx, fz_document_writer *writer_, fz_rect mediabox)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_write_string(ctx, writer->intermediate, "\n<page>\n");
	return fz_new_xmltext_device(ctx, writer->intermediate);
}

static void s_end_page(fz_context *ctx, fz_document_writer *writer_, fz_device *dev)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_write_string(ctx, writer->intermediate, "\n</page>\n");
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

static void s_close(fz_context *ctx, fz_document_writer *writer_)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	extract_document_t *extract_document = NULL;
	char *extract_content = NULL;
	int extract_content_length;
	extract_buffer_t *extract_buffer = NULL;

	fz_var(extract_document);
	fz_var(extract_content);
	fz_var(extract_buffer);
	fz_var(writer);

	fz_try(ctx)
	{
		int e;
		unsigned char *data;
		int data_size;

		fz_close_output(ctx, writer->intermediate);

		/* Intermediate data from xmltext device is in writer->buffer.

		We need to create a extract_buffer_t that reads
		this intermediate data, in order to call
		extract_intermediate_to_document_buffer(). */

		data_size = fz_buffer_storage(ctx, writer->buffer, &data);
		e = extract_buffer_open_simple(data, data_size, &extract_buffer);
		if (e) fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create extract_buffer.");

		e = extract_intermediate_to_document_buffer(extract_buffer, 0 /*autosplit*/, &extract_document);
		if (e) fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to extract intermediate data.");
		extract_buffer_close(extract_buffer);
		extract_buffer = NULL;

		e = extract_document_join(extract_document);
		if (e) fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to join spans: %s", strerror(errno));

		e = extract_document_to_docx_content(extract_document, 1 /*spacing*/, &extract_content, &extract_content_length);
		if (e) fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to generate docx content: %s", strerror(errno));

		e = extract_docx_content_to_docx(extract_content, extract_content_length, writer->path);
		if (e) fz_throw(ctx, FZ_ERROR_GENERIC, "Failed to create .docx file (%s): %s", strerror(errno), writer->path);
	}
	fz_always(ctx)
	{
		free(extract_content);
		extract_document_free(extract_document);
		extract_buffer_close(extract_buffer);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void s_drop(fz_context *ctx, fz_document_writer *writer_)
{
	fz_docx_writer *writer = (fz_docx_writer*) writer_;
	fz_free(ctx, writer->path);
	fz_drop_output(ctx, writer->intermediate);
	fz_drop_buffer(ctx, writer->buffer);
}

fz_document_writer *fz_new_docx_writer(fz_context *ctx, const char *format, const char *path, const char *options)
{
	fz_docx_writer *writer = NULL;

	fz_try(ctx)
	{
		writer = fz_new_derived_document_writer(ctx, fz_docx_writer, s_begin_page, s_end_page, s_close, s_drop);
		writer->intermediate = NULL;
		writer->path = NULL;
		writer->buffer = NULL;
		writer->path = fz_strdup(ctx, path);
		writer->buffer = fz_new_buffer(ctx, 0 /*capacity*/);
		writer->intermediate = fz_new_output_with_buffer(ctx, writer->buffer);
	}
	fz_catch(ctx)
	{
		fz_drop_document_writer(ctx, &writer->super);
		fz_rethrow(ctx);
	}
	return &writer->super;
}

#else

#include "mupdf/fitz.h"

fz_document_writer *fz_new_docx_writer(fz_context *ctx, const char *format, const char *path, const char *options)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "fz_new_docx_writer() not available in this build.");
}

#endif
