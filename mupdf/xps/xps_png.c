#include "fitz.h"
#include "muxps.h"

#include <zlib.h>

struct info
{
	int width, height, depth, n;
	int interlace, indexed;
	int size;
	unsigned char *samples;
	unsigned char palette[256*4];
	int transparency;
	int trns[3];
	int xres, yres;
};

static inline int getint(unsigned char *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
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

static const unsigned char png_signature[8] =
{
	137, 80, 78, 71, 13, 10, 26, 10
};

static void *zalloc(void *opaque, unsigned int items, unsigned int size)
{
	return fz_calloc(items, size);
}

static void zfree(void *opaque, void *address)
{
	fz_free(address);
}

static inline int paeth(int a, int b, int c)
{
	/* The definitions of ac and bc are correct, not a typo. */
	int ac = b - c, bc = a - c, abcc = ac + bc;
	int pa = (ac < 0 ? -ac : ac);
	int pb = (bc < 0 ? -bc : bc);
	int pc = (abcc < 0 ? -abcc : abcc);
	return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

static void
png_predict(unsigned char *samples, int width, int height, int n, int depth)
{
	int stride = (width * n * depth + 7) / 8;
	int bpp = (n * depth + 7) / 8;
	int i, row;

	for (row = 0; row < height; row ++)
	{
		unsigned char *src = samples + (stride + 1) * row;
		unsigned char *dst = samples + stride * row;

		unsigned char *a = dst;
		unsigned char *b = dst - stride;
		unsigned char *c = dst - stride;

		switch (*src++)
		{
		default:
		case 0: /* None */
			for (i = 0; i < stride; i++)
				*dst++ = *src++;
			break;

		case 1: /* Sub */
			for (i = 0; i < bpp; i++)
				*dst++ = *src++;
			for (i = bpp; i < stride; i++)
				*dst++ = *src++ + *a++;
			break;

		case 2: /* Up */
			if (row == 0)
				for (i = 0; i < stride; i++)
					*dst++ = *src++;
			else
				for (i = 0; i < stride; i++)
					*dst++ = *src++ + *b++;
			break;

		case 3: /* Average */
			if (row == 0)
			{
				for (i = 0; i < bpp; i++)
					*dst++ = *src++;
				for (i = bpp; i < stride; i++)
					*dst++ = *src++ + (*a++ >> 1);
			}
			else
			{
				for (i = 0; i < bpp; i++)
					*dst++ = *src++ + (*b++ >> 1);
				for (i = bpp; i < stride; i++)
					*dst++ = *src++ + ((*b++ + *a++) >> 1);
			}
			break;

		case 4: /* Paeth */
			if (row == 0)
			{
				for (i = 0; i < bpp; i++)
					*dst++ = *src++ + paeth(0, 0, 0);
				for (i = bpp; i < stride; i++)
					*dst++ = *src++ + paeth(*a++, 0, 0);
			}
			else
			{
				for (i = 0; i < bpp; i++)
					*dst++ = *src++ + paeth(0, *b++, 0);
				for (i = bpp; i < stride; i++)
					*dst++ = *src++ + paeth(*a++, *b++, *c++);
			}
			break;
		}
	}
}

static const int adam7_ix[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const int adam7_dx[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const int adam7_iy[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const int adam7_dy[7] = { 8, 8, 8, 4, 4, 2, 2 };

static void
png_deinterlace_passes(struct info *info, int *w, int *h, int *ofs)
{
	int p, bpp = info->depth * info->n;
	ofs[0] = 0;
	for (p = 0; p < 7; p++)
	{
		w[p] = (info->width + adam7_dx[p] - adam7_ix[p] - 1) / adam7_dx[p];
		h[p] = (info->height + adam7_dy[p] - adam7_iy[p] - 1) / adam7_dy[p];
		if (w[p] == 0) h[p] = 0;
		if (h[p] == 0) w[p] = 0;
		if (w[p] && h[p])
			ofs[p + 1] = ofs[p] + h[p] * (1 + (w[p] * bpp + 7) / 8);
		else
			ofs[p + 1] = ofs[p];
	}
}

static void
png_deinterlace(struct info *info, int *passw, int *passh, int *passofs)
{
	int n = info->n;
	int depth = info->depth;
	int stride = (info->width * n * depth + 7) / 8;
	unsigned char *output;
	int p, x, y, k;

	output = fz_calloc(info->height, stride);

	for (p = 0; p < 7; p++)
	{
		unsigned char *sp = info->samples + passofs[p];
		int w = passw[p];
		int h = passh[p];

		png_predict(sp, w, h, n, depth);
		for (y = 0; y < h; y++)
		{
			for (x = 0; x < w; x++)
			{
				int outx = x * adam7_dx[p] + adam7_ix[p];
				int outy = y * adam7_dy[p] + adam7_iy[p];
				unsigned char *dp = output + outy * stride;
				for (k = 0; k < n; k++)
				{
					int v = getcomp(sp, x * n + k, depth);
					putcomp(dp, outx * n + k, depth, v);
				}
			}
			sp += (w * depth * n + 7) / 8;
		}
	}

	fz_free(info->samples);
	info->samples = output;
}

static int
png_read_ihdr(struct info *info, unsigned char *p, int size)
{
	int color, compression, filter;

	if (size != 13)
		return fz_throw("IHDR chunk is the wrong size");

	info->width = getint(p + 0);
	info->height = getint(p + 4);
	info->depth = p[8];

	color = p[9];
	compression = p[10];
	filter = p[11];
	info->interlace = p[12];

	if (info->width <= 0)
		return fz_throw("image width must be > 0");
	if (info->height <= 0)
		return fz_throw("image height must be > 0");

	if (info->depth != 1 && info->depth != 2 && info->depth != 4 &&
			info->depth != 8 && info->depth != 16)
		return fz_throw("image bit depth must be one of 1, 2, 4, 8, 16");
	if (color == 2 && info->depth < 8)
		return fz_throw("illegal bit depth for truecolor");
	if (color == 3 && info->depth > 8)
		return fz_throw("illegal bit depth for indexed");
	if (color == 4 && info->depth < 8)
		return fz_throw("illegal bit depth for grayscale with alpha");
	if (color == 6 && info->depth < 8)
		return fz_throw("illegal bit depth for truecolor with alpha");

	info->indexed = 0;
	if (color == 0) /* gray */
		info->n = 1;
	else if (color == 2) /* rgb */
		info->n = 3;
	else if (color == 4) /* gray alpha */
		info->n = 2;
	else if (color == 6) /* rgb alpha */
		info->n = 4;
	else if (color == 3) /* indexed */
	{
		info->indexed = 1;
		info->n = 1;
	}
	else
		return fz_throw("unknown color type");

	if (compression != 0)
		return fz_throw("unknown compression method");
	if (filter != 0)
		return fz_throw("unknown filter method");
	if (info->interlace != 0 && info->interlace != 1)
		return fz_throw("interlace method not supported");

	return fz_okay;
}

static int
png_read_plte(struct info *info, unsigned char *p, int size)
{
	int n = size / 3;
	int i;

	if (n > 256 || n > (1 << info->depth))
		return fz_throw("too many samples in palette");

	for (i = 0; i < n; i++)
	{
		info->palette[i * 4] = p[i * 3];
		info->palette[i * 4 + 1] = p[i * 3 + 1];
		info->palette[i * 4 + 2] = p[i * 3 + 2];
	}

	return fz_okay;
}

static int
png_read_trns(struct info *info, unsigned char *p, int size)
{
	int i;

	info->transparency = 1;

	if (info->indexed)
	{
		if (size > 256 || size > (1 << info->depth))
			return fz_throw("too many samples in transparency table");
		for (i = 0; i < size; i++)
			info->palette[i * 4 + 3] = p[i];
	}
	else
	{
		if (size != info->n * 2)
			return fz_throw("tRNS chunk is the wrong size");
		for (i = 0; i < info->n; i++)
			info->trns[i] = (p[i * 2] << 8 | p[i * 2 + 1]) & ((1 << info->depth) - 1);
	}

	return fz_okay;
}

static int
png_read_idat(struct info *info, unsigned char *p, int size, z_stream *stm)
{
	int code;

	stm->next_in = p;
	stm->avail_in = size;

	code = inflate(stm, Z_SYNC_FLUSH);
	if (code != Z_OK && code != Z_STREAM_END)
		return fz_throw("zlib error: %s", stm->msg);
	if (stm->avail_in != 0)
	{
		if (stm->avail_out == 0)
			return fz_throw("ran out of output before input");
		return fz_throw("inflate did not consume buffer (%d remaining)", stm->avail_in);
	}

	return fz_okay;
}

static int
png_read_phys(struct info *info, unsigned char *p, int size)
{
	if (size != 9)
		return fz_throw("pHYs chunk is the wrong size");
	if (p[8] == 1)
	{
		info->xres = getint(p) * 254 / 10000;
		info->yres = getint(p + 4) * 254 / 10000;
	}
	return fz_okay;
}

static int
png_read_image(struct info *info, unsigned char *p, int total)
{
	int passw[7], passh[7], passofs[8];
	int code, size;
	z_stream stm;

	memset(info, 0, sizeof (struct info));
	memset(info->palette, 255, sizeof(info->palette));
	info->xres = 96;
	info->yres = 96;

	/* Read signature */

	if (total < 8 + 12 || memcmp(p, png_signature, 8))
		return fz_throw("not a png image (wrong signature)");

	p += 8;
	total -= 8;

	/* Read IHDR chunk (must come first) */

	size = getint(p);

	if (size + 12 > total)
		return fz_throw("premature end of data in png image");

	if (!memcmp(p + 4, "IHDR", 4))
	{
		code = png_read_ihdr(info, p + 8, size);
		if (code)
			return fz_rethrow(code, "cannot read png header");
	}
	else
		return fz_throw("png file must start with IHDR chunk");

	p += size + 12;
	total -= size + 12;

	/* Prepare output buffer */

	if (!info->interlace)
	{
		info->size = info->height * (1 + (info->width * info->n * info->depth + 7) / 8);
	}
	else
	{
		png_deinterlace_passes(info, passw, passh, passofs);
		info->size = passofs[7];
	}

	info->samples = fz_malloc(info->size);

	stm.zalloc = zalloc;
	stm.zfree = zfree;
	stm.opaque = NULL;

	stm.next_out = info->samples;
	stm.avail_out = info->size;

	code = inflateInit(&stm);
	if (code != Z_OK)
		return fz_throw("zlib error: %s", stm.msg);

	/* Read remaining chunks until IEND */

	while (total > 8)
	{
		size = getint(p);

		if (size + 12 > total)
			return fz_throw("premature end of data in png image");

		if (!memcmp(p + 4, "PLTE", 4))
		{
			code = png_read_plte(info, p + 8, size);
			if (code)
				return fz_rethrow(code, "cannot read png palette");
		}

		if (!memcmp(p + 4, "tRNS", 4))
		{
			code = png_read_trns(info, p + 8, size);
			if (code)
				return fz_rethrow(code, "cannot read png transparency");
		}

		if (!memcmp(p + 4, "pHYs", 4))
		{
			code = png_read_phys(info, p + 8, size);
			if (code)
				return fz_rethrow(code, "cannot read png resolution");
		}

		if (!memcmp(p + 4, "IDAT", 4))
		{
			code = png_read_idat(info, p + 8, size, &stm);
			if (code)
				return fz_rethrow(code, "cannot read png image data");
		}

		if (!memcmp(p + 4, "IEND", 4))
			break;

		p += size + 12;
		total -= size + 12;
	}

	code = inflateEnd(&stm);
	if (code != Z_OK)
		return fz_throw("zlib error: %s", stm.msg);

	/* Apply prediction filter and deinterlacing */

	if (!info->interlace)
		png_predict(info->samples, info->width, info->height, info->n, info->depth);
	else
		png_deinterlace(info, passw, passh, passofs);

	return fz_okay;
}

static fz_pixmap *
png_expand_palette(struct info *info, fz_pixmap *src)
{
	fz_pixmap *dst = fz_new_pixmap(fz_device_rgb, src->w, src->h);
	unsigned char *sp = src->samples;
	unsigned char *dp = dst->samples;
	int x, y;

	dst->xres = src->xres;
	dst->yres = src->yres;

	for (y = 0; y < info->height; y++)
	{
		for (x = 0; x < info->width; x++)
		{
			int v = *sp << 2;
			*dp++ = info->palette[v];
			*dp++ = info->palette[v + 1];
			*dp++ = info->palette[v + 2];
			*dp++ = info->palette[v + 3];
			sp += 2;
		}
	}

	fz_drop_pixmap(src);
	return dst;
}

static void
png_mask_transparency(struct info *info, fz_pixmap *dst)
{
	int stride = (info->width * info->n * info->depth + 7) / 8;
	int depth = info->depth;
	int n = info->n;
	int x, y, k, t;

	for (y = 0; y < info->height; y++)
	{
		unsigned char *sp = info->samples + y * stride;
		unsigned char *dp = dst->samples + y * dst->w * dst->n;
		for (x = 0; x < info->width; x++)
		{
			t = 1;
			for (k = 0; k < n; k++)
				if (getcomp(sp, x * n + k, depth) != info->trns[k])
					t = 0;
			if (t)
				dp[x * dst->n + dst->n - 1] = 0;
		}
	}
}

int
xps_decode_png(fz_pixmap **imagep, byte *p, int total)
{
	fz_pixmap *image;
	fz_colorspace *colorspace;
	struct info png;
	int code;
	int stride;

	code = png_read_image(&png, p, total);
	if (code)
		return fz_rethrow(code, "cannot read png image");

	if (png.n == 3 || png.n == 4)
		colorspace = fz_device_rgb;
	else
		colorspace = fz_device_gray;

	stride = (png.width * png.n * png.depth + 7) / 8;

	image = fz_new_pixmap_with_limit(colorspace, png.width, png.height);
	if (!image)
	{
		fz_free(png.samples);
		return fz_throw("out of memory");
	}

	image->xres = png.xres;
	image->yres = png.yres;

	fz_unpack_tile(image, png.samples, png.n, png.depth, stride, png.indexed);

	if (png.indexed)
		image = png_expand_palette(&png, image);
	else if (png.transparency)
		png_mask_transparency(&png, image);

	if (png.transparency || png.n == 2 || png.n == 4)
		fz_premultiply_pixmap(image);

	fz_free(png.samples);

	*imagep = image;
	return fz_okay;
}
