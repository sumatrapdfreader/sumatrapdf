#include "fitz.h"
#include "mupdf.h"

/* TODO: special case JPXDecode image loading */
/* TODO: store JPEG compressed samples */
/* TODO: store flate compressed samples */

pdf_image *
pdf_keepimage(pdf_image *image)
{
	image->refs ++;
	return image;
}

void
pdf_dropimage(pdf_image *img)
{
	if (img && --img->refs == 0)
	{
		if (img->colorspace)
			fz_dropcolorspace(img->colorspace);
		if (img->mask)
			pdf_dropimage(img->mask);
		if (img->samples)
			fz_free(img->samples);
		fz_free(img);
	}
}

static fz_error
pdf_loadimageimp(pdf_image **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict, fz_stream *cstm)
{
	pdf_image *img;
	fz_stream *stm;
	fz_error error;
	fz_obj *obj, *res;
	int i, n;

	img = fz_malloc(sizeof(pdf_image));
	memset(img, 0, sizeof(pdf_image));

	img->refs = 1;
	img->w = fz_toint(fz_dictgetsa(dict, "Width", "W"));
	img->h = fz_toint(fz_dictgetsa(dict, "Height", "H"));
	img->bpc = fz_toint(fz_dictgetsa(dict, "BitsPerComponent", "BPC"));
	img->imagemask = fz_tobool(fz_dictgetsa(dict, "ImageMask", "IM"));
	img->interpolate = fz_tobool(fz_dictgetsa(dict, "Interpolate", "I"));

	if (img->imagemask)
		img->bpc = 1;

	if (img->w == 0)
		fz_warn("image width is zero");
	if (img->h == 0)
		fz_warn("image height is zero");
	if (img->bpc == 0)
		img->bpc = 8; /* for JPX */

	obj = fz_dictgetsa(dict, "ColorSpace", "CS");
	if (obj)
	{
		if (fz_isname(obj))
		{
			res = fz_dictget(fz_dictgets(rdb, "ColorSpace"), obj);
			if (res)
				obj = res;
		}

		error = pdf_loadcolorspace(&img->colorspace, xref, obj);
		if (error)
		{
			pdf_dropimage(img);
			return fz_rethrow(error, "cannot load image colorspace");
		}

		if (!strcmp(img->colorspace->name, "Indexed"))
			img->indexed = 1;

		img->n = img->colorspace->n;
	}
	else
	{
		img->n = 1;
	}

	obj = fz_dictgetsa(dict, "Decode", "D");
	if (obj)
	{
		for (i = 0; i < img->n * 2; i++)
			img->decode[i] = fz_toreal(fz_arrayget(obj, i));
	}
	else
	{
		float maxval = img->indexed ? (1 << img->bpc) - 1 : 1;
		for (i = 0; i < img->n * 2; i++)
			img->decode[i] = i & 1 ? maxval : 0;
	}

	/* Not allowed for inline images */
	obj = fz_dictgetsa(dict, "SMask", "Mask");
	if (fz_isdict(obj))
	{
		error = pdf_loadimage(&img->mask, xref, rdb, obj);
		if (error)
		{
			pdf_dropimage(img);
			return fz_rethrow(error, "cannot load image mask/softmask");
		}
		img->mask->imagemask = 1;
		if (img->mask->colorspace)
		{
			fz_dropcolorspace(img->mask->colorspace);
			img->mask->colorspace = nil;
		}
	}
	else if (fz_isarray(obj))
	{
		img->usecolorkey = 1;
		for (i = 0; i < img->n * 2; i++)
			img->colorkey[i] = fz_toint(fz_arrayget(obj, i));
	}

	if (img->imagemask)
	{
		if (img->colorspace)
		{
			fz_dropcolorspace(img->colorspace);
			img->colorspace = nil;
		}
	}

	img->stride = (img->w * img->n * img->bpc + 7) / 8;
	img->samples = fz_malloc(img->h * img->stride);

	if (cstm)
	{
		stm = pdf_openinlinestream(cstm, xref, dict, img->stride * img->h);
	}
	else
	{
		error = pdf_openstream(&stm, xref, fz_tonum(dict), fz_togen(dict));
		if (error)
		{
			pdf_dropimage(img);
			return fz_rethrow(error, "cannot open image data stream (%d 0 R)", fz_tonum(dict));
		}
	}

	n = fz_read(stm, img->samples, img->h * img->stride);
	if (n < 0)
	{
		fz_close(stm);
		pdf_dropimage(img);
		return fz_rethrow(n, "cannot read image data");
	}

	fz_close(stm);

	/* Pad truncated images */
	if (n < img->stride * img->h)
	{
		fz_warn("padding truncated image (%d 0 R)", fz_tonum(dict));
		memset(img->samples + n, 0, img->stride * img->h - n);
	}

	if (img->imagemask)
	{
		/* 0=opaque and 1=transparent so we need to invert */
		unsigned char *p = img->samples;
		n = img->h * img->stride;
		for (i = 0; i < n; i++)
			p[i] = ~p[i];
	}

	pdf_logimage("size %dx%d n=%d bpc=%d (imagemask=%d)\n", img->w, img->h, img->n, img->bpc, img->imagemask);

	*imgp = img;
	return fz_okay;
}

fz_error
pdf_loadinlineimage(pdf_image **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict, fz_stream *file)
{
	fz_error error;
	pdf_image *img;

	pdf_logimage("load inline image {\n");

	error = pdf_loadimageimp(&img, xref, rdb, dict, file);
	if (error)
		return fz_rethrow(error, "cannot load inline image");

	pdf_logimage("}\n");

	*imgp = img;
	return fz_okay;
}

static int
pdf_isjpximage(fz_obj *filter)
{
	int i;
	if (!strcmp(fz_toname(filter), "JPXDecode"))
		return 1;
	for (i = 0; i < fz_arraylen(filter); i++)
		if (!strcmp(fz_toname(fz_arrayget(filter, i)), "JPXDecode"))
			return 1;
	return 0;
}

fz_error
pdf_loadimage(pdf_image **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict)
{
	fz_error error;
	pdf_image *img;
	fz_obj *obj;

	if ((*imgp = pdf_finditem(xref->store, pdf_dropimage, dict)))
	{
		pdf_keepimage(*imgp);
		return fz_okay;
	}

	pdf_logimage("load image (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

	/* special case for JPEG2000 images */
	obj = fz_dictgets(dict, "Filter");
	if (pdf_isjpximage(obj))
	{
		error = pdf_loadjpximage(&img, xref, rdb, dict);
		if (error)
			return fz_rethrow(error, "cannot load jpx image");
	}
	else
	{
		error = pdf_loadimageimp(&img, xref, rdb, dict, nil);
		if (error)
			return fz_rethrow(error, "cannot load image (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	pdf_storeitem(xref->store, pdf_keepimage, pdf_dropimage, dict, img);

	pdf_logimage("}\n");

	*imgp = img;
	return fz_okay;
}

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

fz_pixmap *
pdf_loadtile(pdf_image *img /* ...bbox/y+h should go here... */)
{
	fz_pixmap *tile;
	int scale;

	tile = fz_newpixmap(img->colorspace, 0, 0, img->w, img->h);

	scale = 1;
	if (!img->indexed)
	{
		switch (img->bpc)
		{
		case 1: scale = 255; break;
		case 2: scale = 85; break;
		case 4: scale = 17; break;
		}
	}

	fz_unpacktile(tile, img->samples, img->n, img->bpc, img->stride, scale);

	if (img->usecolorkey)
		pdf_maskcolorkey(tile, img->n, img->colorkey);

	if (img->indexed)
	{
		fz_pixmap *conv;

		fz_decodeindexedtile(tile, img->decode, (1 << img->bpc) - 1);

		conv = pdf_expandindexedpixmap(tile);
		fz_droppixmap(tile);
		tile = conv;
	}
	else
	{
		fz_decodetile(tile, img->decode);
	}

	return tile;
}
