#include "fitz_base.h"
#include "fitz_stream.h"

fz_error
fz_process(fz_filter *f, fz_buffer *in, fz_buffer *out)
{
	fz_error reason;
	unsigned char *oldrp;
	unsigned char *oldwp;

	oldrp = in->rp;
	oldwp = out->wp;

	if (f->done)
		return fz_iodone;

	assert(!out->eof);

	reason = f->process(f, in, out);

	assert(in->rp <= in->wp);
	assert(out->wp <= out->ep);

	f->consumed = in->rp > oldrp;
	f->produced = out->wp > oldwp;
	f->count += out->wp - oldwp;

	/* iodone or error */
	if (reason != fz_ioneedin && reason != fz_ioneedout)
	{
		if (reason != fz_iodone)
			reason = fz_rethrow(reason, "cannot process filter");
		out->eof = 1;
		f->done = 1;
	}

	return reason;
}

fz_filter *
fz_keepfilter(fz_filter *f)
{
	f->refs ++;
	return f;
}

void
fz_dropfilter(fz_filter *f)
{
	if (--f->refs == 0)
	{
		if (f->drop)
			f->drop(f);
		fz_free(f);
	}
}

