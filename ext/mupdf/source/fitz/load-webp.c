// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// Based on Artifex bug 697749 / the koreader webp-upstream-697749 patch.
// Decode uses libwebp (HAVE_WEBP). EXIF/ICC via demux are omitted: we do
// not build libwebp's demux library.

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
