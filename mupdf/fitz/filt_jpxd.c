#include "fitz_base.h"
#include "fitz_stream.h"

#define OPJ_STATIC
#include <openjpeg.h>

typedef struct fz_jpxd_s fz_jpxd;

struct fz_jpxd_s
{
	fz_filter super;
	opj_event_mgr_t evtmgr;
	opj_dparameters_t params;
	opj_dinfo_t *info;
	opj_image_t *image;
	int stage;
	int x, y, k;
};

static void fz_opj_error_callback(const char *msg, void *client_data)
{
	fprintf(stderr, "openjpeg error: %s", msg);
}

static void fz_opj_warning_callback(const char *msg, void *client_data)
{
	fprintf(stderr, "openjpeg warning: %s", msg);
}

static void fz_opj_info_callback(const char *msg, void *client_data)
{
	/* fprintf(stdout, "openjpeg info: %s", msg); */
}


fz_filter *
fz_newjpxd(fz_obj *params)
{
	FZ_NEWFILTER(fz_jpxd, d, jpxd);

	d->info = nil;
	d->image = nil;
	d->stage = 0;

	d->x = 0;
	d->y = 0;
	d->k = 0;

	memset(&d->evtmgr, 0, sizeof(d->evtmgr));
	d->evtmgr.error_handler = fz_opj_error_callback;
	d->evtmgr.warning_handler = fz_opj_warning_callback;
	d->evtmgr.info_handler = fz_opj_info_callback;

	opj_set_default_decoder_parameters(&d->params);

	d->info = opj_create_decompress(CODEC_JP2);
	if (!d->info)
		fz_warn("assert: opj_create_decompress failed");

	opj_set_event_mgr((opj_common_ptr)d->info, &d->evtmgr, stderr);
	opj_setup_decoder(d->info, &d->params);

	return (fz_filter*)d;
}

void
fz_dropjpxd(fz_filter *filter)
{
	fz_jpxd *d = (fz_jpxd*)filter;
	if (d->image) opj_image_destroy(d->image);
	if (d->info) opj_destroy_decompress(d->info);
}

fz_error
fz_processjpxd(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_jpxd *d = (fz_jpxd*)filter;
	int n, w, h, depth, sgnd;
	int k, v;

	opj_cio_t *cio;

	switch (d->stage)
	{
	case 0: goto input;
	case 1: goto decode;
	case 2: goto output;
	}

input:
	/* Wait until we have the entire file in the input buffer */
	if (!in->eof)
		return fz_ioneedin;

	d->stage = 1;

decode:
	cio = opj_cio_open((opj_common_ptr)d->info, in->rp, in->wp - in->rp);
	in->rp = in->wp;

	d->image = opj_decode(d->info, cio);
	if (!d->image)
	{
		opj_cio_close(cio);
		return fz_throw("opj_decode failed");
	}

	opj_cio_close(cio);

	d->stage = 2;

	for (k = 1; k < d->image->numcomps; k++)
	{
		if (d->image->comps[k].w != d->image->comps[0].w)
			return fz_throw("image components have different width");
		if (d->image->comps[k].h != d->image->comps[0].h)
			return fz_throw("image components have different height");
		if (d->image->comps[k].prec != d->image->comps[0].prec)
			return fz_throw("image components have different precision");
	}

	{
		n = d->image->numcomps;
		w = d->image->comps[0].w;
		h = d->image->comps[0].h;
		depth = d->image->comps[0].prec;
	}

output:
	n = d->image->numcomps;
	w = d->image->comps[0].w;
	h = d->image->comps[0].h;
	depth = d->image->comps[0].prec;
	sgnd = d->image->comps[0].sgnd;

	while (d->y < h)
	{
		while (d->x < w)
		{
			while (d->k < n)
			{
				if (out->wp == out->ep)
					return fz_ioneedout;

				v = d->image->comps[d->k].data[d->y * w + d->x];
				if (sgnd)
					v = v + (1 << (depth - 1));
				if (depth > 8)
					v = v >> (depth - 8);

				*out->wp++ = v;

				d->k ++;
			}
			d->x ++;
			d->k = 0;
		}
		d->y ++;
		d->x = 0;
	}

	return fz_iodone;
}

