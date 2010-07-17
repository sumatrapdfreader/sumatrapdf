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
			fz_dropbuffer(img->samples);
		fz_free(img);
	}
}

static fz_error
pdf_loadimageheader(pdf_image **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict)
{
	pdf_image *img;
	fz_error error;
	fz_obj *obj, *res;
	int i;

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
		fz_warn("image bit depth is zero"); /* okay for JPX */

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
		for (i = 0; i < img->n * 2; i++)
		{
			if (i & 1)
			{
				if (img->indexed)
					img->decode[i] = (1 << img->bpc) - 1;
				else
					img->decode[i] = 1;
			}
			else
			{
				img->decode[i] = 0;
			}
		}
	}

	/* Not allowed for inline images */
	obj = fz_dictgetsa(dict, "SMask", "Mask");
	if (pdf_isstream(xref, fz_tonum(obj), fz_togen(obj)))
	{
		error = pdf_loadimage(&img->mask, xref, rdb, obj);
		if (error)
		{
			pdf_dropimage(img);
			return fz_rethrow(error, "cannot load image mask/softmask");
		}
		img->mask->imagemask = 1; /* TODO: this triggers bit inversion later. should we? */
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
	else
	{
		/* add an entry for alpha channel */
		img->decode[img->n * 2] = 0;
		img->decode[img->n * 2 + 1] = 1;
	}

	img->stride = (img->w * img->n * img->bpc + 7) / 8;

	pdf_logimage("size %dx%d n=%d bpc=%d (imagemask=%d)\n", img->w, img->h, img->n, img->bpc, img->imagemask);

	*imgp = img;
	return fz_okay;
}

fz_error
pdf_loadinlineimage(pdf_image **imgp, pdf_xref *xref,
	fz_obj *rdb, fz_obj *dict, fz_stream *file)
{
	fz_error error;
	pdf_image *img;
	fz_filter *filter;
	fz_stream *subfile;
	int n;

	pdf_logimage("load inline image {\n");

	error = pdf_loadimageheader(&img, xref, rdb, dict);
	if (error)
		return fz_rethrow(error, "cannot load inline image");

	filter = pdf_buildinlinefilter(xref, dict);
	subfile = fz_openfilter(filter, file);

	img->samples = fz_newbuffer(img->h * img->stride);
	error = fz_read(&n, file, img->samples->bp, img->h * img->stride);
	if (error)
	{
		pdf_dropimage(img);
		return fz_rethrow(error, "cannot load inline image data");
	}
	img->samples->wp += n;

	if (img->imagemask)
	{
		/* 0=opaque and 1=transparent so we need to invert */
		unsigned char *p;
		for (p = img->samples->bp; p < img->samples->ep; p++)
			*p = ~*p;
	}

	fz_dropstream(subfile);
	fz_dropfilter(filter);

	pdf_logimage("}\n");

	*imgp = img;
	return fz_okay;
}

fz_error
pdf_loadimage(pdf_image **imgp, pdf_xref *xref, fz_obj *rdb, fz_obj *dict)
{
	fz_error error;
	pdf_image *img;

	if ((*imgp = pdf_finditem(xref->store, pdf_dropimage, dict)))
	{
		pdf_keepimage(*imgp);
		return fz_okay;
	}

	pdf_logimage("load image (%d %d R) {\n", fz_tonum(dict), fz_togen(dict));

	error = pdf_loadimageheader(&img, xref, rdb, dict);
	if (error)
		return fz_rethrow(error, "cannot load image (%d %d R)", fz_tonum(dict), fz_togen(dict));

	error = pdf_loadstream(&img->samples, xref, fz_tonum(dict), fz_togen(dict));
	if (error)
	{
		pdf_dropimage(img);
		return fz_rethrow(error, "cannot load image data (%d %d R)", fz_tonum(dict), fz_togen(dict));
	}

	/* Pad truncated images */
	if (img->samples->wp - img->samples->bp < img->stride * img->h)
	{
		fz_warn("padding truncated image");
		fz_resizebuffer(img->samples, img->stride * img->h);
		memset(img->samples->wp, 0, img->samples->ep - img->samples->wp);
		img->samples->wp = img->samples->bp + img->stride * img->h;
	}

	if (img->imagemask)
	{
		/* 0=opaque and 1=transparent so we need to invert */
		unsigned char *p;
		for (p = img->samples->bp; p < img->samples->ep; p++)
			*p = ~*p;
	}

	pdf_logimage("}\n");

	pdf_storeitem(xref->store, pdf_keepimage, pdf_dropimage, dict, img);

	*imgp = img;
	return fz_okay;
}

static void
pdf_maskcolorkey(fz_pixmap *pix, int n, int *colorkey, int scale)
{
	unsigned char *p = pix->samples;
	int len = pix->w * pix->h;
	int k, t;
	while (len--)
	{
		t = 1;
		for (k = 0; k < n; k++)
			if (p[k] < colorkey[k * 2] * scale || p[k] > colorkey[k * 2 + 1] * scale)
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
	switch (img->bpc)
	{
	case 1: scale = 255; break;
	case 2: scale = 85; break;
	case 4: scale = 17; break;
	}

	fz_unpacktile(tile, img->samples->bp, img->n, img->bpc, img->stride, scale);

	if (img->usecolorkey)
		pdf_maskcolorkey(tile, img->n, img->colorkey, scale);

	if (img->indexed)
	{
		fz_pixmap *conv;
		float decode[4];

		decode[0] = img->decode[0] * scale / 255;
		decode[1] = img->decode[1] * scale / 255;
		decode[2] = img->decode[2];
		decode[3] = img->decode[3];

		fz_decodetile(tile, decode);

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
