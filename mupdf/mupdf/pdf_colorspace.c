#include "fitz.h"
#include "mupdf.h"

/* ICCBased */

static fz_error
loadiccbased(fz_colorspace **csp, pdf_xref *xref, fz_obj *dict)
{
	int n;

	pdf_logrsrc("load ICCBased\n");

	n = fz_toint(fz_dictgets(dict, "N"));

	switch (n)
	{
	case 1: *csp = fz_devicegray; return fz_okay;
	case 3: *csp = fz_devicergb; return fz_okay;
	case 4: *csp = fz_devicecmyk; return fz_okay;
	}

	return fz_throw("syntaxerror: ICCBased must have 1, 3 or 4 components");
}

/* Lab */

static inline float fung(float x)
{
	if (x >= 6.0f / 29.0f)
		return x * x * x;
	return (108.0f / 841.0f) * (x - (4.0f / 29.0f));
}

static inline float invg(float x)
{
	if (x > 0.008856f)
		return powf(x, 1.0f / 3.0f);
	return (7.787f * x) + (16.0f / 116.0f);
}

static void
labtoxyz(fz_colorspace *cs, float *lab, float *xyz)
{
	/* input is in range (0..100, -128..127, -128..127) not (0..1, 0..1, 0..1) */
	float lstar, astar, bstar, l, m, n;
	lstar = lab[0];
	astar = lab[1];
	bstar = lab[2];
	m = (lstar + 16) / 116;
	l = m + astar / 500;
	n = m - bstar / 200;
	xyz[0] = fung(l);
	xyz[1] = fung(m);
	xyz[2] = fung(n);
}

static void
xyztolab(fz_colorspace *cs, float *xyz, float *lab)
{
	float lstar, astar, bstar;
	float yyn = xyz[1];
	if (yyn < 0.008856f)
		lstar = 116.0f * yyn * (1.0f / 3.0f) - 16.0f;
	else
		lstar = 903.3f * yyn;
	astar = 500 * (invg(xyz[0]) - invg(xyz[1]));
	bstar = 200 * (invg(xyz[1]) - invg(xyz[2]));
	lab[0] = lstar;
	lab[1] = astar;
	lab[2] = bstar;
}

static fz_colorspace kdevicelab = { -1, "Lab", 3, labtoxyz, xyztolab };
static fz_colorspace *fz_devicelab = &kdevicelab;

/* Separation and DeviceN */

struct separation
{
	fz_colorspace *base;
	pdf_function *tint;
};

static void
separationtoxyz(fz_colorspace *cs, float *color, float *xyz)
{
	struct separation *sep = cs->data;
	fz_error error;
	float alt[FZ_MAXCOLORS];

	error = pdf_evalfunction(sep->tint, color, cs->n, alt, sep->base->n);
	if (error)
	{
		fz_catch(error, "cannot evaluate separation function");
		xyz[0] = 0;
		xyz[1] = 0;
		xyz[2] = 0;
		return;
	}

	sep->base->toxyz(sep->base, alt, xyz);
}

static void
freeseparation(fz_colorspace *cs)
{
	struct separation *sep = cs->data;
	fz_dropcolorspace(sep->base);
	pdf_dropfunction(sep->tint);
	fz_free(sep);
}

static fz_error
loadseparation(fz_colorspace **csp, pdf_xref *xref, fz_obj *array)
{
	fz_error error;
	fz_colorspace *cs;
	struct separation *sep;
	fz_obj *nameobj = fz_arrayget(array, 1);
	fz_obj *baseobj = fz_arrayget(array, 2);
	fz_obj *tintobj = fz_arrayget(array, 3);
	fz_colorspace *base;
	pdf_function *tint;
	int n;

	pdf_logrsrc("load Separation {\n");

	if (fz_isarray(nameobj))
		n = fz_arraylen(nameobj);
	else
		n = 1;

	if (n > FZ_MAXCOLORS)
		return fz_throw("too many components in colorspace");

	pdf_logrsrc("n = %d\n", n);

	error = pdf_loadcolorspace(&base, xref, baseobj);
	if (error)
		return fz_rethrow(error, "cannot load base colorspace (%d %d R)", fz_tonum(baseobj), fz_togen(baseobj));

	error = pdf_loadfunction(&tint, xref, tintobj);
	if (error)
	{
		fz_dropcolorspace(base);
		return fz_rethrow(error, "cannot load tint function (%d %d R)", fz_tonum(tintobj), fz_togen(tintobj));
	}

	sep = fz_malloc(sizeof(struct separation));
	sep->base = base;
	sep->tint = tint;

	cs = fz_newcolorspace(n == 1 ? "Separation" : "DeviceN", n);
	cs->toxyz = separationtoxyz;
	cs->freedata = freeseparation;
	cs->data = sep;

	pdf_logrsrc("}\n");

	*csp = cs;
	return fz_okay;
}

/* Indexed */

struct indexed
{
	fz_colorspace *base;
	int high;
	unsigned char *lookup;
};

static void
indexedtoxyz(fz_colorspace *cs, float *color, float *xyz)
{
	struct indexed *idx = cs->data;
	float alt[FZ_MAXCOLORS];
	int i, k;
	i = color[0] * 255;
	i = CLAMP(i, 0, idx->high);
	for (k = 0; k < idx->base->n; k++)
		alt[k] = idx->lookup[i * idx->base->n + k] / 255.0f;
	idx->base->toxyz(idx->base, alt, xyz);
}

static void
freeindexed(fz_colorspace *cs)
{
	struct indexed *idx = cs->data;
	if (idx->base)
		fz_dropcolorspace(idx->base);
	fz_free(idx->lookup);
	fz_free(idx);
}

fz_pixmap *
pdf_expandindexedpixmap(fz_pixmap *src)
{
	struct indexed *idx;
	fz_pixmap *dst;
	unsigned char *s, *d;
	int y, x, k, n, high;
	unsigned char *lookup;

	assert(src->colorspace->toxyz == indexedtoxyz);
	assert(src->n == 2);

	idx = src->colorspace->data;
	high = idx->high;
	lookup = idx->lookup;
	n = idx->base->n;

	dst = fz_newpixmap(idx->base, src->x, src->y, src->w, src->h);
	s = src->samples;
	d = dst->samples;

	for (y = 0; y < src->h; y++)
	{
		for (x = 0; x < src->w; x++)
		{
			int v = *s++;
			int a = *s++;
			v = MIN(v, high);
			for (k = 0; k < n; k++)
				*d++ = fz_mul255(lookup[v * n + k], a);
			*d++ = a;
		}
	}

	if (src->mask)
		dst->mask = fz_keeppixmap(src->mask);
	dst->interpolate = src->interpolate;

	return dst;
}

static fz_error
loadindexed(fz_colorspace **csp, pdf_xref *xref, fz_obj *array)
{
	fz_error error;
	fz_colorspace *cs;
	struct indexed *idx;
	fz_obj *baseobj = fz_arrayget(array, 1);
	fz_obj *highobj = fz_arrayget(array, 2);
	fz_obj *lookup = fz_arrayget(array, 3);
	fz_colorspace *base;
	int i, n;

	pdf_logrsrc("load Indexed {\n");

	error = pdf_loadcolorspace(&base, xref, baseobj);
	if (error)
		return fz_rethrow(error, "cannot load base colorspace (%d %d R)", fz_tonum(baseobj), fz_togen(baseobj));

	pdf_logrsrc("base %s\n", base->name);

	idx = fz_malloc(sizeof(struct indexed));
	idx->base = base;
	idx->high = fz_toint(highobj);
	n = base->n * (idx->high + 1);
	idx->lookup = fz_malloc(n);

	cs = fz_newcolorspace("Indexed", 1);
	cs->toxyz = indexedtoxyz;
	cs->freedata = freeindexed;
	cs->data = idx;

	if (fz_isstring(lookup) && fz_tostrlen(lookup) == n)
	{
		unsigned char *buf;

		pdf_logrsrc("string lookup\n");

		buf = (unsigned char *) fz_tostrbuf(lookup);
		for (i = 0; i < n; i++)
			idx->lookup[i] = buf[i];
	}
	else if (fz_isindirect(lookup))
	{
		fz_stream *file;

		pdf_logrsrc("stream lookup\n");

		/* TODO: openstream, read, close instead */
		error = pdf_openstream(&file, xref, fz_tonum(lookup), fz_togen(lookup));
		if (error)
		{
			fz_dropcolorspace(cs);
			return fz_rethrow(error, "cannot open colorspace lookup table (%d 0 R)", fz_tonum(lookup));
		}

		i = fz_read(file, idx->lookup, n);
		if (i < 0)
		{
			fz_dropcolorspace(cs);
			return fz_throw("cannot read colorspace lookup table (%d 0 R)", fz_tonum(lookup));
		}

		fz_close(file);
	}
	else
	{
		fz_dropcolorspace(cs);
		return fz_throw("cannot parse colorspace lookup table");
	}

	pdf_logrsrc("}\n");

	*csp = cs;
	return fz_okay;
}

/* Parse and create colorspace from PDF object */

static fz_error
pdf_loadcolorspaceimp(fz_colorspace **csp, pdf_xref *xref, fz_obj *obj)
{
	if (fz_isname(obj))
	{
		if (!strcmp(fz_toname(obj), "Pattern"))
			*csp = fz_devicegray;
		else if (!strcmp(fz_toname(obj), "G"))
			*csp = fz_devicegray;
		else if (!strcmp(fz_toname(obj), "RGB"))
			*csp = fz_devicergb;
		else if (!strcmp(fz_toname(obj), "CMYK"))
			*csp = fz_devicecmyk;
		else if (!strcmp(fz_toname(obj), "DeviceGray"))
			*csp = fz_devicegray;
		else if (!strcmp(fz_toname(obj), "DeviceRGB"))
			*csp = fz_devicergb;
		else if (!strcmp(fz_toname(obj), "DeviceCMYK"))
			*csp = fz_devicecmyk;
		else
			return fz_throw("unknown colorspace: %s", fz_toname(obj));
		return fz_okay;
	}

	else if (fz_isarray(obj))
	{
		fz_obj *name = fz_arrayget(obj, 0);

		if (fz_isname(name))
		{
			/* load base colorspace instead */
			if (!strcmp(fz_toname(name), "Pattern"))
			{
				fz_error error;

				obj = fz_arrayget(obj, 1);
				if (!obj)
				{
					*csp = fz_devicegray;
					return fz_okay;
				}

				error = pdf_loadcolorspace(csp, xref, obj);
				if (error)
					return fz_rethrow(error, "cannot load pattern (%d %d R)", fz_tonum(obj), fz_togen(obj));
			}

			else if (!strcmp(fz_toname(name), "G"))
				*csp = fz_devicegray;
			else if (!strcmp(fz_toname(name), "RGB"))
				*csp = fz_devicergb;
			else if (!strcmp(fz_toname(name), "CMYK"))
				*csp = fz_devicecmyk;
			else if (!strcmp(fz_toname(name), "DeviceGray"))
				*csp = fz_devicegray;
			else if (!strcmp(fz_toname(name), "DeviceRGB"))
				*csp = fz_devicergb;
			else if (!strcmp(fz_toname(name), "DeviceCMYK"))
				*csp = fz_devicecmyk;
			else if (!strcmp(fz_toname(name), "CalGray"))
				*csp = fz_devicegray;
			else if (!strcmp(fz_toname(name), "CalRGB"))
				*csp = fz_devicergb;
			else if (!strcmp(fz_toname(name), "CalCMYK"))
				*csp = fz_devicecmyk;
			else if (!strcmp(fz_toname(name), "Lab"))
				*csp = fz_devicelab;

			else if (!strcmp(fz_toname(name), "ICCBased"))
				return loadiccbased(csp, xref, fz_arrayget(obj, 1));

			else if (!strcmp(fz_toname(name), "Indexed"))
				return loadindexed(csp, xref, obj);
			else if (!strcmp(fz_toname(name), "I"))
				return loadindexed(csp, xref, obj);

			else if (!strcmp(fz_toname(name), "Separation"))
				return loadseparation(csp, xref, obj);

			else if (!strcmp(fz_toname(name), "DeviceN"))
				return loadseparation(csp, xref, obj);

			else
				return fz_throw("syntaxerror: unknown colorspace %s", fz_toname(name));

			return fz_okay;
		}
	}

	return fz_throw("syntaxerror: could not parse color space (%d %d R)", fz_tonum(obj), fz_togen(obj));
}

fz_error
pdf_loadcolorspace(fz_colorspace **csp, pdf_xref *xref, fz_obj *obj)
{
	fz_error error;

	if ((*csp = pdf_finditem(xref->store, fz_dropcolorspace, obj)))
	{
		fz_keepcolorspace(*csp);
		return fz_okay;
	}

	error = pdf_loadcolorspaceimp(csp, xref, obj);
	if (error)
		return fz_rethrow(error, "cannot load colorspace (%d %d R)", fz_tonum(obj), fz_togen(obj));

	pdf_storeitem(xref->store, fz_keepcolorspace, fz_dropcolorspace, obj, *csp);

	return fz_okay;
}
