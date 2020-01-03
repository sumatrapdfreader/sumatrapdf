#include "mupdf/fitz.h"

/* 4bit greyscale Thunderscan decoding */

typedef struct fz_thunder_s fz_thunder;

struct fz_thunder_s
{
	fz_stream *chain;
	int lastpixel;
	int run;
	int pixel;

	int len;
	unsigned char *buffer;
};

static int
next_thunder(fz_context *ctx, fz_stream *stm, size_t max)
{
	fz_thunder *state = stm->state;
	unsigned char *p = state->buffer;
	unsigned char *ep;
	int c, v, i, pixels, index;

	if (max > (size_t)state->len)
		max = (size_t)state->len;

	ep = p + max;

	c = 0;
	while (p < ep && c >= 0)
	{
		pixels = 0;
		v = 0;

		while (pixels < 2)
		{
			if (state->run > 0)
			{
				v <<= 4;
				v |= state->pixel & 0xf;
				state->pixel >>= 4;
				state->run--;
				pixels++;

				if (state->run > 2)
					state->pixel |= ((state->pixel >> 4) & 0xf) << 8;
			}
			else
			{
				c = fz_read_byte(ctx, state->chain);
				if (c < 0)
					break;

				switch ((c >> 6) & 0x3)
				{
				case 0x0: /* run of pixels identical to last pixel */
					state->run = c;
					state->pixel = (state->lastpixel << 8) | (state->lastpixel << 4) | (state->lastpixel << 0);
					break;

				case 0x1: /* three pixels with 2bit deltas to last pixel */
					for (i = 0; i < 3; i++)
					{
						static const int deltas[] = { 0, 1, 0, -1 };
						index = (c >> (4 - i * 2)) & 0x3;
						if (index == 2)
							continue;

						state->lastpixel = (state->lastpixel + deltas[index]) & 0xf;
						state->pixel <<= 4;
						state->pixel |= state->lastpixel;
						state->run++;
					}
					break;

				case 0x2: /* two pixels with 3bit deltas to last pixel */
					for (i = 0; i < 2; i++)
					{
						static const int deltas[] = { 0, 1, 2, 3, 0, -3, -2, -1 };
						index = (c >> (3 - i * 3)) & 0x7;
						if (index == 4)
							continue;

						state->lastpixel = (state->lastpixel + deltas[index]) & 0xf;
						state->pixel <<= 4;
						state->pixel |= state->lastpixel;
						state->run++;
					}
					break;

				case 0x3: /* a single raw 4bit pixel */
					state->run = 1;
					state->pixel = c & 0xf;
					state->lastpixel = state->pixel & 0xf;
					break;
				}
			}
		}

		if (pixels)
			*p++ = v;
	}

	stm->rp = state->buffer;
	stm->wp = p;
	stm->pos += p - state->buffer;

	if (stm->rp != p)
		return *stm->rp++;
	return EOF;
}

static void
close_thunder(fz_context *ctx, void *state_)
{
	fz_thunder *state = (fz_thunder *)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state->buffer);
	fz_free(ctx, state);
}

fz_stream *
fz_open_thunder(fz_context *ctx, fz_stream *chain, int w)
{
	fz_thunder *state = fz_malloc_struct(ctx, fz_thunder);
	fz_try(ctx)
	{
		state->run = 0;
		state->pixel = 0;
		state->lastpixel = 0;
		state->len = w / 2;
		state->buffer = Memento_label(fz_malloc(ctx, state->len), "thunder_buffer");
		state->chain = fz_keep_stream(ctx, chain);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}
	return fz_new_stream(ctx, state, next_thunder, close_thunder);
}
