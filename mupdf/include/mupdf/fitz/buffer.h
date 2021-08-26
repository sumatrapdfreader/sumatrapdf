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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#ifndef MUPDF_FITZ_BUFFER_H
#define MUPDF_FITZ_BUFFER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/**
	fz_buffer is a wrapper around a dynamically allocated array of
	bytes.

	Buffers have a capacity (the number of bytes storage immediately
	available) and a current size.

	The contents of the structure are considered implementation
	details and are subject to change. Users should use the accessor
	functions in preference.
*/
typedef struct
{
	int refs;
	unsigned char *data;
	size_t cap, len;
	int unused_bits;
	int shared;
} fz_buffer;

/**
	Take an additional reference to the buffer. The same pointer
	is returned.

	Never throws exceptions.
*/
fz_buffer *fz_keep_buffer(fz_context *ctx, fz_buffer *buf);

/**
	Drop a reference to the buffer. When the reference count reaches
	zero, the buffer is destroyed.

	Never throws exceptions.
*/
void fz_drop_buffer(fz_context *ctx, fz_buffer *buf);

/**
	Retrieve internal memory of buffer.

	datap: Output parameter that will be pointed to the data.

	Returns the current size of the data in bytes.
*/
size_t fz_buffer_storage(fz_context *ctx, fz_buffer *buf, unsigned char **datap);

/**
	Ensure that a buffer's data ends in a
	0 byte, and return a pointer to it.
*/
const char *fz_string_from_buffer(fz_context *ctx, fz_buffer *buf);

fz_buffer *fz_new_buffer(fz_context *ctx, size_t capacity);

/**
	Create a new buffer with existing data.

	data: Pointer to existing data.
	size: Size of existing data.

	Takes ownership of data. Does not make a copy. Calls fz_free on
	the data when the buffer is deallocated. Do not use 'data' after
	passing to this function.

	Returns pointer to new buffer. Throws exception on allocation
	failure.
*/
fz_buffer *fz_new_buffer_from_data(fz_context *ctx, unsigned char *data, size_t size);

/**
	Like fz_new_buffer, but does not take ownership.
*/
fz_buffer *fz_new_buffer_from_shared_data(fz_context *ctx, const unsigned char *data, size_t size);

/**
	Create a new buffer containing a copy of the passed data.
*/
fz_buffer *fz_new_buffer_from_copied_data(fz_context *ctx, const unsigned char *data, size_t size);

/**
	Create a new buffer with data decoded from a base64 input string.
*/
fz_buffer *fz_new_buffer_from_base64(fz_context *ctx, const char *data, size_t size);

/**
	Ensure that a buffer has a given capacity,
	truncating data if required.

	capacity: The desired capacity for the buffer. If the current
	size of the buffer contents is smaller than capacity, it is
	truncated.
*/
void fz_resize_buffer(fz_context *ctx, fz_buffer *buf, size_t capacity);

/**
	Make some space within a buffer (i.e. ensure that
	capacity > size).
*/
void fz_grow_buffer(fz_context *ctx, fz_buffer *buf);

/**
	Trim wasted capacity from a buffer by resizing internal memory.
*/
void fz_trim_buffer(fz_context *ctx, fz_buffer *buf);

/**
	Empties the buffer. Storage is not freed, but is held ready
	to be reused as the buffer is refilled.

	Never throws exceptions.
*/
void fz_clear_buffer(fz_context *ctx, fz_buffer *buf);

/**
	Append the contents of the source buffer onto the end of the
	destination buffer, extending automatically as required.

	Ownership of buffers does not change.
*/
void fz_append_buffer(fz_context *ctx, fz_buffer *destination, fz_buffer *source);

/**
	fz_append_*: Append data to a buffer.

	The buffer will automatically grow as required.
*/
void fz_append_data(fz_context *ctx, fz_buffer *buf, const void *data, size_t len);
void fz_append_string(fz_context *ctx, fz_buffer *buf, const char *data);
void fz_append_byte(fz_context *ctx, fz_buffer *buf, int c);
void fz_append_rune(fz_context *ctx, fz_buffer *buf, int c);
void fz_append_int32_le(fz_context *ctx, fz_buffer *buf, int x);
void fz_append_int16_le(fz_context *ctx, fz_buffer *buf, int x);
void fz_append_int32_be(fz_context *ctx, fz_buffer *buf, int x);
void fz_append_int16_be(fz_context *ctx, fz_buffer *buf, int x);
void fz_append_bits(fz_context *ctx, fz_buffer *buf, int value, int count);
void fz_append_bits_pad(fz_context *ctx, fz_buffer *buf);

/**
	fz_append_pdf_string: Append a string with PDF syntax quotes and
	escapes.

	The buffer will automatically grow as required.
*/
void fz_append_pdf_string(fz_context *ctx, fz_buffer *buffer, const char *text);

/**
	fz_append_printf: Format and append data to buffer using
	printf-like formatting (see fz_vsnprintf).

	The buffer will automatically grow as required.
*/
void fz_append_printf(fz_context *ctx, fz_buffer *buffer, const char *fmt, ...);

/**
	fz_append_vprintf: Format and append data to buffer using
	printf-like formatting with varargs (see fz_vsnprintf).
*/
void fz_append_vprintf(fz_context *ctx, fz_buffer *buffer, const char *fmt, va_list args);

/**
	Zero-terminate buffer in order to use as a C string.

	This byte is invisible and does not affect the length of the
	buffer as returned by fz_buffer_storage. The zero byte is
	written *after* the data, and subsequent writes will overwrite
	the terminating byte.

	Subsequent changes to the size of the buffer (such as by
	fz_buffer_trim, fz_buffer_grow, fz_resize_buffer, etc) may
	invalidate this.
*/
void fz_terminate_buffer(fz_context *ctx, fz_buffer *buf);

/**
	Create an MD5 digest from buffer contents.

	Never throws exceptions.
*/
void fz_md5_buffer(fz_context *ctx, fz_buffer *buffer, unsigned char digest[16]);

/**
	Take ownership of buffer contents.

	Performs the same task as fz_buffer_storage, but ownership of
	the data buffer returns with this call. The buffer is left
	empty.

	Note: Bad things may happen if this is called on a buffer with
	multiple references that is being used from multiple threads.

	data: Pointer to place to retrieve data pointer.

	Returns length of stream.
*/
size_t fz_buffer_extract(fz_context *ctx, fz_buffer *buf, unsigned char **data);

#endif
