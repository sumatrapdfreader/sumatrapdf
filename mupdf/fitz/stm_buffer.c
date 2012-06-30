#include "fitz-internal.h"

fz_buffer *
fz_new_buffer(fz_context *ctx, int size)
{
	fz_buffer *b;

	size = size > 1 ? size : 16;

	b = fz_malloc_struct(ctx, fz_buffer);
	b->refs = 1;
	fz_try(ctx)
	{
		b->data = fz_malloc(ctx, size);
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

fz_buffer *
fz_keep_buffer(fz_context *ctx, fz_buffer *buf)
{
	if (buf)
	{
		if (buf->refs == 1 && buf->cap > buf->len+1)
			fz_resize_buffer(ctx, buf, buf->len);
		buf->refs ++;
	}

	return buf;
}

void
fz_drop_buffer(fz_context *ctx, fz_buffer *buf)
{
	if (!buf)
		return;
	if (--buf->refs == 0)
	{
		fz_free(ctx, buf->data);
		fz_free(ctx, buf);
	}
}

void
fz_resize_buffer(fz_context *ctx, fz_buffer *buf, int size)
{
	/* SumatraPDF: prevent integer overflows if fz_grow_buffer and fz_trim_buffer */
	if (size < 0)
		fz_throw(ctx, "size %d indicates integer overflow", size);
	buf->data = fz_resize_array(ctx, buf->data, size, 1);
	buf->cap = size;
	if (buf->len > buf->cap)
		buf->len = buf->cap;
}

void
fz_grow_buffer(fz_context *ctx, fz_buffer *buf)
{
	fz_resize_buffer(ctx, buf, (buf->cap * 3) / 2);
}

static void
fz_ensure_buffer(fz_context *ctx, fz_buffer *buf, int min)
{
	int newsize = buf->cap;
	while (newsize < min)
	{
		newsize = (newsize * 3) / 2;
	}
	fz_resize_buffer(ctx, buf, newsize);
}

void
fz_trim_buffer(fz_context *ctx, fz_buffer *buf)
{
	if (buf->cap > buf->len+1)
		fz_resize_buffer(ctx, buf, buf->len);
}

int
fz_buffer_storage(fz_context *ctx, fz_buffer *buf, unsigned char **datap)
{
	if (datap)
		*datap = (buf ? buf->data : NULL);
	return (buf ? buf->len : 0);
}

void fz_write_buffer(fz_context *ctx, fz_buffer *buf, unsigned char *data, int len)
{
	if (buf->len + len > buf->cap)
		fz_ensure_buffer(ctx, buf, buf->len + len);
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
	buf->unused_bits = 0;
}

void fz_write_buffer_byte(fz_context *ctx, fz_buffer *buf, int val)
{
	if (buf->len > buf->cap)
		fz_grow_buffer(ctx, buf);
	buf->data[buf->len++] = val;
	buf->unused_bits = 0;
}

void fz_write_buffer_bits(fz_context *ctx, fz_buffer *buf, int val, int bits)
{
	int extra;
	int shift;

	if (bits == 0)
		return;

	/* buf->len always covers all the bits in the buffer, including
	 * any unused ones in the last byte, which will always be 0.
	 * buf->unused_bits = the number of unused bits in the last byte.
	 */

	/* Extend the buffer as required before we start; that way we never
	 * fail part way during writing. */
	shift = (buf->unused_bits - bits);
	if (shift < 0)
	{
		extra = (7-buf->unused_bits)>>3;
		fz_ensure_buffer(ctx, buf, buf->len + extra);
	}

	/* Write any bits that will fit into the existing byte */
	if (buf->unused_bits)
	{
		buf->data[buf->len-1] |= (shift >= 0 ? (((unsigned int)val)<<shift) : (((unsigned int)val)>>-shift));
		if (shift >= 0)
			return;
		bits += shift;
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
		bits += 8;
		buf->data[buf->len++] = val<<bits;
	}
	buf->unused_bits = bits;
}

void fz_write_buffer_pad(fz_context *ctx, fz_buffer *buf)
{
	buf->unused_bits = 0;
}

void
fz_buffer_printf(fz_context *ctx, fz_buffer *buffer, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	/* Caller guarantees not to generate more than 256 bytes per call */
	while(buffer->cap - buffer->len < 256)
		fz_grow_buffer(ctx, buffer);

	buffer->len += vsprintf((char *)buffer->data + buffer->len, fmt, args);

	va_end(args);
}
