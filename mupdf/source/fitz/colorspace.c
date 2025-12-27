// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "mupdf/fitz.h"

#include "color-imp.h"

#include <assert.h>
#include <math.h>
#include <string.h>

#if FZ_ENABLE_ICC

#include "icc/gray.icc.h"
#include "icc/rgb.icc.h"
#include "icc/cmyk.icc.h"
#include "icc/lab.icc.h"
#include "icc/ps_gray.icc.h"
#include "icc/ps_rgb.icc.h"
#include "icc/ps_cmyk.icc.h"

void fz_new_colorspace_context(fz_context *ctx)
{
	fz_colorspace_context *cct;

	fz_buffer *gray = NULL;
	fz_buffer *rgb = NULL;
	fz_buffer *cmyk = NULL;
	fz_buffer *lab = NULL;
	fz_buffer *ps_gray = NULL;
	fz_buffer *ps_rgb = NULL;
	fz_buffer *ps_cmyk = NULL;

	fz_var(gray);
	fz_var(rgb);
	fz_var(cmyk);
	fz_var(lab);
	fz_var(ps_gray);
	fz_var(ps_rgb);
	fz_var(ps_cmyk);

	cct = ctx->colorspace = fz_malloc_struct(ctx, fz_colorspace_context);
	cct->ctx_refs = 1;

	fz_new_icc_context(ctx);

	ctx->icc_enabled = 1;

	fz_try(ctx)
	{
		gray = fz_new_buffer_from_shared_data(ctx, resources_icc_gray_icc, resources_icc_gray_icc_len);
		rgb = fz_new_buffer_from_shared_data(ctx, resources_icc_rgb_icc, resources_icc_rgb_icc_len);
		cmyk = fz_new_buffer_from_shared_data(ctx, resources_icc_cmyk_icc, resources_icc_cmyk_icc_len);
		lab = fz_new_buffer_from_shared_data(ctx, resources_icc_lab_icc, resources_icc_lab_icc_len);
		ps_gray = fz_new_buffer_from_shared_data(ctx, resources_icc_ps_gray_icc, resources_icc_ps_gray_icc_len);
		ps_rgb = fz_new_buffer_from_shared_data(ctx, resources_icc_ps_rgb_icc, resources_icc_ps_rgb_icc_len);
		ps_cmyk = fz_new_buffer_from_shared_data(ctx, resources_icc_ps_cmyk_icc, resources_icc_ps_cmyk_icc_len);
		cct->gray = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_GRAY, FZ_COLORSPACE_IS_DEVICE, "DeviceGray", gray);
		cct->rgb = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_RGB, FZ_COLORSPACE_IS_DEVICE, "DeviceRGB", rgb);
		cct->bgr = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_BGR, FZ_COLORSPACE_IS_DEVICE, "DeviceBGR", rgb);
		cct->cmyk = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_CMYK, FZ_COLORSPACE_IS_DEVICE, "DeviceCMYK", cmyk);
		cct->lab = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_LAB, FZ_COLORSPACE_IS_DEVICE, "Lab", lab);
		cct->ps_gray = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_GRAY, FZ_COLORSPACE_IS_DEVICE, "DeviceGray", ps_gray);
		cct->ps_rgb = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_RGB, FZ_COLORSPACE_IS_DEVICE, "DeviceRGB", ps_rgb);
		cct->ps_bgr = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_BGR, FZ_COLORSPACE_IS_DEVICE, "DeviceBGR", ps_rgb);
		cct->ps_cmyk = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_CMYK, FZ_COLORSPACE_IS_DEVICE, "DeviceCMYK", ps_cmyk);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, gray);
		fz_drop_buffer(ctx, rgb);
		fz_drop_buffer(ctx, cmyk);
		fz_drop_buffer(ctx, lab);
		fz_drop_buffer(ctx, ps_gray);
		fz_drop_buffer(ctx, ps_rgb);
		fz_drop_buffer(ctx, ps_cmyk);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void fz_enable_icc(fz_context *ctx)
{
	ctx->icc_enabled = 1;
}

void fz_disable_icc(fz_context *ctx)
{
	ctx->icc_enabled = 0;
}

#else

void fz_new_colorspace_context(fz_context *ctx)
{
	fz_colorspace_context *cct;

	cct = ctx->colorspace = fz_malloc_struct(ctx, fz_colorspace_context);
	cct->ctx_refs = 1;

	cct->gray = fz_new_colorspace(ctx, FZ_COLORSPACE_GRAY, FZ_COLORSPACE_IS_DEVICE, 1, "DeviceGray");
	cct->rgb = fz_new_colorspace(ctx, FZ_COLORSPACE_RGB, FZ_COLORSPACE_IS_DEVICE, 3, "DeviceRGB");
	cct->bgr = fz_new_colorspace(ctx, FZ_COLORSPACE_BGR, FZ_COLORSPACE_IS_DEVICE, 3, "DeviceBGR");
	cct->cmyk = fz_new_colorspace(ctx, FZ_COLORSPACE_CMYK, FZ_COLORSPACE_IS_DEVICE, 4, "DeviceCMYK");
	cct->lab = fz_new_colorspace(ctx, FZ_COLORSPACE_LAB, FZ_COLORSPACE_IS_DEVICE, 3, "Lab");
	cct->ps_gray = fz_new_colorspace(ctx, FZ_COLORSPACE_GRAY, FZ_COLORSPACE_IS_DEVICE, 1, "DeviceGray");
	cct->ps_rgb = fz_new_colorspace(ctx, FZ_COLORSPACE_RGB, FZ_COLORSPACE_IS_DEVICE, 3, "DeviceRGB");
	cct->ps_bgr = fz_new_colorspace(ctx, FZ_COLORSPACE_BGR, FZ_COLORSPACE_IS_DEVICE, 3, "DeviceBGR");
	cct->ps_cmyk = fz_new_colorspace(ctx, FZ_COLORSPACE_CMYK, FZ_COLORSPACE_IS_DEVICE, 4, "DeviceCMYK");
}

void fz_enable_icc(fz_context *ctx)
{
	fz_warn(ctx, "ICC support is not available");
}

void fz_disable_icc(fz_context *ctx)
{
}

#endif

fz_colorspace_context *fz_keep_colorspace_context(fz_context *ctx)
{
	fz_keep_imp(ctx, ctx->colorspace, &ctx->colorspace->ctx_refs);
	return ctx->colorspace;
}

void fz_drop_colorspace_context(fz_context *ctx)
{
	if (fz_drop_imp(ctx, ctx->colorspace, &ctx->colorspace->ctx_refs))
	{
		fz_drop_colorspace(ctx, ctx->colorspace->gray);
		fz_drop_colorspace(ctx, ctx->colorspace->rgb);
		fz_drop_colorspace(ctx, ctx->colorspace->bgr);
		fz_drop_colorspace(ctx, ctx->colorspace->cmyk);
		fz_drop_colorspace(ctx, ctx->colorspace->lab);
		fz_drop_colorspace(ctx, ctx->colorspace->ps_gray);
		fz_drop_colorspace(ctx, ctx->colorspace->ps_rgb);
		fz_drop_colorspace(ctx, ctx->colorspace->ps_bgr);
		fz_drop_colorspace(ctx, ctx->colorspace->ps_cmyk);
#if FZ_ENABLE_ICC
		fz_drop_icc_context(ctx);
#endif
		fz_free(ctx, ctx->colorspace);
		ctx->colorspace = NULL;
	}
}

fz_colorspace *fz_device_gray(fz_context *ctx)
{
	return ctx->colorspace->gray;
}

fz_colorspace *fz_device_rgb(fz_context *ctx)
{
	return ctx->colorspace->rgb;
}

fz_colorspace *fz_device_bgr(fz_context *ctx)
{
	return ctx->colorspace->bgr;
}

fz_colorspace *fz_device_cmyk(fz_context *ctx)
{
	return ctx->colorspace->cmyk;
}

fz_colorspace *fz_device_lab(fz_context *ctx)
{
	return ctx->colorspace->lab;
}

/* Same order as needed by LCMS */
static const char *fz_intent_names[] =
{
	"Perceptual",
	"RelativeColorimetric",
	"Saturation",
	"AbsoluteColorimetric",
};

int fz_lookup_rendering_intent(const char *name)
{
	int i;
	for (i = 0; i < (int)nelem(fz_intent_names); i++)
		if (!strcmp(name, fz_intent_names[i]))
			return i;
	return FZ_RI_RELATIVE_COLORIMETRIC;
}

const char *fz_rendering_intent_name(int ri)
{
	if (ri >= 0 && ri < (int)nelem(fz_intent_names))
		return fz_intent_names[ri];
	return "RelativeColorimetric";
}

/* Colorspace feature tests */

const char *fz_colorspace_name(fz_context *ctx, fz_colorspace *cs)
{
	return cs ? cs->name : "None";
}

enum fz_colorspace_type fz_colorspace_type(fz_context *ctx, fz_colorspace *cs)
{
	return cs ? cs->type : FZ_COLORSPACE_NONE;
}

int fz_colorspace_n(fz_context *ctx, fz_colorspace *cs)
{
	return cs ? cs->n : 0;
}

int fz_colorspace_is_gray(fz_context *ctx, fz_colorspace *cs)
{
	return cs && cs->type == FZ_COLORSPACE_GRAY;
}

int fz_colorspace_is_rgb(fz_context *ctx, fz_colorspace *cs)
{
	return cs && cs->type == FZ_COLORSPACE_RGB;
}

int fz_colorspace_is_cmyk(fz_context *ctx, fz_colorspace *cs)
{
	return cs && cs->type == FZ_COLORSPACE_CMYK;
}

int fz_colorspace_is_lab(fz_context *ctx, fz_colorspace *cs)
{
	return cs && cs->type == FZ_COLORSPACE_LAB;
}

int fz_colorspace_is_indexed(fz_context *ctx, fz_colorspace *cs)
{
	return cs && (cs->type == FZ_COLORSPACE_INDEXED);
}

int fz_colorspace_is_device_n(fz_context *ctx, fz_colorspace *cs)
{
	return cs && (cs->type == FZ_COLORSPACE_SEPARATION);
}

int fz_colorspace_is_subtractive(fz_context *ctx, fz_colorspace *cs)
{
	return cs && (cs->type == FZ_COLORSPACE_CMYK || cs->type == FZ_COLORSPACE_SEPARATION);
}

int fz_colorspace_is_device(fz_context *ctx, fz_colorspace *cs)
{
	return cs && (cs->flags & FZ_COLORSPACE_IS_DEVICE);
}

int fz_colorspace_is_icc(fz_context *ctx, fz_colorspace *cs)
{
	return cs && (cs->flags & FZ_COLORSPACE_IS_ICC);
}

int fz_colorspace_is_lab_icc(fz_context *ctx, fz_colorspace *cs)
{
	return cs && (cs->type == FZ_COLORSPACE_LAB) && (cs->flags & FZ_COLORSPACE_IS_ICC);
}

int fz_colorspace_is_device_gray(fz_context *ctx, fz_colorspace *cs)
{
	return fz_colorspace_is_device(ctx, cs) && fz_colorspace_is_gray(ctx, cs);
}

int fz_colorspace_is_device_cmyk(fz_context *ctx, fz_colorspace *cs)
{
	return fz_colorspace_is_device(ctx, cs) && fz_colorspace_is_cmyk(ctx, cs);
}

int fz_colorspace_device_n_has_only_cmyk(fz_context *ctx, fz_colorspace *cs)
{
	return cs && ((cs->flags & FZ_COLORSPACE_HAS_CMYK_AND_SPOTS) == FZ_COLORSPACE_HAS_CMYK);
}

int fz_colorspace_device_n_has_cmyk(fz_context *ctx, fz_colorspace *cs)
{
	return cs && (cs->flags & FZ_COLORSPACE_HAS_CMYK);
}

int fz_is_valid_blend_colorspace(fz_context *ctx, fz_colorspace *cs)
{
	return cs == NULL ||
		cs->type == FZ_COLORSPACE_GRAY ||
		cs->type == FZ_COLORSPACE_RGB ||
		cs->type == FZ_COLORSPACE_CMYK;
}

fz_colorspace *fz_base_colorspace(fz_context *ctx, fz_colorspace *cs)
{
	if (cs == NULL)
		return NULL;
	if (cs->type == FZ_COLORSPACE_INDEXED)
		return cs->u.indexed.base;
	return cs;
}

fz_colorspace *
fz_keep_colorspace(fz_context *ctx, fz_colorspace *cs)
{
	return fz_keep_key_storable(ctx, &cs->key_storable);
}

void
fz_drop_colorspace(fz_context *ctx, fz_colorspace *cs)
{
	fz_drop_key_storable(ctx, &cs->key_storable);
}

fz_colorspace *
fz_keep_colorspace_store_key(fz_context *ctx, fz_colorspace *cs)
{
	return fz_keep_key_storable_key(ctx, &cs->key_storable);
}

void
fz_drop_colorspace_store_key(fz_context *ctx, fz_colorspace *cs)
{
	fz_drop_key_storable_key(ctx, &cs->key_storable);
}

void
fz_drop_colorspace_imp(fz_context *ctx, fz_storable *cs_)
{
	fz_colorspace *cs = (fz_colorspace *)cs_;
	int i;

	if (cs->type == FZ_COLORSPACE_INDEXED)
	{
		fz_drop_colorspace(ctx, cs->u.indexed.base);
		fz_free(ctx, cs->u.indexed.lookup);
	}
	if (cs->type == FZ_COLORSPACE_SEPARATION)
	{
		fz_drop_colorspace(ctx, cs->u.separation.base);
		cs->u.separation.drop(ctx, cs->u.separation.tint);
		for (i = 0; i < FZ_MAX_COLORS; i++)
			fz_free(ctx, cs->u.separation.colorant[i]);
	}
#if FZ_ENABLE_ICC
	if (cs->flags & FZ_COLORSPACE_IS_ICC)
	{
		fz_drop_icc_profile(ctx, cs->u.icc.profile);
		fz_drop_buffer(ctx, cs->u.icc.buffer);
	}
#endif

	fz_free(ctx, cs->name);
	fz_free(ctx, cs);
}

fz_colorspace *
fz_new_colorspace(fz_context *ctx, enum fz_colorspace_type type, int flags, int n, const char *name)
{
	fz_colorspace *cs = fz_malloc_struct(ctx, fz_colorspace);
	FZ_INIT_KEY_STORABLE(cs, 1, fz_drop_colorspace_imp);

	if (n > FZ_MAX_COLORS)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "too many color components (%d > %d)", n, FZ_MAX_COLORS);
	if (n < 1)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "too few color components (%d < 1)", n);

	fz_try(ctx)
	{
		cs->type = type;
		cs->flags = flags;
		cs->n = n;
		cs->name = Memento_label(fz_strdup(ctx, name ? name : "UNKNOWN"), "cs_name");
	}
	fz_catch(ctx)
	{
		fz_free(ctx, cs);
		fz_rethrow(ctx);
	}

	return cs;
}

fz_colorspace *
fz_new_indexed_colorspace(fz_context *ctx, fz_colorspace *base, int high, unsigned char *lookup)
{
	fz_colorspace *cs;
	char name[100];
	if (high < 0 || high > 255)
		fz_throw(ctx, FZ_ERROR_SYNTAX, "invalid maximum value in indexed colorspace");
	fz_snprintf(name, sizeof name, "Indexed(%d,%s)", high, base->name);
	cs = fz_new_colorspace(ctx, FZ_COLORSPACE_INDEXED, 0, 1, name);
	cs->u.indexed.base = fz_keep_colorspace(ctx, base);
	cs->u.indexed.high = high;
	cs->u.indexed.lookup = lookup;
	return cs;
}

fz_colorspace *
fz_new_icc_colorspace(fz_context *ctx, enum fz_colorspace_type type, int flags, const char *name, fz_buffer *buf)
{
#if FZ_ENABLE_ICC
	fz_icc_profile *profile = NULL;
	fz_colorspace *cs = NULL;
	unsigned char *data;
	char name_buf[100];
	size_t size;
	int n;

	fz_var(profile);
	fz_var(cs);
	fz_var(type);

	fz_try(ctx)
	{
		size = fz_buffer_storage(ctx, buf, &data);
		profile = fz_new_icc_profile(ctx, data, size);
		n = fz_icc_profile_components(ctx, profile);
		switch (type)
		{
		default:
			fz_throw(ctx, FZ_ERROR_SYNTAX, "invalid colorspace type for ICC profile");
		case FZ_COLORSPACE_NONE:
			switch (n)
			{
			default:
				fz_throw(ctx, FZ_ERROR_SYNTAX, "ICC profile has unexpected number of channels: %d", n);
			case 1:
				type = FZ_COLORSPACE_GRAY;
				break;
			case 3:
				if (fz_icc_profile_is_lab(ctx, profile))
					type = FZ_COLORSPACE_LAB;
				else
					type = FZ_COLORSPACE_RGB;
				break;
			case 4:
				type = FZ_COLORSPACE_CMYK;
				break;
			}
			break;
		case FZ_COLORSPACE_GRAY:
			if (n != 1)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "ICC profile (N=%d) is not Gray", n);
			break;
		case FZ_COLORSPACE_RGB:
		case FZ_COLORSPACE_BGR:
			if (n != 3 || fz_icc_profile_is_lab(ctx, profile))
				fz_throw(ctx, FZ_ERROR_SYNTAX, "ICC profile (N=%d) is not RGB", n);
			break;
		case FZ_COLORSPACE_LAB:
			if (n != 3 || !fz_icc_profile_is_lab(ctx, profile))
				fz_throw(ctx, FZ_ERROR_SYNTAX, "ICC profile (N=%d) is not Lab", n);
			break;
		case FZ_COLORSPACE_CMYK:
			if (n != 4)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "ICC profile (N=%d) is not CMYK", n);
			break;
		}

		if (!name)
		{
			char cmm_name[100];
			fz_icc_profile_name(ctx, profile, cmm_name, sizeof cmm_name);
			switch (type)
			{
			default: fz_snprintf(name_buf, sizeof name_buf, "ICCBased(%d,%s)", n, cmm_name); break;
			case FZ_COLORSPACE_GRAY: fz_snprintf(name_buf, sizeof name_buf, "ICCBased(Gray,%s)", cmm_name); break;
			case FZ_COLORSPACE_RGB: fz_snprintf(name_buf, sizeof name_buf, "ICCBased(RGB,%s)", cmm_name); break;
			case FZ_COLORSPACE_BGR: fz_snprintf(name_buf, sizeof name_buf, "ICCBased(BGR,%s)", cmm_name); break;
			case FZ_COLORSPACE_CMYK: fz_snprintf(name_buf, sizeof name_buf, "ICCBased(CMYK,%s)", cmm_name); break;
			case FZ_COLORSPACE_LAB: fz_snprintf(name_buf, sizeof name_buf, "ICCBased(Lab,%s)", cmm_name); break;
			}
			name = name_buf;
		}

		cs = fz_new_colorspace(ctx, type, flags | FZ_COLORSPACE_IS_ICC, n, name);
		cs->u.icc.buffer = fz_keep_buffer(ctx, buf);
		cs->u.icc.profile = profile;
		fz_md5_buffer(ctx, buf, cs->u.icc.md5);
	}
	fz_catch(ctx)
	{
		fz_drop_icc_profile(ctx, profile);
		fz_drop_colorspace(ctx, cs);
		fz_rethrow(ctx);
	}
	return cs;
#else
	switch (type)
	{
	default: fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown colorspace type");
	case FZ_COLORSPACE_GRAY: return fz_keep_colorspace(ctx, fz_device_gray(ctx));
	case FZ_COLORSPACE_RGB: return fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	case FZ_COLORSPACE_BGR: return fz_keep_colorspace(ctx, fz_device_bgr(ctx));
	case FZ_COLORSPACE_CMYK: return fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
	case FZ_COLORSPACE_LAB: return fz_keep_colorspace(ctx, fz_device_lab(ctx));
	}
#endif
}

fz_colorspace *fz_new_cal_gray_colorspace(fz_context *ctx, float wp[3], float bp[3], float gamma)
{
#if FZ_ENABLE_ICC
	fz_buffer *buf = fz_new_icc_data_from_cal(ctx, wp, bp, &gamma, NULL, 1);
	fz_colorspace *cs;
	fz_try(ctx)
		cs = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_GRAY, 0, "CalGray", buf);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return cs;
#else
	return fz_keep_colorspace(ctx, fz_device_gray(ctx));
#endif
}

fz_colorspace *fz_new_cal_rgb_colorspace(fz_context *ctx, float wp[3], float bp[3], float gamma[3], float matrix[9])
{
#if FZ_ENABLE_ICC
	fz_buffer *buf = fz_new_icc_data_from_cal(ctx, wp, bp, gamma, matrix, 3);
	fz_colorspace *cs;
	fz_try(ctx)
		cs = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_RGB, 0, "CalRGB", buf);
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return cs;
#else
	return fz_keep_colorspace(ctx, fz_device_rgb(ctx));
#endif
}

void fz_colorspace_name_colorant(fz_context *ctx, fz_colorspace *cs, int i, const char *name)
{
	if (i < 0 || i >= cs->n)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Attempt to name out of range colorant");
	if (cs->type != FZ_COLORSPACE_SEPARATION)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Attempt to name colorant for non-separation colorspace");

	fz_free(ctx, cs->u.separation.colorant[i]);
	cs->u.separation.colorant[i] = NULL;
	cs->u.separation.colorant[i] = fz_strdup(ctx, name);

	if (!strcmp(name, "Cyan") || !strcmp(name, "Magenta") || !strcmp(name, "Yellow") || !strcmp(name, "Black"))
		cs->flags |= FZ_COLORSPACE_HAS_CMYK;
	else
		cs->flags |= FZ_COLORSPACE_HAS_SPOTS;
}

const char *fz_colorspace_colorant(fz_context *ctx, fz_colorspace *cs, int i)
{
	if (!cs || i < 0 || i >= cs->n)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Colorant out of range");
	switch (cs->type)
	{
	case FZ_COLORSPACE_NONE:
		return "None";
	case FZ_COLORSPACE_GRAY:
		return "Gray";
	case FZ_COLORSPACE_RGB:
		if (i == 0) return "Red";
		if (i == 1) return "Green";
		if (i == 2) return "Blue";
		break;
	case FZ_COLORSPACE_BGR:
		if (i == 0) return "Blue";
		if (i == 1) return "Green";
		if (i == 2) return "Red";
		break;
	case FZ_COLORSPACE_CMYK:
		if (i == 0) return "Cyan";
		if (i == 1) return "Magenta";
		if (i == 2) return "Yellow";
		if (i == 3) return "Black";
		break;
	case FZ_COLORSPACE_LAB:
		if (i == 0) return "L*";
		if (i == 1) return "a*";
		if (i == 2) return "b*";
		break;
	case FZ_COLORSPACE_INDEXED:
		return "Index";
	case FZ_COLORSPACE_SEPARATION:
		return cs->u.separation.colorant[i];
	}
	return "None";
}

void
fz_clamp_color(fz_context *ctx, fz_colorspace *cs, const float *in, float *out)
{
	if (cs->type == FZ_COLORSPACE_LAB)
	{
		out[0] = fz_clamp(in[0], 0, 100);
		out[1] = fz_clamp(in[1], -128, 127);
		out[2] = fz_clamp(in[2], -128, 127);
	}
	else if (cs->type == FZ_COLORSPACE_INDEXED)
	{
		/* round color index to integer before rescaling to hival */
		out[0] = fz_clamp((int)(in[0]+0.5), 0, cs->u.indexed.high) / 255.0f;
	}
	else
	{
		int i, n = cs->n;
		for (i = 0; i < n; ++i)
			out[i] = fz_clamp(in[i], 0, 1);
	}
}

const fz_color_params fz_default_color_params = { FZ_RI_RELATIVE_COLORIMETRIC, 1, 0, 0 };

fz_default_colorspaces *fz_new_default_colorspaces(fz_context *ctx)
{
	fz_default_colorspaces *default_cs = fz_malloc_struct(ctx, fz_default_colorspaces);
	default_cs->refs = 1;
	default_cs->gray = fz_keep_colorspace(ctx, fz_device_gray(ctx));
	default_cs->rgb = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	default_cs->cmyk = fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
	default_cs->oi = NULL;
	return default_cs;
}

fz_default_colorspaces *fz_clone_default_colorspaces(fz_context *ctx, fz_default_colorspaces *base)
{
	fz_default_colorspaces *default_cs = fz_malloc_struct(ctx, fz_default_colorspaces);
	default_cs->refs = 1;
	if (base)
	{
		default_cs->gray = fz_keep_colorspace(ctx, base->gray);
		default_cs->rgb = fz_keep_colorspace(ctx, base->rgb);
		default_cs->cmyk = fz_keep_colorspace(ctx, base->cmyk);
		default_cs->oi = fz_keep_colorspace(ctx, base->oi);
	}
	return default_cs;
}

fz_default_colorspaces *fz_keep_default_colorspaces(fz_context *ctx, fz_default_colorspaces *default_cs)
{
	return fz_keep_imp(ctx, default_cs, &default_cs->refs);
}

void
fz_drop_default_colorspaces(fz_context *ctx, fz_default_colorspaces *default_cs)
{
	if (fz_drop_imp(ctx, default_cs, &default_cs->refs))
	{
		fz_drop_colorspace(ctx, default_cs->gray);
		fz_drop_colorspace(ctx, default_cs->rgb);
		fz_drop_colorspace(ctx, default_cs->cmyk);
		fz_drop_colorspace(ctx, default_cs->oi);
		fz_free(ctx, default_cs);
	}
}

fz_colorspace *fz_default_gray(fz_context *ctx, const fz_default_colorspaces *default_cs)
{
	return (default_cs && default_cs->gray) ? default_cs->gray : fz_device_gray(ctx);
}

fz_colorspace *fz_default_rgb(fz_context *ctx, const fz_default_colorspaces *default_cs)
{
	return (default_cs && default_cs->rgb) ? default_cs->rgb : fz_device_rgb(ctx);
}

fz_colorspace *fz_default_cmyk(fz_context *ctx, const fz_default_colorspaces *default_cs)
{
	return (default_cs && default_cs->cmyk)  ? default_cs->cmyk : fz_device_cmyk(ctx);
}

fz_colorspace *fz_default_output_intent(fz_context *ctx, const fz_default_colorspaces *default_cs)
{
	return default_cs ? default_cs->oi : NULL;
}

void fz_set_default_gray(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs)
{
	if (cs->type == FZ_COLORSPACE_GRAY && cs->n == 1)
	{
		fz_drop_colorspace(ctx, default_cs->gray);
		default_cs->gray = fz_keep_colorspace(ctx, cs);
	}
}

void fz_set_default_rgb(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs)
{
	if (cs->type == FZ_COLORSPACE_RGB && cs->n == 3)
	{
		fz_drop_colorspace(ctx, default_cs->rgb);
		default_cs->rgb = fz_keep_colorspace(ctx, cs);
	}
}

void fz_set_default_cmyk(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs)
{
	if (cs->type == FZ_COLORSPACE_CMYK && cs->n == 4)
	{
		fz_drop_colorspace(ctx, default_cs->cmyk);
		default_cs->cmyk = fz_keep_colorspace(ctx, cs);
	}
}

void fz_set_default_output_intent(fz_context *ctx, fz_default_colorspaces *default_cs, fz_colorspace *cs)
{
	fz_drop_colorspace(ctx, default_cs->oi);
	default_cs->oi = NULL;

	/* FIXME: Why do we set DefaultXXX along with the output intent?! */
	switch (cs->type)
	{
	default:
		fz_warn(ctx, "Ignoring incompatible output intent: %s.", cs->name);
		break;
	case FZ_COLORSPACE_GRAY:
		default_cs->oi = fz_keep_colorspace(ctx, cs);
		if (default_cs->gray == fz_device_gray(ctx))
			fz_set_default_gray(ctx, default_cs, cs);
		break;
	case FZ_COLORSPACE_RGB:
		default_cs->oi = fz_keep_colorspace(ctx, cs);
		if (default_cs->rgb == fz_device_rgb(ctx))
			fz_set_default_rgb(ctx, default_cs, cs);
		break;
	case FZ_COLORSPACE_CMYK:
		default_cs->oi = fz_keep_colorspace(ctx, cs);
		if (default_cs->cmyk == fz_device_cmyk(ctx))
			fz_set_default_cmyk(ctx, default_cs, cs);
		break;
	}
}

/* Link cache */

#if FZ_ENABLE_ICC

typedef struct {
	int refs;
	unsigned char src_md5[16];
	unsigned char dst_md5[16];
	fz_color_params rend;
	unsigned char src_extras;
	unsigned char dst_extras;
	unsigned char copy_spots;
	unsigned char format;
	unsigned char proof;
	unsigned char bgr;
} fz_link_key;

static void *
fz_keep_link_key(fz_context *ctx, void *key_)
{
	fz_link_key *key = (fz_link_key *)key_;
	return fz_keep_imp(ctx, key, &key->refs);
}

static void
fz_drop_link_key(fz_context *ctx, void *key_)
{
	fz_link_key *key = (fz_link_key *)key_;
	if (fz_drop_imp(ctx, key, &key->refs))
		fz_free(ctx, key);
}

static int
fz_cmp_link_key(fz_context *ctx, void *k0_, void *k1_)
{
	fz_link_key *k0 = (fz_link_key *)k0_;
	fz_link_key *k1 = (fz_link_key *)k1_;
	return
		memcmp(k0->src_md5, k1->src_md5, 16) == 0 &&
		memcmp(k0->dst_md5, k1->dst_md5, 16) == 0 &&
		k0->src_extras == k1->src_extras &&
		k0->dst_extras == k1->dst_extras &&
		k0->rend.bp == k1->rend.bp &&
		k0->rend.ri == k1->rend.ri &&
		k0->copy_spots == k1->copy_spots &&
		k0->format == k1->format &&
		k0->proof == k1->proof &&
		k0->bgr == k1->bgr;
}

static void
fz_format_link_key(fz_context *ctx, char *s, size_t n, void *key_)
{
	static const char *hex = "0123456789abcdef";
	fz_link_key *key = (fz_link_key *)key_;
	char sm[33], dm[33];
	int i;
	for (i = 0; i < 16; ++i)
	{
		sm[i*2+0] = hex[key->src_md5[i]>>4];
		sm[i*2+1] = hex[key->src_md5[i]&15];
		dm[i*2+0] = hex[key->dst_md5[i]>>4];
		dm[i*2+1] = hex[key->dst_md5[i]&15];
	}
	sm[32] = 0;
	dm[32] = 0;
	fz_snprintf(s, n, "(link src_md5=%s dst_md5=%s)", sm, dm);
}

static int
fz_make_hash_link_key(fz_context *ctx, fz_store_hash *hash, void *key_)
{
	fz_link_key *key = (fz_link_key *)key_;
	memcpy(hash->u.link.dst_md5, key->dst_md5, 16);
	memcpy(hash->u.link.src_md5, key->src_md5, 16);
	hash->u.link.ri = key->rend.ri;
	hash->u.link.bp = key->rend.bp;
	hash->u.link.src_extras = key->src_extras;
	hash->u.link.dst_extras = key->dst_extras;
	hash->u.link.format = key->format;
	hash->u.link.proof = key->proof;
	hash->u.link.copy_spots = key->copy_spots;
	hash->u.link.bgr = key->bgr;
	return 1;
}

static fz_store_type fz_link_store_type =
{
	"fz_icc_link",
	fz_make_hash_link_key,
	fz_keep_link_key,
	fz_drop_link_key,
	fz_cmp_link_key,
	fz_format_link_key,
	NULL
};

fz_icc_link *
fz_find_icc_link(fz_context *ctx,
	fz_colorspace *src, int src_extras,
	fz_colorspace *dst, int dst_extras,
	fz_colorspace *prf,
	fz_color_params rend,
	int format,
	int copy_spots,
	int premult)
{
	fz_icc_link *link, *old_link;
	fz_link_key key, *new_key;

	fz_var(link);

	/* Check the storable to see if we have a copy. */
	key.refs = 1;
	memcpy(&key.src_md5, src->u.icc.md5, 16);
	memcpy(&key.dst_md5, dst->u.icc.md5, 16);
	key.rend = rend;
	key.src_extras = src_extras;
	key.dst_extras = dst_extras;
	key.copy_spots = copy_spots;
	key.format = (format & 1) | (premult*2);
	key.proof = (prf != NULL);
	key.bgr = (dst->type == FZ_COLORSPACE_BGR);

	link = fz_find_item(ctx, fz_drop_icc_link_imp, &key, &fz_link_store_type);
	if (!link)
	{
		new_key = fz_malloc_struct(ctx, fz_link_key);
		memcpy(new_key, &key, sizeof (fz_link_key));
		fz_try(ctx)
		{
			link = fz_new_icc_link(ctx, src, src_extras, dst, dst_extras, prf, rend, format, copy_spots, premult);
			old_link = fz_store_item(ctx, new_key, link, 1000, &fz_link_store_type);
			if (old_link)
			{
				/* Found one while adding! Perhaps from another thread? */
				fz_drop_icc_link(ctx, link);
				link = old_link;
			}
		}
		fz_always(ctx)
		{
			fz_drop_link_key(ctx, new_key);
		}
		fz_catch(ctx)
		{
			fz_drop_icc_link(ctx, link);
			fz_rethrow(ctx);
		}
	}
	return link;
}

#endif

/* Color conversions */

static void indexed_via_base(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst)
{
	fz_colorspace *ss = cc->ss_via;
	const unsigned char *lookup = ss->u.indexed.lookup;
	int high = ss->u.indexed.high;
	int n = ss->u.indexed.base->n;
	float base[4];
	int i, k;

	i = src[0] * 255;
	i = fz_clampi(i, 0, high);
	if (ss->u.indexed.base->type == FZ_COLORSPACE_LAB)
	{
		base[0] = lookup[i * 3 + 0] * 100 / 255.0f;
		base[1] = lookup[i * 3 + 1] - 128;
		base[2] = lookup[i * 3 + 2] - 128;
	}
	else
	{
		for (k = 0; k < n; ++k)
			base[k] = lookup[i * n + k] / 255.0f;
	}

	cc->convert_via(ctx, cc, base, dst);
}

static void separation_via_base(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst)
{
	fz_colorspace *ss = cc->ss_via;
	float base[4];
	ss->u.separation.eval(ctx, ss->u.separation.tint, src, ss->n, base, ss->u.separation.base->n);
	cc->convert_via(ctx, cc, base, dst);
}

static void indexed_via_separation_via_base(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst)
{
	fz_colorspace *ss = cc->ss_via;
	fz_colorspace *ssep = cc->ss_via->u.indexed.base;
	const unsigned char *lookup = ss->u.indexed.lookup;
	int high = ss->u.indexed.high;
	int n = ss->u.indexed.base->n;
	float base[4], mid[FZ_MAX_COLORS];
	int i, k;

	/* First map through the index. */
	i = src[0] * 255;
	i = fz_clampi(i, 0, high);
	for (k = 0; k < n; ++k)
		mid[k] = lookup[i * n + k] / 255.0f;

	/* Then map through the separation. */
	ssep->u.separation.eval(ctx, ssep->u.separation.tint, mid, ssep->n, base, ssep->u.separation.base->n);

	/* Then convert in the base. */
	cc->convert_via(ctx, cc, base, dst);
}

static void
fz_init_process_color_converter(fz_context *ctx, fz_color_converter *cc, fz_colorspace *ss, fz_colorspace *ds, fz_colorspace *is, fz_color_params params)
{
	if (ss->type == FZ_COLORSPACE_INDEXED)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "base colorspace must not be indexed");
	if (ss->type == FZ_COLORSPACE_SEPARATION)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "base colorspace must not be separation");

#if FZ_ENABLE_ICC
	if (ctx->icc_enabled)
	{
		/* Handle identity case. */
		if (ss == ds || (!memcmp(ss->u.icc.md5, ds->u.icc.md5, 16)))
		{
			cc->convert = fz_lookup_fast_color_converter(ctx, ss, ds);
			return;
		}

		/* Handle DeviceGray to CMYK as K only. See note in Section 6.3 of PDF spec 1.7. */
		if (ss->type == FZ_COLORSPACE_GRAY && (ss->flags & FZ_COLORSPACE_IS_DEVICE))
		{
			if (ds->type == FZ_COLORSPACE_CMYK)
			{
				cc->convert = fz_lookup_fast_color_converter(ctx, ss, ds);
				return;
			}
		}

		fz_try(ctx)
		{
			cc->link = fz_find_icc_link(ctx, ss, 0, ds, 0, is, params, 1, 0, 0);
			cc->convert = fz_icc_transform_color;
		}
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
			fz_warn(ctx, "cannot create ICC link, falling back to fast color conversion");
			cc->convert = fz_lookup_fast_color_converter(ctx, ss, ds);
		}
	}
	else
	{
		cc->convert = fz_lookup_fast_color_converter(ctx, ss, ds);
	}
#else
	cc->convert = fz_lookup_fast_color_converter(ctx, ss, ds);
#endif
}

void
fz_find_color_converter(fz_context *ctx, fz_color_converter *cc, fz_colorspace *ss, fz_colorspace *ds, fz_separations *dseps, fz_colorspace *is, fz_color_params params)
{
	cc->ds = ds;
	cc->dseps = NULL;
	cc->dst_n = ds->n;
#if FZ_ENABLE_ICC
	cc->link = NULL;
#endif

	if (ds->type == FZ_COLORSPACE_INDEXED)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot convert into Indexed colorspace.");
	if (ds->type == FZ_COLORSPACE_SEPARATION)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "Cannot convert into Separation colorspace.");

	if (params.ri & FZ_RI_IN_SOFTMASK)
	{
		if (ss->type == FZ_COLORSPACE_GRAY)
			ss = ctx->colorspace->ps_gray;
		else if (ss->type == FZ_COLORSPACE_RGB)
			ss = ctx->colorspace->ps_rgb;
		else if (ss->type == FZ_COLORSPACE_BGR)
			ss = ctx->colorspace->ps_bgr;
		else if (ss->type == FZ_COLORSPACE_CMYK)
			ss = ctx->colorspace->ps_cmyk;
		if (ds->type == FZ_COLORSPACE_GRAY)
			ds = ctx->colorspace->ps_gray;
		else if (ds->type == FZ_COLORSPACE_RGB)
			ds = ctx->colorspace->ps_rgb;
		else if (ds->type == FZ_COLORSPACE_BGR)
			ds = ctx->colorspace->ps_bgr;
		else if (ds->type == FZ_COLORSPACE_CMYK)
			ds = ctx->colorspace->ps_cmyk;
		params.ri &= ~FZ_RI_IN_SOFTMASK;
	}

	if (ss->type == FZ_COLORSPACE_INDEXED)
	{
		if (ss->u.indexed.base->type == FZ_COLORSPACE_SEPARATION)
		{
			cc->ss = ss->u.indexed.base->u.separation.base;
			cc->ss_via = ss;
			fz_init_process_color_converter(ctx, cc, cc->ss, ds, is, params);
			cc->convert_via = cc->convert;
			cc->convert = indexed_via_separation_via_base;
		}
		else
		{
			cc->ss = ss->u.indexed.base;
			cc->ss_via = ss;
			fz_init_process_color_converter(ctx, cc, cc->ss, ds, is, params);
			cc->convert_via = cc->convert;
			cc->convert = indexed_via_base;
		}
	}
	else if (ss->type == FZ_COLORSPACE_SEPARATION)
	{
		if (dseps &&
			fz_init_separation_copy_color_converter(ctx, cc, ss, ds, dseps, is, params))
		{
			/* We can just copy separations from ss to dseps */
			cc->dseps = dseps;
			cc->dst_n += fz_count_separations(ctx, dseps);
		}
		else
		{
			cc->ss = ss->u.separation.base;
			cc->ss_via = ss;
			fz_init_process_color_converter(ctx, cc, cc->ss, ds, is, params);
			cc->convert_via = cc->convert;
			cc->convert = separation_via_base;
		}
	}
	else
	{
		cc->ss = ss;
		fz_init_process_color_converter(ctx, cc, ss, ds, is, params);
	}
}

void
fz_drop_color_converter(fz_context *ctx, fz_color_converter *cc)
{
#if FZ_ENABLE_ICC
	if (cc->link)
	{
		fz_drop_icc_link(ctx, cc->link);
		cc->link = NULL;
	}
#endif
}

void
fz_convert_color(fz_context *ctx, fz_colorspace *ss, const float *sv, fz_colorspace *ds, float *dv, fz_colorspace *is, fz_color_params params)
{
	fz_color_converter cc;
	fz_find_color_converter(ctx, &cc, ss, ds, NULL, is, params);
	cc.convert(ctx, &cc, sv, dv);
	fz_drop_color_converter(ctx, &cc);
}

/* Cached color converter using hash table. */

typedef struct fz_cached_color_converter
{
	fz_color_converter base;
	fz_hash_table *hash;
} fz_cached_color_converter;

static void fz_cached_color_convert(fz_context *ctx, fz_color_converter *cc_, const float *ss, float *ds)
{
	fz_cached_color_converter *cc = cc_->opaque;
	if (cc->hash)
	{
		float *val = fz_hash_find(ctx, cc->hash, ss);
		int n = cc->base.dst_n * sizeof(float);

		if (val)
		{
			memcpy(ds, val, n);
			return;
		}

		cc->base.convert(ctx, &cc->base, ss, ds);

		val = Memento_label(fz_malloc_array(ctx, cc->base.dst_n, float), "cached_color_convert");
		memcpy(val, ds, n);
		fz_try(ctx)
			fz_hash_insert(ctx, cc->hash, ss, val);
		fz_catch(ctx)
		{
			fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
			fz_report_error(ctx);
			fz_free(ctx, val);
		}
	}
	else
	{
		cc->base.convert(ctx, &cc->base, ss, ds);
	}
}

void fz_init_cached_color_converter(fz_context *ctx, fz_color_converter *cc, fz_colorspace *ss, fz_colorspace *ds, fz_separations *dseps, fz_colorspace *is, fz_color_params params)
{
	int n = ss->n;
	fz_cached_color_converter *cached = fz_malloc_struct(ctx, fz_cached_color_converter);

	cc->opaque = cached;
	cc->convert = fz_cached_color_convert;
	cc->ss = ss;
	cc->ds = ds;
#if FZ_ENABLE_ICC
	cc->link = NULL;
#endif

	fz_try(ctx)
	{
		fz_find_color_converter(ctx, &cached->base, ss, ds, dseps, is, params);
		if (n * sizeof(float) <= FZ_HASH_TABLE_KEY_LENGTH)
			cached->hash = fz_new_hash_table(ctx, 256, n * sizeof(float), -1, fz_free);
		else
			fz_warn(ctx, "colorspace has too many components to be cached");
	}
	fz_catch(ctx)
	{
		fz_drop_color_converter(ctx, &cached->base);
		fz_drop_hash_table(ctx, cached->hash);
		fz_free(ctx, cached);
		cc->opaque = NULL;
		fz_rethrow(ctx);
	}
}

void fz_fin_cached_color_converter(fz_context *ctx, fz_color_converter *cc_)
{
	fz_cached_color_converter *cc;
	if (cc_ == NULL)
		return;
	cc = cc_->opaque;
	if (cc == NULL)
		return;
	cc_->opaque = NULL;
	fz_drop_hash_table(ctx, cc->hash);
	fz_drop_color_converter(ctx, &cc->base);
	fz_free(ctx, cc);
}

/* Pixmap color conversion */

static inline void
template_convert_lab(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params, int sa, int da, int spots)
{
	float srcv[FZ_MAX_COLORS];
	float dstv[FZ_MAX_COLORS];
	size_t w = src->w;
	int h = src->h;
	fz_colorspace *src_cs = src->colorspace;
	fz_colorspace *dst_cs = dst->colorspace;

	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;

	int src_n = spots ? src->n : 3+sa;
	int dst_c = dst->n - (spots ? dst->s : 0) - da;
	int dst_n = dst->n;

	fz_color_converter cc;
	int alpha = 255;
	ptrdiff_t d_line_inc = dst->stride - w * dst->n;
	ptrdiff_t s_line_inc = src->stride - w * src->n;

	int k;

	fz_find_color_converter(ctx, &cc, src_cs, dst_cs, NULL, is, params);
	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			if (sa)
			{
				alpha = s[4];
				srcv[0] = fz_div255(s[0], alpha) / 255.0f * 100;
				srcv[1] = fz_div255(s[1], alpha) - 128;
				srcv[2] = fz_div255(s[2], alpha) - 128;
			}
			else
			{
				srcv[0] = s[0] / 255.0f * 100;
				srcv[1] = s[1] - 128;
				srcv[2] = s[2] - 128;
			}
			s += src_n;

			cc.convert(ctx, &cc, srcv, dstv);

			if (da)
			{
				for (k = 0; k < dst_c; k++)
					*d++ = fz_mul255(dstv[k] * 255, alpha);
				/* Just fill in spots as empty */
				if (spots)
					for (; k < dst_n; k++)
						*d++ = 0;
				*d++ = alpha;
			}
			else
			{
				for (k = 0; k < dst_c; k++)
					*d++ = dstv[k] * 255;
				if (spots)
					for (; k < dst_n; k++)
						*d++ = 0;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
	fz_drop_color_converter(ctx, &cc);
}

static void convert_lab(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_convert_lab(ctx, src, dst, is, params, 0, 0, 0);
}

static void convert_lab_sa(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_convert_lab(ctx, src, dst, is, params, 1, 0, 0);
}

static void convert_lab_da(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_convert_lab(ctx, src, dst, is, params, 0, 1, 0);
}

static void convert_lab_sa_da(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_convert_lab(ctx, src, dst, is, params, 1, 1, 0);
}

static void convert_lab_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_convert_lab(ctx, src, dst, is, params, 0, 0, 1);
}

static void convert_lab_sa_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_convert_lab(ctx, src, dst, is, params, 1, 0, 1);
}

static void convert_lab_da_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_convert_lab(ctx, src, dst, is, params, 0, 1, 1);
}

static void convert_lab_sa_da_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_convert_lab(ctx, src, dst, is, params, 1, 1, 1);
}

static inline void
template_brute_force(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params, int sa, int da, int spots)
{
	float srcv[FZ_MAX_COLORS];
	float dstv[FZ_MAX_COLORS];
	size_t w = src->w;
	int h = src->h;
	fz_colorspace *src_cs = src->colorspace;
	fz_colorspace *dst_cs = dst->colorspace;

	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;

	int src_c = src->n - (spots ? src->s : 0) - sa;
	int src_n = src->n;
	int dst_c = dst->n - (spots ? dst->s : 0) - da;
	int dst_n = dst->n;

	fz_color_converter cc;
	int alpha = 255;
	ptrdiff_t d_line_inc = dst->stride - w * dst->n;
	ptrdiff_t s_line_inc = src->stride - w * src->n;

	int k;

	fz_find_color_converter(ctx, &cc, src_cs, dst_cs, NULL, is, params);
	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			if (sa)
			{
				alpha = s[src_n];
				for (k = 0; k < src_c; k++)
					srcv[k] = fz_div255(s[k], alpha) / 255.0f;
			}
			else
			{
				for (k = 0; k < src_c; k++)
					srcv[k] = s[k] / 255.0f;
			}
			s += src_n;

			cc.convert(ctx, &cc, srcv, dstv);

			if (da)
			{
				for (k = 0; k < dst_c; k++)
					*d++ = fz_mul255(dstv[k] * 255, alpha);
				if (spots)
					for (; k < dst_n; k++)
						*d++ = 0;
				*d++ = alpha;
			}
			else
			{
				for (k = 0; k < dst_c; k++)
					*d++ = dstv[k] * 255;
				if (spots)
					for (; k < dst_n; k++)
						*d++ = 0;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
	fz_drop_color_converter(ctx, &cc);
}

static void brute_force(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_brute_force(ctx, src, dst, is, params, 0, 0, 0);
}

static void brute_force_sa(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_brute_force(ctx, src, dst, is, params, 1, 0, 0);
}

static void brute_force_da(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_brute_force(ctx, src, dst, is, params, 0, 1, 0);
}

static void brute_force_sa_da(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_brute_force(ctx, src, dst, is, params, 1, 1, 0);
}

static void brute_force_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_brute_force(ctx, src, dst, is, params, 0, 0, 1);
}

static void brute_force_sa_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_brute_force(ctx, src, dst, is, params, 1, 0, 1);
}

static void brute_force_da_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_brute_force(ctx, src, dst, is, params, 0, 1, 1);
}

static void brute_force_sa_da_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	template_brute_force(ctx, src, dst, is, params, 1, 1, 1);
}

static void
lookup_1d(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	float srcv[FZ_MAX_COLORS];
	float dstv[FZ_MAX_COLORS];
	size_t w = src->w;
	int h = src->h;
	fz_colorspace *src_cs = src->colorspace;
	fz_colorspace *dst_cs = dst->colorspace;

	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;

	int sa = src->alpha;
	int da = dst->alpha;
	int dst_s = dst->s;
	int dst_n = dst->n;
	int dst_c = dst_n - dst_s - da;

	fz_color_converter cc;
	int alpha = 255;
	ptrdiff_t d_line_inc = dst->stride - w * dst->n;
	ptrdiff_t s_line_inc = src->stride - w * src->n;

	int i, k;

	unsigned char lookup[FZ_MAX_COLORS * 256];

	fz_find_color_converter(ctx, &cc, src_cs, dst_cs, NULL, is, params);
	for (i = 0; i < 256; i++)
	{
		srcv[0] = i / 255.0f;
		cc.convert(ctx, &cc, srcv, dstv);
		for (k = 0; k < dst_c; k++)
			lookup[i * dst_c + k] = dstv[k] * 255;
	}
	fz_drop_color_converter(ctx, &cc);

	while (h--)
	{
		size_t ww = w;
		while (ww--)
		{
			if (sa)
			{
				alpha = s[1];
				i = fz_div255(s[0], alpha);
				s += 2;
			}
			else
			{
				i = *s++;
			}

			if (da)
			{
				for (k = 0; k < dst_c; k++)
					*d++ = fz_mul255(lookup[i * dst_c + k], alpha);
				for (; k < dst_n; k++)
					*d++ = 0;
				*d++ = alpha;
			}
			else
			{
				for (k = 0; k < dst_c; k++)
					*d++ = lookup[i * dst_c + k];
				for (; k < dst_n; k++)
					*d++ = 0;
			}
		}
		d += d_line_inc;
		s += s_line_inc;
	}
}

static void
memoize_nospots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	float srcv[FZ_MAX_COLORS];
	float dstv[FZ_MAX_COLORS];
	size_t w = src->w;
	int h = src->h;
	fz_colorspace *src_cs = src->colorspace;
	fz_colorspace *dst_cs = dst->colorspace;

	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;

	int sa = src->alpha;
	int src_s = src->s;
	int src_n = src->n;
	int src_c = src_n - src_s - sa;
	int da = dst->alpha;
	int dst_s = dst->s;
	int dst_n = dst->n;
	int dst_c = dst_n - dst_s - da;

	ptrdiff_t d_line_inc = dst->stride - w * dst->n;
	ptrdiff_t s_line_inc = src->stride - w * src->n;

	int k;

	fz_hash_table *lookup;
	unsigned char *color;
	unsigned char dummy = s[0] ^ 255;
	unsigned char *sold = &dummy;
	unsigned char *dold;
	fz_color_converter cc;
	int alpha = 255;

	lookup = fz_new_hash_table(ctx, 509, src_n, -1, NULL);
	fz_find_color_converter(ctx, &cc, src_cs, dst_cs, NULL, is, params);

	fz_try(ctx)
	{
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				if (*s == *sold && memcmp(sold, s, src_n) == 0)
				{
					sold = s;
					memcpy(d, dold, dst_n);
				}
				else
				{
					sold = s;
					dold = d;
					color = fz_hash_find(ctx, lookup, s);
					if (color)
					{
						memcpy(d, color, dst_n);
					}
					else
					{
						if (sa)
						{
							alpha = s[src_c];
							for (k = 0; k < src_c; k++)
								srcv[k] = fz_div255(s[k], alpha) / 255.0f;
						}
						else
						{
							for (k = 0; k < src_c; k++)
								srcv[k] = s[k] / 255.0f;
						}

						cc.convert(ctx, &cc, srcv, dstv);

						if (da)
						{
							for (k = 0; k < dst_c; k++)
								d[k] = fz_mul255(dstv[k] * 255, alpha);
							d[k] = alpha;
						}
						else
						{
							for (k = 0; k < dst_c; k++)
								d[k] = dstv[k] * 255;
						}

						fz_hash_insert(ctx, lookup, s, d);
					}
				}
				s += src_n;
				d += dst_n;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	fz_always(ctx)
	{
		fz_drop_color_converter(ctx, &cc);
		fz_drop_hash_table(ctx, lookup);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
memoize_spots(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params)
{
	float srcv[FZ_MAX_COLORS];
	float dstv[FZ_MAX_COLORS];
	size_t w = src->w;
	int h = src->h;
	fz_colorspace *src_cs = src->colorspace;
	fz_colorspace *dst_cs = dst->colorspace;

	unsigned char *s = src->samples;
	unsigned char *d = dst->samples;

	int sa = src->alpha;
	int src_s = src->s;
	int src_n = src->n;
	int src_c = src_n - src_s - sa;
	int src_m = src_c + sa;
	int da = dst->alpha;
	int dst_s = dst->s;
	int dst_n = dst->n;
	int dst_c = dst_n - dst_s - da;

	ptrdiff_t d_line_inc = dst->stride - w * dst->n;
	ptrdiff_t s_line_inc = src->stride - w * src->n;

	int k;

	fz_hash_table *lookup;
	unsigned char *color;
	unsigned char sold[FZ_MAX_COLORS];
	unsigned char dold[FZ_MAX_COLORS];
	fz_color_converter cc;
	int alpha = 255;

	sold[0] = s[0] ^ 255;

	lookup = fz_new_hash_table(ctx, 509, src_m, -1, NULL);
	fz_find_color_converter(ctx, &cc, src_cs, dst_cs, NULL, is, params);

	fz_try(ctx)
	{
		while (h--)
		{
			size_t ww = w;
			while (ww--)
			{
				if (*s == *sold && memcmp(sold, s, src_m) == 0)
				{
					memcpy(d, dold, dst_c);
					if (dst_s)
						memset(d + dst_c, 0, dst_s);
					if (da)
						d[dst_n-1] = sold[src_m];
				}
				else
				{
					memcpy(sold, s, src_m);
					if (sa)
						sold[src_m] = s[src_n-1];
					color = fz_hash_find(ctx, lookup, sold);
					if (color)
					{
						memcpy(d, color, dst_n);
					}
					else
					{
						if (sa)
						{
							alpha = s[src_c];
							for (k = 0; k < src_c; k++)
								srcv[k] = fz_div255(s[k], alpha) / 255.0f;
						}
						else
						{
							for (k = 0; k < src_c; k++)
								srcv[k] = s[k] / 255.0f;
						}

						cc.convert(ctx, &cc, srcv, dstv);

						if (da)
						{
							for (k = 0; k < dst_c; k++)
								d[k] = fz_mul255(dstv[k] * 255, alpha);
							if (dst_s)
								memset(d + dst_c, 0, dst_s);
							dold[dst_c] = d[dst_n-1] = alpha;
						}
						else
						{
							for (k = 0; k < dst_c; k++)
								d[k] = dstv[k] * 255;
							if (dst_s)
								memset(d + dst_c, 0, dst_s);
						}
						memcpy(dold, d, dst_c);

						fz_hash_insert(ctx, lookup, sold, dold);
					}
				}
				s += src_n;
				d += dst_n;
			}
			d += d_line_inc;
			s += s_line_inc;
		}
	}
	fz_always(ctx)
	{
		fz_drop_color_converter(ctx, &cc);
		fz_drop_hash_table(ctx, lookup);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
fz_convert_slow_pixmap_samples(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst, fz_colorspace *is, fz_color_params params, int copy_spots)
{
	int sa = src->alpha;
	int src_s = src->s;
	int src_c = src->n - src_s - sa;
	int da = dst->alpha;
	int dst_s = dst->s;
	size_t w = src->w;
	int h = src->h;
	ptrdiff_t d_line_inc = dst->stride - w * dst->n;
	ptrdiff_t s_line_inc = src->stride - w * src->n;

	fz_colorspace *ss = src->colorspace;

	if ((int)w < 0 || h < 0)
		return;

	assert(src->w == dst->w && src->h == dst->h);

	if (d_line_inc == 0 && s_line_inc == 0)
	{
		w *= h;
		h = 1;
	}

	if (src_s != 0 || dst_s != 0)
	{
		fz_warn(ctx, "Spots dropped during pixmap conversion");
	}

	/* Special case for Lab colorspace (scaling of components to float) */
	if (ss->type == FZ_COLORSPACE_LAB)
	{
		if (src_s == 0 && dst_s == 0)
		{
			if (sa)
			{
				if (da)
					convert_lab_sa_da(ctx, src, dst, is, params);
				else
					convert_lab_sa(ctx, src, dst, is, params);
			}
			else
			{
				if (da)
					convert_lab_da(ctx, src, dst, is, params);
				else
					convert_lab(ctx, src, dst, is, params);
			}
		}
		else
		{
			if (sa)
			{
				if (da)
					convert_lab_sa_da_spots(ctx, src, dst, is, params);
				else
					convert_lab_sa_spots(ctx, src, dst, is, params);
			}
			else
			{
				if (da)
					convert_lab_da_spots(ctx, src, dst, is, params);
				else
					convert_lab_spots(ctx, src, dst, is, params);
			}
		}
	}

	/* Brute-force for small images */
	else if (w*h < 256)
	{
		if (src_s == 0 && dst_s == 0)
		{
			if (sa)
			{
				if (da)
					brute_force_sa_da(ctx, src, dst, is, params);
				else
					brute_force_sa(ctx, src, dst, is, params);
			}
			else
			{
				if (da)
					brute_force_da(ctx, src, dst, is, params);
				else
					brute_force(ctx, src, dst, is, params);
			}
		}
		else
		{
			if (sa)
			{
				if (da)
					brute_force_sa_da_spots(ctx, src, dst, is, params);
				else
					brute_force_sa_spots(ctx, src, dst, is, params);
			}
			else
			{
				if (da)
					brute_force_da_spots(ctx, src, dst, is, params);
				else
					brute_force_spots(ctx, src, dst, is, params);
			}
		}
	}

	/* 1-d lookup table for single channel colorspaces */
	else if (src_c == 1)
	{
		lookup_1d(ctx, src, dst, is, params);
	}

	/* Memoize colors using a hash table for the general case */
	else
	{
		if (src_s == 0 && dst_s == 0)
			memoize_nospots(ctx, src, dst, is, params);
		else
			memoize_spots(ctx, src, dst, is, params);
	}
}

void
fz_convert_pixmap_samples(fz_context *ctx, const fz_pixmap *src, fz_pixmap *dst,
	fz_colorspace *prf,
	const fz_default_colorspaces *default_cs,
	fz_color_params params,
	int copy_spots)
{
#if FZ_ENABLE_ICC
	fz_colorspace *ss = src->colorspace;
	fz_colorspace *ds = dst->colorspace;
	fz_pixmap *base_idx = NULL;
	fz_pixmap *base_sep = NULL;
	fz_icc_link *link = NULL;

	fz_var(link);
	fz_var(base_idx);
	fz_var(base_sep);

	if (!ds)
	{
		fz_fast_any_to_alpha(ctx, src, dst, copy_spots);
		return;
	}

	fz_try(ctx)
	{
		/* Treat any alpha-only pixmap as being device gray here. */
		if (!ss)
			ss = fz_device_gray(ctx);

		/* Convert indexed into base colorspace. */
		if (ss->type == FZ_COLORSPACE_INDEXED)
		{
			src = base_idx = fz_convert_indexed_pixmap_to_base(ctx, src);
			ss = src->colorspace;
		}

		/* Convert separation into base colorspace. */
		if (ss->type == FZ_COLORSPACE_SEPARATION)
		{
			src = base_sep = fz_convert_separation_pixmap_to_base(ctx, src);
			ss = src->colorspace;
		}

		/* Adjust colorspaces for being in a softmask */
		if (params.ri & FZ_RI_IN_SOFTMASK)
		{
			if (ss->type == FZ_COLORSPACE_GRAY)
				ss = ctx->colorspace->ps_gray;
			else if (ss->type == FZ_COLORSPACE_RGB)
				ss = ctx->colorspace->ps_rgb;
			else if (ss->type == FZ_COLORSPACE_BGR)
				ss = ctx->colorspace->ps_bgr;
			else if (ss->type == FZ_COLORSPACE_CMYK)
				ss = ctx->colorspace->ps_cmyk;
			if (ds->type == FZ_COLORSPACE_GRAY)
				ds = ctx->colorspace->ps_gray;
			else if (ds->type == FZ_COLORSPACE_RGB)
				ds = ctx->colorspace->ps_rgb;
			else if (ds->type == FZ_COLORSPACE_BGR)
				ds = ctx->colorspace->ps_bgr;
			else if (ds->type == FZ_COLORSPACE_CMYK)
				ds = ctx->colorspace->ps_cmyk;
			params.ri &= ~FZ_RI_IN_SOFTMASK;
		}
		/* Substitute Device colorspace with page Default colorspace: */
		else if (ss->flags & FZ_COLORSPACE_IS_DEVICE)
		{
			switch (ss->type)
			{
			default: break;
			case FZ_COLORSPACE_GRAY: ss = fz_default_gray(ctx, default_cs); break;
			case FZ_COLORSPACE_RGB: ss = fz_default_rgb(ctx, default_cs); break;
			case FZ_COLORSPACE_CMYK: ss = fz_default_cmyk(ctx, default_cs); break;
			}
		}

		if (!ctx->icc_enabled)
		{
			fz_convert_fast_pixmap_samples(ctx, src, dst, copy_spots);
		}

		/* Handle identity case. */
		else if (ss == ds || (!memcmp(ss->u.icc.md5, ds->u.icc.md5, 16)))
		{
			fz_convert_fast_pixmap_samples(ctx, src, dst, copy_spots);
		}

		/* Handle DeviceGray to CMYK as K only. See note in Section 6.3 of PDF spec 1.7. */
		else if ((ss->flags & FZ_COLORSPACE_IS_DEVICE) &&
			(ss->type == FZ_COLORSPACE_GRAY) &&
			(ds->type == FZ_COLORSPACE_CMYK))
		{
			fz_convert_fast_pixmap_samples(ctx, src, dst, copy_spots);
		}

		/* Use slow conversion path for indexed. */
		else if (ss->type == FZ_COLORSPACE_INDEXED)
		{
			fz_convert_slow_pixmap_samples(ctx, src, dst, prf, params, copy_spots);
		}

		/* Use slow conversion path for separation. */
		else if (ss->type == FZ_COLORSPACE_SEPARATION)
		{
			fz_convert_slow_pixmap_samples(ctx, src, dst, prf, params, copy_spots);
		}

		else
		{
			fz_try(ctx)
			{
				int sx = src->s + src->alpha;
				int dx = dst->s + dst->alpha;
				/* If there are no spots to copy, we might as well copy spots! */
				int effectively_copying_spots = copy_spots || (src->s == 0 && dst->s == 0);
				/* If we have alpha, we're preserving spots and we have the same number
				 * of 'extra' (non process, spots+alpha) channels (i.e. sx == dx), then
				 * we get lcms2 to do the premultiplication handling for us. If not,
				 * fz_icc_transform_pixmap will have to do it by steam. */
				int premult = src->alpha && (sx == dx) && effectively_copying_spots;
				link = fz_find_icc_link(ctx, ss, sx, ds, dx, prf, params, 0, effectively_copying_spots, premult);
				fz_icc_transform_pixmap(ctx, link, src, dst, effectively_copying_spots);
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
				fz_report_error(ctx);
				fz_warn(ctx, "falling back to fast color conversion");
				fz_convert_fast_pixmap_samples(ctx, src, dst, copy_spots);
			}
		}
	}
	fz_always(ctx)
	{
		fz_drop_icc_link(ctx, link);
		fz_drop_pixmap(ctx, base_sep);
		fz_drop_pixmap(ctx, base_idx);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
#else
	fz_convert_fast_pixmap_samples(ctx, src, dst, copy_spots);
#endif
}

void fz_colorspace_digest(fz_context *ctx, fz_colorspace *cs, unsigned char digest[16])
{
#if FZ_ENABLE_ICC
	if (!fz_colorspace_is_icc(ctx, cs))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "must have icc profile for colorspace digest");
	memcpy(digest, cs->u.icc.md5, 16);
#else
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "ICC support disabled");
#endif
}
