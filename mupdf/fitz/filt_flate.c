#include "fitz.h"

#include <zlib.h>

typedef struct fz_flate_s fz_flate;

struct fz_flate_s
{
	fz_filter super;
	z_stream z;
};

static void *
zmalloc(void *opaque, unsigned int items, unsigned int size)
{
	return fz_malloc(items * size);
}

static void
zfree(void *opaque, void *ptr)
{
	fz_free(ptr);
}

fz_filter *
fz_newflated(fz_obj *params)
{
	int ei;

	FZ_NEWFILTER(fz_flate, f, flated);

	f->z.zalloc = zmalloc;
	f->z.zfree = zfree;
	f->z.opaque = nil;
	f->z.next_in = nil;
	f->z.avail_in = 0;

	ei = inflateInit(&f->z);

	if (ei != Z_OK)
	{
		fz_warn("zlib error: inflateInit: %s", f->z.msg);
	}

	return (fz_filter*)f;
}

void
fz_dropflated(fz_filter *f)
{
	z_streamp zp = &((fz_flate*)f)->z;
	int err;

	err = inflateEnd(zp);
	if (err != Z_OK)
		fz_warn("inflateEnd: %s", zp->msg);
}

fz_error
fz_processflated(fz_filter *f, fz_buffer *in, fz_buffer *out)
{
	z_streamp zp = &((fz_flate*)f)->z;
	int err;

	if (in->rp == in->wp && !in->eof)
		return fz_ioneedin;
	if (out->wp == out->ep)
		return fz_ioneedout;

	zp->next_in = in->rp;
	zp->avail_in = in->wp - in->rp;

	zp->next_out = out->wp;
	zp->avail_out = out->ep - out->wp;

	err = inflate(zp, Z_SYNC_FLUSH);

	in->rp = in->wp - zp->avail_in;
	out->wp = out->ep - zp->avail_out;

	if (err == Z_DATA_ERROR && in->eof && in->rp == in->wp)
	{
		fz_warn("ignoring zlib error: %s", zp->msg);
		return fz_iodone;
	}
	else if (err == Z_STREAM_END || err == Z_BUF_ERROR)
	{
		return fz_iodone;
	}
	else if (err == Z_OK)
	{
		if (in->rp == in->wp && !in->eof)
			return fz_ioneedin;
		if (out->wp == out->ep)
			return fz_ioneedout;
		return fz_ioneedin; /* hmm, what's going on here? */
	}
	else
	{
		return fz_throw("zlib error: inflate: %s", zp->msg);
	}
}
