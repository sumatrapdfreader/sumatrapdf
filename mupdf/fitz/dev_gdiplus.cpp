#include <windows.h>
#include <gdiplus.h>
extern "C" {
#include <fitz.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_ADVANCES_H
}

using namespace Gdiplus;

static ULONG_PTR m_gdiplusToken;
static LONG m_gdiplusUsage = 0;

class userDataStackItem
{
public:
	Region clip;
	float alpha;
	userDataStackItem *next, *prev;

	userDataStackItem(float _alpha=1.0, userDataStackItem *_prev=NULL) :
		alpha(_alpha), next(NULL), prev(_prev) { }
};

class userData
{
	userDataStackItem *stack;
public:
	Graphics *graphics;
	fz_device *dev;

	userData(HDC hDC) : stack(new userDataStackItem()), dev(NULL)
	{
		assert(GetMapMode(hDC) == MM_TEXT);
		graphics = new Graphics(hDC);
		graphics->SetPageUnit(UnitPoint);
		graphics->SetSmoothingMode(SmoothingModeHighQuality);
		graphics->SetPageScale(72.0 / graphics->GetDpiY());
		graphics->SetCompositingMode(CompositingModeSourceOver);
		graphics->SetInterpolationMode(InterpolationModeHighQualityBicubic);
	}

	~userData()
	{
		delete graphics;
		assert(stack && !stack->next && !stack->prev);
		delete stack;
	}

	void pushClip(Region *clipRegion=NULL, float alpha=1.0, bool accumulate=false)
	{
		if (accumulate && clipRegion)
		{
			graphics->SetClip(clipRegion, CombineModeUnion);
			return;
		}
		
		userDataStackItem *next = new userDataStackItem(stack->alpha * alpha, stack);
		stack = stack->next = next;
		graphics->GetClip(&stack->clip);
		
		if (clipRegion)
			graphics->SetClip(clipRegion, CombineModeIntersect);
	}

	void pushClip(GraphicsPath *gpath, float alpha=1.0, bool accumulate=false)
	{
		pushClip(&Region(gpath), alpha, accumulate);
	}

	void popClip()
	{
		assert(stack->prev);
		if (!stack->prev)
			return;
		
		graphics->SetClip(&stack->clip);
		stack = stack->prev;
		delete stack->next;
		stack->next = NULL;
	}

	float getAlpha(float alpha=1.0) { return stack->alpha * alpha; }
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
	graphics->SetTransform(&Matrix(ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f));
}

static void
gdiplusapplytransform(GraphicsPath *gpath, fz_matrix ctm)
{
	gpath->Transform(&Matrix(ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f));
}

static void
gdiplusapplytransform(Region *rgn, fz_matrix ctm)
{
	rgn->Transform(&Matrix(ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f));
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
gdiplusgetpath(fz_path *path, fz_matrix ctm, int evenodd=1)
{
	PointF *points = new PointF[path->len / 2];
	BYTE *types = new BYTE[path->len / 2];
	int len = 0;
	
	for (int i = 0; i < path->len; )
	{
		switch (path->els[i++].k)
		{
		case FZ_MOVETO:
			points[len].X = path->els[i++].v; points[len].Y = path->els[i++].v;
			if (i < path->len && path->els[i].k != FZ_MOVETO)
				types[len++] = PathPointTypeStart;
			break;
		case FZ_LINETO:
			points[len].X = path->els[i++].v; points[len].Y = path->els[i++].v;
			types[len++] = PathPointTypeLine;
			break;
		case FZ_CURVETO:
			points[len].X = path->els[i++].v; points[len].Y = path->els[i++].v;
			types[len++] = PathPointTypeBezier;
			points[len].X = path->els[i++].v; points[len].Y = path->els[i++].v;
			types[len++] = PathPointTypeBezier;
			points[len].X = path->els[i++].v; points[len].Y = path->els[i++].v;
			types[len++] = PathPointTypeBezier;
			break;
		case FZ_CLOSEPATH:
			types[len - 1] = types[len - 1] | PathPointTypeCloseSubpath;
			break;
		}
	}
	assert(len <= path->len / 2);
	
	GraphicsPath *gpath = new GraphicsPath(points, types, len, evenodd ? FillModeAlternate : FillModeWinding);
	gdiplusapplytransform(gpath, ctm);
	
	delete points;
	delete types;
	
	return gpath;
}

static Pen *
gdiplusgetpen(Brush *brush, fz_matrix ctm, fz_strokestate *stroke)
{
	Pen *pen = new Pen(brush, stroke->linewidth * fz_matrixexpansion(ctm));
	
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
			dashlist[i] = stroke->dashlist[i] ? stroke->dashlist[i] : 0.1;
		pen->SetDashPattern(dashlist, stroke->dashlen);
	}
	
	return pen;
}

static Font *
gdiplusgetfont(PrivateFontCollection *collection, fz_font *font, float height, float *out_ascent)
{
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
	
	if (collection->GetFamilyCount() == 0)
		return NULL;
	
	FontFamily family;
	int found = 0;
	collection->GetFamilies(1, &family, &found);
	assert(found == 1);
	
	WCHAR familyName[LF_FACESIZE];
	family.GetFamilyName(familyName);
	
	FontStyle styles[] = { FontStyleRegular, FontStyleBold, FontStyleItalic, FontStyleBoldItalic };
	for (int i = 0; i < nelem(styles); i++)
	{
		if (family.IsStyleAvailable(styles[i]))
		{
			*out_ascent = 1.0 * family.GetCellAscent(styles[i]) / family.GetEmHeight(styles[i]);
			return new Font(familyName, height, styles[i], UnitPixel, collection);
		}
	}
	
	return NULL;
}

extern "C" static void
fz_gdiplusfillimage(void *user, fz_pixmap *image, fz_matrix ctm, float alpha);

extern "C" static void
fz_gdiplusfillpath(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	GraphicsPath *gpath = gdiplusgetpath(path, ctm, evenodd);
	Brush *brush = gdiplusgetbrush(user, colorspace, color, alpha);
	
	((userData *)user)->graphics->FillPath(brush, gpath);
	
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplusstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	GraphicsPath *gpath = gdiplusgetpath(path, ctm);
	Brush *brush = gdiplusgetbrush(user, colorspace, color, alpha);
	Pen *pen = gdiplusgetpen(brush, ctm, stroke);
	
	((userData *)user)->graphics->DrawPath(pen, gpath);
	
	delete pen;
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplusclippath(void *user, fz_path *path, int evenodd, fz_matrix ctm)
{
	GraphicsPath *gpath = gdiplusgetpath(path, ctm, evenodd);
	
	((userData *)user)->pushClip(gpath);
	
	delete gpath;
}

extern "C" static void
fz_gdiplusclipstrokepath(void *user, fz_path *path, fz_strokestate *stroke, fz_matrix ctm)
{
	GraphicsPath *gpath = gdiplusgetpath(path, ctm);
	
	Pen *pen = gdiplusgetpen(&SolidBrush(Color()), ctm, stroke);
	gpath->Widen(pen);
	
	((userData *)user)->pushClip(gpath);
	
	delete pen;
	delete gpath;
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
	ctrl1.x = from.x + 2.0/3 * (ctrl->x - from.x);
	ctrl1.y = from.y + 2.0/3 * (ctrl->y - from.y);
	ctrl2.x = ctrl1.x + 1.0/3 * (to->x - from.x);
	ctrl2.y = ctrl1.y + 1.0/3 * (to->y - from.y);
	
	return cubic_to(&ctrl1, &ctrl2, to, user);
}
static FT_Outline_Funcs OutlineFuncs = {
	move_to, line_to, conic_to, cubic_to, 0, 0 /* shift, delta */
};

static float
ftgetwidthscale(fz_font *font, int gid)
{
	if (font->ftsubstitute && gid < font->widthcount)
	{
		FT_Fixed advance = 0;
		FT_Get_Advance((FT_Face)font->ftface, gid, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP, &advance);
		if (advance)
			return 1024.0 * font->widthtable[gid] / advance;
	}
	
	return 1.0;
}

static WCHAR ftgetcharcode(fz_font *font, fz_textel *el)
{
	FT_Face face = (FT_Face)font->ftface;
	if (el->gid == FT_Get_Char_Index(face, el->ucs))
		return el->ucs;
	
	FT_UInt gid;
	WCHAR ucs = FT_Get_First_Char(face, &gid);
	while (gid != 0 && gid != el->gid)
		ucs = FT_Get_Next_Char(face, ucs, &gid);
	
	return ucs;
}

static void
gdiplusrendertext(Graphics *graphics, fz_text *text, fz_matrix ctm, Brush *brush, GraphicsPath *gpath=NULL)
{
	GraphicsPath *tpath = gpath ? gpath : new GraphicsPath();
	
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
		
		fz_matrix ctm2 = fz_translate(text->els[i].x, text->els[i].y);
		ctm2 = fz_concat(fz_scale(1.0 / face->units_per_EM, 1.0 / face->units_per_EM), ctm2);
		ctm2 = fz_concat(text->trm, ctm2);
		float widthScale = ftgetwidthscale(text->font, gid);
		if (widthScale != 1.0)
			ctm2 = fz_concat(fz_scale(widthScale, 1), ctm2);
		
		fz_path *path = fz_newpath();
		FT_Outline_Decompose(outline, &OutlineFuncs, path);
		GraphicsPath *gpath2 = gdiplusgetpath(path, ctm2, (outline->flags & FT_OUTLINE_EVEN_ODD_FILL));
		fz_freepath(path);
		
		tpath->AddPath(gpath2, FALSE);
		delete gpath2;
	}
	
	if (!gpath)
	{
		gdiplusapplytransform(tpath, ctm);
		graphics->FillPath(brush, tpath);
		delete tpath;
	}
}

static void
gdiplusruntext(Graphics *graphics, fz_text *text, fz_matrix ctm, Brush *brush)
{
	PrivateFontCollection collection;
	float fontSize = fz_matrixexpansion(text->trm), cellAscent = 0;
	
	Font *font = gdiplusgetfont(&collection, text->font, fontSize, &cellAscent);
	if (!font)
	{
		gdiplusrendertext(graphics, text, ctm, brush);
		return;
	}
	
	Matrix oldTransform;
	graphics->GetTransform(&oldTransform);
	
	const StringFormat *format = StringFormat::GenericTypographic();
	fz_matrix rotate = fz_concat(text->trm, fz_scale(-1.0 / fontSize, -1.0 / fontSize));
	assert(text->trm.e == 0 && text->trm.f == 0);
	
	for (int i = 0; i < text->len; i++)
	{
		WCHAR out = ftgetcharcode(text->font, &text->els[i]);
		if (!out)
		{
			fz_text t2 = *text;
			t2.len = 1;
			t2.els = &text->els[i];
			gdiplusrendertext(graphics, &t2, ctm, brush);
			continue;
		}
		
		fz_matrix ctm2 = fz_concat(fz_translate(text->els[i].x, text->els[i].y), ctm);
		ctm2 = fz_concat(fz_scale(-1, 1), fz_concat(rotate, ctm2));
		ctm2 = fz_concat(fz_translate(0, -fontSize * cellAscent), ctm2);
		float widthScale = ftgetwidthscale(text->font, text->els[i].gid);
		if (widthScale != 1.0)
			ctm2 = fz_concat(fz_scale(widthScale, 1), ctm2);
		
		gdiplusapplytransform(graphics, ctm2);
		graphics->DrawString(&out, 1, font, PointF(0, 0), format, brush);
	}
	
	graphics->SetTransform(&oldTransform);
	
	delete format;
	delete font;
}

static void
gdiplusrunt3text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha, GraphicsPath *gpath=NULL)
{
	// TODO: Type 3 glyphs are rendered with one pixel cut off (why?)
	
	if (gpath)
		fz_warn("stroking Type 3 glyphs is not supported");
	
	fz_font *font = text->font;
	
	for (int i = 0; i < text->len; i++)
	{
		int gid = text->els[i].gid;
		if (gid < 0 || gid > 255 || !font->t3procs[gid])
			continue;
		
		fz_bbox bbox;
		fz_device *dev = fz_newbboxdevice(&bbox);
		font->t3run((pdf_xref_s *)font->t3xref, font->t3resources, font->t3procs[gid], dev, fz_concat(font->t3matrix, text->trm));
		fz_freedevice(dev);
		
		fz_matrix ctm2 = fz_concat(fz_translate(text->els[i].x, text->els[i].y), ctm);
		ctm2 = fz_concat(text->trm, ctm2);
		ctm2 = fz_concat(font->t3matrix, ctm2);
		ctm2 = fz_concat(fz_translate(bbox.x0, bbox.y0), ctm2);
		
		font->t3run((pdf_xref_s *)font->t3xref, font->t3resources, font->t3procs[gid], ((userData *)user)->dev, ctm2);
	}
}

extern "C" static void
fz_gdiplusfilltext(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Brush *brush = gdiplusgetbrush(user, colorspace, color, alpha);
	if (text->font->ftface)
		gdiplusruntext(((userData *)user)->graphics, text, ctm, brush);
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
	GraphicsPath gpath;
	if (text->font->ftface)
		gdiplusrendertext(graphics, text, ctm, brush, &gpath);
	else
		gdiplusrunt3text(user, text, ctm, colorspace, color, alpha, &gpath);
	gdiplusapplytransform(&gpath, ctm);
	
	Pen *pen = gdiplusgetpen(brush, ctm, stroke);
	graphics->DrawPath(pen, &gpath);
	
	delete pen;
	delete brush;
}

extern "C" static void
fz_gdipluscliptext(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	GraphicsPath gpath;
	float black[3] = { 0 };
	if (text->font->ftface)
		gdiplusrendertext(((userData *)user)->graphics, text, ctm, NULL, &gpath);
	else
		gdiplusrunt3text(user, text, ctm, fz_devicebgr, black, 1.0, &gpath);
	gdiplusapplytransform(&gpath, ctm);
	
	((userData *)user)->pushClip(&gpath, 1.0, accumulate == 2);
}

extern "C" static void
fz_gdiplusclipstroketext(void *user, fz_text *text, fz_strokestate *stroke, fz_matrix ctm)
{
	GraphicsPath gpath;
	float black[3] = { 0 };
	if (text->font->ftface)
		gdiplusrendertext(((userData *)user)->graphics, text, ctm, NULL, &gpath);
	else
		gdiplusrunt3text(user, text, ctm, fz_devicebgr, black, 1.0, &gpath);
	gdiplusapplytransform(&gpath, ctm);
	Pen *pen = gdiplusgetpen(&SolidBrush(Color()), ctm, stroke);
	
	gpath.Widen(pen);
	((userData *)user)->pushClip(&gpath);
	
	delete pen;
}

extern "C" static void
fz_gdiplusignoretext(void *user, fz_text *text, fz_matrix ctm)
{
}

extern "C" static void
fz_gdiplusfillshade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	Rect clipRect;
	((userData *)user)->graphics->GetClipBounds(&clipRect);
	fz_bbox clip = { clipRect.X, clipRect.Y, clipRect.X + clipRect.Width, clipRect.Y + clipRect.Height };
	
	fz_rect bounds = fz_boundshade(shade, ctm);
	fz_bbox bbox = fz_intersectbbox(fz_roundrect(bounds), clip);
	
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
	PixmapBitmap bmp(image);
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
	
	Graphics *graphics = ((userData *)user)->graphics;
	Matrix oldTransform;
	graphics->GetTransform(&oldTransform);
	gdiplusapplytransform(graphics, ctm);
	graphics->DrawImage(bmp.bmp, Rect(0, 1, 1, -1), 0, 0, image->w, image->h, UnitPixel, &imgAttrs);
	graphics->SetTransform(&oldTransform);
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
	Region clip(Rect(0, 1, 1, -1));
	gdiplusapplytransform(&clip, ctm);
	((userData *)user)->pushClip(&clip);
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
	// fz_gdiplusclipimagemask(user, mask, fz_identity);
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
fz_newgdiplusdevice(void *hDC)
{
	if (InterlockedIncrement(&m_gdiplusUsage) == 1)
	{
		GdiplusStartupInput gdiplusStartupInput;
		GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
	}
	
	fz_device *dev = fz_newdevice(new userData((HDC)hDC));
	((userData *)dev->user)->dev = dev;
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
