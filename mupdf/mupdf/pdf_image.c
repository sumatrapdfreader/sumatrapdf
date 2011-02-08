#include "fitz.h"
#include "mupdf.h"

/* TODO: store JPEG compressed samples */
/* TODO: store flate compressed samples */

static void
pdf_maskcolorkey(fz_pixmap *pix, int n, int *colorkey)
{
	unsigned char *p = pix->samples;
	int len = pix->w * pix->h;
	int k, t;
	while (len--)
	{
		t = 1;
		for (k = 0; k < n; k++)
			if (p[k] < colorkey[k * 2] || p[k] > colorkey[k * 2 + 1])
				t = 0;
		if (t)
			for (k = 0; k < pix->n; k++)
				p[k] = 0;
		p += pix->n;
	}
}

static fz_error
pdf_loadimageimp(fz_pixmap **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict, fz_stream *cstm, int forcemask)
{
	fz_stream *stm;
	fz_pixmap *tile;
	fz_obj *obj, *res;
	fz_error error;

	int w, h, bpc, n;
	int imagemask;
	int interpolate;
	int indexed;
	fz_colorspace *colorspace;
	fz_pixmap *mask; /* explicit mask/softmask image */
	int usecolorkey;
	int colorkey[FZ_MAXCOLORS * 2];
	float decode[FZ_MAXCOLORS * 2];

	int scale;
	int stride;
	unsigned char *samples;
	int i, len;

	w = fz_toint(fz_dictgetsa(dict, "Width", "W"));
	h = fz_toint(fz_dictgetsa(dict, "Height", "H"));
	bpc = fz_toint(fz_dictgetsa(dict, "BitsPerComponent", "BPC"));
	imagemask = fz_tobool(fz_dictgetsa(dict, "ImageMask", "IM"));
	interpolate = fz_tobool(fz_dictgetsa(dict, "Interpolate", "I"));

	indexed = 0;
	usecolorkey = 0;
	colorspace = nil;
	mask = nil;

	if (imagemask)
		bpc = 1;

	if (w == 0)
		return fz_throw("image width is zero");
	if (h == 0)
		return fz_throw("image height is zero");
	if (bpc == 0)
		return fz_throw("image depth is zero");
	if (w > (1 << 16))
		return fz_throw("image is too wide");
	if (h > (1 << 16))
		return fz_throw("image is too high");

	obj = fz_dictgetsa(dict, "ColorSpace", "CS");
	if (obj && !imagemask && !forcemask)
	{
		/* colorspace resource lookup is only done for inline images */
		if (fz_isname(obj))
		{
			res = fz_dictget(fz_dictgets(rdb, "ColorSpace"), obj);
			if (res)
				obj = res;
		}

		error = pdf_loadcolorspace(&colorspace, xref, obj);
		if (error)
			return fz_rethrow(error, "cannot load image colorspace");

		if (!strcmp(colorspace->name, "Indexed"))
			indexed = 1;

		n = colorspace->n;
	}
	else
	{
		n = 1;
	}

	obj = fz_dictgetsa(dict, "Decode", "D");
	if (obj)
	{
		for (i = 0; i < n * 2; i++)
			decode[i] = fz_toreal(fz_arrayget(obj, i));
	}
	else
	{
		float maxval = indexed ? (1 << bpc) - 1 : 1;
		for (i = 0; i < n * 2; i++)
			decode[i] = i & 1 ? maxval : 0;
	}

	obj = fz_dictgetsa(dict, "SMask", "Mask");
	if (fz_isdict(obj))
	{
		/* Not allowed for inline images */
		if (!cstm)
		{
			error = pdf_loadimageimp(&mask, xref, rdb, obj, nil, 1);
			if (error)
			{
				if (colorspace)
					fz_dropcolorspace(colorspace);
				return fz_rethrow(error, "cannot load image mask/softmask");
			}
		}
	}
	else if (fz_isarray(obj))
	{
		usecolorkey = 1;
		for (i = 0; i < n * 2; i++)
			colorkey[i] = fz_toint(fz_arrayget(obj, i));
	}

	stride = (w * n * bpc + 7) / 8;
	samples = fz_calloc(h, stride);

	if (cstm)
	{
		stm = pdf_openinlinestream(cstm, xref, dict, stride * h);
	}
	else
	{
		error = pdf_openstream(&stm, xref, fz_tonum(dict), fz_togen(dict));
		if (error)
		{
			if (colorspace)
				fz_dropcolorspace(colorspace);
			if (mask)
				fz_droppixmap(mask);
			return fz_rethrow(error, "cannot open image data stream (%d 0 R)", fz_tonum(dict));
		}
	}

	len = fz_read(stm, samples, h * stride);
	if (len < 0)
	{
		fz_close(stm);
		if (colorspace)
			fz_dropcolorspace(colorspace);
		if (mask)
			fz_droppixmap(mask);
		return fz_rethrow(n, "cannot read image data");
	}

	/* Make sure we read the EOF marker (for inline images only) */
	if (cstm)
	{
		unsigned char tbuf[512];
		int tlen = fz_read(stm, tbuf, sizeof tbuf);
		if (tlen < 0)
			fz_catch(tlen, "ignoring error at end of image");
		if (tlen > 0)
			fz_warn("ignoring garbage at end of image");
	}

	fz_close(stm);

	/* Pad truncated images */
	if (len < stride * h)
	{
		fz_warn("padding truncated image (%d 0 R)", fz_tonum(dict));
		memset(samples + len, 0, stride * h - len);
	}

	/* Invert 1-bit image masks */
	if (imagemask)
	{
		/* 0=opaque and 1=transparent so we need to invert */
		unsigned char *p = samples;
		len = h * stride;
		for (i = 0; i < len; i++)
			p[i] = ~p[i];
	}

	pdf_logimage("size %dx%d n=%d bpc=%d imagemask=%d indexed=%d\n", w, h, n, bpc, imagemask, indexed);

	/* Unpack samples into pixmap */

	tile = fz_newpixmap(colorspace, 0, 0, w, h);

	scale = 1;
	if (!indexed)
	{
		switch (bpc)
		{
		case 1: scale = 255; break;
		case 2: scale = 85; break;
		case 4: scale = 17; break;
		}
	}

	fz_unpacktile(tile, samples, n, bpc, stride, scale);

	if (usecolorkey)
		pdf_maskcolorkey(tile, n, colorkey);

	if (indexed)
	{
		fz_pixmap *conv;

		fz_decodeindexedtile(tile, decode, (1 << bpc) - 1);

		conv = pdf_expandindexedpixmap(tile);
		fz_droppixmap(tile);
		tile = conv;
	}
	else
	{
		fz_decodetile(tile, decode);
	}

	if (colorspace)
		fz_dropcolorspace(colorspace);

	tile->mask = mask;
	tile->interpolate = interpolate;

	fz_free(samples);

	*imgp = tile;
	return fz_okay;
}

fz_error
pdf_loadinlineimage(fz_pixmap **pixp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict, fz_stream *file)
{
	fz_error error;

	pdf_logimage("load inline image {\n");

	error = pdf_loadimageimp(pixp, xref, rdb, dict, file, 0);
	if (error)
		return fz_rethrow(error, "cannot load inline image");

	pdf_logimage("}\n");

	return fz_okay;
}

int
pdf_isjpximage(fz_obj *dict)
{
	fz_obj *filter;
	int i;

	filter = fz_dictgets(dict, "Filter");
	if (!strcmp(fz_toname(filter), "JPXDecode"))
		return 1;
	for (i = 0; i < fz_arraylen(filter); i++)
		if (!strcmp(fz_toname(fz_arrayget(filter, i)), "JPXDecode"))
			return 1;
	return 0;
}

static fz_error
pdf_loadjpximage(fz_pixmap **imgp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	fz_buffer *buf;
	fz_pixmap *img;
	fz_obj *obj;

	pdf_logimage("jpeg2000\n");

	error = pdf_loadstream(&buf, xref, fz_tonum(dict), fz_togen(dict));
	if (error)
		return fz_rethrow(error, "cannot load jpx image data");

	error = fz_loadjpximage(&img, buf->data, buf->len);
	if (error)
	{
		fz_dropbuffer(buf);
		return fz_rethrow(error, "cannot load jpx image");
	}

	fz_dropbuffer(buf);

	obj = fz_dictgetsa(dict, "SMask", "Mask");
	if (fz_isdict(obj))
	{
		error = pdf_loadimageimp(&img->mask, xref, nil, obj, nil, 1);
		if (error)
		{
			fz_droppixmap(img);
			return fz_rethrow(error, "cannot load image mask/softmask");
		}
	}

	obj = fz_dictgets(dict, "ColorSpace");
	if (obj)
	{
		fz_colorspace *original = img->colorspace;
		img->colorspace = nil;

		error = pdf_loadcolorspace(&img->colorspace, xref, obj);
		if (error)
		{
			fz_dropcolorspace(original);
			return fz_rethrow(error, "cannot load image colorspace");
		}

		if (original->n != img->colorspace->n)
		{
			fz_warn("jpeg-2000 colorspace (%s) does not match promised colorspace (%s)", original->name, img->colorspace->name);
			fz_dropcolorspace(img->colorspace);
			img->colorspace = original;
		}
		else
			fz_dropcolorspace(original);

		if (!strcmp(img->colorspace->name, "Indexed"))
		{
			fz_pixmap *conv;
			conv = pdf_expandindexedpixmap(img);
			fz_droppixmap(img);
			img = conv;
		}
	}

	*imgp = img;
	return fz_okay;
}

fz_error
pdf_loadimage(fz_pixmap **pixp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;

	if ((*pixp = pdf_finditem(xref->store, fz_droppixmap, dict)))
	{
		fz_keeppixmap(*pixp);
		return fz_okay;
	}

	pdf_logimage("load image (%d 0 R) {\n", fz_tonum(dict));

	/* special case for JPEG2000 images */
	if (pdf_isjpximage(dict))
	{
		error = pdf_loadjpximage(pixp, xref, dict);
		if (error)
			return fz_rethrow(error, "cannot load jpx image (%d 0 R)", fz_tonum(dict));
	}
	else
	{
		error = pdf_loadimageimp(pixp, xref, nil, dict, nil, 0);
		if (error)
			return fz_rethrow(error, "cannot load image (%d 0 R)", fz_tonum(dict));
	}

	pdf_storeitem(xref->store, fz_keeppixmap, fz_droppixmap, dict, *pixp);

	pdf_logimage("}\n");

	return fz_okay;
}
