// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#define _LARGEFILE_SOURCE
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include "mupdf/fitz.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <fcntl.h>
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
			fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot fwrite: %s", strerror(errno));
		return;
	}

	n = fwrite(buffer, 1, count, file);
	if (n < count && ferror(file))
		fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot fwrite: %s", strerror(errno));
}

static int64_t stdout_offset = 0;

static void
stdout_write(fz_context *ctx, void *opaque, const void *buffer, size_t count)
{
	stdout_offset += count;
	file_write(ctx, stdout, buffer, count);
}

static int64_t
stdout_tell(fz_context *ctx, void *opaque)
{
	return stdout_offset;
}

static fz_output fz_stdout_global = {
	NULL,
	stdout_write,
	NULL,
	stdout_tell,
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

#ifdef _WIN32
static void
stdods_write(fz_context *ctx, void *opaque, const void *buffer, size_t count)
{
	char *buf = fz_malloc(ctx, count+1);

	memcpy(buf, buffer, count);
	buf[count] = 0;
	OutputDebugStringA(buf);
	fz_free(ctx, buf);
}

static fz_output fz_stdods_global = {
	NULL,
	stdods_write,
	NULL,
	NULL,
	NULL,
};

fz_output *fz_stdods(fz_context *ctx)
{
	return &fz_stdods_global;
}
#endif

fz_output *fz_stddbg(fz_context *ctx)
{
	if (ctx->stddbg)
		return ctx->stddbg;

	return fz_stderr(ctx);
}

void fz_set_stddbg(fz_context *ctx, fz_output *out)
{
	if (ctx == NULL)
		return;

	ctx->stddbg = out;
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
		fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot fseek: %s", strerror(errno));
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
		fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot ftell: %s", strerror(errno));
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
	{
		__int64 pos = _ftelli64(file);
		if (pos >= 0)
			_chsize_s(fileno(file), pos);
	}
#else
	{
		off_t pos = ftello(file);
		if (pos >= 0)
			(void)ftruncate(fileno(file), pos);
	}
#endif
}

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

fz_output *
fz_new_output_with_path(fz_context *ctx, const char *filename, int append)
{
	FILE *file;

	if (filename == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "no output to write to");

	if (!strcmp(filename, "/dev/null"))
		return fz_new_output(ctx, 0, NULL, null_write, NULL, NULL);
	if (!strcmp(filename, "/dev/stdout"))
	{
#ifdef _WIN32
		(void)setmode(fileno(stdout), O_BINARY);
#endif
		return fz_stdout(ctx);
	}
	if (!strcmp(filename, "/dev/stderr"))
	{
#ifdef _WIN32
		(void)setmode(fileno(stderr), O_BINARY);
#endif
		return fz_stderr(ctx);
	}

	/* If <append> is false, we use fopen()'s 'x' flag to force an error if
	 * some other process creates the file immediately after we have removed
	 * it - this avoids vulnerability where a less-privilege process can create
	 * a link and get us to overwrite a different file. See:
	 * 	https://bugs.ghostscript.com/show_bug.cgi?id=701797
	 * 	http://www.open-std.org/jtc1/sc22//WG14/www/docs/n1339.pdf
	 */
#if defined(_WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
#define CLOBBER "x"
#else
#define CLOBBER ""
#endif
/* On windows, we use variants of fopen and remove that cope with utf8. */
#ifdef _WIN32
#define FOPEN fz_fopen_utf8
#define REMOVE fz_remove_utf8
#else
#define FOPEN fopen
#define REMOVE remove
#endif
	/* Ensure we create a brand new file. We don't want to clobber our old file. */
	if (!append)
	{
		if (REMOVE(filename) < 0)
			if (errno != ENOENT)
				fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot remove file '%s': %s", filename, strerror(errno));
	}
	file = FOPEN(filename, append ? "rb+" : "wb+" CLOBBER);
	if (append)
	{
		if (file == NULL)
			file = FOPEN(filename, "wb+");
		else
			fseek(file, 0, SEEK_END);
	}
#undef FOPEN
#undef REMOVE
#undef CLOBBER
	if (!file)
		fz_throw(ctx, FZ_ERROR_SYSTEM, "cannot open file '%s': %s", filename, strerror(errno));

	return fz_new_output_with_file_ptr(ctx, file);
}

fz_output *
fz_new_output_with_file_ptr(fz_context *ctx, FILE *file)
{
	fz_output *out;

	if (!file)
		return fz_new_output(ctx, 0, NULL, null_write, NULL, NULL);

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
	fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot seek in buffer: %s", strerror(errno));
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

static void
buffer_reset(fz_context *ctx, void *opaque)
{
	fz_buffer *buffer = opaque;
	fz_clear_buffer(ctx, buffer);
}

fz_output *
fz_new_output_with_buffer(fz_context *ctx, fz_buffer *buf)
{
	fz_output *out = fz_new_output(ctx, 0, fz_keep_buffer(ctx, buf), buffer_write, NULL, buffer_drop);
	out->seek = buffer_seek;
	out->tell = buffer_tell;
	out->reset = buffer_reset;
	return out;
}

void
fz_close_output(fz_context *ctx, fz_output *out)
{
	if (out == NULL)
		return;
	fz_flush_output(ctx, out);
	if (!out->closed && out->close)
		out->close(ctx, out->state);
	out->closed = 1;
}

void
fz_drop_output(fz_context *ctx, fz_output *out)
{
	if (out)
	{
		if (!out->closed)
			fz_warn(ctx, "dropping unclosed output");
		if (out->drop)
			out->drop(ctx, out->state);
		fz_free(ctx, out->bp);
		if (out != &fz_stdout_global && out != &fz_stderr_global)
			fz_free(ctx, out);
	}
}

void
fz_reset_output(fz_context *ctx, fz_output *out)
{
	if (!out)
		return;
	if (out->reset == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot reset this output");

	out->reset(ctx, out->state);
	out->closed = 0;
}

void
fz_seek_output(fz_context *ctx, fz_output *out, int64_t off, int whence)
{
	if (out->seek == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot seek in unseekable output stream\n");
	fz_flush_output(ctx, out);
	out->seek(ctx, out->state, off, whence);
}

int64_t
fz_tell_output(fz_context *ctx, fz_output *out)
{
	if (out->tell == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot tell in untellable output stream\n");
	if (out->bp)
		return out->tell(ctx, out->state) + (out->wp - out->bp);
	return out->tell(ctx, out->state);
}

fz_stream *
fz_stream_from_output(fz_context *ctx, fz_output *out)
{
	if (out->as_stream == NULL)
		return NULL;
	fz_flush_output(ctx, out);
	return out->as_stream(ctx, out->state);
}

void
fz_truncate_output(fz_context *ctx, fz_output *out)
{
	if (out->truncate == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot truncate this output stream");
	fz_flush_output(ctx, out);
	out->truncate(ctx, out->state);
}

static void
fz_write_emit(fz_context *ctx, void *out, int c)
{
	fz_write_byte(ctx, out, c);
}

void
fz_write_vprintf(fz_context *ctx, fz_output *out, const char *fmt, va_list args)
{
	fz_format_string(ctx, out, fz_write_emit, fmt, args);
}

void
fz_write_printf(fz_context *ctx, fz_output *out, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fz_format_string(ctx, out, fz_write_emit, fmt, args);
	va_end(args);
}

void
fz_flush_output(fz_context *ctx, fz_output *out)
{
	fz_write_bits_sync(ctx, out);
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

void
fz_write_buffer(fz_context *ctx, fz_output *out, fz_buffer *buf)
{
	fz_write_data(ctx, out, buf->data, buf->len);
}

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

void
fz_write_rune(fz_context *ctx, fz_output *out, int rune)
{
	char data[10];
	fz_write_data(ctx, out, data, fz_runetochar(data, rune));
}

void
fz_write_base64(fz_context *ctx, fz_output *out, const unsigned char *data, size_t size, int newline)
{
	static const char set[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
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
fz_append_base64(fz_context *ctx, fz_buffer *out, const unsigned char *data, size_t size, int newline)
{
	static const char set[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t i;
	for (i = 0; i + 3 <= size; i += 3)
	{
		int c = data[i];
		int d = data[i+1];
		int e = data[i+2];
		if (newline && (i & 15) == 0)
			fz_append_byte(ctx, out, '\n');
		fz_append_byte(ctx, out, set[c>>2]);
		fz_append_byte(ctx, out, set[((c&3)<<4)|(d>>4)]);
		fz_append_byte(ctx, out, set[((d&15)<<2)|(e>>6)]);
		fz_append_byte(ctx, out, set[e&63]);
	}
	if (size - i == 2)
	{
		int c = data[i];
		int d = data[i+1];
		fz_append_byte(ctx, out, set[c>>2]);
		fz_append_byte(ctx, out, set[((c&3)<<4)|(d>>4)]);
		fz_append_byte(ctx, out, set[((d&15)<<2)]);
		fz_append_byte(ctx, out, '=');
	}
	else if (size - i == 1)
	{
		int c = data[i];
		fz_append_byte(ctx, out, set[c>>2]);
		fz_append_byte(ctx, out, set[((c&3)<<4)]);
		fz_append_byte(ctx, out, '=');
		fz_append_byte(ctx, out, '=');
	}
}

void
fz_append_base64_buffer(fz_context *ctx, fz_buffer *out, fz_buffer *buf, int newline)
{
	unsigned char *data;
	size_t size = fz_buffer_storage(ctx, buf, &data);
	fz_append_base64(ctx, out, data, size, newline);
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

void fz_write_header(fz_context *ctx, fz_band_writer *writer, int w, int h, int n, int alpha, int xres, int yres, int pagenum, fz_colorspace *cs, fz_separations *seps)
{
	if (writer == NULL || writer->band == NULL)
		return;

	if (w <= 0 || h <= 0 || n <= 0 || alpha < 0 || alpha > 1)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid bandwriter header dimensions/setup");

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

void fz_write_band(fz_context *ctx, fz_band_writer *writer, int stride, int band_height, const unsigned char *samples)
{
	if (writer == NULL || writer->band == NULL)
		return;
	if (writer->line + band_height > writer->h)
		band_height = writer->h - writer->line;
	if (band_height < 0) {
		fz_throw(ctx, FZ_ERROR_LIMIT, "Too much band data!");
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

void fz_close_band_writer(fz_context *ctx, fz_band_writer *writer)
{
	if (writer == NULL)
		return;
	if (writer->close != NULL)
		writer->close(ctx, writer);
	writer->close = NULL;
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

int fz_output_supports_stream(fz_context *ctx, fz_output *out)
{
	return out != NULL && out->as_stream != NULL;
}

void fz_write_bits(fz_context *ctx, fz_output *out, unsigned int data, int num_bits)
{
	while (num_bits)
	{
		/* How many bits will be left in the current byte after we
		 * insert these bits? */
		int n = 8 - num_bits - out->buffered;
		if (n >= 0)
		{
			/* We can fit our data in. */
			out->bits |= data << n;
			out->buffered += num_bits;
			num_bits = 0;
		}
		else
		{
			/* There are 8 - out->buffered bits left to be filled. We have
			 * num_bits to fill it with, which is more, so we need to throw
			 * away the bottom 'num_bits - (8 - out->buffered)' bits. That's
			 * num_bits + out->buffered - 8 = -(8 - num_bits - out_buffered) = -n */
			out->bits |= data >> -n;
			data &= ~(out->bits << -n);
			num_bits = -n;
			out->buffered = 8;
		}
		if (out->buffered == 8)
		{
			fz_write_byte(ctx, out, out->bits);
			out->buffered = 0;
			out->bits = 0;
		}
	}

}

void fz_write_bits_sync(fz_context *ctx, fz_output *out)
{
	if (out->buffered == 0)
		return;
	fz_write_bits(ctx, out, 0, 8 - out->buffered);
}

void
fz_write_stream(fz_context *ctx, fz_output *out, fz_stream *in)
{
	size_t z;

	while ((z = fz_available(ctx, in, 4096)) != 0)
	{
		fz_write_data(ctx, out, in->rp, z);
		in->rp += z;
	}
}
