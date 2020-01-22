#define _LARGEFILE_SOURCE
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

static void
file_write(fz_context *ctx, void *opaque, const void *buffer, size_t count)
{
	FILE *file = opaque;
	size_t n;

	if (count == 0)
		return;

	if (count == 1)
	{
		int x = putc(((unsigned char*)buffer)[0], file);
		if (x == EOF && ferror(file))
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot fwrite: %s", strerror(errno));
		return;
	}

	n = fwrite(buffer, 1, count, file);
	if (n < count && ferror(file))
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot fwrite: %s", strerror(errno));
}

static void
stdout_write(fz_context *ctx, void *opaque, const void *buffer, size_t count)
{
	file_write(ctx, stdout, buffer, count);
}

static fz_output fz_stdout_global = {
	NULL,
	stdout_write,
	NULL,
	NULL,
	NULL,
};

fz_output *fz_stdout(fz_context *ctx)
{
	return &fz_stdout_global;
}

static void
stderr_write(fz_context *ctx, void *opaque, const void *buffer, size_t count)
{
	file_write(ctx, stderr, buffer, count);
}

static fz_output fz_stderr_global = {
	NULL,
	stderr_write,
	NULL,
	NULL,
	NULL,
};

fz_output *fz_stderr(fz_context *ctx)
{
	return &fz_stderr_global;
}

static void
file_seek(fz_context *ctx, void *opaque, int64_t off, int whence)
{
	FILE *file = opaque;
#ifdef _WIN32
	int n = _fseeki64(file, off, whence);
#else
	int n = fseeko(file, off, whence);
#endif
	if (n < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot fseek: %s", strerror(errno));
}

static int64_t
file_tell(fz_context *ctx, void *opaque)
{
	FILE *file = opaque;
#ifdef _WIN32
	int64_t off = _ftelli64(file);
#else
	int64_t off = ftello(file);
#endif
	if (off == -1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot ftell: %s", strerror(errno));
	return off;
}

static void
file_drop(fz_context *ctx, void *opaque)
{
	FILE *file = opaque;
	int n = fclose(file);
	if (n < 0)
		fz_warn(ctx, "cannot fclose: %s", strerror(errno));
}

static fz_stream *
file_as_stream(fz_context *ctx, void *opaque)
{
	FILE *file = opaque;
	fflush(file);
	return fz_open_file_ptr_no_close(ctx, file);
}

static void file_truncate(fz_context *ctx, void *opaque)
{
	FILE *file = opaque;
	fflush(file);

#ifdef _WIN32
	_chsize_s(fileno(file), ftell(file));
#else
	ftruncate(fileno(file), ftell(file));
#endif
}

/*
	Create a new output object with the given
	internal state and function pointers.

	state: Internal state (opaque to everything but implementation).

	write: Function to output a given buffer.

	close: Cleanup function to destroy state when output closed.
	May permissibly be null.
*/
fz_output *
fz_new_output(fz_context *ctx, int bufsiz, void *state, fz_output_write_fn *write, fz_output_close_fn *close, fz_output_drop_fn *drop)
{
	fz_output *out = NULL;

	fz_var(out);

	fz_try(ctx)
	{
		out = fz_malloc_struct(ctx, fz_output);
		out->state = state;
		out->write = write;
		out->close = close;
		out->drop = drop;
		if (bufsiz > 0)
		{
			out->bp = Memento_label(fz_malloc(ctx, bufsiz), "output_buf");
			out->wp = out->bp;
			out->ep = out->bp + bufsiz;
		}
	}
	fz_catch(ctx)
	{
		if (drop)
			drop(ctx, state);
		fz_free(ctx, out);
		fz_rethrow(ctx);
	}
	return out;
}

static void null_write(fz_context *ctx, void *opaque, const void *buffer, size_t count)
{
}

/*
	Open an output stream that writes to a
	given path.

	filename: The filename to write to (specified in UTF-8).

	append: non-zero if we should append to the file, rather than
	overwriting it.
*/
fz_output *
fz_new_output_with_path(fz_context *ctx, const char *filename, int append)
{
	FILE *file;
	fz_output *out;

	if (!strcmp(filename, "/dev/null") || !fz_strcasecmp(filename, "nul:"))
		return fz_new_output(ctx, 0, NULL, null_write, NULL, NULL);

#ifdef _WIN32
	/* Ensure we create a brand new file. We don't want to clobber our old file. */
	if (!append)
	{
		if (fz_remove_utf8(filename) < 0)
			if (errno != ENOENT)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot remove file '%s': %s", filename, strerror(errno));
	}
	file = fz_fopen_utf8(filename, append ? "rb+" : "wb+");
	if (file == NULL && append)
		file = fz_fopen_utf8(filename, "wb+");
#else
	/* Ensure we create a brand new file. We don't want to clobber our old file. */
	if (!append)
	{
		if (remove(filename) < 0)
			if (errno != ENOENT)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot remove file '%s': %s", filename, strerror(errno));
	}
	file = fopen(filename, append ? "rb+" : "wb+");
	if (file == NULL && append)
		file = fopen(filename, "wb+");
#endif
	if (!file)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file '%s': %s", filename, strerror(errno));

	setvbuf(file, NULL, _IONBF, 0); /* we do our own buffering */
	out = fz_new_output(ctx, 8192, file, file_write, NULL, file_drop);
	out->seek = file_seek;
	out->tell = file_tell;
	out->as_stream = file_as_stream;
	out->truncate = file_truncate;

	return out;
}

static void
buffer_write(fz_context *ctx, void *opaque, const void *data, size_t len)
{
	fz_buffer *buffer = opaque;
	fz_append_data(ctx, buffer, data, len);
}

static void
buffer_seek(fz_context *ctx, void *opaque, int64_t off, int whence)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "cannot seek in buffer: %s", strerror(errno));
}

static int64_t
buffer_tell(fz_context *ctx, void *opaque)
{
	fz_buffer *buffer = opaque;
	return (int64_t)buffer->len;
}

static void
buffer_drop(fz_context *ctx, void *opaque)
{
	fz_buffer *buffer = opaque;
	fz_drop_buffer(ctx, buffer);
}

/*
	Open an output stream that appends
	to a buffer.

	buf: The buffer to append to.
*/
fz_output *
fz_new_output_with_buffer(fz_context *ctx, fz_buffer *buf)
{
	fz_output *out = fz_new_output(ctx, 0, fz_keep_buffer(ctx, buf), buffer_write, NULL, buffer_drop);
	out->seek = buffer_seek;
	out->tell = buffer_tell;
	return out;
}

/*
	Flush pending output and close an output stream.
*/
void
fz_close_output(fz_context *ctx, fz_output *out)
{
	if (out == NULL)
		return;
	fz_flush_output(ctx, out);
	if (out->close)
		out->close(ctx, out->state);
	out->close = NULL;
}

/*
	Free an output stream. Don't forget to close it first!
*/
void
fz_drop_output(fz_context *ctx, fz_output *out)
{
	if (out)
	{
		if (out->close)
			fz_warn(ctx, "dropping unclosed output");
		if (out->drop)
			out->drop(ctx, out->state);
		fz_free(ctx, out->bp);
		if (out != &fz_stdout_global)
			fz_free(ctx, out);
	}
}

/*
	Seek to the specified file position.
	See fseek for arguments.

	Throw an error on unseekable outputs.
*/
void
fz_seek_output(fz_context *ctx, fz_output *out, int64_t off, int whence)
{
	if (out->seek == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot seek in unseekable output stream\n");
	fz_flush_output(ctx, out);
	out->seek(ctx, out->state, off, whence);
}

/*
	Return the current file position.

	Throw an error on untellable outputs.
*/
int64_t
fz_tell_output(fz_context *ctx, fz_output *out)
{
	if (out->tell == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot tell in untellable output stream\n");
	if (out->bp)
		return out->tell(ctx, out->state) + (out->wp - out->bp);
	return out->tell(ctx, out->state);
}

/*
	obtain the fz_output in the form of a fz_stream

	This allows data to be read back from some forms of fz_output object.
	When finished reading, the fz_stream should be released by calling
	fz_drop_stream. Until the fz_stream is dropped, no further operations
	should be performed on the fz_output object.
*/
fz_stream *
fz_stream_from_output(fz_context *ctx, fz_output *out)
{
	if (out->as_stream == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot derive input stream from output stream");
	fz_flush_output(ctx, out);
	return out->as_stream(ctx, out->state);
}

void
fz_truncate_output(fz_context *ctx, fz_output *out)
{
	if (out->truncate == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot truncate this output stream");
	fz_flush_output(ctx, out);
	out->truncate(ctx, out->state);
}

static void
fz_write_emit(fz_context *ctx, void *out, int c)
{
	fz_write_byte(ctx, out, c);
}

/*
	va_list version of fz_write_printf.
*/
void
fz_write_vprintf(fz_context *ctx, fz_output *out, const char *fmt, va_list args)
{
	fz_format_string(ctx, out, fz_write_emit, fmt, args);
}

/*
	Format and write data to an output stream.
	See fz_format_string for formatting details.
*/
void
fz_write_printf(fz_context *ctx, fz_output *out, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fz_format_string(ctx, out, fz_write_emit, fmt, args);
	va_end(args);
}

/*
	Flush unwritten data.
*/
void
fz_flush_output(fz_context *ctx, fz_output *out)
{
	if (out->wp > out->bp)
	{
		out->write(ctx, out->state, out->bp, out->wp - out->bp);
		out->wp = out->bp;
	}
}

void
fz_write_byte(fz_context *ctx, fz_output *out, unsigned char x)
{
	if (out->bp)
	{
		if (out->wp == out->ep)
		{
			out->write(ctx, out->state, out->bp, out->wp - out->bp);
			out->wp = out->bp;
		}
		*out->wp++ = x;
	}
	else
	{
		out->write(ctx, out->state, &x, 1);
	}
}

void
fz_write_char(fz_context *ctx, fz_output *out, char x)
{
	fz_write_byte(ctx, out, (unsigned char)x);
}

/*
	Write data to output.

	data: Pointer to data to write.
	size: Size of data to write in bytes.
*/
void
fz_write_data(fz_context *ctx, fz_output *out, const void *data_, size_t size)
{
	const char *data = data_;

	if (out->bp)
	{
		if (size >= (size_t) (out->ep - out->bp)) /* too large for buffer */
		{
			if (out->wp > out->bp)
			{
				out->write(ctx, out->state, out->bp, out->wp - out->bp);
				out->wp = out->bp;
			}
			out->write(ctx, out->state, data, size);
		}
		else if (out->wp + size <= out->ep) /* fits in current buffer */
		{
			memcpy(out->wp, data, size);
			out->wp += size;
		}
		else /* fits if we flush first */
		{
			size_t n = out->ep - out->wp;
			memcpy(out->wp, data, n);
			out->write(ctx, out->state, out->bp, out->ep - out->bp);
			memcpy(out->bp, data + n, size - n);
			out->wp = out->bp + size - n;
		}
	}
	else
	{
		out->write(ctx, out->state, data, size);
	}
}

/*
	Write a string. Does not write zero terminator.
*/
void
fz_write_string(fz_context *ctx, fz_output *out, const char *s)
{
	fz_write_data(ctx, out, s, strlen(s));
}

void
fz_write_int32_be(fz_context *ctx, fz_output *out, int x)
{
	char data[4];

	data[0] = x>>24;
	data[1] = x>>16;
	data[2] = x>>8;
	data[3] = x;

	fz_write_data(ctx, out, data, 4);
}

void
fz_write_uint32_be(fz_context *ctx, fz_output *out, unsigned int x)
{
	fz_write_int32_be(ctx, out, (unsigned int)x);
}

void
fz_write_int32_le(fz_context *ctx, fz_output *out, int x)
{
	char data[4];

	data[0] = x;
	data[1] = x>>8;
	data[2] = x>>16;
	data[3] = x>>24;

	fz_write_data(ctx, out, data, 4);
}

void
fz_write_uint32_le(fz_context *ctx, fz_output *out, unsigned int x)
{
	fz_write_int32_le(ctx, out, (int)x);
}

void
fz_write_int16_be(fz_context *ctx, fz_output *out, int x)
{
	char data[2];

	data[0] = x>>8;
	data[1] = x;

	fz_write_data(ctx, out, data, 2);
}

void
fz_write_uint16_be(fz_context *ctx, fz_output *out, unsigned int x)
{
	fz_write_int16_be(ctx, out, (int)x);
}

void
fz_write_int16_le(fz_context *ctx, fz_output *out, int x)
{
	char data[2];

	data[0] = x;
	data[1] = x>>8;

	fz_write_data(ctx, out, data, 2);
}

void
fz_write_uint16_le(fz_context *ctx, fz_output *out, unsigned int x)
{
	fz_write_int16_le(ctx, out, (int)x);
}

void
fz_write_float_le(fz_context *ctx, fz_output *out, float f)
{
	union {float f; int32_t i;} u;
	u.f = f;
	fz_write_int32_le(ctx, out, u.i);
}

void
fz_write_float_be(fz_context *ctx, fz_output *out, float f)
{
	union {float f; int32_t i;} u;
	u.f = f;
	fz_write_int32_be(ctx, out, u.i);
}

/*
	Write a UTF-8 encoded unicode character.
*/
void
fz_write_rune(fz_context *ctx, fz_output *out, int rune)
{
	char data[10];
	fz_write_data(ctx, out, data, fz_runetochar(data, rune));
}

void
fz_write_base64(fz_context *ctx, fz_output *out, const unsigned char *data, size_t size, int newline)
{
	static const char set[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t i;
	for (i = 0; i + 3 <= size; i += 3)
	{
		int c = data[i];
		int d = data[i+1];
		int e = data[i+2];
		if (newline && (i & 15) == 0)
			fz_write_byte(ctx, out, '\n');
		fz_write_byte(ctx, out, set[c>>2]);
		fz_write_byte(ctx, out, set[((c&3)<<4)|(d>>4)]);
		fz_write_byte(ctx, out, set[((d&15)<<2)|(e>>6)]);
		fz_write_byte(ctx, out, set[e&63]);
	}
	if (size - i == 2)
	{
		int c = data[i];
		int d = data[i+1];
		fz_write_byte(ctx, out, set[c>>2]);
		fz_write_byte(ctx, out, set[((c&3)<<4)|(d>>4)]);
		fz_write_byte(ctx, out, set[((d&15)<<2)]);
		fz_write_byte(ctx, out, '=');
	}
	else if (size - i == 1)
	{
		int c = data[i];
		fz_write_byte(ctx, out, set[c>>2]);
		fz_write_byte(ctx, out, set[((c&3)<<4)]);
		fz_write_byte(ctx, out, '=');
		fz_write_byte(ctx, out, '=');
	}
}

void
fz_write_base64_buffer(fz_context *ctx, fz_output *out, fz_buffer *buf, int newline)
{
	unsigned char *data;
	size_t size = fz_buffer_storage(ctx, buf, &data);
	fz_write_base64(ctx, out, data, size, newline);
}

void
fz_save_buffer(fz_context *ctx, fz_buffer *buf, const char *filename)
{
	fz_output *out = fz_new_output_with_path(ctx, filename, 0);
	fz_try(ctx)
	{
		fz_write_data(ctx, out, buf->data, buf->len);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

fz_band_writer *fz_new_band_writer_of_size(fz_context *ctx, size_t size, fz_output *out)
{
	fz_band_writer *writer = fz_calloc(ctx, size, 1);
	writer->out = out;
	return writer;
}

/*
	Cause a band writer to write the header for
	a banded image with the given properties/dimensions etc. This
	also configures the bandwriter for the format of the data to be
	passed in future calls.

	w, h: Width and Height of the entire page.

	n: Number of components (including spots and alphas).

	alpha: Number of alpha components.

	xres, yres: X and Y resolutions in dpi.

	cs: Colorspace (NULL for bitmaps)

	seps: Separation details (or NULL).
*/
void fz_write_header(fz_context *ctx, fz_band_writer *writer, int w, int h, int n, int alpha, int xres, int yres, int pagenum, fz_colorspace *cs, fz_separations *seps)
{
	if (writer == NULL || writer->band == NULL)
		return;

	writer->w = w;
	writer->h = h;
	writer->s = fz_count_active_separations(ctx, seps);
	writer->n = n;
	writer->alpha = alpha;
	writer->xres = xres;
	writer->yres = yres;
	writer->pagenum = pagenum;
	writer->line = 0;
	writer->seps = fz_keep_separations(ctx, seps);
	writer->header(ctx, writer, cs);
}

/*
	Cause a band writer to write the next band
	of data for an image.

	stride: The byte offset from the first byte of the data
	for a pixel to the first byte of the data for the same pixel
	on the row below.

	band_height: The number of lines in this band.

	samples: Pointer to first byte of the data.
*/
void fz_write_band(fz_context *ctx, fz_band_writer *writer, int stride, int band_height, const unsigned char *samples)
{
	if (writer == NULL || writer->band == NULL)
		return;
	if (writer->line + band_height > writer->h)
		band_height = writer->h - writer->line;
	if (band_height < 0) {
		fz_throw(ctx, FZ_ERROR_GENERIC, "Too much band data!");
	}
	if (band_height > 0) {
		writer->band(ctx, writer, stride, writer->line, band_height, samples);
		writer->line += band_height;
	}
	if (writer->line == writer->h && writer->trailer) {
		writer->trailer(ctx, writer);
		/* Protect against more band_height == 0 calls */
		writer->line++;
	}
}

void fz_drop_band_writer(fz_context *ctx, fz_band_writer *writer)
{
	if (writer == NULL)
		return;
	if (writer->drop != NULL)
		writer->drop(ctx, writer);
	fz_drop_separations(ctx, writer->seps);
	fz_free(ctx, writer);
}
