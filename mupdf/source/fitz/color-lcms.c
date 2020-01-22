#include "mupdf/fitz.h"

#if FZ_ENABLE_ICC

#ifndef LCMS_USE_FLOAT
#define LCMS_USE_FLOAT 0
#endif

#ifdef HAVE_LCMS2MT
#define GLOINIT cmsContext glo = ctx->colorspace->icc_instance;
#define GLO glo,
#include "lcms2mt.h"
#include "lcms2mt_plugin.h"
#else
#define GLOINIT
#define GLO
#include "lcms2.h"
#endif

static void fz_premultiply_row(fz_context *ctx, int n, int c, int w, unsigned char *s)
{
	unsigned char a;
	int k;
	int n1 = n-1;
	for (; w > 0; w--)
	{
		a = s[n1];
		for (k = 0; k < c; k++)
			s[k] = fz_mul255(s[k], a);
		s += n;
	}
}

static void fz_unmultiply_row(fz_context *ctx, int n, int c, int w, unsigned char *s, const unsigned char *in)
{
	int a, inva;
	int k;
	int n1 = n-1;
	for (; w > 0; w--)
	{
		a = in[n1];
		inva = a ? 255 * 256 / a : 0;
		for (k = 0; k < c; k++)
			s[k] = (in[k] * inva) >> 8;
		for (;k < n1; k++)
			s[k] = in[k];
		s[n1] = a;
		s += n;
		in += n;
	}
}

struct fz_icc_link_s
{
	fz_storable storable;
	void *handle;
};

#ifdef HAVE_LCMS2MT

static void fz_lcms_log_error(cmsContext id, cmsUInt32Number error_code, const char *error_text)
{
	fz_context *ctx = (fz_context *)cmsGetContextUserData(id);
	fz_warn(ctx, "lcms: %s.", error_text);
}

static void *fz_lcms_malloc(cmsContext id, unsigned int size)
{
	fz_context *ctx = cmsGetContextUserData(id);
	return Memento_label(fz_malloc_no_throw(ctx, size), "lcms");
}

static void *fz_lcms_realloc(cmsContext id, void *ptr, unsigned int size)
{
	fz_context *ctx = cmsGetContextUserData(id);
	return Memento_label(fz_realloc_no_throw(ctx, ptr, size), "lcms");
}

static void fz_lcms_free(cmsContext id, void *ptr)
{
	fz_context *ctx = cmsGetContextUserData(id);
	fz_free(ctx, ptr);
}

static cmsPluginMemHandler fz_lcms_memhandler =
{
	{
		cmsPluginMagicNumber,
		LCMS_VERSION,
		cmsPluginMemHandlerSig,
		NULL
	},
	fz_lcms_malloc,
	fz_lcms_free,
	fz_lcms_realloc,
	NULL,
	NULL,
	NULL,
};

void fz_new_icc_context(fz_context *ctx)
{
	cmsContext glo = cmsCreateContext(&fz_lcms_memhandler, ctx);
	if (!glo)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cmsCreateContext failed");
	ctx->colorspace->icc_instance = glo;
	cmsSetLogErrorHandler(glo, fz_lcms_log_error);
}

void fz_drop_icc_context(fz_context *ctx)
{
	cmsContext glo = ctx->colorspace->icc_instance;
	if (glo)
		cmsDeleteContext(glo);
	ctx->colorspace->icc_instance = NULL;
}

#else

static fz_context *glo_ctx = NULL;

static void fz_lcms_log_error(cmsContext id, cmsUInt32Number error_code, const char *error_text)
{
	fz_warn(glo_ctx, "lcms: %s.", error_text);
}

void fz_new_icc_context(fz_context *ctx)
{
	if (glo_ctx != NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Stock LCMS2 library cannot be used in multiple contexts!");
	glo_ctx = ctx;
	cmsSetLogErrorHandler(fz_lcms_log_error);
}

void fz_drop_icc_context(fz_context *ctx)
{
	glo_ctx = NULL;
	cmsSetLogErrorHandler(NULL);
}

#endif

fz_icc_profile *fz_new_icc_profile(fz_context *ctx, unsigned char *data, size_t size)
{
	GLOINIT
	fz_icc_profile *profile;
	profile = cmsOpenProfileFromMem(GLO data, (cmsUInt32Number)size);
	if (profile == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cmsOpenProfileFromMem failed");
	return profile;
}

int fz_icc_profile_is_lab(fz_context *ctx, fz_icc_profile *profile)
{
	GLOINIT
	if (profile == NULL)
		return 0;
	return (cmsGetColorSpace(GLO profile) == cmsSigLabData);
}

void fz_drop_icc_profile(fz_context *ctx, fz_icc_profile *profile)
{
	GLOINIT
	if (profile)
		cmsCloseProfile(GLO profile);
}

void fz_icc_profile_name(fz_context *ctx, fz_icc_profile *profile, char *name, size_t size)
{
	GLOINIT
	cmsMLU *descMLU;
	descMLU = cmsReadTag(GLO profile, cmsSigProfileDescriptionTag);
	name[0] = 0;
	cmsMLUgetASCII(GLO descMLU, "en", "US", name, (cmsUInt32Number)size);
}

int fz_icc_profile_components(fz_context *ctx, fz_icc_profile *profile)
{
	GLOINIT
	return cmsChannelsOf(GLO cmsGetColorSpace(GLO profile));
}

void fz_drop_icc_link_imp(fz_context *ctx, fz_storable *storable)
{
	GLOINIT
	fz_icc_link *link = (fz_icc_link*)storable;
	cmsDeleteTransform(GLO link->handle);
	fz_free(ctx, link);
}

void fz_drop_icc_link(fz_context *ctx, fz_icc_link *link)
{
	fz_drop_storable(ctx, &link->storable);
}

fz_icc_link *
fz_new_icc_link(fz_context *ctx,
	fz_colorspace *src, int src_extras,
	fz_colorspace *dst, int dst_extras,
	fz_colorspace *prf,
	fz_color_params rend,
	int format,
	int copy_spots)
{
	GLOINIT
	cmsHPROFILE src_pro = src->u.icc.profile;
	cmsHPROFILE dst_pro = dst->u.icc.profile;
	cmsHPROFILE prf_pro = prf ? prf->u.icc.profile : NULL;
	int src_bgr = (src->type == FZ_COLORSPACE_BGR);
	int dst_bgr = (dst->type == FZ_COLORSPACE_BGR);
	cmsColorSpaceSignature src_cs, dst_cs;
	cmsUInt32Number src_fmt, dst_fmt;
	cmsUInt32Number flags;
	cmsHTRANSFORM transform;
	fz_icc_link *link;

	flags = cmsFLAGS_LOWRESPRECALC;

	src_cs = cmsGetColorSpace(GLO src_pro);
	src_fmt = COLORSPACE_SH(_cmsLCMScolorSpace(GLO src_cs));
	src_fmt |= CHANNELS_SH(cmsChannelsOf(GLO src_cs));
	src_fmt |= DOSWAP_SH(src_bgr);
	src_fmt |= SWAPFIRST_SH(src_bgr && (src_extras > 0));
#if LCMS_USE_FLOAT
	src_fmt |= BYTES_SH(format ? 4 : 1);
	src_fmt |= FLOAT_SH(format ? 1 : 0)
#else
	src_fmt |= BYTES_SH(format ? 2 : 1);
#endif
	src_fmt |= EXTRA_SH(src_extras);

	dst_cs = cmsGetColorSpace(GLO dst_pro);
	dst_fmt = COLORSPACE_SH(_cmsLCMScolorSpace(GLO dst_cs));
	dst_fmt |= CHANNELS_SH(cmsChannelsOf(GLO dst_cs));
	dst_fmt |= DOSWAP_SH(dst_bgr);
	dst_fmt |= SWAPFIRST_SH(dst_bgr && (dst_extras > 0));
#if LCMS_USE_FLOAT
	dst_fmt |= BYTES_SH(format ? 4 : 1);
	dst_fmt |= FLOAT_SH(format ? 1 : 0);
#else
	dst_fmt |= BYTES_SH(format ? 2 : 1);
#endif
	dst_fmt |= EXTRA_SH(dst_extras);

	/* flags */
	if (rend.bp)
		flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;

	if (copy_spots)
		flags |= cmsFLAGS_COPY_ALPHA;

	if (prf_pro == NULL)
	{
		transform = cmsCreateTransform(GLO src_pro, src_fmt, dst_pro, dst_fmt, rend.ri, flags);
		if (!transform)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cmsCreateTransform(%s,%s) failed", src->name, dst->name);
	}

	/* LCMS proof creation links don't work properly with the Ghent test files. Handle this in a brutish manner. */
	else if (src_pro == prf_pro)
	{
		transform = cmsCreateTransform(GLO src_pro, src_fmt, dst_pro, dst_fmt, INTENT_RELATIVE_COLORIMETRIC, flags);
		if (!transform)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cmsCreateTransform(src=proof,dst) failed");
	}
	else if (prf_pro == dst_pro)
	{
		transform = cmsCreateTransform(GLO src_pro, src_fmt, prf_pro, dst_fmt, rend.ri, flags);
		if (!transform)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cmsCreateTransform(src,proof=dst) failed");
	}
	else
	{
		cmsHPROFILE src_to_prf_pro;
		cmsHTRANSFORM src_to_prf_link;
		cmsColorSpaceSignature prf_cs;
		cmsUInt32Number prf_fmt;
		cmsHPROFILE hProfiles[3];

		prf_cs = cmsGetColorSpace(GLO prf_pro);
		prf_fmt = COLORSPACE_SH(_cmsLCMScolorSpace(GLO prf_cs));
		prf_fmt |= CHANNELS_SH(cmsChannelsOf(GLO prf_cs));
#if LCMS_USE_FLOAT
		prf_fmt |= BYTES_SH(format ? 4 : 1);
		prf_fmt |= FLOAT_SH(format ? 1 : 0);
#else
		prf_fmt |= BYTES_SH(format ? 2 : 1);
#endif

		src_to_prf_link = cmsCreateTransform(GLO src_pro, src_fmt, prf_pro, prf_fmt, rend.ri, flags);
		if (!src_to_prf_link)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cmsCreateTransform(src,proof) failed");
		src_to_prf_pro = cmsTransform2DeviceLink(GLO src_to_prf_link, 3.4, flags);
		cmsDeleteTransform(GLO src_to_prf_link);
		if (!src_to_prf_pro)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cmsTransform2DeviceLink(src,proof) failed");

		hProfiles[0] = src_to_prf_pro;
		hProfiles[1] = prf_pro;
		hProfiles[2] = dst_pro;
		transform = cmsCreateMultiprofileTransform(GLO hProfiles, 3, src_fmt, dst_fmt, INTENT_RELATIVE_COLORIMETRIC, flags);
		cmsCloseProfile(GLO src_to_prf_pro);
		if (!transform)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cmsCreateMultiprofileTransform(src,proof,dst) failed");
	}

	fz_try(ctx)
	{
		link = fz_malloc_struct(ctx, fz_icc_link);
		FZ_INIT_STORABLE(link, 1, fz_drop_icc_link_imp);
		link->handle = transform;
	}
	fz_catch(ctx)
	{
		cmsDeleteTransform(GLO link);
		fz_rethrow(ctx);
	}
	return link;
}

void
fz_icc_transform_color(fz_context *ctx, fz_color_converter *cc, const float *src, float *dst)
{
	GLOINIT
#if LCMS_USE_FLOAT
	cmsDoTransform(GLO cc->link->handle, src, dst, 1);
#else
	uint16_t s16[FZ_MAX_COLORS];
	uint16_t d16[FZ_MAX_COLORS];
	int dn = cc->ds->n;
	int i;
	if (cc->ss->type == FZ_COLORSPACE_LAB)
	{
		s16[0] = src[0] * 655.35f;
		s16[1] = (src[1] + 128) * 257;
		s16[2] = (src[2] + 128) * 257;
	}
	else
	{
		int sn = cc->ss->n;
		for (i = 0; i < sn; ++i)
			s16[i] = src[i] * 65535;
	}
	cmsDoTransform(GLO cc->link->handle, s16, d16, 1);
	for (i = 0; i < dn; ++i)
		dst[i] = d16[i] / 65535.0f;
#endif
}

void
fz_icc_transform_pixmap(fz_context *ctx, fz_icc_link *link, fz_pixmap *src, fz_pixmap *dst, int copy_spots)
{
	GLOINIT
	int cmm_num_src, cmm_num_dst, cmm_extras;
	unsigned char *inputpos, *outputpos, *buffer;
	int ss = src->stride;
	int ds = dst->stride;
	int sw = src->w;
	int dw = dst->w;
	int sn = src->n;
	int dn = dst->n;
	int sa = src->alpha;
	int da = dst->alpha;
	int ssp = src->s;
	int dsp = dst->s;
	int sc = sn - ssp - sa;
	int dc = dn - dsp - da;
	int h = src->h;
	cmsUInt32Number src_format, dst_format;

	/* check the channels. */
	src_format = cmsGetTransformInputFormat(GLO link->handle);
	dst_format = cmsGetTransformOutputFormat(GLO link->handle);
	cmm_num_src = T_CHANNELS(src_format);
	cmm_num_dst = T_CHANNELS(dst_format);
	cmm_extras = T_EXTRA(src_format);
	if (cmm_num_src != sc || cmm_num_dst != dc || cmm_extras != ssp+sa || sa != da || (copy_spots && ssp != dsp))
		fz_throw(ctx, FZ_ERROR_GENERIC, "bad setup in ICC pixmap transform: src: %d vs %d+%d+%d, dst: %d vs %d+%d+%d", cmm_num_src, sc, ssp, sa, cmm_num_dst, dc, dsp, da);

	inputpos = src->samples;
	outputpos = dst->samples;
	if (sa)
	{
		buffer = fz_malloc(ctx, ss);
		for (; h > 0; h--)
		{
			fz_unmultiply_row(ctx, sn, sc, sw, buffer, inputpos);
			cmsDoTransform(GLO link->handle, buffer, outputpos, sw);
			fz_premultiply_row(ctx, dn, dc, dw, outputpos);
			inputpos += ss;
			outputpos += ds;
		}
		fz_free(ctx, buffer);
	}
	else
	{
		for (; h > 0; h--)
		{
			cmsDoTransform(GLO link->handle, inputpos, outputpos, sw);
			inputpos += ss;
			outputpos += ds;
		}
	}
}

#endif
