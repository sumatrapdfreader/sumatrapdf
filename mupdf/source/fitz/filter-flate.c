#include "mupdf/fitz.h"

#include <zlib.h>

typedef struct fz_flate_s fz_flate;

struct fz_flate_s
{
	fz_stream *chain;
	z_stream z;
	unsigned char buffer[4096];
};

static void *zalloc(void *opaque, unsigned int items, unsigned int size)
{
	return fz_malloc_array_no_throw(opaque, items, size);
}

static void zfree(void *opaque, void *ptr)
{
	fz_free(opaque, ptr);
}

static int
next_flated(fz_stream *stm, int required)
{
	fz_flate *state = stm->state;
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
		zp->avail_in = fz_available(chain, 1);
		zp->next_in = chain->rp;

		code = inflate(zp, Z_SYNC_FLUSH);

		chain->rp = chain->wp - zp->avail_in;

		if (code == Z_STREAM_END)
		{
			break;
		}
		else if (code == Z_BUF_ERROR)
		{
			fz_warn(stm->ctx, "premature end of data in flate filter");
			break;
		}
		else if (code == Z_DATA_ERROR && zp->avail_in == 0)
		{
			fz_warn(stm->ctx, "ignoring zlib error: %s", zp->msg);
			break;
		}
		else if (code == Z_DATA_ERROR && !strcmp(zp->msg, "incorrect data check"))
		{
			fz_warn(stm->ctx, "ignoring zlib error: %s", zp->msg);
			chain->rp = chain->wp;
			break;
		}
		else if (code != Z_OK)
		{
			fz_throw(stm->ctx, FZ_ERROR_GENERIC, "zlib error: %s", zp->msg);
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
	fz_flate *state = (fz_flate *)state_;
	int code;

	code = inflateEnd(&state->z);
	if (code != Z_OK)
		fz_warn(ctx, "zlib error: inflateEnd: %s", state->z.msg);

	fz_close(state->chain);
	fz_free(ctx, state);
}

static fz_stream *
rebind_flated(fz_stream *s)
{
	fz_flate *state = s->state;
	return state->chain;
}

fz_stream *
fz_open_flated(fz_stream *chain)
{
	fz_flate *state = NULL;
	int code = Z_OK;
	fz_context *ctx = chain->ctx;

	fz_var(code);
	fz_var(state);

	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, fz_flate);
		state->chain = chain;

		state->z.zalloc = zalloc;
		state->z.zfree = zfree;
		state->z.opaque = ctx;
		state->z.next_in = NULL;
		state->z.avail_in = 0;

		code = inflateInit(&state->z);
		if (code != Z_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "zlib error: inflateInit: %s", state->z.msg);
	}
	fz_catch(ctx)
	{
		if (state && code == Z_OK)
			inflateEnd(&state->z);
		fz_free(ctx, state);
		fz_close(chain);
		fz_rethrow(ctx);
	}
	return fz_new_stream(ctx, state, next_flated, close_flated, rebind_flated);
}
