#include <windows.h>
#include <fitz.h>
#include <fitz_gdidraw.h>

extern fz_colorspace *pdf_devicebgr;

static void
gdiapplytransform(HDC hDC, fz_matrix ctm)
{
	XFORM xform;
	xform.eM11 = ctm.a; xform.eM12 = ctm.b;
	xform.eM21 = ctm.c; xform.eM22 = ctm.d;
	xform.eDx = ctm.e; xform.eDy = ctm.f;
	SetWorldTransform(hDC, &xform);
}

static COLORREF
gdigetcolor(fz_colorspace *colorspace, float *color, float alpha)
{
	float bgr[3];
	fz_convertcolor(colorspace, color, pdf_devicebgr, bgr);
	return RGB(bgr[2] * 255, bgr[1] * 255, bgr[0] * 255);
}

static void
gdirunpath(HDC hDC, fz_path *path)
{
	int i = 0;
	POINT points[3];
	
	BeginPath(hDC);
	while (i < path->len)
	{
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			points[0].x = path->els[i++].v; points[0].y = path->els[i++].v;
			MoveToEx(hDC, points[0].x, points[0].y, NULL);
			break;
		case FZ_LINETO:
			points[0].x = path->els[i++].v; points[0].y = path->els[i++].v;
			LineTo(hDC, points[0].x, points[0].y);
			break;
		case FZ_CURVETO:
			points[0].x = path->els[i++].v; points[0].y = path->els[i++].v;
			points[1].x = path->els[i++].v; points[1].y = path->els[i++].v;
			points[2].x = path->els[i++].v; points[2].y = path->els[i++].v;
			PolyBezierTo(hDC, (const LPPOINT)&points, 3);
			break;
		case FZ_CLOSEPATH:
			CloseFigure(hDC);
			break;
		}
	}
	EndPath(hDC);
}

static void
fz_gdifillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	HDC hDC = user;
	int origDC = SaveDC(hDC);
	HBRUSH brush;
	
	brush = CreateSolidBrush(gdigetcolor(colorspace, color, alpha));
	SelectObject(hDC, brush);
	gdiapplytransform(hDC, ctm);
	
	SetPolyFillMode(hDC, evenodd ? ALTERNATE : WINDING);
	gdirunpath(hDC, path);
	FillPath(hDC);
	
	DeleteObject(brush);
	RestoreDC(hDC, origDC);
}

static void
fz_gdistrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	HDC hDC = user;
	int origDC = SaveDC(hDC);
	HPEN pen;
	
	pen = CreatePen(PS_SOLID, stroke->linewidth, gdigetcolor(colorspace, color, alpha));
	SelectObject(hDC, pen);
	SetMiterLimit(hDC, stroke->miterlimit, NULL);
	gdiapplytransform(hDC, ctm);
	
	gdirunpath(hDC, path);
	StrokePath(hDC);
	
	DeleteObject(pen);
	RestoreDC(hDC, origDC);
}

static void
fz_gdiclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm)
{
}

static void
fz_gdiclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
}

static HFONT
gdigetfont(fz_font *font, float height)
{
	/* TODO: register font with AddFontMemResourceEx */
	int weight = strstr(font->name, "Bold") ? FW_BOLD : FW_DONTCARE;
	BOOL italic = strstr(font->name, "Italic") != NULL;
	
	HFONT ft = CreateFontA(height, 0, 0, 0, weight, italic, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font->name[6] == '+' ? font->name + 7 : font->name);
	if (!ft)
		ft = CreateFontA(height, 0, 0, 0, weight, italic, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
	return ft;
}

static void
fz_gdifilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	HDC hDC = user;
	int origDC = SaveDC(hDC);
	HFONT font;
	int i;
	float fontSize;
	
	fontSize = fz_matrixexpansion(text->trm);
	font = gdigetfont(text->font, fontSize);
	SelectObject(hDC, font);
	SetTextColor(hDC, gdigetcolor(colorspace, color, alpha));
	SetBkMode(hDC, TRANSPARENT);
	
	ctm = fz_concat(ctm, fz_concat(fz_scale(1, -1), fz_translate(0, 2 * ctm.f)));
	gdiapplytransform(hDC, ctm);
	
	for (i = 0; i < text->len; i++)
	{
		WCHAR out[2] = { 0 };
		out[0] = text->els[i].ucs;
		ExtTextOutW(hDC, text->els[i].x, -text->els[i].y - fontSize, 0, NULL, out, 1, NULL);
	}
	
	DeleteObject(font);
	RestoreDC(hDC, origDC);
}

static void
fz_gdistroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
}

static void
fz_gdicliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
}

static void
fz_gdiclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
}

static void
fz_gdiignoretext(void *user, fz_text *text, fz_matrix ctm)
{
}

static void
fz_gdipopclip(void *user)
{
}

static void
fz_gdifillshade(void *user, fz_shade *shade, fz_matrix ctm)
{
}

static void
fz_gdifillimage(void *user, fz_pixmap *image, fz_matrix ctm)
{
	HDC hDC = user;
	int origDC = SaveDC(hDC);
	fz_matrix ctm2;
	
	ctm2 = fz_concat(ctm, fz_scale(1.0 / image->w, -1.0 / image->h));
	ctm2.e = ctm.e; ctm2.f = ctm.f - image->h;
	gdiapplytransform(hDC, ctm2);
	
	fz_pixmaptodc(hDC, image, nil);
	
	RestoreDC(hDC, origDC);
}

static void
fz_gdifillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
}

static void
fz_gdiclipimagemask(void *user, fz_pixmap *image, fz_matrix ctm)
{
}

fz_device *
fz_newgdidevice(HDC hDC)
{
	fz_device *dev = fz_newdevice(hDC);
	SetGraphicsMode(hDC, GM_ADVANCED);

	dev->fillpath = fz_gdifillpath;
	dev->strokepath = fz_gdistrokepath;
	dev->clippath = fz_gdiclippath;
	dev->clipstrokepath = fz_gdiclipstrokepath;

	dev->filltext = fz_gdifilltext;
	dev->stroketext = fz_gdistroketext;
	dev->cliptext = fz_gdicliptext;
	dev->clipstroketext = fz_gdiclipstroketext;
	dev->ignoretext = fz_gdiignoretext;

	dev->fillshade = fz_gdifillshade;
	dev->fillimage = fz_gdifillimage;
	dev->fillimagemask = fz_gdifillimagemask;
	dev->clipimagemask = fz_gdiclipimagemask;

	dev->popclip = fz_gdipopclip;

	return dev;
}

HBITMAP
fz_pixtobitmap(HDC hDC, fz_pixmap *pixmap, BOOL paletted)
{
	int w, h, rows8;
	int paletteSize = 0;
	BOOL hasPalette = FALSE;
	int i, j, k;
	BITMAPINFO *bmi;
	HBITMAP hbmp = NULL;
	unsigned char *bmpData, *source, *dest;
	fz_pixmap *bgrPixmap;
	
	w = pixmap->w;
	h = pixmap->h;
	
	/* abgr is a GDI compatible format */
	bgrPixmap = fz_newpixmap(pdf_devicebgr, pixmap->x, pixmap->y, w, h);
	fz_convertpixmap(pixmap->colorspace, pixmap, pdf_devicebgr, bgrPixmap);
	pixmap = bgrPixmap;
	
	assert(pixmap->n == 4);
	
	bmi = fz_malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	memset(bmi, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	
	if (paletted)
	{
		rows8 = ((w + 3) / 4) * 4;	
		dest = bmpData = fz_malloc(rows8 * h);
		source = pixmap->samples;
		
		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				RGBQUAD c = { 0 };
				
				c.rgbBlue = *source++;
				c.rgbGreen = *source++;
				c.rgbRed = *source++;
				source++;
				
				/* find this color in the palette */
				for (k = 0; k < paletteSize; k++)
					if (*(int *)&bmi->bmiColors[k] == *(int *)&c)
						break;
				/* add it to the palette if it isn't in there and if there's still space left */
				if (k == paletteSize)
				{
					if (k >= 256)
						goto ProducingPaletteDone;
					*(int *)&bmi->bmiColors[paletteSize] = *(int *)&c;
					paletteSize++;
				}
				/* 8-bit data consists of indices into the color palette */
				*dest++ = k;
			}
			dest += rows8 - w;
		}
ProducingPaletteDone:
		hasPalette = paletteSize <= 256;
		if (!hasPalette)
			fz_free(bmpData);
	}
	if (!hasPalette)
		bmpData = pixmap->samples;
	
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = w;
	bmi->bmiHeader.biHeight = -h;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biBitCount = hasPalette ? 8 : 32;
	bmi->bmiHeader.biSizeImage = h * (hasPalette ? rows8 : w * 4);
	bmi->bmiHeader.biClrUsed = hasPalette ? paletteSize : 0;
	
	hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT, bmpData, bmi, DIB_RGB_COLORS);
	
	fz_droppixmap(bgrPixmap);
	if (hasPalette)
		fz_free(bmpData);
	fz_free(bmi);
	
	return hbmp;
}

void
fz_pixmaptodc(HDC hDC, fz_pixmap *pixmap, fz_rect *dest)
{
	// Try to extract a 256 color palette so that we can use 8-bit images instead of 24-bit ones.
	// This should e.g. speed up printing for most monochrome documents by saving spool resources.
	HBITMAP hbmp = fz_pixtobitmap(hDC, pixmap, TRUE);
	HDC bmpDC = CreateCompatibleDC(hDC);
	
	SelectObject(bmpDC, hbmp);
	if (!dest)
		BitBlt(hDC, 0, 0, pixmap->w, pixmap->h, bmpDC, 0, 0, SRCCOPY);
	else if (dest->x1 - dest->x0 == pixmap->w && dest->y1 - dest->y0 == pixmap->h)
		BitBlt(hDC, dest->x0, dest->y0, pixmap->w, pixmap->h, bmpDC, 0, 0, SRCCOPY);
	else
		StretchBlt(hDC, dest->x0, dest->y0, dest->x1 - dest->x0, dest->y1 - dest->y0,
			bmpDC, 0, 0, pixmap->w, pixmap->h, SRCCOPY);
	
	DeleteDC(bmpDC);
	DeleteObject(hbmp);
}
