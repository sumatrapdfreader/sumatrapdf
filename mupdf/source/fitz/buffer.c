#include "fitz-imp.h"

#include <string.h>
#include <stdarg.h>

fz_buffer *
fz_new_buffer(fz_context *ctx, size_t size)
{
	fz_buffer *b;

	size = size > 1 ? size : 16;

	b = fz_malloc_struct(ctx, fz_buffer);
	b->refs = 1;
	fz_try(ctx)
	{
		b->data = Memento_label(fz_malloc(ctx, size), "fz_buffer_data");
	}
	fz_catch(ctx)
	{
		fz_free(ctx, b);
		fz_rethrow(ctx);
	}
	b->cap = size;
	b->len = 0;
	b->unused_bits = 0;

	return b;
}

/*
	Create a new buffer with existing data.

	data: Pointer to existing data.
	size: Size of existing data.

	Takes ownership of data. Does not make a copy. Calls fz_free on the
	data when the buffer is deallocated. Do not use 'data' after passing
	to this function.

	Returns pointer to new buffer. Throws exception on allocation
	failure.
*/
fz_buffer *
fz_new_buffer_from_data(fz_context *ctx, unsigned char *data, size_t size)
{
	fz_buffer *b = NULL;

	fz_try(ctx)
	{
		b = fz_malloc_struct(ctx, fz_buffer);
		b->refs = 1;
		b->data = data;
		b->cap = size;
		b->len = size;
		b->unused_bits = 0;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, data);
		fz_rethrow(ctx);
	}

	return b;
}

/*
	Like fz_new_buffer, but does not take ownership.
*/
fz_buffer *
fz_new_buffer_from_shared_data(fz_context *ctx, const unsigned char *data, size_t size)
{
	fz_buffer *b;

	b = fz_malloc_struct(ctx, fz_buffer);
	b->refs = 1;
	b->data = (unsigned char *)data; /* cast away const */
	b->cap = size;
	b->len = size;
	b->unused_bits = 0;
	b->shared = 1;

	return b;
}

/*
	Create a new buffer containing a copy of the passed data.
*/
fz_buffer *
fz_new_buffer_from_copied_data(fz_context *ctx, const unsigned char *data, size_t size)
{
	fz_buffer *b = fz_new_buffer(ctx, size);
	b->len = size;
	memcpy(b->data, data, size);
	return b;
}

/*
	Create a new buffer with data decoded from a base64 input string.
*/
fz_buffer *
fz_new_buffer_from_base64(fz_context *ctx, const char *data, size_t size)
{
	fz_buffer *buf = fz_new_buffer(ctx, size);
	const char *end = data + (size > 0 ? size : strlen(data));
	const char *s = data;
	fz_try(ctx)
	{
		while (s < end)
		{
			int c = *s++;
			if (c >= 'A' && c <= 'Z')
				fz_append_bits(ctx, buf, c - 'A', 6);
			else if (c >= 'a' && c <= 'z')
				fz_append_bits(ctx, buf, c - 'a' + 26, 6);
			else if (c >= '0' && c <= '9')
				fz_append_bits(ctx, buf, c - '0' + 52, 6);
			else if (c == '+')
				fz_append_bits(ctx, buf, 62, 6);
			else if (c == '/')
				fz_append_bits(ctx, buf, 63, 6);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
	return buf;
}

fz_buffer *
fz_keep_buffer(fz_context *ctx, fz_buffer *buf)
{
	return fz_keep_imp(ctx, buf, &buf->refs);
}

void
fz_drop_buffer(fz_context *ctx, fz_buffer *buf)
{
	if (fz_drop_imp(ctx, buf, &buf->refs))
	{
		if (!buf->shared)
			fz_free(ctx, buf->data);
		fz_free(ctx, buf);
	}
}

/*
	Ensure that a buffer has a given capacity,
	truncating data if required.

	capacity: The desired capacity for the buffer. If the current size
	of the buffer contents is smaller than capacity, it is truncated.
*/
void
fz_resize_buffer(fz_context *ctx, fz_buffer *buf, size_t size)
{
	if (buf->shared)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot resize a buffer with shared storage");
	buf->data = fz_realloc(ctx, buf->data, size);
	buf->cap = size;
	if (buf->len > buf->cap)
		buf->len = buf->cap;
}

/*
	Make some space within a buffer (i.e. ensure that
	capacity > size).
*/
void
fz_grow_buffer(fz_context *ctx, fz_buffer *buf)
{
	size_t newsize = (buf->cap * 3) / 2;
	if (newsize == 0)
		newsize = 256;
	fz_resize_buffer(ctx, buf, newsize);
}

static void
fz_ensure_buffer(fz_context *ctx, fz_buffer *buf, size_t min)
{
	size_t newsize = buf->cap;
	if (newsize < 16)
		newsize = 16;
	while (newsize < min)
	{
		newsize = (newsize * 3) / 2;
	}
	fz_resize_buffer(ctx, buf, newsize);
}

/*
	Trim wasted capacity from a buffer by resizing internal memory.
*/
void
fz_trim_buffer(fz_context *ctx, fz_buffer *buf)
{
	if (buf->cap > buf->len+1)
		fz_resize_buffer(ctx, buf, buf->len);
}

void
fz_clear_buffer(fz_context *ctx, fz_buffer *buf)
{
	buf->len = 0;
}

/*
	Zero-terminate buffer in order to use as a C string.

	This byte is invisible and does not affect the length of the buffer as returned by fz_buffer_storage.
	The zero byte is written *after* the data, and subsequent writes will overwrite the terminating byte.
*/
void
fz_terminate_buffer(fz_context *ctx, fz_buffer *buf)
{
	/* ensure that there is a zero-byte after the end of the data */
	if (buf->len + 1 > buf->cap)
		fz_grow_buffer(ctx, buf);
	buf->data[buf->len] = 0;
}

/*
	Retrieve internal memory of buffer.

	datap: Output parameter that will be pointed to the data.

	Returns the current size of the data in bytes.
*/
size_t
fz_buffer_storage(fz_context *ctx, fz_buffer *buf, unsigned char **datap)
{
	if (datap)
		*datap = (buf ? buf->data : NULL);
	return (buf ? buf->len : 0);
}

/*
	Ensure that a buffer's data ends in a
	0 byte, and return a pointer to it.
*/
const char *
fz_string_from_buffer(fz_context *ctx, fz_buffer *buf)
{
	if (!buf)
		return "";
	fz_terminate_buffer(ctx, buf);
	return (const char *)buf->data;
}

/*
	Take ownership of buffer contents.
	Performs the same task as fz_buffer_storage, but ownership of
	the data buffer returns with this call. The buffer is left
	empty.

	Note: Bad things may happen if this is called on a buffer with
	multiple references that is being used from multiple threads.

	data: Pointer to place to retrieve data pointer.

	Returns length of stream.
*/
size_t
fz_buffer_extract(fz_context *ctx, fz_buffer *buf, unsigned char **datap)
{
	size_t len = buf ? buf->len : 0;
	*datap = (buf ? buf->data : NULL);

	if (buf)
	{
		buf->data = NULL;
		buf->len = 0;
	}
	return len;
}

void
fz_append_buffer(fz_context *ctx, fz_buffer *buf, fz_buffer *extra)
{
	if (buf->cap - buf->len < extra->len)
	{
		buf->data = fz_realloc(ctx, buf->data, buf->len + extra->len);
		buf->cap = buf->len + extra->len;
	}

	memcpy(buf->data + buf->len, extra->data, extra->len);
	buf->len += extra->len;
}

/*
	fz_append_*: Append data to a buffer.
	fz_append_printf: Format and append data to buffer using printf-like formatting (see fz_vsnprintf).
	fz_append_pdf_string: Append a string with PDF syntax quotes and escapes.
	The buffer will automatically grow as required.
*/
void
fz_append_data(fz_context *ctx, fz_buffer *buf, const void *data, size_t len)
{
	if (buf->len + len > buf->cap)
		fz_ensure_buffer(ctx, buf, buf->len + len);
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
	buf->unused_bits = 0;
}

void
fz_append_string(fz_context *ctx, fz_buffer *buf, const char *data)
{
	size_t len = strlen(data);
	if (buf->len + len > buf->cap)
		fz_ensure_buffer(ctx, buf, buf->len + len);
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
	buf->unused_bits = 0;
}

void
fz_append_byte(fz_context *ctx, fz_buffer *buf, int val)
{
	if (buf->len + 1 > buf->cap)
		fz_grow_buffer(ctx, buf);
	buf->data[buf->len++] = val;
	buf->unused_bits = 0;
}

void
fz_append_rune(fz_context *ctx, fz_buffer *buf, int c)
{
	char data[10];
	int len = fz_runetochar(data, c);
	if (buf->len + len > buf->cap)
		fz_ensure_buffer(ctx, buf, buf->len + len);
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
	buf->unused_bits = 0;
}

void
fz_append_int32_be(fz_context *ctx, fz_buffer *buf, int x)
{
	fz_append_byte(ctx, buf, (x >> 24) & 0xFF);
	fz_append_byte(ctx, buf, (x >> 16) & 0xFF);
	fz_append_byte(ctx, buf, (x >> 8) & 0xFF);
	fz_append_byte(ctx, buf, (x) & 0xFF);
}

void
fz_append_int16_be(fz_context *ctx, fz_buffer *buf, int x)
{
	fz_append_byte(ctx, buf, (x >> 8) & 0xFF);
	fz_append_byte(ctx, buf, (x) & 0xFF);
}

void
fz_append_int32_le(fz_context *ctx, fz_buffer *buf, int x)
{
	fz_append_byte(ctx, buf, (x)&0xFF);
	fz_append_byte(ctx, buf, (x>>8)&0xFF);
	fz_append_byte(ctx, buf, (x>>16)&0xFF);
	fz_append_byte(ctx, buf, (x>>24)&0xFF);
}

void
fz_append_int16_le(fz_context *ctx, fz_buffer *buf, int x)
{
	fz_append_byte(ctx, buf, (x)&0xFF);
	fz_append_byte(ctx, buf, (x>>8)&0xFF);
}

void
fz_append_bits(fz_context *ctx, fz_buffer *buf, int val, int bits)
{
	int shift;

	/* Throughout this code, the invariant is that we need to write the
	 * bottom 'bits' bits of 'val' into the stream. On entry we assume
	 * that val & ((1<<bits)-1) == val, but we do not rely on this after
	 * having written the first partial byte. */

	if (bits == 0)
		return;

	/* buf->len always covers all the bits in the buffer, including
	 * any unused ones in the last byte, which will always be 0.
	 * buf->unused_bits = the number of unused bits in the last byte.
	 */

	/* Find the amount we need to shift val up by so that it will be in
	 * the correct position to be inserted into any existing data byte. */
	shift = (buf->unused_bits - bits);

	/* Extend the buffer as required before we start; that way we never
	 * fail part way during writing. If shift < 0, then we'll need -shift
	 * more bits. */
	if (shift < 0)
	{
		int extra = (7-shift)>>3; /* Round up to bytes */
		fz_ensure_buffer(ctx, buf, buf->len + extra);
	}

	/* Write any bits that will fit into the existing byte */
	if (buf->unused_bits)
	{
		buf->data[buf->len-1] |= (shift >= 0 ? (((unsigned int)val)<<shift) : (((unsigned int)val)>>-shift));
		if (shift >= 0)
		{
			/* If we were shifting up, we're done. */
			buf->unused_bits -= bits;
			return;
		}
		/* The number of bits left to write is the number that didn't
		 * fit in this first byte. */
		bits = -shift;
	}

	/* Write any whole bytes */
	while (bits >= 8)
	{
		bits -= 8;
		buf->data[buf->len++] = val>>bits;
	}

	/* Write trailing bits (with 0's in unused bits) */
	if (bits > 0)
	{
		bits = 8-bits;
		buf->data[buf->len++] = val<<bits;
	}
	buf->unused_bits = bits;
}

void
fz_append_bits_pad(fz_context *ctx, fz_buffer *buf)
{
	buf->unused_bits = 0;
}

static void fz_append_emit(fz_context *ctx, void *buffer, int c)
{
	fz_append_byte(ctx, buffer, c);
}

void
fz_append_printf(fz_context *ctx, fz_buffer *buffer, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fz_format_string(ctx, buffer, fz_append_emit, fmt, args);
	va_end(args);
}

void
fz_append_vprintf(fz_context *ctx, fz_buffer *buffer, const char *fmt, va_list args)
{
	fz_format_string(ctx, buffer, fz_append_emit, fmt, args);
}

void
fz_append_pdf_string(fz_context *ctx, fz_buffer *buffer, const char *text)
{
	size_t len = 2;
	const char *s = text;
	char *d;
	char c;

	while ((c = *s++) != 0)
	{
		switch (c)
		{
		case '\n':
		case '\r':
		case '\t':
		case '\b':
		case '\f':
		case '(':
		case ')':
		case '\\':
			len++;
			break;
		}
		len++;
	}

	while(buffer->cap - buffer->len < len)
		fz_grow_buffer(ctx, buffer);

	s = text;
	d = (char *)buffer->data + buffer->len;
	*d++ = '(';
	while ((c = *s++) != 0)
	{
		switch (c)
		{
		case '\n':
			*d++ = '\\';
			*d++ = 'n';
			break;
		case '\r':
			*d++ = '\\';
			*d++ = 'r';
			break;
		case '\t':
			*d++ = '\\';
			*d++ = 't';
			break;
		case '\b':
			*d++ = '\\';
			*d++ = 'b';
			break;
		case '\f':
			*d++ = '\\';
			*d++ = 'f';
			break;
		case '(':
			*d++ = '\\';
			*d++ = '(';
			break;
		case ')':
			*d++ = '\\';
			*d++ = ')';
			break;
		case '\\':
			*d++ = '\\';
			*d++ = '\\';
			break;
		default:
			*d++ = c;
		}
	}
	*d = ')';
	buffer->len += len;
}

void
fz_md5_buffer(fz_context *ctx, fz_buffer *buffer, unsigned char digest[16])
{
	fz_md5 state;
	fz_md5_init(&state);
	if (buffer)
		fz_md5_update(&state, buffer->data, buffer->len);
	fz_md5_final(&state, digest);
}

#ifdef TEST_BUFFER_WRITE

#define TEST_LEN 1024

void
fz_test_buffer_write(fz_context *ctx)
{
	fz_buffer *master = fz_new_buffer(ctx, TEST_LEN);
	fz_buffer *copy = fz_new_buffer(ctx, TEST_LEN);
	fz_stream *stm;
	int i, j, k;

	/* Make us a dummy buffer */
	for (i = 0; i < TEST_LEN; i++)
	{
		master->data[i] = rand();
	}
	master->len = TEST_LEN;

	/* Now copy that buffer several times, checking it for validity */
	stm = fz_open_buffer(ctx, master);
	for (i = 0; i < 256; i++)
	{
		memset(copy->data, i, TEST_LEN);
		copy->len = 0;
		j = TEST_LEN * 8;
		do
		{
			k = (rand() & 31)+1;
			if (k > j)
				k = j;
			fz_append_bits(ctx, copy, fz_read_bits(ctx, stm, k), k);
			j -= k;
		}
		while (j);

		if (memcmp(copy->data, master->data, TEST_LEN) != 0)
			fprintf(stderr, "Copied buffer is different!\n");
		fz_seek(stm, 0, 0);
	}
	fz_drop_stream(stm);
	fz_drop_buffer(ctx, master);
	fz_drop_buffer(ctx, copy);
}
#endif
