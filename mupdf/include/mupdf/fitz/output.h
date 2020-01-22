#ifndef MUPDF_FITZ_OUTPUT_H
#define MUPDF_FITZ_OUTPUT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/string-util.h"
#include "mupdf/fitz/stream.h"

/*
	Generic output streams - generalise between outputting to a file,
	a buffer, etc.
*/
typedef struct fz_output_s fz_output;

/*
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	whenever data is written to the output.

	state: The state for the output stream.

	data: a pointer to a buffer of data to write.

	n: The number of bytes of data to write.
*/
typedef void (fz_output_write_fn)(fz_context *ctx, void *state, const void *data, size_t n);

/*
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called when
	fz_seek_output is requested.

	state: The output stream state to seek within.

	offset, whence: as defined for fs_seek_output.
*/
typedef void (fz_output_seek_fn)(fz_context *ctx, void *state, int64_t offset, int whence);

/*
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called when
	fz_tell_output is requested.

	state: The output stream state to report on.

	Returns the offset within the output stream.
*/
typedef int64_t (fz_output_tell_fn)(fz_context *ctx, void *state);

/*
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	when the output stream is closed, to flush any pending writes.
*/
typedef void (fz_output_close_fn)(fz_context *ctx, void *state);

/*
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	when the output stream is dropped, to release the stream specific
	state information.
*/
typedef void (fz_output_drop_fn)(fz_context *ctx, void *state);

/*
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	when the fz_stream_from_output is called.
*/
typedef fz_stream *(fz_stream_from_output_fn)(fz_context *ctx, void *state);

/*
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	when fz_truncate_output is called to truncate the file
	at that point.
*/
typedef void (fz_truncate_fn)(fz_context *ctx, void *state);

struct fz_output_s
{
	void *state;
	fz_output_write_fn *write;
	fz_output_seek_fn *seek;
	fz_output_tell_fn *tell;
	fz_output_close_fn *close;
	fz_output_drop_fn *drop;
	fz_stream_from_output_fn *as_stream;
	fz_truncate_fn *truncate;
	char *bp, *wp, *ep;
};

fz_output *fz_new_output(fz_context *ctx, int bufsiz, void *state, fz_output_write_fn *write, fz_output_close_fn *close, fz_output_drop_fn *drop);

fz_output *fz_new_output_with_path(fz_context *, const char *filename, int append);

fz_output *fz_new_output_with_buffer(fz_context *ctx, fz_buffer *buf);

fz_output *fz_stdout(fz_context *ctx);

fz_output *fz_stderr(fz_context *ctx);

void fz_set_stdout(fz_context *ctx, fz_output *out);

void fz_set_stderr(fz_context *ctx, fz_output *err);

void fz_write_printf(fz_context *ctx, fz_output *out, const char *fmt, ...);

void fz_write_vprintf(fz_context *ctx, fz_output *out, const char *fmt, va_list ap);

void fz_seek_output(fz_context *ctx, fz_output *out, int64_t off, int whence);

int64_t fz_tell_output(fz_context *ctx, fz_output *out);

void fz_flush_output(fz_context *ctx, fz_output *out);

void fz_close_output(fz_context *, fz_output *);

void fz_drop_output(fz_context *, fz_output *);

fz_stream *fz_stream_from_output(fz_context *, fz_output *);

void fz_truncate_output(fz_context *, fz_output *);

void fz_write_data(fz_context *ctx, fz_output *out, const void *data, size_t size);

void fz_write_string(fz_context *ctx, fz_output *out, const char *s);

void fz_write_int32_be(fz_context *ctx, fz_output *out, int x);
void fz_write_int32_le(fz_context *ctx, fz_output *out, int x);
void fz_write_uint32_be(fz_context *ctx, fz_output *out, unsigned int x);
void fz_write_uint32_le(fz_context *ctx, fz_output *out, unsigned int x);
void fz_write_int16_be(fz_context *ctx, fz_output *out, int x);
void fz_write_int16_le(fz_context *ctx, fz_output *out, int x);
void fz_write_uint16_be(fz_context *ctx, fz_output *out, unsigned int x);
void fz_write_uint16_le(fz_context *ctx, fz_output *out, unsigned int x);
void fz_write_char(fz_context *ctx, fz_output *out, char x);
void fz_write_byte(fz_context *ctx, fz_output *out, unsigned char x);
void fz_write_float_be(fz_context *ctx, fz_output *out, float f);
void fz_write_float_le(fz_context *ctx, fz_output *out, float f);

void fz_write_rune(fz_context *ctx, fz_output *out, int rune);

void fz_write_base64(fz_context *ctx, fz_output *out, const unsigned char *data, size_t size, int newline);
void fz_write_base64_buffer(fz_context *ctx, fz_output *out, fz_buffer *data, int newline);

void fz_format_string(fz_context *ctx, void *user, void (*emit)(fz_context *ctx, void *user, int c), const char *fmt, va_list args);

size_t fz_vsnprintf(char *buffer, size_t space, const char *fmt, va_list args);

size_t fz_snprintf(char *buffer, size_t space, const char *fmt, ...);

char *fz_asprintf(fz_context *ctx, const char *fmt, ...);

void fz_save_buffer(fz_context *ctx, fz_buffer *buf, const char *filename);

fz_output *fz_new_asciihex_output(fz_context *ctx, fz_output *chain);
fz_output *fz_new_ascii85_output(fz_context *ctx, fz_output *chain);
fz_output *fz_new_rle_output(fz_context *ctx, fz_output *chain);
fz_output *fz_new_arc4_output(fz_context *ctx, fz_output *chain, unsigned char *key, size_t keylen);
fz_output *fz_new_deflate_output(fz_context *ctx, fz_output *chain, int effort, int raw);

#endif
