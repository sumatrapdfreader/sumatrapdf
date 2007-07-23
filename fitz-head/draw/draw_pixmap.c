#include "fitz-base.h"
#include "fitz-world.h"
#include "fitz-draw.h"

fz_error *
fz_newpixmap(fz_pixmap **pixp, int x, int y, int w, int h, int n, int a)
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
	pix->a = a;
	pix->s = pix->w * (pix->n + pix->a);

	pix->p = fz_malloc(pix->h * pix->s * sizeof(fz_sample));
	if (!pix->p) {
		fz_free(pix);
		return fz_outofmem;
	}

	return nil;
}

fz_error *
fz_newpixmapwithrect(fz_pixmap **pixp, fz_irect r, int n, int a)
{
	return fz_newpixmap(pixp,
				r.x0, r.y0,
				r.x1 - r.x0,
				r.y1 - r.y0, n, a);
}

fz_error *
fz_newpixmapcopy(fz_pixmap **pixp, fz_pixmap *old)
{
	fz_error *error;
	error = fz_newpixmap(pixp, old->x, old->y, old->w, old->h, old->n, old->a);
	if (error)
		return error;
	memcpy((*pixp)->p, old->p, old->w * old->h * (old->n + old->a));
	return nil;
}

void
fz_droppixmap(fz_pixmap *pix)
{
	fz_free(pix->p);
	fz_free(pix);
}

void
fz_clearpixmap(fz_pixmap *pix)
{
	memset(pix->p, 0, pix->h * pix->s * sizeof(fz_sample));
}

fz_irect
fz_boundpixmap(fz_pixmap *pix)
{
	fz_irect r;
	r.x0 = pix->x;
	r.y0 = pix->y;
	r.x1 = pix->x + pix->w;
	r.y1 = pix->y + pix->h;
	return r;
}

void
fz_gammapixmap(fz_pixmap *pix, float gamma)
{
	unsigned char table[255];
	int n = pix->w * pix->h * pix->n;
	unsigned char *p = pix->p;
	int i;
	for (i = 0; i < 256; i++)
		table[i] = CLAMP(pow(i / 255.0, gamma) * 255.0, 0, 255);
	while (n--)
		*p = table[*p]; p++;
}

void
fz_debugpixmap(fz_pixmap *pix, char *name)
{
	if (pix->n == 3)
	{
		int x, y;
		FILE *ppm = fopen(name, "wb");
printf("* pixmap '%s' is rgb\n", name);
		fprintf(ppm, "P6\n%d %d\n255\n", pix->w, pix->h);

		for (y = 0; y < pix->h; y++)
			for (x = 0; x < pix->w; x++)
			{
				int r = pix->p[x * (pix->n + pix->a) + y * pix->s + 0];
				int g = pix->p[x * (pix->n + pix->a) + y * pix->s + 1];
				int b = pix->p[x * (pix->n + pix->a) + y * pix->s + 2];
				putc(r, ppm);
				putc(g, ppm);
				putc(b, ppm);
			}
		fclose(ppm);
	}

	else if (pix->n == 1 || (pix->n == 0 && pix->a == 1))
	{
		int x, y;
		FILE *pgm = fopen(name, "wb");
printf("* pixmap '%s' is gray\n", name);
		fprintf(pgm, "P5\n%d %d\n255\n", pix->w, pix->h);
		for (y = 0; y < pix->h; y++)
			for (x = 0; x < pix->w; x++)
			{
				putc(pix->p[y * pix->s + x * (pix->n + pix->a) + 0], pgm);
			}
		fclose(pgm);
	}

	else
		printf("* pixmap '%s' is unknown format n %d a %d\n", name, pix->n, pix->a);
}

