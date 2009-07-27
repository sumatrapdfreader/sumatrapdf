#include "fitz_base.h"
#include "fitz_stream.h"

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

fz_error
fz_newflated(fz_filter **fp, fz_obj *params)
{
	fz_error eo;
	fz_obj *obj;
	int zipfmt;
	int ei;

	FZ_NEWFILTER(fz_flate, f, flated);

	f->z.zalloc = zmalloc;
	f->z.zfree = zfree;
	f->z.opaque = nil;
	f->z.next_in = nil;
	f->z.avail_in = 0;

	zipfmt = 0;

	if (params)
	{
		obj = fz_dictgets(params, "ZIP");
		if (obj) zipfmt = fz_tobool(obj);
	}

	if (zipfmt)
	{
		/* if windowbits is negative the zlib header is skipped */
		ei = inflateInit2(&f->z, -15);
	}
	else
		ei = inflateInit(&f->z);

	if (ei != Z_OK)
	{
		eo = fz_throw("zlib error: inflateInit: %s", f->z.msg);
		fz_free(f);
		return eo;
	}

	return fz_okay;
}

void
fz_dropflated(fz_filter *f)
{
	z_streamp zp = &((fz_flate*)f)->z;
	int err;

	err = inflateEnd(zp);
	if (err != Z_OK)
		fprintf(stderr, "inflateEnd: %s", zp->msg);
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

	err = inflate(zp, Z_NO_FLUSH);

	/* Make sure we call it with Z_FINISH at the end of input */
	if (err == Z_OK && in->eof && zp->avail_in == 0 && zp->avail_out > 0)
		err = inflate(zp, Z_FINISH);

	in->rp = in->wp - zp->avail_in;
	out->wp = out->ep - zp->avail_out;

	if (err == Z_STREAM_END || err == Z_BUF_ERROR)
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

fz_error
fz_newflatee(fz_filter **fp, fz_obj *params)
{
	fz_obj *obj;
	fz_error eo;
	int effort;
	int zipfmt;
	int ei;

	FZ_NEWFILTER(fz_flate, f, flatee);

	effort = -1;
	zipfmt = 0;

	if (params)
	{
		obj = fz_dictgets(params, "Effort");
		if (obj) effort = fz_toint(obj);
		obj = fz_dictgets(params, "ZIP");
		if (obj) zipfmt = fz_tobool(obj);
	}

	f->z.zalloc = zmalloc;
	f->z.zfree = zfree;
	f->z.opaque = nil;
	f->z.next_in = nil;
	f->z.avail_in = 0;

	if (zipfmt)
		ei = deflateInit2(&f->z, effort, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
	else
		ei = deflateInit(&f->z, effort);

	if (ei != Z_OK)
	{
		eo = fz_throw("zlib error: deflateInit: %s", f->z.msg);
		fz_free(f);
		return eo;
	}

	return fz_okay;
}

void
fz_dropflatee(fz_filter *f)
{
	z_streamp zp = &((fz_flate*)f)->z;
	int err;

	err = deflateEnd(zp);
	if (err != Z_OK)
		fprintf(stderr, "deflateEnd: %s", zp->msg);

	fz_free(f);
}

fz_error
fz_processflatee(fz_filter *f, fz_buffer *in, fz_buffer *out)
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

	err = deflate(zp, in->eof ? Z_FINISH : Z_NO_FLUSH);

	in->rp = in->wp - zp->avail_in;
	out->wp = out->ep - zp->avail_out;

	if (err == Z_STREAM_END)
	{
		return fz_iodone;
	}
	else if (err == Z_OK)
	{
		if (in->rp == in->wp && !in->eof)
			return fz_ioneedin;
		if (out->wp == out->ep)
			return fz_ioneedout;
		return fz_ioneedin; /* hmm? */
	}
	else
	{
		return fz_throw("zlib error: deflate: %s", zp->msg);
	}
}

