#include "fitz.h"

/* Identity filter */

fz_filter *
fz_newcopyfilter(void)
{
	FZ_NEWFILTER(fz_filter, f, copyfilter);
	return f;
}

void
fz_dropcopyfilter(fz_filter *f)
{
}

fz_error
fz_processcopyfilter(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	int n;

	while (1)
	{
		if (in->rp + 1 > in->wp)
		{
			if (in->eof)
				return fz_iodone;
			return fz_ioneedin;
		}

		if (out->wp + 1 > out->ep)
			return fz_ioneedout;

		n = MIN(in->wp - in->rp, out->ep - out->wp);
		if (n)
		{
			memcpy(out->wp, in->rp, n);
			in->rp += n;
			out->wp += n;
		}
	}
}

/* Null filter copies data until a specified length */

typedef struct fz_nullfilter_s fz_nullfilter;

struct fz_nullfilter_s
{
	fz_filter super;
	int len;
	int cur;
};

fz_filter *
fz_newnullfilter(int len)
{
	FZ_NEWFILTER(fz_nullfilter, f, nullfilter);
	f->len = len;
	f->cur = 0;
	return (fz_filter *)f;
}

void
fz_dropnullfilter(fz_filter *f)
{
}

fz_error
fz_processnullfilter(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_nullfilter *f = (fz_nullfilter*)filter;
	int n;

	n = MIN(in->wp - in->rp, out->ep - out->wp);
	if (f->len >= 0)
		n = MIN(n, f->len - f->cur);

	if (n)
	{
		memcpy(out->wp, in->rp, n);
		in->rp += n;
		out->wp += n;
		f->cur += n;
	}

	if (f->cur == f->len)
		return fz_iodone;
	if (in->rp == in->wp)
		return fz_ioneedin;
	if (out->wp == out->ep)
		return fz_ioneedout;

	return fz_throw("braindead programmer trapped in nullfilter");
}

/* ASCII Hex Decode */

typedef struct fz_ahxd_s fz_ahxd;

struct fz_ahxd_s
{
	fz_filter super;
	int odd;
	int a;
};

static inline int iswhite(int a)
{
	switch (a) {
	case '\n': case '\r': case '\t': case ' ':
	case '\0': case '\f': case '\b': case 0177:
		return 1;
	}
	return 0;
}

static inline int ishex(int a)
{
	return (a >= 'A' && a <= 'F') ||
		(a >= 'a' && a <= 'f') ||
		(a >= '0' && a <= '9');
}

static inline int fromhex(int a)
{
	if (a >= 'A' && a <= 'F')
		return a - 'A' + 0xA;
	if (a >= 'a' && a <= 'f')
		return a - 'a' + 0xA;
	if (a >= '0' && a <= '9')
		return a - '0';
	return 0;
}

fz_filter *
fz_newahxd(fz_obj *params)
{
	FZ_NEWFILTER(fz_ahxd, f, ahxd);
	f->odd = 0;
	f->a = 0;
	return (fz_filter *)f;
}

void
fz_dropahxd(fz_filter *f)
{
}

fz_error
fz_processahxd(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_ahxd *f = (fz_ahxd*)filter;
	int b, c;

	while (1)
	{
		if (in->rp == in->wp)
			return fz_ioneedin;

		if (out->wp == out->ep)
			return fz_ioneedout;

		c = *in->rp++;

		if (ishex(c)) {
			if (!f->odd) {
				f->a = fromhex(c);
				f->odd = 1;
			}
			else {
				b = fromhex(c);
				*out->wp++ = (f->a << 4) | b;
				f->odd = 0;
			}
		}

		else if (c == '>') {
			if (f->odd)
				*out->wp++ = (f->a << 4);
			return fz_iodone;
		}

		else if (!iswhite(c)) {
			return fz_throw("bad data in ahxd: '%c'", c);
		}
	}
}

/* ASCII 85 Decode */

typedef struct fz_a85d_s fz_a85d;

struct fz_a85d_s
{
	fz_filter super;
	unsigned long word;
	int count;
};

fz_filter *
fz_newa85d(fz_obj *params)
{
	FZ_NEWFILTER(fz_a85d, f, a85d);
	f->word = 0;
	f->count = 0;
	return (fz_filter *)f;
}

void
fz_dropa85d(fz_filter *f)
{
}

fz_error
fz_processa85d(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_a85d *f = (fz_a85d*)filter;
	int c;

	while (1)
	{
		if (in->rp == in->wp)
			return fz_ioneedin;

		c = *in->rp++;

		if (c >= '!' && c <= 'u') {
			if (f->count == 4) {
				if (out->wp + 4 > out->ep) {
					in->rp --;
					return fz_ioneedout;
				}

				f->word = f->word * 85 + (c - '!');

				*out->wp++ = (f->word >> 24) & 0xff;
				*out->wp++ = (f->word >> 16) & 0xff;
				*out->wp++ = (f->word >> 8) & 0xff;
				*out->wp++ = (f->word) & 0xff;

				f->word = 0;
				f->count = 0;
			}
			else {
				f->word = f->word * 85 + (c - '!');
				f->count ++;
			}
		}

		else if (c == 'z' && f->count == 0) {
			if (out->wp + 4 > out->ep) {
				in->rp --;
				return fz_ioneedout;
			}
			*out->wp++ = 0;
			*out->wp++ = 0;
			*out->wp++ = 0;
			*out->wp++ = 0;
		}

		else if (c == '~') {
			if (in->rp == in->wp) {
				in->rp --;
				return fz_ioneedin;
			}

			c = *in->rp++;

			if (c != '>') {
				return fz_throw("bad eod marker in a85d");
			}

			if (out->wp + f->count - 1 > out->ep) {
				in->rp -= 2;
				return fz_ioneedout;
			}

			switch (f->count) {
			case 0:
				break;
			case 1:
				return fz_throw("partial final byte in a85d");
			case 2:
				f->word = f->word * (85L * 85 * 85) + 0xffffffL;
				goto o1;
			case 3:
				f->word = f->word * (85L * 85) + 0xffffL;
				goto o2;
			case 4:
				f->word = f->word * 85 + 0xffL;
				*(out->wp+2) = f->word >> 8;
				o2:				*(out->wp+1) = f->word >> 16;
				o1:				*(out->wp+0) = f->word >> 24;
				out->wp += f->count - 1;
				break;
			}
			return fz_iodone;
		}

		else if (!iswhite(c)) {
			return fz_throw("bad data in a85d: '%c'", c);
		}
	}
}

/* Run Length Decode */

fz_filter *
fz_newrld(fz_obj *params)
{
	FZ_NEWFILTER(fz_filter, f, rld);
	return f;
}

void
fz_droprld(fz_filter *rld)
{
}

fz_error
fz_processrld(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	int run, i;
	unsigned char c;

	while (1)
	{
		if (in->rp == in->wp)
		{
			if (in->eof)
			{
				return fz_iodone;
			}
			return fz_ioneedin;
		}

		if (out->wp == out->ep)
			return fz_ioneedout;

		run = *in->rp++;

		if (run == 128)
		{
			return fz_iodone;
		}

		else if (run < 128) {
			run = run + 1;
			if (in->rp + run > in->wp) {
				in->rp --;
				return fz_ioneedin;
			}
			if (out->wp + run > out->ep) {
				in->rp --;
				return fz_ioneedout;
			}
			for (i = 0; i < run; i++)
				*out->wp++ = *in->rp++;
		}

		else if (run > 128) {
			run = 257 - run;
			if (in->rp + 1 > in->wp) {
				in->rp --;
				return fz_ioneedin;
			}
			if (out->wp + run > out->ep) {
				in->rp --;
				return fz_ioneedout;
			}
			c = *in->rp++;
			for (i = 0; i < run; i++)
				*out->wp++ = c;
		}
	}
}

/* RC4 Filter */

typedef struct fz_arc4c_s fz_arc4c;

struct fz_arc4c_s
{
	fz_filter super;
	fz_arc4 arc4;
};

fz_filter *
fz_newarc4filter(unsigned char *key, unsigned keylen)
{
	FZ_NEWFILTER(fz_arc4c, f, arc4filter);
	fz_arc4init(&f->arc4, key, keylen);
	return (fz_filter *)f;
}

void
fz_droparc4filter(fz_filter *f)
{
}

fz_error
fz_processarc4filter(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_arc4c *f = (fz_arc4c*)filter;
	int n;

	while (1)
	{
		if (in->rp + 1 > in->wp) {
			if (in->eof)
				return fz_iodone;
			return fz_ioneedin;
		}
		if (out->wp + 1 > out->ep)
			return fz_ioneedout;

		n = MIN(in->wp - in->rp, out->ep - out->wp);
		fz_arc4encrypt(&f->arc4, out->wp, in->rp, n);
		in->rp += n;
		out->wp += n;
	}
}

/* AES Filter */

typedef struct fz_aesd_s fz_aesd;

struct fz_aesd_s
{
	fz_filter super;
	fz_aes aes;
	unsigned char iv[16];
	int ivcount;
};

fz_filter *
fz_newaesdfilter(unsigned char *key, unsigned keylen)
{
	FZ_NEWFILTER(fz_aesd, f, aesdfilter);
	aes_setkey_dec(&f->aes, key, keylen * 8);
	f->ivcount = 0;
	return (fz_filter *)f;
}

void
fz_dropaesdfilter(fz_filter *f)
{
}

fz_error
fz_processaesdfilter(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_aesd *f = (fz_aesd*)filter;
	int n;

	while (1)
	{
		if (in->rp + 16 > in->wp)
		{
			if (in->eof)
				return fz_iodone;
			return fz_ioneedin;
		}

		if (f->ivcount < 16)
		{
			f->iv[f->ivcount++] = *in->rp++;
		}
		else
		{
			if (out->wp + 16 > out->ep)
				return fz_ioneedout;

			n = MIN(in->wp - in->rp, out->ep - out->wp);
			n = (n / 16) * 16;

			aes_crypt_cbc(&f->aes, AES_DECRYPT, n, f->iv, in->rp, out->wp);
			in->rp += n;
			out->wp += n;

			/* Remove padding bytes */
			if (in->eof && in->rp == in->wp)
			{
				int pad = out->wp[-1];
				if (pad < 1 || pad > 16)
					return fz_throw("aes padding out of range: %d", pad);
				out->wp -= pad;
			}
		}
	}
}
