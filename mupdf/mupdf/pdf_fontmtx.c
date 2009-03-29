#include "fitz.h"
#include "mupdf.h"

void
pdf_setfontwmode(pdf_fontdesc *font, int wmode)
{
	font->wmode = wmode;
}

void
pdf_setdefaulthmtx(pdf_fontdesc *font, int w)
{
	font->dhmtx.w = w;
}

void
pdf_setdefaultvmtx(pdf_fontdesc *font, int y, int w)
{
	font->dvmtx.y = y;
	font->dvmtx.w = w;
}

fz_error
pdf_addhmtx(pdf_fontdesc *font, int lo, int hi, int w)
{
	int newcap;
	pdf_hmtx *newmtx;

	if (font->nhmtx + 1 >= font->hmtxcap)
	{
		newcap = font->hmtxcap + 16;
		newmtx = fz_realloc(font->hmtx, sizeof(pdf_hmtx) * newcap);
		if (!newmtx)
			return fz_rethrow(-1, "out of memory");
		font->hmtxcap = newcap;
		font->hmtx = newmtx;
	}

	font->hmtx[font->nhmtx].lo = lo;
	font->hmtx[font->nhmtx].hi = hi;
	font->hmtx[font->nhmtx].w = w;
	font->nhmtx++;

	return fz_okay;
}

fz_error
pdf_addvmtx(pdf_fontdesc *font, int lo, int hi, int x, int y, int w)
{
	int newcap;
	pdf_vmtx *newmtx;

	if (font->nvmtx + 1 >= font->vmtxcap)
	{
		newcap = font->vmtxcap + 16;
		newmtx = fz_realloc(font->vmtx, sizeof(pdf_vmtx) * newcap);
		if (!newmtx)
			return fz_rethrow(-1, "out of memory");
		font->vmtxcap = newcap;
		font->vmtx = newmtx;
	}

	font->vmtx[font->nvmtx].lo = lo;
	font->vmtx[font->nvmtx].hi = hi;
	font->vmtx[font->nvmtx].x = x;
	font->vmtx[font->nvmtx].y = y;
	font->vmtx[font->nvmtx].w = w;
	font->nvmtx++;

	return fz_okay;
}

static int cmph(const void *a0, const void *b0)
{
	pdf_hmtx *a = (pdf_hmtx*)a0;
	pdf_hmtx *b = (pdf_hmtx*)b0;
	return a->lo - b->lo;
}

static int cmpv(const void *a0, const void *b0)
{
	pdf_vmtx *a = (pdf_vmtx*)a0;
	pdf_vmtx *b = (pdf_vmtx*)b0;
	return a->lo - b->lo;
}

fz_error
pdf_endhmtx(pdf_fontdesc *font)
{
	pdf_hmtx *newmtx;

	if (!font->hmtx)
		return fz_okay;

	qsort(font->hmtx, font->nhmtx, sizeof(pdf_hmtx), cmph);

	newmtx = fz_realloc(font->hmtx, sizeof(pdf_hmtx) * font->nhmtx);
	if (!newmtx)
		return fz_rethrow(-1, "out of memory");
	font->hmtxcap = font->nhmtx;
	font->hmtx = newmtx;

	return fz_okay;
}

fz_error
pdf_endvmtx(pdf_fontdesc *font)
{
	pdf_vmtx *newmtx;

	if (!font->vmtx)
		return fz_okay;

	qsort(font->vmtx, font->nvmtx, sizeof(pdf_vmtx), cmpv);

	newmtx = fz_realloc(font->vmtx, sizeof(pdf_vmtx) * font->nvmtx);
	if (!newmtx)
		return fz_rethrow(-1, "out of memory");
	font->vmtxcap = font->nvmtx;
	font->vmtx = newmtx;

	return fz_okay;
}

pdf_hmtx
pdf_gethmtx(pdf_fontdesc *font, int cid)
{
	int l = 0;
	int r = font->nhmtx - 1;
	int m;

	if (!font->hmtx)
		goto notfound;

	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cid < font->hmtx[m].lo)
			r = m - 1;
		else if (cid > font->hmtx[m].hi)
			l = m + 1;
		else
			return font->hmtx[m];
	}

notfound:
	return font->dhmtx;
}

pdf_vmtx
pdf_getvmtx(pdf_fontdesc *font, int cid)
{
	pdf_hmtx h;
	pdf_vmtx v;
	int l = 0;
	int r = font->nvmtx - 1;
	int m;

	if (!font->vmtx)
		goto notfound;

	while (l <= r)
	{
		m = (l + r) >> 1;
		if (cid < font->vmtx[m].lo)
			r = m - 1;
		else if (cid > font->vmtx[m].hi)
			l = m + 1;
		else
			return font->vmtx[m];
	}

notfound:
	h = pdf_gethmtx(font, cid);
	v = font->dvmtx;
	v.x = h.w / 2;
	return v;
}

