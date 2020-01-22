#include "mupdf/fitz.h"

#include <zlib.h>

/*
	Compression and other filtering outputs.

	These outputs write encoded data to another output. Create a filter
	output with the destination, write to the filter, then close and drop
	it when you're done. These can also be chained together, for example to
	write ASCII Hex encoded, Deflate compressed, and RC4 encrypted data to
	a buffer output.

	Output streams don't use reference counting, so make sure to close all
	of the filters in the reverse order of creation so that data is flushed
	properly.
*/

struct ahx
{
	fz_output *chain;
	int column;
};

static void ahx_write(fz_context *ctx, void *opaque, const void *data, size_t n)
{
	static const char tohex[16] = "0123456789ABCDEF";
	struct ahx *state = opaque;
	const unsigned char *p = data;
	while (n-- > 0)
	{
		int c = *p++;
		fz_write_byte(ctx, state->chain, tohex[(c>>4) & 15]);
		fz_write_byte(ctx, state->chain, tohex[(c) & 15]);
		state->column += 2;
		if (state->column == 64)
		{
			fz_write_byte(ctx, state->chain, '\n');
			state->column = 0;
		}
	}
}

static void ahx_close(fz_context *ctx, void *opaque)
{
	struct ahx *state = opaque;
	fz_write_byte(ctx, state->chain, '>');
}

static void ahx_drop(fz_context *ctx, void *opaque)
{
	struct ahx *state = opaque;
	fz_free(ctx, state);
}

fz_output *
fz_new_asciihex_output(fz_context *ctx, fz_output *chain)
{
	struct ahx *state = fz_malloc_struct(ctx, struct ahx);
	state->chain = chain;
	state->column = 0;
	return fz_new_output(ctx, 512, state, ahx_write, ahx_close, ahx_drop);
}

struct a85
{
	fz_output *chain;
	int column;
	unsigned int word, n;
};

static void a85_flush(fz_context *ctx, struct a85 *state)
{
	unsigned int v1, v2, v3, v4, v5;

	v5 = state->word;
	v4 = v5 / 85;
	v3 = v4 / 85;
	v2 = v3 / 85;
	v1 = v2 / 85;

	if (state->column >= 70)
	{
		fz_write_byte(ctx, state->chain, '\n');
		state->column = 0;
	}

	if (state->n == 4)
	{
		if (state->word == 0)
		{
			fz_write_byte(ctx, state->chain, 'z');
			state->column += 1;
		}
		else
		{
			fz_write_byte(ctx, state->chain, (v1 % 85) + '!');
			fz_write_byte(ctx, state->chain, (v2 % 85) + '!');
			fz_write_byte(ctx, state->chain, (v3 % 85) + '!');
			fz_write_byte(ctx, state->chain, (v4 % 85) + '!');
			fz_write_byte(ctx, state->chain, (v5 % 85) + '!');
			state->column += 5;
		}
	}
	else if (state->n == 3)
	{
		fz_write_byte(ctx, state->chain, (v2 % 85) + '!');
		fz_write_byte(ctx, state->chain, (v3 % 85) + '!');
		fz_write_byte(ctx, state->chain, (v4 % 85) + '!');
		fz_write_byte(ctx, state->chain, (v5 % 85) + '!');
		state->column += 4;
	}
	else if (state->n == 2)
	{
		fz_write_byte(ctx, state->chain, (v3 % 85) + '!');
		fz_write_byte(ctx, state->chain, (v4 % 85) + '!');
		fz_write_byte(ctx, state->chain, (v5 % 85) + '!');
		state->column += 3;
	}
	else if (state->n == 1)
	{
		fz_write_byte(ctx, state->chain, (v4 % 85) + '!');
		fz_write_byte(ctx, state->chain, (v5 % 85) + '!');
		state->column += 2;
	}

	state->word = 0;
	state->n = 0;
}

static void a85_write(fz_context *ctx, void *opaque, const void *data, size_t n)
{
	struct a85 *state = opaque;
	const unsigned char *p = data;
	while (n-- > 0)
	{
		unsigned int c = *p++;
		if (state->n == 4)
			a85_flush(ctx, state);
		state->word = (state->word << 8) | c;
		state->n++;
	}
}

static void a85_close(fz_context *ctx, void *opaque)
{
	struct a85 *state = opaque;
	a85_flush(ctx, state);
	fz_write_byte(ctx, state->chain, '~');
	fz_write_byte(ctx, state->chain, '>');
}

static void a85_drop(fz_context *ctx, void *opaque)
{
	struct a85 *state = opaque;
	fz_free(ctx, state);
}

fz_output *
fz_new_ascii85_output(fz_context *ctx, fz_output *chain)
{
	struct a85 *state = fz_malloc_struct(ctx, struct a85);
	state->chain = chain;
	state->column = 0;
	state->word = 0;
	state->n = 0;
	return fz_new_output(ctx, 512, state, a85_write, a85_close, a85_drop);
}

struct rle
{
	fz_output *chain;
	int state;
	int run;
	unsigned char buf[128];
};

enum { ZERO, ONE, DIFF, SAME };

static void rle_flush_same(fz_context *ctx, struct rle *enc)
{
	fz_write_byte(ctx, enc->chain, 257 - enc->run);
	fz_write_byte(ctx, enc->chain, enc->buf[0]);
}

static void rle_flush_diff(fz_context *ctx, struct rle *enc)
{
	fz_write_byte(ctx, enc->chain, enc->run - 1);
	fz_write_data(ctx, enc->chain, enc->buf, enc->run);
}

static void rle_write(fz_context *ctx, void *opaque, const void *data, size_t n)
{
	struct rle *enc = opaque;
	const unsigned char *p = data;
	while (n-- > 0)
	{
		int c = *p++;
		switch (enc->state)
		{
		case ZERO:
			enc->state = ONE;
			enc->run = 1;
			enc->buf[0] = c;
			break;

		case ONE:
			enc->state = DIFF;
			enc->run = 2;
			enc->buf[1] = c;
			break;

		case DIFF:
			/* Max run length */
			if (enc->run == 128)
			{
				rle_flush_diff(ctx, enc);
				enc->state = ONE;
				enc->run = 1;
				enc->buf[0] = c;
			}
			/* Run of three same */
			else if ((enc->run >= 2) && (c == enc->buf[enc->run-1]) && (c == enc->buf[enc->run-2]))
			{
				if (enc->run >= 3) {
					enc->run -= 2; /* skip last two in previous run */
					rle_flush_diff(ctx, enc);
				}
				enc->state = SAME;
				enc->run = 3;
				enc->buf[0] = c;
			}
			else
			{
				enc->buf[enc->run] = c;
				enc->run++;
			}
			break;

		case SAME:
			if ((enc->run == 128) || (c != enc->buf[0]))
			{
				rle_flush_same(ctx, enc);
				enc->state = ONE;
				enc->run = 1;
				enc->buf[0] = c;
			}
			else
			{
				enc->run++;
			}
		}
	}
}

static void rle_close(fz_context *ctx, void *opaque)
{
	struct rle *enc = opaque;
	switch (enc->state)
	{
		case ZERO: break;
		case ONE: rle_flush_diff(ctx, enc); break;
		case DIFF: rle_flush_diff(ctx, enc); break;
		case SAME: rle_flush_same(ctx, enc); break;
	}
	fz_write_byte(ctx, enc->chain, 128);
}

static void rle_drop(fz_context *ctx, void *opaque)
{
	struct rle *enc = opaque;
	fz_free(ctx, enc);
}

fz_output *
fz_new_rle_output(fz_context *ctx, fz_output *chain)
{
	struct rle *enc = fz_malloc_struct(ctx, struct rle);
	enc->chain = chain;
	enc->state = ZERO;
	enc->run = 0;
	return fz_new_output(ctx, 4096, enc, rle_write, rle_close, rle_drop);
}

struct arc4
{
	fz_output *chain;
	fz_arc4 arc4;
};

static void arc4_write(fz_context *ctx, void *opaque, const void *data, size_t n)
{
	struct arc4 *state = opaque;
	const unsigned char *p = data;
	unsigned char buffer[256];
	while (n > 0)
	{
		size_t x = (n > sizeof buffer) ? sizeof buffer : n;
		fz_arc4_encrypt(&state->arc4, buffer, p, x);
		fz_write_data(ctx, state->chain, buffer, x);
		p += x;
		n -= x;
	}
}

static void arc4_drop(fz_context *ctx, void *opaque)
{
	fz_free(ctx, opaque);
}

fz_output *
fz_new_arc4_output(fz_context *ctx, fz_output *chain, unsigned char *key, size_t keylen)
{
	struct arc4 *state = fz_malloc_struct(ctx, struct arc4);
	state->chain = chain;
	fz_arc4_init(&state->arc4, key, keylen);
	return fz_new_output(ctx, 256, state, arc4_write, NULL, arc4_drop);
}

struct deflate
{
	fz_output *chain;
	z_stream z;
};

static void deflate_write(fz_context *ctx, void *opaque, const void *data, size_t n)
{
	struct deflate *state = opaque;
	unsigned char buffer[32 << 10];
	int err;

	state->z.next_in = (Bytef*)data;
	state->z.avail_in = (uInt)n;
	do
	{
		state->z.next_out = buffer;
		state->z.avail_out = sizeof buffer;
		err = deflate(&state->z, Z_NO_FLUSH);
		if (err != Z_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "zlib compression failed: %d", err);
		if (state->z.avail_out > 0)
			fz_write_data(ctx, state->chain, state->z.next_out, state->z.avail_out);
	} while (state->z.avail_out > 0);
}

static void deflate_close(fz_context *ctx, void *opaque)
{
	struct deflate *state = opaque;
	unsigned char buffer[32 << 10];
	int err;

	state->z.next_in = NULL;
	state->z.avail_in = 0;
	do
	{
		state->z.next_out = buffer;
		state->z.avail_out = sizeof buffer;
		err = deflate(&state->z, Z_FINISH);
		if (state->z.avail_out > 0)
			fz_write_data(ctx, state->chain, state->z.next_out, state->z.avail_out);
	} while (err == Z_OK);

	if (err != Z_STREAM_END)
		fz_throw(ctx, FZ_ERROR_GENERIC, "zlib compression failed: %d", err);
}

static void deflate_drop(fz_context *ctx, void *opaque)
{
	struct deflate *state = opaque;
	(void)deflateEnd(&state->z);
	fz_free(ctx, state);
}

fz_output *
fz_new_deflate_output(fz_context *ctx, fz_output *chain, int effort, int raw)
{
	int err;

	struct deflate *state = fz_malloc_struct(ctx, struct deflate);
	state->chain = chain;
	state->z.opaque = ctx;
	state->z.zalloc = fz_zlib_alloc;
	state->z.zfree = fz_zlib_free;
	err = deflateInit2(&state->z, effort, Z_DEFLATED, raw ? -15 : 15, 8, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
	{
		(void)deflateEnd(&state->z);
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_GENERIC, "zlib deflateInit2 failed: %d", err);
	}
	return fz_new_output(ctx, 8192, state, deflate_write, deflate_close, deflate_drop);
}
