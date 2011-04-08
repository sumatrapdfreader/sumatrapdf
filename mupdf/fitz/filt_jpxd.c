#include "fitz.h"

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
	/* fprintf(stderr, "openjpeg info: %s", msg); */
}

fz_error
fz_load_jpx_image(fz_pixmap **imgp, unsigned char *data, int size, fz_colorspace *defcs)
{
	fz_pixmap *img;
	opj_event_mgr_t evtmgr;
	opj_dparameters_t params;
	opj_dinfo_t *info;
	opj_cio_t *cio;
	opj_image_t *jpx;
	fz_colorspace *colorspace;
	unsigned char *p;
	int format;
	int a, n, w, h, depth, sgnd;
	int x, y, k, v;

	if (size < 2)
		return fz_throw("not enough data to determine image format");

	/* Check for SOC marker -- if found we have a bare J2K stream */
	if (data[0] == 0xFF && data[1] == 0x4F)
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

	cio = opj_cio_open((opj_common_ptr)info, data, size);

	jpx = opj_decode(info, cio);

	opj_cio_close(cio);
	opj_destroy_decompress(info);

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

	if (jpx->color_space == CLRSPC_SRGB && n == 4) { n = 3; a = 1; }
	else if (jpx->color_space == CLRSPC_SYCC && n == 4) { n = 3; a = 1; }
	else if (n == 2) { n = 1; a = 1; }
	else if (n > 4) { n = 4; a = 1; }
	else { a = 0; }

	if (defcs)
	{
		if (defcs->n == n)
		{
			colorspace = defcs;
		}
		else
		{
			fz_warn("jpx file and dict colorspaces do not match");
			defcs = NULL;
		}
	}

	if (!defcs)
	{
		switch (n)
		{
		case 1: colorspace = fz_device_gray; break;
		case 3: colorspace = fz_device_rgb; break;
		case 4: colorspace = fz_device_cmyk; break;
		}
	}

	img = fz_new_pixmap_with_limit(colorspace, w, h);
	if (!img)
	{
		opj_image_destroy(jpx);
		return fz_throw("out of memory");
	}

	p = img->samples;
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			for (k = 0; k < n + a; k++)
			{
				v = jpx->comps[k].data[y * w + x];
				if (sgnd)
					v = v + (1 << (depth - 1));
				if (depth > 8)
					v = v >> (depth - 8);
				*p++ = v;
			}
			if (!a)
				*p++ = 255;
		}
	}

	if (a)
	{
		if (n == 4)
		{
			fz_pixmap *tmp = fz_new_pixmap(fz_device_rgb, w, h);
			fz_convert_pixmap(img, tmp);
			fz_drop_pixmap(img);
			img = tmp;
		}
		fz_premultiply_pixmap(img);
	}

	opj_image_destroy(jpx);

	*imgp = img;
	return fz_okay;
}
