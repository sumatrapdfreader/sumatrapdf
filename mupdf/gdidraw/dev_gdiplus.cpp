#include <windows.h>
#include <gdiplus.h>
extern "C" {
#include <fitz.h>
#include <fitz_gdidraw.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_ADVANCES_H
}

// TODO: using the GDI+ font APIs leads to crashes (e.g. in ~PrivateFontCollection)
//       and to misrenderings (e.g. of stroked text)
// #define USE_GDI_FONT_API

using namespace Gdiplus;

static ULONG_PTR m_gdiplusToken;
static LONG m_gdiplusUsage = 0;

#define MAX_CLIP_DEPTH 32

class userData
{
	Region clips[MAX_CLIP_DEPTH];
	float clipAlpha[MAX_CLIP_DEPTH + 1];
	int clipCount;
public:
	Graphics *graphics;
	fz_glyphcache *cache;

	userData(HDC hDC)
	{
		assert(GetMapMode(hDC) == MM_TEXT);
		graphics = new Graphics(hDC);
		graphics->SetPageUnit(UnitPoint);
		graphics->SetSmoothingMode(SmoothingModeHighQuality);
		graphics->SetPageScale(72.0 / graphics->GetDpiY());
		graphics->SetCompositingMode(CompositingModeSourceOver);
		
		cache = NULL;
		clipCount = 0;
		clipAlpha[clipCount] = 1.0;
	}

	~userData()
	{
		delete graphics;
		if (cache)
			fz_freeglyphcache(cache);
		assert(clipCount == 0);
	}

	void pushClip(Region *rgn, float alpha=1.0, CombineMode combineMode=CombineModeReplace)
	{
		assert(clipCount < MAX_CLIP_DEPTH);
		if (clipCount < MAX_CLIP_DEPTH)
		{
			graphics->GetClip(&clips[clipCount++]);
			clipAlpha[clipCount] = clipAlpha[clipCount - 1] * alpha;
		}
		
		if (combineMode == CombineModeReplace)
			rgn->Intersect(&clips[clipCount - 1]);
		graphics->SetClip(rgn, combineMode);
	}

	void popClip()
	{
		assert(clipCount > 0);
		if (clipCount > 0)
			graphics->SetClip(&clips[--clipCount]);
	}

	float getAlpha(float alpha=1.0) { return clipAlpha[clipCount] * alpha; }
};

class PixmapBitmap
{
	fz_pixmap *pix;
public:
	Bitmap *bmp;

	PixmapBitmap(fz_pixmap *pixmap)
	{
		assert(!pixmap->mask || pixmap->w == pixmap->mask->w && pixmap->h == pixmap->mask->h && pixmap->mask->n == 1);
		
		pix = fz_newpixmap(fz_devicebgr, pixmap->x, pixmap->y, pixmap->w, pixmap->h);
		fz_convertpixmap(pixmap, pix);
		
		if (pixmap->mask && pixmap->mask->n == 1 && pix->w * pix->h <= pixmap->mask->w * pixmap->mask->h)
			for (int i = 0; i < pix->w * pix->h; i++)
				pix->samples[i * 4 + 3] *= pixmap->mask->samples[i] / 255.0;
		
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
gdiplusgetbrush(void *user, fz_colorspace *colorspace, float *color, float alpha)
{
	float bgr[3];
	fz_convertcolor(colorspace, color, fz_devicebgr, bgr);
	return new SolidBrush(Color(((userData *)user)->getAlpha(alpha) * 255,
		bgr[2] * 255, bgr[1] * 255, bgr[0] * 255));
}

static GraphicsPath *
gdiplusgetpath(fz_path *path, int evenodd=1)
{
	GraphicsPath *gpath = new GraphicsPath(evenodd ? FillModeAlternate : FillModeWinding);
	PointF points[3], origin = PointF(0, 0);
	
	for (int i = 0; i < path->len; )
	{
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			origin.X = path->els[i++].v; origin.Y = path->els[i++].v;
			gpath->StartFigure();
			break;
		case FZ_LINETO:
			points[0].X = path->els[i++].v; points[0].Y = path->els[i++].v;
			gpath->AddLine(origin.X, origin.Y, points[0].X, points[0].Y);
			origin = points[0];
			break;
		case FZ_CURVETO:
			points[0].X = path->els[i++].v; points[0].Y = path->els[i++].v;
			points[1].X = path->els[i++].v; points[1].Y = path->els[i++].v;
			points[2].X = path->els[i++].v; points[2].Y = path->els[i++].v;
			gpath->AddBezier(origin.X, origin.Y, points[0].X, points[0].Y, points[1].X, points[1].Y, points[2].X, points[2].Y);
			origin = points[2];
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
	Pen *pen = new Pen(brush, stroke->linewidth);
	
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
	if (stroke->dashlen > 0)
	{
		REAL dashlist[nelem(stroke->dashlist)];
		for (int i = 0; i < stroke->dashlen; i++)
			dashlist[i] = stroke->dashlist[i] ? stroke->dashlist[i] * 2 : 0.1;
		pen->SetDashPattern(dashlist, stroke->dashlen);
	}
	
	return pen;
}

#ifdef USE_GDI_FONT_API
static Font *
gdiplusgetfont(PrivateFontCollection *collection, fz_font *font, float height)
{
	WCHAR familyName[LF_FACESIZE];
	
	assert(collection->GetFamilyCount() == 0);
	if (font->_data_len != 0)
	{
		collection->AddMemoryFont(font->_data, font->_data_len);
	}
	else
	{
		WCHAR fontPath[MAX_PATH];
		MultiByteToWideChar(CP_ACP, 0, font->_data, -1, fontPath, nelem(fontPath));
		collection->AddFontFile(fontPath);
	}
	
	if (collection->GetFamilyCount() > 0)
	{
		FontFamily family;
		int found = 0;
		
		collection->GetFamilies(1, &family, &found);
		assert(found == 1);
		family.GetFamilyName(familyName);
		
		FontStyle styles[] = { FontStyleRegular, FontStyleBold, FontStyleItalic, FontStyleBoldItalic };
		for (int i = 0; i < nelem(styles); i++)
			if (family.IsStyleAvailable(styles[i]))
				return new Font(familyName, height, styles[i], UnitPixel, collection);
	}
	
	MultiByteToWideChar(CP_UTF8, 0, font->name, -1, familyName, nelem(familyName));
	FontFamily family(familyName);
	if (family.IsAvailable())
		return new Font(&family, height, FontStyleRegular);
	
	return NULL;
}
#endif

extern "C" static void
fz_gdiplusfillimage(void *user, fz_pixmap *image, fz_matrix ctm, float alpha);

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
	Brush *brush = gdiplusgetbrush(user, colorspace, color, alpha);
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
	Brush *brush = gdiplusgetbrush(user, colorspace, color, alpha);
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
	
	// TODO: evenodd clipping is too aggressive (and thus disabled)
	GraphicsPath *gpath = gdiplusgetpath(path, evenodd);
	if (evenodd)
		((userData *)user)->pushClip(&Region());
	else
		((userData *)user)->pushClip(&Region(gpath));
	
	delete gpath;
}

extern "C" static void
fz_gdiplusclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	// TODO: what's the difference to fz_gdiplusclippath?
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
	FT_Vector from, ctrl1, ctrl2;
	fz_path *path = (fz_path *)user;
	
	assert(path->len > 0);
	if (path->len == 0)
		fz_moveto(path, 0, 0);
	
	// cf. http://fontforge.sourceforge.net/bezier.html
	from.x = path->els[path->len - 2].v;
	from.y = path->els[path->len - 1].v;
	ctrl1.x = from.x + 2/3 * (ctrl->x - from.x);
	ctrl1.y = from.y + 2/3 * (ctrl->y - from.y);
	ctrl2.x = ctrl1.x + 1/3 * (to->x - from.x);
	ctrl2.y = ctrl1.y + 1/3 * (to->y - from.y);
	
	return cubic_to(&ctrl1, &ctrl2, to, user);
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
	FT_Set_Char_Size(face, 1000, 1000, 72, 72);
	FT_Set_Transform(face, NULL, NULL);
	
	for (int i = 0; i < text->len; i++)
	{
		int gid = text->els[i].gid;
		FT_Error fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_SCALE);
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
		
		if (text->font->ftsubstitute && gid < text->font->widthcount)
		{
			FT_Fixed advance = 0;
			FT_Get_Advance(face, gid, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP, &advance);
			if (advance)
				matrix.Scale((float)text->font->widthtable[gid] / advance * 1024, 1);
		}
		
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
#ifdef USE_GDI_FONT_API
	PrivateFontCollection collection;
	float fontSize = fz_matrixexpansion(text->trm);
	Font *font = gdiplusgetfont(&collection, text->font, fontSize / (gpath ? 1 : M_SQRT2 /* ??? */) * 96.0 / graphics->GetDpiY());
	
	if (font && text->len > 0 && text->els[0].ucs == '?')
	{
		// TODO: output text by glyph ID instead of character code
		delete font;
		font = NULL;
	}
	if (!font)
	{
		gdiplusrendertext(graphics, text, ctm, brush, gpath);
		return;
	}
	
	fz_matrix rotate = fz_concat(text->trm, fz_scale(-1.0 / fontSize, 1.0 / fontSize));
	assert(text->trm.e == 0 && text->trm.f == 0);
	ctm = fz_concat(fz_scale(1, -1), ctm);
	
	FontFamily fontFamily;
	if (gpath)
	{
		font->GetFamily(&fontFamily);
		gdiplusapplytransform(graphics, ctm);
	}
	
	for (int i = 0; i < text->len; i++)
	{
		WCHAR out[2] = { 0 };
		out[0] = text->els[i].ucs;
		if (!gpath)
		{
			fz_matrix ctm2 = fz_concat(fz_translate(text->els[i].x, -text->els[i].y), ctm);
			ctm2 = fz_concat(fz_scale(-1, 1), fz_concat(rotate, ctm2));
			ctm2 = fz_concat(fz_translate(0, -fontSize), ctm2);
			
			gdiplusapplytransform(graphics, ctm2);
			graphics->DrawString(out, 1, font, PointF(0, 0), brush);
		}
		else
		{
			// TODO: correctly rotate glyphs according to text->trm
			PointF origin(text->els[i].x, -text->els[i].y - fontSize);
			gpath->AddString(out, 1, &fontFamily, font->GetStyle(), font->GetSize(), origin, NULL);
		}
	}
	
	delete font;
#else
	gdiplusrendertext(graphics, text, ctm, brush, gpath);
#endif
}

static void
gdiplusrunt3text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	// TODO: the first scan line doesn't seem to be drawn for most/all Type 3 glyphs
	userData *data = (userData *)user;
	if (!data->cache)
		data->cache = fz_newglyphcache();
	
	float res = fz_matrixexpansion(ctm) * 3;
	fz_matrix size = fz_concat(fz_scale(res, res), text->trm);
	float fontSize = fz_matrixexpansion(size);
	ctm = fz_concat(fz_scale(1, -1), ctm);
	
	for (int i = 0; i < text->len; i++)
	{
		fz_pixmap *glyph = fz_renderglyph(data->cache, text->font, text->els[i].gid, size);
		
		fz_matrix ctm2 = fz_concat(fz_translate(text->els[i].x, -text->els[i].y), ctm);
		ctm2 = fz_concat(fz_translate(-glyph->x / res, -(glyph->y + glyph->h) / res), ctm2);
		ctm2 = fz_concat(text->trm, ctm2);
		ctm2 = fz_concat(fz_scale(glyph->w / fontSize, glyph->h / fontSize), ctm2);
		
		fz_gdiplusfillimagemask(user, glyph, ctm2, colorspace, color, alpha);
		fz_droppixmap(glyph);
	}
}

extern "C" static void
fz_gdiplusfilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Graphics *graphics = ((userData *)user)->graphics;
	
	Brush *brush = gdiplusgetbrush(user, colorspace, color, alpha);
	if (text->font->ftface)
		gdiplusruntext(graphics, text, ctm, brush);
	else
		gdiplusrunt3text(user, text, ctm, colorspace, color, alpha);
	
	delete brush;
}

extern "C" static void
fz_gdiplusstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Graphics *graphics = ((userData *)user)->graphics;
	
	Brush *brush = gdiplusgetbrush(user, colorspace, color, alpha);
	GraphicsPath *gpath = new GraphicsPath();
	if (text->font->ftface)
		gdiplusruntext(graphics, text, ctm, brush, gpath);
	else
		gdiplusrunt3text(user, text, ctm, colorspace, color, alpha/*, gpath */);
	
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
	float black[3] = { 0 };
	if (text->font->ftface)
		gdiplusruntext(graphics, text, ctm, NULL, gpath);
	else
		gdiplusrunt3text(user, text, ctm, fz_devicebgr, black, 1.0/*, gpath */);
	((userData *)user)->pushClip(&Region(gpath), 1.0, accumulate ? CombineModeUnion : CombineModeReplace);
	
	delete gpath;
}

extern "C" static void
fz_gdiplusclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
	// TODO: what's the difference to fz_gdipluscliptext?
	fz_gdipluscliptext(user, text, ctm, 0);
}

extern "C" static void
fz_gdiplusignoretext(void *user, fz_text *text, fz_matrix ctm)
{
	// TODO: anything to do here?
}

extern "C" static void
fz_gdiplusfillshade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	Graphics *graphics = ((userData *)user)->graphics;
	
	Rect clipRect;
	graphics->GetClipBounds(&clipRect);
	fz_rect clip = { clipRect.X, clipRect.Y, clipRect.X + clipRect.Width, clipRect.Y + clipRect.Height };
	clip = fz_transformrect(ctm, clip);
	
	fz_rect bounds = fz_boundshade(shade, ctm);
	fz_bbox bbox = fz_intersectbbox(fz_roundrect(bounds), fz_roundrect(clip));
	
	if (!fz_isemptyrect(shade->bbox))
	{
		bounds = fz_transformrect(fz_concat(shade->matrix, ctm), shade->bbox);
		bbox = fz_intersectbbox(fz_roundrect(bounds), bbox);
	}
	
	if (fz_isemptyrect(bbox))
		return;
	
	fz_pixmap *dest = fz_newpixmapwithrect(fz_devicergb, bbox);
	fz_clearpixmap(dest, 0);
	
	if (shade->usebackground)
	{
		float colorfv[4];
		fz_convertcolor(shade->cs, shade->background, fz_devicergb, colorfv);
		colorfv[3] = 1.0;
		
		for (int y = bbox.y0; y < bbox.y1; y++)
		{
			unsigned char *s = dest->samples + ((bbox.x0 - dest->x) + (y - dest->y) * dest->w) * 4;
			for (int x = bbox.x0; x < bbox.x1; x++)
				for (int i = 0; i < 4; i++)
					*s++ = colorfv[i] * 255;
		}
	}
	
	fz_rendershade(shade, ctm, dest, bbox);
	
	ctm = fz_concat(fz_scale(dest->w, -dest->h), fz_translate(dest->x, dest->y + dest->h));
	fz_gdiplusfillimage(user, dest, ctm, alpha);
	
	fz_droppixmap(dest);
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
	
	alpha = ((userData *)user)->getAlpha(alpha);
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
	((userData *)user)->pushClip(&Region(destination));
}

extern "C" static void
fz_gdipluspopclip(void *user)
{
	((userData *)user)->popClip();
}

extern "C" static void
fz_gdiplusbeginmask(void *user, fz_rect rect, int luminosity,
	fz_colorspace *colorspace, float *colorfv)
{
	// TODO: implement masking
	RectF clip(rect.x0, rect.y0, rect.x1 - rect.x0, rect.y1 - rect.y0);
	((userData *)user)->pushClip(&Region(clip));
}

extern "C" static void
fz_gdiplusendmask(void *user)
{
	// fz_gdipluspopclip(user);
}

extern "C" static void
fz_gdiplusbegingroup(void *user, fz_rect rect, int isolated, int knockout,
	fz_blendmode blendmode, float alpha)
{
	// TODO: implement blending
	RectF clip(rect.x0, rect.y0, rect.x1 - rect.x0, rect.y1 - rect.y0);
	((userData *)user)->pushClip(&Region(clip), alpha);
}

extern "C" static void
fz_gdiplusendgroup(void *user)
{
	fz_gdipluspopclip(user);
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
	
	dev->beginmask = fz_gdiplusbeginmask;
	dev->endmask = fz_gdiplusendmask;
	dev->begingroup = fz_gdiplusbegingroup;
	dev->endgroup = fz_gdiplusendgroup;
	
	return dev;
}
