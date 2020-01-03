#include "mupdf/fitz.h"

#include <limits.h>
#include <assert.h>
#include <string.h>

/*
 * TIFF image loader. Should be enough to support TIFF files in XPS.
 * Baseline TIFF 6.0 plus CMYK, LZW, Flate and JPEG support.
 * Limited bit depths (1,2,4,8).
 * Limited planar configurations (1=chunky).
 * TODO: RGBPal images
 */

struct tiff
{
	/* "file" */
	const unsigned char *bp, *rp, *ep;

	/* byte order */
	unsigned order;

	/* offset of first ifd */
	unsigned *ifd_offsets;
	int ifds;

	/* where we can find the strips of image data */
	unsigned rowsperstrip;
	unsigned *stripoffsets;
	unsigned *stripbytecounts;
	unsigned stripoffsetslen;
	unsigned stripbytecountslen;

	/* where we can find the tiles of image data */
	unsigned tilelength;
	unsigned tilewidth;
	unsigned *tileoffsets;
	unsigned *tilebytecounts;
	unsigned tileoffsetslen;
	unsigned tilebytecountslen;

	/* colormap */
	unsigned *colormap;
	unsigned colormaplen;

	/* assorted tags */
	unsigned subfiletype;
	unsigned photometric;
	unsigned compression;
	unsigned imagewidth;
	unsigned imagelength;
	unsigned samplesperpixel;
	unsigned bitspersample;
	unsigned planar;
	unsigned extrasamples;
	unsigned xresolution;
	unsigned yresolution;
	unsigned resolutionunit;
	unsigned fillorder;
	unsigned g3opts;
	unsigned g4opts;
	unsigned predictor;

	unsigned ycbcrsubsamp[2];

	const unsigned char *jpegtables; /* point into "file" buffer */
	unsigned jpegtableslen;

	unsigned char *profile;
	int profilesize;

	/* decoded data */
	fz_colorspace *colorspace;
	unsigned char *samples;
	unsigned char *data;
	int tilestride;
	int stride;
};

enum
{
	TII = 0x4949, /* 'II' */
	TMM = 0x4d4d, /* 'MM' */
	TBYTE = 1,
	TASCII = 2,
	TSHORT = 3,
	TLONG = 4,
	TRATIONAL = 5
};

#define NewSubfileType 254
#define ImageWidth 256
#define ImageLength 257
#define BitsPerSample 258
#define Compression 259
#define PhotometricInterpretation 262
#define FillOrder 266
#define StripOffsets 273
#define SamplesPerPixel 277
#define RowsPerStrip 278
#define StripByteCounts 279
#define XResolution 282
#define YResolution 283
#define PlanarConfiguration 284
#define T4Options 292
#define T6Options 293
#define ResolutionUnit 296
#define Predictor 317
#define ColorMap 320
#define TileWidth 322
#define TileLength 323
#define TileOffsets 324
#define TileByteCounts 325
#define ExtraSamples 338
#define JPEGTables 347
#define YCbCrSubSampling 530
#define ICCProfile 34675

static const unsigned char bitrev[256] =
{
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

static inline int tiff_getcomp(unsigned char *line, int x, int bpc)
{
	switch (bpc)
	{
	case 1: return (line[x >> 3] >> ( 7 - (x & 7) ) ) & 1;
	case 2: return (line[x >> 2] >> ( ( 3 - (x & 3) ) << 1 ) ) & 3;
	case 4: return (line[x >> 1] >> ( ( 1 - (x & 1) ) << 2 ) ) & 15;
	case 8: return line[x];
	case 16: return line[x << 1] << 8 | line[(x << 1) + 1];
	}
	return 0;
}

static inline void tiff_putcomp(unsigned char *line, int x, int bpc, int value)
{
	int maxval = (1 << bpc) - 1;

	switch (bpc)
	{
	case 1: line[x >> 3] &= ~(maxval << (7 - (x & 7))); break;
	case 2: line[x >> 2] &= ~(maxval << ((3 - (x & 3)) << 1)); break;
	case 4: line[x >> 1] &= ~(maxval << ((1 - (x & 1)) << 2)); break;
	}

	switch (bpc)
	{
	case 1: line[x >> 3] |= value << (7 - (x & 7)); break;
	case 2: line[x >> 2] |= value << ((3 - (x & 3)) << 1); break;
	case 4: line[x >> 1] |= value << ((1 - (x & 1)) << 2); break;
	case 8: line[x] = value; break;
	case 16: line[x << 1] = value >> 8; line[(x << 1) + 1] = value & 0xFF; break;
	}
}

static void
tiff_unpredict_line(unsigned char *line, int width, int comps, int bits)
{
	unsigned char left[FZ_MAX_COLORS];
	int i, k, v;

	for (k = 0; k < comps; k++)
		left[k] = 0;

	for (i = 0; i < width; i++)
	{
		for (k = 0; k < comps; k++)
		{
			v = tiff_getcomp(line, i * comps + k, bits);
			v = v + left[k];
			v = v % (1 << bits);
			tiff_putcomp(line, i * comps + k, bits, v);
			left[k] = v;
		}
	}
}

static void
tiff_invert_line(unsigned char *line, int width, int comps, int bits, int alpha)
{
	int i, k, v;
	int m = (1 << bits) - 1;

	for (i = 0; i < width; i++)
	{
		for (k = 0; k < comps; k++)
		{
			v = tiff_getcomp(line, i * comps + k, bits);
			if (!alpha || k < comps - 1)
				v = m - v;
			tiff_putcomp(line, i * comps + k, bits, v);
		}
	}
}

static void
tiff_expand_colormap(fz_context *ctx, struct tiff *tiff)
{
	int maxval = 1 << tiff->bitspersample;
	unsigned char *samples;
	unsigned char *src, *dst;
	unsigned int x, y;
	unsigned int stride;

	/* colormap has first all red, then all green, then all blue values */
	/* colormap values are 0..65535, bits is 4 or 8 */
	/* image can be with or without extrasamples: comps is 1 or 2 */

	if (tiff->samplesperpixel != 1 && tiff->samplesperpixel != 2)
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid number of samples for RGBPal");

	if (tiff->bitspersample != 1 && tiff->bitspersample != 2 && tiff->bitspersample != 4 && tiff->bitspersample != 8 && tiff->bitspersample != 16)
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid number of bits for RGBPal");

	if (tiff->colormaplen < (unsigned)maxval * 3)
		fz_throw(ctx, FZ_ERROR_GENERIC, "insufficient colormap data");

	if (tiff->imagelength > UINT_MAX / tiff->imagewidth / (tiff->samplesperpixel + 2))
		fz_throw(ctx, FZ_ERROR_GENERIC, "image too large");

	stride = tiff->imagewidth * (tiff->samplesperpixel + 2);

	samples = Memento_label(fz_malloc(ctx, stride * tiff->imagelength), "tiff_samples");

	for (y = 0; y < tiff->imagelength; y++)
	{
		src = tiff->samples + (unsigned int)(tiff->stride * y);
		dst = samples + (unsigned int)(stride * y);

		for (x = 0; x < tiff->imagewidth; x++)
		{
			if (tiff->extrasamples)
			{
				int c = tiff_getcomp(src, x * 2, tiff->bitspersample);
				int a = tiff_getcomp(src, x * 2 + 1, tiff->bitspersample);
				*dst++ = tiff->colormap[c + 0] >> 8;
				*dst++ = tiff->colormap[c + maxval] >> 8;
				*dst++ = tiff->colormap[c + maxval * 2] >> 8;
				if (tiff->bitspersample <= 8)
					*dst++ = a << (8 - tiff->bitspersample);
				else
					*dst++ = a >> (tiff->bitspersample - 8);
			}
			else
			{
				int c = tiff_getcomp(src, x, tiff->bitspersample);
				*dst++ = tiff->colormap[c + 0] >> 8;
				*dst++ = tiff->colormap[c + maxval] >> 8;
				*dst++ = tiff->colormap[c + maxval * 2] >> 8;
			}
		}
	}

	tiff->samplesperpixel += 2;
	tiff->bitspersample = 8;
	tiff->stride = stride;
	fz_free(ctx, tiff->samples);
	tiff->samples = samples;
}

static unsigned
tiff_decode_data(fz_context *ctx, struct tiff *tiff, const unsigned char *rp, unsigned int rlen, unsigned char *wp, unsigned int wlen)
{
	fz_stream *encstm = NULL;
	fz_stream *stm = NULL;
	unsigned i, size = 0;
	unsigned char *reversed = NULL;
	fz_stream *jpegtables = NULL;
	int old_tiff;

	if (rp + rlen > tiff->ep)
		fz_throw(ctx, FZ_ERROR_GENERIC, "strip extends beyond the end of the file");

	/* the bits are in un-natural order */
	if (tiff->fillorder == 2)
	{
		reversed = fz_malloc(ctx, rlen);
		for (i = 0; i < rlen; i++)
			reversed[i] = bitrev[rp[i]];
		rp = reversed;
	}

	fz_var(jpegtables);
	fz_var(encstm);
	fz_var(stm);

	fz_try(ctx)
	{
		encstm = fz_open_memory(ctx, rp, rlen);

		/* switch on compression to create a filter */
		/* feed each chunk (strip or tile) to the filter */
		/* read out the data into a buffer */
		/* the level above packs the chunk's samples into a pixmap */

		/* type 32773 / packbits -- nothing special (same row-padding as PDF) */
		/* type 2 / ccitt rle -- no EOL, no RTC, rows are byte-aligned */
		/* type 3 and 4 / g3 and g4 -- each strip starts new section */
		/* type 5 / lzw -- each strip is handled separately */

		switch (tiff->compression)
		{
		case 1:
			/* stm already open and reading uncompressed data */
			stm = fz_keep_stream(ctx, encstm);
			break;
		case 2:
		case 3:
		case 4:
			stm = fz_open_faxd(ctx, encstm,
					tiff->compression == 4 ? -1 :
					tiff->compression == 2 ? 0 :
					(int) (tiff->g3opts & 1),
					0,
					tiff->compression == 2,
					tiff->imagewidth,
					tiff->imagelength,
					0,
					1);
			break;
		case 5:
			old_tiff = rp[0] == 0 && (rp[1] & 1);
			stm = fz_open_lzwd(ctx, encstm, old_tiff ? 0 : 1, 9, old_tiff ? 1 : 0, old_tiff);
			break;
		case 6:
			fz_warn(ctx, "deprecated JPEG in TIFF compression not fully supported");
			/* fall through */
		case 7:
			if (tiff->jpegtables && (int)tiff->jpegtableslen > 0)
				jpegtables = fz_open_memory(ctx, tiff->jpegtables, tiff->jpegtableslen);

			stm = fz_open_dctd(ctx, encstm,
					tiff->photometric == 2 || tiff->photometric == 3 ? 0 : -1,
					0,
					jpegtables);
			break;
		case 8:
		case 32946:
			stm = fz_open_flated(ctx, encstm, 15);
			break;
		case 32773:
			stm = fz_open_rld(ctx, encstm);
			break;
		case 34676:
			if (tiff->photometric == 32845)
				stm = fz_open_sgilog32(ctx, encstm, tiff->imagewidth);
			else
				stm = fz_open_sgilog16(ctx, encstm, tiff->imagewidth);
			break;
		case 34677:
			stm = fz_open_sgilog24(ctx, encstm, tiff->imagewidth);
			break;
		case 32809:
			if (tiff->bitspersample != 4)
				fz_throw(ctx, FZ_ERROR_GENERIC, "invalid bits per pixel in thunder encoding");
			stm = fz_open_thunder(ctx, encstm, tiff->imagewidth);
			break;
		default:
			fz_throw(ctx, FZ_ERROR_GENERIC, "unknown TIFF compression: %d", tiff->compression);
		}

		size = (unsigned)fz_read(ctx, stm, wp, wlen);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, jpegtables);
		fz_drop_stream(ctx, encstm);
		fz_drop_stream(ctx, stm);
		fz_free(ctx, reversed);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	return size;
}

static void
tiff_paste_tile(fz_context *ctx, struct tiff *tiff, unsigned char *tile, unsigned int row, unsigned int col)
{
	unsigned int x, y, k;

	for (y = 0; y < tiff->tilelength && row + y < tiff->imagelength; y++)
	{
		for (x = 0; x < tiff->tilewidth && col + x < tiff->imagewidth; x++)
		{
			for (k = 0; k < tiff->samplesperpixel; k++)
			{
				unsigned char *dst, *src;

				dst = tiff->samples;
				dst += (row + y) * tiff->stride;
				dst += (((col + x) * tiff->samplesperpixel + k) * tiff->bitspersample + 7) / 8;

				src = tile;
				src += y * tiff->tilestride;
				src += ((x * tiff->samplesperpixel + k) * tiff->bitspersample + 7) / 8;

				switch (tiff->bitspersample)
				{
				case 1: *dst |= (*src >> (7 - 1 * ((col + x) % 8))) & 0x1; break;
				case 2: *dst |= (*src >> (6 - 2 * ((col + x) % 4))) & 0x3; break;
				case 4: *dst |= (*src >> (4 - 4 * ((col + x) % 2))) & 0xf; break;
				case 8: *dst = *src; break;
				case 16: dst[0] = src[0]; dst[1] = src[1]; break;
				case 24: dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; break;
				case 32: dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3]; break;
				}
			}
		}
	}
}

static void
tiff_paste_subsampled_tile(fz_context *ctx, struct tiff *tiff, unsigned char *tile, unsigned len, unsigned tw, unsigned th, unsigned col, unsigned row)
{
	/*
	This explains how the samples are laid out in tiff data, the spec example is non-obvious.
	The y, cb, cr indicies follow the spec, i.e. y17 is the y sample at row 1, column 7.
	All indicies start at 0.

	hexlookup = (horizontalsubsampling & 0xf) << 4 | (verticalsubsampling & 0xf)

	0x11	y00 cb00 cr00	0x21	y00 y01 cb00 cr00	0x41	y00 y01 y02 y03 cb00 cr00
		y01 cb01 cr01		y10 y11 cb01 cr01		y04 y05 y06 y07 cb01 cr01
		....			...				...
		y10 cb10 cr10		y20 y21 cb10 cr10		y10 y11 y12 y13 cb10 cr10
		y11 cb11 cr11		y30 y31 cb11 cr11		y14 y15 y16 y17 cb11 cr11

	0x12	y00		0x22	y00 y01			0x42	y00 y01 y02 y03
		y10 cb00 cr00		y10 y11 cb00 cr00		y10 y11 y12 y13 cb00 cr00
		y01			y02 y03				y04 y05 y06 y07
		y11 cb01 cr01		y12 y13 cb01 cr01		y14 y15 y16 y17 cb01 cr01
		....			...				...
		y20			y20 y21				y20 y21 y22 y23
		y30 cb10 cr10		y30 y31 cb10 cr10		y30 y31 y32 y33 cb10 cr10
		y21			y22 y23				y24 y25 y26 y27
		y31 cb11 cr11		y32 y33 cb11 cr11		y34 y35 y36 y37 cb11 cr11

	0x14	y00		0x24	y00 y01			0x44	y00 y01 y02 y03
		y10			y10 y11				y10 y11 y12 y13
		y20			y20 y21				y20 y21 y22 y23
		y30 cb00 cr00		y30 y31 cb00 cr00		y30 y31 y32 y33 cb00 cr00
		y01			y02 y03				y04 y05 y06 y07
		y11			y12 y13				y14 y15 y16 y17
		y21			y22 y23				y24 y25 y26 y27
		y31 cb01 cr01		y32 y33 cb01 cr01		y34 y35 y36 y37 cb01 cr01
		....			...				...
		y40			y40 y41				y40 y41 y42 y43
		y50			y50 y51				y50 y51 y52 y53
		y60			y60 y61				y60 y61 y62 y63
		y70 cb10 cr10		y70 y71 cb10 cr10		y70 y71 y72 y73 cb10 cr10
		y41			y42 y43				y44 y45 y46 y47
		y51			y52 y53				y54 y55 y56 y57
		y61			y62 y63				y64 y65 y66 y67
		y71 cb11 cr11		y72 y73 cb11 cr11		y74 y75 y76 y77 cb11 cr11
	*/

	unsigned char *src = tile;
	unsigned char *dst;
	unsigned x, y, w, h; /* coordinates and dimensions of entire image */
	unsigned sx, sy, sw, sh; /* coordinates and dimensions of a single subsample region, i.e. max 4 x 4 samples */
	int k;
	int offsets[4 * 4 * 3]; /* for a pixel position, these point to all pixel components in a subsample region */
	int *offset = offsets;

	assert(tiff->samplesperpixel == 3);
	assert(tiff->bitspersample == 8);

	w = tiff->imagewidth;
	h = tiff->imagelength;

	sx = 0;
	sy = 0;
	sw = tiff->ycbcrsubsamp[0];
	sh = tiff->ycbcrsubsamp[1];
	if (sw > 4 || sh > 4 || !fz_is_pow2(sw) || !fz_is_pow2(sh))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Illegal TIFF Subsample values %d %d", sw, sh);

	for (k = 0; k < 3; k++)
		for (y = 0; y < sh; y++)
			for (x = 0; x < sw; x++)
				*offset++ = k + y * tiff->stride + x * 3;

	offset = offsets;
	x = col;
	y = row;
	k = 0;

	dst = &tiff->samples[row * tiff->stride + col * 3];

	while (src < tile + len)
	{
		if (k == 0)
		{ /* put all Y samples for a subsample region at the correct image pixel */
			if (y + sy < h && y + sy < row + th && x + sx < w && x + sx < col + tw)
				dst[*offset] = *src;
			offset++;

			if (++sx >= sw)
			{
				sx = 0;
				if (++sy >= sh)
				{
					sy = 0;
					k++;
				}
			}
		}
		else
		{ /* put all Cb/Cr samples for a subsample region at the correct image pixel */
			for (sy = 0; sy < sh; sy++)
				for (sx = 0; sx < sw; sx++)
				{
					if (y + sy < h && y + sy < row + th && x + sx < w && x + sx < col + tw)
						dst[*offset] = *src;
					offset++;
				}

			if (++k >= 3)
			{ /* we're done with this subsample region, on to the next one */
				k = sx = sy = 0;
				offset = offsets;

				dst += sw * 3;

				x += sw;
				if (x >= col + tw)
				{
					dst -= (x - (col + tw)) * 3;
					dst += (sh - 1) * w * 3;
					dst += col * 3;
					x = col;
					y += sh;
				}
			}
		}

		src++;
	}
}

static void
tiff_decode_tiles(fz_context *ctx, struct tiff *tiff)
{
	unsigned char *data;
	unsigned x, y, wlen, tile;
	unsigned tiles, tilesacross, tilesdown;

	tilesdown = (tiff->imagelength + tiff->tilelength - 1) / tiff->tilelength;
	tilesacross = (tiff->imagewidth + tiff->tilewidth - 1) / tiff->tilewidth;
	tiles = tilesacross * tilesdown;
	if (tiff->tileoffsetslen < tiles || tiff->tilebytecountslen < tiles)
		fz_throw(ctx, FZ_ERROR_GENERIC, "insufficient tile metadata");

	/* JPEG can handle subsampling on its own */
	if (tiff->photometric == 6 && tiff->compression != 6 && tiff->compression != 7)
	{
		/* regardless of how this is subsampled, a tile is never larger */
		if (tiff->tilelength >= tiff->ycbcrsubsamp[1])
			wlen = tiff->tilestride * tiff->tilelength;
		else
			wlen = tiff->tilestride * tiff->ycbcrsubsamp[1];

		data = tiff->data = Memento_label(fz_malloc(ctx, wlen), "tiff_tile_jpg");

		tile = 0;
		for (x = 0; x < tiff->imagelength; x += tiff->tilelength)
		{
			for (y = 0; y < tiff->imagewidth; y += tiff->tilewidth)
			{
				unsigned int offset = tiff->tileoffsets[tile];
				unsigned int rlen = tiff->tilebytecounts[tile];
				const unsigned char *rp = tiff->bp + offset;
				unsigned decoded;

				if (offset > (unsigned)(tiff->ep - tiff->bp))
					fz_throw(ctx, FZ_ERROR_GENERIC, "invalid tile offset %u", offset);
				if (rlen > (unsigned)(tiff->ep - rp))
					fz_throw(ctx, FZ_ERROR_GENERIC, "invalid tile byte count %u", rlen);

				decoded = tiff_decode_data(ctx, tiff, rp, rlen, data, wlen);
				tiff_paste_subsampled_tile(ctx, tiff, data, decoded, tiff->tilewidth, tiff->tilelength, x, y);
				tile++;
			}
		}
	}
	else
	{
		wlen = tiff->tilelength * tiff->tilestride;
		data = tiff->data = Memento_label(fz_malloc(ctx, wlen), "tiff_tile");

		tile = 0;
		for (x = 0; x < tiff->imagelength; x += tiff->tilelength)
		{
			for (y = 0; y < tiff->imagewidth; y += tiff->tilewidth)
			{
				unsigned int offset = tiff->tileoffsets[tile];
				unsigned int rlen = tiff->tilebytecounts[tile];
				const unsigned char *rp = tiff->bp + offset;

				if (offset > (unsigned)(tiff->ep - tiff->bp))
					fz_throw(ctx, FZ_ERROR_GENERIC, "invalid tile offset %u", offset);
				if (rlen > (unsigned)(tiff->ep - rp))
					fz_throw(ctx, FZ_ERROR_GENERIC, "invalid tile byte count %u", rlen);

				if (tiff_decode_data(ctx, tiff, rp, rlen, data, wlen) != wlen)
					fz_throw(ctx, FZ_ERROR_GENERIC, "decoded tile is the wrong size");

				tiff_paste_tile(ctx, tiff, data, x, y);
				tile++;
			}
		}
	}
}

static void
tiff_decode_strips(fz_context *ctx, struct tiff *tiff)
{
	unsigned char *data;
	unsigned strips;
	unsigned strip;
	unsigned y;

	strips = (tiff->imagelength + tiff->rowsperstrip - 1) / tiff->rowsperstrip;
	if (tiff->stripoffsetslen < strips || tiff->stripbytecountslen < strips)
		fz_throw(ctx, FZ_ERROR_GENERIC, "insufficient strip metadata");

	data = tiff->samples;

	/* JPEG can handle subsampling on its own */
	if (tiff->photometric == 6 && tiff->compression != 6 && tiff->compression != 7)
	{
		unsigned wlen;
		unsigned rowsperstrip;

		/* regardless of how this is subsampled, a strip is never taller */
		if (tiff->rowsperstrip >= tiff->ycbcrsubsamp[1])
			rowsperstrip = tiff->rowsperstrip;
		else
			rowsperstrip = tiff->ycbcrsubsamp[1];

		wlen = rowsperstrip * tiff->stride;
		data = tiff->data = Memento_label(fz_malloc(ctx, wlen), "tiff_strip_jpg");

		strip = 0;
		for (y = 0; y < tiff->imagelength; y += rowsperstrip)
		{
			unsigned offset = tiff->stripoffsets[strip];
			unsigned rlen = tiff->stripbytecounts[strip];
			const unsigned char *rp = tiff->bp + offset;
			int decoded;

			if (offset > (unsigned)(tiff->ep - tiff->bp))
				fz_throw(ctx, FZ_ERROR_GENERIC, "invalid strip offset %u", offset);
			if (rlen > (unsigned)(tiff->ep - rp))
				fz_throw(ctx, FZ_ERROR_GENERIC, "invalid strip byte count %u", rlen);

			decoded = tiff_decode_data(ctx, tiff, rp, rlen, data, wlen);
			tiff_paste_subsampled_tile(ctx, tiff, data, decoded, tiff->imagewidth, tiff->rowsperstrip, 0, y);
			strip++;
		}
	}
	else
	{
		strip = 0;
		for (y = 0; y < tiff->imagelength; y += tiff->rowsperstrip)
		{
			unsigned offset = tiff->stripoffsets[strip];
			unsigned rlen = tiff->stripbytecounts[strip];
			unsigned wlen = tiff->stride * tiff->rowsperstrip;
			const unsigned char *rp = tiff->bp + offset;

			if (offset > (unsigned)(tiff->ep - tiff->bp))
				fz_throw(ctx, FZ_ERROR_GENERIC, "invalid strip offset %u", offset);
			if (rlen > (unsigned)(tiff->ep - rp))
				fz_throw(ctx, FZ_ERROR_GENERIC, "invalid strip byte count %u", rlen);

			/* if imagelength is not a multiple of rowsperstrip, adjust the expectation of the size of the decoded data */
			if (y + tiff->rowsperstrip >= tiff->imagelength)
				wlen = tiff->stride * (tiff->imagelength - y);

			if (tiff_decode_data(ctx, tiff, rp, rlen, data, wlen) < wlen)
			{
				fz_warn(ctx, "premature end of data in decoded strip");
				break;
			}

			data += wlen;
			strip ++;
		}
	}
}

static inline int tiff_readbyte(struct tiff *tiff)
{
	if (tiff->rp < tiff->ep)
		return *tiff->rp++;
	return EOF;
}

static inline unsigned readshort(struct tiff *tiff)
{
	unsigned a = tiff_readbyte(tiff);
	unsigned b = tiff_readbyte(tiff);
	if (tiff->order == TII)
		return (b << 8) | a;
	return (a << 8) | b;
}

static inline unsigned tiff_readlong(struct tiff *tiff)
{
	unsigned a = tiff_readbyte(tiff);
	unsigned b = tiff_readbyte(tiff);
	unsigned c = tiff_readbyte(tiff);
	unsigned d = tiff_readbyte(tiff);
	if (tiff->order == TII)
		return (d << 24) | (c << 16) | (b << 8) | a;
	return (a << 24) | (b << 16) | (c << 8) | d;
}

static void
tiff_read_bytes(unsigned char *p, struct tiff *tiff, unsigned ofs, unsigned n)
{
	if (ofs > (unsigned)(tiff->ep - tiff->bp))
		ofs = (unsigned)(tiff->ep - tiff->bp);
	tiff->rp = tiff->bp + ofs;

	while (n--)
		*p++ = tiff_readbyte(tiff);
}

static void
tiff_read_tag_value(unsigned *p, struct tiff *tiff, unsigned type, unsigned ofs, unsigned n)
{
	unsigned den;

	if (ofs > (unsigned)(tiff->ep - tiff->bp))
		ofs = (unsigned)(tiff->ep - tiff->bp);
	tiff->rp = tiff->bp + ofs;

	while (n--)
	{
		switch (type)
		{
		case TRATIONAL:
			*p = tiff_readlong(tiff);
			den = tiff_readlong(tiff);
			if (den)
				*p = *p / den;
			else
				*p = UINT_MAX;
			p ++;
			break;
		case TBYTE: *p++ = tiff_readbyte(tiff); break;
		case TSHORT: *p++ = readshort(tiff); break;
		case TLONG: *p++ = tiff_readlong(tiff); break;
		default: *p++ = 0; break;
		}
	}
}

static void
tiff_read_tag(fz_context *ctx, struct tiff *tiff, unsigned offset)
{
	unsigned tag;
	unsigned type;
	unsigned count;
	unsigned value;

	tiff->rp = tiff->bp + offset;

	tag = readshort(tiff);
	type = readshort(tiff);
	count = tiff_readlong(tiff);

	if ((type == TBYTE && count <= 4) ||
			(type == TSHORT && count <= 2) ||
			(type == TLONG && count <= 1))
		value = tiff->rp - tiff->bp;
	else
		value = tiff_readlong(tiff);

	switch (tag)
	{
	case NewSubfileType:
		tiff_read_tag_value(&tiff->subfiletype, tiff, type, value, 1);
		break;
	case ImageWidth:
		tiff_read_tag_value(&tiff->imagewidth, tiff, type, value, 1);
		break;
	case ImageLength:
		tiff_read_tag_value(&tiff->imagelength, tiff, type, value, 1);
		break;
	case BitsPerSample:
		tiff_read_tag_value(&tiff->bitspersample, tiff, type, value, 1);
		break;
	case Compression:
		tiff_read_tag_value(&tiff->compression, tiff, type, value, 1);
		break;
	case PhotometricInterpretation:
		tiff_read_tag_value(&tiff->photometric, tiff, type, value, 1);
		break;
	case FillOrder:
		tiff_read_tag_value(&tiff->fillorder, tiff, type, value, 1);
		break;
	case SamplesPerPixel:
		tiff_read_tag_value(&tiff->samplesperpixel, tiff, type, value, 1);
		break;
	case RowsPerStrip:
		tiff_read_tag_value(&tiff->rowsperstrip, tiff, type, value, 1);
		break;
	case XResolution:
		tiff_read_tag_value(&tiff->xresolution, tiff, type, value, 1);
		break;
	case YResolution:
		tiff_read_tag_value(&tiff->yresolution, tiff, type, value, 1);
		break;
	case PlanarConfiguration:
		tiff_read_tag_value(&tiff->planar, tiff, type, value, 1);
		break;
	case T4Options:
		tiff_read_tag_value(&tiff->g3opts, tiff, type, value, 1);
		break;
	case T6Options:
		tiff_read_tag_value(&tiff->g4opts, tiff, type, value, 1);
		break;
	case Predictor:
		tiff_read_tag_value(&tiff->predictor, tiff, type, value, 1);
		break;
	case ResolutionUnit:
		tiff_read_tag_value(&tiff->resolutionunit, tiff, type, value, 1);
		break;
	case YCbCrSubSampling:
		tiff_read_tag_value(tiff->ycbcrsubsamp, tiff, type, value, 2);
		break;
	case ExtraSamples:
		tiff_read_tag_value(&tiff->extrasamples, tiff, type, value, 1);
		break;

	case ICCProfile:
		if (tiff->profile)
			fz_throw(ctx, FZ_ERROR_GENERIC, "at most one ICC profile tag allowed");
		tiff->profile = Memento_label(fz_malloc(ctx, count), "tiff_profile");
		/* ICC profile data type is set to UNDEFINED.
		 * TBYTE reading not correct in tiff_read_tag_value */
		tiff_read_bytes(tiff->profile, tiff, value, count);
		tiff->profilesize = count;
		break;

	case JPEGTables:
		/* Check both value and value + count to allow for overflow */
		if (value > (size_t)(tiff->ep - tiff->bp) || value + count > (size_t)(tiff->ep - tiff->bp))
			fz_throw(ctx, FZ_ERROR_GENERIC, "TIFF JPEG tables out of range");
		tiff->jpegtables = tiff->bp + value;
		tiff->jpegtableslen = count;
		break;

	case StripOffsets:
		if (tiff->stripoffsets)
			fz_throw(ctx, FZ_ERROR_GENERIC, "at most one strip offsets tag allowed");
		tiff->stripoffsets = Memento_label(fz_malloc_array(ctx, count, unsigned), "tiff_stripoffsets");
		tiff_read_tag_value(tiff->stripoffsets, tiff, type, value, count);
		tiff->stripoffsetslen = count;
		break;

	case StripByteCounts:
		if (tiff->stripbytecounts)
			fz_throw(ctx, FZ_ERROR_GENERIC, "at most one strip byte counts tag allowed");
		tiff->stripbytecounts = Memento_label(fz_malloc_array(ctx, count, unsigned), "tiff_stripbytecounts");
		tiff_read_tag_value(tiff->stripbytecounts, tiff, type, value, count);
		tiff->stripbytecountslen = count;
		break;

	case ColorMap:
		if (tiff->colormap)
			fz_throw(ctx, FZ_ERROR_GENERIC, "at most one color map allowed");
		tiff->colormap = Memento_label(fz_malloc_array(ctx, count, unsigned), "tiff_colormap");
		tiff_read_tag_value(tiff->colormap, tiff, type, value, count);
		tiff->colormaplen = count;
		break;

	case TileWidth:
		tiff_read_tag_value(&tiff->tilewidth, tiff, type, value, 1);
		break;

	case TileLength:
		tiff_read_tag_value(&tiff->tilelength, tiff, type, value, 1);
		break;

	case TileOffsets:
		if (tiff->tileoffsets)
			fz_throw(ctx, FZ_ERROR_GENERIC, "at most one tile offsets tag allowed");
		tiff->tileoffsets = Memento_label(fz_malloc_array(ctx, count, unsigned), "tiff_tileoffsets");
		tiff_read_tag_value(tiff->tileoffsets, tiff, type, value, count);
		tiff->tileoffsetslen = count;
		break;

	case TileByteCounts:
		if (tiff->tilebytecounts)
			fz_throw(ctx, FZ_ERROR_GENERIC, "at most one tile byte counts tag allowed");
		tiff->tilebytecounts = Memento_label(fz_malloc_array(ctx, count, unsigned), "tiff_tilebytecounts");
		tiff_read_tag_value(tiff->tilebytecounts, tiff, type, value, count);
		tiff->tilebytecountslen = count;
		break;

	default:
		/* fz_warn(ctx, "unknown tag: %d t=%d n=%d", tag, type, count); */
		break;
	}
}

static void
tiff_swap_byte_order(unsigned char *buf, int n)
{
	int i, t;
	for (i = 0; i < n; i++)
	{
		t = buf[i * 2 + 0];
		buf[i * 2 + 0] = buf[i * 2 + 1];
		buf[i * 2 + 1] = t;
	}
}

static void
tiff_scale_lab_samples(fz_context *ctx, unsigned char *buf, int bps, int n)
{
	int i;
	if (bps == 8)
		for (i = 0; i < n; i++, buf += 3)
		{
			buf[1] ^= 128;
			buf[2] ^= 128;
		}
	else if (bps == 16)
		for (i = 0; i < n; i++, buf += 6)
		{
			buf[2] ^= 128;
			buf[4] ^= 128;
		}
}

static void
tiff_read_header(fz_context *ctx, struct tiff *tiff, const unsigned char *buf, size_t len)
{
	unsigned version;

	memset(tiff, 0, sizeof(struct tiff));
	tiff->bp = buf;
	tiff->rp = buf;
	tiff->ep = buf + len;

	/* tag defaults, where applicable */
	tiff->bitspersample = 1;
	tiff->compression = 1;
	tiff->samplesperpixel = 1;
	tiff->resolutionunit = 2;
	tiff->rowsperstrip = 0xFFFFFFFF;
	tiff->fillorder = 1;
	tiff->planar = 1;
	tiff->subfiletype = 0;
	tiff->predictor = 1;
	tiff->ycbcrsubsamp[0] = 2;
	tiff->ycbcrsubsamp[1] = 2;

	/*
	 * Read IFH
	 */

	/* get byte order marker */
	tiff->order = readshort(tiff);
	if (tiff->order != TII && tiff->order != TMM)
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a TIFF file, wrong magic marker");

	/* check version */
	version = readshort(tiff);
	if (version != 42)
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a TIFF file, wrong version marker");

	/* get offset of IFD */
	tiff->ifd_offsets = Memento_label(fz_malloc_array(ctx, 1, unsigned), "tiff_ifd_offsets");
	tiff->ifd_offsets[0] = tiff_readlong(tiff);
	tiff->ifds = 1;
}

static unsigned
tiff_next_ifd(fz_context *ctx, struct tiff *tiff, unsigned offset)
{
	unsigned count;
	int i;

	if (offset > (unsigned)(tiff->ep - tiff->bp))
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid IFD offset %u", offset);

	tiff->rp = tiff->bp + offset;
	count = readshort(tiff);

	if (count * 12 > (unsigned)(tiff->ep - tiff->rp))
		fz_throw(ctx, FZ_ERROR_GENERIC, "overlarge IFD entry count %u", count);

	tiff->rp += count * 12;
	offset = tiff_readlong(tiff);

	for (i = 0; i < tiff->ifds; i++)
		if (tiff->ifd_offsets[i] == offset)
			fz_throw(ctx, FZ_ERROR_GENERIC, "cycle in IFDs detected");

	tiff->ifd_offsets = fz_realloc_array(ctx, tiff->ifd_offsets, tiff->ifds + 1, unsigned);
	tiff->ifd_offsets[tiff->ifds] = offset;
	tiff->ifds++;

	return offset;
}

static void
tiff_seek_ifd(fz_context *ctx, struct tiff *tiff, int subimage)
{
	unsigned offset = tiff->ifd_offsets[0];

	while (subimage--)
	{
		offset = tiff_next_ifd(ctx, tiff, offset);

		if (offset == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "subimage index %i out of range", subimage);
	}

	tiff->rp = tiff->bp + offset;

	if (tiff->rp < tiff->bp || tiff->rp > tiff->ep)
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid IFD offset %u", offset);
}

static void
tiff_read_ifd(fz_context *ctx, struct tiff *tiff)
{
	unsigned offset;
	unsigned count;
	unsigned i;

	offset = tiff->rp - tiff->bp;

	count = readshort(tiff);

	if (count * 12 > (unsigned)(tiff->ep - tiff->rp))
		fz_throw(ctx, FZ_ERROR_GENERIC, "overlarge IFD entry count %u", count);

	offset += 2;
	for (i = 0; i < count; i++)
	{
		tiff_read_tag(ctx, tiff, offset);
		offset += 12;
	}
}

static void
tiff_ycc_to_rgb(fz_context *ctx, struct tiff *tiff)
{
	unsigned x, y;
	int offset = tiff->samplesperpixel;

	for (y = 0; y < tiff->imagelength; y++)
	{
		unsigned char * row = &tiff->samples[tiff->stride * y];
		for (x = 0; x < tiff->imagewidth; x++)
		{
			int ycc[3];
			ycc[0] = row[x * offset + 0];
			ycc[1] = row[x * offset + 1] - 128;
			ycc[2] = row[x * offset + 2] - 128;

			row[x * offset + 0] = fz_clampi(ycc[0] + 1.402f * ycc[2], 0, 255);
			row[x * offset + 1] = fz_clampi(ycc[0] - 0.34413f * ycc[1] - 0.71414f * ycc[2], 0, 255);
			row[x * offset + 2] = fz_clampi(ycc[0] + 1.772f * ycc[1], 0, 255);
		}
	}
}

static void
tiff_decode_ifd(fz_context *ctx, struct tiff *tiff)
{
	unsigned i;

	if (tiff->imagelength <= 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image height must be > 0");
	if (tiff->imagewidth <= 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image width must be > 0");
	if (tiff->bitspersample > 16 || !fz_is_pow2(tiff->bitspersample))
		fz_throw(ctx, FZ_ERROR_GENERIC, "bits per sample illegal %d", tiff->bitspersample);
	if (tiff->samplesperpixel == 0 || tiff->samplesperpixel >= FZ_MAX_COLORS)
		fz_throw(ctx, FZ_ERROR_GENERIC, "components per pixel out of range");
	if (tiff->imagelength > UINT_MAX / tiff->imagewidth / (tiff->samplesperpixel + 2) / (tiff->bitspersample / 8 + 1))
		fz_throw(ctx, FZ_ERROR_GENERIC, "image too large");

	if (tiff->planar != 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image data is not in chunky format");

	if (tiff->photometric == 6)
	{
		if (tiff->samplesperpixel != 3)
			fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported samples per pixel when subsampling");
		if (tiff->bitspersample != 8)
			fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported bits per sample when subsampling");
		if (tiff->ycbcrsubsamp[0] == 0 || tiff->ycbcrsubsamp[1] == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "unsupported subsampling factor");
	}

	tiff->stride = (tiff->imagewidth * tiff->samplesperpixel * tiff->bitspersample + 7) / 8;
	tiff->tilestride = (tiff->tilewidth * tiff->samplesperpixel * tiff->bitspersample + 7) / 8;

	switch (tiff->photometric)
	{
	case 0: /* WhiteIsZero -- inverted */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_gray(ctx));
		break;
	case 1: /* BlackIsZero */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_gray(ctx));
		break;
	case 2: /* RGB */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		break;
	case 3: /* RGBPal */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		break;
	case 4: /* Transparency mask */
		tiff->colorspace = NULL;
		break;
	case 5: /* CMYK */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_cmyk(ctx));
		break;
	case 6: /* YCbCr */
		/* it's probably a jpeg ... we let jpeg convert to rgb */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		break;
	case 8: /* Direct L*a*b* encoding. a*, b* signed values */
	case 9: /* ICC Style L*a*b* encoding */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_lab(ctx));
		break;
	case 32844: /* SGI CIE Log 2 L (16bpp Greyscale) */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_gray(ctx));
		if (tiff->bitspersample != 8)
			tiff->bitspersample = 8;
		tiff->stride >>= 1;
		break;
	case 32845: /* SGI CIE Log 2 L, u, v (24bpp or 32bpp) */
		tiff->colorspace = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		if (tiff->bitspersample != 8)
			tiff->bitspersample = 8;
		tiff->stride >>= 1;
		break;
	default:
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown photometric: %d", tiff->photometric);
	}

#if FZ_ENABLE_ICC
	if (tiff->profile)
	{
		fz_buffer *buff = NULL;
		fz_colorspace *icc = NULL;
		fz_var(buff);
		fz_try(ctx)
		{
			buff = fz_new_buffer_from_copied_data(ctx, tiff->profile, tiff->profilesize);
			icc = fz_new_icc_colorspace(ctx, fz_colorspace_type(ctx, tiff->colorspace), 0, NULL, buff);
			fz_drop_colorspace(ctx, tiff->colorspace);
			tiff->colorspace = icc;
		}
		fz_always(ctx)
			fz_drop_buffer(ctx, buff);
		fz_catch(ctx)
			fz_warn(ctx, "ignoring embedded ICC profile");
	}
#endif

	if (!tiff->colorspace && tiff->samplesperpixel < 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "too few components for transparency mask");
	if (tiff->colorspace && tiff->colormap && tiff->samplesperpixel < 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "too few components for RGBPal");
	if (tiff->colorspace && !tiff->colormap && tiff->samplesperpixel < (unsigned) fz_colorspace_n(ctx, tiff->colorspace))
		fz_throw(ctx, FZ_ERROR_GENERIC, "fewer components per pixel than indicated by colorspace");

	switch (tiff->resolutionunit)
	{
	case 2:
		/* no unit conversion needed */
		break;
	case 3:
		tiff->xresolution = tiff->xresolution * 254 / 100;
		tiff->yresolution = tiff->yresolution * 254 / 100;
		break;
	default:
		tiff->xresolution = 96;
		tiff->yresolution = 96;
		break;
	}

	/* Note xres and yres could be 0 even if unit was set. If so default to 96dpi. */
	if (tiff->xresolution == 0 || tiff->yresolution == 0)
	{
		tiff->xresolution = 96;
		tiff->yresolution = 96;
	}

	if (tiff->rowsperstrip > tiff->imagelength)
		tiff->rowsperstrip = tiff->imagelength;

	/* some creators don't write byte counts for uncompressed images */
	if (tiff->compression == 1)
	{
		if (tiff->rowsperstrip == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "rowsperstrip cannot be 0");
		if (!tiff->tilelength && !tiff->tilewidth && !tiff->stripbytecounts)
		{
			tiff->stripbytecountslen = (tiff->imagelength + tiff->rowsperstrip - 1) / tiff->rowsperstrip;
			tiff->stripbytecounts = Memento_label(fz_malloc_array(ctx, tiff->stripbytecountslen, unsigned), "tiff_stripbytecounts");
			for (i = 0; i < tiff->stripbytecountslen; i++)
				tiff->stripbytecounts[i] = tiff->rowsperstrip * tiff->stride;
		}
		if (tiff->tilelength && tiff->tilewidth && !tiff->tilebytecounts)
		{
			unsigned tilesdown = (tiff->imagelength + tiff->tilelength - 1) / tiff->tilelength;
			unsigned tilesacross = (tiff->imagewidth + tiff->tilewidth - 1) / tiff->tilewidth;
			tiff->tilebytecountslen = tilesacross * tilesdown;
			tiff->tilebytecounts = Memento_label(fz_malloc_array(ctx, tiff->tilebytecountslen, unsigned), "tiff_tilebytecounts");
			for (i = 0; i < tiff->tilebytecountslen; i++)
				tiff->tilebytecounts[i] = tiff->tilelength * tiff->tilestride;
		}
	}

	/* some creators write strip tags when they meant to write tile tags... */
	if (tiff->tilelength && tiff->tilewidth)
	{
		if (!tiff->tileoffsets && !tiff->tileoffsetslen &&
				tiff->stripoffsets && tiff->stripoffsetslen)
		{
			tiff->tileoffsets = tiff->stripoffsets;
			tiff->tileoffsetslen = tiff->stripoffsetslen;
			tiff->stripoffsets = NULL;
			tiff->stripoffsetslen = 0;
		}
		if (!tiff->tilebytecounts && !tiff->tilebytecountslen &&
				tiff->stripbytecounts && tiff->stripbytecountslen)
		{
			tiff->tilebytecounts = tiff->stripbytecounts;
			tiff->tilebytecountslen = tiff->stripbytecountslen;
			tiff->stripbytecounts = NULL;
			tiff->stripbytecountslen = 0;
		}
	}
}

static void
tiff_decode_samples(fz_context *ctx, struct tiff *tiff)
{
	unsigned i;

	if (tiff->imagelength > UINT_MAX / tiff->stride)
		fz_throw(ctx, FZ_ERROR_MEMORY, "image too large");
	tiff->samples = Memento_label(fz_malloc(ctx, tiff->imagelength * tiff->stride), "tiff_samples");
	memset(tiff->samples, 0x55, tiff->imagelength * tiff->stride);

	if (tiff->tilelength && tiff->tilewidth && tiff->tileoffsets && tiff->tilebytecounts)
		tiff_decode_tiles(ctx, tiff);
	else if (tiff->rowsperstrip && tiff->stripoffsets && tiff->stripbytecounts)
		tiff_decode_strips(ctx, tiff);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "image is missing both strip and tile data");

	/* Predictor (only for LZW and Flate) */
	if ((tiff->compression == 5 || tiff->compression == 8 || tiff->compression == 32946) && tiff->predictor == 2)
	{
		unsigned char *p = tiff->samples;
		for (i = 0; i < tiff->imagelength; i++)
		{
			tiff_unpredict_line(p, tiff->imagewidth, tiff->samplesperpixel, tiff->bitspersample);
			p += tiff->stride;
		}
	}

	/* YCbCr -> RGB, but JPEG already has done this conversion  */
	if (tiff->photometric == 6 && tiff->compression != 6 && tiff->compression != 7)
		tiff_ycc_to_rgb(ctx, tiff);

	/* RGBPal */
	if (tiff->photometric == 3 && tiff->colormap)
		tiff_expand_colormap(ctx, tiff);

	/* WhiteIsZero .. invert */
	if (tiff->photometric == 0)
	{
		unsigned char *p = tiff->samples;
		for (i = 0; i < tiff->imagelength; i++)
		{
			tiff_invert_line(p, tiff->imagewidth, tiff->samplesperpixel, tiff->bitspersample, tiff->extrasamples);
			p += tiff->stride;
		}
	}

	/* Premultiplied transparency */
	if (tiff->extrasamples == 1)
	{
		/* In GhostXPS we undo the premultiplication here; muxps holds
		 * all our images premultiplied by default, so nothing to do.
		 */
	}

	/* Non-premultiplied transparency */
	if (tiff->extrasamples == 2)
	{
		/* Premultiplied files are corrected for elsewhere */
	}

	/* Byte swap 16-bit images to big endian if necessary */
	if (tiff->bitspersample == 16 && tiff->order == TII)
		tiff_swap_byte_order(tiff->samples, tiff->imagewidth * tiff->imagelength * tiff->samplesperpixel);

	/* Lab colorspace expects all sample components 0..255.
	TIFF supplies them as L = 0..255, a/b = -128..127 (for
	8 bits per sample, -32768..32767 for 16 bits per sample)
	Scale them to the colorspace's expectations. */
	if (tiff->photometric == 8 && tiff->samplesperpixel == 3)
		tiff_scale_lab_samples(ctx, tiff->samples, tiff->bitspersample, tiff->imagewidth * tiff->imagelength);
}

fz_pixmap *
fz_load_tiff_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int subimage)
{
	fz_pixmap *image = NULL;
	struct tiff tiff = { 0 };
	int alpha;

	fz_var(image);

	fz_try(ctx)
	{
		tiff_read_header(ctx, &tiff, buf, len);
		tiff_seek_ifd(ctx, &tiff, subimage);
		tiff_read_ifd(ctx, &tiff);

		/* Decode the image data */
		tiff_decode_ifd(ctx, &tiff);
		tiff_decode_samples(ctx, &tiff);

		/* Expand into fz_pixmap struct */
		alpha = tiff.extrasamples != 0 || tiff.colorspace == NULL;
		image = fz_new_pixmap(ctx, tiff.colorspace, tiff.imagewidth, tiff.imagelength, NULL, alpha);
		image->xres = tiff.xresolution;
		image->yres = tiff.yresolution;

		fz_unpack_tile(ctx, image, tiff.samples, tiff.samplesperpixel, tiff.bitspersample, tiff.stride, 0);

		/* We should only do this on non-pre-multiplied images, but files in the wild are bad */
		/* TODO: check if any samples are non-premul to detect bad files */
		if (tiff.extrasamples /* == 2 */)
			fz_premultiply_pixmap(ctx, image);
	}
	fz_always(ctx)
	{
		/* Clean up scratch memory */
		fz_drop_colorspace(ctx, tiff.colorspace);
		fz_free(ctx, tiff.colormap);
		fz_free(ctx, tiff.stripoffsets);
		fz_free(ctx, tiff.stripbytecounts);
		fz_free(ctx, tiff.tileoffsets);
		fz_free(ctx, tiff.tilebytecounts);
		fz_free(ctx, tiff.data);
		fz_free(ctx, tiff.samples);
		fz_free(ctx, tiff.profile);
		fz_free(ctx, tiff.ifd_offsets);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, image);
		fz_rethrow(ctx);
	}

	return image;
}

fz_pixmap *
fz_load_tiff(fz_context *ctx, const unsigned char *buf, size_t len)
{
	return fz_load_tiff_subimage(ctx, buf, len, 0);
}

void
fz_load_tiff_info_subimage(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep, int subimage)
{
	struct tiff tiff = { 0 };

	fz_try(ctx)
	{
		tiff_read_header(ctx, &tiff, buf, len);
		tiff_seek_ifd(ctx, &tiff, subimage);
		tiff_read_ifd(ctx, &tiff);

		tiff_decode_ifd(ctx, &tiff);

		*wp = tiff.imagewidth;
		*hp = tiff.imagelength;
		*xresp = (tiff.xresolution ? tiff.xresolution : 96);
		*yresp = (tiff.yresolution ? tiff.yresolution : 96);
		if (tiff.extrasamples /* == 2 */)
		{
			fz_drop_colorspace(ctx, tiff.colorspace);
			tiff.colorspace = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		}
		*cspacep = fz_keep_colorspace(ctx, tiff.colorspace);
	}
	fz_always(ctx)
	{
		/* Clean up scratch memory */
		fz_drop_colorspace(ctx, tiff.colorspace);
		fz_free(ctx, tiff.colormap);
		fz_free(ctx, tiff.stripoffsets);
		fz_free(ctx, tiff.stripbytecounts);
		fz_free(ctx, tiff.tileoffsets);
		fz_free(ctx, tiff.tilebytecounts);
		fz_free(ctx, tiff.data);
		fz_free(ctx, tiff.samples);
		fz_free(ctx, tiff.profile);
		fz_free(ctx, tiff.ifd_offsets);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void
fz_load_tiff_info(fz_context *ctx, const unsigned char *buf, size_t len, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	fz_load_tiff_info_subimage(ctx, buf, len, wp, hp, xresp, yresp, cspacep, 0);
}

int
fz_load_tiff_subimage_count(fz_context *ctx, const unsigned char *buf, size_t len)
{
	unsigned offset;
	unsigned subimage_count = 0;
	struct tiff tiff = { 0 };

	fz_try(ctx)
	{
		tiff_read_header(ctx, &tiff, buf, len);

		offset = tiff.ifd_offsets[0];

		do {
			subimage_count++;
			offset = tiff_next_ifd(ctx, &tiff, offset);
		} while (offset != 0);
	}
	fz_always(ctx)
		fz_free(ctx, tiff.ifd_offsets);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return subimage_count;
}
