#include "fitz-imp.h"

#ifdef HAVE_LURATECH

#include <string.h>

#include <ldf_jb2.h>

typedef struct fz_jbig2d_s fz_jbig2d;

struct fz_jbig2_globals_s
{
	fz_storable storable;
	fz_buffer *buf;
};

struct fz_jbig2d_s
{
	fz_stream *chain;
	fz_context *ctx;
	fz_jbig2_globals *gctx;
	JB2_Handle_Document doc;
	unsigned long width;
	unsigned long height;
	int stride;
	fz_buffer *input;
	unsigned char *output;
	int idx;
};

fz_jbig2_globals *
fz_keep_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals)
{
	return fz_keep_storable(ctx, &globals->storable);
}

void
fz_drop_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals)
{
	fz_drop_storable(ctx, &globals->storable);
}

static void
close_jbig2d(fz_context *ctx, void *state_)
{
	fz_jbig2d *state = state_;
	fz_free(ctx, state->output);
	fz_drop_jbig2_globals(ctx, state->gctx);
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

static void * JB2_Callback
jbig2_alloc(unsigned long size, void *userdata)
{
	fz_jbig2d *state = userdata;
	return Memento_label(fz_malloc(state->ctx, size), "jbig2_alloc");
}

static JB2_Error JB2_Callback
jbig2_free(void *ptr, void *userdata)
{
	fz_jbig2d *state = userdata;
	fz_free(state->ctx, ptr);
	return cJB2_Error_OK;
}

static void JB2_Callback
jbig2_message(const char *msg, JB2_Message_Level level, void *userdata)
{
	fz_jbig2d *state = userdata;

	if (msg != NULL && msg[0] != '\0')
		switch (level)
		{
		case cJB2_Message_Information:
#ifdef JBIG2_DEBUG
			fz_warn(state->ctx, "luratech jbig2 info: %s", msg);
#endif
			break;
		case cJB2_Message_Warning:
			fz_warn(state->ctx, "luratech jbig2 warning: %s", msg);
			break;
		case cJB2_Message_Error:
			fz_warn(state->ctx, "luratech jbig2 error: %s", msg);
			break;
		default:
			fz_warn(state->ctx, "luratech jbig2 message: %s", msg);
			break;
		}
}

static JB2_Size_T JB2_Callback
jbig2_read(unsigned char *buf, JB2_Size_T offset, JB2_Size_T size, void *userdata)
{
	fz_jbig2d *state = userdata;
	size_t available;

	/* globals data */
	if (state->gctx && offset < state->gctx->buf->len)
	{
		available = fz_minz(state->gctx->buf->len - offset, size);
		memcpy(buf, state->gctx->buf->data + offset, available);
		return (JB2_Size_T)available;
	}

	/* image data */
	if (state->gctx)
		offset -= (JB2_Size_T)state->gctx->buf->len;
	if (state->input->len <= offset)
		return 0;
	available = fz_minz(state->input->len - offset, size);
	memcpy(buf, state->input->data + offset, available);
	return (JB2_Size_T)available;
}

static JB2_Error JB2_Callback
jbig2_write(unsigned char *buf, unsigned long row, unsigned long width, unsigned long bpp, void *userdata)
{
	fz_jbig2d *state = userdata;
	int stride = (width + 7) >> 3;
	unsigned char *dp = state->output + row * stride;

	if (row >= state->height)
	{
		fz_warn(state->ctx, "row %lu outside of image", row);
		return cJB2_Error_OK;
	}

	while (stride--)
		*(dp++) = *(buf++) ^ 0xff;

	return cJB2_Error_OK;
}

static int
next_jbig2d(fz_context *ctx, fz_stream *stm, size_t len)
{
	fz_jbig2d *state = stm->state;
	JB2_Error err;
	JB2_Scaling_Factor scale = {1, 1};
	JB2_Rect rect = {0, 0, 0, 0};

	if (!state->output)
	{
		fz_try(ctx)
		{
			state->input = fz_read_all(state->ctx, state->chain, 0);

			err = JB2_Document_Start(&state->doc,
					jbig2_alloc, state,
					jbig2_free, state,
					jbig2_read, state,
					jbig2_message, state);
			if (err != cJB2_Error_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open image: %d", (int) err);

#if defined(JB2_LICENSE_NUM_1) && defined(JB2_LICENSE_NUM_2)
			err = JB2_Document_Set_License(doc, JB2_LICENSE_NUM_1, JB2_LICENSE_NUM_2);
			if (err != cJB2_Error_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot set license: %d", (int) err);
#endif

			err = JB2_Document_Set_Page(state->doc, 0);
			if (err != cJB2_Error_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot select page: %d", (int) err);

			err = JB2_Document_Get_Property(state->doc, cJB2_Prop_Page_Width, &state->width);
			if (err != cJB2_Error_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot get page width: %d", (int) err);
			err = JB2_Document_Get_Property(state->doc, cJB2_Prop_Page_Height, &state->height);
			if (err != cJB2_Error_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot get page height: %d", (int) err);

			state->stride = (state->width + 7) >> 3;
			stm->pos = state->stride * state->height;
			state->output = Memento_label(fz_malloc(state->ctx, stm->pos), "jbig2_output");
			stm->rp = state->output;
			stm->wp = state->output;

			err = JB2_Document_Decompress_Page(state->doc, scale, rect, jbig2_write, state);
			if (err != cJB2_Error_OK)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot decode image: %d", (int) err);

			/* update wp last. rp == wp upon errors above, causing
			subsequent EOFs in comparison below */
			stm->wp += stm->pos;
		}
		fz_always(ctx)
		{
			fz_drop_buffer(ctx, state->input);
			JB2_Document_End(&state->doc);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}

	if (stm->rp == stm->wp)
		return EOF;
	return *stm->rp++;
}

fz_jbig2_globals *
fz_load_jbig2_globals(fz_context *ctx, fz_buffer *buf)
{
	fz_jbig2_globals *globals = fz_malloc_struct(ctx, fz_jbig2_globals);

	FZ_INIT_STORABLE(globals, 1, fz_drop_jbig2_globals_imp);
	globals->buf = fz_keep_buffer(ctx, buf);

	return globals;
}

void
fz_drop_jbig2_globals_imp(fz_context *ctx, fz_storable *globals_)
{
	fz_jbig2_globals *globals = (fz_jbig2_globals *)globals_;
	fz_drop_buffer(ctx, globals->buf);
	fz_free(ctx, globals);
}

fz_stream *
fz_open_jbig2d(fz_context *ctx, fz_stream *chain, fz_jbig2_globals *globals)
{
	fz_jbig2d *state = NULL;

	state = fz_malloc_struct(ctx, fz_jbig2d);
	state->ctx = ctx;
	state->gctx = fz_keep_jbig2_globals(ctx, globals);
	state->chain = fz_keep_stream(ctx, chain);
	state->idx = 0;
	state->output = NULL;
	state->doc = NULL;

	return fz_new_stream(ctx, state, next_jbig2d, close_jbig2d);
}

#else /* HAVE_LURATECH */

#include <jbig2.h>

typedef struct fz_jbig2d_s fz_jbig2d;

struct fz_jbig2_alloc_s
{
	Jbig2Allocator alloc;
	fz_context *ctx;
};

struct fz_jbig2_globals_s
{
	fz_storable storable;
	Jbig2GlobalCtx *gctx;
	struct fz_jbig2_alloc_s alloc;
};

struct fz_jbig2d_s
{
	fz_stream *chain;
	Jbig2Ctx *ctx;
	struct fz_jbig2_alloc_s alloc;
	fz_jbig2_globals *gctx;
	Jbig2Image *page;
	int idx;
	unsigned char buffer[4096];
};

fz_jbig2_globals *
fz_keep_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals)
{
	return fz_keep_storable(ctx, &globals->storable);
}

void
fz_drop_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals)
{
	fz_drop_storable(ctx, &globals->storable);
}

static void
close_jbig2d(fz_context *ctx, void *state_)
{
	fz_jbig2d *state = state_;
	if (state->page)
		jbig2_release_page(state->ctx, state->page);
	fz_drop_jbig2_globals(ctx, state->gctx);
	jbig2_ctx_free(state->ctx);
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state);
}

static int
next_jbig2d(fz_context *ctx, fz_stream *stm, size_t len)
{
	fz_jbig2d *state = stm->state;
	unsigned char tmp[4096];
	unsigned char *buf = state->buffer;
	unsigned char *p = buf;
	unsigned char *ep;
	unsigned char *s;
	int x, w;
	size_t n;

	if (len > sizeof(state->buffer))
		len = sizeof(state->buffer);
	ep = buf + len;

	if (!state->page)
	{
		while (1)
		{
			n = fz_read(ctx, state->chain, tmp, sizeof tmp);
			if (n == 0)
				break;

			if (jbig2_data_in(state->ctx, tmp, n) < 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot decode jbig2 image");
		}

		if (jbig2_complete_page(state->ctx) < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot complete jbig2 image");

		state->page = jbig2_page_out(state->ctx);
		if (!state->page)
			fz_throw(ctx, FZ_ERROR_GENERIC, "no jbig2 image decoded");
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

static void
error_callback(void *data, const char *msg, Jbig2Severity severity, int32_t seg_idx)
{
	fz_context *ctx = data;
	if (severity == JBIG2_SEVERITY_FATAL)
		fz_warn(ctx, "jbig2dec error: %s (segment %d)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_WARNING)
		fz_warn(ctx, "jbig2dec warning: %s (segment %d)", msg, seg_idx);
#ifdef JBIG2_DEBUG
	else if (severity == JBIG2_SEVERITY_INFO)
		fz_warn(ctx, "jbig2dec info: %s (segment %d)", msg, seg_idx);
	else if (severity == JBIG2_SEVERITY_DEBUG)
		fz_warn(ctx, "jbig2dec debug: %s (segment %d)", msg, seg_idx);
#endif
}

static void *fz_jbig2_alloc(Jbig2Allocator *allocator, size_t size)
{
	fz_context *ctx = ((struct fz_jbig2_alloc_s *) allocator)->ctx;
	return Memento_label(fz_malloc_no_throw(ctx, size), "jbig2_alloc");
}

static void fz_jbig2_free(Jbig2Allocator *allocator, void *p)
{
	fz_context *ctx = ((struct fz_jbig2_alloc_s *) allocator)->ctx;
	fz_free(ctx, p);
}

static void *fz_jbig2_realloc(Jbig2Allocator *allocator, void *p, size_t size)
{
	fz_context *ctx = ((struct fz_jbig2_alloc_s *) allocator)->ctx;
	if (size == 0)
	{
		fz_free(ctx, p);
		return NULL;
	}
	if (p == NULL)
		return Memento_label(fz_malloc(ctx, size), "jbig2_realloc");
	return Memento_label(fz_realloc_no_throw(ctx, p, size), "jbig2_realloc");
}

fz_jbig2_globals *
fz_load_jbig2_globals(fz_context *ctx, fz_buffer *buf)
{
	fz_jbig2_globals *globals = fz_malloc_struct(ctx, fz_jbig2_globals);
	Jbig2Ctx *jctx;

	globals->alloc.ctx = ctx;
	globals->alloc.alloc.alloc = fz_jbig2_alloc;
	globals->alloc.alloc.free = fz_jbig2_free;
	globals->alloc.alloc.realloc = fz_jbig2_realloc;

	jctx = jbig2_ctx_new((Jbig2Allocator *) &globals->alloc, JBIG2_OPTIONS_EMBEDDED, NULL, error_callback, ctx);
	if (!jctx)
	{
		fz_free(ctx, globals);
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot allocate jbig2 globals context");
	}

	if (jbig2_data_in(jctx, buf->data, buf->len) < 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot decode jbig2 globals");

	FZ_INIT_STORABLE(globals, 1, fz_drop_jbig2_globals_imp);
	globals->gctx = jbig2_make_global_ctx(jctx);

	return globals;
}

void
fz_drop_jbig2_globals_imp(fz_context *ctx, fz_storable *globals_)
{
	fz_jbig2_globals *globals = (fz_jbig2_globals *)globals_;
	jbig2_global_ctx_free(globals->gctx);
	fz_free(ctx, globals);
}

fz_stream *
fz_open_jbig2d(fz_context *ctx, fz_stream *chain, fz_jbig2_globals *globals)
{
	fz_jbig2d *state = NULL;

	fz_var(state);

	state = fz_malloc_struct(ctx, fz_jbig2d);
	state->gctx = fz_keep_jbig2_globals(ctx, globals);
	state->alloc.ctx = ctx;
	state->alloc.alloc.alloc = fz_jbig2_alloc;
	state->alloc.alloc.free = fz_jbig2_free;
	state->alloc.alloc.realloc = fz_jbig2_realloc;

	state->ctx = jbig2_ctx_new((Jbig2Allocator *) &state->alloc, JBIG2_OPTIONS_EMBEDDED, globals ? globals->gctx : NULL, error_callback, ctx);
	if (state->ctx == NULL)
	{
		fz_drop_jbig2_globals(ctx, state->gctx);
		fz_free(ctx, state);
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot allocate jbig2 context");
	}

	state->page = NULL;
	state->idx = 0;
	state->chain = fz_keep_stream(ctx, chain);

	return fz_new_stream(ctx, state, next_jbig2d, close_jbig2d);
}

#endif /* HAVE_LURATECH */
