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

#ifndef MUPDF_FITZ_STREAM_H
#define MUPDF_FITZ_STREAM_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"

/**
	Return true if the named file exists and is readable.
*/
int fz_file_exists(fz_context *ctx, const char *path);

/**
	fz_stream is a buffered reader capable of seeking in both
	directions.

	Streams are reference counted, so references must be dropped
	by a call to fz_drop_stream.

	Only the data between rp and wp is valid.
*/
typedef struct fz_stream fz_stream;

/**
	Open the named file and wrap it in a stream.

	filename: Path to a file. On non-Windows machines the filename
	should be exactly as it would be passed to fopen(2). On Windows
	machines, the path should be UTF-8 encoded so that non-ASCII
	characters can be represented. Other platforms do the encoding
	as standard anyway (and in most cases, particularly for MacOS
	and Linux, the encoding they use is UTF-8 anyway).
*/
fz_stream *fz_open_file(fz_context *ctx, const char *filename);

/**
	Open the named file and wrap it in a stream.

	Does the same as fz_open_file, but in the event the file
	does not open, it will return NULL rather than throw an
	exception.
*/
fz_stream *fz_try_open_file(fz_context *ctx, const char *name);

#ifdef _WIN32
/**
	Open the named file and wrap it in a stream.

	This function is only available when compiling for Win32.

	filename: Wide character path to the file as it would be given
	to _wfopen().
*/
fz_stream *fz_open_file_w(fz_context *ctx, const wchar_t *filename);
#endif /* _WIN32 */

/**
	Open a block of memory as a stream.

	data: Pointer to start of data block. Ownership of the data
	block is NOT passed in.

	len: Number of bytes in data block.

	Returns pointer to newly created stream. May throw exceptions on
	failure to allocate.
*/
fz_stream *fz_open_memory(fz_context *ctx, const unsigned char *data, size_t len);

/**
	Open a buffer as a stream.

	buf: The buffer to open. Ownership of the buffer is NOT passed
	in (this function takes its own reference).

	Returns pointer to newly created stream. May throw exceptions on
	failure to allocate.
*/
fz_stream *fz_open_buffer(fz_context *ctx, fz_buffer *buf);

/**
	Attach a filter to a stream that will store any
	characters read from the stream into the supplied buffer.

	chain: The underlying stream to leech from.

	buf: The buffer into which the read data should be appended.
	The buffer will be resized as required.

	Returns pointer to newly created stream. May throw exceptions on
	failure to allocate.
*/
fz_stream *fz_open_leecher(fz_context *ctx, fz_stream *chain, fz_buffer *buf);

/**
	Increments the reference count for a stream. Returns the same
	pointer.

	Never throws exceptions.
*/
fz_stream *fz_keep_stream(fz_context *ctx, fz_stream *stm);

/**
	Decrements the reference count for a stream.

	When the reference count for the stream hits zero, frees the
	storage used for the fz_stream itself, and (usually)
	releases the underlying resources that the stream is based upon
	(depends on the method used to open the stream initially).
*/
void fz_drop_stream(fz_context *ctx, fz_stream *stm);

/**
	return the current reading position within a stream
*/
int64_t fz_tell(fz_context *ctx, fz_stream *stm);

/**
	Seek within a stream.

	stm: The stream to seek within.

	offset: The offset to seek to.

	whence: From where the offset is measured (see fseek).
	SEEK_SET - start of stream.
	SEEK_CUR - current position.
	SEEK_END - end of stream.

*/
void fz_seek(fz_context *ctx, fz_stream *stm, int64_t offset, int whence);

/**
	Read from a stream into a given data block.

	stm: The stream to read from.

	data: The data block to read into.

	len: The length of the data block (in bytes).

	Returns the number of bytes read. May throw exceptions.
*/
size_t fz_read(fz_context *ctx, fz_stream *stm, unsigned char *data, size_t len);

/**
	Read from a stream discarding data.

	stm: The stream to read from.

	len: The number of bytes to read.

	Returns the number of bytes read. May throw exceptions.
*/
size_t fz_skip(fz_context *ctx, fz_stream *stm, size_t len);

/**
	Read all of a stream into a buffer.

	stm: The stream to read from

	initial: Suggested initial size for the buffer.

	Returns a buffer created from reading from the stream. May throw
	exceptions on failure to allocate.
*/
fz_buffer *fz_read_all(fz_context *ctx, fz_stream *stm, size_t initial);

/**
	Read all the contents of a file into a buffer.
*/
fz_buffer *fz_read_file(fz_context *ctx, const char *filename);

/**
	Read all the contents of a file into a buffer.

	Returns NULL if the file does not exist, otherwise
	behaves exactly as fz_read_file.
*/
fz_buffer *fz_try_read_file(fz_context *ctx, const char *filename);

/**
	fz_read_[u]int(16|24|32|64)(_le)?

	Read a 16/32/64 bit signed/unsigned integer from stream,
	in big or little-endian byte orders.

	Throws an exception if EOF is encountered.
*/
uint16_t fz_read_uint16(fz_context *ctx, fz_stream *stm);
uint32_t fz_read_uint24(fz_context *ctx, fz_stream *stm);
uint32_t fz_read_uint32(fz_context *ctx, fz_stream *stm);
uint64_t fz_read_uint64(fz_context *ctx, fz_stream *stm);

uint16_t fz_read_uint16_le(fz_context *ctx, fz_stream *stm);
uint32_t fz_read_uint24_le(fz_context *ctx, fz_stream *stm);
uint32_t fz_read_uint32_le(fz_context *ctx, fz_stream *stm);
uint64_t fz_read_uint64_le(fz_context *ctx, fz_stream *stm);

int16_t fz_read_int16(fz_context *ctx, fz_stream *stm);
int32_t fz_read_int32(fz_context *ctx, fz_stream *stm);
int64_t fz_read_int64(fz_context *ctx, fz_stream *stm);

int16_t fz_read_int16_le(fz_context *ctx, fz_stream *stm);
int32_t fz_read_int32_le(fz_context *ctx, fz_stream *stm);
int64_t fz_read_int64_le(fz_context *ctx, fz_stream *stm);

float fz_read_float_le(fz_context *ctx, fz_stream *stm);
float fz_read_float(fz_context *ctx, fz_stream *stm);

/**
	Read a null terminated string from the stream into
	a buffer of a given length. The buffer will be null terminated.
	Throws on failure (including the failure to fit the entire
	string including the terminator into the buffer).
*/
void fz_read_string(fz_context *ctx, fz_stream *stm, char *buffer, int len);

/**
	Read a utf-8 rune from a stream.

	In the event of encountering badly formatted utf-8 codes
	(such as a leading code with an unexpected number of following
	codes) no error/exception is given, but undefined values may be
	returned.
*/
int fz_read_rune(fz_context *ctx, fz_stream *in);

/**
	Read a utf-16 rune from a stream. (little endian and
	big endian respectively).

	In the event of encountering badly formatted utf-16 codes
	(mismatched surrogates) no error/exception is given, but
	undefined values may be returned.
*/
int fz_read_utf16_le(fz_context *ctx, fz_stream *stm);
int fz_read_utf16_be(fz_context *ctx, fz_stream *stm);

/**
	A function type for use when implementing
	fz_streams. The supplied function of this type is called
	whenever data is required, and the current buffer is empty.

	stm: The stream to operate on.

	max: a hint as to the maximum number of bytes that the caller
	needs to be ready immediately. Can safely be ignored.

	Returns -1 if there is no more data in the stream. Otherwise,
	the function should find its internal state using stm->state,
	refill its buffer, update stm->rp and stm->wp to point to the
	start and end of the new data respectively, and then
	"return *stm->rp++".
*/
typedef int (fz_stream_next_fn)(fz_context *ctx, fz_stream *stm, size_t max);

/**
	A function type for use when implementing
	fz_streams. The supplied function of this type is called
	when the stream is dropped, to release the stream specific
	state information.

	state: The stream state to release.
*/
typedef void (fz_stream_drop_fn)(fz_context *ctx, void *state);

/**
	A function type for use when implementing
	fz_streams. The supplied function of this type is called when
	fz_seek is requested, and the arguments are as defined for
	fz_seek.

	The stream can find it's private state in stm->state.
*/
typedef void (fz_stream_seek_fn)(fz_context *ctx, fz_stream *stm, int64_t offset, int whence);

struct fz_stream
{
	int refs;
	int error;
	int eof;
	int progressive;
	int64_t pos;
	int avail;
	int bits;
	unsigned char *rp, *wp;
	void *state;
	fz_stream_next_fn *next;
	fz_stream_drop_fn *drop;
	fz_stream_seek_fn *seek;
};

/**
	Create a new stream object with the given
	internal state and function pointers.

	state: Internal state (opaque to everything but implementation).

	next: Should provide the next set of bytes (up to max) of stream
	data. Return the number of bytes read, or EOF when there is no
	more data.

	drop: Should clean up and free the internal state. May not
	throw exceptions.
*/
fz_stream *fz_new_stream(fz_context *ctx, void *state, fz_stream_next_fn *next, fz_stream_drop_fn *drop);

/**
	Attempt to read a stream into a buffer. If truncated
	is NULL behaves as fz_read_all, sets a truncated flag in case of
	error.

	stm: The stream to read from.

	initial: Suggested initial size for the buffer.

	truncated: Flag to store success/failure indication in.

	worst_case: 0 for unknown, otherwise an upper bound for the
	size of the stream.

	Returns a buffer created from reading from the stream.
*/
fz_buffer *fz_read_best(fz_context *ctx, fz_stream *stm, size_t initial, int *truncated, size_t worst_case);

/**
	Read a line from stream into the buffer until either a
	terminating newline or EOF, which it replaces with a null byte
	('\0').

	Returns buf on success, and NULL when end of file occurs while
	no characters have been read.
*/
char *fz_read_line(fz_context *ctx, fz_stream *stm, char *buf, size_t max);

/**
	Skip over a given string in a stream. Return 0 if successfully
	skipped, non-zero otherwise. As many characters will be skipped
	over as matched in the string.
*/
int fz_skip_string(fz_context *ctx, fz_stream *stm, const char *str);

/**
	Skip over whitespace (bytes <= 32) in a stream.
*/
void fz_skip_space(fz_context *ctx, fz_stream *stm);

/**
	Ask how many bytes are available immediately from
	a given stream.

	stm: The stream to read from.

	max: A hint for the underlying stream; the maximum number of
	bytes that we are sure we will want to read. If you do not know
	this number, give 1.

	Returns the number of bytes immediately available between the
	read and write pointers. This number is guaranteed only to be 0
	if we have hit EOF. The number of bytes returned here need have
	no relation to max (could be larger, could be smaller).
*/
static inline size_t fz_available(fz_context *ctx, fz_stream *stm, size_t max)
{
	size_t len = stm->wp - stm->rp;
	int c = EOF;

	if (len)
		return len;
	if (stm->eof)
		return 0;

	fz_try(ctx)
		c = stm->next(ctx, stm, max);
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_report_error(ctx);
		fz_warn(ctx, "read error; treating as end of file");
		stm->error = 1;
		c = EOF;
	}
	if (c == EOF)
	{
		stm->eof = 1;
		return 0;
	}
	stm->rp--;
	return stm->wp - stm->rp;
}

/**
	Read the next byte from a stream.

	stm: The stream t read from.

	Returns -1 for end of stream, or the next byte. May
	throw exceptions.
*/
static inline int fz_read_byte(fz_context *ctx, fz_stream *stm)
{
	int c = EOF;

	if (stm->rp != stm->wp)
		return *stm->rp++;
	if (stm->eof)
		return EOF;
	fz_try(ctx)
		c = stm->next(ctx, stm, 1);
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_report_error(ctx);
		fz_warn(ctx, "read error; treating as end of file");
		stm->error = 1;
		c = EOF;
	}
	if (c == EOF)
		stm->eof = 1;
	return c;
}

/**
	Peek at the next byte in a stream.

	stm: The stream to peek at.

	Returns -1 for EOF, or the next byte that will be read.
*/
static inline int fz_peek_byte(fz_context *ctx, fz_stream *stm)
{
	int c = EOF;

	if (stm->rp != stm->wp)
		return *stm->rp;
	if (stm->eof)
		return EOF;

	fz_try(ctx)
	{
		c = stm->next(ctx, stm, 1);
		if (c != EOF)
			stm->rp--;
	}
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_report_error(ctx);
		fz_warn(ctx, "read error; treating as end of file");
		stm->error = 1;
		c = EOF;
	}
	if (c == EOF)
		stm->eof = 1;
	return c;
}

/**
	Unread the single last byte successfully
	read from a stream. Do not call this without having
	successfully read a byte.

	stm: The stream to operate upon.
*/
static inline void fz_unread_byte(fz_context *ctx FZ_UNUSED, fz_stream *stm)
{
	stm->rp--;
}

/**
	Query if the stream has reached EOF (during normal bytewise
	reading).

	See fz_is_eof_bits for the equivalent function for bitwise
	reading.
*/
static inline int fz_is_eof(fz_context *ctx, fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		if (stm->eof)
			return 1;
		return fz_peek_byte(ctx, stm) == EOF;
	}
	return 0;
}

/**
	Read the next n bits from a stream (assumed to
	be packed most significant bit first).

	stm: The stream to read from.

	n: The number of bits to read, between 1 and 8*sizeof(int)
	inclusive.

	Returns -1 for EOF, or the required number of bits.
*/
static inline unsigned int fz_read_bits(fz_context *ctx, fz_stream *stm, int n)
{
	int x;

	if (n <= stm->avail)
	{
		stm->avail -= n;
		x = (stm->bits >> stm->avail) & ((1 << n) - 1);
	}
	else
	{
		x = stm->bits & ((1 << stm->avail) - 1);
		n -= stm->avail;
		stm->avail = 0;

		while (n > 8)
		{
			x = (x << 8) | fz_read_byte(ctx, stm);
			n -= 8;
		}

		if (n > 0)
		{
			stm->bits = fz_read_byte(ctx, stm);
			stm->avail = 8 - n;
			x = (x << n) | (stm->bits >> stm->avail);
		}
	}

	return x;
}

/**
	Read the next n bits from a stream (assumed to
	be packed least significant bit first).

	stm: The stream to read from.

	n: The number of bits to read, between 1 and 8*sizeof(int)
	inclusive.

	Returns (unsigned int)-1 for EOF, or the required number of bits.
*/
static inline unsigned int fz_read_rbits(fz_context *ctx, fz_stream *stm, int n)
{
	int x;

	if (n <= stm->avail)
	{
		x = stm->bits & ((1 << n) - 1);
		stm->avail -= n;
		stm->bits = stm->bits >> n;
	}
	else
	{
		unsigned int used = 0;

		x = stm->bits & ((1 << stm->avail) - 1);
		n -= stm->avail;
		used = stm->avail;
		stm->avail = 0;

		while (n > 8)
		{
			x = (fz_read_byte(ctx, stm) << used) | x;
			n -= 8;
			used += 8;
		}

		if (n > 0)
		{
			stm->bits = fz_read_byte(ctx, stm);
			x = ((stm->bits & ((1 << n) - 1)) << used) | x;
			stm->avail = 8 - n;
			stm->bits = stm->bits >> n;
		}
	}

	return x;
}

/**
	Called after reading bits to tell the stream
	that we are about to return to reading bytewise. Resyncs
	the stream to whole byte boundaries.
*/
static inline void fz_sync_bits(fz_context *ctx FZ_UNUSED, fz_stream *stm)
{
	stm->avail = 0;
}

/**
	Query if the stream has reached EOF (during bitwise
	reading).

	See fz_is_eof for the equivalent function for bytewise
	reading.
*/
static inline int fz_is_eof_bits(fz_context *ctx, fz_stream *stm)
{
	return fz_is_eof(ctx, stm) && (stm->avail == 0 || stm->bits == EOF);
}

/* Implementation details: subject to change. */

/**
	Create a stream from a FILE * that will not be closed
	when the stream is dropped.
*/
fz_stream *fz_open_file_ptr_no_close(fz_context *ctx, FILE *file);

#endif
