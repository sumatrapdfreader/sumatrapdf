#include "fitz_base.h"
#include "fitz_stream.h"

typedef struct fz_copyfilter_s fz_copyfilter;

struct fz_copyfilter_s
{
	fz_filter super;
};

fz_error
fz_newcopyfilter(fz_filter **fp)
{
    FZ_NEWFILTER(fz_copyfilter, f, copyfilter);
    return fz_okay;
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

