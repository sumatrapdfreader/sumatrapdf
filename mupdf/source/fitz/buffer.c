#include "mupdf/fitz.h"

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
fz_new_buffer_from_data(fz_context *ctx, unsigned char *data, int size)
{
	fz_buffer *b;

	b = fz_malloc_struct(ctx, fz_buffer);
	b->refs = 1;
	b->data = data;
	b->cap = size;
	b->len = size;
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
	/* SumatraPDF: prevent integer overflows in fz_grow_buffer and fz_trim_buffer */
	if (size < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "size %d indicates integer overflow", size);
	buf->data = fz_resize_array(ctx, buf->data, size, 1);
	buf->cap = size;
	if (buf->len > buf->cap)
		buf->len = buf->cap;
}

void
fz_grow_buffer(fz_context *ctx, fz_buffer *buf)
{
	int newsize = (buf->cap * 3) / 2;
	if (newsize == 0)
		newsize = 256;
	fz_resize_buffer(ctx, buf, newsize);
}

static void
fz_ensure_buffer(fz_context *ctx, fz_buffer *buf, int min)
{
	int newsize = buf->cap;
	if (newsize < 16)
		newsize = 16;
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

void
fz_buffer_cat(fz_context *ctx, fz_buffer *buf, fz_buffer *extra)
{
	if (buf->cap - buf->len < extra->len)
	{
		buf->data = fz_resize_array(ctx, buf->data, buf->len + extra->len, 1);
		buf->cap = buf->len + extra->len;
	}

	memcpy(buf->data + buf->len, extra->data, extra->len);
	buf->len += extra->len;
}

void fz_write_buffer(fz_context *ctx, fz_buffer *buf, const void *data, int len)
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

void fz_write_buffer_rune(fz_context *ctx, fz_buffer *buf, int c)
{
	char data[10];
	int len = fz_runetochar(data, c);
	if (buf->len + len > buf->cap)
		fz_ensure_buffer(ctx, buf, buf->len + len);
	memcpy(buf->data + buf->len, data, len);
	buf->len += len;
	buf->unused_bits = 0;
}

void fz_write_buffer_bits(fz_context *ctx, fz_buffer *buf, int val, int bits)
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

void fz_write_buffer_pad(fz_context *ctx, fz_buffer *buf)
{
	buf->unused_bits = 0;
}

int
fz_buffer_printf(fz_context *ctx, fz_buffer *buffer, const char *fmt, ...)
{
	int ret;
	va_list args;
	va_start(args, fmt);

	ret = fz_buffer_vprintf(ctx, buffer, fmt, args);

	va_end(args);

	return ret;
}

int
fz_buffer_vprintf(fz_context *ctx, fz_buffer *buffer, const char *fmt, va_list old_args)
{
	int len;

	do
	{
		int slack = buffer->cap - buffer->len;

		if (slack > 0)
		{
			va_list args;
#ifdef _MSC_VER /* Microsoft Visual C */
			args = old_args;
#else
			va_copy(args, old_args);
#endif
			len = vsnprintf((char *)buffer->data + buffer->len, slack, fmt, args);
#ifndef _MSC_VER
			va_end(args);
#endif
			/* len = number of chars written, not including the terminating
			 * NULL, so len+1 > slack means "truncated". MSVC differs here
			 * and returns -1 for truncated. */
			if (len >= 0 && len+1 <= slack)
				break;
		}
		/* Grow the buffer and retry */
		fz_grow_buffer(ctx, buffer);
	}
	while (1);

	buffer->len += len;

	return len;
}

void
fz_buffer_cat_pdf_string(fz_context *ctx, fz_buffer *buffer, const char *text)
{
	int len = 2;
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
	*d++ = ')';
	buffer->len += len;
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
			fz_write_buffer_bits(ctx, copy, fz_read_bits(stm, k), k);
			j -= k;
		}
		while (j);

		if (memcmp(copy->data, master->data, TEST_LEN) != 0)
			fprintf(stderr, "Copied buffer is different!\n");
		fz_seek(stm, 0, 0);
	}
	fz_close(stm);
	fz_drop_buffer(ctx, master);
	fz_drop_buffer(ctx, copy);
}
#endif
