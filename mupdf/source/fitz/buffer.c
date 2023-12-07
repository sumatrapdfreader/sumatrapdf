// Copyright (C) 2004-2023 Artifex Software, Inc.
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

#include "mupdf/fitz.h"

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

fz_buffer *
fz_new_buffer_from_copied_data(fz_context *ctx, const unsigned char *data, size_t size)
{
	fz_buffer *b = fz_new_buffer(ctx, size);
	b->len = size;
	memcpy(b->data, data, size);
	return b;
}

fz_buffer *fz_clone_buffer(fz_context *ctx, fz_buffer *buf)
{
	return fz_new_buffer_from_copied_data(ctx, buf ? buf->data : NULL, buf ? buf->len : 0);
}

static inline int iswhite(int a)
{
	switch (a) {
	case '\n': case '\r': case '\t': case ' ':
	case '\f':
		return 1;
	}
	return 0;
}

fz_buffer *
fz_new_buffer_from_base64(fz_context *ctx, const char *data, size_t size)
{
	fz_buffer *out = fz_new_buffer(ctx, size > 0 ? size : strlen(data));
	const char *end = data + (size > 0 ? size : strlen(data));
	const char *s = data;
	uint32_t buf = 0;
	int bits = 0;

	/* This is https://infra.spec.whatwg.org/#forgiving-base64-decode
	 * but even more relaxed. We allow any number of trailing '=' code
	 * points and instead of returning failure on invalid characters, we
	 * warn and truncate.
	 */

	while (s < end && iswhite(*s))
		s++;
	while (s < end && iswhite(end[-1]))
		end--;
	while (s < end && end[-1] == '=')
		end--;

	fz_try(ctx)
	{
		while (s < end)
		{
			int c = *s++;

			if (c >= 'A' && c <= 'Z')
				c = c - 'A';
			else if (c >= 'a' && c <= 'z')
				c = c - 'a' + 26;
			else if (c >= '0' && c <= '9')
				c = c - '0' + 52;
			else if (c == '+')
				c = 62;
			else if (c == '/')
				c = 63;
			else if (iswhite(c))
				continue;
			else
			{
				fz_warn(ctx, "invalid character in base64");
				break;
			}

			buf <<= 6;
			buf |= c & 0x3f;
			bits += 6;

			if (bits == 24)
			{
				fz_append_byte(ctx, out, buf >> 16);
				fz_append_byte(ctx, out, buf >> 8);
				fz_append_byte(ctx, out, buf >> 0);
				bits = 0;
			}
		}

		if (bits == 18)
		{
			fz_append_byte(ctx, out, buf >> 10);
			fz_append_byte(ctx, out, buf >> 2);
		}
		else if (bits == 12)
		{
			fz_append_byte(ctx, out, buf >> 4);
		}
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, out);
		fz_rethrow(ctx);
	}
	return out;
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

void
fz_resize_buffer(fz_context *ctx, fz_buffer *buf, size_t size)
{
	if (buf->shared)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot resize a buffer with shared storage");
	buf->data = fz_realloc(ctx, buf->data, size);
	buf->cap = size;
	if (buf->len > buf->cap)
		buf->len = buf->cap;
}

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

void
fz_terminate_buffer(fz_context *ctx, fz_buffer *buf)
{
	/* ensure that there is a zero-byte after the end of the data */
	if (buf->len + 1 > buf->cap)
		fz_grow_buffer(ctx, buf);
	buf->data[buf->len] = 0;
}

size_t
fz_buffer_storage(fz_context *ctx, fz_buffer *buf, unsigned char **datap)
{
	if (datap)
		*datap = (buf ? buf->data : NULL);
	return (buf ? buf->len : 0);
}

const char *
fz_string_from_buffer(fz_context *ctx, fz_buffer *buf)
{
	if (!buf)
		return "";
	fz_terminate_buffer(ctx, buf);
	return (const char *)buf->data;
}

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

fz_buffer *
fz_slice_buffer(fz_context *ctx, fz_buffer *buf, int64_t start, int64_t end)
{
	unsigned char *src = NULL;
	size_t size = fz_buffer_storage(ctx, buf, &src);
	size_t s, e;

	if (start < 0)
		start += size;
	if (end < 0)
		end += size;

	s = fz_clamp64(start, 0, size);
	e = fz_clamp64(end, 0, size);

	if (s == size || e <= s)
		return fz_new_buffer(ctx, 0);

	return fz_new_buffer_from_copied_data(ctx, &src[s], e - s);
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
