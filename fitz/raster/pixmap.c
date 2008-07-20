#include "fitz-base.h"
#include "fitz-world.h"
#include "fitz-draw.h"

fz_error *
fz_newpixmap(fz_pixmap **pixp, int x, int y, int w, int h, int n)
{
	fz_pixmap *pix;

	pix = *pixp = fz_malloc(sizeof(fz_pixmap));
	if (!pix)
		return fz_outofmem;

	pix->x = x;
	pix->y = y;
	pix->w = w;
	pix->h = h;
	pix->n = n;

	pix->samples = fz_malloc(pix->w * pix->h * pix->n * sizeof(fz_sample));
	if (!pix->samples) {
		fz_free(pix);
		return fz_outofmem;
	}

	return fz_okay;
}

fz_error *
fz_newpixmapwithrect(fz_pixmap **pixp, fz_irect r, int n)
{
	return fz_newpixmap(pixp,
				r.x0, r.y0,
				r.x1 - r.x0,
				r.y1 - r.y0, n);
}

fz_error *
fz_newpixmapcopy(fz_pixmap **pixp, fz_pixmap *old)
{
	fz_error *error;
	error = fz_newpixmap(pixp, old->x, old->y, old->w, old->h, old->n);
	if (error)
		return error;
	memcpy((*pixp)->samples, old->samples, old->w * old->h * old->n);
	return fz_okay;
}

void
fz_droppixmap(fz_pixmap *pix)
{
	fz_free(pix->samples);
	fz_free(pix);
}

void
fz_clearpixmap(fz_pixmap *pix)
{
	memset(pix->samples, 0, pix->w * pix->h * pix->n * sizeof(fz_sample));
}

void
fz_gammapixmap(fz_pixmap *pix, float gamma)
{
	unsigned char table[255];
	int n = pix->w * pix->h * pix->n;
	unsigned char *p = pix->samples;
	int i;
	for (i = 0; i < 256; i++)
		table[i] = CLAMP(pow(i / 255.0, gamma) * 255.0, 0, 255);
	while (n--)
		*p = table[*p]; p++;
}

#ifdef _MSC_VER
/* TODO: quick hack to fix msvc compilation problem due to F_OK not being
   defined. A proper fix would be to implement file_exists() functions
   in a portable way. */
void
fz_debugpixmap(fz_pixmap *pix, char *prefix)
{
}
#else
void
fz_debugpixmap(fz_pixmap *pix, char *prefix)
{
	int hasalpha = pix->n > 1;
	FILE *image = NULL;
	FILE *alpha = NULL;
	int i = 0;
	int x;
	int y;

	do
	{
		char imagename[40];
		char alphaname[40];

		if (pix->n == 1 || pix->n == 2)
			sprintf(imagename, "%s-%04d-image.pgm", prefix, i);
		else
			sprintf(imagename, "%s-%04d-image.ppm", prefix, i);
		if (hasalpha)
			sprintf(alphaname, "%s-%04d-alpha.pgm", prefix, i);

		if (access(imagename, F_OK) == 0 ||
				(hasalpha && access(alphaname, F_OK) == 0))
		{
			i++;
			continue;
		}

		image = fopen(imagename, "wb");
		if (hasalpha)
			alpha = fopen(alphaname, "wb");
	} while (image == NULL || (hasalpha && alpha == NULL));

	if (pix->n == 5)
	{
		fprintf(image, "P6\n%d %d\n255\n", pix->w, pix->h);
		fprintf(alpha, "P5\n%d %d\n255\n", pix->w, pix->h);

		for (y = 0; y < pix->h; y++)
			for (x = 0; x < pix->w; x++)
			{
				int a = pix->samples[x * pix->n + y * pix->w * pix->n + 0];
				int cc = pix->samples[x * pix->n + y * pix->w * pix->n + 1];
				int mm = pix->samples[x * pix->n + y * pix->w * pix->n + 2];
				int yy = pix->samples[x * pix->n + y * pix->w * pix->n + 3];
				int kk = pix->samples[x * pix->n + y * pix->w * pix->n + 4];
				int r = 255 - MIN(cc + kk, 255);
				int g = 255 - MIN(mm + kk, 255);
				int b = 255 - MIN(yy + kk, 255);
				fputc(a, alpha);
				fputc(r, image);
				fputc(g, image);
				fputc(b, image);
			}
	}

	else if (pix->n == 4)
	{
		fprintf(image, "P6\n%d %d\n255\n", pix->w, pix->h);
		fprintf(alpha, "P5\n%d %d\n255\n", pix->w, pix->h);

		for (y = 0; y < pix->h; y++)
			for (x = 0; x < pix->w; x++)
			{
				int a = pix->samples[x * pix->n + y * pix->w * pix->n + 0];
				int r = pix->samples[x * pix->n + y * pix->w * pix->n + 1];
				int g = pix->samples[x * pix->n + y * pix->w * pix->n + 2];
				int b = pix->samples[x * pix->n + y * pix->w * pix->n + 3];
				fputc(a, alpha);
				fputc(r, image);
				fputc(g, image);
				fputc(b, image);
			}
	}

	else if (pix->n == 2)
	{
		fprintf(image, "P5\n%d %d\n255\n", pix->w, pix->h);
		fprintf(alpha, "P5\n%d %d\n255\n", pix->w, pix->h);

		for (y = 0; y < pix->h; y++)
			for (x = 0; x < pix->w; x++)
			{
				int a = pix->samples[x * pix->n + y * pix->w * pix->n + 0];
				int g = pix->samples[x * pix->n + y * pix->w * pix->n + 1];
				fputc(a, alpha);
				fputc(g, image);
			}
	}

	else if (pix->n == 1)
	{
		fprintf(image, "P5\n%d %d\n255\n", pix->w, pix->h);

		for (y = 0; y < pix->h; y++)
			for (x = 0; x < pix->w; x++)
			{
				int g = pix->samples[x * pix->n + y * pix->w * pix->n + 1];
				fputc(g, image);
			}
	}

	if (hasalpha)
		fclose(alpha);
	fclose(image);
}
#endif
