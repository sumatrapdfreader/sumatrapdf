#include "mupdf/fitz.h"

#include <string.h>
#include <limits.h>

static const unsigned char web_palette[] = {
	0x00, 0x00, 0x00, 0x33, 0x00, 0x00, 0x66, 0x00, 0x00, 0x99, 0x00, 0x00, 0xCC, 0x00, 0x00, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x33, 0x33, 0x00, 0x33, 0x66, 0x00, 0x33, 0x99, 0x00, 0x33, 0xCC, 0x00, 0x33, 0xFF, 0x00, 0x33,
	0x00, 0x00, 0x66, 0x33, 0x00, 0x66, 0x66, 0x00, 0x66, 0x99, 0x00, 0x66, 0xCC, 0x00, 0x66, 0xFF, 0x00, 0x66,
	0x00, 0x00, 0x99, 0x33, 0x00, 0x99, 0x66, 0x00, 0x99, 0x99, 0x00, 0x99, 0xCC, 0x00, 0x99, 0xFF, 0x00, 0x99,
	0x00, 0x00, 0xCC, 0x33, 0x00, 0xCC, 0x66, 0x00, 0xCC, 0x99, 0x00, 0xCC, 0xCC, 0x00, 0xCC, 0xFF, 0x00, 0xCC,
	0x00, 0x00, 0xFF, 0x33, 0x00, 0xFF, 0x66, 0x00, 0xFF, 0x99, 0x00, 0xFF, 0xCC, 0x00, 0xFF, 0xFF, 0x00, 0xFF,
	0x00, 0x33, 0x00, 0x33, 0x33, 0x00, 0x66, 0x33, 0x00, 0x99, 0x33, 0x00, 0xCC, 0x33, 0x00, 0xFF, 0x33, 0x00,
	0x00, 0x33, 0x33, 0x33, 0x33, 0x33, 0x66, 0x33, 0x33, 0x99, 0x33, 0x33, 0xCC, 0x33, 0x33, 0xFF, 0x33, 0x33,
	0x00, 0x33, 0x66, 0x33, 0x33, 0x66, 0x66, 0x33, 0x66, 0x99, 0x33, 0x66, 0xCC, 0x33, 0x66, 0xFF, 0x33, 0x66,
	0x00, 0x33, 0x99, 0x33, 0x33, 0x99, 0x66, 0x33, 0x99, 0x99, 0x33, 0x99, 0xCC, 0x33, 0x99, 0xFF, 0x33, 0x99,
	0x00, 0x33, 0xCC, 0x33, 0x33, 0xCC, 0x66, 0x33, 0xCC, 0x99, 0x33, 0xCC, 0xCC, 0x33, 0xCC, 0xFF, 0x33, 0xCC,
	0x00, 0x33, 0xFF, 0x33, 0x33, 0xFF, 0x66, 0x33, 0xFF, 0x99, 0x33, 0xFF, 0xCC, 0x33, 0xFF, 0xFF, 0x33, 0xFF,
	0x00, 0x66, 0x00, 0x33, 0x66, 0x00, 0x66, 0x66, 0x00, 0x99, 0x66, 0x00, 0xCC, 0x66, 0x00, 0xFF, 0x66, 0x00,
	0x00, 0x66, 0x33, 0x33, 0x66, 0x33, 0x66, 0x66, 0x33, 0x99, 0x66, 0x33, 0xCC, 0x66, 0x33, 0xFF, 0x66, 0x33,
	0x00, 0x66, 0x66, 0x33, 0x66, 0x66, 0x66, 0x66, 0x66, 0x99, 0x66, 0x66, 0xCC, 0x66, 0x66, 0xFF, 0x66, 0x66,
	0x00, 0x66, 0x99, 0x33, 0x66, 0x99, 0x66, 0x66, 0x99, 0x99, 0x66, 0x99, 0xCC, 0x66, 0x99, 0xFF, 0x66, 0x99,
	0x00, 0x66, 0xCC, 0x33, 0x66, 0xCC, 0x66, 0x66, 0xCC, 0x99, 0x66, 0xCC, 0xCC, 0x66, 0xCC, 0xFF, 0x66, 0xCC,
	0x00, 0x66, 0xFF, 0x33, 0x66, 0xFF, 0x66, 0x66, 0xFF, 0x99, 0x66, 0xFF, 0xCC, 0x66, 0xFF, 0xFF, 0x66, 0xFF,
	0x00, 0x99, 0x00, 0x33, 0x99, 0x00, 0x66, 0x99, 0x00, 0x99, 0x99, 0x00, 0xCC, 0x99, 0x00, 0xFF, 0x99, 0x00,
	0x00, 0x99, 0x33, 0x33, 0x99, 0x33, 0x66, 0x99, 0x33, 0x99, 0x99, 0x33, 0xCC, 0x99, 0x33, 0xFF, 0x99, 0x33,
	0x00, 0x99, 0x66, 0x33, 0x99, 0x66, 0x66, 0x99, 0x66, 0x99, 0x99, 0x66, 0xCC, 0x99, 0x66, 0xFF, 0x99, 0x66,
	0x00, 0x99, 0x99, 0x33, 0x99, 0x99, 0x66, 0x99, 0x99, 0x99, 0x99, 0x99, 0xCC, 0x99, 0x99, 0xFF, 0x99, 0x99,
	0x00, 0x99, 0xCC, 0x33, 0x99, 0xCC, 0x66, 0x99, 0xCC, 0x99, 0x99, 0xCC, 0xCC, 0x99, 0xCC, 0xFF, 0x99, 0xCC,
	0x00, 0x99, 0xFF, 0x33, 0x99, 0xFF, 0x66, 0x99, 0xFF, 0x99, 0x99, 0xFF, 0xCC, 0x99, 0xFF, 0xFF, 0x99, 0xFF,
	0x00, 0xCC, 0x00, 0x33, 0xCC, 0x00, 0x66, 0xCC, 0x00, 0x99, 0xCC, 0x00, 0xCC, 0xCC, 0x00, 0xFF, 0xCC, 0x00,
	0x00, 0xCC, 0x33, 0x33, 0xCC, 0x33, 0x66, 0xCC, 0x33, 0x99, 0xCC, 0x33, 0xCC, 0xCC, 0x33, 0xFF, 0xCC, 0x33,
	0x00, 0xCC, 0x66, 0x33, 0xCC, 0x66, 0x66, 0xCC, 0x66, 0x99, 0xCC, 0x66, 0xCC, 0xCC, 0x66, 0xFF, 0xCC, 0x66,
	0x00, 0xCC, 0x99, 0x33, 0xCC, 0x99, 0x66, 0xCC, 0x99, 0x99, 0xCC, 0x99, 0xCC, 0xCC, 0x99, 0xFF, 0xCC, 0x99,
	0x00, 0xCC, 0xCC, 0x33, 0xCC, 0xCC, 0x66, 0xCC, 0xCC, 0x99, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xFF, 0xCC, 0xCC,
	0x00, 0xCC, 0xFF, 0x33, 0xCC, 0xFF, 0x66, 0xCC, 0xFF, 0x99, 0xCC, 0xFF, 0xCC, 0xCC, 0xFF, 0xFF, 0xCC, 0xFF,
	0x00, 0xFF, 0x00, 0x33, 0xFF, 0x00, 0x66, 0xFF, 0x00, 0x99, 0xFF, 0x00, 0xCC, 0xFF, 0x00, 0xFF, 0xFF, 0x00,
	0x00, 0xFF, 0x33, 0x33, 0xFF, 0x33, 0x66, 0xFF, 0x33, 0x99, 0xFF, 0x33, 0xCC, 0xFF, 0x33, 0xFF, 0xFF, 0x33,
	0x00, 0xFF, 0x66, 0x33, 0xFF, 0x66, 0x66, 0xFF, 0x66, 0x99, 0xFF, 0x66, 0xCC, 0xFF, 0x66, 0xFF, 0xFF, 0x66,
	0x00, 0xFF, 0x99, 0x33, 0xFF, 0x99, 0x66, 0xFF, 0x99, 0x99, 0xFF, 0x99, 0xCC, 0xFF, 0x99, 0xFF, 0xFF, 0x99,
	0x00, 0xFF, 0xCC, 0x33, 0xFF, 0xCC, 0x66, 0xFF, 0xCC, 0x99, 0xFF, 0xCC, 0xCC, 0xFF, 0xCC, 0xFF, 0xFF, 0xCC,
	0x00, 0xFF, 0xFF, 0x33, 0xFF, 0xFF, 0x66, 0xFF, 0xFF, 0x99, 0xFF, 0xFF, 0xCC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const unsigned char vga_palette[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0x00, 0xAA, 0xAA,
	0xAA, 0x00, 0x00, 0xAA, 0x00, 0xAA, 0xAA, 0x55, 0x00, 0xAA, 0xAA, 0xAA,
	0x55, 0x55, 0x55, 0x55, 0x55, 0xFF, 0x55, 0xFF, 0x55, 0x55, 0xFF, 0xFF,
	0xFF, 0x55, 0x55, 0xFF, 0x55, 0xFF, 0xFF, 0xFF, 0x55, 0xFF, 0xFF, 0xFF,
};

static const unsigned char gray_palette[] = {
	0x00, 0x00, 0x00, 0x54, 0x54, 0x54,
	0xA8, 0xA8, 0xA8, 0xFF, 0xFF, 0xFF,
};

static const unsigned char bw_palette[] = {
	0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
};

enum {
	BI_RLE24 = -1,
	BI_RGB = 0,
	BI_RLE8 = 1,
	BI_RLE4 = 2,
	BI_BITFIELDS = 3,
	BI_JPEG = 4,
	BI_PNG = 5,
	BI_ALPHABITS = 6,
	BI_UNSUPPORTED = 42,
};

struct info
{
	int filesize;
	int offset;
	int topdown;
	int width, height;
	int xres, yres;
	int bitcount;
	int compression;
	int colors;
	int rmask, gmask, bmask, amask;
	unsigned char palette[256 * 3];

	int extramasks;
	int palettetype;
	unsigned char *samples;

	int rshift, gshift, bshift, ashift;
	int rbits, gbits, bbits, abits;
};

#define read8(p) ((p)[0])
#define read16(p) (((p)[1] << 8) | (p)[0])
#define read32(p) (((p)[3] << 24) | ((p)[2] << 16) | ((p)[1] << 8) | (p)[0])

static const unsigned char *
bmp_read_file_header(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	if (end - p < 14)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in file header in bmp image");

	if (memcmp(&p[0], "BM", 2))
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid signature in bmp image");

	info->filesize = read32(p + 2);
	info->offset = read32(p + 10);

	return p + 14;
}

static const unsigned char *
bmp_read_bitmap_core_header(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	int size;

	size = read32(p + 0);
	if (size != 12)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported core header size in bmp image");

	if (size >= 12)
	{
		if (end - p < 12)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in bitmap core header in bmp image");

		info->width = read16(p + 4);
		info->height = read16(p + 6);
		info->bitcount = read16(p + 10);
	}

	info->xres = 2835;
	info->yres = 2835;
	info->compression = BI_RGB;
	info->palettetype = 0;

	return p + size;
}

static const unsigned char *
bmp_read_bitmap_os2_header(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	int size;

	size = read32(p + 0);
	if (size != 16 && size != 64)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported os2 header size in bmp image");

	if (size >= 16)
	{
		if (end - p < 16)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in bitmap os2 header in bmp image");

		info->width = read32(p + 4);
		info->height = read32(p + 8);
		info->bitcount = read16(p + 14);

		info->compression = BI_RGB;
	}
	if (size >= 64)
	{
		if (end - p < 64)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in bitmap os2 header in bmp image");

		info->compression = read32(p + 16);
		info->xres = read32(p + 24);
		info->yres = read32(p + 28);
		info->colors = read32(p + 32);

		/* 4 in this header is interpreted as 24 bit RLE encoding */
		if (info->compression < 0)
			info->compression = BI_UNSUPPORTED;
		else if (info->compression == 4)
			info->compression = BI_RLE24;
	}

	info->palettetype = 1;

	return p + size;
}

static void maskinfo(unsigned int mask, int *shift, int *bits)
{
	*bits = 0;
	*shift = 0;
	if (mask) {
		while ((mask & 1) == 0) {
			*shift += 1;
			mask >>= 1;
		}
		while ((mask & 1) == 1) {
			*bits += 1;
			mask >>= 1;
		}
	}
}

static const unsigned char *
bmp_read_bitmap_info_header(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	int size;

	size = read32(p + 0);
	if (size != 40 && size != 52 && size != 56 && size != 64 &&
			size != 108 && size != 124)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported info header size in bmp image");

	if (size >= 40)
	{
		if (end - p < 40)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in bitmap info header in bmp image");

		info->width = read32(p + 4);
		info->topdown = (p[8 + 3] & 0x80) != 0;
		if (info->topdown)
			info->height = -read32(p + 8);
		else
			info->height = read32(p + 8);
		info->bitcount = read16(p + 14);
		info->compression = read32(p + 16);
		info->xres = read32(p + 24);
		info->yres = read32(p + 28);
		info->colors = read32(p + 32);

		if (size == 40 && info->compression == BI_BITFIELDS && (info->bitcount == 16 || info->bitcount == 32))
			info->extramasks = 1;
		else if (size == 40 && info->compression == BI_ALPHABITS && (info->bitcount == 16 || info->bitcount == 32))
			info->extramasks = 1;

		if (info->bitcount == 16) {
			info->rmask = 0x00007c00;
			info->gmask = 0x000003e0;
			info->bmask = 0x0000001f;
			info->amask = 0x00000000;
		} else if (info->bitcount == 32) {
			info->rmask = 0x00ff0000;
			info->gmask = 0x0000ff00;
			info->bmask = 0x000000ff;
			info->amask = 0x00000000;
		}
	}
	if (size >= 52)
	{
		if (end - p < 52)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in bitmap info header in bmp image");

		if (info->compression == BI_BITFIELDS) {
			info->rmask = read32(p + 40);
			info->gmask = read32(p + 44);
			info->bmask = read32(p + 48);
		}
	}
	if (size >= 56)
	{
		if (end - p < 56)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in bitmap info header in bmp image");

		if (info->compression == BI_BITFIELDS) {
			info->amask = read32(p + 52);
		}
	}

	info->palettetype = 1;

	return p + size;
}

static const unsigned char *
bmp_read_extra_masks(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	int size = 0;

	if (info->compression == BI_BITFIELDS)
	{
		size = 12;
		if (end - p < 12)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in mask header in bmp image");

		info->rmask = read32(p + 0);
		info->gmask = read32(p + 4);
		info->bmask = read32(p + 8);
	}
	else if (info->compression == BI_ALPHABITS)
	{
		size = 16;
		if (end - p < 16)
			fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in mask header in bmp image");

		/* ignore alpha mask */
		info->rmask = read32(p + 0);
		info->gmask = read32(p + 4);
		info->bmask = read32(p + 8);
	}

	return p + size;
}

static int
bmp_palette_is_gray(fz_context *ctx, struct info *info, int readcolors)
{
	int i;
	for (i = 0; i < readcolors; i++)
	{
		int rgdiff = fz_absi(info->palette[3 * i + 0] - info->palette[3 * i + 1]);
		int gbdiff = fz_absi(info->palette[3 * i + 1] - info->palette[3 * i + 2]);
		int rbdiff = fz_absi(info->palette[3 * i + 0] - info->palette[3 * i + 2]);
		if (rgdiff > 2 || gbdiff > 2 || rbdiff > 2)
			return 0;
	}
	return 1;
}

static void
bmp_load_default_palette(fz_context *ctx, struct info *info, int readcolors)
{
	int i;

	fz_warn(ctx, "color table too short; loading default palette");

	if (info->bitcount == 8)
	{
		if (!bmp_palette_is_gray(ctx, info, readcolors))
			memcpy(&info->palette[readcolors * 3], &web_palette[readcolors * 3],
					sizeof(web_palette) - readcolors * 3);
		else
			for (i = readcolors; i < 256; i++)
			{
				info->palette[3 * i + 0] = i;
				info->palette[3 * i + 1] = i;
				info->palette[3 * i + 2] = i;
			}
	}
	else if (info->bitcount == 4)
	{
		if (!bmp_palette_is_gray(ctx, info, readcolors))
			memcpy(&info->palette[readcolors * 3], &vga_palette[readcolors * 3],
					sizeof(vga_palette) - readcolors * 3);
		else
			for (i = readcolors; i < 16; i++)
			{
				info->palette[3 * i + 0] = (i << 4) | i;
				info->palette[3 * i + 1] = (i << 4) | i;
				info->palette[3 * i + 2] = (i << 4) | i;
			}
	}
	else if (info->bitcount == 2)
		memcpy(info->palette, gray_palette, sizeof(gray_palette));
	else if (info->bitcount == 1)
		memcpy(info->palette, bw_palette, sizeof(bw_palette));
}

static const unsigned char *
bmp_read_color_table(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	int i, colors, readcolors;

	if (info->bitcount > 8)
		return p;

	if (info->colors == 0)
		colors = 1 << info->bitcount;
	else
		colors = info->colors;

	colors = fz_mini(colors, 1 << info->bitcount);

	if (info->palettetype == 0)
	{
		readcolors = fz_mini(colors, (end - p) / 3);
		for (i = 0; i < readcolors; i++)
		{
			info->palette[3 * i + 0] = read8(p + i * 3 + 2);
			info->palette[3 * i + 1] = read8(p + i * 3 + 1);
			info->palette[3 * i + 2] = read8(p + i * 3 + 0);
		}
		if (readcolors < colors)
			bmp_load_default_palette(ctx, info, readcolors);
		return p + readcolors * 3;
	}
	else
	{
		readcolors = fz_mini(colors, (end - p) / 4);
		for (i = 0; i < readcolors; i++)
		{
			/* ignore alpha channel */
			info->palette[3 * i + 0] = read8(p + i * 4 + 2);
			info->palette[3 * i + 1] = read8(p + i * 4 + 1);
			info->palette[3 * i + 2] = read8(p + i * 4 + 0);
		}
		if (readcolors < colors)
			bmp_load_default_palette(ctx, info, readcolors);
		return p + readcolors * 4;
	}

	return p;
}

static unsigned char *
bmp_decompress_rle24(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char **end)
{
	const unsigned char *sp;
	unsigned char *dp, *ep, *decompressed;
	int width = info->width;
	int height = info->height;
	int stride;
	int x, i;

	stride = (width*3 + 3) / 4 * 4;

	sp = p;
	dp = decompressed = fz_calloc(ctx, height, stride);
	ep = dp + height * stride;
	x = 0;

	while (sp + 2 <= *end)
	{
		if (sp[0] == 0 && sp[1] == 0)
		{ /* end of line */
			if (x*3 < stride)
				dp += stride - x*3;
			sp += 2;
			x = 0;
		}
		else if (sp[0] == 0 && sp[1] == 1)
		{ /* end of bitmap */
			dp = ep;
			break;
		}
		else if (sp[0] == 0 && sp[1] == 2)
		{ /* delta */
			int deltax, deltay;
			if (sp + 4 > *end)
				break;
			deltax = sp[2];
			deltay = sp[3];
			dp += deltax*3 + deltay * stride;
			sp += 4;
			x += deltax;
		}
		else if (sp[0] == 0 && sp[1] >= 3)
		{ /* absolute */
			int n = sp[1] * 3;
			int nn = (n + 1) / 2 * 2;
			if (sp + 2 + nn > *end)
				break;
			if (dp + n > ep) {
				fz_warn(ctx, "buffer overflow in bitmap data in bmp image");
				break;
			}
			sp += 2;
			for (i = 0; i < n; i++)
				dp[i] = sp[i];
			dp += n;
			sp += (n + 1) / 2 * 2;
			x += n;
		}
		else
		{ /* encoded */
			int n = sp[0] * 3;
			if (sp + 1 + 3 > *end)
				break;
			if (dp + n > ep) {
				fz_warn(ctx, "buffer overflow in bitmap data in bmp image");
				break;
			}
			for (i = 0; i < n / 3; i++) {
				dp[i * 3 + 0] = sp[1];
				dp[i * 3 + 1] = sp[2];
				dp[i * 3 + 2] = sp[3];
			}
			dp += n;
			sp += 1 + 3;
			x += n;
		}
	}

	if (dp < ep)
		fz_warn(ctx, "premature end of bitmap data in bmp image");

	info->compression = BI_RGB;
	info->bitcount = 24;
	*end = ep;
	return decompressed;
}

static unsigned char *
bmp_decompress_rle8(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char **end)
{
	const unsigned char *sp;
	unsigned char *dp, *ep, *decompressed;
	int width = info->width;
	int height = info->height;
	int stride;
	int x, i;

	stride = (width + 3) / 4 * 4;

	sp = p;
	dp = decompressed = fz_calloc(ctx, height, stride);
	ep = dp + height * stride;
	x = 0;

	while (sp + 2 <= *end)
	{
		if (sp[0] == 0 && sp[1] == 0)
		{ /* end of line */
			if (x < stride)
				dp += stride - x;
			sp += 2;
			x = 0;
		}
		else if (sp[0] == 0 && sp[1] == 1)
		{ /* end of bitmap */
			dp = ep;
			break;
		}
		else if (sp[0] == 0 && sp[1] == 2)
		{ /* delta */
			int deltax, deltay;
			if (sp + 4 > *end)
				break;
			deltax = sp[2];
			deltay = sp[3];
			dp += deltax + deltay * stride;
			sp += 4;
			x += deltax;
		}
		else if (sp[0] == 0 && sp[1] >= 3)
		{ /* absolute */
			int n = sp[1];
			int nn = (n + 1) / 2 * 2;
			if (sp + 2 + nn > *end)
				break;
			if (dp + n > ep) {
				fz_warn(ctx, "buffer overflow in bitmap data in bmp image");
				break;
			}
			sp += 2;
			for (i = 0; i < n; i++)
				dp[i] = sp[i];
			dp += n;
			sp += (n + 1) / 2 * 2;
			x += n;
		}
		else
		{ /* encoded */
			int n = sp[0];
			if (dp + n > ep) {
				fz_warn(ctx, "buffer overflow in bitmap data in bmp image");
				break;
			}
			for (i = 0; i < n; i++)
				dp[i] = sp[1];
			dp += n;
			sp += 2;
			x += n;
		}
	}

	if (dp < ep)
		fz_warn(ctx, "premature end of bitmap data in bmp image");

	info->compression = BI_RGB;
	info->bitcount = 8;
	*end = ep;
	return decompressed;
}

static unsigned char *
bmp_decompress_rle4(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char **end)
{
	const unsigned char *sp;
	unsigned char *dp, *ep, *decompressed;
	int width = info->width;
	int height = info->height;
	int stride;
	int i, x;

	stride = ((width + 1) / 2 + 3) / 4 * 4;

	sp = p;
	dp = decompressed = fz_calloc(ctx, height, stride);
	ep = dp + height * stride;
	x = 0;

	while (sp + 2 <= *end)
	{
		if (sp[0] == 0 && sp[1] == 0)
		{ /* end of line */
			int xx = x / 2;
			if (xx < stride)
				dp += stride - xx;
			sp += 2;
			x = 0;
		}
		else if (sp[0] == 0 && sp[1] == 1)
		{ /* end of bitmap */
			dp = ep;
			break;
		}
		else if (sp[0] == 0 && sp[1] == 2)
		{ /* delta */
			int deltax, deltay, startlow;
			if (sp + 4 > *end)
				break;
			deltax = sp[2];
			deltay = sp[3];
			startlow = x & 1;
			dp += (deltax + startlow) / 2 + deltay * stride;
			sp += 4;
			x += deltax;
		}
		else if (sp[0] == 0 && sp[1] >= 3)
		{ /* absolute */
			int n = sp[1];
			int nn = ((n + 1) / 2 + 1) / 2 * 2;
			if (sp + 2 + nn > *end)
				break;
			if (dp + n / 2 > ep) {
				fz_warn(ctx, "buffer overflow in bitmap data in bmp image");
				break;
			}
			sp += 2;
			for (i = 0; i < n; i++, x++)
			{
				int val = i & 1 ? (sp[i/2]) & 0xF : (sp[i/2] >> 4) & 0xF;
				if (x & 1)
					*dp++ |= val;
				else
					*dp |= val << 4;
			}
			sp += nn;
		}
		else
		{ /* encoded */
			int n = sp[0];
			int hi = (sp[1] >> 4) & 0xF;
			int lo = sp[1] & 0xF;
			if (dp + n / 2 + (x & 1) > ep) {
				fz_warn(ctx, "buffer overflow in bitmap data in bmp image");
				break;
			}
			for (i = 0; i < n; i++, x++)
			{
				int val = i & 1 ? lo : hi;
				if (x & 1)
					*dp++ |= val;
				else
					*dp |= val << 4;
			}
			sp += 2;
		}
	}

	info->compression = BI_RGB;
	info->bitcount = 4;
	*end = ep;
	return decompressed;
}

static fz_pixmap *
bmp_read_bitmap(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char *end)
{
	const int mults[] = { 0, 8191, 2730, 1170, 546, 264, 130, 64 };
	fz_pixmap *pix;
	const unsigned char *ssp;
	unsigned char *ddp;
	unsigned char *decompressed = NULL;
	int bitcount, width, height;
	int sstride, dstride;
	int rmult, gmult, bmult, amult;
	int rtrunc, gtrunc, btrunc, atrunc;
	int x, y;

	if (info->compression == BI_RLE8)
		ssp = decompressed = bmp_decompress_rle8(ctx, info, p, &end);
	else if (info->compression == BI_RLE4)
		ssp = decompressed = bmp_decompress_rle4(ctx, info, p, &end);
	else if (info->compression == BI_RLE24)
		ssp = decompressed = bmp_decompress_rle24(ctx, info, p, &end);
	else
		ssp = p;

	bitcount = info->bitcount;
	width = info->width;
	height = info->height;

	sstride = ((width * bitcount + 31) / 32) * 4;

	if (ssp + sstride * height > end)
	{
		fz_free(ctx, decompressed);
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in bitmap data in bmp image");
	}

	fz_try(ctx)
		pix = fz_new_pixmap(ctx, fz_device_rgb(ctx), width, height, NULL, 1);
	fz_catch(ctx)
	{
		fz_free(ctx, decompressed);
		fz_rethrow(ctx);
	}

	ddp = pix->samples;
	dstride = pix->stride;
	if (!info->topdown)
	{
		ddp = pix->samples + (height - 1) * dstride;
		dstride = -dstride;
	}

	/* These only apply for components in 16-bit and 32-bit mode
	   1-bit (1 * 8191) / 32
	   2-bit (3 * 2730) / 32
	   3-bit (7 * 1170) / 32
	   4-bit (15 * 546) / 32
	   5-bit (31 * 264) / 32
	   6-bit (63 * 130) / 32
	   7-bit (127 * 64) / 32
	*/
	rmult = info->rbits < 8 ? mults[info->rbits] : 1;
	gmult = info->gbits < 8 ? mults[info->gbits] : 1;
	bmult = info->bbits < 8 ? mults[info->bbits] : 1;
	amult = info->abits < 8 ? mults[info->abits] : 1;
	rtrunc = info->rbits < 8 ? 5 : (info->rbits - 8);
	gtrunc = info->gbits < 8 ? 5 : (info->gbits - 8);
	btrunc = info->bbits < 8 ? 5 : (info->bbits - 8);
	atrunc = info->abits < 8 ? 5 : (info->abits - 8);

	for (y = 0; y < height; y++)
	{
		const unsigned char *sp = ssp + y * sstride;
		unsigned char *dp = ddp + y * dstride;

		switch (bitcount)
		{
		case 32:
			for (x = 0; x < width; x++)
			{
				unsigned int sample = (sp[3] << 24) | (sp[2] << 16) | (sp[1] << 8) | sp[0];
				unsigned int r = (sample & info->rmask) >> info->rshift;
				unsigned int g = (sample & info->gmask) >> info->gshift;
				unsigned int b = (sample & info->bmask) >> info->bshift;
				unsigned int a = info->abits == 0 ? 255 : (sample & info->amask) >> info->ashift;
				*dp++ = (r * rmult) >> rtrunc;
				*dp++ = (g * gmult) >> gtrunc;
				*dp++ = (b * bmult) >> btrunc;
				*dp++ = info->abits == 0 ? a : (a * amult) >> atrunc;
				sp += 4;
			}
			break;
		case 24:
			for (x = 0; x < width; x++)
			{
				*dp++ = sp[2];
				*dp++ = sp[1];
				*dp++ = sp[0];
				*dp++ = 255;
				sp += 3;
			}
			break;
		case 16:
			for (x = 0; x < width; x++)
			{
				unsigned int sample = (sp[1] << 8) | sp[0];
				unsigned int r = (sample & info->rmask) >> info->rshift;
				unsigned int g = (sample & info->gmask) >> info->gshift;
				unsigned int b = (sample & info->bmask) >> info->bshift;
				unsigned int a = (sample & info->amask) >> info->ashift;
				*dp++ = (r * rmult) >> rtrunc;
				*dp++ = (g * gmult) >> gtrunc;
				*dp++ = (b * bmult) >> btrunc;
				*dp++ = info->abits == 0 ? 255 : (a * amult) >> atrunc;
				sp += 2;
			}
			break;
		case 8:
			for (x = 0; x < width; x++)
			{
				*dp++ = info->palette[3 * sp[0] + 0];
				*dp++ = info->palette[3 * sp[0] + 1];
				*dp++ = info->palette[3 * sp[0] + 2];
				*dp++ = 255;
				sp++;
			}
			break;
		case 4:
			for (x = 0; x < width; x++)
			{
				int idx;
				switch (x & 1)
				{
				case 0: idx = (sp[0] >> 4) & 0x0f; break;
				case 1: idx = (sp[0] >> 0) & 0x0f; sp++; break;
				}
				*dp++ = info->palette[3 * idx + 0];
				*dp++ = info->palette[3 * idx + 1];
				*dp++ = info->palette[3 * idx + 2];
				*dp++ = 255;
			}
			break;
		case 2:
			for (x = 0; x < width; x++)
			{
				int idx;
				switch (x & 3)
				{
				case 0: idx = (sp[0] >> 6) & 0x03; break;
				case 1: idx = (sp[0] >> 4) & 0x03; break;
				case 2: idx = (sp[0] >> 2) & 0x03; break;
				case 3: idx = (sp[0] >> 0) & 0x03; sp++; break;
				}
				*dp++ = info->palette[3 * idx + 0];
				*dp++ = info->palette[3 * idx + 1];
				*dp++ = info->palette[3 * idx + 2];
				*dp++ = 255;
			}
			break;
		case 1:
			for (x = 0; x < width; x++)
			{
				int idx;
				switch (x & 7)
				{
				case 0: idx = (sp[0] >> 7) & 0x01; break;
				case 1: idx = (sp[0] >> 6) & 0x01; break;
				case 2: idx = (sp[0] >> 5) & 0x01; break;
				case 3: idx = (sp[0] >> 4) & 0x01; break;
				case 4: idx = (sp[0] >> 3) & 0x01; break;
				case 5: idx = (sp[0] >> 2) & 0x01; break;
				case 6: idx = (sp[0] >> 1) & 0x01; break;
				case 7: idx = (sp[0] >> 0) & 0x01; sp++; break;
				}
				*dp++ = info->palette[3 * idx + 0];
				*dp++ = info->palette[3 * idx + 1];
				*dp++ = info->palette[3 * idx + 2];
				*dp++ = 255;
			}
			break;
		}
	}

	fz_free(ctx, decompressed);
	fz_premultiply_pixmap(ctx, pix);
	return pix;
}

static fz_pixmap *
bmp_read_image(fz_context *ctx, struct info *info, const unsigned char *p, size_t total, int only_metadata)
{
	const unsigned char *begin = p;
	const unsigned char *end = p + total;
	int size;

	memset(info, 0x00, sizeof (*info));

	p = bmp_read_file_header(ctx, info, p, end);

	info->filesize = fz_mini(info->filesize, (int)total);

	if (end - p < 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end in bitmap core header in bmp image");
	size = read32(p + 0);

	if (size == 12)
		p = bmp_read_bitmap_core_header(ctx, info, p, end);
	else if (size == 40 || size == 52 || size == 56 || size == 108 || size == 124)
	{
		p = bmp_read_bitmap_info_header(ctx, info, p, end);
		if (info->extramasks)
			p = bmp_read_extra_masks(ctx, info, p, end);
	}
	else if (size == 16 || size == 64)
		p = bmp_read_bitmap_os2_header(ctx, info, p, end);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid header size (%d) in bmp image", size);

	maskinfo(info->rmask, &info->rshift, &info->rbits);
	maskinfo(info->gmask, &info->gshift, &info->gbits);
	maskinfo(info->bmask, &info->bshift, &info->bbits);
	maskinfo(info->amask, &info->ashift, &info->abits);

	if (info->width <= 0 || info->width > SHRT_MAX || info->height <= 0 || info->height > SHRT_MAX)
		fz_throw(ctx, FZ_ERROR_GENERIC, "dimensions (%d x %d) out of range in bmp image",
				info->width, info->height);
	if (info->compression != BI_RGB && info->compression != BI_RLE8 &&
			info->compression != BI_RLE4 && info->compression != BI_BITFIELDS &&
			info->compression != BI_JPEG && info->compression != BI_PNG &&
			info->compression != BI_ALPHABITS && info->compression != BI_RLE24)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported compression method (%d) in bmp image", info->compression);
	if ((info->compression == BI_RGB && info->bitcount != 1 &&
			info->bitcount != 2 && info->bitcount != 4 &&
			info->bitcount != 8 && info->bitcount != 16 &&
			info->bitcount != 24 && info->bitcount != 32) ||
			(info->compression == BI_RLE8 && info->bitcount != 8) ||
			(info->compression == BI_RLE4 && info->bitcount != 4) ||
			(info->compression == BI_BITFIELDS && info->bitcount != 16 && info->bitcount != 32) ||
			(info->compression == BI_JPEG && info->bitcount != 0) ||
			(info->compression == BI_PNG && info->bitcount != 0) ||
			(info->compression == BI_ALPHABITS && info->bitcount != 16 && info->bitcount != 32) ||
			(info->compression == BI_RLE24 && info->bitcount != 24))
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid bits per pixel (%d) for compression (%d) in bmp image",
				info->bitcount, info->compression);
	if (info->rbits < 0 || info->rbits > info->bitcount)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported %d bit red mask in bmp image", info->rbits);
	if (info->gbits < 0 || info->gbits > info->bitcount)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported %d bit green mask in bmp image", info->gbits);
	if (info->bbits < 0 || info->bbits > info->bitcount)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported %d bit blue mask in bmp image", info->bbits);
	if (info->abits < 0 || info->abits > info->bitcount)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported %d bit alpha mask in bmp image", info->abits);

	if (only_metadata)
		return NULL;

	if (info->compression == BI_JPEG)
	{
		if (p - begin < info->offset)
			p = begin + info->offset;
		return fz_load_jpeg(ctx, p, end - p);
	}
	else if (info->compression == BI_PNG)
	{
		if (p - begin < info->offset)
			p = begin + info->offset;
		return fz_load_png(ctx, p, end - p);
	}
	else
	{
		const unsigned char *color_table_end = begin + info->offset;
		if (end - begin < info->offset)
			color_table_end = end;
		p = bmp_read_color_table(ctx, info, p, color_table_end);

		if (p - begin < info->offset)
			p = begin + info->offset;
		return bmp_read_bitmap(ctx, info, p, end);
	}
}

fz_pixmap *
fz_load_bmp(fz_context *ctx, const unsigned char *p, size_t total)
{
	struct info bmp;
	fz_pixmap *image;

	image = bmp_read_image(ctx, &bmp, p, total, 0);
	image->xres = bmp.xres / (1000.0f / 25.4f);
	image->yres = bmp.yres / (1000.0f / 25.4f);

	return image;
}

void
fz_load_bmp_info(fz_context *ctx, const unsigned char *p, size_t total, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	struct info bmp;

	bmp_read_image(ctx, &bmp, p, total, 1);

	*cspacep = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	*wp = bmp.width;
	*hp = bmp.height;
	*xresp = bmp.xres / (1000.0f / 25.4f);
	*yresp = bmp.yres / (1000.0f / 25.4f);
}
