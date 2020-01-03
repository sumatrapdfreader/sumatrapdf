#include "mupdf/fitz.h"

#include <zlib.h>

#include <limits.h>
#include <string.h>

struct info
{
	unsigned int width, height, depth, n;
	enum fz_colorspace_type type;
	int interlace, indexed;
	unsigned int size;
	unsigned char *samples;
	unsigned char palette[256*4];
	int transparency;
	int trns[3];
	int xres, yres;
	fz_colorspace *cs;
};

static inline unsigned int getuint(const unsigned char *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static inline int getcomp(const unsigned char *line, int x, int bpc)
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
png_predict(unsigned char *samples, unsigned int width, unsigned int height, unsigned int n, unsigned int depth)
{
	unsigned int stride = (width * n * depth + 7) / 8;
	unsigned int bpp = (n * depth + 7) / 8;
	unsigned int i, row;

	for (row = 0; row < height; row ++)
	{
		unsigned char *src = samples + (unsigned int)((stride + 1) * row);
		unsigned char *dst = samples + (unsigned int)(stride * row);

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

static const unsigned int adam7_ix[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const unsigned int adam7_dx[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const unsigned int adam7_iy[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const unsigned int adam7_dy[7] = { 8, 8, 8, 4, 4, 2, 2 };

static void
png_deinterlace_passes(fz_context *ctx, struct info *info, unsigned int *w, unsigned int *h, unsigned int *ofs)
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
png_deinterlace(fz_context *ctx, struct info *info, unsigned int *passw, unsigned int *passh, unsigned int *passofs)
{
	unsigned int n = info->n;
	unsigned int depth = info->depth;
	unsigned int stride = (info->width * n * depth + 7) / 8;
	unsigned char *output;
	unsigned int p, x, y, k;

	if (info->height > UINT_MAX / stride)
		fz_throw(ctx, FZ_ERROR_MEMORY, "image too large");
	output = Memento_label(fz_malloc(ctx, info->height * stride), "png_deinterlace");

	for (p = 0; p < 7; p++)
	{
		unsigned char *sp = info->samples + (passofs[p]);
		unsigned int w = passw[p];
		unsigned int h = passh[p];

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

	fz_free(ctx, info->samples);
	info->samples = output;
}

static void
png_read_ihdr(fz_context *ctx, struct info *info, const unsigned char *p, unsigned int size)
{
	int color, compression, filter;

	if (size != 13)
		fz_throw(ctx, FZ_ERROR_GENERIC, "IHDR chunk is the wrong size");

	info->width = getuint(p + 0);
	info->height = getuint(p + 4);
	info->depth = p[8];

	color = p[9];
	compression = p[10];
	filter = p[11];
	info->interlace = p[12];

	if (info->width <= 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image width must be > 0");
	if (info->height <= 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image height must be > 0");

	if (info->depth != 1 && info->depth != 2 && info->depth != 4 &&
			info->depth != 8 && info->depth != 16)
		fz_throw(ctx, FZ_ERROR_GENERIC, "image bit depth must be one of 1, 2, 4, 8, 16");
	if (color == 2 && info->depth < 8)
		fz_throw(ctx, FZ_ERROR_GENERIC, "illegal bit depth for truecolor");
	if (color == 3 && info->depth > 8)
		fz_throw(ctx, FZ_ERROR_GENERIC, "illegal bit depth for indexed");
	if (color == 4 && info->depth < 8)
		fz_throw(ctx, FZ_ERROR_GENERIC, "illegal bit depth for grayscale with alpha");
	if (color == 6 && info->depth < 8)
		fz_throw(ctx, FZ_ERROR_GENERIC, "illegal bit depth for truecolor with alpha");

	info->indexed = 0;
	if (color == 0) /* gray */
		info->n = 1, info->type = FZ_COLORSPACE_GRAY;
	else if (color == 2) /* rgb */
		info->n = 3, info->type = FZ_COLORSPACE_RGB;
	else if (color == 4) /* gray alpha */
		info->n = 2, info->type = FZ_COLORSPACE_GRAY;
	else if (color == 6) /* rgb alpha */
		info->n = 4, info->type = FZ_COLORSPACE_RGB;
	else if (color == 3) /* indexed */
	{
		info->type = FZ_COLORSPACE_RGB; /* after colorspace expansion it will be */
		info->indexed = 1;
		info->n = 1;
	}
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown color type");

	if (compression != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown compression method");
	if (filter != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "unknown filter method");
	if (info->interlace != 0 && info->interlace != 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "interlace method not supported");
	if (info->height > UINT_MAX / info->width / info->n / (info->depth / 8 + 1))
		fz_throw(ctx, FZ_ERROR_GENERIC, "image dimensions might overflow");
}

static void
png_read_plte(fz_context *ctx, struct info *info, const unsigned char *p, unsigned int size)
{
	int n = size / 3;
	int i;

	if (n > 256)
	{
		fz_warn(ctx, "too many samples in palette");
		n = 256;
	}

	for (i = 0; i < n; i++)
	{
		info->palette[i * 4] = p[i * 3];
		info->palette[i * 4 + 1] = p[i * 3 + 1];
		info->palette[i * 4 + 2] = p[i * 3 + 2];
	}

	/* Fill in any missing palette entries */
	for (; i < 256; i++)
	{
		info->palette[i * 4] = 0;
		info->palette[i * 4 + 1] = 0;
		info->palette[i * 4 + 2] = 0;
	}
}

static void
png_read_trns(fz_context *ctx, struct info *info, const unsigned char *p, unsigned int size)
{
	unsigned int i;

	info->transparency = 1;

	if (info->indexed)
	{
		if (size > 256)
		{
			fz_warn(ctx, "too many samples in transparency table");
			size = 256;
		}
		for (i = 0; i < size; i++)
			info->palette[i * 4 + 3] = p[i];
		/* Fill in any missing entries */
		for (; i < 256; i++)
			info->palette[i * 4 + 3] = 255;
	}
	else
	{
		if (size != info->n * 2)
			fz_throw(ctx, FZ_ERROR_GENERIC, "tRNS chunk is the wrong size");
		for (i = 0; i < info->n; i++)
			info->trns[i] = (p[i * 2] << 8 | p[i * 2 + 1]) & ((1 << info->depth) - 1);
	}
}

static void
png_read_icc(fz_context *ctx, struct info *info, const unsigned char *p, unsigned int size)
{
#if FZ_ENABLE_ICC
	fz_stream *mstm = NULL, *zstm = NULL;
	fz_colorspace *cs = NULL;
	fz_buffer *buf = NULL;
	size_t m = fz_mini(80, size);
	size_t n = fz_strnlen((const char *)p, m);
	if (n + 2 > m)
	{
		fz_warn(ctx, "invalid ICC profile name");
		return;
	}

	fz_var(mstm);
	fz_var(zstm);
	fz_var(buf);

	fz_try(ctx)
	{
		mstm = fz_open_memory(ctx, p + n + 2, size - n - 2);
		zstm = fz_open_flated(ctx, mstm, 15);
		buf = fz_read_all(ctx, zstm, 0);
		cs = fz_new_icc_colorspace(ctx, info->type, 0, NULL, buf);
		fz_drop_colorspace(ctx, info->cs);
		info->cs = cs;
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_drop_stream(ctx, zstm);
		fz_drop_stream(ctx, mstm);
	}
	fz_catch(ctx)
		fz_warn(ctx, "ignoring embedded ICC profile in PNG");
#endif
}

static void
png_read_idat(fz_context *ctx, struct info *info, const unsigned char *p, unsigned int size, z_stream *stm)
{
	int code;

	stm->next_in = (Bytef*)p;
	stm->avail_in = size;

	code = inflate(stm, Z_SYNC_FLUSH);
	if (code != Z_OK && code != Z_STREAM_END)
		fz_throw(ctx, FZ_ERROR_GENERIC, "zlib error: %s", stm->msg);
	if (stm->avail_in != 0)
	{
		if (stm->avail_out == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "ran out of output before input");
		fz_throw(ctx, FZ_ERROR_GENERIC, "inflate did not consume buffer (%d remaining)", stm->avail_in);
	}
}

static void
png_read_phys(fz_context *ctx, struct info *info, const unsigned char *p, unsigned int size)
{
	if (size != 9)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pHYs chunk is the wrong size");
	if (p[8] == 1)
	{
		info->xres = (getuint(p) * 254 + 5000) / 10000;
		info->yres = (getuint(p + 4) * 254 + 5000) / 10000;
	}
}

static void
png_read_image(fz_context *ctx, struct info *info, const unsigned char *p, size_t total, int only_metadata)
{
	unsigned int passw[7], passh[7], passofs[8];
	unsigned int code, size;
	z_stream stm;

	memset(info, 0, sizeof (struct info));
	memset(info->palette, 255, sizeof(info->palette));
	info->xres = 96;
	info->yres = 96;

	/* Read signature */

	if (total < 8 + 12 || memcmp(p, png_signature, 8))
		fz_throw(ctx, FZ_ERROR_GENERIC, "not a png image (wrong signature)");

	p += 8;
	total -= 8;

	/* Read IHDR chunk (must come first) */

	size = getuint(p);
	if (total < 12 || size > total - 12)
		fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in png image");

	if (!memcmp(p + 4, "IHDR", 4))
		png_read_ihdr(ctx, info, p + 8, size);
	else
		fz_throw(ctx, FZ_ERROR_GENERIC, "png file must start with IHDR chunk");

	p += size + 12;
	total -= size + 12;

	/* Prepare output buffer */
	if (!only_metadata)
	{
		if (!info->interlace)
		{
			info->size = info->height * (1 + (info->width * info->n * info->depth + 7) / 8);
		}
		else
		{
			png_deinterlace_passes(ctx, info, passw, passh, passofs);
			info->size = passofs[7];
		}

		info->samples = Memento_label(fz_malloc(ctx, info->size), "png_samples");

		stm.zalloc = fz_zlib_alloc;
		stm.zfree = fz_zlib_free;
		stm.opaque = ctx;

		stm.next_out = info->samples;
		stm.avail_out = info->size;

		code = inflateInit(&stm);
		if (code != Z_OK)
			fz_throw(ctx, FZ_ERROR_GENERIC, "zlib error: %s", stm.msg);
	}

	fz_try(ctx)
	{
		/* Read remaining chunks until IEND */
		while (total > 8)
		{
			size = getuint(p);

			if (total < 12 || size > total - 12)
				fz_throw(ctx, FZ_ERROR_GENERIC, "premature end of data in png image");

			if (!memcmp(p + 4, "PLTE", 4) && !only_metadata)
				png_read_plte(ctx, info, p + 8, size);
			if (!memcmp(p + 4, "tRNS", 4) && !only_metadata)
				png_read_trns(ctx, info, p + 8, size);
			if (!memcmp(p + 4, "pHYs", 4))
				png_read_phys(ctx, info, p + 8, size);
			if (!memcmp(p + 4, "IDAT", 4) && !only_metadata)
				png_read_idat(ctx, info, p + 8, size, &stm);
			if (!memcmp(p + 4, "iCCP", 4))
				png_read_icc(ctx, info, p + 8, size);
			if (!memcmp(p + 4, "IEND", 4))
				break;

			p += size + 12;
			total -= size + 12;
		}
		if (!only_metadata && stm.avail_out != 0)
		{
			memset(stm.next_out, 0xff, stm.avail_out);
			fz_warn(ctx, "missing pixel data in png image; possibly truncated");
		}
		else if (total <= 8)
			fz_warn(ctx, "missing IEND chunk in png image; possibly truncated");
	}
	fz_catch(ctx)
	{
		if (!only_metadata)
		{
			inflateEnd(&stm);
			fz_free(ctx, info->samples);
			info->samples = NULL;
		}
		fz_rethrow(ctx);
	}

	if (!only_metadata)
	{
		code = inflateEnd(&stm);
		if (code != Z_OK)
		{
			fz_free(ctx, info->samples);
			info->samples = NULL;
			fz_throw(ctx, FZ_ERROR_GENERIC, "zlib error: %s", stm.msg);
		}

		/* Apply prediction filter and deinterlacing */
		fz_try(ctx)
		{
			if (!info->interlace)
				png_predict(info->samples, info->width, info->height, info->n, info->depth);
			else
				png_deinterlace(ctx, info, passw, passh, passofs);
		}
		fz_catch(ctx)
		{
			fz_free(ctx, info->samples);
			info->samples = NULL;
			fz_rethrow(ctx);
		}
	}

	if (info->cs && fz_colorspace_type(ctx, info->cs) != info->type)
	{
		fz_warn(ctx, "embedded ICC profile does not match PNG colorspace");
		fz_drop_colorspace(ctx, info->cs);
		info->cs = NULL;
	}

	if (info->cs == NULL)
	{
		if (info->n == 3 || info->n == 4 || info->indexed)
			info->cs = fz_keep_colorspace(ctx, fz_device_rgb(ctx));
		else
			info->cs = fz_keep_colorspace(ctx, fz_device_gray(ctx));
	}
}

static fz_pixmap *
png_expand_palette(fz_context *ctx, struct info *info, fz_pixmap *src)
{
	fz_pixmap *dst = fz_new_pixmap(ctx, info->cs, src->w, src->h, NULL, info->transparency);
	unsigned char *sp = src->samples;
	unsigned char *dp = dst->samples;
	unsigned int x, y;
	int dstride = dst->stride - dst->w * dst->n;
	int sstride = src->stride - src->w * src->n;

	dst->xres = src->xres;
	dst->yres = src->yres;

	for (y = info->height; y > 0; y--)
	{
		for (x = info->width; x > 0; x--)
		{
			int v = *sp << 2;
			*dp++ = info->palette[v];
			*dp++ = info->palette[v + 1];
			*dp++ = info->palette[v + 2];
			if (info->transparency)
				*dp++ = info->palette[v + 3];
			++sp;
		}
		sp += sstride;
		dp += dstride;
	}

	fz_drop_pixmap(ctx, src);
	return dst;
}

static void
png_mask_transparency(struct info *info, fz_pixmap *dst)
{
	unsigned int stride = (info->width * info->n * info->depth + 7) / 8;
	unsigned int depth = info->depth;
	unsigned int n = info->n;
	unsigned int x, y, k, t;

	for (y = 0; y < info->height; y++)
	{
		unsigned char *sp = info->samples + (unsigned int)(y * stride);
		unsigned char *dp = dst->samples + (unsigned int)(y * dst->stride);
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

fz_pixmap *
fz_load_png(fz_context *ctx, const unsigned char *p, size_t total)
{
	fz_pixmap *image = NULL;
	struct info png;
	int stride;
	int alpha;

	fz_var(image);

	fz_try(ctx)
	{
		png_read_image(ctx, &png, p, total, 0);

		stride = (png.width * png.n * png.depth + 7) / 8;
		alpha = (png.n == 2 || png.n == 4 || png.transparency);

		if (png.indexed)
		{
			image = fz_new_pixmap(ctx, NULL, png.width, png.height, NULL, 1);
			fz_unpack_tile(ctx, image, png.samples, png.n, png.depth, stride, 1);
			image = png_expand_palette(ctx, &png, image);
		}
		else
		{
			image = fz_new_pixmap(ctx, png.cs, png.width, png.height, NULL, alpha);
			fz_unpack_tile(ctx, image, png.samples, png.n, png.depth, stride, 0);
			if (png.transparency)
				png_mask_transparency(&png, image);
		}
		if (alpha)
			fz_premultiply_pixmap(ctx, image);
		fz_set_pixmap_resolution(ctx, image, png.xres, png.yres);
	}
	fz_always(ctx)
	{
		fz_drop_colorspace(ctx, png.cs);
		fz_free(ctx, png.samples);
	}
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, image);
		fz_rethrow(ctx);
	}

	return image;
}

void
fz_load_png_info(fz_context *ctx, const unsigned char *p, size_t total, int *wp, int *hp, int *xresp, int *yresp, fz_colorspace **cspacep)
{
	struct info png;

	fz_try(ctx)
		png_read_image(ctx, &png, p, total, 1);
	fz_catch(ctx)
	{
		fz_drop_colorspace(ctx, png.cs);
		fz_rethrow(ctx);
	}

	*cspacep = png.cs;
	*wp = png.width;
	*hp = png.height;
	*xresp = png.xres;
	*yresp = png.xres;
}
