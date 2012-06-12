#include "fitz-internal.h"

/*
 * TIFF image loader. Should be enough to support TIFF files in XPS.
 * Baseline TIFF 6.0 plus CMYK, LZW, Flate and JPEG support.
 * Limited bit depths (1,2,4,8).
 * Limited planar configurations (1=chunky).
 * No tiles (easy fix if necessary).
 * TODO: RGBPal images
 */

struct tiff
{
	fz_context *ctx;

	/* "file" */
	unsigned char *bp, *rp, *ep;

	/* byte order */
	unsigned order;

	/* where we can find the strips of image data */
	unsigned rowsperstrip;
	unsigned *stripoffsets;
	unsigned *stripbytecounts;

	/* colormap */
	unsigned *colormap;

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

	unsigned char *jpegtables;		/* point into "file" buffer */
	unsigned jpegtableslen;

	unsigned char *profile;
	int profilesize;

	/* decoded data */
	fz_colorspace *colorspace;
	unsigned char *samples;
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
#define YCbCrSubSampling 520
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

static void
fz_decode_tiff_uncompressed(struct tiff *tiff, fz_stream *stm, unsigned char *wp, int wlen)
{
	fz_read(stm, wp, wlen);
	fz_close(stm);
}

static void
fz_decode_tiff_packbits(struct tiff *tiff, fz_stream *chain, unsigned char *wp, int wlen)
{
	fz_stream *stm = fz_open_rld(chain);
	fz_read(stm, wp, wlen);
	fz_close(stm);
}

static void
fz_decode_tiff_lzw(struct tiff *tiff, fz_stream *chain, unsigned char *wp, int wlen)
{
	fz_stream *stm = fz_open_lzwd(chain, 1);
	fz_read(stm, wp, wlen);
	fz_close(stm);
}

static void
fz_decode_tiff_flate(struct tiff *tiff, fz_stream *chain, unsigned char *wp, int wlen)
{
	fz_stream *stm = fz_open_flated(chain);
	fz_read(stm, wp, wlen);
	fz_close(stm);
}

static void
fz_decode_tiff_fax(struct tiff *tiff, int comp, fz_stream *chain, unsigned char *wp, int wlen)
{
	fz_stream *stm;
	int black_is_1 = tiff->photometric == 0;
	int k = comp == 4 ? -1 : 0;
	int encoded_byte_align = comp == 2;
	stm = fz_open_faxd(chain,
			k, 0, encoded_byte_align,
			tiff->imagewidth, tiff->imagelength, 0, black_is_1);
	fz_read(stm, wp, wlen);
	fz_close(stm);
}

static void
fz_decode_tiff_jpeg(struct tiff *tiff, fz_stream *chain, unsigned char *wp, int wlen)
{
	fz_stream *stm = fz_open_dctd(chain, -1);
	fz_read(stm, wp, wlen);
	fz_close(stm);
}

static inline int getcomp(unsigned char *line, int x, int bpc)
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

static inline void putcomp(unsigned char *line, int x, int bpc, int value)
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
fz_unpredict_tiff(unsigned char *line, int width, int comps, int bits)
{
	unsigned char left[32];
	int i, k, v;

	for (k = 0; k < comps; k++)
		left[k] = 0;

	for (i = 0; i < width; i++)
	{
		for (k = 0; k < comps; k++)
		{
			v = getcomp(line, i * comps + k, bits);
			v = v + left[k];
			v = v % (1 << bits);
			putcomp(line, i * comps + k, bits, v);
			left[k] = v;
		}
	}
}

static void
fz_invert_tiff(unsigned char *line, int width, int comps, int bits, int alpha)
{
	int i, k, v;
	int m = (1 << bits) - 1;

	for (i = 0; i < width; i++)
	{
		for (k = 0; k < comps; k++)
		{
			v = getcomp(line, i * comps + k, bits);
			if (!alpha || k < comps - 1)
				v = m - v;
			putcomp(line, i * comps + k, bits, v);
		}
	}
}

static void
fz_expand_tiff_colormap(struct tiff *tiff)
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
		fz_throw(tiff->ctx, "invalid number of samples for RGBPal");

	if (tiff->bitspersample != 4 && tiff->bitspersample != 8)
		fz_throw(tiff->ctx, "invalid number of bits for RGBPal");

	stride = tiff->imagewidth * (tiff->samplesperpixel + 2);

	samples = fz_malloc(tiff->ctx, stride * tiff->imagelength);

	for (y = 0; y < tiff->imagelength; y++)
	{
		src = tiff->samples + (unsigned int)(tiff->stride * y);
		dst = samples + (unsigned int)(stride * y);

		for (x = 0; x < tiff->imagewidth; x++)
		{
			if (tiff->extrasamples)
			{
				int c = getcomp(src, x * 2, tiff->bitspersample);
				int a = getcomp(src, x * 2 + 1, tiff->bitspersample);
				*dst++ = tiff->colormap[c + 0] >> 8;
				*dst++ = tiff->colormap[c + maxval] >> 8;
				*dst++ = tiff->colormap[c + maxval * 2] >> 8;
				*dst++ = a << (8 - tiff->bitspersample);
			}
			else
			{
				int c = getcomp(src, x, tiff->bitspersample);
				*dst++ = tiff->colormap[c + 0] >> 8;
				*dst++ = tiff->colormap[c + maxval] >> 8;
				*dst++ = tiff->colormap[c + maxval * 2] >> 8;
			}
		}
	}

	tiff->samplesperpixel += 2;
	tiff->bitspersample = 8;
	tiff->stride = stride;
	fz_free(tiff->ctx, tiff->samples);
	tiff->samples = samples;
}

static void
fz_decode_tiff_strips(struct tiff *tiff)
{
	fz_stream *stm;

	/* switch on compression to create a filter */
	/* feed each strip to the filter */
	/* read out the data and pack the samples into a pixmap */

	/* type 32773 / packbits -- nothing special (same row-padding as PDF) */
	/* type 2 / ccitt rle -- no EOL, no RTC, rows are byte-aligned */
	/* type 3 and 4 / g3 and g4 -- each strip starts new section */
	/* type 5 / lzw -- each strip is handled separately */

	unsigned char *wp;
	unsigned row;
	unsigned strip;
	unsigned i;

	if (!tiff->rowsperstrip || !tiff->stripoffsets || !tiff->rowsperstrip)
		fz_throw(tiff->ctx, "no image data in tiff; maybe it is tiled");

	if (tiff->planar != 1)
		fz_throw(tiff->ctx, "image data is not in chunky format");

	tiff->stride = (tiff->imagewidth * tiff->samplesperpixel * tiff->bitspersample + 7) / 8;

	switch (tiff->photometric)
	{
	case 0: /* WhiteIsZero -- inverted */
		tiff->colorspace = fz_device_gray;
		break;
	case 1: /* BlackIsZero */
		tiff->colorspace = fz_device_gray;
		break;
	case 2: /* RGB */
		tiff->colorspace = fz_device_rgb;
		break;
	case 3: /* RGBPal */
		tiff->colorspace = fz_device_rgb;
		break;
	case 5: /* CMYK */
		tiff->colorspace = fz_device_cmyk;
		break;
	case 6: /* YCbCr */
		/* it's probably a jpeg ... we let jpeg convert to rgb */
		tiff->colorspace = fz_device_rgb;
		break;
	default:
		fz_throw(tiff->ctx, "unknown photometric: %d", tiff->photometric);
	}

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

	tiff->samples = fz_malloc_array(tiff->ctx, tiff->imagelength, tiff->stride);
	memset(tiff->samples, 0x55, tiff->imagelength * tiff->stride);
	wp = tiff->samples;

	strip = 0;
	for (row = 0; row < tiff->imagelength; row += tiff->rowsperstrip)
	{
		unsigned offset = tiff->stripoffsets[strip];
		unsigned rlen = tiff->stripbytecounts[strip];
		unsigned wlen = tiff->stride * tiff->rowsperstrip;
		unsigned char *rp = tiff->bp + offset;

		if (wp + wlen > tiff->samples + (unsigned int)(tiff->stride * tiff->imagelength))
			wlen = tiff->samples + (unsigned int)(tiff->stride * tiff->imagelength) - wp;

		if (rp + rlen > tiff->ep)
			fz_throw(tiff->ctx, "strip extends beyond the end of the file");

		/* the bits are in un-natural order */
		if (tiff->fillorder == 2)
			for (i = 0; i < rlen; i++)
				rp[i] = bitrev[rp[i]];

		/* the strip decoders will close this */
		stm = fz_open_memory(tiff->ctx, rp, rlen);

		switch (tiff->compression)
		{
		case 1:
			fz_decode_tiff_uncompressed(tiff, stm, wp, wlen);
			break;
		case 2:
			fz_decode_tiff_fax(tiff, 2, stm, wp, wlen);
			break;
		case 3:
			fz_decode_tiff_fax(tiff, 3, stm, wp, wlen);
			break;
		case 4:
			fz_decode_tiff_fax(tiff, 4, stm, wp, wlen);
			break;
		case 5:
			fz_decode_tiff_lzw(tiff, stm, wp, wlen);
			break;
		case 6:
			fz_throw(tiff->ctx, "deprecated JPEG in TIFF compression not supported");
			break;
		case 7:
			fz_decode_tiff_jpeg(tiff, stm, wp, wlen);
			break;
		case 8:
			fz_decode_tiff_flate(tiff, stm, wp, wlen);
			break;
		case 32773:
			fz_decode_tiff_packbits(tiff, stm, wp, wlen);
			break;
		default:
			fz_throw(tiff->ctx, "unknown TIFF compression: %d", tiff->compression);
		}

		/* scramble the bits back into original order */
		if (tiff->fillorder == 2)
			for (i = 0; i < rlen; i++)
				rp[i] = bitrev[rp[i]];

		wp += tiff->stride * tiff->rowsperstrip;
		strip ++;
	}

	/* Predictor (only for LZW and Flate) */
	if ((tiff->compression == 5 || tiff->compression == 8) && tiff->predictor == 2)
	{
		unsigned char *p = tiff->samples;
		for (i = 0; i < tiff->imagelength; i++)
		{
			fz_unpredict_tiff(p, tiff->imagewidth, tiff->samplesperpixel, tiff->bitspersample);
			p += tiff->stride;
		}
	}

	/* RGBPal */
	if (tiff->photometric == 3 && tiff->colormap)
		fz_expand_tiff_colormap(tiff);

	/* WhiteIsZero .. invert */
	if (tiff->photometric == 0)
	{
		unsigned char *p = tiff->samples;
		for (i = 0; i < tiff->imagelength; i++)
		{
			fz_invert_tiff(p, tiff->imagewidth, tiff->samplesperpixel, tiff->bitspersample, tiff->extrasamples);
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
}

static inline int readbyte(struct tiff *tiff)
{
	if (tiff->rp < tiff->ep)
		return *tiff->rp++;
	return EOF;
}

static inline unsigned readshort(struct tiff *tiff)
{
	unsigned a = readbyte(tiff);
	unsigned b = readbyte(tiff);
	if (tiff->order == TII)
		return (b << 8) | a;
	return (a << 8) | b;
}

static inline unsigned readlong(struct tiff *tiff)
{
	unsigned a = readbyte(tiff);
	unsigned b = readbyte(tiff);
	unsigned c = readbyte(tiff);
	unsigned d = readbyte(tiff);
	if (tiff->order == TII)
		return (d << 24) | (c << 16) | (b << 8) | a;
	return (a << 24) | (b << 16) | (c << 8) | d;
}

static void
fz_read_tiff_bytes(unsigned char *p, struct tiff *tiff, unsigned ofs, unsigned n)
{
	tiff->rp = tiff->bp + ofs;
	if (tiff->rp > tiff->ep)
		tiff->rp = tiff->bp;

	while (n--)
		*p++ = readbyte(tiff);
}

static void
fz_read_tiff_tag_value(unsigned *p, struct tiff *tiff, unsigned type, unsigned ofs, unsigned n)
{
	tiff->rp = tiff->bp + ofs;
	if (tiff->rp > tiff->ep)
		tiff->rp = tiff->bp;

	while (n--)
	{
		switch (type)
		{
		case TRATIONAL:
			*p = readlong(tiff);
			*p = *p / readlong(tiff);
			p ++;
			break;
		case TBYTE: *p++ = readbyte(tiff); break;
		case TSHORT: *p++ = readshort(tiff); break;
		case TLONG: *p++ = readlong(tiff); break;
		default: *p++ = 0; break;
		}
	}
}

static void
fz_read_tiff_tag(struct tiff *tiff, unsigned offset)
{
	unsigned tag;
	unsigned type;
	unsigned count;
	unsigned value;

	tiff->rp = tiff->bp + offset;

	tag = readshort(tiff);
	type = readshort(tiff);
	count = readlong(tiff);

	if ((type == TBYTE && count <= 4) ||
			(type == TSHORT && count <= 2) ||
			(type == TLONG && count <= 1))
		value = tiff->rp - tiff->bp;
	else
		value = readlong(tiff);

	switch (tag)
	{
	case NewSubfileType:
		fz_read_tiff_tag_value(&tiff->subfiletype, tiff, type, value, 1);
		break;
	case ImageWidth:
		fz_read_tiff_tag_value(&tiff->imagewidth, tiff, type, value, 1);
		break;
	case ImageLength:
		fz_read_tiff_tag_value(&tiff->imagelength, tiff, type, value, 1);
		break;
	case BitsPerSample:
		fz_read_tiff_tag_value(&tiff->bitspersample, tiff, type, value, 1);
		break;
	case Compression:
		fz_read_tiff_tag_value(&tiff->compression, tiff, type, value, 1);
		break;
	case PhotometricInterpretation:
		fz_read_tiff_tag_value(&tiff->photometric, tiff, type, value, 1);
		break;
	case FillOrder:
		fz_read_tiff_tag_value(&tiff->fillorder, tiff, type, value, 1);
		break;
	case SamplesPerPixel:
		fz_read_tiff_tag_value(&tiff->samplesperpixel, tiff, type, value, 1);
		break;
	case RowsPerStrip:
		fz_read_tiff_tag_value(&tiff->rowsperstrip, tiff, type, value, 1);
		break;
	case XResolution:
		fz_read_tiff_tag_value(&tiff->xresolution, tiff, type, value, 1);
		break;
	case YResolution:
		fz_read_tiff_tag_value(&tiff->yresolution, tiff, type, value, 1);
		break;
	case PlanarConfiguration:
		fz_read_tiff_tag_value(&tiff->planar, tiff, type, value, 1);
		break;
	case T4Options:
		fz_read_tiff_tag_value(&tiff->g3opts, tiff, type, value, 1);
		break;
	case T6Options:
		fz_read_tiff_tag_value(&tiff->g4opts, tiff, type, value, 1);
		break;
	case Predictor:
		fz_read_tiff_tag_value(&tiff->predictor, tiff, type, value, 1);
		break;
	case ResolutionUnit:
		fz_read_tiff_tag_value(&tiff->resolutionunit, tiff, type, value, 1);
		break;
	case YCbCrSubSampling:
		fz_read_tiff_tag_value(tiff->ycbcrsubsamp, tiff, type, value, 2);
		break;
	case ExtraSamples:
		fz_read_tiff_tag_value(&tiff->extrasamples, tiff, type, value, 1);
		break;

	case ICCProfile:
		tiff->profile = fz_malloc(tiff->ctx, count);
		/* ICC profile data type is set to UNDEFINED.
		 * TBYTE reading not correct in fz_read_tiff_tag_value */
		fz_read_tiff_bytes(tiff->profile, tiff, value, count);
		tiff->profilesize = count;
		break;

	case JPEGTables:
		fz_warn(tiff->ctx, "jpeg tables in tiff not implemented");
		tiff->jpegtables = tiff->bp + value;
		tiff->jpegtableslen = count;
		break;

	case StripOffsets:
		tiff->stripoffsets = fz_malloc_array(tiff->ctx, count, sizeof(unsigned));
		fz_read_tiff_tag_value(tiff->stripoffsets, tiff, type, value, count);
		break;

	case StripByteCounts:
		tiff->stripbytecounts = fz_malloc_array(tiff->ctx, count, sizeof(unsigned));
		fz_read_tiff_tag_value(tiff->stripbytecounts, tiff, type, value, count);
		break;

	case ColorMap:
		tiff->colormap = fz_malloc_array(tiff->ctx, count, sizeof(unsigned));
		fz_read_tiff_tag_value(tiff->colormap, tiff, type, value, count);
		break;

	case TileWidth:
	case TileLength:
	case TileOffsets:
	case TileByteCounts:
		fz_throw(tiff->ctx, "tiled tiffs not supported");

	default:
		/* printf("unknown tag: %d t=%d n=%d\n", tag, type, count); */
		break;
	}
}

static void
fz_swap_tiff_byte_order(unsigned char *buf, int n)
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
fz_decode_tiff_header(fz_context *ctx, struct tiff *tiff, unsigned char *buf, int len)
{
	unsigned version;
	unsigned offset;
	unsigned count;
	unsigned i;

	memset(tiff, 0, sizeof(struct tiff));
	tiff->ctx = ctx;
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
	tiff->order = TII;
	tiff->order = readshort(tiff);
	if (tiff->order != TII && tiff->order != TMM)
		fz_throw(tiff->ctx, "not a TIFF file, wrong magic marker");

	/* check version */
	version = readshort(tiff);
	if (version != 42)
		fz_throw(tiff->ctx, "not a TIFF file, wrong version marker");

	/* get offset of IFD */
	offset = readlong(tiff);

	/*
	 * Read IFD
	 */

	tiff->rp = tiff->bp + offset;

	count = readshort(tiff);

	offset += 2;
	for (i = 0; i < count; i++)
	{
		fz_read_tiff_tag(tiff, offset);
		offset += 12;
	}
}

fz_pixmap *
fz_load_tiff(fz_context *ctx, unsigned char *buf, int len)
{
	fz_pixmap *image;
	struct tiff tiff;

	fz_try(ctx)
	{
		fz_decode_tiff_header(ctx, &tiff, buf, len);

		/* Decode the image strips */

		if (tiff.rowsperstrip > tiff.imagelength)
			tiff.rowsperstrip = tiff.imagelength;

		fz_decode_tiff_strips(&tiff);

		/* Byte swap 16-bit images to big endian if necessary */
		if (tiff.bitspersample == 16)
			if (tiff.order == TII)
				fz_swap_tiff_byte_order(tiff.samples, tiff.imagewidth * tiff.imagelength * tiff.samplesperpixel);

		/* Expand into fz_pixmap struct */
		image = fz_new_pixmap(tiff.ctx, tiff.colorspace, tiff.imagewidth, tiff.imagelength);
		image->xres = tiff.xresolution;
		image->yres = tiff.yresolution;

		fz_unpack_tile(image, tiff.samples, tiff.samplesperpixel, tiff.bitspersample, tiff.stride, 0);

		/* We should only do this on non-pre-multiplied images, but files in the wild are bad */
		if (tiff.extrasamples /* == 2 */)
		{
			/* CMYK is a subtractive colorspace, we want additive for premul alpha */
			if (image->n == 5)
			{
				fz_pixmap *rgb = fz_new_pixmap(tiff.ctx, fz_device_rgb, image->w, image->h);
				fz_convert_pixmap(tiff.ctx, rgb, image);
				rgb->xres = image->xres;
				rgb->yres = image->yres;
				fz_drop_pixmap(ctx, image);
				image = rgb;
			}
			fz_premultiply_pixmap(ctx, image);
		}
	}
	fz_always(ctx)
	{
		/* Clean up scratch memory */
		if (tiff.colormap) fz_free(ctx, tiff.colormap);
		if (tiff.stripoffsets) fz_free(ctx, tiff.stripoffsets);
		if (tiff.stripbytecounts) fz_free(ctx, tiff.stripbytecounts);
		if (tiff.samples) fz_free(ctx, tiff.samples);
		if (tiff.profile) fz_free(ctx, tiff.profile);
	}
	fz_catch(ctx)
	{
		fz_throw(ctx, "out of memory");
	}

	return image;
}
