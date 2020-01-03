#include "fitz-imp.h"

#include <string.h>
#include <limits.h>

struct info
{
	int gif89a;
	unsigned int width, height;
	unsigned char aspect;
	unsigned int xres, yres;

	unsigned int image_left, image_top;
	unsigned int image_width, image_height;
	unsigned int image_interlaced;

	int has_gct;
	int gct_entries;
	unsigned char *gct;
	unsigned int gct_background;

	int has_lct;
	int lct_entries;
	unsigned char *lct;

	int has_transparency;
	unsigned int transparent;
	unsigned char *mask;

	fz_pixmap *pix;
};

/* default color table, where the first two entries are black and white */
static const unsigned char dct[256 * 3] = {
	0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02,
	0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x06, 0x06, 0x06,
	0x07, 0x07, 0x07, 0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x0a, 0x0a, 0x0a,
	0x0b, 0x0b, 0x0b, 0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0e, 0x0e, 0x0e,
	0x0f, 0x0f, 0x0f, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x12, 0x12, 0x12,
	0x13, 0x13, 0x13, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x16, 0x16, 0x16,
	0x17, 0x17, 0x17, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x1a, 0x1a, 0x1a,
	0x1b, 0x1b, 0x1b, 0x1c, 0x1c, 0x1c, 0x1d, 0x1d, 0x1d, 0x1e, 0x1e, 0x1e,
	0x1f, 0x1f, 0x1f, 0x20, 0x20, 0x20, 0x21, 0x21, 0x21, 0x22, 0x22, 0x22,
	0x23, 0x23, 0x23, 0x24, 0x24, 0x24, 0x25, 0x25, 0x25, 0x26, 0x26, 0x26,
	0x27, 0x27, 0x27, 0x28, 0x28, 0x28, 0x29, 0x29, 0x29, 0x2a, 0x2a, 0x2a,
	0x2b, 0x2b, 0x2b, 0x2c, 0x2c, 0x2c, 0x2d, 0x2d, 0x2d, 0x2e, 0x2e, 0x2e,
	0x2f, 0x2f, 0x2f, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32,
	0x33, 0x33, 0x33, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35, 0x36, 0x36, 0x36,
	0x37, 0x37, 0x37, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39, 0x3a, 0x3a, 0x3a,
	0x3b, 0x3b, 0x3b, 0x3c, 0x3c, 0x3c, 0x3d, 0x3d, 0x3d, 0x3e, 0x3e, 0x3e,
	0x3f, 0x3f, 0x3f, 0x40, 0x40, 0x40, 0x41, 0x41, 0x41, 0x42, 0x42, 0x42,
	0x43, 0x43, 0x43, 0x44, 0x44, 0x44, 0x45, 0x45, 0x45, 0x46, 0x46, 0x46,
	0x47, 0x47, 0x47, 0x48, 0x48, 0x48, 0x49, 0x49, 0x49, 0x4a, 0x4a, 0x4a,
	0x4b, 0x4b, 0x4b, 0x4c, 0x4c, 0x4c, 0x4d, 0x4d, 0x4d, 0x4e, 0x4e, 0x4e,
	0x4f, 0x4f, 0x4f, 0x50, 0x50, 0x50, 0x51, 0x51, 0x51, 0x52, 0x52, 0x52,
	0x53, 0x53, 0x53, 0x54, 0x54, 0x54, 0x55, 0x55, 0x55, 0x56, 0x56, 0x56,
	0x57, 0x57, 0x57, 0x58, 0x58, 0x58, 0x59, 0x59, 0x59, 0x5a, 0x5a, 0x5a,
	0x5b, 0x5b, 0x5b, 0x5c, 0x5c, 0x5c, 0x5d, 0x5d, 0x5d, 0x5e, 0x5e, 0x5e,
	0x5f, 0x5f, 0x5f, 0x60, 0x60, 0x60, 0x61, 0x61, 0x61, 0x62, 0x62, 0x62,
	0x63, 0x63, 0x63, 0x64, 0x64, 0x64, 0x65, 0x65, 0x65, 0x66, 0x66, 0x66,
	0x67, 0x67, 0x67, 0x68, 0x68, 0x68, 0x69, 0x69, 0x69, 0x6a, 0x6a, 0x6a,
	0x6b, 0x6b, 0x6b, 0x6c, 0x6c, 0x6c, 0x6d, 0x6d, 0x6d, 0x6e, 0x6e, 0x6e,
	0x6f, 0x6f, 0x6f, 0x70, 0x70, 0x70, 0x71, 0x71, 0x71, 0x72, 0x72, 0x72,
	0x73, 0x73, 0x73, 0x74, 0x74, 0x74, 0x75, 0x75, 0x75, 0x76, 0x76, 0x76,
	0x77, 0x77, 0x77, 0x78, 0x78, 0x78, 0x79, 0x79, 0x79, 0x7a, 0x7a, 0x7a,
	0x7b, 0x7b, 0x7b, 0x7c, 0x7c, 0x7c, 0x7d, 0x7d, 0x7d, 0x7e, 0x7e, 0x7e,
	0x7f, 0x7f, 0x7f, 0x80, 0x80, 0x80, 0x81, 0x81, 0x81, 0x82, 0x82, 0x82,
	0x83, 0x83, 0x83, 0x84, 0x84, 0x84, 0x85, 0x85, 0x85, 0x86, 0x86, 0x86,
	0x87, 0x87, 0x87, 0x88, 0x88, 0x88, 0x89, 0x89, 0x89, 0x8a, 0x8a, 0x8a,
	0x8b, 0x8b, 0x8b, 0x8c, 0x8c, 0x8c, 0x8d, 0x8d, 0x8d, 0x8e, 0x8e, 0x8e,
	0x8f, 0x8f, 0x8f, 0x90, 0x90, 0x90, 0x91, 0x91, 0x91, 0x92, 0x92, 0x92,
	0x93, 0x93, 0x93, 0x94, 0x94, 0x94, 0x95, 0x95, 0x95, 0x96, 0x96, 0x96,
	0x97, 0x97, 0x97, 0x98, 0x98, 0x98, 0x99, 0x99, 0x99, 0x9a, 0x9a, 0x9a,
	0x9b, 0x9b, 0x9b, 0x9c, 0x9c, 0x9c, 0x9d, 0x9d, 0x9d, 0x9e, 0x9e, 0x9e,
	0x9f, 0x9f, 0x9f, 0xa0, 0xa0, 0xa0, 0xa1, 0xa1, 0xa1, 0xa2, 0xa2, 0xa2,
	0xa3, 0xa3, 0xa3, 0xa4, 0xa4, 0xa4, 0xa5, 0xa5, 0xa5, 0xa6, 0xa6, 0xa6,
	0xa7, 0xa7, 0xa7, 0xa8, 0xa8, 0xa8, 0xa9, 0xa9, 0xa9, 0xaa, 0xaa, 0xaa,
	0xab, 0xab, 0xab, 0xac, 0xac, 0xac, 0xad, 0xad, 0xad, 0xae, 0xae, 0xae,
	0xaf, 0xaf, 0xaf, 0xb0, 0xb0, 0xb0, 0xb1, 0xb1, 0xb1, 0xb2, 0xb2, 0xb2,
	0xb3, 0xb3, 0xb3, 0xb4, 0xb4, 0xb4, 0xb5, 0xb5, 0xb5, 0xb6, 0xb6, 0xb6,
	0xb7, 0xb7, 0xb7, 0xb8, 0xb8, 0xb8, 0xb9, 0xb9, 0xb9, 0xba, 0xba, 0xba,
	0xbb, 0xbb, 0xbb, 0xbc, 0xbc, 0xbc, 0xbd, 0xbd, 0xbd, 0xbe, 0xbe, 0xbe,
	0xbf, 0xbf, 0xbf, 0xc0, 0xc0, 0xc0, 0xc1, 0xc1, 0xc1, 0xc2, 0xc2, 0xc2,
	0xc3, 0xc3, 0xc3, 0xc4, 0xc4, 0xc4, 0xc5, 0xc5, 0xc5, 0xc6, 0xc6, 0xc6,
	0xc7, 0xc7, 0xc7, 0xc8, 0xc8, 0xc8, 0xc9, 0xc9, 0xc9, 0xca, 0xca, 0xca,
	0xcb, 0xcb, 0xcb, 0xcc, 0xcc, 0xcc, 0xcd, 0xcd, 0xcd, 0xce, 0xce, 0xce,
	0xcf, 0xcf, 0xcf, 0xd0, 0xd0, 0xd0, 0xd1, 0xd1, 0xd1, 0xd2, 0xd2, 0xd2,
	0xd3, 0xd3, 0xd3, 0xd4, 0xd4, 0xd4, 0xd5, 0xd5, 0xd5, 0xd6, 0xd6, 0xd6,
	0xd7, 0xd7, 0xd7, 0xd8, 0xd8, 0xd8, 0xd9, 0xd9, 0xd9, 0xda, 0xda, 0xda,
	0xdb, 0xdb, 0xdb, 0xdc, 0xdc, 0xdc, 0xdd, 0xdd, 0xdd, 0xde, 0xde, 0xde,
	0xdf, 0xdf, 0xdf, 0xe0, 0xe0, 0xe0, 0xe1, 0xe1, 0xe1, 0xe2, 0xe2, 0xe2,
	0xe3, 0xe3, 0xe3, 0xe4, 0xe4, 0xe4, 0xe5, 0xe5, 0xe5, 0xe6, 0xe6, 0xe6,
	0xe7, 0xe7, 0xe7, 0xe8, 0xe8, 0xe8, 0xe9, 0xe9, 0xe9, 0xea, 0xea, 0xea,
	0xeb, 0xeb, 0xeb, 0xec, 0xec, 0xec, 0xed, 0xed, 0xed, 0xee, 0xee, 0xee,
	0xef, 0xef, 0xef, 0xf0, 0xf0, 0xf0, 0xf1, 0xf1, 0xf1, 0xf2, 0xf2, 0xf2,
	0xf3, 0xf3, 0xf3, 0xf4, 0xf4, 0xf4, 0xf5, 0xf5, 0xf5, 0xf6, 0xf6, 0xf6,
	0xf7, 0xf7, 0xf7, 0xf8, 0xf8, 0xf8, 0xf9, 0xf9, 0xf9, 0xfa, 0xfa, 0xfa,
	0xfb, 0xfb, 0xfb, 0xfc, 0xfc, 0xfc, 0xfd, 0xfd, 0xfd, 0xfe, 0xfe, 0xfe,
};

static const unsigned char *
gif_read_subblocks(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end, fz_buffer *buf)
{
	int len;

	do
	{
		if (end - p < 1)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in data subblocks in gif image");
		len = *p;
		p += 1;

		if (len > 0)
		{
			if (end - p < len)
				fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in data subblock in gif image");
			if (buf)
				fz_append_data(ctx, buf, p, len);
			p += len;
		}
	} while (len > 0);

	return p;
}

static const unsigned char *
gif_read_header(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	if (end - p < 6)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in header in gif image");

	if (memcmp(&p[0], "GIF", 3))
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid signature in gif image");
	if (memcmp(&p[3], "87a", 3) && memcmp(&p[3], "89a", 3))
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported version in gif image");

	info->gif89a = !memcmp(p, "GIF89a", 6);

	return p + 6;
}

static const unsigned char *
gif_read_lsd(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	if (end - p < 7)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in logical screen descriptor in gif image");

	info->width = p[1] << 8 | p[0];
	info->height = p[3] << 8 | p[2];
	if (info->width <= 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image width must be > 0");
	if (info->height <= 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image height must be > 0");
	if (info->height > UINT_MAX / info->width / 3 /* components */)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image dimensions might overflow");

	info->has_gct = (p[4] >> 7) & 0x1;
	if (info->has_gct)
	{
		info->gct_entries = 1 << ((p[4] & 0x7) + 1);
		info->gct_background = fz_clampi(p[5], 0, info->gct_entries - 1);
	}
	info->aspect = p[6];

	info->xres = 96;
	info->yres= 96;
	if (info->aspect > 0)
		info->yres = (((float) info->aspect + 15) / 64) * 96;

	return p + 7;
}

static const unsigned char *
gif_read_gct(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	if (end - p < info->gct_entries * 3)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in global color table in gif image");

	info->gct = Memento_label(fz_malloc(ctx, info->gct_entries * 3), "gif_gct");
	memmove(info->gct, p, info->gct_entries * 3);

	return p + info->gct_entries * 3;
}

static const unsigned char *
gif_read_id(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	if (end - p < 10)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in image descriptor in gif image");

	info->image_left = p[2] << 8 | p[1];
	info->image_top = p[4] << 8 | p[3];
	info->image_width = p[6] << 8 | p[5];
	info->image_height = p[8] << 8 | p[7];
	info->has_lct = (p[9] >> 7) & 0x1;
	info->image_interlaced = (p[9] >> 6) & 0x1;

	if (info->has_lct)
		info->lct_entries = 1 << ((p[9] & 0x7) + 1);

	return p + 10;
}

static const unsigned char *
gif_read_lct(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	if (end - p < info->lct_entries * 3)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in local color table in gif image");

	info->lct = Memento_label(fz_malloc(ctx, info->lct_entries * 3), "gif_lct");
	memmove(info->lct, p, info->lct_entries * 3);

	return p + info->lct_entries * 3;
}

static void
gif_read_line(fz_context *ctx, struct info *info, int ct_entries, const unsigned char *ct, unsigned int y, unsigned char *sp)
{
	unsigned int index = (info->image_top + y) * info->width + info->image_left;
	unsigned char *samples = fz_pixmap_samples(ctx, info->pix);
	unsigned char *dp = &samples[index * 4];
	unsigned char *mp = &info->mask[index];
	unsigned int x, k;

	if (info->image_top + y >= info->height)
		return;

	for (x = 0; x < info->image_width && info->image_left + x < info->width; x++, sp++, mp++, dp += 4)
		if (!info->has_transparency || *sp != info->transparent)
		{
			*mp = 0x02;
			for (k = 0; k < 3; k++)
				dp[k] = ct[fz_clampi(*sp, 0, ct_entries - 1) * 3 + k];
			dp[3] = 255;
		}
		else if (*mp == 0x01)
			*mp = 0x00;
}

static const unsigned char *
gif_read_tbid(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	fz_stream *stm = NULL, *lzwstm = NULL;
	unsigned int mincodesize, y;
	fz_buffer *compressed = NULL, *uncompressed = NULL;
	const unsigned char *ct;
	unsigned char *sp;
	int ct_entries;

	if (end - p < 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in table based image data in gif image");

	mincodesize = *p;

	/* if there is no overlap, avoid pasting image data, just consume it */
	if (info->image_top >= info->height || info->image_left >= info->width)
	{
		p = gif_read_subblocks(ctx, info, p + 1, end, NULL);
		return p;
	}

	fz_var(compressed);
	fz_var(lzwstm);
	fz_var(stm);
	fz_var(uncompressed);

	fz_try(ctx)
	{
		compressed = fz_new_buffer(ctx, 0);
		p = gif_read_subblocks(ctx, info, p + 1, end, compressed);

		stm = fz_open_buffer(ctx, compressed);
		lzwstm = fz_open_lzwd(ctx, stm, 0, mincodesize + 1, 1, 1);

		uncompressed = fz_read_all(ctx, lzwstm, 0);
		if (uncompressed->len < info->image_width * info->image_height)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in compressed table based image data in gif image");

		if (info->has_lct)
		{
			ct = info->lct;
			ct_entries = info->lct_entries;
		}
		else if (info->has_gct)
		{
			ct = info->gct;
			ct_entries = info->gct_entries;
		}
		else
		{
			ct = dct;
			ct_entries = 256;
		}

		sp = uncompressed->data;
		if (info->image_interlaced)
		{
			for (y = 0; y < info->image_height; y += 8, sp += info->image_width)
				gif_read_line(ctx, info, ct_entries, ct, y, sp);
			for (y = 4; y < info->image_height; y += 8, sp += info->image_width)
				gif_read_line(ctx, info, ct_entries, ct, y, sp);
			for (y = 2; y < info->image_height; y += 4, sp += info->image_width)
				gif_read_line(ctx, info, ct_entries, ct, y, sp);
			for (y = 1; y < info->image_height; y += 2, sp += info->image_width)
				gif_read_line(ctx, info, ct_entries, ct, y, sp);
		}
		else
			for (y = 0; y < info->image_height; y++, sp += info->image_width)
				gif_read_line(ctx, info, ct_entries, ct, y, sp);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, uncompressed);
		fz_drop_buffer(ctx, compressed);
		fz_drop_stream(ctx, lzwstm);
		fz_drop_stream(ctx, stm);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return p;
}

static const unsigned char *
gif_read_gce(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	if (end - p < 8)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in graphic control extension in gif image");
	if (p[2] != 0x04)
		fz_throw(ctx, FZ_ERROR_GENERIC, "out of range graphic control extension block size in gif image");

	info->has_transparency = p[3] & 0x1;
	if (info->has_transparency)
		info->transparent = p[6];

	return p + 8;
}

static const unsigned char *
gif_read_ce(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	return gif_read_subblocks(ctx, info, p + 2, end, NULL);
}

static const unsigned char*
gif_read_pte(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	if (end - p < 15)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in plain text extension in gif image");
	if (p[2] != 0x0c)
		fz_throw(ctx, FZ_ERROR_GENERIC, "out of range plain text extension block size in gif image");
	return gif_read_subblocks(ctx, info, p + 15, end, NULL);
}

static const unsigned char *
gif_read_icc(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
#if FZ_ENABLE_ICC
	fz_colorspace *icc = NULL;
	fz_buffer *buf = NULL;

	fz_var(p);

	buf = fz_new_buffer(ctx, 0);
	fz_try(ctx)
	{
		p = gif_read_subblocks(ctx, info, p, end, buf);
		icc = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_RGB, 0, NULL, buf);
		fz_drop_colorspace(ctx, info->pix->colorspace);
		info->pix->colorspace = icc;
	}
	fz_always(ctx)
		fz_drop_buffer(ctx, buf);
	fz_catch(ctx)
		fz_warn(ctx, "ignoring embedded ICC profile in GIF");

	return p;
#else
	return gif_read_subblocks(ctx, info, p, end, NULL);
#endif
}

/*
NETSCAPE2.0
	http://odur.let.rug.nl/~kleiweg/gif/netscape.html
	http://www.vurdalakov.net/misc/gif/netscape-looping-application-extension
	http://www.vurdalakov.net/misc/gif/netscape-buffering-application-extension
	https://code.google.com/p/gifdotnet/source/browse/src/GifDotNet/GifApplicationExtensionBlock.cs#95
	http://trac.webkit.org/browser/trunk/Source/WebCore/platform/image-decoders/gif/GIFImageReader.cpp#L617

ANIMEXTS1.0
	http://www.vurdalakov.net/misc/gif/animexts-looping-application-extension
	https://code.google.com/p/gifdotnet/source/browse/src/GifDotNet/GifApplicationExtensionBlock.cs#95

ICCRGBG1012
	http://www.color.org/icc1V42.pdf

XMP DataXMP
	http://wwwimages.adobe.com/www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart3.pdf

fractint
	http://fractint.net/fractsvn/trunk/fractint/common/loadfile.c

ZGATEXTI5 ZGATILEI5 ZGACTRLI5 ZGANPIMGI5
ZGAVECTI5 ZGAALPHAI5 ZGATITLE4.0 ZGATEXTI4.0
	Zoner GIF animator 4.0 and 5.0
*/
static const unsigned char *
gif_read_ae(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	static char *ignorable[] = {
		"NETSACPE2.0", "ANIMEXTS1.0", "XMP DataXMP",
		"ZGATEXTI5\0\0", "ZGATILEI5\0\0", "ZGANPIMGI5\0", "ZGACTRLI5\0\0",
		"ZGAVECTI5\0\0", "ZGAALPHAI5\0", "ZGATITLE4.0", "ZGATEXTI4.0",
	};
	int i, ignored;

	if (end - p < 14)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in application extension in gif image");
	if (p[2] != 0x0b)
		fz_throw(ctx, FZ_ERROR_GENERIC, "out of range application extension block size in gif image");

	ignored = 0;
	for (i = 0; i < (int)nelem(ignorable); i++)
		ignored |= memcmp(&p[3], ignorable[i], 8 + 3);
	if (!ignored)
	{
		char extension[9];
		memmove(extension, &p[3], 8);
		extension[8] = '\0';
		fz_warn(ctx, "ignoring unsupported application extension '%s' in gif image", extension);
	}

	if (!memcmp(&p[3], "ICCRGBG1012", 11))
		return gif_read_icc(ctx, info, p + 14, end);

	return gif_read_subblocks(ctx, info, p + 14, end, NULL);
}

static void
gif_mask_transparency(fz_context *ctx, struct info *info)
{
	unsigned char *mp = info->mask;
	unsigned char *dp = fz_pixmap_samples(ctx, info->pix);
	unsigned int x, y;

	for (y = 0; y < info->height; y++)
		for (x = 0; x < info->width; x++, mp++, dp += 4)
			if (*mp == 0x00)
				dp[3] = 0;
}

static fz_pixmap *
gif_read_image(fz_context *ctx, struct info *info, const unsigned char *p, size_t total, int only_metadata)
{
	const unsigned char *end = p + total;

	memset(info, 0x00, sizeof (*info));

	/* Read header */
	p = gif_read_header(ctx, info, p, end);

	/* Read logical screen descriptor */
	p = gif_read_lsd(ctx, info, p, end);

	if (only_metadata)
		return NULL;

	info->pix = fz_new_pixmap(ctx, fz_device_rgb(ctx), info->width, info->height, NULL, 1);

	fz_try(ctx)
	{
		info->mask = fz_calloc(ctx, info->width * info->height, 1);

		/* Read optional global color table */
		if (info->has_gct)
		{
			unsigned char *bp, *dp = fz_pixmap_samples(ctx, info->pix);
			unsigned int x, y, k;

			p = gif_read_gct(ctx, info, p, end);
			bp = &info->gct[info->gct_background * 3];

			memset(info->mask, 0x01, info->width * info->height);

			for (y = 0; y < info->height; y++)
				for (x = 0; x < info->width; x++, dp += 4)
				{
					for (k = 0; k < 3; k++)
						dp[k] = bp[k];
					dp[3] = 255;
				}
		}

		while (1)
		{
			/* Read block indicator */
			if (end - p < 1)
				fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of block indicator in gif image");

			/* Read trailer */
			if (p[0] == 0x3b)
			{
				break;
			}
			/* Read extension */
			else if (p[0] == 0x21)
			{
				/* Read extension label */
				if (end - p < 2)
					fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in extension label in gif image");

				if (p[1] == 0x01 && info->gif89a)
				{
					/* Read plain text extension */
					p = gif_read_pte(ctx, info, p, end);

					/* Graphic control extension applies only to the graphic rendering block following it */
					info->transparent = 0;
					info->has_transparency = 0;
				}
				else if (p[1] == 0xf9 && info->gif89a)
					/* Read graphic control extension */
					p = gif_read_gce(ctx, info, p, end);
				else if (p[1] == 0xfe && info->gif89a)
					/* Read comment extension */
					p = gif_read_ce(ctx, info, p, end);
				else if (p[1] == 0xff && info->gif89a)
					/* Read application extension */
					p = gif_read_ae(ctx, info, p, end);
				else
					fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported extension label %02x in gif image", p[1]);
			}
			/* Read image descriptor */
			else if (p[0] == 0x2c)
			{
				p = gif_read_id(ctx, info, p, end);

				if (info->has_lct)
					/* Read local color table */
					p = gif_read_lct(ctx, info, p, end);

				/* Read table based image data */
				p = gif_read_tbid(ctx, info, p, end);

				/* Graphic control extension applies only to the graphic rendering block following it */
				info->transparent = 0;
				info->has_transparency = 0;

				/* Image descriptor applies only to the table based image data following it */
				info->image_left = info->image_top = 0;
				info->image_width = info->width;
				info->image_height = info->height;
				info->image_interlaced = 0;
				fz_free(ctx, info->lct);
				info->lct = NULL;
				info->has_lct = 0;
			}
			else
				fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported block indicator %02x in gif image", p[0]);
		}

		gif_mask_transparency(ctx, info);
		fz_premultiply_pixmap(ctx, info->pix);
	}
	fz_always(ctx)
	{
		fz_free(ctx, info->mask);
		fz_free(ctx, info->lct);
		fz_free(ctx, info->gct);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, info->pix);
		fz_rethrow(ctx);
	}

	return info->pix;
}

fz_pixmap *
fz_load_gif(fz_context *ctx, const unsigned char *p, size_t total)
{
	fz_pixmap *image;
	struct info gif;

	image = gif_read_image(ctx, &gif, p, total, 0);
	image->xres = gif.xres;
	image->yres = gif.yres;

	return image;
}

void
fz_load_gif_info(fz_context *ctx, const unsigned char *p, size_t total, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	struct info gif;

	gif_read_image(ctx, &gif, p, total, 1);
	*cspacep = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	*wp = gif.width;
	*hp = gif.height;
	*xresp = gif.xres;
	*yresp = gif.yres;
}
