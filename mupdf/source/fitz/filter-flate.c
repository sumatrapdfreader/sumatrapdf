#include "mupdf/fitz.h"

#include <zlib.h>

#include <string.h>

typedef struct fz_inflate_state_s fz_inflate_state;

struct fz_inflate_state_s
{
	fz_stream *chain;
	z_stream z;
	unsigned char buffer[4096];
};

void *fz_zlib_alloc(void *ctx, unsigned int items, unsigned int size)
{
	return Memento_label(fz_malloc_no_throw(ctx, items * size), "zlib_alloc");
}

void fz_zlib_free(void *ctx, void *ptr)
{
	fz_free(ctx, ptr);
}

static int
next_flated(fz_context *ctx, fz_stream *stm, size_t required)
{
	fz_inflate_state *state = stm->state;
	fz_stream *chain = state->chain;
	z_streamp zp = &state->z;
	int code;
	unsigned char *outbuf = state->buffer;
	int outlen = sizeof(state->buffer);

	if (stm->eof)
		return EOF;

	zp->next_out = outbuf;
	zp->avail_out = outlen;

	while (zp->avail_out > 0)
	{
		zp->avail_in = (uInt)fz_available(ctx, chain, 1);
		zp->next_in = chain->rp;

		code = inflate(zp, Z_SYNC_FLUSH);

		chain->rp = chain->wp - zp->avail_in;

		if (code == Z_STREAM_END)
		{
			break;
		}
		else if (code == Z_BUF_ERROR)
		{
			fz_warn(ctx, "premature end of data in flate filter");
			break;
		}
		else if (code == Z_DATA_ERROR && zp->avail_in == 0)
		{
			fz_warn(ctx, "ignoring zlib error: %s", zp->msg);
			break;
		}
		else if (code == Z_DATA_ERROR && !strcmp(zp->msg, "incorrect data check"))
		{
			fz_warn(ctx, "ignoring zlib error: %s", zp->msg);
			chain->rp = chain->wp;
			break;
		}
		else if (code != Z_OK)
		{
			fz_throw(ctx, FZ_ERROR_GENERIC, "zlib error: %s", zp->msg);
		}
	}

	stm->rp = state->buffer;
	stm->wp = state->buffer + outlen - zp->avail_out;
	stm->pos += outlen - zp->avail_out;
	if (stm->rp == stm->wp)
	{
		stm->eof = 1;
		return EOF;
	}
	return *stm->rp++;
}

static void
close_flated(fz_context *ctx, void *state_)
{
	fz_inflate_state *state = (fz_inflate_state *)state_;
	int code;

	code = inflateEnd(&state->z);
	if (code != Z_OK)
		fz_warn(ctx, "zlib error: inflateEnd: %s", state->z.msg);

	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

fz_stream *
fz_open_flated(fz_context *ctx, fz_stream *chain, int window_bits)
{
	fz_inflate_state *state;
	int code;

	state = fz_malloc_struct(ctx, fz_inflate_state);
	state->z.zalloc = fz_zlib_alloc;
	state->z.zfree = fz_zlib_free;
	state->z.opaque = ctx;
	state->z.next_in = NULL;
	state->z.avail_in = 0;

	code = inflateInit2(&state->z, window_bits);
	if (code != Z_OK)
	{
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_GENERIC, "zlib error: inflateInit2 failed");
	}

	state->chain = fz_keep_stream(ctx, chain);

	return fz_new_stream(ctx, state, next_flated, close_flated);
}
