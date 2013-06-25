#ifndef MUPDF_FITZ_BUFFER_H
#define MUPDF_FITZ_BUFFER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

/*
	fz_buffer is a wrapper around a dynamically allocated array of bytes.

	Buffers have a capacity (the number of bytes storage immediately
	available) and a current size.
*/
typedef struct fz_buffer_s fz_buffer;

/*
	fz_keep_buffer: Increment the reference count for a buffer.

	buf: The buffer to increment the reference count for.

	Returns a pointer to the buffer. Does not throw exceptions.
*/
fz_buffer *fz_keep_buffer(fz_context *ctx, fz_buffer *buf);

/*
	fz_drop_buffer: Decrement the reference count for a buffer.

	buf: The buffer to decrement the reference count for.
*/
void fz_drop_buffer(fz_context *ctx, fz_buffer *buf);

/*
	fz_buffer_storage: Retrieve information on the storage currently used
	by a buffer.

	data: Pointer to place to retrieve data pointer.

	Returns length of stream.
*/
int fz_buffer_storage(fz_context *ctx, fz_buffer *buf, unsigned char **data);

struct fz_buffer_s
{
	int refs;
	unsigned char *data;
	int cap, len;
	int unused_bits;
};

/*
	fz_new_buffer: Create a new buffer.

	capacity: Initial capacity.

	Returns pointer to new buffer. Throws exception on allocation
	failure.
*/
fz_buffer *fz_new_buffer(fz_context *ctx, int capacity);

/*
	fz_new_buffer: Create a new buffer.

	capacity: Initial capacity.

	Returns pointer to new buffer. Throws exception on allocation
	failure.
*/
fz_buffer *fz_new_buffer_from_data(fz_context *ctx, unsigned char *data, int size);

/*
	fz_resize_buffer: Ensure that a buffer has a given capacity,
	truncating data if required.

	buf: The buffer to alter.

	capacity: The desired capacity for the buffer. If the current size
	of the buffer contents is smaller than capacity, it is truncated.

*/
void fz_resize_buffer(fz_context *ctx, fz_buffer *buf, int capacity);

/*
	fz_grow_buffer: Make some space within a buffer (i.e. ensure that
	capacity > size).

	buf: The buffer to grow.

	May throw exception on failure to allocate.
*/
void fz_grow_buffer(fz_context *ctx, fz_buffer *buf);

/*
	fz_trim_buffer: Trim wasted capacity from a buffer.

	buf: The buffer to trim.
*/
void fz_trim_buffer(fz_context *ctx, fz_buffer *buf);

/*
	fz_buffer_cat: Concatenate buffers

	buf: first to concatenate and the holder of the result
	extra: second to concatenate

	May throw exception on failure to allocate.
*/
void fz_buffer_cat(fz_context *ctx, fz_buffer *buf, fz_buffer *extra);

void fz_write_buffer(fz_context *ctx, fz_buffer *buf, const void *data, int len);

void fz_write_buffer_byte(fz_context *ctx, fz_buffer *buf, int val);

void fz_write_buffer_rune(fz_context *ctx, fz_buffer *buf, int val);

void fz_write_buffer_bits(fz_context *ctx, fz_buffer *buf, int val, int bits);

void fz_write_buffer_pad(fz_context *ctx, fz_buffer *buf);

/*
	fz_buffer_printf: print formatted to a buffer. The buffer will grow
	as required.
*/
int fz_buffer_printf(fz_context *ctx, fz_buffer *buffer, const char *fmt, ...);
int fz_buffer_vprintf(fz_context *ctx, fz_buffer *buffer, const char *fmt, va_list args);

/*
	fz_buffer_printf: print a string formatted as a pdf string to a buffer.
	The buffer will grow.
*/
void
fz_buffer_cat_pdf_string(fz_context *ctx, fz_buffer *buffer, const char *text);

#endif
