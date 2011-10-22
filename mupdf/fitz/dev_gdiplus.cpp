// GDI+ rendering device for MuPDF
// Copyright (C) 2010 - 2011  Simon Bünzli <zeniko@gmail.com>

// This file is licensed under GPLv3 (see ../COPYING)

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

static ColorPalette *
gdiplus_create_grayscale_palette()
{
	ColorPalette * pal8bit = (ColorPalette *)fz_malloc(sizeof(ColorPalette) + 255 * sizeof(ARGB));
	
	pal8bit->Flags = PaletteFlagsGrayScale;
	pal8bit->Count = 256;
	for (int i = 0; i < 256; i++)
		pal8bit->Entries[i] = Color::MakeARGB(255, i, i, i);
	
	return pal8bit;
}

class PixmapBitmap : public Bitmap
{
	ColorPalette *pal8bit;

public:
	PixmapBitmap(fz_pixmap *pixmap) : Bitmap(pixmap->w, pixmap->h,
		pixmap->has_alpha ? PixelFormat32bppARGB : pixmap->colorspace != fz_device_gray ? PixelFormat24bppRGB : PixelFormat8bppIndexed),
		pal8bit(NULL)
	{
		BitmapData data;
		if (pixmap->colorspace == fz_device_gray && !pixmap->has_alpha)
		{
			SetPalette((pal8bit = gdiplus_create_grayscale_palette()));
			
			Status status = LockBits(&Rect(0, 0, pixmap->w, pixmap->h), ImageLockModeWrite, PixelFormat8bppIndexed, &data);
			if (status == Ok)
			{
				for (int y = 0; y < pixmap->h; y++)
					for (int x = 0; x < pixmap->w; x++)
						((unsigned char *)data.Scan0)[y * data.Stride + x] = pixmap->samples[(y * pixmap->w + x) * 2];
				UnlockBits(&data);
			}
			return;
		}
		
		fz_pixmap *pix;
		if (pixmap->colorspace != fz_device_bgr)
		{
			pix = fz_new_pixmap_with_limit(fz_device_bgr, pixmap->w, pixmap->h);
			if (!pix)
			{
				fz_warn("OOM in PixmapBitmap constructor: painting blank image");
				return;
			}
			pix->x = pixmap->x; pix->y = pixmap->y;
			
			if (!pixmap->colorspace)
				for (int i = 0; i < pix->w * pix->h; i++)
					pix->samples[i * 4 + 3] = pixmap->samples[i];
			else
				fz_convert_pixmap(pixmap, pix);
		}
		else
			pix = fz_keep_pixmap(pixmap);
		
		if (pixmap->has_alpha)
		{
			Status status = LockBits(&Rect(0, 0, pix->w, pix->h), ImageLockModeWrite, PixelFormat32bppARGB, &data);
			if (status == Ok)
			{
				memcpy(data.Scan0, pix->samples, pix->w * pix->h * pix->n);
				UnlockBits(&data);
			}
		}
		else
		{
			Status status = LockBits(&Rect(0, 0, pix->w, pix->h), ImageLockModeWrite, PixelFormat24bppRGB, &data);
			if (status == Ok)
			{
				for (int y = 0; y < pix->h; y++)
					for (int x = 0; x < pix->w; x++)
						for (int n = 0; n < 3; n++)
							((unsigned char *)data.Scan0)[y * data.Stride + x * 3 + n] = pix->samples[(y * pix->w + x) * 4 + n];
				UnlockBits(&data);
			}
		}
		
		fz_drop_pixmap(pix);
	}
	virtual ~PixmapBitmap()
	{
		fz_free(pal8bit);
	}
};

typedef BYTE (* separableBlend)(BYTE s, BYTE bg);
static BYTE BlendNormal(BYTE s, BYTE bg)     { return s; }
static BYTE BlendMultiply(BYTE s, BYTE bg)   { return s * bg / 255; }
static BYTE BlendScreen(BYTE s, BYTE bg)     { return 255 - (255 - s) * (255 - bg) / 255; }
static BYTE BlendHardLight(BYTE s, BYTE bg)  { return s <= 127 ? s * bg * 2 / 255 : BlendScreen((s - 128) * 2 + 1, bg); }
static BYTE BlendSoftLight(BYTE s, BYTE bg);
static BYTE BlendOverlay(BYTE s, BYTE bg)    { return BlendHardLight(bg, s); }
static BYTE BlendDarken(BYTE s, BYTE bg)     { return MIN(s, bg); }
static BYTE BlendLighten(BYTE s, BYTE bg)    { return MAX(s, bg); }
static BYTE BlendColorDodge(BYTE s, BYTE bg) { return bg == 0 ? 0 : bg >= 255 - s ? 255 : (510 * bg + 255 - s) / 2 / (255 - s); }
static BYTE BlendColorBurn(BYTE s, BYTE bg)  { return bg == 255 ? 255 : 255 - bg >= s ? 0 : 255 - (510 * (255 - bg) + s) / 2 / s; }
static BYTE BlendDifference(BYTE s, BYTE bg) { return ABS(s - bg); }
static BYTE BlendExclusion(BYTE s, BYTE bg)  { return s + bg - s * bg * 2 / 255; }

struct userDataStackItem
{
	Region clip;
	float alpha, layerAlpha;
	Graphics *saveG;
	Bitmap *layer, *mask;
	Rect bounds;
	bool luminosity;
	int blendmode;
	float xstep, ystep;
	fz_rect tileArea;
	fz_matrix tileCtm;
	userDataStackItem *prev;

	userDataStackItem(float _alpha=1.0, userDataStackItem *_prev=NULL) :
		alpha(_alpha), prev(_prev), saveG(NULL), layer(NULL), mask(NULL),
		luminosity(false), blendmode(0), xstep(0), ystep(0), layerAlpha(1.0) { }
};

class TempFile
{
public:
	WCHAR path[MAX_PATH];
	TempFile *next;

	TempFile(UCHAR *data, UINT size, TempFile *next=NULL) : next(next)
	{
		path[0] = L'\0';
		
		WCHAR tempPath[MAX_PATH - 14];
		DWORD res = GetTempPathW(MAX_PATH - 14, tempPath);
		if (!res || res >= MAX_PATH - 14 || !GetTempFileNameW(tempPath, L"DG+", 0, path))
			return;
		
		HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
			WriteFile(hFile, data, size, &res, NULL);
		CloseHandle(hFile);
	}

	~TempFile()
	{
		if (path[0])
			DeleteFileW(path);
		delete next;
	}
};

class DrawImageAttributes : public ImageAttributes
{
public:
	DrawImageAttributes(float alpha) : ImageAttributes()
	{
		ColorMatrix matrix = {
			1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.0f, alpha, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f, 1.0f
		};
		if (alpha != 1.0f)
			SetColorMatrix(&matrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
		SetWrapMode(WrapModeTileFlipXY);
	}
};

static PointF
gdiplus_transform_point(fz_matrix ctm, float x, float y)
{
	fz_point point = { x, y };
	point = fz_transform_point(ctm, point);
	return PointF(point.x, point.y);
}

inline float round(float x) { return floorf(x + 0.5); }
inline float roundup(float x) { return x < 0 ? floorf(x) : ceilf(x); }

class userData
{
	userDataStackItem *stack;
public:
	Graphics *graphics;
	fz_device *dev;
	fz_hash_table *outlines, *fontCollections;
	TempFile *tempFiles;
	float *t3color;

	userData(HDC hDC, fz_bbox clip) : stack(new userDataStackItem()), dev(NULL),
		outlines(NULL), fontCollections(NULL), tempFiles(NULL), t3color(NULL)
	{
		assert(GetMapMode(hDC) == MM_TEXT);
		graphics = _setup(new Graphics(hDC));
		graphics->GetClip(&stack->clip);
		graphics->SetClip(Rect(clip.x0, clip.y0, clip.x1 - clip.x0, clip.y1 - clip.y0));
	}

	~userData()
	{
		assert(stack && !stack->prev);
		if (stack)
			graphics->SetClip(&stack->clip);
		delete stack;
		delete graphics;
		
		if (outlines)
		{
			for (int i = 0; i < fz_hash_len(outlines); i++)
				delete (GraphicsPath *)fz_hash_get_val(outlines, i);
			fz_free_hash(outlines);
		}
		if (fontCollections)
		{
			for (int i = 0; i < fz_hash_len(fontCollections); i++)
				delete (PrivateFontCollection *)fz_hash_get_val(fontCollections, i);
			fz_free_hash(fontCollections);
		}
		delete tempFiles;
	}

	void pushClip(Region *clipRegion, float alpha=1.0)
	{
		assert(clipRegion);
		
		stack = new userDataStackItem(stack->alpha * alpha, stack);
		graphics->GetClip(&stack->clip);
		graphics->SetClip(clipRegion, CombineModeIntersect);
	}

	void pushClip(GraphicsPath *gpath, float alpha=1.0, bool accumulate=false)
	{
		if (accumulate)
			graphics->SetClip(&Region(gpath), CombineModeUnion);
		else
			pushClip(&Region(gpath), alpha);
	}

	void pushClipMask(fz_pixmap *mask, fz_matrix ctm)
	{
		Region clipRegion(Rect(0, 0, 1, 1));
		clipRegion.Transform(&Matrix(ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f));
		pushClip(&clipRegion);
		graphics->GetClip(&clipRegion);
		stack->layerAlpha = stack->alpha;
		stack->alpha = 1.0;
		
		RectF bounds;
		clipRegion.GetBounds(&bounds, graphics);
		stack->bounds.X = floorf(bounds.X); stack->bounds.Width = ceilf(bounds.Width) + 1;
		stack->bounds.Y = floorf(bounds.Y); stack->bounds.Height = ceilf(bounds.Height) + 1;
		
		stack->saveG = graphics;
		stack->layer = new Bitmap(stack->bounds.Width, stack->bounds.Height, PixelFormat32bppARGB);
		graphics = _setup(new Graphics(stack->layer));
		graphics->TranslateTransform(-stack->bounds.X, -stack->bounds.Y);
		graphics->SetClip(&Region(stack->bounds));
		
		if (mask)
		{
			assert(mask->n == 1 && !mask->colorspace);
			stack->mask = new Bitmap(stack->bounds.Width, stack->bounds.Height, PixelFormat32bppARGB);
			
			Graphics g2(stack->mask);
			_setup(&g2);
			g2.TranslateTransform(-stack->bounds.X, -stack->bounds.Y);
			drawPixmap(mask, ctm, 1.0f, &g2);
		}
	}

	void recordClipMask(fz_rect rect, bool luminosity, float *color)
	{
		RectF rectf(rect.x0, rect.y0, rect.x1 - rect.x0, rect.y1 - rect.y0);
		pushClipMask(NULL, fz_concat(fz_scale(rectf.Width, rectf.Height), fz_translate(rectf.X, rectf.Y)));
		if (luminosity)
			graphics->FillRectangle(&SolidBrush(Color(255, color[0] * 255, color[1] * 255, color[2] * 255)), rectf);
		stack->luminosity = luminosity;
		stack->blendmode = FZ_BLEND_ISOLATED | FZ_BLEND_KNOCKOUT;
	}

	void pushClipBlend(fz_rect rect, int blendmode, float alpha, bool isolated, bool knockout)
	{
		recordClipMask(rect, false, NULL);
		stack->layerAlpha *= alpha;
		stack->blendmode = blendmode | (isolated ? FZ_BLEND_ISOLATED : 0) | (knockout ? FZ_BLEND_KNOCKOUT : 0);
	}

	void applyClipMask()
	{
		assert(stack->layer && !stack->mask);
		stack->mask = stack->layer;
		stack->layer = new Bitmap(stack->bounds.Width, stack->bounds.Height, PixelFormat32bppARGB);
		delete graphics;
		graphics = _setup(new Graphics(stack->layer));
		graphics->TranslateTransform(-stack->bounds.X, -stack->bounds.Y);
		graphics->SetClip(&Region(stack->bounds));
	}

	void recordTile(fz_rect rect, fz_rect area, fz_matrix ctm, float xstep, float ystep)
	{
		pushClip(&Region());
		
		fz_bbox bbox = fz_round_rect(fz_transform_rect(ctm, rect));
		stack->bounds.X = bbox.x0; stack->bounds.Width = bbox.x1 - bbox.x0;
		stack->bounds.Y = bbox.y0; stack->bounds.Height = bbox.y1 - bbox.y0;
		
		stack->saveG = graphics;
		stack->layer = new Bitmap(stack->bounds.Width, stack->bounds.Height, PixelFormat32bppARGB);
		graphics = _setup(new Graphics(stack->layer));
		graphics->TranslateTransform(-stack->bounds.X, -stack->bounds.Y);
		graphics->SetClip(&Region(stack->bounds));
		
		area.x0 /= xstep; area.x1 /= xstep;
		area.y0 /= ystep; area.y1 /= ystep;
		ctm.e = bbox.x0; ctm.f = bbox.y0;
		
		stack->xstep = xstep;
		stack->ystep = ystep;
		stack->tileArea = area;
		stack->tileCtm = ctm;
	}

	void applyTiling()
	{
		assert(stack->layer && stack->saveG && stack->xstep && stack->ystep);
		
		for (int y = floorf(stack->tileArea.y0); y <= ceilf(stack->tileArea.y1); y++)
		{
			for (int x = floorf(stack->tileArea.x0); x <= ceilf(stack->tileArea.x1); x++)
			{
				fz_matrix ttm = fz_concat(fz_translate(x * stack->xstep, y * stack->ystep), stack->tileCtm);
				Rect bounds = stack->bounds;
				bounds.X = ttm.e;
				bounds.Y = ttm.f;
				stack->saveG->DrawImage(stack->layer, bounds, 0, 0, stack->layer->GetWidth(), stack->layer->GetHeight(), UnitPixel);
			}
		}
		
		delete graphics;
		graphics = stack->saveG;
		stack->saveG = NULL;
		delete stack->layer;
		stack->layer = NULL;
		
		popClip();
	}

	void popClip()
	{
		assert(stack->prev);
		if (!stack->prev)
			return;
		
		assert(!stack->layer == !stack->saveG);
		if (stack->layer)
		{
			delete graphics;
			graphics = stack->saveG;
			if (stack->mask)
			{
				_applyMask(stack->layer, stack->mask, stack->luminosity);
				delete stack->mask;
			}
			int blendmode = stack->blendmode & FZ_BLEND_MODEMASK;
			if (blendmode != 0)
				_applyBlend(stack->layer, stack->bounds, blendmode);
			
			graphics->DrawImage(stack->layer, stack->bounds, 0, 0, stack->layer->GetWidth(), stack->layer->GetHeight(), UnitPixel, &DrawImageAttributes(stack->layerAlpha));
			delete stack->layer;
		}
		
		graphics->SetClip(&stack->clip);
		userDataStackItem *prev = stack->prev;
		delete stack;
		stack = prev;
	}

	void drawPixmap(fz_pixmap *image, fz_matrix ctm, float alpha=1.0, Graphics *graphics=NULL) const
	{
		if (!image->samples)
			return;

		if (!graphics)
		{
			graphics = this->graphics;
			alpha = getAlpha(alpha);
		}
		
		// grid fit the image similar to draw_affine.c's fz_paint_image_imp
		if (fz_is_rectilinear(ctm))
		{
			ctm.a = roundup(ctm.a);
			ctm.b = roundup(ctm.b);
			ctm.c = roundup(ctm.c);
			ctm.d = roundup(ctm.d);
			ctm.e = round(ctm.e);
			ctm.f = round(ctm.f);
		}
		
		if (_hasSingleColor(image))
		{
			PointF corners[4] = {
				gdiplus_transform_point(ctm, 0, 0),
				gdiplus_transform_point(ctm, 0, 1),
				gdiplus_transform_point(ctm, 1, 1),
				gdiplus_transform_point(ctm, 1, 0)
			};
			
			float srcv[FZ_MAX_COLORS], rgb[3];
			for (int k = 0; k < image->colorspace->n; k++)
				srcv[k] = image->samples[k] / 255.0f;
			fz_convert_color(image->colorspace, srcv, fz_device_rgb, rgb);
			SolidBrush brush(Color(alpha * image->samples[image->colorspace->n],
				rgb[0] * 255, rgb[1] * 255, rgb[2] * 255));
			
			graphics->FillPolygon(&brush, corners, 4);
			
			return;
		}
		
		PointF corners[3] = {
			gdiplus_transform_point(ctm, 0, 1),
			gdiplus_transform_point(ctm, 1, 1),
			gdiplus_transform_point(ctm, 0, 0)
		};
		
		float scale = _hypotf(_hypotf(ctm.a, ctm.b), _hypotf(ctm.c, ctm.d)) / _hypotf(image->w, image->h);
		/* cf. fz_paint_image_imp in draw/imagedraw.c for when (not) to interpolate */
		bool downscale = _hypotf(ctm.a, ctm.b) < image->w && _hypotf(ctm.c, ctm.d) < image->h;
		bool alwaysInterpolate = downscale ||
			_hypotf(ctm.a, ctm.b) > image->w && _hypotf(ctm.c, ctm.d) > image->h &&
			_hypotf(ctm.a, ctm.b) < 2 * image->w && _hypotf(ctm.c, ctm.d) < 2 * image->h;
		
		if (!image->interpolate && !alwaysInterpolate && graphics == this->graphics)
		{
			GraphicsState state = graphics->Save();
			graphics->SetInterpolationMode(InterpolationModeNearestNeighbor);
			graphics->DrawImage(&PixmapBitmap(image), corners, 3, 0, 0, image->w, image->h, UnitPixel, &DrawImageAttributes(alpha));
			graphics->Restore(state);
		}
		else if (scale < 1.0 && MIN(image->w, image->h) > 10)
		{
			int w = round(image->w * scale);
			int h = round(image->h * scale);
			
			Bitmap *scaled = new Bitmap(w, h, image->has_alpha ? PixelFormat32bppARGB : PixelFormat24bppRGB);
			ColorPalette *pal8bit = NULL;
			if (!image->has_alpha && image->colorspace == fz_device_gray && w * h > (1 << 18))
			{
				delete scaled;
				scaled = new Bitmap(w, h, PixelFormat8bppIndexed);
				scaled->SetPalette((pal8bit = gdiplus_create_grayscale_palette()));
			}
			Graphics g2(scaled);
			_setup(&g2);
			g2.DrawImage(&PixmapBitmap(image), Rect(0, 0, w, h), 0, 0, image->w, image->h, UnitPixel, &DrawImageAttributes(1.0f));
			
			graphics->DrawImage(scaled, corners, _countof(corners), 0, 0, w, h, UnitPixel, &DrawImageAttributes(alpha));
			delete scaled;
			fz_free(pal8bit);
		}
		else
			graphics->DrawImage(&PixmapBitmap(image), corners, 3, 0, 0, image->w, image->h, UnitPixel, &DrawImageAttributes(alpha));
	}

	float getAlpha(float alpha=1.0) const { return stack->alpha * alpha; }

protected:
	Graphics *_setup(Graphics *graphics) const
	{
		graphics->SetPageUnit(UnitPoint);
		graphics->SetPageScale(72.0 / graphics->GetDpiY());
		graphics->SetCompositingMode(CompositingModeSourceOver);
		graphics->SetInterpolationMode(InterpolationModeHighQualityBicubic);
		graphics->SetSmoothingMode(SmoothingModeHighQuality);
		graphics->SetTextRenderingHint(TextRenderingHintAntiAlias);
		graphics->SetPixelOffsetMode(PixelOffsetModeHighQuality);
		
		return graphics;
	}

	void _applyMask(Bitmap *bitmap, Bitmap *mask, bool luminosity=false) const
	{
		Rect bounds(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
		assert(bounds.Width == mask->GetWidth() && bounds.Height == mask->GetHeight());
		
		BitmapData data, dataMask;
		Status status = bitmap->LockBits(&bounds, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &data);
		if (status != Ok)
			return;
		status = mask->LockBits(&bounds, ImageLockModeRead, PixelFormat32bppARGB, &dataMask);
		if (status != Ok)
		{
			bitmap->UnlockBits(&data);
			return;
		}
		
		for (int row = 0; row < bounds.Height; row++)
		{
			LPBYTE Scan0 = (LPBYTE)data.Scan0 + row * data.Stride;
			LPBYTE maskScan0 = (LPBYTE)dataMask.Scan0 + row * dataMask.Stride;
			
			for (int col = 0; col < bounds.Width; col++)
			{
				BYTE alpha = maskScan0[col * 4 + 3];
				if (luminosity)
				{
					float color[3], gray;
					color[0] = maskScan0[col * 4] / 255.0f;
					color[1] = maskScan0[col * 4 + 1] / 255.0f;
					color[2] = maskScan0[col * 4 + 2] / 255.0f;
					fz_convert_color(fz_device_rgb, color, fz_device_gray, &gray);
					alpha = gray * 255;
				}
				Scan0[col * 4 + 3] *= alpha / 255.0f;
			}
		}
		
		mask->UnlockBits(&dataMask);
		bitmap->UnlockBits(&data);
	}

	void _applyBlend(Bitmap *bitmap, Rect clipBounds, int blendmode) const
	{
		userDataStackItem *bgStack = stack->prev;
		while (bgStack && !bgStack->layer)
			bgStack = bgStack->prev;
		assert(bgStack);
		if (!bgStack)
		{
			fz_warn("background stack required for blending");
			return;
		}
		
		Rect boundsBg(clipBounds);
		for (userDataStackItem *si = bgStack; si; si = si->prev)
			if (si->layer)
				boundsBg.Intersect(si->bounds);
		
		Bitmap *backdrop = _flattenBlendBackdrop(bgStack, boundsBg);
		if (!backdrop)
		{
			fz_warn("OOM while flatting blend backdrop");
			return;
		}
		
		Rect bounds(boundsBg);
		bounds.Offset(-clipBounds.X, -clipBounds.Y);
		boundsBg.Offset(-boundsBg.X, -boundsBg.Y);
		
		_compositeWithBackground(bitmap, bounds, backdrop, boundsBg, blendmode, false);
		
		delete backdrop;
	}

	Bitmap *_flattenBlendBackdrop(userDataStackItem *group, Rect clipBounds) const
	{
		userDataStackItem *bgStack = group->prev;
		while (bgStack && !bgStack->layer)
			bgStack = bgStack->prev;
		
		if (!bgStack || (group->blendmode & FZ_BLEND_ISOLATED))
		{
			if ((group->blendmode & FZ_BLEND_KNOCKOUT))
				return new Bitmap(clipBounds.Width, clipBounds.Height, PixelFormat32bppARGB);
			
			clipBounds.Offset(-group->bounds.X, -group->bounds.Y);
			return group->layer->Clone(clipBounds, PixelFormat32bppARGB);
		}
		
		Bitmap *backdrop = _flattenBlendBackdrop(bgStack, clipBounds);
		if (!backdrop)
			return NULL;
		
		Rect bounds(clipBounds);
		bounds.Offset(-group->bounds.X, -group->bounds.Y);
		clipBounds.Offset(-clipBounds.X, -clipBounds.Y);
		
		if (!(group->blendmode & FZ_BLEND_KNOCKOUT))
			_compositeWithBackground(group->layer, bounds, backdrop, clipBounds, (group->blendmode & FZ_BLEND_MODEMASK), true);
		
		return backdrop;
	}

	void _compositeWithBackground(Bitmap *bitmap, Rect bounds, Bitmap *backdrop, Rect boundsBg, int blendmode, bool modifyBackdrop) const
	{
		separableBlend funcs[] = {
			BlendNormal,
			BlendMultiply,
			BlendScreen,
			BlendOverlay,
			BlendDarken,
			BlendLighten,
			BlendColorDodge,
			BlendColorBurn,
			BlendHardLight,
			BlendSoftLight,
			BlendDifference,
			BlendExclusion,
			NULL // FZ_BLEND_HUE, FZ_BLEND_SATURATION, FZ_BLEND_COLOR, FZ_BLEND_LUMINOSITY
		};
		
		if (blendmode >= nelem(funcs) || !funcs[blendmode])
		{
			fz_warn("blend mode %d not implemented for GDI+", blendmode);
			blendmode = 0;
		}
		
		assert(bounds.X >= 0 && bounds.Y >= 0);
		assert(boundsBg.X == 0 && boundsBg.Y == 0);
		assert(bounds.Width == boundsBg.Width && bounds.Height == boundsBg.Height);
		assert(boundsBg.Width == backdrop->GetWidth() && boundsBg.Height == backdrop->GetHeight());
		
		BitmapData data, dataBg;
		Status status = bitmap->LockBits(&bounds, ImageLockModeRead | (modifyBackdrop ? 0 : ImageLockModeWrite), PixelFormat32bppARGB, &data);
		if (status != Ok)
			return;
		status = backdrop->LockBits(&boundsBg, ImageLockModeRead | (modifyBackdrop ? ImageLockModeWrite : 0), PixelFormat32bppARGB, &dataBg);
		if (status != Ok)
		{
			bitmap->UnlockBits(&data);
			return;
		}
		
		LPBYTE Scan0 = (LPBYTE)data.Scan0, bgScan0 = (LPBYTE)dataBg.Scan0;
		for (int row = 0; row < bounds.Height; row++)
		{
			for (int col = 0; col < bounds.Width; col++)
			{
				if (bgScan0[row * dataBg.Stride + col * 4 + 3] > 0)
				{
					BYTE alpha = Scan0[row * data.Stride + col * 4 + 3];
					BYTE bgAlpha = bgScan0[row * dataBg.Stride + col * 4 + 3];
					BYTE newAlpha = BlendScreen(alpha, bgAlpha);
					// don't add background to the bitmap beyond its shape
					if (!modifyBackdrop && alpha < newAlpha)
						newAlpha = alpha;
					if (newAlpha != 0)
					{
						for (int i = 0; i < 3; i++)
						{
							BYTE color = Scan0[row * data.Stride + col * 4 + i];
							BYTE bgColor = bgScan0[row * dataBg.Stride + col * 4 + i];
							// basic compositing formula
							BYTE newColor = (1 - 1.0 * alpha / newAlpha) * bgColor + 1.0 * alpha / newAlpha * ((255 - bgAlpha) * color + bgAlpha * funcs[blendmode](color, bgColor)) / 255;
							
							if (modifyBackdrop)
								bgScan0[row * dataBg.Stride + col * 4 + i] = newColor;
							else
								Scan0[row * data.Stride + col * 4 + i] = newColor;
						}
					}
					if (modifyBackdrop)
						bgScan0[row * dataBg.Stride + col * 4 + 3] = newAlpha;
					else
						Scan0[row * data.Stride + col * 4 + 3] = newAlpha;
				}
			}
		}
		
		backdrop->UnlockBits(&dataBg);
		bitmap->UnlockBits(&data);
	}

	bool _hasSingleColor(fz_pixmap *image) const
	{
		if (image->w > 2 || image->h > 2 || !image->colorspace)
			return false;
		
		for (int i = 1; i < image->w * image->h; i++)
			if (memcmp(image->samples + i * image->n, image->samples, image->n) != 0)
				return false;
		
		return true;
	}
};


static void
gdiplus_apply_transform(Graphics *graphics, fz_matrix ctm)
{
	graphics->SetTransform(&Matrix(ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f));
}

static fz_matrix
gdiplus_get_transform(Graphics *graphics)
{
	fz_matrix ctm;
	Matrix matrix;
	
	graphics->GetTransform(&matrix);
	assert(sizeof(fz_matrix) == 6 * sizeof(REAL));
	matrix.GetElements((REAL *)&ctm);
	
	return ctm;
}

static void
gdiplus_apply_transform(GraphicsPath *gpath, fz_matrix ctm)
{
	gpath->Transform(&Matrix(ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f));
}

static Brush *
gdiplus_get_brush(void *user, fz_colorspace *colorspace, float *color, float alpha)
{
	float rgb[3];
	
	if (!((userData *)user)->t3color)
		fz_convert_color(colorspace, color, fz_device_rgb, rgb);
	else
		memcpy(rgb, ((userData *)user)->t3color, sizeof(rgb));
	
	return new SolidBrush(Color(((userData *)user)->getAlpha(alpha) * 255,
		rgb[0] * 255, rgb[1] * 255, rgb[2] * 255));
}

static GraphicsPath *
gdiplus_get_path(fz_path *path, fz_matrix ctm, int evenodd=1)
{
	PointF *points = new PointF[path->len / 2];
	BYTE *types = new BYTE[path->len / 2];
	PointF origin;
	int len = 0;
	
	for (int i = 0; i < path->len; )
	{
		switch (path->items[i++].k)
		{
		case FZ_MOVETO:
			points[len].X = path->items[i++].v; points[len].Y = path->items[i++].v;
			origin = points[len];
			// empty paths seem to confuse GDI+, so filter them out
			if (i < path->len && path->items[i].k == FZ_CLOSE_PATH)
				i++;
			else if (i < path->len && path->items[i].k != FZ_MOVETO)
				types[len++] = PathPointTypeStart;
			break;
		case FZ_LINETO:
			points[len].X = path->items[i++].v; points[len].Y = path->items[i++].v;
			types[len++] = PathPointTypeLine;
			break;
		case FZ_CURVETO:
			points[len].X = path->items[i++].v; points[len].Y = path->items[i++].v;
			types[len++] = PathPointTypeBezier;
			points[len].X = path->items[i++].v; points[len].Y = path->items[i++].v;
			types[len++] = PathPointTypeBezier;
			points[len].X = path->items[i++].v; points[len].Y = path->items[i++].v;
			types[len++] = PathPointTypeBezier;
			break;
		case FZ_CLOSE_PATH:
			types[len - 1] = types[len - 1] | PathPointTypeCloseSubpath;
			if (i < path->len && (path->items[i].k != FZ_MOVETO && path->items[i].k != FZ_CLOSE_PATH))
			{
				points[len] = origin;
				types[len++] = PathPointTypeStart;
			}
			break;
		}
	}
	assert(len <= path->len / 2);
	
	// clipping intermittently fails for overly large regions (cf. pathscan.c::fz_insertgel)
	fz_rect BBOX_BOUNDS = { -(1<<20), -(1<<20) , (1<<20), (1<<20) };
	BBOX_BOUNDS = fz_transform_rect(fz_invert_matrix(ctm), BBOX_BOUNDS);
	for (int i = 0; i < len; i++)
	{
		points[i].X = CLAMP(points[i].X, BBOX_BOUNDS.x0, BBOX_BOUNDS.x1);
		points[i].Y = CLAMP(points[i].Y, BBOX_BOUNDS.y0, BBOX_BOUNDS.y1);
	}
	
	GraphicsPath *gpath = new GraphicsPath(points, types, len, evenodd ? FillModeAlternate : FillModeWinding);
	gdiplus_apply_transform(gpath, ctm);
	
	delete[] points;
	delete[] types;
	
	return gpath;
}

static Pen *
gdiplus_get_pen(Brush *brush, fz_matrix ctm, fz_stroke_state *stroke)
{
	// TODO: pens are too narrow at low zoom levels
	float me = fz_matrix_expansion(ctm);
	Pen *pen = new Pen(brush, stroke->linewidth * me);
	pen->SetTransform(&Matrix(ctm.a / me, ctm.b / me, ctm.c / me, ctm.d / me, 0, 0));
	
	pen->SetMiterLimit(stroke->miterlimit);
	pen->SetLineCap(stroke->start_cap == 1 ? LineCapRound : stroke->start_cap == 2 ? LineCapSquare : LineCapFlat,
		stroke->end_cap == 1 ? LineCapRound : stroke->end_cap == 2 ? LineCapSquare : LineCapFlat,
		stroke->dash_cap == 1 ? DashCapRound : DashCapFlat);
	pen->SetLineJoin(stroke->linejoin == 1 ? LineJoinRound : stroke->linejoin == 2 ? LineJoinBevel : LineJoinMiter);
	
	if (stroke->dash_len > 0)
	{
		REAL dashlist[nelem(stroke->dash_list) + 1];
		int dashCount = stroke->dash_len;
		for (int i = 0; i < dashCount; i++)
		{
			dashlist[i] = stroke->dash_list[i] ? stroke->dash_list[i] / stroke->linewidth : 0.1 /* ??? */;
			if (stroke->dash_cap != 0)
				dashlist[i] += (i % 2 == 0) ? 1 : -1;
		}
		if (dashCount % 2 == 1)
		{
			dashlist[dashCount] = dashlist[dashCount - 1];
			dashCount++;
		}
		pen->SetDashPattern(dashlist, dashCount);
		pen->SetDashOffset(stroke->dash_phase);
	}
	
	return pen;
}

static Font *
gdiplus_get_font(userData *user, fz_font *font, float height, float *out_ascent)
{
	if (!font->ft_data && !font->ft_file)
		return NULL;
	
	if (!user->fontCollections)
		user->fontCollections = fz_new_hash_table(13, sizeof(font->name));
	PrivateFontCollection *collection = (PrivateFontCollection *)fz_hash_find(user->fontCollections, font->name);
	
	if (!collection)
	{
		collection = new PrivateFontCollection();
		assert(collection->GetFamilyCount() == 0);
		
		/* TODO: this seems to make print-outs larger rather than smaller * /
		if (font->ft_data)
		{
			user->tempFiles = new TempFile(font->ft_data, font->ft_size, user->tempFiles);
			if (*user->tempFiles->path)
				collection->AddFontFile(user->tempFiles->path);
			// TODO: memory fonts seem to get substituted in release builds
			// collection->AddMemoryFont(font->ft_data, font->ft_size);
		}
		else */ if (font->ft_file)
		{
			WCHAR fontPath[MAX_PATH];
			MultiByteToWideChar(CP_ACP, 0, font->ft_file, -1, fontPath, nelem(fontPath));
			collection->AddFontFile(fontPath);
		}
		
		fz_hash_insert(user->fontCollections, font->name, collection);
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
fz_gdiplus_fill_path(void *user, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	GraphicsPath *gpath = gdiplus_get_path(path, ctm, evenodd);
	Brush *brush = gdiplus_get_brush(user, colorspace, color, alpha);
	
	((userData *)user)->graphics->FillPath(brush, gpath);
	
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplus_stroke_path(void *user, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	GraphicsPath *gpath = gdiplus_get_path(path, ctm);
	Brush *brush = gdiplus_get_brush(user, colorspace, color, alpha);
	Pen *pen = gdiplus_get_pen(brush, ctm, stroke);
	
	((userData *)user)->graphics->DrawPath(pen, gpath);
	
	delete pen;
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplus_clip_path(void *user, fz_path *path, fz_rect *rect, int evenodd, fz_matrix ctm)
{
	GraphicsPath *gpath = gdiplus_get_path(path, ctm, evenodd);
	
	// TODO: clipping non-rectangular areas doesn't result in anti-aliased edges
	if (path->len > 0)
		((userData *)user)->pushClip(gpath);
	else
		((userData *)user)->pushClip(&Region(Rect()));
	
	delete gpath;
}

extern "C" static void
fz_gdiplus_clip_stroke_path(void *user, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm)
{
	GraphicsPath *gpath = gdiplus_get_path(path, ctm);
	
	Pen *pen = gdiplus_get_pen(&SolidBrush(Color()), ctm, stroke);
	gpath->Widen(pen);
	
	if (path->len > 0)
		((userData *)user)->pushClip(gpath);
	else
		((userData *)user)->pushClip(&Region(Rect()));
	
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
	from.x = path->items[path->len - 2].v;
	from.y = path->items[path->len - 1].v;
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
	if (font->ft_substitute && gid < font->width_count)
	{
		FT_Fixed advance = 0;
		FT_Face face = (FT_Face)font->ft_face;
		FT_Get_Advance(face, gid, FT_LOAD_NO_BITMAP | (font->ft_hint ? 0 : FT_LOAD_NO_HINTING), &advance);
		
		if (advance)
		{
			float charSize = (float)CLAMP(face->units_per_EM, 1000, 65536);
			return charSize * font->width_table[gid] / advance;
		}
	}
	
	return 1.0;
}

static WCHAR
ftgetcharcode(fz_font *font, fz_text_item *el)
{
	FT_Face face = (FT_Face)font->ft_face;
	if (el->gid == FT_Get_Char_Index(face, el->ucs))
		return el->ucs;
	
	FT_UInt gid;
	WCHAR ucs = FT_Get_First_Char(face, &gid), prev = ucs - 1;
	while (gid != 0 && ucs != prev && ucs < 256)
	{
		if (gid == el->gid)
			return ucs;
		ucs = FT_Get_Next_Char(face, (prev = ucs), &gid);
	}
	
	return 0;
}

typedef struct {
	FT_Face face;
	int gid;
} ftglyphkey;

static GraphicsPath *
ftrenderglyph(fz_font *font, int gid, fz_hash_table *outlines)
{
	FT_Face face = (FT_Face)font->ft_face;
	ftglyphkey key = { face, gid };
	
	GraphicsPath *glyph = (GraphicsPath *)fz_hash_find(outlines, &key);
	if (glyph)
		return glyph;
	
	FT_Error fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | (font->ft_hint ? 0 : FT_LOAD_NO_HINTING));
	if (fterr)
		return NULL;
	
	fz_path *path = fz_new_path();
	FT_Outline_Decompose(&face->glyph->outline, &OutlineFuncs, path);
	int evenodd = face->glyph->outline.flags & FT_OUTLINE_EVEN_ODD_FILL;
	
	glyph = gdiplus_get_path(path, fz_scale(ftgetwidthscale(font, gid), 1), evenodd);
	
	fz_free_path(path);
	fz_hash_insert(outlines, &key, glyph);
	
	return glyph;
}

static void
gdiplus_render_text(userData *user, fz_text *text, fz_matrix ctm, Brush *brush, GraphicsPath *gpath=NULL)
{
	Graphics *graphics = user->graphics;
	
	if (!user->outlines)
		user->outlines = fz_new_hash_table(509, sizeof(ftglyphkey));
	
	FT_Face face = (FT_Face)text->font->ft_face;
	FT_UShort charSize = CLAMP(face->units_per_EM, 1000, 65536);
	FT_Set_Char_Size(face, charSize, charSize, 72, 72);
	FT_Set_Transform(face, NULL, NULL);
	
	for (int i = 0; i < text->len; i++)
	{
		GraphicsPath *glyph = ftrenderglyph(text->font, text->items[i].gid, user->outlines);
		if (!glyph)
			continue;
		
		fz_matrix ctm2 = fz_translate(text->items[i].x, text->items[i].y);
		ctm2 = fz_concat(fz_scale(1.0 / charSize, 1.0 / charSize), ctm2);
		ctm2 = fz_concat(text->trm, ctm2);
		if (!gpath)
			ctm2 = fz_concat(ctm2, ctm);
		
		GraphicsPath *gpath2 = glyph->Clone();
		gdiplus_apply_transform(gpath2, ctm2);
		if (!gpath)
			graphics->FillPath(brush, gpath2);
		else
			gpath->AddPath(gpath2, FALSE);
		delete gpath2;
	}
}

static void
gdiplus_run_text(userData *user, fz_text *text, fz_matrix ctm, Brush *brush)
{
	float fontSize = fz_matrix_expansion(text->trm), cellAscent = 0;
	Font *font = gdiplus_get_font(user, text->font, fontSize, &cellAscent);
	if (!font)
	{
		gdiplus_render_text(user, text, ctm, brush);
		return;
	}
	
	Graphics *graphics = user->graphics;
	
	FT_Face face = (FT_Face)text->font->ft_face;
	FT_UShort charSize = CLAMP(face->units_per_EM, 1000, 65536);
	FT_Set_Char_Size(face, charSize, charSize, 72, 72);
	FT_Set_Transform(face, NULL, NULL);
	
	const StringFormat *format = StringFormat::GenericTypographic();
	fz_matrix rotate = fz_concat(text->trm, fz_scale(-1.0 / fontSize, -1.0 / fontSize));
	assert(text->trm.e == 0 && text->trm.f == 0);
	fz_matrix oldCtm = gdiplus_get_transform(graphics);
	
	for (int i = 0; i < text->len; i++)
	{
		WCHAR out = ftgetcharcode(text->font, &text->items[i]);
		if (!out)
		{
			fz_text t2 = *text;
			t2.len = 1;
			t2.items = &text->items[i];
			gdiplus_apply_transform(graphics, oldCtm);
			gdiplus_render_text(user, &t2, ctm, brush);
			continue;
		}
		
		fz_matrix ctm2 = fz_concat(fz_translate(text->items[i].x, text->items[i].y), ctm);
		ctm2 = fz_concat(fz_scale(-1, 1), fz_concat(rotate, ctm2));
		ctm2 = fz_concat(fz_translate(0, -fontSize * cellAscent), ctm2);
		float widthScale = ftgetwidthscale(text->font, text->items[i].gid);
		if (widthScale != 1.0)
			ctm2 = fz_concat(fz_scale(widthScale, 1), ctm2);
		
		gdiplus_apply_transform(graphics, fz_concat(ctm2, oldCtm));
		graphics->DrawString(&out, 1, font, PointF(0, 0), format, brush);
	}
	
	gdiplus_apply_transform(graphics, oldCtm);
	
	delete font;
}

static void
gdiplus_run_t3_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha, GraphicsPath *gpath=NULL)
{
	// TODO: type 3 glyphs are rendered slightly cropped
	if (gpath)
		fz_warn("stroking Type 3 glyphs is not supported");
	
	float rgb[3];
	fz_convert_color(colorspace, color, fz_device_rgb, rgb);
	((userData *)user)->t3color = rgb;
	
	fz_font *font = text->font;
	for (int i = 0; i < text->len; i++)
	{
		int gid = text->items[i].gid;
		if (gid < 0 || gid > 255 || !font->t3procs[gid])
			continue;
		
		fz_matrix ctm2 = fz_concat(fz_translate(text->items[i].x, text->items[i].y), ctm);
		ctm2 = fz_concat(text->trm, ctm2);
		ctm2 = fz_concat(font->t3matrix, ctm2);
		
		font->t3run((void *)font->t3xref, font->t3resources, font->t3procs[gid], ((userData *)user)->dev, ctm2);
	}
	
	((userData *)user)->t3color = NULL;
}

extern "C" static void
fz_gdiplus_fill_text(void *user, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Brush *brush = gdiplus_get_brush(user, colorspace, color, alpha);
	if (text->font->ft_face)
		gdiplus_run_text((userData *)user, text, ctm, brush);
	else
		gdiplus_run_t3_text(user, text, ctm, colorspace, color, alpha);
	
	delete brush;
}

extern "C" static void
fz_gdiplus_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	GraphicsPath gpath;
	if (text->font->ft_face)
		gdiplus_render_text((userData *)user, text, ctm, NULL, &gpath);
	else
		gdiplus_run_t3_text(user, text, ctm, colorspace, color, alpha, &gpath);
	gdiplus_apply_transform(&gpath, ctm);
	
	Brush *brush = gdiplus_get_brush(user, colorspace, color, alpha);
	Pen *pen = gdiplus_get_pen(brush, ctm, stroke);
	((userData *)user)->graphics->DrawPath(pen, &gpath);
	
	delete pen;
	delete brush;
}

extern "C" static void
fz_gdiplus_clip_text(void *user, fz_text *text, fz_matrix ctm, int accumulate)
{
	GraphicsPath gpath;
	float black[3] = { 0 };
	if (text->font->ft_face)
		gdiplus_render_text((userData *)user, text, ctm, NULL, &gpath);
	else
		gdiplus_run_t3_text(user, text, ctm, fz_device_rgb, black, 1.0, &gpath);
	gdiplus_apply_transform(&gpath, ctm);
	
	((userData *)user)->pushClip(&gpath, 1.0, accumulate == 2);
}

extern "C" static void
fz_gdiplus_clip_stroke_text(void *user, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	GraphicsPath gpath;
	float black[3] = { 0 };
	if (text->font->ft_face)
		gdiplus_render_text((userData *)user, text, ctm, NULL, &gpath);
	else
		gdiplus_run_t3_text(user, text, ctm, fz_device_rgb, black, 1.0, &gpath);
	gdiplus_apply_transform(&gpath, ctm);
	Pen *pen = gdiplus_get_pen(&SolidBrush(Color()), ctm, stroke);
	
	gpath.Widen(pen);
	((userData *)user)->pushClip(&gpath);
	
	delete pen;
}

extern "C" static void
fz_gdiplus_ignore_text(void *user, fz_text *text, fz_matrix ctm)
{
}

extern "C" static void
fz_gdiplus_fill_shade(void *user, fz_shade *shade, fz_matrix ctm, float alpha)
{
	Rect clipRect;
	((userData *)user)->graphics->GetClipBounds(&clipRect);
	fz_bbox clip = { clipRect.X, clipRect.Y, clipRect.X + clipRect.Width, clipRect.Y + clipRect.Height };
	
	fz_rect bounds = fz_bound_shade(shade, ctm);
	fz_bbox bbox = fz_intersect_bbox(fz_round_rect(bounds), clip);
	
	if (!fz_is_empty_rect(shade->bbox))
	{
		bounds = fz_transform_rect(fz_concat(shade->matrix, ctm), shade->bbox);
		bbox = fz_intersect_bbox(fz_round_rect(bounds), bbox);
	}
	
	if (fz_is_empty_rect(bbox))
		return;
	
	fz_pixmap *dest = fz_new_pixmap_with_rect(fz_device_rgb, bbox);
	fz_clear_pixmap(dest);
	
	if (shade->use_background)
	{
		float colorfv[4];
		fz_convert_color(shade->colorspace, shade->background, fz_device_rgb, colorfv);
		colorfv[3] = 1.0;
		
		for (int y = bbox.y0; y < bbox.y1; y++)
		{
			unsigned char *s = dest->samples + ((bbox.x0 - dest->x) + (y - dest->y) * dest->w) * 4;
			for (int x = bbox.x0; x < bbox.x1; x++)
				for (int i = 0; i < 4; i++)
					*s++ = colorfv[i] * 255;
		}
	}
	
	fz_paint_shade(shade, ctm, dest, bbox);
	
	ctm = fz_concat(fz_scale(dest->w, -dest->h), fz_translate(dest->x, dest->y + dest->h));
	((userData *)user)->drawPixmap(dest, ctm, alpha);
	
	fz_drop_pixmap(dest);
}

extern "C" static void
fz_gdiplus_fill_image(void *user, fz_pixmap *image, fz_matrix ctm, float alpha)
{
	((userData *)user)->drawPixmap(image, ctm, alpha);
}

extern "C" static void
fz_gdiplus_fill_image_mask(void *user, fz_pixmap *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	float rgb[3];
	if (!((userData *)user)->t3color)
		fz_convert_color(colorspace, color, fz_device_rgb, rgb);
	else
		memcpy(rgb, ((userData *)user)->t3color, sizeof(rgb));
	
	fz_pixmap *img2 = fz_new_pixmap_with_limit(fz_device_rgb, image->w, image->h);
	if (!img2)
		return;
	img2->x = image->x; img2->y = image->y;
	
	for (int i = 0; i < img2->w * img2->h; i++)
	{
		img2->samples[i * 4] = rgb[0] * 255;
		img2->samples[i * 4 + 1] = rgb[1] * 255;
		img2->samples[i * 4 + 2] = rgb[2] * 255;
		img2->samples[i * 4 + 3] = image->samples[i];
	}
	img2->interpolate = image->interpolate;
	
	((userData *)user)->drawPixmap(img2, ctm, alpha);
	
	fz_drop_pixmap(img2);
}

extern "C" static void
fz_gdiplus_clip_image_mask(void *user, fz_pixmap *image, fz_rect *rect, fz_matrix ctm)
{
	((userData *)user)->pushClipMask(image, ctm);
}

extern "C" static void
fz_gdiplus_pop_clip(void *user)
{
	((userData *)user)->popClip();
}

extern "C" static void
fz_gdiplus_begin_mask(void *user, fz_rect rect, int luminosity,
	fz_colorspace *colorspace, float *colorfv)
{
	float rgb[3] = { 0 };
	if (luminosity && colorspace && colorfv)
		fz_convert_color(colorspace, colorfv, fz_device_rgb, rgb);
	
	((userData *)user)->recordClipMask(rect, !!luminosity, rgb);
}

extern "C" static void
fz_gdiplus_end_mask(void *user)
{
	((userData *)user)->applyClipMask();
}

static BYTE BlendSoftLight(BYTE s, BYTE bg)
{
	if (s < 128)
		return bg - ((255 - 2 * s) / 255.0f * bg) / 255.0f * (255 - bg);
	
	int dbd;
	if (bg < 64)
		dbd = (((bg * 16 - 12) / 255.0f * bg) + 4) / 255.0f * bg;
	else
		dbd = (int)sqrtf(255.0f * bg);
	return bg + ((2 * s - 255) / 255.0f * (dbd - bg));
}

extern "C" static void
fz_gdiplus_begin_group(void *user, fz_rect rect, int isolated, int knockout,
	int blendmode, float alpha)
{
	((userData *)user)->pushClipBlend(rect, blendmode, alpha, !!isolated, !!knockout);
}

extern "C" static void
fz_gdiplus_end_group(void *user)
{
	((userData *)user)->popClip();
}

extern "C" static void
fz_gdiplus_begin_tile(void *user, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	((userData *)user)->recordTile(view, area, ctm, xstep, ystep);
}

extern "C" static void
fz_gdiplus_end_tile(void *user)
{
	((userData *)user)->applyTiling();
}

extern "C" static void
fz_gdiplus_free_user(void *user)
{
	delete (userData *)user;
	
	fz_synchronize_begin();
	if (--m_gdiplusUsage == 0)
		GdiplusShutdown(m_gdiplusToken);
	fz_synchronize_end();
}

fz_device *
fz_new_gdiplus_device(void *hDC, fz_bbox baseClip)
{
	fz_synchronize_begin();
	if (++m_gdiplusUsage == 1)
		GdiplusStartup(&m_gdiplusToken, &GdiplusStartupInput(), NULL);
	fz_synchronize_end();
	
	fz_device *dev = fz_new_device(new userData((HDC)hDC, baseClip));
	((userData *)dev->user)->dev = dev;
	dev->free_user = fz_gdiplus_free_user;
	
	dev->fill_path = fz_gdiplus_fill_path;
	dev->stroke_path = fz_gdiplus_stroke_path;
	dev->clip_path = fz_gdiplus_clip_path;
	dev->clip_stroke_path = fz_gdiplus_clip_stroke_path;
	
	dev->fill_text = fz_gdiplus_fill_text;
	dev->stroke_text = fz_gdiplus_stroke_text;
	dev->clip_text = fz_gdiplus_clip_text;
	dev->clip_stroke_text = fz_gdiplus_clip_stroke_text;
	dev->ignore_text = fz_gdiplus_ignore_text;
	
	dev->fill_shade = fz_gdiplus_fill_shade;
	dev->fill_image = fz_gdiplus_fill_image;
	dev->fill_image_mask = fz_gdiplus_fill_image_mask;
	dev->clip_image_mask = fz_gdiplus_clip_image_mask;
	
	dev->pop_clip = fz_gdiplus_pop_clip;
	
	dev->begin_mask = fz_gdiplus_begin_mask;
	dev->end_mask = fz_gdiplus_end_mask;
	dev->begin_group = fz_gdiplus_begin_group;
	dev->end_group = fz_gdiplus_end_group;
	
	dev->begin_tile = fz_gdiplus_begin_tile;
	dev->end_tile = fz_gdiplus_end_tile;
	
	return dev;
}

/* TODO: move concurrency related code to another file(?) */

class NativeLock
{
	CRITICAL_SECTION cs;

public:
	NativeLock() { InitializeCriticalSection(&cs); }
	~NativeLock() { DeleteCriticalSection(&cs); }

	void Acquire() { EnterCriticalSection(&cs); }
	void Release() { LeaveCriticalSection(&cs); }
};

static NativeLock globalLock;

extern "C" void fz_synchronize_begin()
{
	globalLock.Acquire();
}

extern "C" void fz_synchronize_end()
{
	globalLock.Release();
}
