#include "mupdf/fitz.h"

#include <zlib.h>

typedef struct fz_leech_s fz_leech;

struct fz_leech_s
{
	fz_stream *chain;
	fz_buffer *buffer;
};

static int
next_leech(fz_stream *stm, int max)
{
	fz_leech *state = stm->state;
	fz_buffer *buffer = state->buffer;
	int n = fz_available(state->chain, max);

	if (n > max)
		n = max;

	while (buffer->cap < buffer->len + n)
	{
		fz_grow_buffer(stm->ctx, state->buffer);
	}
	memcpy(buffer->data + buffer->len, state->chain->rp, n);
	stm->rp = buffer->data + buffer->len;
	stm->wp = buffer->data + buffer->len + n;
	state->chain->rp += n;
	buffer->len += n;

	if (n == 0)
		return EOF;
	return *stm->rp++;
}

static void
close_leech(fz_context *ctx, void *state_)
{
	fz_leech *state = (fz_leech *)state_;

	fz_close(state->chain);
	fz_free(ctx, state);
}

static fz_stream *
rebind_leech(fz_stream *s)
{
	fz_leech *state = s->state;
	return state->chain;
}

fz_stream *
fz_open_leecher(fz_stream *chain, fz_buffer *buffer)
{
	fz_leech *state = NULL;
	fz_context *ctx = chain->ctx;

	fz_var(state);

	fz_try(ctx)
	{
		state = fz_malloc_struct(ctx, fz_leech);
		state->chain = chain;
		state->buffer = buffer;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_close(chain);
		fz_rethrow(ctx);
	}
	return fz_new_stream(ctx, state, next_leech, close_leech, rebind_leech);
}
