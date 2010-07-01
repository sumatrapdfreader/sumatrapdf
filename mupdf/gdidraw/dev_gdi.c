#include <windows.h>
#include <fitz.h>
#include <fitz_gdidraw.h>

static void
gdiapplytransform(HDC hDC, fz_matrix ctm)
{
	XFORM xform = { 1, 0, 0, 1, 0, 0 };
	xform.eM11 = ctm.a; xform.eM12 = ctm.b;
	xform.eM21 = ctm.c; xform.eM22 = ctm.d;
	xform.eDx = ctm.e; xform.eDy = ctm.f;
	SetWorldTransform(hDC, &xform);
}

static COLORREF
gdigetcolor(fz_colorspace *colorspace, float *color, float alpha)
{
	switch (colorspace->n)
	{
	case 1: return RGB(color[0] * 255, color[0] * 255, color[0] * 255);
	case 3: return RGB(color[0] * 255, color[1] * 255, color[2] * 255);
	default: return -1;
	}
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
gdigetfont(fz_font *font)
{
	/* TODO: register font with AddFontMemResourceEx */
	int height = (font->bbox.y1 - font->bbox.y0) / 100;
	HFONT ft = CreateFontA(height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font->name[6] == '+' ? font->name + 7 : font->name);
	if (!ft)
		ft = CreateFontA(height, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, NULL);
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
	
	font = gdigetfont(text->font);
	SelectObject(hDC, font);
	SetTextColor(hDC, gdigetcolor(colorspace, color, alpha));
	SetBkMode(hDC, TRANSPARENT);
	/* TODO: adjust transform so that text isn't drawn upside down */
	gdiapplytransform(hDC, ctm);
	
	for (i = 0; i < text->len; i++)
	{
		WCHAR out[2] = { 0 };
		out[0] = text->els[i].ucs;
		ExtTextOutW(hDC, text->els[i].x, text->els[i].y, 0, NULL, out, 1, NULL);
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
	
	/* TODO: what's wrong here? */
	ctm.a /= image->w; ctm.b /= image->w;
	ctm.c /= -image->h; ctm.d /= -image->h;
	ctm.f -= image->h;
	gdiapplytransform(hDC, ctm);
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

extern fz_colorspace *pdf_devicebgr;

HBITMAP
fz_pixtobitmap(HDC hDC, fz_pixmap *pixmap, BOOL paletted)
{
	int w, h, rows, rows8;
	int paletteSize = 0;
	int hasPalette = 0;
	int i, j, k;
	BITMAPINFO *bmi;
	HBITMAP hbmp = NULL;
	unsigned char *samples, *bmpData;
	fz_pixmap *bgrPixmap;
	
	w = pixmap->w;
	h = pixmap->h;
	rows = ((w * 3 + 3) / 4) * 4;
	
	bgrPixmap = fz_newpixmap(pdf_devicebgr, pixmap->x, pixmap->y, w, h);
	fz_convertpixmap(pixmap->colorspace, pixmap, pdf_devicebgr, bgrPixmap);
	pixmap = bgrPixmap;
	
	assert(pixmap->n == 4);
	
	bmi = fz_malloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	memset(bmi, 0, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	samples = pixmap->samples;
	
	if (paletted)
	{
		rows8 = ((w + 3) / 4) * 4;	
		bmpData = fz_malloc(rows8 * h);
		
		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				RGBQUAD c = { 0 };
				
				c.rgbRed = samples[j * w * 4 + i * 4 + 2];
				c.rgbGreen = samples[j * w * 4 + i * 4 + 1];
				c.rgbBlue = samples[j * w * 4 + i * 4];
				
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
				bmpData[j * rows8 + i] = k;
			}
		}
ProducingPaletteDone:
		hasPalette = paletteSize <= 256;
		if (!hasPalette)
			fz_free(bmpData);
	}
	
	if (!hasPalette)
	{
		unsigned char *dest = bmpData = fz_malloc(rows * h);
		samples = pixmap->samples;
		
		for (j = 0; j < h; j++)
		{
			for (i = 0; i < w; i++)
			{
				*dest++ = *samples++;
				*dest++ = *samples++;
				*dest++ = *samples++;
				samples++;
			}
			dest += rows - w * 3;
		}
	}
	
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = w;
	bmi->bmiHeader.biHeight = -h;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biBitCount = hasPalette ? 8 : 24;
	bmi->bmiHeader.biSizeImage = h * (hasPalette ? rows8 : rows);
	bmi->bmiHeader.biClrUsed = hasPalette ? paletteSize : 0;
	
	hbmp = CreateDIBitmap(hDC, &bmi->bmiHeader, CBM_INIT, bmpData, bmi, DIB_RGB_COLORS);
	
	fz_droppixmap(bgrPixmap);
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
	if (dest)
		StretchBlt(hDC, dest->x0, dest->y0, dest->x1 - dest->x0, dest->y1 - dest->y0,
			bmpDC, 0, 0, pixmap->w, pixmap->h, SRCCOPY);
	else
		BitBlt(hDC, 0, 0, pixmap->w, pixmap->h, bmpDC, 0, 0, SRCCOPY);
	
	DeleteDC(bmpDC);
	DeleteObject(hbmp);
}
