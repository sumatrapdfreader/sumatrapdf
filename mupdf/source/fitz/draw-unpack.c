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

#include "mupdf/fitz.h"
#include "draw-imp.h"

#include <string.h>

/* Unpack image samples and optionally pad pixels with opaque alpha */

#define get1(buf,x) ((buf[x >> 3] >> ( 7 - (x & 7) ) ) & 1 )
#define get2(buf,x) ((buf[x >> 2] >> ( ( 3 - (x & 3) ) << 1 ) ) & 3 )
#define get4(buf,x) ((buf[x >> 1] >> ( ( 1 - (x & 1) ) << 2 ) ) & 15 )
#define get8(buf,x) (buf[x])
#define get16(buf,x) (buf[x << 1])
#define get24(buf,x) (buf[(x << 1) + x])
#define get32(buf,x) (buf[x << 2])

static unsigned char get1_tab_1[256][8];
static unsigned char get1_tab_1p[256][16];
static unsigned char get1_tab_255[256][8];
static unsigned char get1_tab_255p[256][16];

/*
	Bug 697012 shows that the unpacking code can confuse valgrind due
	to the use of undefined bits in the padding at the end of lines.
	We unpack from bits to bytes by copying from a lookup table.
	Valgrind is not capable of understanding that it doesn't matter
	what the undefined bits are, as the bytes we copy that correspond
	to the defined bits will always agree regardless of these
	undefined bits by construction of the table.

	We therefore have a VGMASK macro that explicitly masks off these
	bits in PACIFY_VALGRIND builds.
*/
#ifdef PACIFY_VALGRIND
static const unsigned char mask[9] = { 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
#define VGMASK(v,m) (v & mask[(m)])
#else
#define VGMASK(v,m) (v)
#endif

static void
init_get1_tables(void)
{
	static int once = 0;
	unsigned char bits[1];
	int i, k, x;

	/* TODO: mutex lock here */

	if (once)
		return;

	for (i = 0; i < 256; i++)
	{
		bits[0] = i;
		for (k = 0; k < 8; k++)
		{
			x = get1(bits, k);

			get1_tab_1[i][k] = x;
			get1_tab_1p[i][k * 2] = x;
			get1_tab_1p[i][k * 2 + 1] = 255;

			get1_tab_255[i][k] = x * 255;
			get1_tab_255p[i][k * 2] = x * 255;
			get1_tab_255p[i][k * 2 + 1] = 255;
		}
	}

	once = 1;
}

static void
fz_unpack_mono_line_unscaled(unsigned char *dp, unsigned char *sp, int w, int n, int depth, int scale, int pad, int skip)
{
	int w3 = w >> 3;
	int x;

	for (x = 0; x < w3; x++)
	{
		memcpy(dp, get1_tab_1[*sp++], 8);
		dp += 8;
	}
	x = x << 3;
	if (x < w)
		memcpy(dp, get1_tab_1[VGMASK(*sp, w - x)], w - x);
}

static void
fz_unpack_mono_line_scaled(unsigned char *dp, unsigned char *sp, int w, int n, int depth, int scale, int pad, int skip)
{
	int w3 = w >> 3;
	int x;

	for (x = 0; x < w3; x++)
	{
		memcpy(dp, get1_tab_255[*sp++], 8);
		dp += 8;
	}
	x = x << 3;
	if (x < w)
		memcpy(dp, get1_tab_255[VGMASK(*sp, w - x)], w - x);
}

static void
fz_unpack_mono_line_unscaled_with_padding(unsigned char *dp, unsigned char *sp, int w, int n, int depth, int scale, int pad, int skip)
{
	int w3 = w >> 3;
	int x;

	for (x = 0; x < w3; x++)
	{
		memcpy(dp, get1_tab_1p[*sp++], 16);
		dp += 16;
	}
	x = x << 3;
	if (x < w)
		memcpy(dp, get1_tab_1p[VGMASK(*sp, w - x)], (w - x) << 1);
}

static void
fz_unpack_mono_line_scaled_with_padding(unsigned char *dp, unsigned char *sp, int w, int n, int depth, int scale, int pad, int skip)
{
	int w3 = w >> 3;
	int x;

	for (x = 0; x < w3; x++)
	{
		memcpy(dp, get1_tab_255p[*sp++], 16);
		dp += 16;
	}
	x = x << 3;
	if (x < w)
		memcpy(dp, get1_tab_255p[VGMASK(*sp, w - x)], (w - x) << 1);
}

static void
fz_unpack_line(unsigned char *dp, unsigned char *sp, int w, int n, int depth, int scale, int pad, int skip)
{
	int len = w * n;
	while (len--)
		*dp++ = *sp++;
}

static void
fz_unpack_line_with_padding(unsigned char *dp, unsigned char *sp, int w, int n, int depth, int scale, int pad, int skip)
{
	int x, k;

	for (x = 0; x < w; x++)
	{
		for (k = 0; k < n; k++)
			*dp++ = *sp++;
		*dp++ = 255;
	}
}

static void
fz_unpack_any_l2depth(unsigned char *dp, unsigned char *sp, int w, int n, int depth, int scale, int pad, int skip)
{
	unsigned char *p = dp;
	int b = 0;
	int x, k;

	for (x = 0; x < w; x++)
	{
		for (k = 0; k < n; k++)
		{
			switch (depth)
			{
			case 1: *p++ = get1(sp, b) * scale; break;
			case 2: *p++ = get2(sp, b) * scale; break;
			case 4: *p++ = get4(sp, b) * scale; break;
			case 8: *p++ = get8(sp, b); break;
			case 16: *p++ = get16(sp, b); break;
			case 24: *p++ = get24(sp, b); break;
			case 32: *p++ = get32(sp, b); break;
			}
			b++;
		}
		b += skip;
		if (pad)
			*p++ = 255;
	}
}

typedef void (*fz_unpack_line_fn)(unsigned char *dp, unsigned char *sp, int w, int n, int depth, int scale, int pad, int skip);

void
fz_unpack_tile(fz_context *ctx, fz_pixmap *dst, unsigned char *src, int n, int depth, size_t stride, int scale)
{
	unsigned char *sp = src;
	unsigned char *dp = dst->samples;
	fz_unpack_line_fn unpack_line = NULL;
	int pad, y, skip;
	int w = dst->w;
	int h = dst->h;

	pad = 0;
	skip = 0;
	if (dst->n > n)
		pad = 255;
	if (dst->n < n)
	{
		skip = n - dst->n;
		n = dst->n;
	}

	if (depth == 1)
		init_get1_tables();

	if (scale == 0)
	{
		switch (depth)
		{
		case 1: scale = 255; break;
		case 2: scale = 85; break;
		case 4: scale = 17; break;
		}
	}

	if (n == 1 && depth == 1 && scale == 1 && !pad && !skip)
		unpack_line = fz_unpack_mono_line_unscaled;
	else if (n == 1 && depth == 1 && scale == 255 && !pad && !skip)
		unpack_line = fz_unpack_mono_line_scaled;
	else if (n == 1 && depth == 1 && scale == 1 && pad && !skip)
		unpack_line = fz_unpack_mono_line_unscaled_with_padding;
	else if (n == 1 && depth == 1 && scale == 255 && pad && !skip)
		unpack_line = fz_unpack_mono_line_scaled_with_padding;
	else if (depth == 8 && !pad && !skip)
		unpack_line = fz_unpack_line;
	else if (depth == 8 && pad && !skip)
		unpack_line = fz_unpack_line_with_padding;
	else if (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth  == 16 || depth == 24 || depth == 32)
		unpack_line = fz_unpack_any_l2depth;

	if (unpack_line)
	{
		for (y = 0; y < h; y++, sp += stride, dp += dst->stride)
			unpack_line(dp, sp, w, n, depth, scale, pad, skip);
	}
	else if (depth > 0 && depth <= 8 * (int)sizeof(int))
	{
		fz_stream *stm;
		int x, k;
		size_t skipbits = 8 * stride - (size_t)w * n * depth;

		if (skipbits > 32)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Inappropriate stride!");

		stm = fz_open_memory(ctx, sp, h * stride);
		fz_try(ctx)
		{
			for (y = 0; y < h; y++)
			{
				for (x = 0; x < w; x++)
				{
					for (k = 0; k < n; k++)
					{
						if (depth <= 8)
							*dp++ = fz_read_bits(ctx, stm, depth) << (8 - depth);
						else
							*dp++ = fz_read_bits(ctx, stm, depth) >> (depth - 8);
					}
					if (pad)
						*dp++ = 255;
				}

				dp += dst->stride - (size_t)w * (n + (pad > 0));
				(void) fz_read_bits(ctx, stm, (int)skipbits);
			}
		}
		fz_always(ctx)
			fz_drop_stream(ctx, stm);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot unpack tile with %d bits per component", depth);
}

/* Apply decode array */

void
fz_decode_indexed_tile(fz_context *ctx, fz_pixmap *pix, const float *decode, int maxval)
{
	int add[FZ_MAX_COLORS];
	int mul[FZ_MAX_COLORS];
	unsigned char *p = pix->samples;
	size_t stride = pix->stride - pix->w * (size_t)pix->n;
	int len;
	int pn = pix->n;
	int n = pn - pix->alpha;
	int needed;
	int k;
	int h;

	needed = 0;
	for (k = 0; k < n; k++)
	{
		int min = decode[k * 2] * 256;
		int max = decode[k * 2 + 1] * 256;
		add[k] = min;
		mul[k] = (max - min) / maxval;
		needed |= min != 0 || max != maxval * 256;
	}

	if (!needed)
		return;

	h = pix->h;
	while (h--)
	{
		len = pix->w;
		while (len--)
		{
			for (k = 0; k < n; k++)
			{
				int value = (add[k] + (((p[k] << 8) * mul[k]) >> 8)) >> 8;
				p[k] = fz_clampi(value, 0, 255);
			}
			p += pn;
		}
		p += stride;
	}
}

void
fz_decode_tile(fz_context *ctx, fz_pixmap *pix, const float *decode)
{
	int add[FZ_MAX_COLORS];
	int mul[FZ_MAX_COLORS];
	unsigned char *p = pix->samples;
	size_t stride = pix->stride - pix->w * (size_t)pix->n;
	int len;
	int n = fz_maxi(1, pix->n - pix->alpha);
	int k;
	int h;

	for (k = 0; k < n; k++)
	{
		int min = decode[k * 2] * 255;
		int max = decode[k * 2 + 1] * 255;
		add[k] = min;
		mul[k] = max - min;
	}

	h = pix->h;
	while (h--)
	{
		len = pix->w;
		while (len--)
		{
			for (k = 0; k < n; k++)
			{
				int value = add[k] + fz_mul255(p[k], mul[k]);
				p[k] = fz_clampi(value, 0, 255);
			}
			p += pix->n;
		}
		p += stride;
	}
}

typedef struct
{
	fz_stream *src;
	int depth;
	int w;
	int h;
	int n;
	int skip;
	int pad;
	int scale;
	int src_stride;
	int dst_stride;
	fz_unpack_line_fn unpack;
	unsigned char buf[1];
} unpack_state;

static int
unpack_next(fz_context *ctx, fz_stream *stm, size_t max)
{
	unpack_state *state = (unpack_state *)stm->state;
	size_t n = state->src_stride;

	stm->rp = state->buf;
	do
	{
		size_t a = fz_available(ctx, state->src, n);
		if (a == 0)
			return EOF;
		if (a > n)
			a = n;
		memcpy(stm->rp, state->src->rp, a);
		stm->rp += a;
		state->src->rp += a;
		n -= a;
	}
	while (n);

	state->h--;
	stm->pos += state->dst_stride;
	stm->wp = stm->rp + state->dst_stride;
	state->unpack(stm->rp, state->buf, state->w, state->n, state->depth, state->scale, state->pad, state->skip);

	return *stm->rp++;
}

static void
unpack_drop(fz_context *ctx, void *state)
{
	fz_free(ctx, state);
}

fz_stream *
fz_unpack_stream(fz_context *ctx, fz_stream *src, int depth, int w, int h, int n, int indexed, int pad, int skip)
{
	int src_stride = (w*depth*n+7)>>3;
	int dst_stride;
	unpack_state *state;
	fz_unpack_line_fn unpack_line = NULL;
	int scale = 1;

	if (depth == 1)
		init_get1_tables();

	if (!indexed)
		switch (depth)
		{
		case 1: scale = 255; break;
		case 2: scale = 85; break;
		case 4: scale = 17; break;
		}

	dst_stride = w * (n + !!pad);

	if (n == 1 && depth == 1 && scale == 1 && !pad && !skip)
		unpack_line = fz_unpack_mono_line_unscaled;
	else if (n == 1 && depth == 1 && scale == 255 && !pad && !skip)
		unpack_line = fz_unpack_mono_line_scaled;
	else if (n == 1 && depth == 1 && scale == 1 && pad && !skip)
		unpack_line = fz_unpack_mono_line_unscaled_with_padding;
	else if (n == 1 && depth == 1 && scale == 255 && pad && !skip)
		unpack_line = fz_unpack_mono_line_scaled_with_padding;
	else if (depth == 8 && !pad && !skip)
		unpack_line = fz_unpack_line;
	else if (depth == 8 && pad && !skip)
		unpack_line = fz_unpack_line_with_padding;
	else if (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth  == 16 || depth == 24 || depth == 32)
		unpack_line = fz_unpack_any_l2depth;
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "Unsupported combination in fz_unpack_stream");

	state = fz_malloc(ctx, sizeof(unpack_state) + dst_stride + src_stride);
	state->src = src;
	state->depth = depth;
	state->w = w;
	state->h = h;
	state->n = n;
	state->skip = skip;
	state->pad = pad;
	state->scale = scale;
	state->unpack = unpack_line;
	state->src_stride = src_stride;
	state->dst_stride = dst_stride;

	return fz_new_stream(ctx, state, unpack_next, unpack_drop);
}
