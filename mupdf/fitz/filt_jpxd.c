#include "fitz.h"
#include "mupdf.h"

#define OPJ_STATIC
#include <openjpeg.h>

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
	char buf[80];
	int len = fz_strlcpy(buf, msg, sizeof buf);
	if (len > 0) buf[len-1] = 0;
	pdf_logimage("%s\n", buf);
}

fz_error
pdf_loadjpximage(pdf_image **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict)
{
	pdf_image *img;
	opj_event_mgr_t evtmgr;
	opj_dparameters_t params;
	opj_dinfo_t *info;
	opj_cio_t *cio;
	opj_image_t *jpx;
	fz_error error;
	fz_buffer *buf;
	unsigned char *p;
	int format;
	int n, w, h, depth, sgnd;
	int x, y, k, v;

	error = pdf_loadstream(&buf, xref, fz_tonum(dict), fz_togen(dict));
	if (error)
		return fz_throw("read error in jpx image");

	if (buf->len < 2)
	{
		fz_dropbuffer(buf);
		return fz_throw("not enough data to determine image format");
	}

	/* Check for SOC marker -- if found we have a bare J2K stream */
	if (buf->data[0] == 0xFF && buf->data[1] == 0x4F)
		format = CODEC_J2K;
	else
		format = CODEC_JP2;

	memset(&evtmgr, 0, sizeof(evtmgr));
	evtmgr.error_handler = fz_opj_error_callback;
	evtmgr.warning_handler = fz_opj_warning_callback;
	evtmgr.info_handler = fz_opj_info_callback;

	opj_set_default_decoder_parameters(&params);

	info = opj_create_decompress(format);
	opj_set_event_mgr((opj_common_ptr)info, &evtmgr, stderr);
	opj_setup_decoder(info, &params);

	cio = opj_cio_open((opj_common_ptr)info, buf->data, buf->len);

	jpx = opj_decode(info, cio);

	opj_cio_close(cio);
	opj_destroy_decompress(info);
	fz_dropbuffer(buf);

	if (!jpx)
		return fz_throw("opj_decode failed");

	for (k = 1; k < jpx->numcomps; k++)
	{
		if (jpx->comps[k].w != jpx->comps[0].w)
			return fz_throw("image components have different width");
		if (jpx->comps[k].h != jpx->comps[0].h)
			return fz_throw("image components have different height");
		if (jpx->comps[k].prec != jpx->comps[0].prec)
			return fz_throw("image components have different precision");
	}

	n = jpx->numcomps;
	w = jpx->comps[0].w;
	h = jpx->comps[0].h;
	depth = jpx->comps[0].prec;
	sgnd = jpx->comps[0].sgnd;

	img = fz_malloc(sizeof(pdf_image));
	memset(img, 0, sizeof(pdf_image));
	img->refs = 1;
	img->w = w;
	img->h = h;
	img->bpc = 8;
	img->n = n;
	img->stride = w * n;
	img->samples = fz_malloc(w * n * h);

	switch (n)
	{
	case 1: img->colorspace = fz_devicegray; break;
	case 3: img->colorspace = fz_devicergb; break;
	case 4: img->colorspace = fz_devicecmyk; break;
	default:
		/* TODO: SMaskInData */
		opj_image_destroy(jpx);
		fz_free(img);
		return fz_throw("unknown jpx colorspace (%d components)", n);
	}

	for (k = 0; k < n; k++)
	{
		img->decode[k * 2] = 0;
		img->decode[k * 2 + 1] = 1;
	}

	p = img->samples;
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			for (k = 0; k < n; k++)
			{
				v = jpx->comps[k].data[y * w + x];
				if (sgnd)
					v = v + (1 << (depth - 1));
				if (depth > 8)
					v = v >> (depth - 8);
				*p++ = v;
			}
		}
	}

	opj_image_destroy(jpx);

	*imgp = img;
	return fz_okay;
}
