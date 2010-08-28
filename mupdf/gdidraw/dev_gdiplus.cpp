#include <windows.h>
#include <gdiplus.h>
extern "C" {
#include <fitz.h>
#include <fitz_gdidraw.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
}
#include <vector>

using namespace Gdiplus;

static ULONG_PTR m_gdiplusToken;
static LONG m_gdiplusUsage = 0;

class userData
{
	std::vector<Region *> regions;
public:
	Graphics *graphics;

	userData(HDC hDC)
	{
		assert(GetMapMode(hDC) == MM_TEXT);
		graphics = new Graphics(hDC);
		graphics->SetPageUnit(UnitPoint);
		graphics->SetSmoothingMode(SmoothingModeHighQuality);
		graphics->SetPageScale(72.0 / graphics->GetDpiY());
	}

	~userData()
	{
		delete graphics;
		assert(regions.size() == 0);
	}

	void pushClip()
	{
		Region *region = new Region();
		graphics->GetClip(region);
		regions.push_back(region);
	}

	void popClip()
	{
		assert(regions.size() > 0);
		if (regions.size() == 0)
			return;
		Region *region = regions.back();
		graphics->SetClip(region);
		delete region;
		regions.pop_back();
	}
};

class PixmapBitmap
{
	fz_pixmap *pix;
public:
	Bitmap *bmp;

	PixmapBitmap(fz_pixmap *pixmap)
	{
		pix = fz_newpixmap(fz_devicebgr, pixmap->x, pixmap->y, pixmap->w, pixmap->h);
		fz_convertpixmap(pixmap, pix);
		bmp = new Bitmap(pix->w, pix->h, pix->w * 4, PixelFormat32bppARGB, pix->samples);
	}

	~PixmapBitmap()
	{
		fz_droppixmap(pix);
		delete bmp;
	}
};


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
gdiplusgetpath(fz_path *path, int evenodd=1)
{
	GraphicsPath *gpath = new GraphicsPath(evenodd ? FillModeAlternate : FillModeWinding);
	POINT points[4] = { 0 };
	
	for (int i = 0; i < path->len; )
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
gdiplusgetpen(Brush *brush, fz_matrix ctm, fz_strokestate *stroke)
{
	float linewidth = stroke->linewidth * fz_matrixexpansion(ctm);
	if (linewidth < 0.1f)
		linewidth = 1.0 / fz_matrixexpansion(ctm);
	
	Pen *pen = new Pen(brush, linewidth / (2 * M_SQRT2));
	
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
	
	return pen;
}

static Font *
gdiplusgetfont(fz_font *font, float height)
{
	/* TODO: use embedded fonts when available */
	WCHAR fontName[LF_FACESIZE];
	
	MultiByteToWideChar(CP_UTF8, 0, font->name, -1, fontName, LF_FACESIZE);
	FontFamily family(fontName);
	FontStyle style =
		strstr(font->name, "BoldItalic") || strstr(font->name, "BoldOblique") ? FontStyleBoldItalic :
		strstr(font->name, "Italic") || strstr(font->name, "Oblique") ? FontStyleItalic :
		strstr(font->name, "Bold") ? FontStyleBold : FontStyleRegular;
	
	if (!family.IsAvailable())
		return NULL;
	return new Font(&family, height, style);
}

extern "C" static void
fz_gdiplusfillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha);

extern "C" static void
fz_gdiplusfillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Graphics *graphics = ((userData *)user)->graphics;
	gdiplusapplytransform(graphics, ctm);
	
	GraphicsPath *gpath = gdiplusgetpath(path, evenodd);
	Brush *brush = gdiplusgetbrush(colorspace, color, alpha);
	graphics->FillPath(brush, gpath);
	
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplusstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Graphics *graphics = ((userData *)user)->graphics;
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
fz_gdiplusclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm)
{
	Graphics *graphics = ((userData *)user)->graphics;
	gdiplusapplytransform(graphics, ctm);
	
	GraphicsPath *gpath = gdiplusgetpath(path, evenodd);
	((userData *)user)->pushClip();
	graphics->SetClip(gpath);
	
	delete gpath;
}

extern "C" static void
fz_gdiplusclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	/* TODO: what's the difference to fz_gdiplusclippath? */
	fz_gdiplusclippath(user, path, 0, ctm);
}

extern "C" static int move_to(const FT_Vector *to, void *user)
{
	fz_moveto((fz_path *)user, to->x, to->y); return 0;
}
extern "C" static int line_to(const FT_Vector *to, void *user)
{
	fz_lineto((fz_path *)user, to->x, to->y); return 0;
}
extern "C" static int cubic_to(const FT_Vector *ctrl1, const FT_Vector *ctrl2, const FT_Vector *to, void *user)
{
	fz_curveto((fz_path *)user, ctrl1->x, ctrl1->y, ctrl2->x, ctrl2->y, to->x, to->y); return 0;
}
extern "C" static int conic_to(const FT_Vector *ctrl, const FT_Vector *to, void *user)
{
	return cubic_to(ctrl, ctrl, to, user);
}
static FT_Outline_Funcs OutlineFuncs = {
	move_to, line_to, conic_to, cubic_to, 0, 0 /* shift, delta */
};

static void
gdiplusrendertext(Graphics *graphics, fz_text *text, fz_matrix ctm, Brush *brush, GraphicsPath *gpath=NULL)
{
	gdiplusapplytransform(graphics, ctm);
	
	Matrix trm(text->trm.a, text->trm.b, text->trm.c, text->trm.d, text->trm.e, text->trm.f);
	FT_Face face = (FT_Face)text->font->ftface;
	FT_Outline *outline = &face->glyph->outline;
	FT_Set_Transform(face, NULL, NULL);
	
	for (int i = 0; i < text->len; i++)
	{
		int fterr = FT_Load_Glyph(face, text->els[i].gid, FT_LOAD_NO_SCALE);
		if (fterr)
			continue;
		
		fz_path *path = fz_newpath();
		FT_Outline_Decompose(outline, &OutlineFuncs, path);
		GraphicsPath *gpath2 = gdiplusgetpath(path, (outline->flags & FT_OUTLINE_EVEN_ODD_FILL));
		fz_freepath(path);
		
		Matrix matrix;
		matrix.Translate(text->els[i].x, text->els[i].y);
		matrix.Scale(1.0 / face->units_per_EM, 1.0 / face->units_per_EM);
		matrix.Multiply(&trm);
		gpath2->Transform(&matrix);
		
		if (!gpath)
			graphics->FillPath(brush, gpath2);
		else
			gpath->AddPath(gpath2, FALSE);
		
		delete gpath2;
	}
}

static void
gdiplusruntext(Graphics *graphics, fz_text *text, fz_matrix ctm, Brush *brush, GraphicsPath *gpath=NULL)
{
	/* TODO: correctly turn and size font according to text->trm */
	float fontSize = fz_matrixexpansion(text->trm);
	Font *font = gdiplusgetfont(text->font, fontSize / (gpath ? 1 : M_SQRT2) * 96.0 / graphics->GetDpiY());
	
	if (!font)
	{
		if (text->font->ftface)
			gdiplusrendertext(graphics, text, ctm, brush, gpath);
		/* TODO: render Type 3 glyphs (fz_rendert3glyph) */
		return;
	}
	
	ctm = fz_concat(fz_scale(1, -1), ctm);
	gdiplusapplytransform(graphics, ctm);
	assert(text->trm.e == 0 && text->trm.f == 0);
	
	FontFamily fontFamily;
	if (gpath)
		font->GetFamily(&fontFamily);
	
	for (int i = 0; i < text->len; i++)
	{
		WCHAR out[2] = { 0 };
		out[0] = text->els[i].ucs;
		PointF origin(text->els[i].x, -text->els[i].y - fontSize);
		if (!gpath)
			graphics->DrawString(out, 1, font, origin, brush);
		else
			gpath->AddString(out, 1, &fontFamily, font->GetStyle(), font->GetSize(), origin, NULL);
	}
	
	delete font;
}

extern "C" static void
fz_gdiplusfilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Graphics *graphics = ((userData *)user)->graphics;
	
	Brush *brush = gdiplusgetbrush(colorspace, color, alpha);
	gdiplusruntext(graphics, text, ctm, brush);
	
	delete brush;
}

extern "C" static void
fz_gdiplusstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Graphics *graphics = ((userData *)user)->graphics;
	
	Brush *brush = gdiplusgetbrush(colorspace, color, alpha);
	GraphicsPath *gpath = new GraphicsPath();
	gdiplusruntext(graphics, text, ctm, brush, gpath);
	
	Pen *pen = gdiplusgetpen(brush, ctm, stroke);
	graphics->DrawPath(pen, gpath);
	
	delete gpath;
	delete pen;
	delete brush;
}

extern "C" static void
fz_gdipluscliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	Graphics *graphics = ((userData *)user)->graphics;
	gdiplusapplytransform(graphics, ctm);
	
	GraphicsPath *gpath = new GraphicsPath();
	gdiplusruntext(graphics, text, ctm, NULL, gpath);
	((userData *)user)->pushClip();
	graphics->SetClip(gpath, accumulate ? CombineModeUnion : CombineModeReplace);
	
	delete gpath;
}

extern "C" static void
fz_gdiplusclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
	/* TODO: what's the difference to fz_gdipluscliptext? */
	fz_gdipluscliptext(user, text, ctm, 0);
}

extern "C" static void
fz_gdiplusignoretext(void *user, fz_text *text, fz_matrix ctm)
{
	/* TODO: anything to do here? */
}

extern "C" static void
fz_gdiplusfillshade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	/* TODO: implement shading (or use fz_rendershade) */
	Graphics *graphics = ((userData *)user)->graphics;
}

extern "C" static void
fz_gdiplusfillimage(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	Graphics *graphics = ((userData *)user)->graphics;
	
	ctm = fz_concat(fz_scale(1.0 / image->w, 1.0 / image->h), ctm);
	gdiplusapplytransform(graphics, ctm);
	
	PixmapBitmap *bmp = new PixmapBitmap(image);
	RectF destination(0, image->h, image->w, -image->h);
	ImageAttributes imgAttrs;
	
	if (alpha != 1.0f)
	{
		ColorMatrix matrix = { 
			1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, alpha, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 1.0f
		};
		imgAttrs.SetColorMatrix(&matrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
	}
	graphics->DrawImage(bmp->bmp, destination, 0, 0, image->w, image->h, UnitPixel, &imgAttrs);
	
	delete bmp;
}

extern "C" static void
fz_gdiplusfillimagemask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	float bgr[3];
	fz_convertcolor(colorspace, color, fz_devicebgr, bgr);
	
	fz_pixmap *img2 = fz_newpixmap(fz_devicebgr, image->x, image->y, image->w, image->h);
	for (int i = 0; i < img2->w * img2->h; i++)
	{
		img2->samples[i * 4] = bgr[2] * 255;
		img2->samples[i * 4 + 1] = bgr[1] * 255;
		img2->samples[i * 4 + 2] = bgr[0] * 255;
		img2->samples[i * 4 + 3] = image->samples[i];
	}
	
	fz_gdiplusfillimage(user, img2, ctm, alpha);
	fz_droppixmap(img2);
}

extern "C" static void
fz_gdiplusclipimagemask(void *user, fz_pixmap *image, fz_matrix ctm)
{
	Graphics *graphics = ((userData *)user)->graphics;
	
	ctm = fz_concat(fz_scale(1.0 / image->w, 1.0 / image->h), ctm);
	gdiplusapplytransform(graphics, ctm);
	
	RectF destination(0, image->h, image->w, -image->h);
	((userData *)user)->pushClip();
	graphics->SetClip(destination);
}

extern "C" static void
fz_gdipluspopclip(void *user)
{
	((userData *)user)->popClip();
}

extern "C" static void
fz_gdiplusfreeuser(void *user)
{
	delete (userData *)user;
	
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
	
	fz_device *dev = fz_newdevice(new userData(hDC));
	dev->freeuser = fz_gdiplusfreeuser;
	
	dev->fillpath = fz_gdiplusfillpath;
	dev->strokepath = fz_gdiplusstrokepath;
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
	
	return dev;
}
