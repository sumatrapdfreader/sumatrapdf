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

#include "image-imp.h"
#include "pixmap-imp.h"

#include <string.h>
#include <limits.h>

#undef BMP_DEBUG

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
	BI_NONE = 0,
	BI_RLE8 = 1,
	BI_RLE4 = 2,
	BI_BITFIELDS = 3,
	BI_HUFFMAN1D = 3,
	BI_JPEG = 4,
	BI_RLE24 = 4,
	BI_PNG = 5,
	BI_ALPHABITS = 6,
};

struct info
{
	char type[2];
	uint32_t version;
	uint32_t bitmapoffset;
	uint32_t width, height;
	uint16_t bitcount;
	uint32_t compression;
	uint32_t bitmapsize;
	uint32_t xres, yres;
	uint32_t colors;
	uint32_t rmask, gmask, bmask, amask;
	uint8_t palette[256 * 3];
	uint32_t colorspacetype;
	uint32_t endpoints[3 * 3];
	uint32_t gamma[3];
	uint32_t intent;
	uint32_t profileoffset;
	uint32_t profilesize;

	int topdown;
	unsigned int rshift, gshift, bshift, ashift;
	unsigned int rbits, gbits, bbits, abits;

	unsigned char *samples;
	fz_colorspace *cs;
};

#define read8(p) ((p)[0])
#define read16(p) (((p)[1] << 8) | (p)[0])
#define read32(p) (((p)[3] << 24) | ((p)[2] << 16) | ((p)[1] << 8) | (p)[0])

#define DPM_TO_DPI(dpm) ((dpm) * 25.4f / 1000.0f)

#define is_bitmap_array(p) ((p)[0] == 'B' && (p)[1] == 'A')
#define is_bitmap(p) ((p)[0] == 'B' && (p)[1] == 'M')

#define is_os2_bmp(info) ((info)->version == 12 || (info)->version == 16 || (info)->version == 64)
#define is_win_bmp(info) ((info)->version == 12 || (info)->version == 40 || (info)->version == 52 || (info)->version == 56 || (info)->version == 108 || (info)->version == 124)

#define is_valid_win_compression(info) (is_win_bmp(info) && ((info)->compression == BI_NONE || (info)->compression == BI_RLE8 || (info)->compression == BI_RLE4 || (info)->compression == BI_BITFIELDS || (info)->compression == BI_JPEG || (info)->compression == BI_PNG || (info)->compression == BI_ALPHABITS || (info)->compression == BI_RLE24))
#define is_valid_os2_compression(info) (is_os2_bmp(info) && ((info)->compression == BI_NONE || (info)->compression == BI_RLE8 || (info)->compression == BI_RLE4 || (info)->compression == BI_HUFFMAN1D || (info)->compression == BI_RLE24))
#define is_valid_compression(info) (is_valid_win_compression(info) || is_valid_os2_compression(info))

#define is_valid_rgb_bitcount(info) ((info)->compression == BI_NONE && ((info)->bitcount == 1 || (info)->bitcount == 2 || (info)->bitcount == 4 || (info)->bitcount == 8 || (info)->bitcount == 16 || (info)->bitcount == 24 || (info)->bitcount == 32 || (info)->bitcount == 64))
#define is_valid_rle8_bitcount(info) ((info)->compression == BI_RLE8 && (info)->bitcount == 8)
#define is_valid_rle4_bitcount(info) ((info)->compression == BI_RLE4 && (info)->bitcount == 4)
#define is_valid_bitfields_bitcount(info) (is_win_bmp(info) && (info)->compression == BI_BITFIELDS && ((info)->bitcount == 16 || (info)->bitcount == 32))
#define is_valid_jpeg_bitcount(info) (is_win_bmp(info) && (info)->compression == BI_JPEG && (info)->bitcount == 0)
#define is_valid_png_bitcount(info) (is_win_bmp(info) && (info)->compression == BI_PNG && (info)->bitcount == 0)
#define is_valid_alphabits_bitcount(info) (is_win_bmp(info) && (info)->compression == BI_ALPHABITS && ((info)->bitcount == 16 || (info)->bitcount == 32))
#define is_valid_rle24_bitcount(info) (is_os2_bmp(info) && (info)->compression == BI_RLE24 && (info)->bitcount == 24)
#define is_valid_huffman1d_bitcount(info) (is_os2_bmp(info) && (info)->compression == BI_HUFFMAN1D && (info)->bitcount == 1)
#define is_valid_bitcount(info) (is_valid_rgb_bitcount(info) || is_valid_rle8_bitcount(info) || is_valid_rle4_bitcount(info) || is_valid_bitfields_bitcount(info) || is_valid_jpeg_bitcount(info) || is_valid_png_bitcount(info) || is_valid_alphabits_bitcount(info) || is_valid_rle24_bitcount(info) || is_valid_huffman1d_bitcount(info))

#define has_palette(info) ((info)->bitcount == 1 || (info)->bitcount == 2 || (info)->bitcount == 4 || (info)->bitcount == 8)
#define has_color_masks(info) (((info)->bitcount == 16 || (info)->bitcount == 32) && (info)->version == 40 && ((info)->compression == BI_BITFIELDS || (info)->compression == BI_ALPHABITS))
#define has_color_profile(info) ((info)->version == 108 || (info)->version == 124)

#define palette_entry_size(info) ((info)->version == 12 ? 3 : 4)

static const unsigned char *
bmp_read_file_header(fz_context *ctx, struct info *info, const unsigned char *begin, const unsigned char *end, const unsigned char *p)
{
	if (end - p < 14)
		fz_throw(ctx, FZ_ERROR_FORMAT, "premature end in file header in bmp image");

	if (!is_bitmap(p))
		fz_throw(ctx, FZ_ERROR_FORMAT, "invalid signature %02x%02x in bmp image", p[0], p[1]);

	info->type[0] = read8(p + 0);
	info->type[1] = read8(p + 1);
	/* read32(p+2) == file or header size */
	/* read16(p+6) == hotspot x for icons/cursors */
	/* read16(p+8) == hotspot y for icons/cursors */
	info->bitmapoffset = read32(p + 10);

	return p + 14;
}

static unsigned char *
bmp_decompress_huffman1d(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char **end)
{
	fz_stream *encstm, *decstm;
	fz_buffer *buf;
	unsigned char *decoded;
	size_t size;

	encstm = fz_open_memory(ctx, p, *end - p);

	fz_var(decstm);
	fz_var(buf);

	fz_try(ctx)
	{
		decstm = fz_open_faxd(ctx, encstm,
			0, /* 1 dimensional encoding */
			0, /* end of line not required */
			0, /* encoded byte align */
			info->width, info->height,
			0, /* end of block expected */
			1 /* black is 1 */
		);
		buf = fz_read_all(ctx, decstm, 1024);
		size = fz_buffer_extract(ctx, buf, &decoded);
		*end = decoded + size;
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_drop_stream(ctx, decstm);
		fz_drop_stream(ctx, encstm);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return decoded;
}

static unsigned char *
bmp_decompress_rle24(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char **end)
{
	const unsigned char *sp;
	unsigned char *dp, *ep, *decompressed;
	uint32_t width = info->width;
	uint32_t height = info->height;
	uint32_t stride;
	uint32_t x, y;
	int i;

	stride = (width*3 + 3) / 4 * 4;

	sp = p;
	dp = decompressed = fz_calloc(ctx, height, stride);
	ep = dp + height * stride;
	x = 0;
	y = 0;

	while (sp + 2 <= *end)
	{
		if (sp[0] == 0 && sp[1] == 0)
		{ /* end of line */
			sp += 2;
			x = 0;
			y++;
		}
		else if (sp[0] == 0 && sp[1] == 1)
		{ /* end of bitmap */
			sp += 2;
			break;
		}
		else if (sp[0] == 0 && sp[1] == 2)
		{ /* delta */
			sp += 2;
			x += sp < *end ? *sp++ : 0;
			y += sp < *end ? *sp++ : 0;
		}
		else if (sp[0] == 0 && sp[1] >= 3)
		{ /* absolute */
			int dn, sn, pad;
			dn = sp[1];
			sn = (dn * 3 + 1) / 2 * 2;
			pad = sn & 1;
			sp += 2;
			if (sn > *end - sp)
			{
				fz_warn(ctx, "premature end of pixel data in absolute code in bmp image");
				sn = ((*end - sp) / 3) * 3;
				pad = (*end - sp) % 3;
				dn = sn / 3;
			}
			else if (sn + pad > *end - sp)
			{
				fz_warn(ctx, "premature end of padding in absolute code in bmp image");
				pad = 0;
			}
			for (i = 0; i < dn; i++)
			{
				uint32_t actualx = x;
				uint32_t actualy = y;
				if (actualx >= width || actualy >= height)
				{
					actualx = x % width;
					actualy = y + x / width;
				}
				if (actualx < width && actualy < height)
				{
					dp = decompressed + actualy * stride + actualx * 3;
					*dp++ = sp[i * 3 + 0];
					*dp++ = sp[i * 3 + 1];
					*dp++ = sp[i * 3 + 2];
				}
				x++;
			}
			sp += sn + pad;
		}
		else
		{ /* encoded */
			int dn, sn;
			dn = sp[0];
			sn = 3;
			sp++;
			if (sn > *end - sp)
			{
				fz_warn(ctx, "premature end of pixel data in encoded code in bmp image");
				sn = 0;
				dn = 0;
			}
			for (i = 0; i < dn; i++)
			{
				uint32_t actualx = x;
				uint32_t actualy = y;
				if (actualx >= width || actualy >= height)
				{
					actualx = x % width;
					actualy = y + x / width;
				}
				if (actualx < width && actualy < height)
				{
					dp = decompressed + actualy * stride + actualx * 3;
					*dp++ = sp[0];
					*dp++ = sp[1];
					*dp++ = sp[2];
				}
				x++;
			}
			sp += sn;
		}
	}

	info->compression = BI_NONE;
	info->bitcount = 24;
	*end = ep;
	return decompressed;
}

static unsigned char *
bmp_decompress_rle8(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char **end)
{
	const unsigned char *sp;
	unsigned char *dp, *ep, *decompressed;
	uint32_t width = info->width;
	uint32_t height = info->height;
	uint32_t stride;
	uint32_t x, y;
	int i;

	stride = (width + 3) / 4 * 4;

	sp = p;
	dp = decompressed = fz_calloc(ctx, height, stride);
	ep = dp + height * stride;
	x = 0;
	y = 0;

	while (sp + 2 <= *end)
	{
		if (sp[0] == 0 && sp[1] == 0)
		{ /* end of line */
			sp += 2;
			x = 0;
			y++;
		}
		else if (sp[0] == 0 && sp[1] == 1)
		{ /* end of bitmap */
			sp += 2;
			break;
		}
		else if (sp[0] == 0 && sp[1] == 2)
		{ /* delta */
			sp +=2;
			x += sp < *end ? *sp++ : 0;
			y += sp < *end ? *sp++ : 0;
		}
		else if (sp[0] == 0 && sp[1] >= 3)
		{ /* absolute */
			int dn, sn, pad;
			dn = sp[1];
			sn = dn;
			pad = sn & 1;
			sp += 2;
			if (sn > *end - sp)
			{
				fz_warn(ctx, "premature end of pixel data in absolute code in bmp image");
				sn = *end - sp;
				pad = 0;
				dn = sn;
			}
			else if (sn + pad > *end - sp)
			{
				fz_warn(ctx, "premature end of padding in absolute code in bmp image");
				pad = 0;
			}
			for (i = 0; i < dn; i++)
			{
				uint32_t actualx = x;
				uint32_t actualy = y;
				if (actualx >= width || actualy >= height)
				{
					actualx = x % width;
					actualy = y + x / width;
				}
				if (actualx < width && actualy < height)
				{
					dp = decompressed + actualy * stride + actualx;
					*dp++ = sp[i];
				}
				x++;
			}
			sp += sn + pad;
		}
		else
		{ /* encoded */
			int dn, sn;
			dn = sp[0];
			sn = 1;
			sp++;
			for (i = 0; i < dn; i++)
			{
				uint32_t actualx = x;
				uint32_t actualy = y;
				if (actualx >= width || actualy >= height)
				{
					actualx = x % width;
					actualy = y + x / width;
				}
				if (actualx < width && actualy < height)
				{
					dp = decompressed + actualy * stride + actualx;
					*dp++ = sp[0];
				}
				x++;
			}
			sp += sn;
		}
	}

	info->compression = BI_NONE;
	info->bitcount = 8;
	*end = ep;
	return decompressed;
}

static unsigned char *
bmp_decompress_rle4(fz_context *ctx, struct info *info, const unsigned char *p, const unsigned char **end)
{
	const unsigned char *sp;
	unsigned char *dp, *ep, *decompressed;
	uint32_t width = info->width;
	uint32_t height = info->height;
	uint32_t stride;
	uint32_t x, y;
	int i;

	stride = ((width + 1) / 2 + 3) / 4 * 4;

	sp = p;
	dp = decompressed = fz_calloc(ctx, height, stride);
	ep = dp + height * stride;
	x = 0;
	y = 0;

	while (sp + 2 <= *end)
	{
		if (sp[0] == 0 && sp[1] == 0)
		{ /* end of line */
			sp += 2;
			x = 0;
			y++;
		}
		else if (sp[0] == 0 && sp[1] == 1)
		{ /* end of bitmap */
			sp += 2;
			break;
		}
		else if (sp[0] == 0 && sp[1] == 2)
		{ /* delta */
			sp += 2;
			x += sp < *end ? *sp++ : 0;
			y += sp < *end ? *sp++ : 0;
		}
		else if (sp[0] == 0 && sp[1] >= 3)
		{ /* absolute */
			int dn, sn, pad;
			dn = sp[1];
			sn = (dn + 1) / 2;
			pad = sn & 1;
			sp += 2;
			if (sn > *end - sp)
			{
				fz_warn(ctx, "premature end of pixel data in absolute code in bmp image");
				sn = *end - sp;
				pad = 0;
				dn = sn * 2;
			}
			else if (sn + pad > *end - sp)
			{
				fz_warn(ctx, "premature end of padding in absolute code in bmp image");
				pad = 0;
			}
			for (i = 0; i < dn; i++)
			{
				uint32_t actualx = x;
				uint32_t actualy = y;
				if (actualx >= width || actualy >= height)
				{
					actualx = x % width;
					actualy = y + x / width;
				}
				if (actualx < width && actualy < height)
				{
					int val = i & 1 ? (sp[i >> 1]) & 0xF : (sp[i >> 1] >> 4) & 0xF;
					dp = decompressed + actualy * stride + actualx / 2;
					if (x & 1)
						*dp++ |= val;
					else
						*dp |= val << 4;
				}
				x++;
			}
			sp += sn + pad;
		}
		else
		{ /* encoded */
			int dn, sn;
			dn = sp[0];
			sn = 1;
			sp++;
			for (i = 0; i < dn; i++)
			{
				uint32_t actualx = x;
				uint32_t actualy = y;
				if (actualx >= width || actualy >= height)
				{
					actualx = x % width;
					actualy = y + x / width;
				}
				if (actualx < width && actualy < height)
				{
					int val = i & 1 ? (sp[0] & 0xf) : (sp[0] >> 4) & 0xf;
					dp = decompressed + actualy * stride + actualx / 2;
					if (x & 1)
						*dp++ |= val;
					else
						*dp |= val << 4;
				}
				x++;
			}
			sp += sn;
		}
	}

	info->compression = BI_NONE;
	info->bitcount = 4;
	*end = ep;
	return decompressed;
}

static fz_pixmap *
bmp_read_bitmap(fz_context *ctx, struct info *info, const unsigned char *begin, const unsigned char *end, const unsigned char *p)
{
	const unsigned int mults[] = { 0, 8191, 2730, 1170, 546, 264, 130, 64 };
	fz_pixmap *pix;
	const unsigned char *ssp;
	unsigned char *ddp;
	unsigned char *decompressed = NULL;
	uint32_t bitcount;
	uint32_t width;
	int32_t height;
	uint32_t sstride;
	int32_t dstride;
	unsigned int rmult, gmult, bmult, amult;
	unsigned int rtrunc, gtrunc, btrunc, atrunc;
	uint32_t x;
	int32_t y;

	assert(info->width > 0 && info->width <= SHRT_MAX);
	assert(info->height > 0 && info->height <= SHRT_MAX);

	if (info->compression == BI_NONE)
		ssp = p;
	else if (info->compression == BI_RLE4)
		ssp = decompressed = bmp_decompress_rle4(ctx, info, p, &end);
	else if (info->compression == BI_RLE8)
		ssp = decompressed = bmp_decompress_rle8(ctx, info, p, &end);
	else if (is_win_bmp(info) && (info->compression == BI_BITFIELDS || info->compression == BI_ALPHABITS))
		ssp = p;
	else if (is_os2_bmp(info) && info->compression == BI_RLE24)
		ssp = decompressed = bmp_decompress_rle24(ctx, info, p, &end);
	else if (is_os2_bmp(info) && info->compression == BI_HUFFMAN1D)
		ssp = decompressed = bmp_decompress_huffman1d(ctx, info, p, &end);
	else
		fz_throw(ctx, FZ_ERROR_FORMAT, "unhandled compression (%u)  in bmp image", info->compression);

	bitcount = info->bitcount;
	width = info->width;
	height = info->height;

	sstride = ((width * bitcount + 31) / 32) * 4;
	if (ssp + sstride * height > end)
	{
		int32_t h = (end - ssp) / sstride;
		if (h == 0 || h > SHRT_MAX)
		{
			fz_free(ctx, decompressed);
			fz_throw(ctx, FZ_ERROR_LIMIT, "image dimensions out of range in bmp image");
		}
	}

	fz_try(ctx)
	{
		pix = fz_new_pixmap(ctx, info->cs, width, height, NULL, 1);
		fz_set_pixmap_resolution(ctx, pix, info->xres, info->yres);
		fz_clear_pixmap(ctx, pix);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, decompressed);
		fz_rethrow(ctx);
	}

	ddp = pix->samples;
	dstride = pix->stride;
	if (!info->topdown)
	{
		ddp = pix->samples + ((size_t) (height - 1)) * ((size_t) dstride);
		dstride = -dstride;
	}

	if (ssp + sstride * height > end)
	{
		fz_warn(ctx, "premature end in bitmap data in bmp image");
		height = (end - ssp) / sstride;
	}

	/* These are only used for 16- and 32-bit components
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
#ifdef BMP_DEBUG
	fz_warn(ctx, "rbits = %2d mult = %2d trunc = %2d", info->rbits, rmult, rtrunc);
	fz_warn(ctx, "gbits = %2d mult = %2d trunc = %2d", info->gbits, gmult, gtrunc);
	fz_warn(ctx, "bbits = %2d mult = %2d trunc = %2d", info->bbits, bmult, btrunc);
	fz_warn(ctx, "abits = %2d mult = %2d trunc = %2d", info->abits, amult, atrunc);
#endif

	for (y = 0; y < height; y++)
	{
		const unsigned char *sp = ssp + ((size_t) y) * ((size_t) sstride);
		unsigned char *dp = ddp + ((size_t) y) * ((size_t) dstride);

		switch (bitcount)
		{
		case 64:
			for (x = 0; x < width; x++)
			{
				uint32_t a = (((uint16_t)sp[7]) << 8) | (((uint16_t)sp[6]) << 0);
				uint32_t r = (((uint16_t)sp[5]) << 8) | (((uint16_t)sp[4]) << 0);
				uint32_t g = (((uint16_t)sp[3]) << 8) | (((uint16_t)sp[2]) << 0);
				uint32_t b = (((uint16_t)sp[1]) << 8) | (((uint16_t)sp[0]) << 0);
				r = (r * 255 + 4096) >> 13;
				g = (g * 255 + 4096) >> 13;
				b = (b * 255 + 4096) >> 13;
				a = (a * 255 + 4096) >> 13;
				*dp++ = r;
				*dp++ = g;
				*dp++ = b;
				*dp++ = a;
				sp += 8;
			}
			break;
		case 32:
			for (x = 0; x < width; x++)
			{
				uint32_t sample =
					(((uint32_t) sp[3]) << 24) |
					(((uint32_t) sp[2]) << 16) |
					(((uint32_t) sp[1]) <<  8) |
					(((uint32_t) sp[0]) <<  0);
				uint32_t r = (sample & info->rmask) >> info->rshift;
				uint32_t g = (sample & info->gmask) >> info->gshift;
				uint32_t b = (sample & info->bmask) >> info->bshift;
				uint32_t a = info->abits == 0 ? 255 : (sample & info->amask) >> info->ashift;
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
				uint16_t sample =
					(((uint16_t)sp[1]) << 8) |
					(((uint16_t)sp[0]) << 0);
				uint16_t r = (sample & info->rmask) >> info->rshift;
				uint16_t g = (sample & info->gmask) >> info->gshift;
				uint16_t b = (sample & info->bmask) >> info->bshift;
				uint16_t a = (sample & info->amask) >> info->ashift;
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

static fz_colorspace *
bmp_read_color_profile(fz_context *ctx, struct info *info, const unsigned char *begin, const unsigned char *end)
{
	if (info->colorspacetype == 0)
	{
		float matrix[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
		float wp[3] = { 0.95047f, 1.0f, 1.08883f }; /* D65 white point */
		float bp[3] = { 0, 0, 0 };
		float gamma[3] = { 1, 1, 1 };
		int i;

		for (i = 0; i < 3; i++)
			gamma[i] = (float) info->gamma[i] / (float) (1 << 16);
		for (i = 0; i < 9; i++)
			matrix[i] = (float) info->endpoints[i] / (float) (1 << 30);

#ifdef BMP_DEBUG
		fz_warn(ctx, "wp = %.6f %.6f %.6f", wp[0], wp[1], wp[2]);
		fz_warn(ctx, "bp = %.6f %.6f %.6f", bp[0], bp[1], bp[2]);
		fz_warn(ctx, "endpoints = %.6f %.6f %.6f", matrix[0], matrix[1], matrix[2]);
		fz_warn(ctx, "endpoints = %.6f %.6f %.6f", matrix[3], matrix[4], matrix[5]);
		fz_warn(ctx, "endpoints = %.6f %.6f %.6f", matrix[6], matrix[7], matrix[8]);
		fz_warn(ctx, "gamma = %.6f %.6f %.6f", gamma[0], gamma[1], gamma[2]);
#endif

		return fz_new_cal_rgb_colorspace(ctx, wp, bp, gamma, matrix);
	}
	else if (info->colorspacetype == 0x4c494e4b)
	{
		fz_warn(ctx, "ignoring linked color profile in bmp image");
		return NULL;
	}
	else if (info->colorspacetype == 0x57696e20)
	{
		fz_warn(ctx, "ignoring windows color profile in bmp image");
		return NULL;
	}
	else if (info->colorspacetype == 0x4d424544)
	{
		fz_buffer *profile;
		fz_colorspace *cs;

		if ((uint32_t)(end - begin) <= info->profileoffset)
		{
			fz_warn(ctx, "ignoring color profile located outside bmp image");
			return NULL;
		}
		if ((uint32_t)(end - begin) - info->profileoffset < info->profilesize)
		{
			fz_warn(ctx, "ignoring truncated color profile in bmp image");
			return NULL;
		}
		if (info->profilesize == 0)
		{
			fz_warn(ctx, "ignoring color profile without data in bmp image");
			return NULL;
		}

		profile = fz_new_buffer_from_copied_data(ctx, begin + info->profileoffset, info->profilesize);

		fz_try(ctx)
			cs = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_RGB, 0, "BMPRGB", profile);
		fz_always(ctx)
			fz_drop_buffer(ctx, profile);
		fz_catch(ctx)
			fz_rethrow(ctx);

		return cs;
	}
	else if (info->colorspacetype == 0x73524742)
	{
		return fz_keep_colorspace(ctx, fz_device_rgb(ctx));
	}

	fz_warn(ctx, "ignoring color profile with unknown type in bmp image");
	return NULL;
}

static void
compute_mask_info(unsigned int mask, unsigned int *shift, unsigned int *bits)
{
	*bits = 0;
	*shift = 0;

	while (mask && (mask & 1) == 0) {
		*shift += 1;
		mask >>= 1;
	}
	while (mask && (mask & 1) == 1) {
		*bits += 1;
		mask >>= 1;
	}
}

static const unsigned char *
bmp_read_color_masks(fz_context *ctx, struct info *info, const unsigned char *begin, const unsigned char *end, const unsigned char *p)
{
	int size = 0;

	if (info->compression == BI_BITFIELDS)
	{
		size = 12;
		if (end - p < 12)
			fz_throw(ctx, FZ_ERROR_FORMAT, "premature end in mask header in bmp image");

		info->rmask = read32(p + 0);
		info->gmask = read32(p + 4);
		info->bmask = read32(p + 8);
	}
	else if (info->compression == BI_ALPHABITS)
	{
		size = 16;
		if (end - p < 16)
			fz_throw(ctx, FZ_ERROR_FORMAT, "premature end in mask header in bmp image");

		info->rmask = read32(p + 0);
		info->gmask = read32(p + 4);
		info->bmask = read32(p + 8);
		info->amask = read32(p + 12);
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
bmp_read_palette(fz_context *ctx, struct info *info, const unsigned char *begin, const unsigned char *end, const unsigned char *p)
{
	int i, expected, present, entry_size;

	entry_size = palette_entry_size(info);

	if (info->colors == 0)
		expected = info->colors = 1 << info->bitcount;
	else
		expected = fz_mini(info->colors, 1 << info->bitcount);

	if (info->bitmapoffset == 0)
		present = fz_mini(expected, (end - p) / entry_size);
	else
		present = fz_mini(expected, (begin + info->bitmapoffset - p) / entry_size);

	for (i = 0; i < present; i++)
	{
		/* ignore alpha channel even if present */
		info->palette[3 * i + 0] = read8(p + i * entry_size + 2);
		info->palette[3 * i + 1] = read8(p + i * entry_size + 1);
		info->palette[3 * i + 2] = read8(p + i * entry_size + 0);
	}

	if (present < expected)
		bmp_load_default_palette(ctx, info, present);

	return p + present * entry_size;
}

static const unsigned char *
bmp_read_info_header(fz_context *ctx, struct info *info, const unsigned char *begin, const unsigned char *end, const unsigned char *p)
{
	uint32_t size;

	if (end - p < 4)
		fz_throw(ctx, FZ_ERROR_FORMAT, "premature end in info header in bmp image");
	size = info->version = read32(p + 0);

	if (!is_win_bmp(info) && !is_os2_bmp(info))
		fz_throw(ctx, FZ_ERROR_FORMAT, "unknown header version (%u) in bmp image", info->version);
	if ((uint32_t)(end - p) < size)
		fz_throw(ctx, FZ_ERROR_FORMAT, "premature end in info header in bmp image");

	/* default compression */
	info->compression = BI_NONE;

	/* OS/2 1.x or Windows v2 */
	if (size == 12)
	{
		/* read32(p+0) == header size */
		info->width = read16(p + 4);
		info->height = read16(p + 6);
		/* read16(p+8) == planes */
		info->bitcount = read16(p + 10);
	}
	/* OS/2 2.x short header */
	if (size >= 16)
	{
		/* read32(p+0) == header size */
		info->width = read32(p + 4);
		info->height = read32(p + 8);
		/* read16(p+12) == planes */
		info->bitcount = read16(p + 14);
	}

	/* default masks */
	if (info->bitcount == 16)
	{
		info->rmask = 0x00007c00;
		info->gmask = 0x000003e0;
		info->bmask = 0x0000001f;
		info->amask = 0x00000000;
	}
	else if (info->bitcount == 24 || info->bitcount == 32)
	{
		info->rmask = 0x00ff0000;
		info->gmask = 0x0000ff00;
		info->bmask = 0x000000ff;
		info->amask = 0x00000000;
	}

	/* Windows v3 header */
	if (size >= 40)
	{
		info->compression = read32(p + 16);
		info->bitmapsize = read32(p + 20);
		info->xres = read32(p + 24);
		info->yres = read32(p + 28);
		info->colors = read32(p + 32);
		if (info->bitcount >= 32)
		{
			if (info->colors != 0)
				fz_warn(ctx, "Suspect BMP header; bitcount=%d, colors=%d", info->bitcount, info->colors);
			info->colors = 0;
		}
		else if (info->colors > (1U<<info->bitcount))
		{
			fz_warn(ctx, "Suspect BMP header; bitcount=%d, colors=%d", info->bitcount, info->colors);
			info->colors = 1<<info->bitcount;
		}
		/* read32(p+36) == important colors */
	}
	/* Windows v3 header with RGB masks */
	if (size == 52 || size == 56 || size == 108 || size == 124)
	{
		info->rmask = read32(p + 40);
		info->gmask = read32(p + 44);
		info->bmask = read32(p + 48);
	}
	/* Windows v3 header with RGBA masks */
	if (size == 56 || size == 108 || size == 124)
	{
		info->amask = read32(p + 52);
	}
	/* OS/2 2.x long header */
	if (size == 64)
	{
		/* read16(p+40) == units */
		/* read16(p+42) == reserved */
		/* read16(p+44) == recording */
		/* read16(p+46) == rendering */
		/* read32(p+48) == size1 */
		/* read32(p+52) == size2 */
		/* read32(p+56) == color encoding */
		/* read32(p+60) == identifier */
	}
	/* Windows v4 header */
	if (size >= 108)
	{
		info->colorspacetype = read32(p + 56);

		info->endpoints[0] = read32(p + 60);
		info->endpoints[1] = read32(p + 64);
		info->endpoints[2] = read32(p + 68);

		info->endpoints[3] = read32(p + 72);
		info->endpoints[4] = read32(p + 76);
		info->endpoints[5] = read32(p + 80);

		info->endpoints[6] = read32(p + 84);
		info->endpoints[7] = read32(p + 88);
		info->endpoints[8] = read32(p + 92);

		info->gamma[0] = read32(p + 96);
		info->gamma[1] = read32(p + 100);
		info->gamma[2] = read32(p + 104);
	}
	/* Windows v5 header */
	if (size >= 124)
	{
		info->intent = read32(p + 108);
		info->profileoffset = read32(p + 112);
		info->profilesize = read32(p + 116);
		/* read32(p+120) == reserved */
	}

	return p + size;
}


static fz_pixmap *
bmp_read_image(fz_context *ctx, struct info *info, const unsigned char *begin, const unsigned char *end, const unsigned char *p, int only_metadata)
{
	const unsigned char *profilebegin;

	memset(info, 0x00, sizeof (*info));
	info->colorspacetype = 0xffffffff;

	p = profilebegin = bmp_read_file_header(ctx, info, begin, end, p);

	p = bmp_read_info_header(ctx, info, begin, end, p);

	/* clamp bitmap offset to buffer size */
	if (info->bitmapoffset < (uint32_t)(p - begin))
		info->bitmapoffset = (uint32_t)(p - begin);
	if ((uint32_t)(end - begin) < info->bitmapoffset)
		info->bitmapoffset = end - begin;

	if (has_palette(info))
		p = bmp_read_palette(ctx, info, begin, end, p);

	if (has_color_masks(info))
		p = bmp_read_color_masks(ctx, info, begin, end, p);

	info->xres = DPM_TO_DPI(info->xres);
	info->yres = DPM_TO_DPI(info->yres);

	/* extract topdown/bottomup from height for windows bitmaps */
	if (is_win_bmp(info))
	{
		int bits = info->version == 12 ? 16 : 32;

		info->topdown = (info->height >> (bits - 1)) & 1;
		if (info->topdown)
		{
			info->height--;
			info->height = ~info->height;
			info->height &= bits == 16 ? 0xffff : 0xffffffff;
		}
	}

	/* GIMP incorrectly writes BMP v5 headers that omit color masks
	but include colorspace information. This means they look like
	BMP v4 headers and that we interpret the colorspace information
	partially as color mask data, partially as colorspace information.
	Let's work around this... */
	if (info->version == 108 &&
			info->rmask == 0x73524742 && /* colorspacetype */
			info->gmask == 0x00000000 && /* endpoints[0] */
			info->bmask == 0x00000000 && /* endpoints[1] */
			info->amask == 0x00000000 && /* endpoints[2] */
			info->colorspacetype == 0x00000000 && /* endpoints[3] */
			info->endpoints[0] == 0x00000000 && /* endpoints[4] */
			info->endpoints[1] == 0x00000000 && /* endpoints[5] */
			info->endpoints[2] == 0x00000000 && /* endpoints[6] */
			info->endpoints[3] == 0x00000000 && /* endpoints[7] */
			info->endpoints[4] == 0x00000000 && /* endpoints[8] */
			info->endpoints[5] == 0x00000000 && /* gamma[0] */
			info->endpoints[6] == 0x00000000 && /* gamma[1] */
			info->endpoints[7] == 0x00000000 && /* gamma[2] */
			info->endpoints[8] == 0x00000002) /* intent */
	{
		info->rmask = 0;
		/* default masks */
		if (info->bitcount == 16)
		{
			info->rmask = 0x00007c00;
			info->gmask = 0x000003e0;
			info->bmask = 0x0000001f;
			info->amask = 0x00000000;
		}
		else if (info->bitcount >= 24)
		{
			info->rmask = 0x00ff0000;
			info->gmask = 0x0000ff00;
			info->bmask = 0x000000ff;
			info->amask = 0x00000000;
		}

		info->colorspacetype = 0x73524742;
		info->intent = 0x00000002;
	}

	/* get number of bits per component and component shift */
	compute_mask_info(info->rmask, &info->rshift, &info->rbits);
	compute_mask_info(info->gmask, &info->gshift, &info->gbits);
	compute_mask_info(info->bmask, &info->bshift, &info->bbits);
	compute_mask_info(info->amask, &info->ashift, &info->abits);

#ifdef BMP_DEBUG
	{
		#define chr(c) (((c) >= ' ' && (c) <= '~') ? (c) : '?')
		fz_warn(ctx, "type = %02x%02x %c%c", info->type[0], info->type[1], chr(info->type[0]), chr(info->type[1]));
		if (is_bitmap_array(info->type)) fz_warn(ctx, "\tbitmap array");
		if (is_bitmap(info->type)) fz_warn(ctx, "\tbitmap");
		fz_warn(ctx, "version = %zu", (size_t) info->version);
		if (is_os2_bmp(info)) fz_warn(ctx, "OS/2 bmp");
		if (is_win_bmp(info)) fz_warn(ctx, "Windows bmp");
		fz_warn(ctx, "bitmapoffset = %zu", (size_t) info->bitmapoffset);
		fz_warn(ctx, "width = %zu", (size_t) info->width);
		fz_warn(ctx, "height = %zu", (size_t) info->height);
		fz_warn(ctx, "bitcount = %zu", (size_t) info->bitcount);
		fz_warn(ctx, "compression = %zu", (size_t) info->compression);
		if (info->compression == BI_NONE) fz_warn(ctx, "\tNone");
		if (info->compression == BI_RLE8) fz_warn(ctx, "\tRLE 8");
		if (info->compression == BI_RLE4) fz_warn(ctx, "\tRLE 4");
		if (is_valid_win_compression(info) && info->compression == BI_BITFIELDS) fz_warn(ctx, "\tBITFIELDS");
		if (is_valid_os2_compression(info) && info->compression == BI_HUFFMAN1D) fz_warn(ctx, "\tHUFFMAN1D");
		if (info->compression == BI_JPEG) fz_warn(ctx, "\tJPEG");
		if (info->compression == BI_RLE24) fz_warn(ctx, "\tRLE24");
		if (info->compression == BI_PNG) fz_warn(ctx, "\tPNG");
		if (info->compression == BI_ALPHABITS) fz_warn(ctx, "\tALPHABITS");
		fz_warn(ctx, "bitmapsize = %zu", (size_t) info->bitmapsize);
		fz_warn(ctx, "xres = %zu", (size_t) info->xres);
		fz_warn(ctx, "yres = %zu", (size_t) info->yres);
		fz_warn(ctx, "colors = %zu", (size_t) info->colors);
		fz_warn(ctx, "rmask = 0x%08zx rshift = %d rbits = %d", (size_t) info->rmask, info->rshift, info->rbits);
		fz_warn(ctx, "gmask = 0x%08zx gshift = %d gbits = %d", (size_t) info->gmask, info->gshift, info->gbits);
		fz_warn(ctx, "bmask = 0x%08zx bshift = %d bbits = %d", (size_t) info->bmask, info->bshift, info->bbits);
		fz_warn(ctx, "amask = 0x%08zx ashift = %d abits = %d", (size_t) info->amask, info->ashift, info->abits);
		fz_warn(ctx, "colorspacetype = %08zx %c%c%c%c", (size_t) info->colorspacetype,
		chr((info->colorspacetype >> 24) & 0xff),
		chr((info->colorspacetype >> 16) & 0xff),
		chr((info->colorspacetype >>  8) & 0xff),
		chr((info->colorspacetype >>  0) & 0xff));
		fz_warn(ctx, "endpoints[%d] = 0x%08zx 0x%08zx 0x%08zx", 0, (size_t) info->endpoints[0], (size_t) info->endpoints[1], (size_t) info->endpoints[2]);
		fz_warn(ctx, "endpoints[%d] = 0x%08zx 0x%08zx 0x%08zx", 3, (size_t) info->endpoints[3], (size_t) info->endpoints[4], (size_t) info->endpoints[5]);
		fz_warn(ctx, "endpoints[%d] = 0x%08zx 0x%08zx 0x%08zx", 6, (size_t) info->endpoints[6], (size_t) info->endpoints[7], (size_t) info->endpoints[8]);
		fz_warn(ctx, "gamma = 0x%08zx 0x%08zx 0x%08zx", (size_t) info->gamma[0], (size_t) info->gamma[1], (size_t) info->gamma[2]);
		fz_warn(ctx, "profileoffset = %zu", (size_t) info->profileoffset);
		fz_warn(ctx, "profilesize = %zu", (size_t) info->profilesize);
		#undef chr
	}
#endif

	if (info->width == 0 || info->width > SHRT_MAX || info->height == 0 || info->height > SHRT_MAX)
		fz_throw(ctx, FZ_ERROR_LIMIT, "image dimensions (%u x %u) out of range in bmp image", info->width, info->height);
	if (!is_valid_compression(info))
		fz_throw(ctx, FZ_ERROR_FORMAT, "unsupported compression method (%u) in bmp image", info->compression);
	if (!is_valid_bitcount(info))
		fz_throw(ctx, FZ_ERROR_FORMAT, "invalid bits per pixel (%u) for compression (%u) in bmp image", info->bitcount, info->compression);
	if (info->rbits > info->bitcount)
		fz_throw(ctx, FZ_ERROR_FORMAT, "unsupported %u bit red mask in bmp image", info->rbits);
	if (info->gbits > info->bitcount)
		fz_throw(ctx, FZ_ERROR_FORMAT, "unsupported %u bit green mask in bmp image", info->gbits);
	if (info->bbits > info->bitcount)
		fz_throw(ctx, FZ_ERROR_FORMAT, "unsupported %u bit blue mask in bmp image", info->bbits);
	if (info->abits > info->bitcount)
		fz_throw(ctx, FZ_ERROR_FORMAT, "unsupported %u bit alpha mask in bmp image", info->abits);

	/* Read color profile or default to RGB */
	if (has_color_profile(info))
		info->cs = bmp_read_color_profile(ctx, info, profilebegin, end);
	if (!info->cs)
		info->cs = fz_keep_colorspace(ctx, fz_device_rgb(ctx));

	if (only_metadata)
		return NULL;

	/* bitmap cannot begin before headers have ended */
	if ((uint32_t)(p - begin) < info->bitmapoffset)
		p = begin + info->bitmapoffset;

	if (is_win_bmp(info) && info->compression == BI_JPEG)
	{
		if ((uint32_t)(end - p) < info->bitmapsize)
			fz_warn(ctx, "premature end in jpeg image embedded in bmp image");
		return fz_load_jpeg(ctx, p, end - p);
	}
	else if (is_win_bmp(info) && info->compression == BI_PNG)
	{
		if ((uint32_t)(end - p) < info->bitmapsize)
			fz_warn(ctx, "premature end in png image embedded in bmp image");
		return fz_load_png(ctx, p, end - p);
	}
	else
		return bmp_read_bitmap(ctx, info, begin, end, p);
}

fz_pixmap *
fz_load_bmp_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage)
{
	const unsigned char *begin = buf;
	const unsigned char *end = buf + len;
	const unsigned char *p = begin;
	struct info info = { 0 };
	int nextoffset = 0;
	fz_pixmap *image = NULL;
	int origidx = subimage;

	(void) p;

	do
	{
		p = begin + nextoffset;

		if (end - p < 14)
			fz_throw(ctx, FZ_ERROR_FORMAT, "not enough data for bitmap array (%02x%02x) in bmp image", p[0], p[1]);

		if (is_bitmap_array(p))
		{
			/* read16(p+0) == type */
			/* read32(p+2) == size of this header in bytes */
			nextoffset = read32(p + 6);
			/* read16(p+10) == suitable pelx dimensions */
			/* read16(p+12) == suitable pely dimensions */
			p += 14;
			(void) p;
		}
		else if (is_bitmap(p))
		{
			nextoffset = 0;
		}
		else
		{
			fz_warn(ctx, "treating invalid subimage as end of file");
			nextoffset = 0;
		}

		if (end - begin < nextoffset)
		{
			fz_warn(ctx, "treating invalid next subimage offset as end of file");
			nextoffset = 0;
		}
		else
			subimage--;

	} while (subimage >= 0 && nextoffset > 0);

	if (subimage != -1)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "subimage index (%d) out of range in bmp image", origidx);

	fz_try(ctx)
		image = bmp_read_image(ctx, &info, begin, end, p, 0);
	fz_always(ctx)
		fz_drop_colorspace(ctx, info.cs);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return image;
}

void
fz_load_bmp_info_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep, int subimage)
{
	const unsigned char *begin = buf;
	const unsigned char *end = buf + len;
	const unsigned char *p = begin;
	struct info info = { 0 };
	int nextoffset = 0;
	int origidx = subimage;

	(void) p;

	do
	{
		p = begin + nextoffset;

		if (end - p < 14)
			fz_throw(ctx, FZ_ERROR_FORMAT, "not enough data for bitmap array (%02x%02x) in bmp image", p[0], p[1]);

		if (is_bitmap_array(p))
		{
			/* read16(p+0) == type */
			/* read32(p+2) == size of this header in bytes */
			nextoffset = read32(p + 6);
			/* read16(p+10) == suitable pelx dimensions */
			/* read16(p+12) == suitable pely dimensions */
			p += 14;
			(void) p;
		}
		else if (is_bitmap(p))
		{
			nextoffset = 0;
		}
		else
		{
			fz_warn(ctx, "treating invalid subimage as end of file");
			nextoffset = 0;
		}

		if (end - begin < nextoffset)
		{
			fz_warn(ctx, "treating invalid next subimage offset as end of file");
			nextoffset = 0;
		}
		else
			subimage--;

	} while (subimage >= 0 && nextoffset > 0);

	if (subimage != -1)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "subimage index (%d) out of range in bmp image", origidx);

	fz_try(ctx)
	{
		(void) bmp_read_image(ctx, &info, begin, end, p, 1);
		*cspacep = fz_keep_colorspace(ctx, info.cs);
		*wp = info.width;
		*hp = info.height;
		*xresp = info.xres;
		*yresp = info.yres;
	}
	fz_always(ctx)
		fz_drop_colorspace(ctx, info.cs);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

int
fz_load_bmp_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len)
{
	const unsigned char *begin = buf;
	const unsigned char *end = buf + len;
	uint32_t nextoffset = 0;
	int count = 0;

	do
	{
		const unsigned char *p = begin + nextoffset;

		if (end - p < 14)
			fz_throw(ctx, FZ_ERROR_FORMAT, "not enough data for bitmap array in bmp image");

		if (is_bitmap_array(p))
		{
			/* read16(p+0) == type */
			/* read32(p+2) == size of this header in bytes */
			nextoffset = read32(p + 6);
			/* read16(p+10) == suitable pelx dimensions */
			/* read16(p+12) == suitable pely dimensions */
			p += 14;
		}
		else if (is_bitmap(p))
		{
			nextoffset = 0;
		}
		else
		{
			fz_warn(ctx, "treating invalid subimage as end of file");
			nextoffset = 0;
		}

		if ((uint32_t) (end - begin) < nextoffset)
		{
			fz_warn(ctx, "treating invalid next subimage offset as end of file");
			nextoffset = 0;
		}
		else
			count++;

	} while (nextoffset > 0);

	return count;
}

fz_pixmap *
fz_load_bmp(fz_context *ctx, const unsigned char *p, size_t total)
{
	return fz_load_bmp_subimage(ctx, p, total, 0);
}

void
fz_load_bmp_info(fz_context *ctx, const unsigned char *p, size_t total, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_load_bmp_info_subimage(ctx, p, total, wp, hp, xresp, yresp, cspacep, 0);
}
