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
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_FITZ_OUTPUT_H
#define MUPDF_FITZ_OUTPUT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/string-util.h"
#include "mupdf/fitz/stream.h"

/**
	Generic output streams - generalise between outputting to a
	file, a buffer, etc.
*/

/**
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	whenever data is written to the output.

	state: The state for the output stream.

	data: a pointer to a buffer of data to write.

	n: The number of bytes of data to write.
*/
typedef void (fz_output_write_fn)(fz_context *ctx, void *state, const void *data, size_t n);

/**
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called when
	fz_seek_output is requested.

	state: The output stream state to seek within.

	offset, whence: as defined for fz_seek().
*/
typedef void (fz_output_seek_fn)(fz_context *ctx, void *state, int64_t offset, int whence);

/**
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called when
	fz_tell_output is requested.

	state: The output stream state to report on.

	Returns the offset within the output stream.
*/
typedef int64_t (fz_output_tell_fn)(fz_context *ctx, void *state);

/**
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	when the output stream is closed, to flush any pending writes.
*/
typedef void (fz_output_close_fn)(fz_context *ctx, void *state);

/**
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	when the output stream is dropped, to release the stream
	specific state information.
*/
typedef void (fz_output_drop_fn)(fz_context *ctx, void *state);

/**
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	when the fz_stream_from_output is called.
*/
typedef fz_stream *(fz_stream_from_output_fn)(fz_context *ctx, void *state);

/**
	A function type for use when implementing
	fz_outputs. The supplied function of this type is called
	when fz_truncate_output is called to truncate the file
	at that point.
*/
typedef void (fz_truncate_fn)(fz_context *ctx, void *state);

struct fz_output
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
	/* If buffered is non-zero, then we have that many
	 * bits (1-7) waiting to be written in bits. */
	int buffered;
	int bits;
};

/**
	Create a new output object with the given
	internal state and function pointers.

	state: Internal state (opaque to everything but implementation).

	write: Function to output a given buffer.

	close: Cleanup function to destroy state when output closed.
	May permissibly be null.
*/
fz_output *fz_new_output(fz_context *ctx, int bufsiz, void *state, fz_output_write_fn *write, fz_output_close_fn *close, fz_output_drop_fn *drop);

/**
	Open an output stream that writes to a
	given path.

	filename: The filename to write to (specified in UTF-8).

	append: non-zero if we should append to the file, rather than
	overwriting it.
*/
fz_output *fz_new_output_with_path(fz_context *, const char *filename, int append);

/**
	Open an output stream that appends
	to a buffer.

	buf: The buffer to append to.
*/
fz_output *fz_new_output_with_buffer(fz_context *ctx, fz_buffer *buf);

/**
	Retrieve an fz_output that directs to stdout.

	Optionally may be fz_dropped when finished with.
*/
fz_output *fz_stdout(fz_context *ctx);

/**
	Retrieve an fz_output that directs to stdout.

	Optionally may be fz_dropped when finished with.
*/
fz_output *fz_stderr(fz_context *ctx);

#ifdef _WIN32
/**
	Retrieve an fz_output that directs to OutputDebugString.

	Optionally may be fz_dropped when finished with.
*/
fz_output *fz_stdods(fz_context *ctx);
#endif

/**
	Set the output stream to be used for fz_stddbg. Set to NULL to
	reset to default (stderr).
*/
void fz_set_stddbg(fz_context *ctx, fz_output *out);

/**
	Retrieve an fz_output for the default debugging stream. On
	Windows this will be OutputDebugString for non-console apps.
	Otherwise, it is always fz_stderr.

	Optionally may be fz_dropped when finished with.
*/
fz_output *fz_stddbg(fz_context *ctx);

/**
	Format and write data to an output stream.
	See fz_format_string for formatting details.
*/
void fz_write_printf(fz_context *ctx, fz_output *out, const char *fmt, ...);

/**
	va_list version of fz_write_printf.
*/
void fz_write_vprintf(fz_context *ctx, fz_output *out, const char *fmt, va_list ap);

/**
	Seek to the specified file position.
	See fseek for arguments.

	Throw an error on unseekable outputs.
*/
void fz_seek_output(fz_context *ctx, fz_output *out, int64_t off, int whence);

/**
	Return the current file position.

	Throw an error on untellable outputs.
*/
int64_t fz_tell_output(fz_context *ctx, fz_output *out);

/**
	Flush unwritten data.
*/
void fz_flush_output(fz_context *ctx, fz_output *out);

/**
	Flush pending output and close an output stream.
*/
void fz_close_output(fz_context *, fz_output *);

/**
	Free an output stream. Don't forget to close it first!
*/
void fz_drop_output(fz_context *, fz_output *);

/**
	Query whether a given fz_output supports fz_stream_from_output.
*/
int fz_output_supports_stream(fz_context *ctx, fz_output *out);

/**
	Obtain the fz_output in the form of a fz_stream.

	This allows data to be read back from some forms of fz_output
	object. When finished reading, the fz_stream should be released
	by calling fz_drop_stream. Until the fz_stream is dropped, no
	further operations should be performed on the fz_output object.
*/
fz_stream *fz_stream_from_output(fz_context *, fz_output *);

/**
	Truncate the output at the current position.

	This allows output streams which have seeked back from the end
	of their storage to be truncated at the current point.
*/
void fz_truncate_output(fz_context *, fz_output *);

/**
	Write data to output.

	data: Pointer to data to write.
	size: Size of data to write in bytes.
*/
void fz_write_data(fz_context *ctx, fz_output *out, const void *data, size_t size);
void fz_write_buffer(fz_context *ctx, fz_output *out, fz_buffer *data);

/**
	Write a string. Does not write zero terminator.
*/
void fz_write_string(fz_context *ctx, fz_output *out, const char *s);

/**
	Write different sized data to an output stream.
*/
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

/**
	Write a UTF-8 encoded unicode character.
*/
void fz_write_rune(fz_context *ctx, fz_output *out, int rune);

/**
	Write a base64 encoded data block, optionally with periodic
	newlines.
*/
void fz_write_base64(fz_context *ctx, fz_output *out, const unsigned char *data, size_t size, int newline);

/**
	Write a base64 encoded fz_buffer, optionally with periodic
	newlines.
*/
void fz_write_base64_buffer(fz_context *ctx, fz_output *out, fz_buffer *data, int newline);

/**
	Write num_bits of data to the end of the output stream, assumed to be packed
	most significant bits first.
*/
void fz_write_bits(fz_context *ctx, fz_output *out, unsigned int data, int num_bits);

/**
	Sync to byte boundary after writing bits.
*/
void fz_write_bits_sync(fz_context *ctx, fz_output *out);

/**
	Our customised 'printf'-like string formatter.
	Takes %c, %d, %s, %u, %x, %X as usual.
	Modifiers are not supported except for zero-padding ints (e.g.
	%02d, %03u, %04x, etc).
	%g output in "as short as possible hopefully lossless
	non-exponent" form, see fz_ftoa for specifics.
	%f and %e output as usual.
	%C outputs a utf8 encoded int.
	%M outputs a fz_matrix*.
	%R outputs a fz_rect*.
	%P outputs a fz_point*.
	%n outputs a PDF name (with appropriate escaping).
	%q and %( output escaped strings in C/PDF syntax.
	%l{d,u,x,X} indicates that the values are int64_t.
	%z{d,u,x,X} indicates that the value is a size_t.

	user: An opaque pointer that is passed to the emit function.

	emit: A function pointer called to emit output bytes as the
	string is being formatted.
*/
void fz_format_string(fz_context *ctx, void *user, void (*emit)(fz_context *ctx, void *user, int c), const char *fmt, va_list args);

/**
	A vsnprintf work-alike, using our custom formatter.
*/
size_t fz_vsnprintf(char *buffer, size_t space, const char *fmt, va_list args);

/**
	The non va_list equivalent of fz_vsnprintf.
*/
size_t fz_snprintf(char *buffer, size_t space, const char *fmt, ...);

/**
	Allocated sprintf.

	Returns a null terminated allocated block containing the
	formatted version of the format string/args.
*/
char *fz_asprintf(fz_context *ctx, const char *fmt, ...);

/**
	Save the contents of a buffer to a file.
*/
void fz_save_buffer(fz_context *ctx, fz_buffer *buf, const char *filename);

/**
	Compression and other filtering outputs.

	These outputs write encoded data to another output. Create a
	filter output with the destination, write to the filter, then
	close and drop it when you're done. These can also be chained
	together, for example to write ASCII Hex encoded, Deflate
	compressed, and RC4 encrypted data to a buffer output.

	Output streams don't use reference counting, so make sure to
	close all of the filters in the reverse order of creation so
	that data is flushed properly.

	Accordingly, ownership of 'chain' is never passed into the
	following functions, but remains with the caller, whose
	responsibility it is to ensure they exist at least until
	the returned fz_output is dropped.
*/

fz_output *fz_new_asciihex_output(fz_context *ctx, fz_output *chain);
fz_output *fz_new_ascii85_output(fz_context *ctx, fz_output *chain);
fz_output *fz_new_rle_output(fz_context *ctx, fz_output *chain);
fz_output *fz_new_arc4_output(fz_context *ctx, fz_output *chain, unsigned char *key, size_t keylen);
fz_output *fz_new_deflate_output(fz_context *ctx, fz_output *chain, int effort, int raw);

#endif
