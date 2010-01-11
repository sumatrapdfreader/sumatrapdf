#include "fitz_base.h"
#include "fitz_stream.h"

#include <jpeglib.h>

#include <setjmp.h>

typedef struct fz_dctd_s fz_dctd;

struct myerrmgr
{
	struct jpeg_error_mgr super;
	jmp_buf jb;
	char msg[JMSG_LENGTH_MAX];
};

static void myerrexit(j_common_ptr cinfo)
{
	struct myerrmgr *err = (struct myerrmgr *)cinfo->err;
	char msgbuf[JMSG_LENGTH_MAX];
	err->super.format_message(cinfo, msgbuf);
	strlcpy(err->msg, msgbuf, sizeof err->msg);
	longjmp(err->jb, 1);
}

static void myoutmess(j_common_ptr cinfo)
{
	struct myerrmgr *err = (struct myerrmgr *)cinfo->err;
	char msgbuf[JMSG_LENGTH_MAX];
	err->super.format_message(cinfo, msgbuf);
	fz_warn("jpeg error: %s", msgbuf);
}

static void myiniterr(struct myerrmgr *err)
{
	jpeg_std_error(&err->super);
	err->super.error_exit = myerrexit;
	err->super.output_message = myoutmess;
}

struct mysrcmgr
{
	struct jpeg_source_mgr super;
	fz_buffer *buf;
	int skip;
};

struct fz_dctd_s
{
	fz_filter super;
	struct jpeg_decompress_struct cinfo;
	struct mysrcmgr src;
	struct myerrmgr err;
	int colortransform;
	int stage;
};

static void myinitsource(j_decompress_ptr cinfo) { /* empty */ }
static int myfillinput(j_decompress_ptr cinfo) { return FALSE; }
static void mytermsource(j_decompress_ptr cinfo) { /* empty */ }

static void myskipinput(j_decompress_ptr cinfo, long n)
{
	struct mysrcmgr *src = (struct mysrcmgr *)cinfo->src;
	fz_buffer *in = src->buf;

	assert(src->skip == 0);

	in->rp = in->wp - src->super.bytes_in_buffer;

	if (in->rp + n > in->wp) {
		src->skip = (in->rp + n) - in->wp;
		in->rp = in->wp;
	}
	else {
		src->skip = 0;
		in->rp += n;
	}

	src->super.bytes_in_buffer = in->wp - in->rp;
	src->super.next_input_byte = in->rp;
}

fz_filter *
fz_newdctd(fz_obj *params)
{
	fz_obj *obj;
	int colortransform;

	FZ_NEWFILTER(fz_dctd, d, dctd);

	colortransform = -1; /* "unset" */

	if (params)
	{
		obj = fz_dictgets(params, "ColorTransform");
		if (obj)
			colortransform = fz_toint(obj);
	}

	d->colortransform = colortransform;
	d->stage = 0;

	/* setup error callback first thing */
	myiniterr(&d->err);
	d->cinfo.err = (struct jpeg_error_mgr*) &d->err;

	if (setjmp(d->err.jb))
		fz_warn("cannot initialise jpeg: %s", d->err.msg);

	/* create decompression object. this zeroes cinfo except for err. */
	jpeg_create_decompress(&d->cinfo);

	/* prepare source manager */
	d->cinfo.src = (struct jpeg_source_mgr *)&d->src;
	d->src.super.init_source = myinitsource;
	d->src.super.fill_input_buffer = myfillinput;
	d->src.super.skip_input_data = myskipinput;
	d->src.super.resync_to_restart = jpeg_resync_to_restart;
	d->src.super.term_source = mytermsource;

	d->src.super.bytes_in_buffer = 0;
	d->src.super.next_input_byte = nil;
	d->src.skip = 0;

	/* speed up jpeg decoding a bit */
	d->cinfo.dct_method = JDCT_FASTEST;
	d->cinfo.do_fancy_upsampling = FALSE;

	return (fz_filter *)d;
}

void
fz_dropdctd(fz_filter *filter)
{
	fz_dctd *d = (fz_dctd*)filter;
	if (setjmp(d->err.jb)) {
		fz_warn("jpeg error: jpeg_destroy_decompress: %s", d->err.msg);
		return;
	}
	jpeg_destroy_decompress(&d->cinfo);
}

fz_error
fz_processdctd(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_dctd *d = (fz_dctd*)filter;
	int b;
	int i;
	int stride;
	JSAMPROW scanlines[1];

	d->src.buf = in;

	/* skip any bytes left over from myskipinput() */
	if (d->src.skip > 0) {
		if (in->rp + d->src.skip > in->wp) {
			d->src.skip = (in->rp + d->src.skip) - in->wp;
			in->rp = in->wp;
			goto needinput;
		}
		else {
			in->rp += d->src.skip;
			d->src.skip = 0;
		}
	}

	d->src.super.bytes_in_buffer = in->wp - in->rp;
	d->src.super.next_input_byte = in->rp;

	if (setjmp(d->err.jb))
	{
		return fz_throw("cannot decode jpeg: %s", d->err.msg);
	}

	switch (d->stage)
	{
	case 0:
		i = jpeg_read_header(&d->cinfo, TRUE);
		if (i == JPEG_SUSPENDED)
			goto needinput;

		/* default value if ColorTransform is not set */
		if (d->colortransform == -1)
		{
			if (d->cinfo.num_components == 3)
				d->colortransform = 1;
			else
				d->colortransform = 0;
		}

		if (d->cinfo.saw_Adobe_marker)
			d->colortransform = d->cinfo.Adobe_transform;

		/* Guess the input colorspace, and set output colorspace accordingly */
		switch (d->cinfo.num_components)
		{
		case 3:
			if (d->colortransform)
				d->cinfo.jpeg_color_space = JCS_YCbCr;
			else
				d->cinfo.jpeg_color_space = JCS_RGB;
			break;
		case 4:
			if (d->colortransform)
				d->cinfo.jpeg_color_space = JCS_YCCK;
			else
				d->cinfo.jpeg_color_space = JCS_CMYK;
			break;
		}

		/* fall through */
		d->stage = 1;

	case 1:
		b = jpeg_start_decompress(&d->cinfo);
		if (b == FALSE)
			goto needinput;

		/* fall through */
		d->stage = 2;

	case 2:
		stride = d->cinfo.output_width * d->cinfo.output_components;

		while (d->cinfo.output_scanline < d->cinfo.output_height)
		{
			if (out->wp + stride > out->ep)
				goto needoutput;

			scanlines[0] = out->wp;

			i = jpeg_read_scanlines(&d->cinfo, scanlines, 1);

			if (i == 0)
				goto needinput;

			out->wp += stride;
		}

		/* fall through */
		d->stage = 3;

	case 3:
		b = jpeg_finish_decompress(&d->cinfo);
		if (b == FALSE)
			goto needinput;
		d->stage = 4;
		in->rp = in->wp - d->src.super.bytes_in_buffer;
		return fz_iodone;
	}

needinput:
	in->rp = in->wp - d->src.super.bytes_in_buffer;
	return fz_ioneedin;

needoutput:
	in->rp = in->wp - d->src.super.bytes_in_buffer;
	return fz_ioneedout;
}

