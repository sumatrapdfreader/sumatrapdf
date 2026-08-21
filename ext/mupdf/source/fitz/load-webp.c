// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// Based on Artifex bug 697749 / the koreader webp-upstream-697749 patch.
// Decode uses libwebp (HAVE_WEBP). ICCP is applied via our own RIFF walk,
// not libwebp demux (we do not build that library).

#include "mupdf/fitz.h"

#ifdef HAVE_WEBP

#include "pixmap-imp.h"

#include <string.h>

#include <webp/decode.h>

struct info
{
	int width, height;
	int xres, yres;
	fz_colorspace *cs;
};

// payload of the first RIFF chunk whose FourCC is fourcc; 0 if missing
static int
webp_find_chunk(const unsigned char *p, size_t total, const char fourcc[4], const unsigned char **out, size_t *out_len)
{
	size_t idx = 12;
	if (total < 12 || memcmp(p, "RIFF", 4) || memcmp(p + 8, "WEBP", 4))
		return 0;
	while (idx + 8 <= total)
	{
		size_t size = (size_t)p[idx + 4] | ((size_t)p[idx + 5] << 8) |
			((size_t)p[idx + 6] << 16) | ((size_t)p[idx + 7] << 24);
		size_t payload = idx + 8;
		if (payload + size > total)
			return 0;
		if (memcmp(p + idx, fourcc, 4) == 0)
		{
			*out = p + payload;
			*out_len = size;
			return 1;
		}
		idx = payload + size + (size & 1);
	}
	return 0;
}

static fz_colorspace *
webp_icc_colorspace(fz_context *ctx, const unsigned char *p, size_t total, fz_colorspace *colorspace)
{
#if FZ_ENABLE_ICC
	const unsigned char *icc = NULL;
	size_t icc_len = 0;
	fz_buffer *buf = NULL;
	fz_colorspace *cs = NULL;

	if (!webp_find_chunk(p, total, "ICCP", &icc, &icc_len) || icc_len < 128)
		return colorspace;

	fz_var(buf);
	fz_try(ctx)
	{
		buf = fz_new_buffer_from_copied_data(ctx, icc, icc_len);
		cs = fz_new_icc_colorspace(ctx, fz_colorspace_type(ctx, colorspace), 0, NULL, buf);
		fz_drop_colorspace(ctx, colorspace);
		colorspace = cs;
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
	{
		fz_rethrow_if(ctx, FZ_ERROR_SYSTEM);
		fz_report_error(ctx);
		fz_warn(ctx, "ignoring embedded ICC profile in WebP");
	}
	return colorspace;
#else
	(void)p;
	(void)total;
	return colorspace;
#endif
}

static fz_pixmap *
webp_read_image(fz_context *ctx, struct info *info, const unsigned char *p, size_t total, int only_metadata)
{
	fz_pixmap *image = NULL;
	WebPBitstreamFeatures features;

	memset(info, 0, sizeof(*info));

	if (WebPGetFeatures(p, total, &features) != VP8_STATUS_OK)
		fz_throw(ctx, FZ_ERROR_FORMAT, "unable to extract webp features");

	info->width = features.width;
	info->height = features.height;
	info->xres = 72;
	info->yres = 72;
	info->cs = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	info->cs = webp_icc_colorspace(ctx, p, total, info->cs);

	if (only_metadata)
		return NULL;

	fz_var(image);

	fz_try(ctx)
	{
		int alpha = features.has_alpha;
		size_t out_size;
		image = fz_new_pixmap(ctx, info->cs, info->width, info->height, NULL, alpha);
		fz_set_pixmap_resolution(ctx, image, info->xres, info->yres);
		out_size = (size_t)image->stride * (size_t)image->h;
		if (alpha)
		{
			if (!WebPDecodeRGBAInto(p, total, image->samples, out_size, (int)image->stride))
				fz_throw(ctx, FZ_ERROR_FORMAT, "failed decoding webp image");
			fz_premultiply_pixmap(ctx, image);
		}
		else
		{
			if (!WebPDecodeRGBInto(p, total, image->samples, out_size, (int)image->stride))
				fz_throw(ctx, FZ_ERROR_FORMAT, "failed decoding webp image");
		}
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, image);
		fz_rethrow(ctx);
	}

	return image;
}

fz_pixmap *
fz_load_webp(fz_context *ctx, const unsigned char *p, size_t total)
{
	fz_pixmap *image = NULL;
	struct info info;

	memset(&info, 0, sizeof info);
	fz_var(image);

	fz_try(ctx)
	{
		image = webp_read_image(ctx, &info, p, total, 0);
	}
	fz_always(ctx)
		fz_drop_colorspace(ctx, info.cs);
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, image);
		fz_rethrow(ctx);
	}

	return image;
}

void
fz_load_webp_info(fz_context *ctx, const unsigned char *p, size_t total, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	struct info info;

	memset(&info, 0, sizeof info);

	fz_try(ctx)
		webp_read_image(ctx, &info, p, total, 1);
	fz_catch(ctx)
	{
		fz_drop_colorspace(ctx, info.cs);
		fz_rethrow(ctx);
	}

	*cspacep = info.cs;
	*wp = info.width;
	*hp = info.height;
	*xresp = info.xres;
	*yresp = info.yres;
}

#else /* HAVE_WEBP */

fz_pixmap *
fz_load_webp(fz_context *ctx, const unsigned char *p, size_t total)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "WebP codec is not available");
}

void
fz_load_webp_info(fz_context *ctx, const unsigned char *p, size_t total, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "WebP codec is not available");
}

#endif /* HAVE_WEBP */
