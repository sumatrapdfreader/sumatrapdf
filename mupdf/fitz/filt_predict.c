#include "fitz_base.h"
#include "fitz_stream.h"

/* TODO: check if this works with 16bpp images */

enum { MAXC = 32 };

typedef struct fz_predict_s fz_predict;

struct fz_predict_s
{
	fz_filter super;

	int predictor;
	int columns;
	int colors;
	int bpc;

	int stride;
	int bpp;
	unsigned char *ref;
};

fz_filter *
fz_newpredictd(fz_obj *params)
{
	fz_obj *obj;

	FZ_NEWFILTER(fz_predict, p, predict);

	p->predictor = 1;
	p->columns = 1;
	p->colors = 1;
	p->bpc = 8;

	obj = fz_dictgets(params, "Predictor");
	if (obj)
		p->predictor = fz_toint(obj);

	if (p->predictor != 1 && p->predictor != 2 &&
		p->predictor != 10 && p->predictor != 11 &&
		p->predictor != 12 && p->predictor != 13 &&
		p->predictor != 14 && p->predictor != 15)
	{
		fz_warn("invalid predictor: %d", p->predictor);
		p->predictor = 1;
	}

	obj = fz_dictgets(params, "Columns");
	if (obj)
		p->columns = fz_toint(obj);

	obj = fz_dictgets(params, "Colors");
	if (obj)
		p->colors = fz_toint(obj);

	obj = fz_dictgets(params, "BitsPerComponent");
	if (obj)
		p->bpc = fz_toint(obj);

	p->stride = (p->bpc * p->colors * p->columns + 7) / 8;
	p->bpp = (p->bpc * p->colors + 7) / 8;

	if (p->predictor >= 10)
	{
		p->ref = fz_malloc(p->stride);
		memset(p->ref, 0, p->stride);
	}
	else
	{
		p->ref = nil;
	}

	return (fz_filter*)p;
}

void
fz_droppredict(fz_filter *filter)
{
	fz_predict *p = (fz_predict*)filter;
	fz_free(p->ref);
}

static inline int
getcomponent(unsigned char *buf, int x, int bpc)
{
	switch (bpc)
	{
	case 1: return buf[x / 8] >> (7 - (x % 8)) & 0x01;
	case 2: return buf[x / 4] >> ((3 - (x % 4)) * 2) & 0x03;
	case 4: return buf[x / 2] >> ((1 - (x % 2)) * 4) & 0x0f;
	case 8: return buf[x];
	}
	return 0;
}

static inline void
putcomponent(unsigned char *buf, int x, int bpc, int value)
{
	switch (bpc)
	{
	case 1: buf[x / 8] |= value << (7 - (x % 8)); break;
	case 2: buf[x / 4] |= value << ((3 - (x % 4)) * 2); break;
	case 4: buf[x / 2] |= value << ((1 - (x % 2)) * 4); break;
	case 8: buf[x] = value; break;
	}
}

static inline int
paeth(int a, int b, int c)
{
	/* The definitions of ac and bc are correct, not a typo. */
	int ac = b - c, bc = a - c, abcc = ac + bc;
	int pa = (ac < 0 ? -ac : ac);
	int pb = (bc < 0 ? -bc : bc);
	int pc = (abcc < 0 ? -abcc : abcc);
	return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

static inline void
fz_predictnone(fz_predict *p, unsigned char *in, unsigned char *out)
{
	memcpy(out, in, p->stride);
}

static void
fz_predicttiff(fz_predict *p, unsigned char *in, unsigned char *out)
{
	int left[MAXC];
	int i, k;

	for (k = 0; k < p->colors; k++)
		left[k] = 0;

	for (i = 0; i < p->columns; i++)
	{
		for (k = 0; k < p->colors; k++)
		{
			int a = getcomponent(in, i * p->colors + k, p->bpc);
			int b = a + left[k];
			int c = b % (1 << p->bpc);
			putcomponent(out, i * p->colors + k, p->bpc, c);
			left[k] = c;
		}
	}
}

static void
fz_predictpng(fz_predict *p, unsigned char *in, unsigned char *out, int predictor)
{
	int upleft[MAXC], left[MAXC], i, k;

	for (k = 0; k < p->bpp; k++)
	{
		left[k] = 0;
		upleft[k] = 0;
	}

	for (k = 0, i = 0; i < p->stride; k = (k + 1) % p->bpp, i ++)
	{
		switch (predictor)
		{
		case 0: out[i] = in[i]; break;
		case 1: out[i] = in[i] + left[k]; break;
		case 2: out[i] = in[i] + p->ref[i]; break;
		case 3: out[i] = in[i] + (left[k] + p->ref[i]) / 2; break;
		case 4: out[i] = in[i] + paeth(left[k], p->ref[i], upleft[k]); break;
		}
		left[k] = out[i];
		upleft[k] = p->ref[i];
	}
}

fz_error
fz_processpredict(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_predict *dec = (fz_predict*)filter;
	int ispng = dec->predictor >= 10;
	int predictor;

	while (1)
	{
		if (in->rp + dec->stride + ispng > in->wp)
		{
			if (in->eof)
				return fz_iodone;
			return fz_ioneedin;
		}

		if (out->wp + dec->stride > out->ep)
			return fz_ioneedout;

		if (dec->predictor == 1)
		{
			fz_predictnone(dec, in->rp, out->wp);
		}
		else if (dec->predictor == 2)
		{
			if (dec->bpc != 8)
				memset(out->wp, 0, dec->stride);
			fz_predicttiff(dec, in->rp, out->wp);
		}
		else
		{
			predictor = *in->rp++;
			fz_predictpng(dec, in->rp, out->wp, predictor);
		}

		if (dec->ref)
			memcpy(dec->ref, out->wp, dec->stride);

		in->rp += dec->stride;
		out->wp += dec->stride;
	}
}

