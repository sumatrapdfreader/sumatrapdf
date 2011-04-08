#include "fitz.h"

/* TODO: check if this works with 16bpp images */

enum { MAXC = 32 };

typedef struct fz_predict_s fz_predict;

struct fz_predict_s
{
	fz_stream *chain;

	int predictor;
	int columns;
	int colors;
	int bpc;

	int stride;
	int bpp;
	unsigned char *in;
	unsigned char *out;
	unsigned char *ref;
	unsigned char *rp, *wp;
};

static inline int getcomponent(unsigned char *line, int x, int bpc)
{
	switch (bpc)
	{
	case 1: return (line[x >> 3] >> ( 7 - (x & 7) ) ) & 1;
	case 2: return (line[x >> 2] >> ( ( 3 - (x & 3) ) << 1 ) ) & 3;
	case 4: return (line[x >> 1] >> ( ( 1 - (x & 1) ) << 2 ) ) & 15;
	case 8: return line[x];
	}
	return 0;
}


static inline void putcomponent(unsigned char *buf, int x, int bpc, int value)
{
	switch (bpc)
	{
	case 1: buf[x >> 3] |= value << (7 - (x & 7)); break;
	case 2: buf[x >> 2] |= value << ((3 - (x & 3)) << 1); break;
	case 4: buf[x >> 1] |= value << ((1 - (x & 1)) << 2); break;
	case 8: buf[x] = value; break;
	}
}

static inline int paeth(int a, int b, int c)
{
	/* The definitions of ac and bc are correct, not a typo. */
	int ac = b - c, bc = a - c, abcc = ac + bc;
	int pa = ABS(ac);
	int pb = ABS(bc);
	int pc = ABS(abcc);
	return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

static void
fz_predict_tiff(fz_predict *state, unsigned char *out, unsigned char *in, int len)
{
	int left[MAXC];
	int i, k;

	for (k = 0; k < state->colors; k++)
		left[k] = 0;

	for (i = 0; i < state->columns; i++)
	{
		for (k = 0; k < state->colors; k++)
		{
			int a = getcomponent(in, i * state->colors + k, state->bpc);
			int b = a + left[k];
			int c = b % (1 << state->bpc);
			putcomponent(out, i * state->colors + k, state->bpc, c);
			left[k] = c;
		}
	}
}

static void
fz_predict_png(fz_predict *state, unsigned char *out, unsigned char *in, int len, int predictor)
{
	int bpp = state->bpp;
	int i;
	unsigned char *ref = state->ref;

	switch (predictor)
	{
	case 0:
		memcpy(out, in, len);
		break;
	case 1:
		for (i = bpp; i > 0; i--)
		{
			*out++ = *in++;
		}
		for (i = len - bpp; i > 0; i--)
		{
			*out = *in++ + out[-bpp];
			out++;
		}
		break;
	case 2:
		for (i = bpp; i > 0; i--)
		{
			*out++ = *in++ + *ref++;
		}
		for (i = len - bpp; i > 0; i--)
		{
			*out++ = *in++ + *ref++;
		}
		break;
	case 3:
		for (i = bpp; i > 0; i--)
		{
			*out++ = *in++ + (*ref++) / 2;
		}
		for (i = len - bpp; i > 0; i--)
		{
			*out = *in++ + (out[-bpp] + *ref++) / 2;
			out++;
		}
		break;
	case 4:
		for (i = bpp; i > 0; i--)
		{
			*out++ = *in++ + paeth(0, *ref++, 0);
		}
		for (i = len - bpp; i > 0; i --)
		{
			*out = *in++ + paeth(out[-bpp], *ref, ref[-bpp]);
			ref++;
			out++;
		}
		break;
	}
}

static int
read_predict(fz_stream *stm, unsigned char *buf, int len)
{
	fz_predict *state = stm->state;
	unsigned char *p = buf;
	unsigned char *ep = buf + len;
	int ispng = state->predictor >= 10;
	int n;

	while (state->rp < state->wp && p < ep)
		*p++ = *state->rp++;

	while (p < ep)
	{
		n = fz_read(state->chain, state->in, state->stride + ispng);
		if (n < 0)
			return fz_rethrow(n, "read error in prediction filter");
		if (n == 0)
			return p - buf;

		if (state->predictor == 1)
			memcpy(state->out, state->in, n);
		else if (state->predictor == 2)
			fz_predict_tiff(state, state->out, state->in, n);
		else
		{
			fz_predict_png(state, state->out, state->in + 1, n - 1, state->in[0]);
			memcpy(state->ref, state->out, state->stride);
		}

		state->rp = state->out;
		state->wp = state->out + n - ispng;

		while (state->rp < state->wp && p < ep)
			*p++ = *state->rp++;
	}

	return p - buf;
}

static void
close_predict(fz_stream *stm)
{
	fz_predict *state = stm->state;
	fz_close(state->chain);
	fz_free(state->in);
	fz_free(state->out);
	fz_free(state->ref);
	fz_free(state);
}

fz_stream *
fz_open_predict(fz_stream *chain, fz_obj *params)
{
	fz_predict *state;
	fz_obj *obj;

	state = fz_malloc(sizeof(fz_predict));
	state->chain = chain;

	state->predictor = 1;
	state->columns = 1;
	state->colors = 1;
	state->bpc = 8;

	obj = fz_dict_gets(params, "Predictor");
	if (obj)
		state->predictor = fz_to_int(obj);

	if (state->predictor != 1 && state->predictor != 2 &&
		state->predictor != 10 && state->predictor != 11 &&
		state->predictor != 12 && state->predictor != 13 &&
		state->predictor != 14 && state->predictor != 15)
	{
		fz_warn("invalid predictor: %d", state->predictor);
		state->predictor = 1;
	}

	obj = fz_dict_gets(params, "Columns");
	if (obj)
		state->columns = fz_to_int(obj);

	obj = fz_dict_gets(params, "Colors");
	if (obj)
		state->colors = fz_to_int(obj);

	obj = fz_dict_gets(params, "BitsPerComponent");
	if (obj)
		state->bpc = fz_to_int(obj);

	state->stride = (state->bpc * state->colors * state->columns + 7) / 8;
	state->bpp = (state->bpc * state->colors + 7) / 8;

	state->in = fz_malloc(state->stride + 1);
	state->out = fz_malloc(state->stride);
	state->ref = fz_malloc(state->stride);
	state->rp = state->out;
	state->wp = state->out;

	memset(state->ref, 0, state->stride);

	return fz_new_stream(state, read_predict, close_predict);
}
