/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   This code is in public domain.
*/
#include "fitz-base.h"
#include "fitz-stream.h"

typedef struct fz_aesc_s fz_aesc;

struct fz_aesc_s
{
	fz_filter super;
	fz_aes aes;
	unsigned char iv[16];
	int ivhas;
};

fz_error *
fz_newaesfilter(fz_filter **fp, unsigned char *key, unsigned keylen)
{
	FZ_NEWFILTER(fz_aesc, f, aesfilter);
	fz_aesinit(&f->aes, key, keylen);
	f->ivhas = 0;
	return fz_okay;
}

void
fz_dropaesfilter(fz_filter *f)
{
}

fz_error *
fz_processaesfilter(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_aesc *f = (fz_aesc*)filter;
	int n;

	while (1)
	{
		if (in->rp + 1 > in->wp) {
			if (in->eof)
				return fz_iodone;
			return fz_ioneedin;
		}

		/* first 16 bytes is iv for cbc mode of aes encryption */
		if (f->ivhas < 16) 
		{
			int ivlen;
			n = in->wp - in->rp;
			ivlen = MIN(n, 16-f->ivhas);
			memcpy(&f->iv[f->ivhas], in->rp, ivlen);
			in->rp += ivlen;
			f->ivhas += ivlen;
			assert(f->ivhas <= 16);
			fz_setiv(&f->aes, &f->iv[0]);
		}

		if (f->ivhas < 16)
			continue;

		if (out->wp + 1 > out->ep)
			return fz_ioneedout;

		n = MIN(in->wp - in->rp, out->ep - out->wp);
		fz_aesdecrypt(&f->aes, out->wp, in->rp, n);
		in->rp += n;
		out->wp += n;
	}
}

