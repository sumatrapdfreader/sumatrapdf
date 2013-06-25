#include "mupdf/fitz.h"

fz_bitmap *
fz_new_bitmap(fz_context *ctx, int w, int h, int n, int xres, int yres)
{
	fz_bitmap *bit;

	bit = fz_malloc_struct(ctx, fz_bitmap);
	bit->refs = 1;
	bit->w = w;
	bit->h = h;
	bit->n = n;
	bit->xres = xres;
	bit->yres = yres;
	/* Span is 32 bit aligned. We may want to make this 64 bit if we
	 * use SSE2 etc. */
	bit->stride = ((n * w + 31) & ~31) >> 3;

	bit->samples = fz_malloc_array(ctx, h, bit->stride);

	return bit;
}

fz_bitmap *
fz_keep_bitmap(fz_context *ctx, fz_bitmap *bit)
{
	if (bit)
		bit->refs++;
	return bit;
}

void
fz_drop_bitmap(fz_context *ctx, fz_bitmap *bit)
{
	if (bit && --bit->refs == 0)
	{
		fz_free(ctx, bit->samples);
		fz_free(ctx, bit);
	}
}

void
fz_clear_bitmap(fz_context *ctx, fz_bitmap *bit)
{
	memset(bit->samples, 0, bit->stride * bit->h);
}

/*
 * Write bitmap to PBM file
 */

void
fz_write_pbm(fz_context *ctx, fz_bitmap *bitmap, char *filename)
{
	FILE *fp;
	unsigned char *p;
	int h, bytestride;

	fp = fopen(filename, "wb");
	if (!fp)
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot open file '%s': %s", filename, strerror(errno));

	assert(bitmap->n == 1);

	fprintf(fp, "P4\n%d %d\n", bitmap->w, bitmap->h);

	p = bitmap->samples;

	h = bitmap->h;
	bytestride = (bitmap->w + 7) >> 3;
	while (h--)
	{
		fwrite(p, 1, bytestride, fp);
		p += bitmap->stride;
	}

	fclose(fp);
}

fz_colorspace *fz_pixmap_colorspace(fz_context *ctx, fz_pixmap *pix)
{
	if (!pix)
		return NULL;
	return pix->colorspace;
}

int fz_pixmap_components(fz_context *ctx, fz_pixmap *pix)
{
	if (!pix)
		return 0;
	return pix->n;
}

unsigned char *fz_pixmap_samples(fz_context *ctx, fz_pixmap *pix)
{
	if (!pix)
		return NULL;
	return pix->samples;
}

void fz_bitmap_details(fz_bitmap *bit, int *w, int *h, int *n, int *stride)
{
	if (!bit)
	{
		if (w)
			*w = 0;
		if (h)
			*h = 0;
		if (n)
			*n = 0;
		if (stride)
			*stride = 0;
		return;
	}
	if (w)
		*w = bit->w;
	if (h)
		*h = bit->h;
	if (n)
		*n = bit->n;
	if (stride)
		*stride = bit->stride;
}
