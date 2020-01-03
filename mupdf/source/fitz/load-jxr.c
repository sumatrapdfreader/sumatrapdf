#include "mupdf/fitz.h"

#ifdef HAVE_JPEGXR

#include <math.h>
#include <string.h>

#include <jpegxr.h>

struct info
{
	fz_context *ctx;

	float xres, yres;
	int width, height;
	int format;
	int has_alpha;
	int has_premul;

	int comps, stride;
	unsigned char *samples;
	fz_colorspace *cspace;
};

static const char *
jxr_error_string(int rc)
{
	switch (rc)
	{
	case JXR_EC_OK: return "No error";
	default:
	case JXR_EC_ERROR: return "Unspecified error";
	case JXR_EC_BADMAGIC: return "Stream lacks proper magic number";
	case JXR_EC_FEATURE_NOT_IMPLEMENTED: return "Feature not implemented";
	case JXR_EC_IO: return "Error reading/writing data";
	case JXR_EC_BADFORMAT: return "Bad file format";
	}
}

struct {
	jxrc_t_pixelFormat format;
	int comps;
} pixelformats[] = {
	{JXRC_FMT_BlackWhite, 1},
	{JXRC_FMT_8bppGray, 1},
	{JXRC_FMT_16bppGray, 1},
	{JXRC_FMT_16bppGrayFixedPoint, 1},
	{JXRC_FMT_16bppGrayHalf, 1},
	{JXRC_FMT_32bppGrayFixedPoint, 1},
	{JXRC_FMT_32bppGrayFloat, 1},
	{JXRC_FMT_16bppBGR555, 3},
	{JXRC_FMT_16bppBGR565, 3},
	{JXRC_FMT_24bppBGR, 3},
	{JXRC_FMT_24bppRGB, 3},
	{JXRC_FMT_32bppBGR101010, 3},
	{JXRC_FMT_32bppBGRA, 3},
	{JXRC_FMT_32bppBGR, 3},
	{JXRC_FMT_32bppPBGRA, 3},
	{JXRC_FMT_48bppRGBFixedPoint, 3},
	{JXRC_FMT_48bppRGBHalf, 3},
	{JXRC_FMT_48bppRGB, 3},
	{JXRC_FMT_64bppPRGBA, 3},
	{JXRC_FMT_64bppRGBAFixedPoint, 3},
	{JXRC_FMT_64bppRGBAHalf, 3},
	{JXRC_FMT_64bppRGBA, 3},
	{JXRC_FMT_64bppRGBFixedPoint, 3},
	{JXRC_FMT_64bppRGBHalf, 3},
	{JXRC_FMT_96bppRGBFixedPoint, 3},
	{JXRC_FMT_128bppPRGBAFloat, 3},
	{JXRC_FMT_128bppRGBAFixedPoint, 3},
	{JXRC_FMT_128bppRGBAFloat, 3},
	{JXRC_FMT_128bppRGBFixedPoint, 3},
	{JXRC_FMT_128bppRGBFloat, 3},
	{JXRC_FMT_32bppRGBE, 3},
	{JXRC_FMT_32bppCMYK, 4},
	{JXRC_FMT_40bppCMYKAlpha, 4},
	{JXRC_FMT_64bppCMYK, 4},
	{JXRC_FMT_80bppCMYKAlpha, 4},
	{JXRC_FMT_24bpp3Channels, 3},
	{JXRC_FMT_32bpp3ChannelsAlpha, 3},
	{JXRC_FMT_32bpp4Channels, 4},
	{JXRC_FMT_40bpp4ChannelsAlpha, 4},
	{JXRC_FMT_40bpp5Channels, 5},
	{JXRC_FMT_48bpp3Channels, 3},
	{JXRC_FMT_48bpp5ChannelsAlpha, 5},
	{JXRC_FMT_48bpp6Channels, 6},
	{JXRC_FMT_56bpp6ChannelsAlpha, 6},
	{JXRC_FMT_56bpp7Channels, 7},
	{JXRC_FMT_64bpp3ChannelsAlpha, 3},
	{JXRC_FMT_64bpp4Channels, 4},
	{JXRC_FMT_64bpp7ChannelsAlpha, 7},
	{JXRC_FMT_64bpp8Channels, 8},
	{JXRC_FMT_72bpp8ChannelsAlpha, 8},
	{JXRC_FMT_80bpp4ChannelsAlpha, 4},
	{JXRC_FMT_80bpp5Channels, 5},
	{JXRC_FMT_96bpp5ChannelsAlpha, 5},
	{JXRC_FMT_96bpp6Channels, 6},
	{JXRC_FMT_112bpp6ChannelsAlpha, 6},
	{JXRC_FMT_112bpp7Channels, 7},
	{JXRC_FMT_128bpp7ChannelsAlpha, 7},
	{JXRC_FMT_128bpp8Channels, 8},
	{JXRC_FMT_144bpp8ChannelsAlpha, 8},
};

static inline float
float32_from_int32_bits(int v)
{
	return *((float*) &v);
}

static inline float
float32_from_float16(int v)
{
	int s = (v >> 15) & 0x1;
	int e = (v >> 10) & 0x1f;
	int m = (v >> 0) & 0x3ff;
	int i = (s << 31) | ((e - 15 + 127) << 23) | (m << 13);
	return float32_from_int32_bits(i);
}

static inline float
sRGB_from_scRGB(float v)
{
	if (v <= 0.0031308f)
		return v * 12.92f;
	return 1.055f * powf(v, 1.0f / 2.4f) - 0.055f;
}

static inline void
jxr_unpack_sample(fz_context *ctx, struct info *info, jxr_image_t image, int *sp, unsigned char *dp)
{
	int k, bpc, comps, alpha;
	float v;

	if (info->format == JXRC_FMT_32bppRGBE)
	{
		dp[0] = sRGB_from_scRGB(ldexpf(sp[0], sp[3] - 128 - 8)) * 255 + 0.5f;
		dp[1] = sRGB_from_scRGB(ldexpf(sp[1], sp[3] - 128 - 8)) * 255 + 0.5f;
		dp[2] = sRGB_from_scRGB(ldexpf(sp[2], sp[3] - 128 - 8)) * 255 + 0.5f;
		return;
	}
	if (info->format == JXRC_FMT_16bppBGR565)
	{
		dp[0] = sp[0] << 3;
		dp[1] = sp[1] << 2;
		dp[2] = sp[2] << 3;
		return;
	}

	comps = fz_mini(fz_colorspace_n(ctx, info->cspace), jxr_get_IMAGE_CHANNELS(image));
	alpha = jxr_get_ALPHACHANNEL_FLAG(image);
	bpc = jxr_get_CONTAINER_BPC(image);

	for (k = 0; k < comps + alpha; k++)
	{
		switch (bpc)
		{
		default: fz_throw(ctx, FZ_ERROR_GENERIC, "unknown sample type: %d", bpc);
		case JXR_BD1WHITE1: dp[k] = sp[k] ? 255 : 0; break;
		case JXR_BD1BLACK1: dp[k] = sp[k] ? 0 : 255; break;
		case JXR_BD5: dp[k] = sp[k] << 3; break;
		case JXR_BD8: dp[k] = sp[k]; break;
		case JXR_BD10: dp[k] = sp[k] >> 2; break;
		case JXR_BD16: dp[k] = sp[k] >> 8; break;

		case JXR_BD16S:
			v = sp[k] * (1.0f / (1 << 13));
			goto decode_float32;
		case JXR_BD32S:
			v = sp[k] * (1.0f / (1 << 24));
			goto decode_float32;
		case JXR_BD16F:
			v = float32_from_float16(sp[k]);
			goto decode_float32;
		case JXR_BD32F:
			v = float32_from_int32_bits(sp[k]);
			goto decode_float32;
		decode_float32:
			if (k < comps)
				dp[k] = sRGB_from_scRGB(fz_clamp(v, 0, 1)) * 255 + 0.5f;
			else
				dp[k] = fz_clamp(v, 0, 1) * 255 + 0.5f;
			break;
		}
	}
}

static inline void
jxr_unpack_alpha_sample(fz_context *ctx, struct info *info, jxr_image_t image, int *sp, unsigned char *dp)
{
	int bpc = jxr_get_CONTAINER_BPC(image);
	switch (bpc)
	{
	default: fz_throw(ctx, FZ_ERROR_GENERIC, "unknown alpha sample type: %d", bpc);
	case JXR_BD8: dp[0] = sp[0]; break;
	case JXR_BD10: dp[0] = sp[0] >> 2; break;
	case JXR_BD16: dp[0] = sp[0] >> 8; break;

	case JXR_BD16S:
		dp[0] = fz_clamp(sp[0] * (1.0f / (1 << 13)), 0, 1) * 255 + 0.5f;
		break;
	case JXR_BD32S:
		dp[0] = fz_clamp(sp[0] * (1.0f / (1 << 24)), 0, 1) * 255 + 0.5f;
		break;
	case JXR_BD16F:
		dp[0] = fz_clamp(float32_from_float16(sp[0]), 0, 1) * 255 + 0.5f;
		break;
	case JXR_BD32F:
		dp[0] = fz_clamp(float32_from_int32_bits(sp[0]), 0, 1) * 255 + 0.5f;
		break;
	}
}

static void
jxr_decode_block(jxr_image_t image, int mx, int my, int *data)
{
	struct info *info = jxr_get_user_data(image);
	fz_context *ctx = info->ctx;
	unsigned char *p;
	int x, y, n1;

	mx *= 16;
	my *= 16;

	n1 = fz_colorspace_n(ctx, info->cspace) + 1;
	for (y = 0; y < 16; y++)
	{
		if ((my + y) >= info->height)
			return;

		p = info->samples + (my + y) * info->stride + mx * n1;

		for (x = 0; x < 16; x++)
		{
			if ((mx + x) < info->width)
			{
				jxr_unpack_sample(ctx, info, image, data, p);
				p += n1;
			}

			data += jxr_get_IMAGE_CHANNELS(image) + jxr_get_ALPHACHANNEL_FLAG(image);
			data += (info->format == JXRC_FMT_32bppRGBE ? 1 : 0);
		}
	}
}

static void
jxr_decode_block_alpha(jxr_image_t image, int mx, int my, int *data)
{
	struct info *info = jxr_get_user_data(image);
	fz_context *ctx = info->ctx;
	unsigned char *p;
	int x, y, n;

	mx *= 16;
	my *= 16;

	n = fz_colorspace_n(ctx, info->cspace);
	for (y = 0; y < 16; y++)
	{
		if ((my + y) >= info->height)
			return;

		p = info->samples + (my + y) * info->stride + mx * (n + 1);

		for (x = 0; x < 16; x++)
		{
			if ((mx + x) < info->width)
			{
				jxr_unpack_alpha_sample(ctx, info, image, data, p + n);
				p += n + 1;
			}

			data++;
		}
	}
}

static void
jxr_read_image(fz_context *ctx, const unsigned char *data, int size, struct info *info, int only_metadata)
{
	jxr_container_t container;
	jxr_image_t image = NULL;
	jxr_image_t alpha = NULL;
	int rc, i;

	fz_var(image);
	fz_var(alpha);

	fz_try(ctx)
	{
		container = jxr_create_container();

		rc = jxr_read_image_container_memory(container, (unsigned char *)data, size);
		if (rc < 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read jxr image container: %s", jxr_error_string(rc));

		info->xres = jxrc_width_resolution(container, 0);
		info->yres = jxrc_height_resolution(container, 0);
		info->width = jxrc_image_width(container, 0);
		info->height = jxrc_image_height(container, 0);

		info->format = jxrc_image_pixelformat(container, 0);

		for (i = 0; i < nelem(pixelformats); i++)
			if (pixelformats[i].format == info->format)
			{
				info->comps = pixelformats[i].comps;
				break;
			}
		if (i == nelem(pixelformats))
			fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported pixel format: %u", info->format);

		if (info->comps == 1)
			info->cspace = fz_device_gray(ctx);
		else if (info->comps == 3)
			info->cspace = fz_device_rgb(ctx);
		else if (info->comps >= 4)
			info->cspace = fz_device_cmyk(ctx);

		info->stride = info->width * (fz_colorspace_n(ctx, info->cspace) + 1);

		if (!only_metadata)
		{
			unsigned long image_offset;
			unsigned char image_band;
			unsigned long alpha_offset;
			unsigned char alpha_band;

			info->ctx = ctx;
			info->samples = Memento_label(fz_malloc(ctx, info->stride * info->height), "jxr_samples");
			memset(info->samples, 0xff, info->stride * info->height);

			image_offset = jxrc_image_offset(container, 0);
			image_band = jxrc_image_band_presence(container, 0);
			alpha_offset = jxrc_alpha_offset(container, 0);
			alpha_band = jxrc_alpha_band_presence(container, 0);

			image = jxr_create_input();

			jxr_set_PROFILE_IDC(image, 111);
			jxr_set_LEVEL_IDC(image, 255);
			jxr_set_pixel_format(image, info->format);
			jxr_set_container_parameters(image, info->format,
				info->width, info->height, alpha_offset,
				image_band, alpha_band, 0);

			jxr_set_user_data(image, info);
			jxr_set_block_output(image, jxr_decode_block);

			rc = jxr_read_image_bitstream_memory(image, (unsigned char *)data + image_offset, size - image_offset);
			if (rc < 0)
				fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read jxr image: %s", jxr_error_string(rc));

			if (info->format == JXRC_FMT_32bppPBGRA ||
					info->format == JXRC_FMT_64bppPRGBA ||
					info->format == JXRC_FMT_128bppPRGBAFloat)
				info->has_premul = 1;

			if (jxr_get_ALPHACHANNEL_FLAG(image))
				info->has_alpha = 1;

			if (alpha_offset > 0)
			{
				info->has_alpha = 1;

				alpha = jxr_create_input();

				jxr_set_PROFILE_IDC(alpha, 111);
				jxr_set_LEVEL_IDC(alpha, 255);
				jxr_set_pixel_format(alpha, info->format);
				jxr_set_container_parameters(alpha, info->format,
					info->width, info->height, alpha_offset,
					image_band, alpha_band, 1);

				jxr_set_user_data(alpha, info);
				jxr_set_block_output(alpha, jxr_decode_block_alpha);

				rc = jxr_read_image_bitstream_memory(alpha, (unsigned char *)data + alpha_offset, size - alpha_offset);
				if (rc < 0)
					fz_throw(ctx, FZ_ERROR_GENERIC, "cannot read jxr image: %s", jxr_error_string(rc));
			}
		}
	}
	fz_always(ctx)
	{
		if (alpha)
			jxr_destroy(alpha);
		if (image)
			jxr_destroy(image);
		jxr_destroy_container(container);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

fz_pixmap *
fz_load_jxr(fz_context *ctx, const unsigned char *data, size_t size)
{
	struct info info = { 0 };
	fz_pixmap *image = NULL;

	fz_var(image);

	fz_try(ctx)
	{
		jxr_read_image(ctx, data, size, &info, 0);

		image = fz_new_pixmap(ctx, info.cspace, info.width, info.height, NULL, 1);

		image->xres = info.xres;
		image->yres = info.yres;

		fz_unpack_tile(ctx, image, info.samples, fz_colorspace_n(ctx, info.cspace) + 1, 8, info.stride, 0);
		if (info.has_alpha && !info.has_premul)
			fz_premultiply_pixmap(ctx, image);
	}
	fz_always(ctx)
	{
		fz_free(ctx, info.samples);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, image);
		fz_rethrow(ctx);
	}

	return image;
}

void
fz_load_jxr_info(fz_context *ctx, const unsigned char *data, size_t size, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	struct info info = { 0 };

	jxr_read_image(ctx, data, size, &info, 1);
	*cspacep = fz_keep_colorspace(ctx, info.cspace); /* info.cspace is a borrowed device colorspace */
	*wp = info.width;
	*hp = info.height;
	*xresp = info.xres;
	*yresp = info.yres;
}
#else /* HAVE_JPEGXR */

fz_pixmap *
fz_load_jxr(fz_context *ctx, const unsigned char *data, size_t size)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "JPEG-XR codec is not available");
}

void
fz_load_jxr_info(fz_context *ctx, const unsigned char *data, size_t size, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_throw(ctx, FZ_ERROR_GENERIC, "JPEG-XR codec is not available");
}

#endif /* HAVE_JPEGXR */
