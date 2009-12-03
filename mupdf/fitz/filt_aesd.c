#include "fitz_base.h"
#include "fitz_stream.h"

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

