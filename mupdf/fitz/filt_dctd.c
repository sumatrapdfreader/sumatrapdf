#include "fitz-internal.h"

#include <jpeglib.h>
#include <setjmp.h>

typedef struct fz_dctd_s fz_dctd;

struct fz_dctd_s
{
	fz_stream *chain;
	fz_context *ctx;
	int color_transform;
	int init;
	int stride;
	int factor;
	unsigned char *scanline;
	unsigned char *rp, *wp;
	struct jpeg_decompress_struct cinfo;
	struct jpeg_source_mgr srcmgr;
	struct jpeg_error_mgr errmgr;
	jmp_buf jb;
	char msg[JMSG_LENGTH_MAX];
};

static void error_exit(j_common_ptr cinfo)
{
	fz_dctd *state = cinfo->client_data;
	cinfo->err->format_message(cinfo, state->msg);
	longjmp(state->jb, 1);
}

static void init_source(j_decompress_ptr cinfo)
{
	/* nothing to do */
}

static void term_source(j_decompress_ptr cinfo)
{
	/* nothing to do */
}

static boolean fill_input_buffer(j_decompress_ptr cinfo)
{
	struct jpeg_source_mgr *src = cinfo->src;
	fz_dctd *state = cinfo->client_data;
	fz_stream *chain = state->chain;
	fz_context *ctx = chain->ctx;

	chain->rp = chain->wp;
	fz_try(ctx)
	{
		fz_fill_buffer(chain);
	}
	fz_catch(ctx)
	{
		return 0;
	}
	src->next_input_byte = chain->rp;
	src->bytes_in_buffer = chain->wp - chain->rp;

	if (src->bytes_in_buffer == 0)
	{
		static unsigned char eoi[2] = { 0xFF, JPEG_EOI };
		fz_warn(state->ctx, "premature end of file in jpeg");
		src->next_input_byte = eoi;
		src->bytes_in_buffer = 2;
	}

	return 1;
}

static void skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	struct jpeg_source_mgr *src = cinfo->src;
	if (num_bytes > 0)
	{
		while ((size_t)num_bytes > src->bytes_in_buffer)
		{
			num_bytes -= src->bytes_in_buffer;
			(void) src->fill_input_buffer(cinfo);
		}
		src->next_input_byte += num_bytes;
		src->bytes_in_buffer -= num_bytes;
	}
}

static int
read_dctd(fz_stream *stm, unsigned char *buf, int len)
{
	fz_dctd *state = stm->state;
	j_decompress_ptr cinfo = &state->cinfo;
	unsigned char *p = buf;
	unsigned char *ep = buf + len;

	if (setjmp(state->jb))
	{
		if (cinfo->src)
			state->chain->rp = state->chain->wp - cinfo->src->bytes_in_buffer;
		fz_throw(stm->ctx, "jpeg error: %s", state->msg);
	}

	if (!state->init)
	{
		cinfo->client_data = state;
		cinfo->err = &state->errmgr;
		jpeg_std_error(cinfo->err);
		cinfo->err->error_exit = error_exit;
		jpeg_create_decompress(cinfo);

		cinfo->src = &state->srcmgr;
		cinfo->src->init_source = init_source;
		cinfo->src->fill_input_buffer = fill_input_buffer;
		cinfo->src->skip_input_data = skip_input_data;
		cinfo->src->resync_to_restart = jpeg_resync_to_restart;
		cinfo->src->term_source = term_source;
		cinfo->src->next_input_byte = state->chain->rp;
		cinfo->src->bytes_in_buffer = state->chain->wp - state->chain->rp;

		jpeg_read_header(cinfo, 1);

		/* speed up jpeg decoding a bit */
		cinfo->dct_method = JDCT_FASTEST;
		cinfo->do_fancy_upsampling = FALSE;

		/* default value if ColorTransform is not set */
		if (state->color_transform == -1)
		{
			if (state->cinfo.num_components == 3)
				state->color_transform = 1;
			else
				state->color_transform = 0;
		}

		if (cinfo->saw_Adobe_marker)
			state->color_transform = cinfo->Adobe_transform;

		/* Guess the input colorspace, and set output colorspace accordingly */
		switch (cinfo->num_components)
		{
		case 3:
			if (state->color_transform)
				cinfo->jpeg_color_space = JCS_YCbCr;
			else
				cinfo->jpeg_color_space = JCS_RGB;
			break;
		case 4:
			if (state->color_transform)
				cinfo->jpeg_color_space = JCS_YCCK;
			else
				cinfo->jpeg_color_space = JCS_CMYK;
			break;
		}

		cinfo->scale_num = 8/state->factor;
		cinfo->scale_denom = 8;

		jpeg_start_decompress(cinfo);

		state->stride = cinfo->output_width * cinfo->output_components;
		state->scanline = fz_malloc(state->ctx, state->stride);
		state->rp = state->scanline;
		state->wp = state->scanline;

		state->init = 1;
	}

	while (state->rp < state->wp && p < ep)
		*p++ = *state->rp++;

	while (p < ep)
	{
		if (cinfo->output_scanline == cinfo->output_height)
			break;

		if (p + state->stride <= ep)
		{
			jpeg_read_scanlines(cinfo, &p, 1);
			p += state->stride;
		}
		else
		{
			jpeg_read_scanlines(cinfo, &state->scanline, 1);
			state->rp = state->scanline;
			state->wp = state->scanline + state->stride;
		}

		while (state->rp < state->wp && p < ep)
			*p++ = *state->rp++;
	}

	return p - buf;
}

static void
close_dctd(fz_context *ctx, void *state_)
{
	fz_dctd *state = (fz_dctd *)state_;

	if (setjmp(state->jb))
	{
		fz_warn(ctx, "jpeg error: %s", state->msg);
		goto skip;
	}

	if (state->init)
		jpeg_finish_decompress(&state->cinfo);

skip:
	if (state->cinfo.src)
		state->chain->rp = state->chain->wp - state->cinfo.src->bytes_in_buffer;
	if (state->init)
		jpeg_destroy_decompress(&state->cinfo);

	fz_free(ctx, state->scanline);
	fz_close(state->chain);
	fz_free(ctx, state);
}

/* Default: color_transform = -1 (unset) */
fz_stream *
fz_open_dctd(fz_stream *chain, int color_transform)
{
	return fz_open_resized_dctd(chain, color_transform, 1);
}

fz_stream *
fz_open_resized_dctd(fz_stream *chain, int color_transform, int factor)
{
	fz_context *ctx = chain->ctx;
	fz_dctd *state = NULL;

	fz_var(state);

	fz_try(ctx)
	{
		state = fz_malloc_struct(chain->ctx, fz_dctd);
		state->ctx = ctx;
		state->chain = chain;
		state->color_transform = color_transform;
		state->init = 0;
		state->factor = factor;
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state);
		fz_close(chain);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, read_dctd, close_dctd);
}
