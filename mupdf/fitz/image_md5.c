#include "fitz-internal.h"

void fz_md5_pixmap(fz_pixmap *pix, unsigned char digest[16])
{
	fz_md5 md5;

	fz_md5_init(&md5);
	if (pix)
		fz_md5_update(&md5, pix->samples, pix->w * pix->h * pix->n);
	fz_md5_final(&md5, digest);
}

/* SumatraPDF: generalize fz_md5_pixmap for e.g. bitmaps */
void fz_md5_data(void *data, int len, unsigned char digest[16])
{
	fz_md5 md5;

	fz_md5_init(&md5);
	fz_md5_update(&md5, data, len);
	fz_md5_final(&md5, digest);
}
