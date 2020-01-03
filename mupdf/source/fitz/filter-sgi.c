#include "mupdf/fitz.h"

#include <math.h>
#include <string.h>

/* Table stolen from LibTiff */
#define UV_SQSIZ	0.003500f
#define UV_NDIVS	16289
#define UV_VSTART	0.016940f
#define UV_NVS		163
#define U_NEU		0.210526316f
#define V_NEU		0.473684211f
#define UVSCALE		410
static struct {
	float	ustart;
	short	nus, ncum;
}	uv_row[UV_NVS] = {
	{ 0.247663f,	4,	0 },
	{ 0.243779f,	6,	4 },
	{ 0.241684f,	7,	10 },
	{ 0.237874f,	9,	17 },
	{ 0.235906f,	10,	26 },
	{ 0.232153f,	12,	36 },
	{ 0.228352f,	14,	48 },
	{ 0.226259f,	15,	62 },
	{ 0.222371f,	17,	77 },
	{ 0.220410f,	18,	94 },
	{ 0.214710f,	21,	112 },
	{ 0.212714f,	22,	133 },
	{ 0.210721f,	23,	155 },
	{ 0.204976f,	26,	178 },
	{ 0.202986f,	27,	204 },
	{ 0.199245f,	29,	231 },
	{ 0.195525f,	31,	260 },
	{ 0.193560f,	32,	291 },
	{ 0.189878f,	34,	323 },
	{ 0.186216f,	36,	357 },
	{ 0.186216f,	36,	393 },
	{ 0.182592f,	38,	429 },
	{ 0.179003f,	40,	467 },
	{ 0.175466f,	42,	507 },
	{ 0.172001f,	44,	549 },
	{ 0.172001f,	44,	593 },
	{ 0.168612f,	46,	637 },
	{ 0.168612f,	46,	683 },
	{ 0.163575f,	49,	729 },
	{ 0.158642f,	52,	778 },
	{ 0.158642f,	52,	830 },
	{ 0.158642f,	52,	882 },
	{ 0.153815f,	55,	934 },
	{ 0.153815f,	55,	989 },
	{ 0.149097f,	58,	1044 },
	{ 0.149097f,	58,	1102 },
	{ 0.142746f,	62,	1160 },
	{ 0.142746f,	62,	1222 },
	{ 0.142746f,	62,	1284 },
	{ 0.138270f,	65,	1346 },
	{ 0.138270f,	65,	1411 },
	{ 0.138270f,	65,	1476 },
	{ 0.132166f,	69,	1541 },
	{ 0.132166f,	69,	1610 },
	{ 0.126204f,	73,	1679 },
	{ 0.126204f,	73,	1752 },
	{ 0.126204f,	73,	1825 },
	{ 0.120381f,	77,	1898 },
	{ 0.120381f,	77,	1975 },
	{ 0.120381f,	77,	2052 },
	{ 0.120381f,	77,	2129 },
	{ 0.112962f,	82,	2206 },
	{ 0.112962f,	82,	2288 },
	{ 0.112962f,	82,	2370 },
	{ 0.107450f,	86,	2452 },
	{ 0.107450f,	86,	2538 },
	{ 0.107450f,	86,	2624 },
	{ 0.107450f,	86,	2710 },
	{ 0.100343f,	91,	2796 },
	{ 0.100343f,	91,	2887 },
	{ 0.100343f,	91,	2978 },
	{ 0.095126f,	95,	3069 },
	{ 0.095126f,	95,	3164 },
	{ 0.095126f,	95,	3259 },
	{ 0.095126f,	95,	3354 },
	{ 0.088276f,	100,	3449 },
	{ 0.088276f,	100,	3549 },
	{ 0.088276f,	100,	3649 },
	{ 0.088276f,	100,	3749 },
	{ 0.081523f,	105,	3849 },
	{ 0.081523f,	105,	3954 },
	{ 0.081523f,	105,	4059 },
	{ 0.081523f,	105,	4164 },
	{ 0.074861f,	110,	4269 },
	{ 0.074861f,	110,	4379 },
	{ 0.074861f,	110,	4489 },
	{ 0.074861f,	110,	4599 },
	{ 0.068290f,	115,	4709 },
	{ 0.068290f,	115,	4824 },
	{ 0.068290f,	115,	4939 },
	{ 0.068290f,	115,	5054 },
	{ 0.063573f,	119,	5169 },
	{ 0.063573f,	119,	5288 },
	{ 0.063573f,	119,	5407 },
	{ 0.063573f,	119,	5526 },
	{ 0.057219f,	124,	5645 },
	{ 0.057219f,	124,	5769 },
	{ 0.057219f,	124,	5893 },
	{ 0.057219f,	124,	6017 },
	{ 0.050985f,	129,	6141 },
	{ 0.050985f,	129,	6270 },
	{ 0.050985f,	129,	6399 },
	{ 0.050985f,	129,	6528 },
	{ 0.050985f,	129,	6657 },
	{ 0.044859f,	134,	6786 },
	{ 0.044859f,	134,	6920 },
	{ 0.044859f,	134,	7054 },
	{ 0.044859f,	134,	7188 },
	{ 0.040571f,	138,	7322 },
	{ 0.040571f,	138,	7460 },
	{ 0.040571f,	138,	7598 },
	{ 0.040571f,	138,	7736 },
	{ 0.036339f,	142,	7874 },
	{ 0.036339f,	142,	8016 },
	{ 0.036339f,	142,	8158 },
	{ 0.036339f,	142,	8300 },
	{ 0.032139f,	146,	8442 },
	{ 0.032139f,	146,	8588 },
	{ 0.032139f,	146,	8734 },
	{ 0.032139f,	146,	8880 },
	{ 0.027947f,	150,	9026 },
	{ 0.027947f,	150,	9176 },
	{ 0.027947f,	150,	9326 },
	{ 0.023739f,	154,	9476 },
	{ 0.023739f,	154,	9630 },
	{ 0.023739f,	154,	9784 },
	{ 0.023739f,	154,	9938 },
	{ 0.019504f,	158,	10092 },
	{ 0.019504f,	158,	10250 },
	{ 0.019504f,	158,	10408 },
	{ 0.016976f,	161,	10566 },
	{ 0.016976f,	161,	10727 },
	{ 0.016976f,	161,	10888 },
	{ 0.016976f,	161,	11049 },
	{ 0.012639f,	165,	11210 },
	{ 0.012639f,	165,	11375 },
	{ 0.012639f,	165,	11540 },
	{ 0.009991f,	168,	11705 },
	{ 0.009991f,	168,	11873 },
	{ 0.009991f,	168,	12041 },
	{ 0.009016f,	170,	12209 },
	{ 0.009016f,	170,	12379 },
	{ 0.009016f,	170,	12549 },
	{ 0.006217f,	173,	12719 },
	{ 0.006217f,	173,	12892 },
	{ 0.005097f,	175,	13065 },
	{ 0.005097f,	175,	13240 },
	{ 0.005097f,	175,	13415 },
	{ 0.003909f,	177,	13590 },
	{ 0.003909f,	177,	13767 },
	{ 0.002340f,	177,	13944 },
	{ 0.002389f,	170,	14121 },
	{ 0.001068f,	164,	14291 },
	{ 0.001653f,	157,	14455 },
	{ 0.000717f,	150,	14612 },
	{ 0.001614f,	143,	14762 },
	{ 0.000270f,	136,	14905 },
	{ 0.000484f,	129,	15041 },
	{ 0.001103f,	123,	15170 },
	{ 0.001242f,	115,	15293 },
	{ 0.001188f,	109,	15408 },
	{ 0.001011f,	103,	15517 },
	{ 0.000709f,	97,	15620 },
	{ 0.000301f,	89,	15717 },
	{ 0.002416f,	82,	15806 },
	{ 0.003251f,	76,	15888 },
	{ 0.003246f,	69,	15964 },
	{ 0.004141f,	62,	16033 },
	{ 0.005963f,	55,	16095 },
	{ 0.008839f,	47,	16150 },
	{ 0.010490f,	40,	16197 },
	{ 0.016994f,	31,	16237 },
	{ 0.023659f,	21,	16268 },
};

/* SGI Log 16bit (greyscale) */

typedef struct fz_sgilog16_s fz_sgilog16;

struct fz_sgilog16_s
{
	fz_stream *chain;
	int run, n, c, w;
	uint16_t *temp;
};

static inline int
sgilog16val(fz_context *ctx, uint16_t v)
{
	int Le;
	float Y;

	Le = v & 0x7fff;
	if (!Le)
		Y = 0;
	else
	{
		Y = expf(FZ_LN2/256 * (Le + .5f) - FZ_LN2*64);
		if (v & 0x8000)
			Y = -Y;
	}

	return ((Y <= 0) ? 0 : (Y >= 1) ? 255 : (int)(256*sqrtf(Y)));
}

static int
next_sgilog16(fz_context *ctx, fz_stream *stm, size_t max)
{
	fz_sgilog16 *state = stm->state;
	uint16_t *p;
	uint16_t *ep;
	uint8_t *q;
	int shift;

	(void)max;

	if (state->run < 0)
		return EOF;

	memset(state->temp, 0, state->w * sizeof(uint16_t));

	for (shift = 8; shift >= 0; shift -= 8)
	{
		p = state->temp;
		ep = p + state->w;
		while (p < ep)
		{
			if (state->n == 0)
			{
				state->run = fz_read_byte(ctx, state->chain);
				if (state->run < 0)
				{
					state->run = -1;
					fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
				}
				if (state->run < 128)
					state->n = state->run;
				else
				{
					state->n = state->run - 126;
					state->c = fz_read_byte(ctx, state->chain);
					if (state->c < 0)
					{
						state->run = -1;
						fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
					}
				}
			}

			if (state->run < 128)
			{
				while (p < ep && state->n)
				{
					int c = fz_read_byte(ctx, state->chain);
					if (c < 0)
					{
						state->run = -1;
						fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
					}
					*p++ |= c<<shift;
					state->n--;
				}
			}
			else
			{
				while (p < ep && state->n)
				{
					*p++ |= state->c<<shift;
					state->n--;
				}
			}
		}
	}

	p = state->temp;
	q = (uint8_t *)p;
	ep = p + state->w;
	while (p < ep)
	{
		*q++ = sgilog16val(ctx, *p++);
	}

	stm->rp = (uint8_t *)(state->temp);
	stm->wp = q;
	stm->pos += q - stm->rp;

	if (q == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void
close_sgilog16(fz_context *ctx, void *state_)
{
	fz_sgilog16 *state = (fz_sgilog16 *)state_;
	fz_stream *chain = state->chain;

	fz_free(ctx, state->temp);
	fz_free(ctx, state);
	fz_drop_stream(ctx, chain);
}

fz_stream *
fz_open_sgilog16(fz_context *ctx, fz_stream *chain, int w)
{
	fz_sgilog16 *state = fz_malloc_struct(ctx, fz_sgilog16);

	fz_try(ctx)
	{
		state->run = 0;
		state->n = 0;
		state->c = 0;
		state->w = w;
		state->temp = Memento_label(fz_malloc(ctx, w * sizeof(uint16_t)), "sgilog16_temp");
		state->chain = fz_keep_stream(ctx, chain);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state->temp);
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_sgilog16, close_sgilog16);
}

/* SGI Log 24bit (LUV) */

typedef struct fz_sgilog24_s fz_sgilog24;

struct fz_sgilog24_s
{
	fz_stream *chain;
	int err, w;
	uint8_t *temp;
};

static int
uv_decode(float *up, float *vp, int c)	/* decode (u',v') index */
{
	int	upper, lower;
	register int	ui, vi;

	if (c < 0 || c >= UV_NDIVS)
		return (-1);
	lower = 0;				/* binary search */
	upper = UV_NVS;
	while (upper - lower > 1) {
		vi = (lower + upper) >> 1;
		ui = c - uv_row[vi].ncum;
		if (ui > 0)
			lower = vi;
		else if (ui < 0)
			upper = vi;
		else {
			lower = vi;
			break;
		}
	}
	vi = lower;
	ui = c - uv_row[vi].ncum;
	*up = uv_row[vi].ustart + (ui+.5f)*UV_SQSIZ;
	*vp = UV_VSTART + (vi+.5f)*UV_SQSIZ;
	return (0);
}

static inline int
sgilog24val(fz_context *ctx, fz_stream *chain, uint8_t *rgb)
{
	int b0, b1, b2;
	int luv, p;
	float u, v, s, x, y, X, Y, Z;
	float r, g, b;

	b0 = fz_read_byte(ctx, chain);
	if (b0 < 0)
		return b0;
	b1 = fz_read_byte(ctx, chain);
	if (b1 < 0)
		return b1;
	b2 = fz_read_byte(ctx, chain);
	if (b2 < 0)
		return b2;

	luv = (b0<<16) | (b1<<8) | b2;

	/* decode luminance */
	p = (luv>>14) & 0x3ff;
	Y = (p == 0 ? 0 : expf(FZ_LN2/64*(p+.5f) - FZ_LN2*12));
	if (Y <= 0)
	{
		X = Y = Z = 0;
	}
	else
	{
		/* decode color */
		if (uv_decode(&u, &v, luv & 0x3fff) < 0) {
			u = U_NEU; v = V_NEU;
		}
		s = 6*u - 16*v + 12;
		x = 9*u;
		y = 4*v;
		/* convert to XYZ */
		X = x/y * Y;
		Z = (s-x-y)/y * Y;
	}

	/* assume CCIR-709 primaries */
	r =  2.690f*X + -1.276f*Y + -0.414f*Z;
	g = -1.022f*X +  1.978f*Y +  0.044f*Z;
	b =  0.061f*X + -0.224f*Y +  1.163f*Z;

	/* assume 2.0 gamma for speed */
	/* could use integer sqrt approx., but this is probably faster */
	rgb[0] = (uint8_t)((r<=0) ? 0 : (r >= 1) ? 255 : (int)(256*sqrtf(r)));
	rgb[1] = (uint8_t)((g<=0) ? 0 : (g >= 1) ? 255 : (int)(256*sqrtf(g)));
	rgb[2] = (uint8_t)((b<=0) ? 0 : (b >= 1) ? 255 : (int)(256*sqrtf(b)));

	return 0;
}

static int
next_sgilog24(fz_context *ctx, fz_stream *stm, size_t max)
{
	fz_sgilog24 *state = stm->state;
	uint8_t *p;
	uint8_t *ep;

	(void)max;

	if (state->err)
		return EOF;

	memset(state->temp, 0, state->w * 3);

	p = state->temp;
	ep = p + state->w * 3;
	while (p < ep)
	{
		int c = sgilog24val(ctx, state->chain, p);
		if (c < 0)
		{
			state->err = 1;
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
		}
		p += 3;
	}

	stm->rp = state->temp;
	stm->wp = p;
	stm->pos += p - stm->rp;

	if (p == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void
close_sgilog24(fz_context *ctx, void *state_)
{
	fz_sgilog24 *state = (fz_sgilog24 *)state_;
	fz_stream *chain = state->chain;

	fz_free(ctx, state->temp);
	fz_free(ctx, state);
	fz_drop_stream(ctx, chain);
}

fz_stream *
fz_open_sgilog24(fz_context *ctx, fz_stream *chain, int w)
{
	fz_sgilog24 *state = fz_malloc_struct(ctx, fz_sgilog24);

	fz_try(ctx)
	{
		state->err = 0;
		state->w = w;
		state->temp = Memento_label(fz_malloc(ctx, w * 3), "sgilog24_temp");
		state->chain = fz_keep_stream(ctx, chain);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state->temp);
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}

	return fz_new_stream(ctx, state, next_sgilog24, close_sgilog24);
}

/* SGI Log 32bit */

typedef struct fz_sgilog32_s fz_sgilog32;

struct fz_sgilog32_s
{
	fz_stream *chain;
	int run, n, c, w;
	uint32_t *temp;
};

static inline void
sgilog32val(fz_context *ctx, uint32_t p, uint8_t *rgb)
{
	float r, g, b;
	float u, v, s, x, y;
	float X, Y, Z;

	if (p>>31)
	{
		X = Y = Z = 0;
	}
	else
	{
		int Le = (p>>16) & 0x7fff;
		Y = !Le ? 0 : expf(FZ_LN2/256*(Le+.5f) - FZ_LN2*64);
		/* decode color */
		u = (1.f/UVSCALE) * ((p>>8 & 0xff) + .5f);
		v = (1.f/UVSCALE) * ((p & 0xff) + .5f);
		s = 6*u - 16*v + 12;
		x = 9 * u;
		y = 4 * v;

		/* convert to XYZ */
		X = x/y * Y;
		Z = (s-x-y)/y * Y;
	}

	/* assume CCIR-709 primaries */
	r =  2.690f*X + -1.276f*Y + -0.414f*Z;
	g = -1.022f*X +  1.978f*Y +  0.044f*Z;
	b =  0.061f*X + -0.224f*Y +  1.163f*Z;

	/* assume 2.0 gamma for speed */
	/* could use integer sqrt approx., but this is probably faster */
	rgb[0] = (uint8_t)((r<=0) ? 0 : (r >= 1) ? 255 : (int)(256*sqrtf(r)));
	rgb[1] = (uint8_t)((g<=0) ? 0 : (g >= 1) ? 255 : (int)(256*sqrtf(g)));
	rgb[2] = (uint8_t)((b<=0) ? 0 : (b >= 1) ? 255 : (int)(256*sqrtf(b)));
}

static int
next_sgilog32(fz_context *ctx, fz_stream *stm, size_t max)
{
	fz_sgilog32 *state = stm->state;
	uint32_t *p;
	uint32_t *ep;
	uint8_t *q;
	int shift;

	(void)max;

	if (state->run < 0)
		return EOF;

	memset(state->temp, 0, state->w * sizeof(uint32_t));

	for (shift = 24; shift >= 0; shift -= 8)
	{
		p = state->temp;
		ep = p + state->w;
		while (p < ep)
		{
			if (state->n == 0)
			{
				state->run = fz_read_byte(ctx, state->chain);
				if (state->run < 0)
				{
					state->run = -1;
					fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
				}
				if (state->run < 128)
					state->n = state->run;
				else
				{
					state->n = state->run - 126;
					state->c = fz_read_byte(ctx, state->chain);
					if (state->c < 0)
					{
						state->run = -1;
						fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
					}
				}
			}

			if (state->run < 128)
			{
				while (p < ep && state->n)
				{
					int c = fz_read_byte(ctx, state->chain);
					if (c < 0)
					{
						state->run = -1;
						fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in run length decode");
					}
					*p++ |= c<<shift;
					state->n--;
				}
			}
			else
			{
				while (p < ep && state->n)
				{
					*p++ |= state->c<<shift;
					state->n--;
				}
			}
		}
	}

	p = state->temp;
	q = (uint8_t *)p;
	ep = p + state->w;
	while (p < ep)
	{
		sgilog32val(ctx, *p++, q);
		q += 3;
	}

	stm->rp = (uint8_t *)(state->temp);
	stm->wp = q;
	stm->pos += q - stm->rp;

	if (q == stm->rp)
		return EOF;

	return *stm->rp++;
}

static void
close_sgilog32(fz_context *ctx, void *state_)
{
	fz_sgilog32 *state = (fz_sgilog32 *)state_;
	fz_drop_stream(ctx, state->chain);
	fz_free(ctx, state->temp);
	fz_free(ctx, state);
}

fz_stream *
fz_open_sgilog32(fz_context *ctx, fz_stream *chain, int w)
{
	fz_sgilog32 *state = fz_malloc_struct(ctx, fz_sgilog32);
	fz_try(ctx)
	{
		state->run = 0;
		state->n = 0;
		state->c = 0;
		state->w = w;
		state->temp = Memento_label(fz_malloc(ctx, w * sizeof(uint32_t)), "sgilog32_temp");
		state->chain = fz_keep_stream(ctx, chain);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, state->temp);
		fz_free(ctx, state);
		fz_rethrow(ctx);
	}
	return fz_new_stream(ctx, state, next_sgilog32, close_sgilog32);
}
