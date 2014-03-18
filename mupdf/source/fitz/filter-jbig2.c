#include "mupdf/fitz.h"

#include <jbig2.h>

typedef struct fz_jbig2d_s fz_jbig2d;

struct fz_jbig2_globals_s
{
	fz_storable storable;
	Jbig2GlobalCtx *gctx;
};

struct fz_jbig2d_s
{
	fz_stream *chain;
	Jbig2Ctx *ctx;
	fz_jbig2_globals *gctx;
	Jbig2Image *page;
	int idx;
	unsigned char buffer[4096];
};

static void
fz_drop_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals)
{
	fz_drop_storable(ctx, &globals->storable);
}

static void
close_jbig2d(fz_context *ctx, void *state_)
{
	fz_jbig2d *state = (fz_jbig2d *)state_;
	if (state->page)
		jbig2_release_page(state->ctx, state->page);
	if (state->gctx)
		fz_drop_jbig2_globals(ctx, state->gctx);
	jbig2_ctx_free(state->ctx);
	fz_close(state->chain);
	fz_free(ctx, state);
}

static int
next_jbig2d(fz_stream *stm, int len)
{
	fz_jbig2d *state = stm->state;
	unsigned char tmp[4096];
	unsigned char *buf = state->buffer;
	unsigned char *p = buf;
	unsigned char *ep;
	unsigned char *s;
	int x, w, n;

	if (len > sizeof(state->buffer))
		len = sizeof(state->buffer);
	ep = buf + len;

	if (!state->page)
	{
		while (1)
		{
			n = fz_read(state->chain, tmp, sizeof tmp);
			if (n == 0)
				break;
			jbig2_data_in(state->ctx, tmp, n);
		}

		jbig2_complete_page(state->ctx);

		state->page = jbig2_page_out(state->ctx);
		if (!state->page)
			fz_throw(stm->ctx, FZ_ERROR_GENERIC, "jbig2_page_out failed");
	}

	s = state->page->data;
	w = state->page->height * state->page->stride;
	x = state->idx;
	while (p < ep && x < w)
		*p++ = s[x++] ^ 0xff;
	state->idx = x;

	stm->rp = buf;
	stm->wp = p;
	if (p == buf)
		return EOF;
	stm->pos += p - buf;
	return *stm->rp++;
}

static fz_stream *
rebind_jbig2d(fz_stream *s)
{
	fz_jbig2d *state = s->state;
	return state->chain;
}

static int
error_callback(void *data, const char *msg, Jbig2Severity severity, int32_t seg_idx)
{
	fz_context *ctx = data;
	if (severity == JBIG2_SEVERITY_FATAL)
		fz_warn(ctx, "jbig2dec error: %s (segment %d)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_WARNING)
		fz_warn(ctx, "jbig2dec warning: %s (segment %d)", msg, seg_idx);
	return 0;
}

fz_jbig2_globals *
fz_load_jbig2_globals(fz_context *ctx, unsigned char *data, int size)
{
	fz_jbig2_globals *globals = fz_malloc_struct(ctx, fz_jbig2_globals);

	Jbig2Ctx *jctx = jbig2_ctx_new(NULL, JBIG2_OPTIONS_EMBEDDED, NULL, error_callback, ctx);
	jbig2_data_in(jctx, data, size);

	FZ_INIT_STORABLE(globals, 1, fz_free_jbig2_globals_imp);
	globals->gctx = jbig2_make_global_ctx(jctx);

	return globals;
}

void
fz_free_jbig2_globals_imp(fz_context *ctx, fz_storable *globals_)
{
	fz_jbig2_globals *globals = (fz_jbig2_globals *)globals_;
	jbig2_global_ctx_free(globals->gctx);
	fz_free(ctx, globals);
}

fz_stream *
fz_open_jbig2d(fz_stream *chain, fz_jbig2_globals *globals)
{
	fz_jbig2d *state = NULL;
	fz_context *ctx = chain->ctx;

	fz_var(state);

	fz_try(ctx)
	{
		state = fz_malloc_struct(chain->ctx, fz_jbig2d);
		state->ctx = NULL;
		state->gctx = globals;
		state->chain = chain;
		state->ctx = jbig2_ctx_new(NULL, JBIG2_OPTIONS_EMBEDDED, globals ? globals->gctx : NULL, error_callback, ctx);
		state->page = NULL;
		state->idx = 0;
	}
	fz_catch(ctx)
	{
		if (state)
		{
			fz_drop_jbig2_globals(ctx, state->gctx);
			if (state->ctx)
				jbig2_ctx_free(state->ctx);
		}
		fz_free(ctx, state);
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_jbig2d, close_jbig2d, rebind_jbig2d);
}
