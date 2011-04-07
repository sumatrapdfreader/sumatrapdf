#include "fitz.h"

fz_bitmap *
fz_new_bitmap(int w, int h, int n)
{
	fz_bitmap *bit;

	bit = fz_malloc(sizeof(fz_bitmap));
	bit->refs = 1;
	bit->w = w;
	bit->h = h;
	bit->n = n;
	/* Span is 32 bit aligned. We may want to make this 64 bit if we
	 * use SSE2 etc. */
	bit->stride = ((n * w + 31) & ~31) >> 3;

	bit->samples = fz_calloc(h, bit->stride);

	return bit;
}

fz_bitmap *
fz_keep_bitmap(fz_bitmap *pix)
{
	pix->refs++;
	return pix;
}

void
fz_drop_bitmap(fz_bitmap *bit)
{
	if (bit && --bit->refs == 0)
	{
		fz_free(bit->samples);
		fz_free(bit);
	}
}

void
fz_clear_bitmap(fz_bitmap *bit)
{
	memset(bit->samples, 0, bit->stride * bit->h);
}

/*
 * Write bitmap to PBM file
 */

fz_error
fz_write_pbm(fz_bitmap *bitmap, char *filename)
{
	FILE *fp;
	unsigned char *p;
	int h, bytestride;

	fp = fopen(filename, "wb");
	if (!fp)
		return fz_throw("cannot open file '%s': %s", filename, strerror(errno));

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
	return fz_okay;
}
