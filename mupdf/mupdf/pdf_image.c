/*
 * TODO: this needs serious cleaning up, and error checking.
 */

#include "fitz.h"
#include "mupdf.h"

static void
pdf_freeimage(fz_image *fzimg)
{
	pdf_image *img = (pdf_image*)fzimg;
	fz_dropbuffer(img->samples);
	if (img->indexed)
		fz_dropcolorspace((fz_colorspace *) img->indexed);
	if (img->mask)
		fz_dropimage(img->mask);
}

fz_error
pdf_loadinlineimage(pdf_image **imgp, pdf_xref *xref,
	fz_obj *rdb, fz_obj *dict, fz_stream *file)
{
	fz_error error;
	pdf_image *img;
	fz_filter *filter;
	fz_obj *f;
	fz_obj *cs;
	fz_obj *d;
	int ismask;
	int i;

	img = fz_malloc(sizeof(pdf_image));

	pdf_logimage("load inline image %p {\n", img);

	img->super.refs = 1;
	img->super.cs = nil;
	img->super.loadtile = pdf_loadtile;
	img->super.freefunc = pdf_freeimage;
	img->super.n = 0;
	img->super.a = 0;
	img->indexed = nil;
	img->usecolorkey = 0;
	img->mask = nil;

	img->super.w = fz_toint(fz_dictgetsa(dict, "Width", "W"));
	img->super.h = fz_toint(fz_dictgetsa(dict, "Height", "H"));
	img->bpc = fz_toint(fz_dictgetsa(dict, "BitsPerComponent", "BPC"));
	ismask = fz_tobool(fz_dictgetsa(dict, "ImageMask", "IM"));
	d = fz_dictgetsa(dict, "Decode", "D");
	cs = fz_dictgetsa(dict, "ColorSpace", "CS");
	if (img->super.w == 0)
		fz_warn("inline image width is zero or undefined");
	if (img->super.h == 0)
		fz_warn("inline image height is zero or undefined");

	pdf_logimage("size %dx%d %d\n", img->super.w, img->super.h, img->bpc);

	if (cs)
	{
		fz_obj *csd = nil;
		fz_obj *cso = nil;

		/* Attempt to lookup any name in the resource dictionary */
		if (fz_isname(cs))
		{
			csd = fz_dictgets(rdb, "ColorSpace");
			cso = fz_dictget(csd, cs);
		}

		/* If no colorspace found in resource dictionary,
		* assume that reference is a standard name */
		if (!cso)
			cso = cs;

		error = pdf_loadcolorspace(&img->super.cs, xref, cso);
		if (error)
		{
			pdf_freeimage((fz_image *) img);
			return fz_rethrow(error, "cannot load colorspace");
		}

		if (!img->super.cs)
			return fz_throw("image is missing colorspace");

		if (!strcmp(img->super.cs->name, "Indexed"))
		{
			pdf_logimage("indexed\n");
			img->indexed = (pdf_indexed*)img->super.cs;
			img->super.cs = img->indexed->base;
			fz_keepcolorspace(img->super.cs);
		}

		pdf_logimage("colorspace %s\n", img->super.cs->name);

		img->super.n = img->super.cs->n;
		img->super.a = 0;
	}

	if (ismask)
	{
		pdf_logimage("is mask\n");
		if (img->super.cs)
		{
			fz_warn("masks can not have colorspace, proceeding anyway.");
			fz_dropcolorspace(img->super.cs);
			img->super.cs = nil;
		}
		if (img->bpc != 1)
			fz_warn("masks can only have one component, proceeding anyway.");

		img->bpc = 1;
		img->super.n = 0;
		img->super.a = 1;
	}
	else if (!cs)
		return fz_throw("image is missing colorspace");

	if (fz_isarray(d))
	{
		pdf_logimage("decode array\n");
		if (img->indexed)
			for (i = 0; i < 2; i++)
				img->decode[i] = fz_toreal(fz_arrayget(d, i));
		else
			for (i = 0; i < (img->super.n + img->super.a) * 2; i++)
				img->decode[i] = fz_toreal(fz_arrayget(d, i));
	}
	else
	{
		if (img->indexed)
			for (i = 0; i < 2; i++)
				img->decode[i] = i & 1 ? (1 << img->bpc) - 1 : 0;
		else
			for (i = 0; i < (img->super.n + img->super.a) * 2; i++)
				img->decode[i] = i & 1;
	}

	if (img->indexed)
		img->stride = (img->super.w * img->bpc + 7) / 8;
	else
		img->stride = (img->super.w * (img->super.n + img->super.a) * img->bpc + 7) / 8;

	/* load image data */

	f = fz_dictgetsa(dict, "Filter", "F");
	if (!f || (fz_isarray(f) && fz_arraylen(f) == 0))
	{
		img->samples = fz_newbuffer(img->super.h * img->stride);

		error = fz_read(&i, file, img->samples->bp, img->super.h * img->stride);
		if (error)
			return error;

		img->samples->wp += img->super.h * img->stride;
	}
	else
	{
		fz_stream *tempfile;

		filter = pdf_buildinlinefilter(xref, dict);

		tempfile = fz_openrfilter(filter, file);

		img->samples = fz_readall(tempfile, img->stride * img->super.h);
		fz_dropstream(tempfile);

		fz_dropfilter(filter);
	}

	/* 0 means opaque and 1 means transparent, so we invert to get alpha */
	if (ismask)
	{
		unsigned char *p;
		for (p = img->samples->bp; p < img->samples->ep; p++)
			*p = ~*p;
	}

	pdf_logimage("}\n");

	*imgp = img;
	return fz_okay;
}

static void
loadcolorkey(int *colorkey, int bpc, int indexed, fz_obj *obj)
{
	int scale = 1;
	int i;

	pdf_logimage("keyed mask\n");

	if (!indexed)
	{
		switch (bpc)
		{
		case 1: scale = 255; break;
		case 2: scale = 85; break;
		case 4: scale = 17; break;
		case 8: scale = 1; break;
		}
	}

	for (i = 0; i < fz_arraylen(obj); i++)
		colorkey[i] = fz_toint(fz_arrayget(obj, i)) * scale;
}

/* TODO error cleanup */
fz_error
pdf_loadimage(pdf_image **imgp, pdf_xref *xref, fz_obj *dict)
{
	fz_error error;
	pdf_image *img;
	pdf_image *mask;
	int ismask;
	fz_obj *obj;
	int i;

	int w, h, bpc;
	int n = 0;
	int a = 0;
	int usecolorkey = 0;
	fz_colorspace *cs = nil;
	pdf_indexed *indexed = nil;
	int stride;
	int realsize, expectedsize;

	if ((*imgp = pdf_finditem(xref->store, PDF_KIMAGE, dict)))
	{
		fz_keepimage((fz_image*)*imgp);
		return fz_okay;
	}

	img = fz_malloc(sizeof(pdf_image));

	pdf_logimage("load image (%d %d R) ptr=%p {\n", fz_tonum(dict), fz_togen(dict), img);

	/*
	 * Dimensions, BPC and ColorSpace
	 */

	w = fz_toint(fz_dictgets(dict, "Width"));
	if (w == 0)
		fz_warn("image width is zero or undefined");

	h = fz_toint(fz_dictgets(dict, "Height"));
	if (h == 0)
		fz_warn("image height is zero or undefined");

	bpc = fz_toint(fz_dictgets(dict, "BitsPerComponent"));

	pdf_logimage("size %dx%d %d\n", w, h, bpc);

	cs = nil;
	obj = fz_dictgets(dict, "ColorSpace");
	if (obj)
	{
		error = pdf_loadcolorspace(&cs, xref, obj);
		if (error)
		{
			fz_dropimage((fz_image *) img);
			return fz_rethrow(error, "cannot load colorspace");
		}

		if (!strcmp(cs->name, "Indexed"))
		{
			pdf_logimage("indexed\n");
			indexed = (pdf_indexed*)cs;
			cs = indexed->base;
			fz_keepcolorspace(cs);
		}
		n = cs->n;
		a = 0;

		pdf_logimage("colorspace %s\n", cs->name);
	}

	/*
	 * ImageMask, Mask and SoftMask
	 */

	mask = nil;

	ismask = fz_tobool(fz_dictgets(dict, "ImageMask"));
	if (ismask)
	{
		pdf_logimage("is mask\n");
		if (cs)
		{
			fz_warn("masks can not have colorspace, proceeding anyway.");
			fz_dropcolorspace(cs);
			cs = nil;
		}
		if (bpc != 0 && bpc != 1)
			fz_warn("masks can only have one component, proceeding anyway.");

		bpc = 1;
		n = 0;
		a = 1;
	}
	else
	{
		if (!cs)
			return fz_throw("colorspace missing for image");
		if (bpc == 0)
			return fz_throw("image has no bits per component");
	}

	obj = fz_dictgets(dict, "SMask");
	if (fz_isindirect(obj))
	{
		pdf_logimage("has soft mask\n");

		error = pdf_loadimage(&mask, xref, obj);
		if (error)
			return error;

		if (mask->super.cs && mask->super.cs != pdf_devicegray)
			return fz_throw("syntaxerror: SMask must be DeviceGray");

		mask->super.cs = nil;
		mask->super.n = 0;
		mask->super.a = 1;
	}

	obj = fz_dictgets(dict, "Mask");
	if (fz_isindirect(obj))
	{
		if (fz_isarray(obj))
		{
			usecolorkey = 1;
			loadcolorkey(img->colorkey, bpc, indexed != nil, obj);
		}
		else
		{
			pdf_logimage("has mask\n");
			if (mask)
			{
				fz_warn("image has both a mask and a soft mask. ignoring the soft mask.");
				pdf_freeimage((fz_image*)mask);
				mask = nil;
			}
			error = pdf_loadimage(&mask, xref, obj);
			if (error)
				return error;
		}
	}
	else if (fz_isarray(obj))
	{
		usecolorkey = 1;
		loadcolorkey(img->colorkey, bpc, indexed != nil, obj);
	}

	/*
	 * Decode
	 */

	obj = fz_dictgets(dict, "Decode");
	if (fz_isarray(obj))
	{
		pdf_logimage("decode array\n");
		if (indexed)
			for (i = 0; i < 2; i++)
				img->decode[i] = fz_toreal(fz_arrayget(obj, i));
		else
			for (i = 0; i < (n + a) * 2; i++)
				img->decode[i] = fz_toreal(fz_arrayget(obj, i));
	}
	else
	{
		if (indexed)
			for (i = 0; i < 2; i++)
				img->decode[i] = i & 1 ? (1 << bpc) - 1 : 0;
		else
			for (i = 0; i < (n + a) * 2; i++)
				img->decode[i] = i & 1;
	}

	/*
	 * Load samples
	 */

	if (indexed)
		stride = (w * bpc + 7) / 8;
	else
		stride = (w * (n + a) * bpc + 7) / 8;

	error = pdf_loadstream(&img->samples, xref, fz_tonum(dict), fz_togen(dict));
	if (error)
	{
		/* TODO: colorspace? */
		fz_free(img);
		return error;
	}

	expectedsize = stride *h;
	realsize = img->samples->wp - img->samples->bp;
	if (realsize < expectedsize)
	{
		/* don't treat truncated image as fatal - get as much as possible and
		fill the rest with 0 */
		fz_buffer *buf;
		buf = fz_newbuffer(expectedsize);
		memset(buf->bp, 0, expectedsize);
		memmove(buf->bp, img->samples->bp, realsize);
		buf->wp = buf->bp + expectedsize;
		fz_dropbuffer(img->samples);
		img->samples = buf;
		fz_warn("truncated image; proceeding anyway");
	}

	/* 0 means opaque and 1 means transparent, so we invert to get alpha */
	if (ismask)
	{
		unsigned char *p;
		for (p = img->samples->bp; p < img->samples->ep; p++)
			*p = ~*p;
	}

	/*
	 * Create image object
	 */

	img->super.refs = 1;
	img->super.loadtile = pdf_loadtile;
	img->super.freefunc = pdf_freeimage;
	img->super.cs = cs;
	img->super.w = w;
	img->super.h = h;
	img->super.n = n;
	img->super.a = a;
	img->indexed = indexed;
	img->stride = stride;
	img->bpc = bpc;
	img->mask = (fz_image*)mask;
	img->usecolorkey = usecolorkey;

	pdf_logimage("}\n");

	pdf_storeitem(xref->store, PDF_KIMAGE, dict, img);

	*imgp = img;
	return fz_okay;
}

static void
maskcolorkey(fz_pixmap *pix, int *colorkey)
{
	unsigned char *p = pix->samples;
	int i, k, t;
	for (i = 0; i < pix->w * pix->h; i++)
	{
		t = 1;
		for (k = 1; k < pix->n; k++)
			if (p[k] < colorkey[k * 2 - 2] || p[k] > colorkey[k * 2 - 1])
				t = 0;
		if (t)
			for (k = 0; k < pix->n; k++)
				p[k] = 0;
		p += pix->n;
	}
}

static void
maskcolorkeyindexed(fz_pixmap *ind, fz_pixmap *pix, int *colorkey)
{
	unsigned char *s = ind->samples;
	unsigned char *d = pix->samples;
	int i, k;

	for (i = 0; i < pix->w * pix->h; i++)
	{
		if (s[0] >= colorkey[0] && s[0] <= colorkey[1])
			for (k = 0; k < pix->n; k++)
				d[k] = 0;
		s += ind->n;
		d += pix->n;
	}
}

fz_error
pdf_loadtile(fz_image *img, fz_pixmap *tile)
{
	pdf_image *src = (pdf_image*)img;
	void (*tilefunc)(unsigned char*,int,unsigned char*, int, int, int, int);
	fz_error error;

	assert(tile->x == 0); /* can't handle general tile yet, only y-banding */

	assert(tile->n == img->n + 1);
	assert(tile->x >= 0);
	assert(tile->y >= 0);
	assert(tile->x + tile->w <= img->w);
	assert(tile->y + tile->h <= img->h);

	switch (src->bpc)
	{
	case 1: tilefunc = fz_loadtile1; break;
	case 2: tilefunc = fz_loadtile2; break;
	case 4: tilefunc = fz_loadtile4; break;
	case 8: tilefunc = fz_loadtile8; break;
	case 16: tilefunc = fz_loadtile16; break;
	default:
		return fz_throw("rangecheck: unsupported bit depth: %d", src->bpc);
	}

	if (src->indexed)
	{
		fz_pixmap *tmp;
		int x, y, k, i;
		int bpcfact = 1;

		error = fz_newpixmap(&tmp, tile->x, tile->y, tile->w, tile->h, 1);
		if (error)
			return error;

		switch (src->bpc)
		{
		case 1: bpcfact = 255; break;
		case 2: bpcfact = 85; break;
		case 4: bpcfact = 17; break;
		case 8: bpcfact = 1; break;
		}

		tilefunc(src->samples->rp + (tile->y * src->stride), src->stride,
			tmp->samples, tmp->w,
			tmp->w, tmp->h, 0);

		for (y = 0; y < tile->h; y++)
		{
			int dn = tile->n;
			unsigned char *dst = tile->samples + y * tile->w * dn;
			unsigned char *st = tmp->samples + y * tmp->w;
			unsigned char *index = src->indexed->lookup;
			int high = src->indexed->high;
			int sn = src->indexed->base->n;
			for (x = 0; x < tile->w; x++)
			{
				dst[x * dn] = 255; /* alpha */
				i = st[x] / bpcfact;
				i = CLAMP(i, 0, high);
				for (k = 0; k < sn; k++)
				{
					dst[x * dn + k + 1] = index[i * sn + k];
				}
			}
		}

		if (src->usecolorkey)
			maskcolorkeyindexed(tmp, tile, src->colorkey);

		fz_droppixmap(tmp);
	}

	else
	{
		tilefunc(src->samples->rp + (tile->y * src->stride), src->stride,
			tile->samples, tile->w * tile->n,
			tile->w * (img->n + img->a), tile->h, img->a ? 0 : img->n);
		if (src->usecolorkey)
			maskcolorkey(tile, src->colorkey);
		fz_decodetile(tile, !img->a, src->decode);
	}

	return fz_okay;
}

