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

void
pdf_addhmtx(pdf_fontdesc *font, int lo, int hi, int w)
{
	if (font->nhmtx + 1 >= font->hmtxcap)
	{
		font->hmtxcap = font->hmtxcap + 16;
		font->hmtx = fz_realloc(font->hmtx, sizeof(pdf_hmtx) * font->hmtxcap);
	}

	font->hmtx[font->nhmtx].lo = lo;
	font->hmtx[font->nhmtx].hi = hi;
	font->hmtx[font->nhmtx].w = w;
	font->nhmtx++;
}

void
pdf_addvmtx(pdf_fontdesc *font, int lo, int hi, int x, int y, int w)
{
	if (font->nvmtx + 1 >= font->vmtxcap)
	{
		font->vmtxcap = font->vmtxcap + 16;
		font->vmtx = fz_realloc(font->vmtx, sizeof(pdf_vmtx) * font->vmtxcap);
	}

	font->vmtx[font->nvmtx].lo = lo;
	font->vmtx[font->nvmtx].hi = hi;
	font->vmtx[font->nvmtx].x = x;
	font->vmtx[font->nvmtx].y = y;
	font->vmtx[font->nvmtx].w = w;
	font->nvmtx++;
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

void
pdf_endhmtx(pdf_fontdesc *font)
{
	if (!font->hmtx)
		return;
	qsort(font->hmtx, font->nhmtx, sizeof(pdf_hmtx), cmph);
}

void
pdf_endvmtx(pdf_fontdesc *font)
{
	if (!font->vmtx)
		return;
	qsort(font->vmtx, font->nvmtx, sizeof(pdf_vmtx), cmpv);
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

