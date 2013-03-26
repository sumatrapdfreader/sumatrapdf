#include "fitz-internal.h"
#include "mupdf-internal.h"

/* ICCBased */

static fz_colorspace *
load_icc_based(pdf_document *xref, pdf_obj *dict)
{
	int n;

	n = pdf_to_int(pdf_dict_gets(dict, "N"));

	/* SumatraPDF: support alternate colorspaces for ICCBased */
	if (pdf_dict_gets(dict, "Alternate"))
	{
		fz_colorspace *cs_alt = pdf_load_colorspace(xref, pdf_dict_gets(dict, "Alternate"));
		if (cs_alt->n != n)
		{
			fz_drop_colorspace(xref->ctx, cs_alt);
			fz_throw(xref->ctx, "ICCBased /Alternate colorspace must have %d components (not %d)", n, cs_alt->n);
		}
		return cs_alt;
	}

	switch (n)
	{
	case 1: return fz_device_gray;
	case 3: return fz_device_rgb;
	case 4: return fz_device_cmyk;
	}

	fz_throw(xref->ctx, "syntaxerror: ICCBased must have 1, 3 or 4 components");
	return NULL; /* Stupid MSVC */
}

/* Lab */

static inline float fung(float x)
{
	if (x >= 6.0f / 29.0f)
		return x * x * x;
	return (108.0f / 841.0f) * (x - (4.0f / 29.0f));
}

static void
lab_to_rgb(fz_context *ctx, fz_colorspace *cs, float *lab, float *rgb)
{
	/* input is in range (0..100, -128..127, -128..127) not (0..1, 0..1, 0..1) */
	float lstar, astar, bstar, l, m, n, x, y, z, r, g, b;
	lstar = lab[0];
	astar = lab[1];
	bstar = lab[2];
	m = (lstar + 16) / 116;
	l = m + astar / 500;
	n = m - bstar / 200;
	x = fung(l);
	y = fung(m);
	z = fung(n);
	r = (3.240449f * x + -1.537136f * y + -0.498531f * z) * 0.830026f;
	g = (-0.969265f * x + 1.876011f * y + 0.041556f * z) * 1.05452f;
	b = (0.055643f * x + -0.204026f * y + 1.057229f * z) * 1.1003f;
	rgb[0] = sqrtf(fz_clamp(r, 0, 1));
	rgb[1] = sqrtf(fz_clamp(g, 0, 1));
	rgb[2] = sqrtf(fz_clamp(b, 0, 1));
}

static void
rgb_to_lab(fz_context *ctx, fz_colorspace *cs, float *rgb, float *lab)
{
	fz_warn(ctx, "cannot convert into L*a*b colorspace");
	lab[0] = rgb[0];
	lab[1] = rgb[1];
	lab[2] = rgb[2];
}

static fz_colorspace k_device_lab = { {-1, fz_free_colorspace_imp}, 0, "Lab", 3, lab_to_rgb, rgb_to_lab };
static fz_colorspace *fz_device_lab = &k_device_lab;

/* Separation and DeviceN */

struct separation
{
	fz_colorspace *base;
	fz_function *tint;
};

static void
separation_to_rgb(fz_context *ctx, fz_colorspace *cs, float *color, float *rgb)
{
	struct separation *sep = cs->data;
	float alt[FZ_MAX_COLORS];
	fz_eval_function(ctx, sep->tint, color, cs->n, alt, sep->base->n);
	sep->base->to_rgb(ctx, sep->base, alt, rgb);
}

static void
free_separation(fz_context *ctx, fz_colorspace *cs)
{
	struct separation *sep = cs->data;
	fz_drop_colorspace(ctx, sep->base);
	fz_drop_function(ctx, sep->tint);
	fz_free(ctx, sep);
}

static fz_colorspace *
load_separation(pdf_document *xref, pdf_obj *array)
{
	fz_colorspace *cs;
	struct separation *sep = NULL;
	fz_context *ctx = xref->ctx;
	pdf_obj *nameobj = pdf_array_get(array, 1);
	pdf_obj *baseobj = pdf_array_get(array, 2);
	pdf_obj *tintobj = pdf_array_get(array, 3);
	fz_colorspace *base;
	fz_function *tint = NULL;
	int n;

	fz_var(tint);
	fz_var(sep);

	if (pdf_is_array(nameobj))
		n = pdf_array_len(nameobj);
	else
		n = 1;

	if (n > FZ_MAX_COLORS)
		fz_throw(ctx, "too many components in colorspace");

	base = pdf_load_colorspace(xref, baseobj);

	fz_try(ctx)
	{
		tint = pdf_load_function(xref, tintobj, n, base->n);
		/* RJW: fz_drop_colorspace(ctx, base);
		 * "cannot load tint function (%d %d R)", pdf_to_num(tintobj), pdf_to_gen(tintobj) */

		sep = fz_malloc_struct(ctx, struct separation);
		sep->base = base;
		sep->tint = tint;

		cs = fz_new_colorspace(ctx, n == 1 ? "Separation" : "DeviceN", n);
		cs->to_rgb = separation_to_rgb;
		cs->free_data = free_separation;
		cs->data = sep;
		cs->size += sizeof(struct separation) + (base ? base->size : 0) + fz_function_size(tint);
	}
	fz_catch(ctx)
	{
		fz_drop_colorspace(ctx, base);
		fz_drop_function(ctx, tint);
		fz_free(ctx, sep);
		fz_rethrow(ctx);
	}

	return cs;
}

/* Indexed */

struct indexed
{
	fz_colorspace *base;
	int high;
	unsigned char *lookup;
};

static void
indexed_to_rgb(fz_context *ctx, fz_colorspace *cs, float *color, float *rgb)
{
	struct indexed *idx = cs->data;
	float alt[FZ_MAX_COLORS];
	int i, k;
	i = color[0] * 255;
	i = fz_clampi(i, 0, idx->high);
	for (k = 0; k < idx->base->n; k++)
		alt[k] = idx->lookup[i * idx->base->n + k] / 255.0f;
	idx->base->to_rgb(ctx, idx->base, alt, rgb);
}

static void
free_indexed(fz_context *ctx, fz_colorspace *cs)
{
	struct indexed *idx = cs->data;
	if (idx->base)
		fz_drop_colorspace(ctx, idx->base);
	fz_free(ctx, idx->lookup);
	fz_free(ctx, idx);
}

fz_pixmap *
pdf_expand_indexed_pixmap(fz_context *ctx, fz_pixmap *src)
{
	struct indexed *idx;
	fz_pixmap *dst;
	unsigned char *s, *d;
	int y, x, k, n, high;
	unsigned char *lookup;
	fz_irect bbox;

	assert(src->colorspace->to_rgb == indexed_to_rgb);
	assert(src->n == 2);

	idx = src->colorspace->data;
	high = idx->high;
	lookup = idx->lookup;
	n = idx->base->n;

	dst = fz_new_pixmap_with_bbox(ctx, idx->base, fz_pixmap_bbox(ctx, src, &bbox));
	s = src->samples;
	d = dst->samples;

	for (y = 0; y < src->h; y++)
	{
		for (x = 0; x < src->w; x++)
		{
			int v = *s++;
			int a = *s++;
			v = fz_mini(v, high);
			for (k = 0; k < n; k++)
				*d++ = fz_mul255(lookup[v * n + k], a);
			*d++ = a;
		}
	}

	dst->interpolate = src->interpolate;

	return dst;
}

static fz_colorspace *
load_indexed(pdf_document *xref, pdf_obj *array)
{
	struct indexed *idx = NULL;
	fz_context *ctx = xref->ctx;
	pdf_obj *baseobj = pdf_array_get(array, 1);
	pdf_obj *highobj = pdf_array_get(array, 2);
	pdf_obj *lookup = pdf_array_get(array, 3);
	fz_colorspace *base = NULL;
	fz_colorspace *cs = NULL;
	int i, n;

	fz_var(idx);
	fz_var(base);
	fz_var(cs);

	fz_try(ctx)
	{
		base = pdf_load_colorspace(xref, baseobj);

		idx = fz_malloc_struct(ctx, struct indexed);
		idx->lookup = NULL;
		idx->base = base;
		idx->high = pdf_to_int(highobj);
		idx->high = fz_clampi(idx->high, 0, 255);
		n = base->n * (idx->high + 1);
		idx->lookup = fz_malloc_array(ctx, 1, n);

		cs = fz_new_colorspace(ctx, "Indexed", 1);
		cs->to_rgb = indexed_to_rgb;
		cs->free_data = free_indexed;
		cs->data = idx;
		cs->size += sizeof(*idx) + n + (base ? base->size : 0);

		if (pdf_is_string(lookup) && pdf_to_str_len(lookup) == n)
		{
			unsigned char *buf = (unsigned char *) pdf_to_str_buf(lookup);
			for (i = 0; i < n; i++)
				idx->lookup[i] = buf[i];
		}
		else if (pdf_is_indirect(lookup))
		{
			fz_stream *file = NULL;

			fz_var(file);

			fz_try(ctx)
			{
				file = pdf_open_stream(xref, pdf_to_num(lookup), pdf_to_gen(lookup));
				i = fz_read(file, idx->lookup, n);
			}
			fz_always(ctx)
			{
				fz_close(file);
			}
			fz_catch(ctx)
			{
				fz_throw(ctx, "cannot open colorspace lookup table (%d 0 R)", pdf_to_num(lookup));
			}
		}
		else
		{
			fz_throw(ctx, "cannot parse colorspace lookup table");
		}
	}
	fz_catch(ctx)
	{
		if (cs == NULL || cs->data != idx)
		{
			fz_drop_colorspace(ctx, base);
			if (idx)
				fz_free(ctx, idx->lookup);
			fz_free(ctx, idx);
		}
		fz_drop_colorspace(ctx, cs);
		fz_rethrow(ctx);
	}

	return cs;
}

/* Parse and create colorspace from PDF object */

static fz_colorspace *
pdf_load_colorspace_imp(pdf_document *xref, pdf_obj *obj)
{
	fz_context *ctx = xref->ctx;

	if (pdf_obj_marked(obj))
		fz_throw(ctx, "Recursion in colorspace definition");

	if (pdf_is_name(obj))
	{
		const char *str = pdf_to_name(obj);
		if (!strcmp(str, "Pattern"))
			return fz_device_gray;
		else if (!strcmp(str, "G"))
			return fz_device_gray;
		else if (!strcmp(str, "RGB"))
			return fz_device_rgb;
		else if (!strcmp(str, "CMYK"))
			return fz_device_cmyk;
		else if (!strcmp(str, "DeviceGray"))
			return fz_device_gray;
		else if (!strcmp(str, "DeviceRGB"))
			return fz_device_rgb;
		else if (!strcmp(str, "DeviceCMYK"))
			return fz_device_cmyk;
		else
			fz_throw(ctx, "unknown colorspace: %s", pdf_to_name(obj));
	}

	else if (pdf_is_array(obj))
	{
		pdf_obj *name = pdf_array_get(obj, 0);
		const char *str = pdf_to_name(name);

		if (pdf_is_name(name))
		{
			/* load base colorspace instead */
			if (!strcmp(str, "G"))
				return fz_device_gray;
			else if (!strcmp(str, "RGB"))
				return fz_device_rgb;
			else if (!strcmp(str, "CMYK"))
				return fz_device_cmyk;
			else if (!strcmp(str, "DeviceGray"))
				return fz_device_gray;
			else if (!strcmp(str, "DeviceRGB"))
				return fz_device_rgb;
			else if (!strcmp(str, "DeviceCMYK"))
				return fz_device_cmyk;
			else if (!strcmp(str, "CalGray"))
				return fz_device_gray;
			else if (!strcmp(str, "CalRGB"))
				return fz_device_rgb;
			else if (!strcmp(str, "CalCMYK"))
				return fz_device_cmyk;
			else if (!strcmp(str, "Lab"))
				return fz_device_lab;
			else
			{
				fz_colorspace *cs;
				fz_try(ctx)
				{
					pdf_obj_mark(obj);
					if (!strcmp(str, "ICCBased"))
						cs = load_icc_based(xref, pdf_array_get(obj, 1));

					else if (!strcmp(str, "Indexed"))
						cs = load_indexed(xref, obj);
					else if (!strcmp(str, "I"))
						cs = load_indexed(xref, obj);

					else if (!strcmp(str, "Separation"))
						cs = load_separation(xref, obj);

					else if (!strcmp(str, "DeviceN"))
						cs = load_separation(xref, obj);
					else if (!strcmp(str, "Pattern"))
					{
						pdf_obj *pobj;

						pobj = pdf_array_get(obj, 1);
						if (!pobj)
						{
							cs = fz_device_gray;
							break;
						}

						cs = pdf_load_colorspace(xref, pobj);
					}
					else
						fz_throw(ctx, "syntaxerror: unknown colorspace %s", str);
				}
				fz_always(ctx)
				{
					pdf_obj_unmark(obj);
				}
				fz_catch(ctx)
				{
					fz_rethrow(ctx);
				}
				return cs;
			}
		}
	}

	fz_throw(xref->ctx, "syntaxerror: could not parse color space (%d %d R)", pdf_to_num(obj), pdf_to_gen(obj));
	return NULL; /* Stupid MSVC */
}

fz_colorspace *
pdf_load_colorspace(pdf_document *xref, pdf_obj *obj)
{
	fz_context *ctx = xref->ctx;
	fz_colorspace *cs;

	if ((cs = pdf_find_item(ctx, fz_free_colorspace_imp, obj)))
	{
		return cs;
	}

	cs = pdf_load_colorspace_imp(xref, obj);

	pdf_store_item(ctx, obj, cs, cs->size);

	return cs;
}
