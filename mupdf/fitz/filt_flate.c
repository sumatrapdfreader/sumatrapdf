#include "fitz.h"

#include <zlib.h>

typedef struct fz_flate_s fz_flate;

struct fz_flate_s
{
	fz_stream *chain;
	z_stream z;
};

static void *zmalloc(void *opaque, unsigned int items, unsigned int size)
{
	return fz_calloc(items, size);
}

static void zfree(void *opaque, void *ptr)
{
	fz_free(ptr);
}

static int
readflated(fz_stream *stm, unsigned char *outbuf, int outlen)
{
	fz_flate *state = stm->state;
	fz_stream *chain = state->chain;
	z_streamp zp = &state->z;
	int code;

	if (chain->rp == chain->wp)
		fz_fillbuffer(chain);

	zp->next_in = chain->rp;
	zp->avail_in = chain->wp - chain->rp;
	zp->next_out = outbuf;
	zp->avail_out = outlen;

	code = inflate(zp, Z_SYNC_FLUSH);

	chain->rp = chain->wp - zp->avail_in;

	if (code == Z_STREAM_END || code == Z_OK)
	{
		return outlen - zp->avail_out;
	}
	else if (code == Z_BUF_ERROR)
	{
		fz_warn("premature end of data in flate filter");
		return outlen - zp->avail_out;
	}
	else if (code == Z_DATA_ERROR && zp->avail_in == 0)
	{
		fz_warn("ignoring zlib error: %s", zp->msg);
		return outlen - zp->avail_out;
	}
	else
	{
		return fz_throw("zlib error: %s", zp->msg);
	}
}

static void
closeflated(fz_stream *stm)
{
	fz_flate *state = stm->state;
	int code;

	code = inflateEnd(&state->z);
	if (code != Z_OK)
		fz_warn("zlib error: inflateEnd: %s", state->z.msg);

	fz_close(state->chain);
	fz_free(state);
}

fz_stream *
fz_openflated(fz_stream *chain)
{
	fz_flate *state;
	int code;

	state = fz_malloc(sizeof(fz_flate));
	state->chain = chain;

	state->z.zalloc = zmalloc;
	state->z.zfree = zfree;
	state->z.opaque = nil;
	state->z.next_in = nil;
	state->z.avail_in = 0;

	code = inflateInit(&state->z);
	if (code != Z_OK)
		fz_warn("zlib error: inflateInit: %s", state->z.msg);

	return fz_newstream(state, readflated, closeflated);
}
