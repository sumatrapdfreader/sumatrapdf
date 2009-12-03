#include "fitz_base.h"
#include "fitz_stream.h"

#include "filt_faxd.h"

/* common bit magic */

static inline void
printbits(FILE *f, int code, int nbits)
{
	int n, b;
	for (n = nbits - 1; n >= 0; n--)
	{
		b = (code >> n) & 1;
		fprintf(f, "%c", b ? '1' : '0');
	}
}

static inline int
getbit(const unsigned char *buf, int x)
{
	return ( buf[x >> 3] >> ( 7 - (x & 7) ) ) & 1;
}

static inline void
printline(FILE *f, unsigned char *line, int w)
{
	int i;
	for (i = 0; i < w; i++)
		fprintf(f, "%c", getbit(line, i) ? '#' : '.');
	fprintf(f, "\n");
}

static inline int
getrun(const unsigned char *line, int x, int w, int c)
{
	int z;
	int b;

	if (x < 0)
		x = 0;

	z = x;
	while (z < w)
	{
		b = getbit(line, z);
		if (c != b)
			break;
		z ++;
	}

	return z - x;
}

static inline int
findchanging(const unsigned char *line, int x, int w)
{
	int a, b;

	if (!line)
		return w;

	if (x == -1)
	{
		a = 0;
		x = 0;
	}
	else
	{
		a = getbit(line, x);
		x++;
	}

	while (x < w)
	{
		b = getbit(line, x);
		if (a != b)
			break;
		x++;
	}

	return x;
}

static inline int
findchangingcolor(const unsigned char *line, int x, int w, int color)
{
	if (!line)
		return w;

	x = findchanging(line, x, w);

	if (x < w && getbit(line, x) != color)
		x = findchanging(line, x, w);

	return x;
}

static const unsigned char lm[8] =
{ 0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01 };

static const unsigned char rm[8] =
{ 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };

static inline void
setbits(unsigned char *line, int x0, int x1)
{
	int a0, a1, b0, b1, a;

	a0 = x0 >> 3;
	a1 = x1 >> 3;

	b0 = x0 & 7;
	b1 = x1 & 7;

	if (a0 == a1)
	{
		if (b1)
			line[a0] |= lm[b0] & rm[b1];
	}
	else
	{
		line[a0] |= lm[b0];
		for (a = a0 + 1; a < a1; a++)
			line[a] = 0xFF;
		if (b1)
			line[a1] |= rm[b1];
	}
}

typedef enum fax_stage_e
{
	SNORMAL,	/* neutral state, waiting for any code */
	SMAKEUP,	/* got a 1d makeup code, waiting for terminating code */
	SEOL,		/* at eol, needs output buffer space */
	SH1, SH2	/* in H part 1 and 2 (both makeup and terminating codes) */
} fax_stage_e;

/* TODO: uncompressed */

typedef struct fz_faxd_s fz_faxd;

struct fz_faxd_s
{
	fz_filter super;

	int k;
	int endofline;
	int encodedbytealign;
	int columns;
	int rows;
	int endofblock;
	int blackis1;

	int stride;
	int ridx;

	int bidx;
	unsigned int word;

	fax_stage_e stage;

	int a, c, dim, eolc;
	unsigned char *ref;
	unsigned char *dst;
};

fz_filter *
fz_newfaxd(fz_obj *params)
{
	fz_obj *obj;

	FZ_NEWFILTER(fz_faxd, fax, faxd);

	fax->ref = nil;
	fax->dst = nil;

	fax->k = 0;
	fax->endofline = 0;
	fax->encodedbytealign = 0;
	fax->columns = 1728;
	fax->rows = 0;
	fax->endofblock = 1;
	fax->blackis1 = 0;

	obj = fz_dictgets(params, "K");
	if (obj) fax->k = fz_toint(obj);

	obj = fz_dictgets(params, "EndOfLine");
	if (obj) fax->endofline = fz_tobool(obj);

	obj = fz_dictgets(params, "EncodedByteAlign");
	if (obj) fax->encodedbytealign = fz_tobool(obj);

	obj = fz_dictgets(params, "Columns");
	if (obj) fax->columns = fz_toint(obj);

	obj = fz_dictgets(params, "Rows");
	if (obj) fax->rows = fz_toint(obj);

	obj = fz_dictgets(params, "EndOfBlock");
	if (obj) fax->endofblock = fz_tobool(obj);

	obj = fz_dictgets(params, "BlackIs1");
	if (obj) fax->blackis1 = fz_tobool(obj);

	fax->stride = ((fax->columns - 1) >> 3) + 1;
	fax->ridx = 0;
	fax->bidx = 32;
	fax->word = 0;

	fax->stage = SNORMAL;
	fax->a = -1;
	fax->c = 0;
	fax->dim = fax->k < 0 ? 2 : 1;
	fax->eolc = 0;

	fax->ref = fz_malloc(fax->stride);
	fax->dst = fz_malloc(fax->stride);

	memset(fax->ref, 0, fax->stride);
	memset(fax->dst, 0, fax->stride);

	return (fz_filter*)fax;
}

void
fz_dropfaxd(fz_filter *p)
{
	fz_faxd *fax = (fz_faxd*) p;
	fz_free(fax->ref);
	fz_free(fax->dst);
}

static inline void eatbits(fz_faxd *fax, int nbits)
{
	fax->word <<= nbits;
	fax->bidx += nbits;
}

static inline fz_error fillbits(fz_faxd *fax, fz_buffer *in)
{
	while (fax->bidx >= 8)
	{
		if (in->rp + 1 > in->wp)
			return fz_ioneedin;
		fax->bidx -= 8;
		fax->word |= *in->rp << fax->bidx;
		in->rp ++;
	}
	return fz_okay;
}

static int
getcode(fz_faxd *fax, const cfd_node *table, int initialbits)
{
	unsigned int word = fax->word;
	int tidx = word >> (32 - initialbits);
	int val = table[tidx].val;
	int nbits = table[tidx].nbits;

	if (nbits > initialbits)
	{
		int mask = (1 << (32 - initialbits)) - 1;
		tidx = val + ((word & mask) >> (32 - nbits));
		val = table[tidx].val;
		nbits = initialbits + table[tidx].nbits;
	}

	eatbits(fax, nbits);

	return val;
}

/* decode one 1d code */
static fz_error
dec1d(fz_faxd *fax)
{
	int code;

	if (fax->a == -1)
		fax->a = 0;

	if (fax->c)
		code = getcode(fax, cf_black_decode, cfd_black_initial_bits);
	else
		code = getcode(fax, cf_white_decode, cfd_white_initial_bits);

	if (code == UNCOMPRESSED)
		return fz_throw("uncompressed data in faxd");

	if (code < 0)
		return fz_throw("negative code in 1d faxd");

	if (fax->a + code > fax->columns)
		return fz_throw("overflow in 1d faxd");

	if (fax->c)
		setbits(fax->dst, fax->a, fax->a + code);

	fax->a += code;

	if (code < 64)
	{
		fax->c = !fax->c;
		fax->stage = SNORMAL;
	}
	else
		fax->stage = SMAKEUP;

	return fz_okay;
}

/* decode one 2d code */
static fz_error
dec2d(fz_faxd *fax)
{
	int code, b1, b2;

	if (fax->stage == SH1 || fax->stage == SH2)
	{
		if (fax->a == -1)
			fax->a = 0;

		if (fax->c)
			code = getcode(fax, cf_black_decode, cfd_black_initial_bits);
		else
			code = getcode(fax, cf_white_decode, cfd_white_initial_bits);

		if (code == UNCOMPRESSED)
			return fz_throw("uncompressed data in faxd");

		if (code < 0)
			return fz_throw("negative code in 2d faxd");

		if (fax->a + code > fax->columns)
			return fz_throw("overflow in 2d faxd");

		if (fax->c)
			setbits(fax->dst, fax->a, fax->a + code);

		fax->a += code;

		if (code < 64)
		{
			fax->c = !fax->c;
			if (fax->stage == SH1)
				fax->stage = SH2;
			else if (fax->stage == SH2)
				fax->stage = SNORMAL;
		}

		return fz_okay;
	}

	code = getcode(fax, cf_2d_decode, cfd_2d_initial_bits);

	switch (code)
	{
	case H:
		fax->stage = SH1;
		break;

	case P:
		b1 = findchangingcolor(fax->ref, fax->a, fax->columns, !fax->c);
		if (b1 >= fax->columns)
			b2 = fax->columns;
		else
			b2 = findchanging(fax->ref, b1, fax->columns);
		if (fax->c) setbits(fax->dst, fax->a, b2);
		fax->a = b2;
		break;

	case V0:
		b1 = findchangingcolor(fax->ref, fax->a, fax->columns, !fax->c);
		if (fax->c) setbits(fax->dst, fax->a, b1);
		fax->a = b1;
		fax->c = !fax->c;
		break;

	case VR1:
		b1 = 1 + findchangingcolor(fax->ref, fax->a, fax->columns, !fax->c);
		if (b1 >= fax->columns) b1 = fax->columns;
		if (fax->c) setbits(fax->dst, fax->a, b1);
		fax->a = b1;
		fax->c = !fax->c;
		break;

	case VR2:
		b1 = 2 + findchangingcolor(fax->ref, fax->a, fax->columns, !fax->c);
		if (b1 >= fax->columns) b1 = fax->columns;
		if (fax->c) setbits(fax->dst, fax->a, b1);
		fax->a = b1;
		fax->c = !fax->c;
		break;

	case VR3:
		b1 = 3 + findchangingcolor(fax->ref, fax->a, fax->columns, !fax->c);
		if (b1 >= fax->columns) b1 = fax->columns;
		if (fax->c) setbits(fax->dst, fax->a, b1);
		fax->a = b1;
		fax->c = !fax->c;
		break;

	case VL1:
		b1 = -1 + findchangingcolor(fax->ref, fax->a, fax->columns, !fax->c);
		if (b1 < 0) b1 = 0;
		if (fax->c) setbits(fax->dst, fax->a, b1);
		fax->a = b1;
		fax->c = !fax->c;
		break;

	case VL2:
		b1 = -2 + findchangingcolor(fax->ref, fax->a, fax->columns, !fax->c);
		if (b1 < 0) b1 = 0;
		if (fax->c) setbits(fax->dst, fax->a, b1);
		fax->a = b1;
		fax->c = !fax->c;
		break;

	case VL3:
		b1 = -3 + findchangingcolor(fax->ref, fax->a, fax->columns, !fax->c);
		if (b1 < 0) b1 = 0;
		if (fax->c) setbits(fax->dst, fax->a, b1);
		fax->a = b1;
		fax->c = !fax->c;
		break;

	case UNCOMPRESSED:
		return fz_throw("uncompressed data in faxd");

	case ERROR:
		return fz_throw("invalid code in 2d faxd");

	default:
		return fz_throw("invalid code in 2d faxd (%d)", code);
	}

	return 0;
}

fz_error
fz_processfaxd(fz_filter *f, fz_buffer *in, fz_buffer *out)
{
	fz_faxd *fax = (fz_faxd*)f;
	fz_error error;
	int i;
	unsigned char *tmp;

	if (fax->stage == SEOL)
		goto eol;

loop:

	if (fillbits(fax, in))
	{
		if (in->eof)
		{
			if (fax->bidx > 31)
			{
				if (fax->a > 0)
					goto eol;
				goto rtc;
			}
		}
		else
		{
			return fz_ioneedin;
		}
	}

	if ((fax->word >> (32 - 12)) == 0)
	{
		eatbits(fax, 1);
		goto loop;
	}

	if ((fax->word >> (32 - 12)) == 1)
	{
		eatbits(fax, 12);
		fax->eolc ++;

		if (fax->k > 0)
		{
			if (fax->a == -1)
				fax->a = 0;
			if ((fax->word >> (32 - 1)) == 1)
				fax->dim = 1;
			else
				fax->dim = 2;
			eatbits(fax, 1);
		}
	}
	else if (fax->k > 0 && fax->a == -1)
	{
		fax->a = 0;
		if ((fax->word >> (32 - 1)) == 1)
			fax->dim = 1;
		else
			fax->dim = 2;
		eatbits(fax, 1);
	}
	else if (fax->dim == 1)
	{
		fax->eolc = 0;
		error = dec1d(fax);
		if (error) return error; /* can be fz_io* or real error */

	}
	else if (fax->dim == 2)
	{
		fax->eolc = 0;
		error = dec2d(fax);
		if (error) return error; /* can be fz_io* or real error */
	}

	/* no eol check after makeup codes nor in the middle of an H code */
	if (fax->stage == SMAKEUP || fax->stage == SH1 || fax->stage == SH2)
		goto loop;

	/* check for eol conditions */
	if (fax->eolc || fax->a >= fax->columns)
	{
		if (fax->a > 0)
			goto eol;
		if (fax->eolc == (fax->k < 0 ? 2 : 6))
			goto rtc;
	}

	goto loop;

eol:
	fax->stage = SEOL;
	if (out->wp + fax->stride > out->ep)
		return fz_ioneedout;

	if (fax->blackis1)
		memcpy(out->wp, fax->dst, fax->stride);
	else {
		unsigned char * restrict d = out->wp;
		unsigned char * restrict s = fax->dst;
		unsigned w = fax->stride;
		for (i = 0; i < w; i++)
			*d++ = *s++ ^ 0xff;
	}

	tmp = fax->ref;
	fax->ref = fax->dst;
	fax->dst = tmp;
	memset(fax->dst, 0, fax->stride);
	out->wp += fax->stride;

	fax->stage = SNORMAL;
	fax->c = 0;
	fax->a = -1;
	fax->ridx ++;

	if (!fax->endofblock && fax->rows)
	{
		if (fax->ridx >= fax->rows)
			goto rtc;
	}

	/* we have not read dim from eol, make a guess */
	if (fax->k > 0 && !fax->eolc && fax->a == -1)
	{
		if (fax->ridx % fax->k == 0)
			fax->dim = 1;
		else
			fax->dim = 2;
	}

	/* if endofline & encodedbytealign, EOLs are *not* optional */
	if (fax->encodedbytealign)
	{
		if (fax->endofline)
			eatbits(fax, (12 - fax->bidx) & 7);
		else
			eatbits(fax, (8 - fax->bidx) & 7);
	}

	goto loop;

rtc:
	i = (32 - fax->bidx) / 8;
	while (i-- && in->rp > in->bp)
		in->rp --;

	return fz_iodone;
}

