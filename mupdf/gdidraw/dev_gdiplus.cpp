#include <windows.h>
#include <gdiplus.h>
extern "C" {
#include <fitz.h>
#include <fitz_gdidraw.h>
}

using namespace Gdiplus;

static ULONG_PTR m_gdiplusToken;
static LONG m_gdiplusUsage = 0;


static void
gdiplusapplytransform(Graphics *graphics, fz_matrix ctm)
{
	Matrix matrix(ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f);
	graphics->SetTransform(&matrix);
}

static Brush *
gdiplusgetbrush(fz_colorspace *colorspace, float *color, float alpha)
{
	float bgr[3];
	fz_convertcolor(colorspace, color, fz_devicebgr, bgr);
	return new SolidBrush(Color(alpha * 255, bgr[2] * 255, bgr[1] * 255, bgr[0] * 255));
}

static GraphicsPath *
gdiplusgetpath(fz_path *path, FillMode fillMode=FillModeAlternate)
{
	GraphicsPath *gpath = new GraphicsPath(fillMode);
	POINT points[4] = { 0 };
	int i = 0;
	
	while (i < path->len)
	{
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			points[0].x = path->els[i++].v; points[0].y = path->els[i++].v;
			gpath->StartFigure();
			break;
		case FZ_LINETO:
			points[1].x = path->els[i++].v; points[1].y = path->els[i++].v;
			gpath->AddLine(points[0].x, points[0].y, points[1].x, points[1].y);
			points[0] = points[1];
			break;
		case FZ_CURVETO:
			points[1].x = path->els[i++].v; points[1].y = path->els[i++].v;
			points[2].x = path->els[i++].v; points[2].y = path->els[i++].v;
			points[3].x = path->els[i++].v; points[3].y = path->els[i++].v;
			gpath->AddBezier(points[0].x, points[0].y, points[1].x, points[1].y, points[2].x, points[2].y, points[3].x, points[3].y);
			points[0] = points[3];
			break;
		case FZ_CLOSEPATH:
			gpath->CloseFigure();
			break;
		}
	}
	
	return gpath;
}

static Pen *
gdiplusgetpen(Brush *brush, fz_matrix ctm, fz_strokestate *stroke=NULL)
{
	float linewidth = 0;
	
	if (stroke)
	{
		linewidth = stroke->linewidth * fz_matrixexpansion(ctm);
		if (linewidth < 0.1f)
			linewidth = 1.0 / fz_matrixexpansion(ctm);
	}
	
	Pen *pen = new Pen(brush, linewidth);
	
	if (stroke)
	{
		pen->SetMiterLimit(stroke->miterlimit);
		
		switch (stroke->linecap)
		{
		case 0: pen->SetStartCap(LineCapFlat); pen->SetEndCap(LineCapFlat); break;
		case 1: pen->SetStartCap(LineCapRound); pen->SetEndCap(LineCapRound); break;
		case 2: pen->SetStartCap(LineCapSquare); pen->SetEndCap(LineCapSquare); break;
		}
		switch (stroke->linejoin)
		{
		case 0: pen->SetLineJoin(LineJoinMiter); break;
		case 1: pen->SetLineJoin(LineJoinRound); break;
		case 2: pen->SetLineJoin(LineJoinBevel); break;
		}
	}
	
	return pen;
}

static Bitmap *
fz_pixtobitmap2(fz_pixmap *pixmap)
{
	/* abgr is a GDI+ compatible format */
	fz_pixmap *bgrPixmap = fz_newpixmap(fz_devicebgr, pixmap->x, pixmap->y, pixmap->w, pixmap->h);
	fz_convertpixmap(pixmap, bgrPixmap);
	assert(bgrPixmap->n == 4);
	
	BITMAPINFO bmi = { 0 };
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = pixmap->w;
	bmi.bmiHeader.biHeight = pixmap->h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biSizeImage = pixmap->w * pixmap->h * 4;
	bmi.bmiHeader.biClrUsed = 0;
	
	Bitmap *bmp = new Bitmap(&bmi, bgrPixmap->samples);
	
	fz_droppixmap(bgrPixmap);
	
	return bmp;
}

extern "C" static void
fz_gdiplusfillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Graphics *graphics = (Graphics *)user;
	gdiplusapplytransform(graphics, ctm);
	
	GraphicsPath *gpath = gdiplusgetpath(path, evenodd ? FillModeAlternate : FillModeWinding);
	Brush *brush = gdiplusgetbrush(colorspace, color, alpha);
	graphics->FillPath(brush, gpath);
	
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplusstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Graphics *graphics = (Graphics *)user;
	gdiplusapplytransform(graphics, ctm);
	
	GraphicsPath *gpath = gdiplusgetpath(path);
	Brush *brush = gdiplusgetbrush(colorspace, color, alpha);
	Pen *pen = gdiplusgetpen(brush, ctm, stroke);
	graphics->DrawPath(pen, gpath);
	
	delete pen;
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplusfillimage(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	Graphics *graphics = (Graphics *)user;
	
	fz_matrix ctm2 = fz_concat(ctm, fz_scale(1.0 / image->w, -1.0 / image->h));
	ctm2.e = ctm.e; ctm2.f = ctm.f - image->h;
	gdiplusapplytransform(graphics, ctm2);
	
	/* TODO: why doesn't this work yet? */
	Bitmap *bmp = fz_pixtobitmap2(image);
	CachedBitmap *cbmp = new CachedBitmap(bmp, graphics);
	graphics->DrawCachedBitmap(cbmp, 0, 0);
	
	delete cbmp;
	delete bmp;
}

extern "C" static void
fz_gdiplusfreeuser(void *user)
{
	Graphics *graphics = (Graphics *)user;
	delete graphics;
	
	if (InterlockedDecrement(&m_gdiplusUsage) == 0)
		GdiplusShutdown(m_gdiplusToken);
}

fz_device *
fz_newgdiplusdevice(HDC hDC)
{
	if (InterlockedIncrement(&m_gdiplusUsage) == 1)
	{
		GdiplusStartupInput gdiplusStartupInput;
		GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
	}
	
	Graphics *graphics = new Graphics(hDC);
	graphics->SetSmoothingMode(SmoothingModeHighQuality);
	
	fz_device *dev = fz_newdevice(graphics);
	dev->freeuser = fz_gdiplusfreeuser;
	
	dev->fillpath = fz_gdiplusfillpath;
	dev->strokepath = fz_gdiplusstrokepath;
/*
	dev->clippath = fz_gdiplusclippath;
	dev->clipstrokepath = fz_gdiplusclipstrokepath;

	dev->filltext = fz_gdiplusfilltext;
	dev->stroketext = fz_gdiplusstroketext;
	dev->cliptext = fz_gdipluscliptext;
	dev->clipstroketext = fz_gdiplusclipstroketext;
	dev->ignoretext = fz_gdiplusignoretext;

	dev->fillshade = fz_gdiplusfillshade;
	dev->fillimage = fz_gdiplusfillimage;
	dev->fillimagemask = fz_gdiplusfillimagemask;
	dev->clipimagemask = fz_gdiplusclipimagemask;

	dev->popclip = fz_gdipluspopclip;
*/
	
	return dev;
}
