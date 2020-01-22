#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>

/* ICCBased */
static fz_colorspace *
load_icc_based(fz_context *ctx, pdf_obj *dict, int allow_alt)
{
	int n = pdf_dict_get_int(ctx, dict, PDF_NAME(N));
	fz_colorspace *alt = NULL;
	fz_colorspace *cs = NULL;
	pdf_obj *obj;

	fz_var(alt);
	fz_var(cs);

	/* Look at Alternate to detect type (especially Lab). */
	if (allow_alt)
	{
		obj = pdf_dict_get(ctx, dict, PDF_NAME(Alternate));
		if (obj)
		{
			fz_try(ctx)
				alt = pdf_load_colorspace(ctx, obj);
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_warn(ctx, "ignoring broken ICC Alternate colorspace");
			}
		}
	}

#if FZ_ENABLE_ICC
	{
		fz_buffer *buf = NULL;
		fz_var(buf);
		fz_try(ctx)
		{
			buf = pdf_load_stream(ctx, dict);
			cs = fz_new_icc_colorspace(ctx, alt ? alt->type : FZ_COLORSPACE_NONE, 0, NULL, buf);
			if (cs->n > n)
			{
				fz_warn(ctx, "ICC colorspace N=%d does not match profile N=%d (ignoring profile)", n, cs->n);
				fz_drop_colorspace(ctx, cs);
				cs = NULL;
			}
			else if (cs->n < n)
			{
				fz_warn(ctx, "ICC colorspace N=%d does not match profile N=%d (using profile)", n, cs->n);
			}
		}
		fz_always(ctx)
			fz_drop_buffer(ctx, buf);
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
			fz_warn(ctx, "ignoring broken ICC profile");
		}
	}
#endif

	if (!cs)
		cs = alt;
	else
		fz_drop_colorspace(ctx, alt);

	if (!cs)
	{
		if (n == 1) cs = fz_keep_colorspace(ctx, fz_device_gray(ctx));
		else if (n == 3) cs = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		else if (n == 4) cs = fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
		else fz_throw(ctx, FZ_ERROR_SYNTAX, "invalid ICC colorspace");
	}

	return cs;
}

static void
devicen_eval(fz_context *ctx, void *tint, const float *sv, int sn, float *dv, int dn)
{
	pdf_eval_function(ctx, tint, sv, sn, dv, dn);
}

static void
devicen_drop(fz_context *ctx, void *tint)
{
	pdf_drop_function(ctx, tint);
}

static fz_colorspace *
load_devicen(fz_context *ctx, pdf_obj *array, int is_devn)
{
	fz_colorspace *base = NULL;
	fz_colorspace *cs = NULL;
	pdf_obj *nameobj = pdf_array_get(ctx, array, 1);
	pdf_obj *baseobj = pdf_array_get(ctx, array, 2);
	pdf_obj *tintobj = pdf_array_get(ctx, array, 3);
	char name[100];
	int i, n;

	if (pdf_is_array(ctx, nameobj))
	{
		n = pdf_array_len(ctx, nameobj);
		if (n < 1)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "too few components in DeviceN colorspace");
		if (n > FZ_MAX_COLORS)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "too many components in DeviceN colorspace");
	}
	else
	{
		n = 1;
	}

	base = pdf_load_colorspace(ctx, baseobj);
	fz_try(ctx)
	{
		if (is_devn)
		{
			fz_snprintf(name, sizeof name, "DeviceN(%d,%s", n, base->name);
			for (i = 0; i < n; i++) {
				fz_strlcat(name, ",", sizeof name);
				fz_strlcat(name, pdf_array_get_name(ctx, nameobj, i), sizeof name);
			}
			fz_strlcat(name, ")", sizeof name);
		}
		else
		{
			fz_snprintf(name, sizeof name, "Separation(%s,%s)", base->name, pdf_to_name(ctx, nameobj));
		}

		cs = fz_new_colorspace(ctx, FZ_COLORSPACE_SEPARATION, 0, n, name);
		cs->u.separation.eval = devicen_eval;
		cs->u.separation.drop = devicen_drop;
		cs->u.separation.base = fz_keep_colorspace(ctx, base);
		cs->u.separation.tint = pdf_load_function(ctx, tintobj, n, cs->u.separation.base->n);
		if (pdf_is_array(ctx, nameobj))
			for (i = 0; i < n; i++)
				fz_colorspace_name_colorant(ctx, cs, i, pdf_to_name(ctx, pdf_array_get(ctx, nameobj, i)));
		else
			fz_colorspace_name_colorant(ctx, cs, 0, pdf_to_name(ctx, nameobj));
	}
	fz_always(ctx)
	{
		fz_drop_colorspace(ctx, base);
	}
	fz_catch(ctx)
	{
		fz_drop_colorspace(ctx, cs);
		fz_rethrow(ctx);
	}

	return cs;
}

int
pdf_is_tint_colorspace(fz_context *ctx, fz_colorspace *cs)
{
	return cs && cs->type == FZ_COLORSPACE_SEPARATION;
}

/* Indexed */

static fz_colorspace *
load_indexed(fz_context *ctx, pdf_obj *array)
{
	pdf_obj *baseobj = pdf_array_get(ctx, array, 1);
	pdf_obj *highobj = pdf_array_get(ctx, array, 2);
	pdf_obj *lookupobj = pdf_array_get(ctx, array, 3);
	fz_colorspace *base = NULL;
	fz_colorspace *cs;
	size_t i, n;
	int high;
	unsigned char *lookup = NULL;

	fz_var(base);
	fz_var(lookup);

	fz_try(ctx)
	{
		base = pdf_load_colorspace(ctx, baseobj);

		high = pdf_to_int(ctx, highobj);
		high = fz_clampi(high, 0, 255);
		n = base->n * (high + 1);
		lookup = Memento_label(fz_malloc(ctx, n), "cs_lookup");

		if (pdf_is_string(ctx, lookupobj))
		{
			size_t sn = fz_minz(n, pdf_to_str_len(ctx, lookupobj));
			unsigned char *buf = (unsigned char *) pdf_to_str_buf(ctx, lookupobj);
			for (i = 0; i < sn; ++i)
				lookup[i] = buf[i];
			for (; i < n; ++i)
				lookup[i] = 0;
		}
		else if (pdf_is_indirect(ctx, lookupobj))
		{
			fz_stream *file = NULL;

			fz_var(file);

			fz_try(ctx)
			{
				file = pdf_open_stream(ctx, lookupobj);
				i = fz_read(ctx, file, lookup, n);
				if (i < n)
					memset(lookup+i, 0, n-i);
			}
			fz_always(ctx)
			{
				fz_drop_stream(ctx, file);
			}
			fz_catch(ctx)
			{
				fz_rethrow(ctx);
			}
		}
		else
		{
			fz_throw(ctx, FZ_ERROR_SYNTAX, "cannot parse colorspace lookup table");
		}

		cs = fz_new_indexed_colorspace(ctx, base, high, lookup);
	}
	fz_always(ctx)
		fz_drop_colorspace(ctx, base);
	fz_catch(ctx)
	{
		fz_free(ctx, lookup);
		fz_rethrow(ctx);
	}

	return cs;
}

static void
pdf_load_cal_common(fz_context *ctx, pdf_obj *dict, float *wp, float *bp, float *gamma)
{
	pdf_obj *obj;
	int i;

	obj = pdf_dict_get(ctx, dict, PDF_NAME(WhitePoint));
	if (pdf_array_len(ctx, obj) != 3)
		fz_throw(ctx, FZ_ERROR_SYNTAX, "WhitePoint must be a 3-element array");

	for (i = 0; i < 3; i++)
	{
		wp[i] = pdf_array_get_real(ctx, obj, i);
		if (wp[i] < 0)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "WhitePoint numbers must be positive");
	}
	if (wp[1] != 1)
		fz_throw(ctx, FZ_ERROR_SYNTAX, "WhitePoint Yw must be 1.0");

	obj = pdf_dict_get(ctx, dict, PDF_NAME(BlackPoint));
	if (pdf_array_len(ctx, obj) == 3)
	{
		for (i = 0; i < 3; i++)
		{
			bp[i] = pdf_array_get_real(ctx, obj, i);
			if (bp[i] < 0)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "BlackPoint numbers must be positive");
		}
	}

	obj = pdf_dict_get(ctx, dict, PDF_NAME(Gamma));
	if (pdf_is_number(ctx, obj))
	{
		gamma[0] = pdf_to_real(ctx, obj);
		gamma[1] = gamma[2];
		if (gamma[0] <= 0)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "Gamma must be greater than zero");
	}
	else if (pdf_array_len(ctx, obj) == 3)
	{
		for (i = 0; i < 3; i++)
		{
			gamma[i] = pdf_array_get_real(ctx, obj, i);
			if (gamma[i] <= 0)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "Gamma must be greater than zero");
		}
	}
}

static fz_colorspace *
pdf_load_cal_gray(fz_context *ctx, pdf_obj *dict)
{
	float wp[3];
	float bp[3] = { 0, 0, 0 };
	float gamma[3] = { 1, 1, 1 };

	if (dict == NULL)
		return fz_keep_colorspace(ctx, fz_device_gray(ctx));

	fz_try(ctx)
		pdf_load_cal_common(ctx, dict, wp, bp, gamma);
	fz_catch(ctx)
		return fz_keep_colorspace(ctx, fz_device_gray(ctx));
	return fz_new_cal_gray_colorspace(ctx, wp, bp, gamma[0]);
}

static fz_colorspace *
pdf_load_cal_rgb(fz_context *ctx, pdf_obj *dict)
{
	pdf_obj *obj;
	float matrix[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	float wp[3];
	float bp[3] = { 0, 0, 0 };
	float gamma[3] = { 1, 1, 1 };
	int i;

	if (dict == NULL)
		return fz_keep_colorspace(ctx, fz_device_rgb(ctx));

	fz_try(ctx)
	{
		pdf_load_cal_common(ctx, dict, wp, bp, gamma);
		obj = pdf_dict_get(ctx, dict, PDF_NAME(Matrix));
		if (pdf_array_len(ctx, obj) == 9)
		{
			for (i = 0; i < 9; i++)
				matrix[i] = pdf_array_get_real(ctx, obj, i);
		}
	}
	fz_catch(ctx)
		return fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	return fz_new_cal_rgb_colorspace(ctx, wp, bp, gamma, matrix);
}

/* Parse and create colorspace from PDF object */

static fz_colorspace *
pdf_load_colorspace_imp(fz_context *ctx, pdf_obj *obj)
{
	if (pdf_obj_marked(ctx, obj))
		fz_throw(ctx, FZ_ERROR_SYNTAX, "recursion in colorspace definition");

	if (pdf_is_name(ctx, obj))
	{
		if (pdf_name_eq(ctx, obj, PDF_NAME(Pattern)))
			return fz_keep_colorspace(ctx, fz_device_gray(ctx));
		else if (pdf_name_eq(ctx, obj, PDF_NAME(G)))
			return fz_keep_colorspace(ctx, fz_device_gray(ctx));
		else if (pdf_name_eq(ctx, obj, PDF_NAME(RGB)))
			return fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		else if (pdf_name_eq(ctx, obj, PDF_NAME(CMYK)))
			return fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
		else if (pdf_name_eq(ctx, obj, PDF_NAME(DeviceGray)))
			return fz_keep_colorspace(ctx, fz_device_gray(ctx));
		else if (pdf_name_eq(ctx, obj, PDF_NAME(DeviceRGB)))
			return fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		else if (pdf_name_eq(ctx, obj, PDF_NAME(DeviceCMYK)))
			return fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
		else
			fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown colorspace: %s", pdf_to_name(ctx, obj));
	}

	else if (pdf_is_array(ctx, obj))
	{
		pdf_obj *name = pdf_array_get(ctx, obj, 0);

		if (pdf_is_name(ctx, name))
		{
			/* load base colorspace instead */
			if (pdf_name_eq(ctx, name, PDF_NAME(G)))
				return fz_keep_colorspace(ctx, fz_device_gray(ctx));
			else if (pdf_name_eq(ctx, name, PDF_NAME(RGB)))
				return fz_keep_colorspace(ctx, fz_device_rgb(ctx));
			else if (pdf_name_eq(ctx, name, PDF_NAME(CMYK)))
				return fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
			else if (pdf_name_eq(ctx, name, PDF_NAME(DeviceGray)))
				return fz_keep_colorspace(ctx, fz_device_gray(ctx));
			else if (pdf_name_eq(ctx, name, PDF_NAME(DeviceRGB)))
				return fz_keep_colorspace(ctx, fz_device_rgb(ctx));
			else if (pdf_name_eq(ctx, name, PDF_NAME(DeviceCMYK)))
				return fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
			else if (pdf_name_eq(ctx, name, PDF_NAME(CalGray)))
				return pdf_load_cal_gray(ctx, pdf_array_get(ctx, obj, 1));
			else if (pdf_name_eq(ctx, name, PDF_NAME(CalRGB)))
				return pdf_load_cal_rgb(ctx, pdf_array_get(ctx, obj, 1));
			else if (pdf_name_eq(ctx, name, PDF_NAME(CalCMYK)))
				return fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
			else if (pdf_name_eq(ctx, name, PDF_NAME(Lab)))
				return fz_keep_colorspace(ctx, fz_device_lab(ctx));
			else
			{
				fz_colorspace *cs;
				fz_try(ctx)
				{
					if (pdf_mark_obj(ctx, obj))
						fz_throw(ctx, FZ_ERROR_SYNTAX, "recursive colorspace");
					if (pdf_name_eq(ctx, name, PDF_NAME(ICCBased)))
						cs = load_icc_based(ctx, pdf_array_get(ctx, obj, 1), 1);

					else if (pdf_name_eq(ctx, name, PDF_NAME(Indexed)))
						cs = load_indexed(ctx, obj);
					else if (pdf_name_eq(ctx, name, PDF_NAME(I)))
						cs = load_indexed(ctx, obj);

					else if (pdf_name_eq(ctx, name, PDF_NAME(Separation)))
						cs = load_devicen(ctx, obj, 0);

					else if (pdf_name_eq(ctx, name, PDF_NAME(DeviceN)))
						cs = load_devicen(ctx, obj, 1);
					else if (pdf_name_eq(ctx, name, PDF_NAME(Pattern)))
					{
						pdf_obj *pobj;

						pobj = pdf_array_get(ctx, obj, 1);
						if (!pobj)
						{
							cs = fz_keep_colorspace(ctx, fz_device_gray(ctx));
							break;
						}

						cs = pdf_load_colorspace(ctx, pobj);
					}
					else
						fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown colorspace %s", pdf_to_name(ctx, name));
				}
				fz_always(ctx)
				{
					pdf_unmark_obj(ctx, obj);
				}
				fz_catch(ctx)
				{
					fz_rethrow(ctx);
				}
				return cs;
			}
		}
	}

	/* We have seen files where /DefaultRGB is specified as 1 0 R,
	 * and 1 0 obj << /Length 3144 /Alternate /DeviceRGB /N 3 >>
	 * stream ...iccprofile... endstream endobj.
	 * This *should* be [ /ICCBased 1 0 R ], but Acrobat seems to
	 * handle it, so do our best. */
	else if (pdf_is_dict(ctx, obj))
	{
		return load_icc_based(ctx, obj, 1);
	}

	fz_throw(ctx, FZ_ERROR_SYNTAX, "could not parse color space (%d 0 R)", pdf_to_num(ctx, obj));
}

fz_colorspace *
pdf_load_colorspace(fz_context *ctx, pdf_obj *obj)
{
	fz_colorspace *cs;

	if ((cs = pdf_find_item(ctx, fz_drop_colorspace_imp, obj)) != NULL)
	{
		return cs;
	}

	cs = pdf_load_colorspace_imp(ctx, obj);

	pdf_store_item(ctx, obj, cs, 1000);

	return cs;
}

#if FZ_ENABLE_ICC

static fz_colorspace *
pdf_load_output_intent(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
	pdf_obj *intents = pdf_dict_get(ctx, root, PDF_NAME(OutputIntents));
	pdf_obj *intent_dict;
	pdf_obj *dest_profile;
	fz_colorspace *cs = NULL;

	/* An array of intents */
	if (!intents)
		return NULL;

	/* For now, always just use the first intent. I have never even seen a file
	 * with multiple intents but it could happen */
	intent_dict = pdf_array_get(ctx, intents, 0);
	if (!intent_dict)
		return NULL;
	dest_profile = pdf_dict_get(ctx, intent_dict, PDF_NAME(DestOutputProfile));
	if (!dest_profile)
		return NULL;

	fz_var(cs);

	fz_try(ctx)
		cs = load_icc_based(ctx, dest_profile, 0);
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
		fz_warn(ctx, "Attempt to read Output Intent failed");
	}
	return cs;
}

fz_colorspace *
pdf_document_output_intent(fz_context *ctx, pdf_document *doc)
{
	if (!doc->oi)
		doc->oi = pdf_load_output_intent(ctx, doc);
	return doc->oi;
}

#else

fz_colorspace *
pdf_document_output_intent(fz_context *ctx, pdf_document *doc)
{
	return NULL;
}

#endif
