#include <windows.h>
#include <fitz.h>
#include <fitz_gdidraw.h>

extern fz_colorspace *pdf_devicebgr;

typedef struct {
	HDC hDC;
	
	int *clipIDs;
	int clipIx;
	int clipCap;
} fz_gdidevice;


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
gdipushclip(fz_gdidevice *dev)
{
	if (dev->clipIx >= dev->clipCap)
	{
		dev->clipCap *= 2;
		dev->clipIDs = fz_realloc(dev->clipIDs, dev->clipCap * sizeof(int));
	}
	
	dev->clipIDs[dev->clipIx++] = SaveDC(dev->hDC);
}

static void
gdipopclip(fz_gdidevice *dev)
{
	assert(dev->clipIx > 0);
	RestoreDC(dev->hDC, dev->clipIDs[--dev->clipIx]);
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
	HDC hDC = ((fz_gdidevice *)user)->hDC;
	HBRUSH brush;
	
	gdipushclip(user);
	brush = CreateSolidBrush(gdigetcolor(colorspace, color, alpha));
	SelectObject(hDC, brush);
	gdiapplytransform(hDC, ctm);
	
	SetPolyFillMode(hDC, evenodd ? ALTERNATE : WINDING);
	gdirunpath(hDC, path);
	FillPath(hDC);
	
	DeleteObject(brush);
	gdipopclip(user);
}

static HPEN
gdicreatepen(fz_strokestate *stroke, COLORREF color, float expansion)
{
	float linewidth = stroke->linewidth * expansion;
	DWORD penStyle = PS_GEOMETRIC | PS_SOLID;
	LOGBRUSH brush;
	
	if (linewidth < 0.1f)
		linewidth = 1.0 / expansion;
		
	switch (stroke->linecap)
	{
	case 0: penStyle |= PS_ENDCAP_FLAT; break;
	case 1: penStyle |= PS_ENDCAP_ROUND; break;
	case 2: penStyle |= PS_ENDCAP_SQUARE; break;
	}
	switch (stroke->linejoin)
	{
	case 0: penStyle |= PS_JOIN_MITER; break;
	case 1: penStyle |= PS_JOIN_ROUND; break;
	case 2: penStyle |= PS_JOIN_BEVEL; break;
	}
	
	brush.lbStyle = BS_SOLID;
	brush.lbColor = color;
	brush.lbHatch = 0;
	
	return ExtCreatePen(penStyle, linewidth, &brush, 0, NULL);
}

static void
fz_gdistrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	HDC hDC = ((fz_gdidevice *)user)->hDC;
	HPEN pen;
	
	gdipushclip(user);
	
	SetMiterLimit(hDC, stroke->miterlimit, NULL);
	pen = gdicreatepen(stroke, gdigetcolor(colorspace, color, alpha), fz_matrixexpansion(ctm));
	SelectObject(hDC, pen);
	gdiapplytransform(hDC, ctm);
	
	gdirunpath(hDC, path);
	StrokePath(hDC);
	
	DeleteObject(pen);
	gdipopclip(user);
}

static void
fz_gdiclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm)
{
	HDC hDC = ((fz_gdidevice *)user)->hDC;
	
	gdipushclip(user);
	
	gdiapplytransform(hDC, ctm);
	gdirunpath(hDC, path);
	/* TODO: that's not what evenodd means */
	SelectClipPath(hDC, evenodd ? RGN_DIFF : RGN_AND);
}

static void
fz_gdiclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	/* TODO: what's the difference to fz_gdiclippath? */
	fz_gdiclippath(user, path, 0, ctm);
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
gdiruntext(HDC hDC, fz_text *text, fz_matrix ctm, COLORREF color)
{
	float fontSize, angle;
	HFONT font;
	int i;
	
	assert(text->trm.e == 0 && text->trm.f == 0);
	/* TODO: correctly turn and size font according to text->trm */
	fontSize = fz_matrixexpansion(text->trm);
	angle = atanf(text->trm.a / text->trm.b) / M_PI * 180.0 - 90;
	font = gdigetfont(text->font, fontSize);
	SelectObject(hDC, font);
	SetTextColor(hDC, color);
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
}

static void
fz_gdifilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	HDC hDC = ((fz_gdidevice *)user)->hDC;
	
	gdipushclip(user);
	gdiruntext(hDC, text, ctm, gdigetcolor(colorspace, color, alpha));
	gdipopclip(user);
}

static void
fz_gdistroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	HDC hDC = ((fz_gdidevice *)user)->hDC;
	HPEN pen;
	
	gdipushclip(user);
	
	SetMiterLimit(hDC, stroke->miterlimit, NULL);
	pen = gdicreatepen(stroke, gdigetcolor(colorspace, color, alpha), fz_matrixexpansion(ctm));
	SelectObject(hDC, pen);
	
	BeginPath(hDC);
	gdiruntext(hDC, text, ctm, gdigetcolor(colorspace, color, alpha));
	EndPath(hDC);
	StrokePath(hDC);
	
	DeleteObject(pen);
	gdipopclip(user);
}

static void
fz_gdicliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	HDC hDC = ((fz_gdidevice *)user)->hDC;
	
	gdipushclip(user);
	
	BeginPath(hDC);
	gdiruntext(hDC, text, ctm, TRANSPARENT);
	EndPath(hDC);
	SelectClipPath(hDC, RGN_AND);
}

static void
fz_gdiclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
	/* TODO: what's the difference to fz_gdicliptext? */
	fz_gdicliptext(user, text, ctm, 0);
}

static void
fz_gdiignoretext(void *user, fz_text *text, fz_matrix ctm)
{
	/* TODO: print transparent text? */
}

static void
fz_gdipopclip(void *user)
{
	gdipopclip(user);
}

static void
fz_gdifillshade(void *user, fz_shade *shade, fz_matrix ctm)
{
}

static void
fz_gdifillimage(void *user, fz_pixmap *image, fz_matrix ctm)
{
	HDC hDC = ((fz_gdidevice *)user)->hDC;
	fz_matrix ctm2;
	
	gdipushclip(user);
	ctm2 = fz_concat(ctm, fz_scale(1.0 / image->w, -1.0 / image->h));
	ctm2.e = ctm.e; ctm2.f = ctm.f - image->h;
	gdiapplytransform(hDC, ctm2);
	
	fz_pixmaptodc(hDC, image, nil);
	
	gdipopclip(user);
}

static void
fz_gdifillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
}

static void
fz_gdiclipimagemask(void *user, fz_pixmap *image, fz_matrix ctm)
{
	gdipushclip(user);
}

static void
fz_gdifreeuser(void *user)
{
	fz_gdidevice *dev = user;
	fz_free(dev->clipIDs);
	fz_free(dev);
}

fz_device *
fz_newgdidevice(HDC hDC)
{
	fz_gdidevice *gdev = fz_malloc(sizeof(fz_gdidevice));
	fz_device *dev = fz_newdevice(gdev);
	dev->freeuser = fz_gdifreeuser;
	
	gdev->hDC = hDC;
	gdev->clipIx = 0;
	gdev->clipCap = 16;
	gdev->clipIDs = fz_malloc(gdev->clipCap * sizeof(int));
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
	fz_convertpixmap(pixmap, bgrPixmap);
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
