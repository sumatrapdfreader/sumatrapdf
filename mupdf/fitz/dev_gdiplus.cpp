// GDI+ rendering device for MuPDF
// Copyright (C) 2010 - 2012  Simon Bünzli <zeniko@gmail.com>

// This file is licensed under GPLv3 (see ../COPYING)

#include <windows.h>
#include <gdiplus.h>
extern "C" {
#include "fitz-internal.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_ADVANCES_H
}

// define the following to output intermediary rendering stages to files
#undef DUMP_BITMAP_STEPS

using namespace Gdiplus;

static ULONG_PTR g_gdiplusToken;
static LONG g_gdiplusUsage = 0;

static ColorPalette *
gdiplus_create_grayscale_palette(fz_context *ctx, int depth)
{
	ColorPalette * palette = (ColorPalette *)fz_malloc(ctx, sizeof(ColorPalette) + (1 << depth) * sizeof(ARGB));
	
	palette->Flags = PaletteFlagsGrayScale;
	palette->Count = 1 << depth;
	for (unsigned int i = 0; i < palette->Count; i++)
	{
		int val = i * 255 / (palette->Count - 1);
		palette->Entries[i] = Color::MakeARGB(255, val, val, val);
	}
	
	return palette;
}

class PixmapBitmap : public Bitmap
{
	ColorPalette *palette;
	fz_context *ctx;

public:
	PixmapBitmap(fz_context *ctx, fz_pixmap *pixmap) : Bitmap(pixmap->w, pixmap->h,
		pixmap->has_alpha ? PixelFormat32bppPARGB:
		pixmap->colorspace != fz_device_gray ? PixelFormat24bppRGB :
		pixmap->single_bit ? PixelFormat1bppIndexed : PixelFormat8bppIndexed),
		palette(NULL), ctx(ctx)
	{
		BitmapData data;
		// 1-bit pixmap (black and white)
		if (pixmap->single_bit)
		{
			assert(pixmap->colorspace == fz_device_gray && !pixmap->has_alpha);
			SetPalette((palette = gdiplus_create_grayscale_palette(ctx, 1)));
			
			Status status = LockBits(&Rect(0, 0, pixmap->w, pixmap->h), ImageLockModeWrite, PixelFormat1bppIndexed, &data);
			if (status == Ok)
			{
				for (int y = 0; y < pixmap->h; y++)
				{
					for (int x = 0; x < pixmap->w; x += 8)
					{
						unsigned char byte = 0;
						for (int i = 0; i < 8 && i < pixmap->w - x; i++)
						{
							if (pixmap->samples[(y * pixmap->w + x + i) * 2])
								byte |= 1 << (7 - i);
						}
						((unsigned char *)data.Scan0)[y * data.Stride + x / 8] = byte;
					}
				}
				UnlockBits(&data);
			}
			return;
		}
		// 8-bit pixmap (grayscale)
		if (pixmap->colorspace == fz_device_gray && !pixmap->has_alpha)
		{
			SetPalette((palette = gdiplus_create_grayscale_palette(ctx, 8)));
			
			Status status = LockBits(&Rect(0, 0, pixmap->w, pixmap->h), ImageLockModeWrite, PixelFormat8bppIndexed, &data);
			if (status == Ok)
			{
				for (int y = 0; y < pixmap->h; y++)
				{
					for (int x = 0; x < pixmap->w; x++)
					{
						((unsigned char *)data.Scan0)[y * data.Stride + x] = pixmap->samples[(y * pixmap->w + x) * 2];
					}
				}
				UnlockBits(&data);
			}
			return;
		}
		// color pixmaps (24-bit or 32-bit)
		fz_pixmap *pix;
		if (pixmap->colorspace != fz_device_bgr)
		{
			fz_try(ctx)
			{
				pix = fz_new_pixmap_with_bbox(ctx, fz_device_bgr, fz_pixmap_bbox(ctx, pixmap));
			}
			fz_catch(ctx)
			{
				fz_warn(ctx, "OOM in PixmapBitmap constructor: painting blank image");
				return;
			}
			
			if (!pixmap->colorspace)
			{
				for (int i = 0; i < pix->w * pix->h; i++)
				{
					pix->samples[i * 4 + 3] = pixmap->samples[i];
				}
			}
			else
			{
				fz_convert_pixmap(ctx, pix, pixmap);
			}
		}
		else
		{
			pix = fz_keep_pixmap(ctx, pixmap);
		}
		// 32-bit pixmap (color and alpha)
		if (pixmap->has_alpha)
		{
			Status status = LockBits(&Rect(0, 0, pix->w, pix->h), ImageLockModeWrite, PixelFormat32bppARGB, &data);
			if (status == Ok)
			{
				memcpy(data.Scan0, pix->samples, pix->w * pix->h * pix->n);
				UnlockBits(&data);
			}
		}
		// 24-bit pixmap (color without alpha)
		else
		{
			Status status = LockBits(&Rect(0, 0, pix->w, pix->h), ImageLockModeWrite, PixelFormat24bppRGB, &data);
			if (status == Ok)
			{
				for (int y = 0; y < pix->h; y++)
				{
					for (int x = 0; x < pix->w; x++)
					{
						for (int n = 0; n < 3; n++)
						{
							((unsigned char *)data.Scan0)[y * data.Stride + x * 3 + n] = pix->samples[(y * pix->w + x) * 4 + n];
						}
					}
				}
				UnlockBits(&data);
			}
		}
		
		fz_drop_pixmap(ctx, pix);
	}
	virtual ~PixmapBitmap()
	{
		fz_free(ctx, palette);
	}
};

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
	fz_bbox tileArea;
	fz_matrix tileCtm;
	userDataStackItem *prev;

	userDataStackItem(float _alpha=1.0, userDataStackItem *_prev=NULL) :
		alpha(_alpha), prev(_prev), saveG(NULL), layer(NULL), mask(NULL),
		luminosity(false), blendmode(0), xstep(0), ystep(0), layerAlpha(1.0) { }

	~userDataStackItem()
	{
		delete layer;
		delete mask;
		delete saveG;
	}
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

static fz_pixmap *
fz_scale_pixmap_near(fz_context *ctx, fz_pixmap *src, int w, int h)
{
	fz_pixmap *dst = fz_new_pixmap(ctx, src->colorspace, w, h);
	unsigned char *d = dst->samples;
	
	int scale_x = (src->w << 16) / w;
	int scale_y = (src->h << 16) / h;
	for (int y = 0; y < h; y++)
	{
		unsigned char *line = src->samples + fz_mini((y * scale_y + (1 << 10)) >> 16, src->h - 1) * src->w * src->n;
		for (int x = 0; x < w; x++)
		{
			unsigned char *s = line + fz_mini((x * scale_x + (1 << 10)) >> 16, src->w - 1) * src->n;
			for (int n = 0; n < src->n; n++)
				*d++ = *s++;
		}
	}
	
	dst->interpolate = src->interpolate;
	dst->has_alpha = src->has_alpha;
	dst->single_bit = src->single_bit;
	return dst;
}

inline BYTE BlendScreen(BYTE s, BYTE bg) { return 255 - (255 - s) * (255 - bg) / 255; }

#ifdef DUMP_BITMAP_STEPS
static void
fz_dump_bitmap(fz_context *ctx, Bitmap *bmp, const char *msg)
{
	static int count = 1;
	
	CLSID clsid;
	UINT numEncoders, size;
	GetImageEncodersSize(&numEncoders, &size);
	ImageCodecInfo *codecInfo = (ImageCodecInfo *)fz_malloc(ctx, size);
	GetImageEncoders(numEncoders, size, codecInfo);
	for (UINT j = 0; j < numEncoders; j++)
		if (!wcscmp(codecInfo[j].MimeType, L"image/png"))
			clsid = codecInfo[j].Clsid;
	fz_free(ctx, codecInfo);
	
	WCHAR filename[MAX_PATH];
	_snwprintf(filename, nelem(filename), L"gdump%03d - %S.png", count, msg);
	filename[MAX_PATH - 1] = '\0';
	printf("%03d: %s (%dx%d)\n", count, msg, bmp->GetWidth(), bmp->GetHeight());
	count++;
	
	bmp->Save(filename, &clsid);
}
#endif

class userData
{
	userDataStackItem *stack;
	fz_context *ctx;
public:
	Graphics *graphics;
	fz_hash_table *outlines, *fontCollections;
	TempFile *tempFiles;
	float *t3color;
	bool transparency, started;

	userData(fz_context *ctx, HDC hDC, fz_rect clip) : stack(new userDataStackItem()),
		ctx(ctx), outlines(NULL), fontCollections(NULL), tempFiles(NULL), t3color(NULL),
		transparency(false), started(false)
	{
		assert(GetMapMode(hDC) == MM_TEXT);
		graphics = _setup(new Graphics(hDC));
		graphics->GetClip(&stack->clip);
		graphics->SetClip(RectF(clip.x0, clip.y0, clip.x1 - clip.x0, clip.y1 - clip.y0));
	}

	~userData()
	{
		if (transparency && stack && stack->prev && !stack->prev->prev)
		{
			transparency = false;
			popClip();
		}
		if (!stack)
			fz_warn(ctx, "draw stack base item is missing");
		else if (stack->prev)
			fz_warn(ctx, "draw stack is not empty");
		while (stack && stack->prev)
		{
			userDataStackItem *item = stack;
			stack = stack->prev;
			delete item;
		}
		if (stack)
			graphics->SetClip(&stack->clip);
		delete stack;
		delete graphics;
		
		if (outlines)
		{
			for (int i = 0; i < fz_hash_len(ctx, outlines); i++)
				delete (GraphicsPath *)fz_hash_get_val(ctx, outlines, i);
			fz_free_hash(ctx, outlines);
		}
		if (fontCollections)
		{
			for (int i = 0; i < fz_hash_len(ctx, fontCollections); i++)
			{
				fz_drop_font(ctx, *(fz_font **)fz_hash_get_key(ctx, fontCollections, i));
				delete (PrivateFontCollection *)fz_hash_get_val(ctx, fontCollections, i);
			}
			fz_free_hash(ctx, fontCollections);
		}
		delete tempFiles;
	}

	void pushClip(Region *clipRegion, float alpha=1.0)
	{
		assert(clipRegion);
		
		stack = new userDataStackItem(stack->alpha * alpha, stack);
		graphics->GetClip(&stack->clip);
		graphics->SetClip(clipRegion, CombineModeIntersect);
		started = true;
	}

	void pushClip(GraphicsPath *gpath, float alpha=1.0, bool accumulate=false)
	{
		if (accumulate)
			graphics->SetClip(&Region(gpath), CombineModeUnion);
		// contrary to Fitz, GDI+ ignores empty paths when clipping
		else if (gpath->GetPointCount() > 0)
			pushClip(&Region(gpath), alpha);
		else
			pushClip(&Region(Rect()), alpha);
		started = true;
	}

	void pushClipMask(fz_pixmap *mask, fz_matrix ctm)
	{
		Region clipRegion(Rect(0, 0, 1, 1));
		clipRegion.Transform(&Matrix(ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f));
		pushClip(&clipRegion);
		stack->layerAlpha = stack->alpha;
		stack->alpha = 1.0;
		
		RectF bounds;
		graphics->GetClipBounds(&bounds);
		stack->bounds.X = floorf(bounds.X); stack->bounds.Width = ceilf(bounds.Width) + 1;
		stack->bounds.Y = floorf(bounds.Y); stack->bounds.Height = ceilf(bounds.Height) + 1;
		
		stack->saveG = graphics;
		stack->layer = new Bitmap(stack->bounds.Width, stack->bounds.Height, PixelFormat32bppPARGB);
		graphics = _setup(new Graphics(stack->layer));
		graphics->TranslateTransform(-stack->bounds.X, -stack->bounds.Y);
		graphics->SetClip(&Region(stack->bounds));
		
		if (mask)
		{
			assert(mask->n == 1 && !mask->colorspace);
			stack->mask = new Bitmap(stack->bounds.Width, stack->bounds.Height, PixelFormat32bppPARGB);
			
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
		// heuristic to determine whether we'll have to keep the first
		// transparency group for rendering annotations as well
		if (!started)
			transparency = blendmode == FZ_BLEND_NORMAL && alpha == 1.0f && isolated && !knockout;
		recordClipMask(rect, false, NULL);
		stack->layerAlpha *= alpha;
		stack->blendmode = blendmode | (isolated ? FZ_BLEND_ISOLATED : 0) | (knockout ? FZ_BLEND_KNOCKOUT : 0);
	}

	void applyClipMask()
	{
		assert(stack->layer && !stack->mask);
		stack->mask = stack->layer;
		stack->layer = new Bitmap(stack->bounds.Width, stack->bounds.Height, PixelFormat32bppPARGB);
		delete graphics;
		graphics = _setup(new Graphics(stack->layer));
		graphics->TranslateTransform(-stack->bounds.X, -stack->bounds.Y);
		graphics->SetClip(&Region(stack->bounds));
	}

	void recordTile(fz_rect rect, fz_rect area, fz_matrix ctm, float xstep, float ystep)
	{
		pushClip(&Region());
		
		fz_bbox bbox = fz_bbox_from_rect(fz_transform_rect(ctm, rect));
		stack->bounds.X = bbox.x0; stack->bounds.Width = bbox.x1 - bbox.x0;
		stack->bounds.Y = bbox.y0; stack->bounds.Height = bbox.y1 - bbox.y0;
		
		stack->saveG = graphics;
		stack->layer = new Bitmap(stack->bounds.Width, stack->bounds.Height, PixelFormat32bppPARGB);
		graphics = _setup(new Graphics(stack->layer));
		graphics->TranslateTransform(-stack->bounds.X, -stack->bounds.Y);
		graphics->SetClip(&Region(stack->bounds));
		
		stack->xstep = xstep;
		stack->ystep = ystep;
		stack->tileCtm = ctm;
		stack->tileCtm.e = bbox.x0;
		stack->tileCtm.f = bbox.y0;
		
		RectF bounds;
		stack->saveG->GetClipBounds(&bounds);
		bounds.Inflate(stack->bounds.Width, stack->bounds.Height);
		fz_rect area2 = { bounds.X, bounds.Y, bounds.X + bounds.Width, bounds.Y + bounds.Height };
		area2 = fz_transform_rect(fz_invert_matrix(stack->tileCtm), area2);
		area = fz_intersect_rect(area, area2);
		
		stack->tileArea.x0 = floorf(area.x0 / xstep);
		stack->tileArea.y0 = floorf(area.y0 / ystep);
		stack->tileArea.x1 = ceilf(area.x1 / xstep + 0.001f);
		stack->tileArea.y1 = ceilf(area.y1 / ystep + 0.001f);
	}

	void applyTiling()
	{
		assert(stack->layer && stack->saveG && stack->xstep && stack->ystep);
#ifdef DUMP_BITMAP_STEPS
		dumpLayer("a single tile");
#endif
		
		for (int y = stack->tileArea.y0; y < stack->tileArea.y1; y++)
		{
			for (int x = stack->tileArea.x0; x < stack->tileArea.x1; x++)
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
		
#ifdef DUMP_BITMAP_STEPS
		dumpLayer("result of applyTiling", true);
#endif
		popClip();
	}

	void popClip()
	{
		assert(stack && stack->prev);
		if (!stack || !stack->prev)
			return;
		
		if (transparency && !stack->prev->prev)
		{
			// instead of blending onto the (white) background, blend onto
			// a white layer so that blending for annotations works as expected
			assert(stack->layer);
#ifdef DUMP_BITMAP_STEPS
			dumpLayer("before blending onto white background for annotations");
#endif
			Bitmap *whiteBg = new Bitmap(stack->layer->GetWidth(), stack->layer->GetHeight(), PixelFormat32bppPARGB);
			delete graphics;
			graphics = _setup(new Graphics(whiteBg));
			graphics->TranslateTransform(-stack->bounds.X, -stack->bounds.Y);
			graphics->SetClip(&Region(stack->bounds));
			graphics->Clear(Color::White);
			graphics->DrawImage(stack->layer, stack->bounds, 0, 0, stack->layer->GetWidth(), stack->layer->GetHeight(), UnitPixel, &DrawImageAttributes(1.0f));
			delete stack->layer;
			stack->layer = whiteBg;
#ifdef DUMP_BITMAP_STEPS
			dumpLayer("after blending onto white background for annotations");
#endif
			return;
		}
		
		assert(!stack->layer == !stack->saveG);
		if (stack->layer)
		{
#ifdef DUMP_BITMAP_STEPS
			dumpLayer("layer at start of popClip");
			dumpLayer(" background at start of popClip", true);
#endif
			delete graphics;
			graphics = stack->saveG;
			stack->saveG = NULL;
			if (stack->mask)
			{
#ifdef DUMP_BITMAP_STEPS
				fz_dump_bitmap(ctx, stack->mask, "  applying mask to layer");
#endif
				_applyMask(stack->layer, stack->mask, stack->luminosity);
#ifdef DUMP_BITMAP_STEPS
				dumpLayer("  result of _applyMask");
#endif
			}
			int blendmode = stack->blendmode & FZ_BLEND_MODEMASK;
			if (blendmode != 0)
			{
				_applyBlend(stack->layer, stack->bounds, blendmode);
#ifdef DUMP_BITMAP_STEPS
				dumpLayer("  applying blending to layer");
#endif
			}
			graphics->DrawImage(stack->layer, stack->bounds, 0, 0, stack->layer->GetWidth(), stack->layer->GetHeight(), UnitPixel, &DrawImageAttributes(stack->layerAlpha));
#ifdef DUMP_BITMAP_STEPS
			dumpLayer(" result of popClip", true);
#endif
		}
		
		graphics->SetClip(&stack->clip);
		userDataStackItem *prev = stack->prev;
		delete stack;
		stack = prev;
	}

	void applyTransferFunction(fz_transfer_function *tr, bool for_mask)
	{
		userDataStackItem *fgStack = stack;
		while (fgStack && !fgStack->layer)
			fgStack = fgStack->prev;
		assert(fgStack);
		if (!fgStack)
		{
			fz_warn(ctx, "layer stack item required for transfer functions");
			return;
		}
#ifdef DUMP_BITMAP_STEPS
		dumpLayer("layer before transfer function");
#endif
		
		Bitmap *layer = fgStack->layer;
		Rect bounds(0, 0, layer->GetWidth(), layer->GetHeight());
		BitmapData data;
		Status status = layer->LockBits(&bounds, ImageLockModeRead | ImageLockModeWrite, PixelFormat32bppARGB, &data);
		if (status != Ok)
			return;
		
		for (int row = 0; row < bounds.Height; row++)
		{
			LPBYTE Scan0 = (LPBYTE)data.Scan0 + row * data.Stride;
			for (int col = 0; col < bounds.Width; col++)
			{
				for (int n = 0; n < 3; n++)
					Scan0[col * 4 + n] = tr->function[2 - n][Scan0[col * 4 + n]];
				if (for_mask && !stack->luminosity)
					Scan0[col * 4 + 3] = tr->function[3][Scan0[col * 4 + 3]];
			}
		}
		
		layer->UnlockBits(&data);
		
#ifdef DUMP_BITMAP_STEPS
		dumpLayer("layer after transfer function");
#endif
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
		
		ctm = fz_gridfit_matrix(ctm);
		
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
			fz_convert_color(ctx, fz_device_rgb, rgb, image->colorspace, srcv);
			SolidBrush brush(Color(alpha * image->samples[image->colorspace->n],
				rgb[0] * 255, rgb[1] * 255, rgb[2] * 255));
			
			graphics->FillPolygon(&brush, corners, 4);
			
			return;
		}
		
		PointF corners[3] = {
			gdiplus_transform_point(ctm, 0, 0),
			gdiplus_transform_point(ctm, 1, 0),
			gdiplus_transform_point(ctm, 0, 1)
		};
		
		float scale = hypotf(hypotf(ctm.a, ctm.b), hypotf(ctm.c, ctm.d)) / hypotf(image->w, image->h);
		// cf. fz_paint_image_imp in draw/imagedraw.c for when (not) to interpolate
		bool downscale = hypotf(ctm.a, ctm.b) < image->w && hypotf(ctm.c, ctm.d) < image->h;
		bool alwaysInterpolate = downscale ||
			hypotf(ctm.a, ctm.b) > image->w && hypotf(ctm.c, ctm.d) > image->h &&
			hypotf(ctm.a, ctm.b) < 2 * image->w && hypotf(ctm.c, ctm.d) < 2 * image->h;
		
		if (!image->interpolate && !alwaysInterpolate && scale > 1.0 && fz_maxi(image->w, image->h) < 200 && fz_is_rectilinear(ctm))
		{
			fz_pixmap *scaledPixmap = NULL;
			fz_var(scaledPixmap);
			fz_try(ctx)
			{
#ifdef DUMP_BITMAP_STEPS
				fz_dump_bitmap(ctx, &PixmapBitmap(ctx, image), "image to scale");
#endif
				int w = floorf(hypotf(corners[0].X - corners[1].X, corners[0].Y - corners[1].Y) + 0.5f);
				int h = floorf(hypotf(corners[0].X - corners[2].X, corners[0].Y - corners[2].Y) + 0.5f);
				scaledPixmap = fz_scale_pixmap_near(ctx, image, w, h);
				graphics->DrawImage(&PixmapBitmap(ctx, scaledPixmap), corners, 3, 0, 0, w, h, UnitPixel, &DrawImageAttributes(alpha));
#ifdef DUMP_BITMAP_STEPS
				fz_dump_bitmap(ctx, &PixmapBitmap(ctx, scaledPixmap), "scaled image to draw");
#endif
			}
			fz_always(ctx)
			{
				fz_drop_pixmap(ctx, scaledPixmap);
			}
			fz_catch(ctx) { }
		}
		else if (!image->interpolate && !alwaysInterpolate)
		{
			GraphicsState state = graphics->Save();
			// TODO: why does this lead to subpar results when printing?
			graphics->SetInterpolationMode(InterpolationModeNearestNeighbor);
			graphics->DrawImage(&PixmapBitmap(ctx, image), corners, 3, 0, 0, image->w, image->h, UnitPixel, &DrawImageAttributes(alpha));
#ifdef DUMP_BITMAP_STEPS
			fz_dump_bitmap(ctx, &PixmapBitmap(ctx, image), "image to draw");
#endif
			graphics->Restore(state);
		}
		else if (scale < 1.0 && fz_mini(image->w, image->h) > 100 && !image->has_alpha && fz_is_rectilinear(ctm))
		{
			fz_pixmap *scaledPixmap = NULL;
			fz_var(scaledPixmap);
			fz_try(ctx)
			{
#ifdef DUMP_BITMAP_STEPS
				fz_dump_bitmap(ctx, &PixmapBitmap(ctx, image), "image to scale");
#endif
				int w = floorf(hypotf(corners[0].X - corners[1].X, corners[0].Y - corners[1].Y) + 0.5f);
				int h = floorf(hypotf(corners[0].X - corners[2].X, corners[0].Y - corners[2].Y) + 0.5f);
				scaledPixmap = fz_scale_pixmap(ctx, image, 0, 0, w, h, NULL);
				if (scaledPixmap)
				{
					graphics->DrawImage(&PixmapBitmap(ctx, scaledPixmap), corners, 3, 0, 0, w, h, UnitPixel, &DrawImageAttributes(alpha));
#ifdef DUMP_BITMAP_STEPS
					fz_dump_bitmap(ctx, &PixmapBitmap(ctx, scaledPixmap), "scaled image to draw");
#endif
				}
			}
			fz_always(ctx)
			{
				fz_drop_pixmap(ctx, scaledPixmap);
			}
			fz_catch(ctx) { }
		}
		else
		{
			graphics->DrawImage(&PixmapBitmap(ctx, image), corners, 3, 0, 0, image->w, image->h, UnitPixel, &DrawImageAttributes(alpha));
#ifdef DUMP_BITMAP_STEPS
			fz_dump_bitmap(ctx, &PixmapBitmap(ctx, image), "image to draw");
#endif
		}
#ifdef DUMP_BITMAP_STEPS
		dumpLayer("result of drawPixmap");
#endif
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
		graphics->SetPixelOffsetMode(PixelOffsetModeHalf);
		
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
		
		fz_color_converter cc;
		fz_find_color_converter(&cc, ctx, fz_device_gray, fz_device_rgb);
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
					cc.convert(&cc, &gray, color);
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
			fz_warn(ctx, "background stack item required for blending");
			return;
		}
		
		Rect boundsBg(clipBounds);
		for (userDataStackItem *si = bgStack; si; si = si->prev)
			if (si->layer)
				boundsBg.Intersect(si->bounds);
		if (boundsBg.Width == 0 || boundsBg.Height == 0)
			return;
		
		Bitmap *backdrop = _flattenBlendBackdrop(bgStack, boundsBg);
		if (!backdrop)
		{
			fz_warn(ctx, "OOM while flatting blend backdrop");
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
				return new Bitmap(clipBounds.Width, clipBounds.Height, PixelFormat32bppPARGB);
			
			clipBounds.Offset(-group->bounds.X, -group->bounds.Y);
			return group->layer->Clone(clipBounds, PixelFormat32bppPARGB);
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
						// note: GDI+ stores colors as BGRA order while Fitz expects RGBA
						BYTE rgb[3], rgbSrc[3], rgbBg[3];
						for (int i = 0; i < 3; i++)
						{
							rgbSrc[i] = Scan0[row * data.Stride + col * 4 + 2 - i];
							rgbBg[i] = bgScan0[row * dataBg.Stride + col * 4 + 2 - i];
						}
						fz_blend_pixel(rgb, rgbBg, rgbSrc, blendmode);
						for (int i = 0; i < 3; i++)
						{
							// basic compositing formula
							BYTE newColor = (1 - 1.0 * alpha / newAlpha) * rgbBg[i] + 1.0 * alpha / newAlpha * ((255 - bgAlpha) * rgbSrc[i] + bgAlpha * rgb[i]) / 255;
							if (modifyBackdrop)
								bgScan0[row * dataBg.Stride + col * 4 + 2 - i] = newColor;
							else
								Scan0[row * data.Stride + col * 4 + 2 - i] = newColor;
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
		{
			if (memcmp(image->samples + i * image->n, image->samples, image->n) != 0)
				return false;
		}
		return true;
	}

#ifdef DUMP_BITMAP_STEPS
public:
	void dumpLayer(const char *msg, bool bgLayer=false) const
	{
		userDataStackItem *bgStack = bgLayer ? stack->prev : stack;
		while (bgStack && !bgStack->layer)
			bgStack = bgStack->prev;
		if (bgStack)
			fz_dump_bitmap(ctx, bgStack->layer, msg);
	}
#endif
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
gdiplus_get_brush(fz_device *dev, fz_colorspace *colorspace, float *color, float alpha)
{
	float rgb[3];
	
	if (!((userData *)dev->user)->t3color)
		fz_convert_color(dev->ctx, fz_device_rgb, rgb, colorspace, color);
	else
		memcpy(rgb, ((userData *)dev->user)->t3color, sizeof(rgb));
	
	return new SolidBrush(Color(((userData *)dev->user)->getAlpha(alpha) * 255,
		rgb[0] * 255, rgb[1] * 255, rgb[2] * 255));
}

static GraphicsPath *
gdiplus_get_path(fz_path *path, fz_matrix ctm, bool has_caps, int evenodd=1)
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
			// zero-length paths are omitted by GDI+ even if the have start/end caps
			if (has_caps && points[len].Equals(points[len - 1]))
				points[len].X += 0.01f / fz_matrix_expansion(ctm);
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
		points[i].X = fz_clamp(points[i].X, BBOX_BOUNDS.x0, BBOX_BOUNDS.x1);
		points[i].Y = fz_clamp(points[i].Y, BBOX_BOUNDS.y0, BBOX_BOUNDS.y1);
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
	pen->SetLineCap(stroke->start_cap == FZ_LINECAP_ROUND ? LineCapRound : stroke->start_cap == FZ_LINECAP_SQUARE ? LineCapSquare : stroke->start_cap == FZ_LINECAP_TRIANGLE ? LineCapTriangle : LineCapFlat,
		stroke->end_cap == FZ_LINECAP_ROUND ? LineCapRound : stroke->end_cap == FZ_LINECAP_SQUARE ? LineCapSquare : stroke->end_cap == FZ_LINECAP_TRIANGLE ? LineCapTriangle : LineCapFlat,
		stroke->dash_cap == FZ_LINECAP_ROUND ? DashCapRound : stroke->dash_cap == FZ_LINECAP_TRIANGLE ? DashCapTriangle : DashCapFlat);
	pen->SetLineJoin(stroke->linejoin == FZ_LINEJOIN_ROUND ? LineJoinRound : stroke->linejoin == FZ_LINEJOIN_BEVEL ? LineJoinBevel : stroke->linejoin == FZ_LINEJOIN_MITER_XPS ? LineJoinMiter : LineJoinMiterClipped);
	
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
		pen->SetDashOffset(stroke->dash_phase / stroke->linewidth);
	}
	
	return pen;
}

static Font *
gdiplus_get_font(fz_device *dev, fz_font *font, float height, float *out_ascent)
{
	userData *user = (userData *)dev->user;
	if (!font->ft_data && !font->ft_file)
		return NULL;
	if (font->ft_bold || font->ft_italic)
		return NULL;
	
	if (!user->fontCollections)
		user->fontCollections = fz_new_hash_table(dev->ctx, 13, sizeof(font), -1);
	PrivateFontCollection *collection = (PrivateFontCollection *)fz_hash_find(dev->ctx, user->fontCollections, &font);
	
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
		
		fz_keep_font(dev->ctx, font);
		fz_hash_insert(dev->ctx, user->fontCollections, &font, collection);
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
fz_gdiplus_fill_path(fz_device *dev, fz_path *path, int evenodd, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	GraphicsPath *gpath = gdiplus_get_path(path, ctm, false, evenodd);
	Brush *brush = gdiplus_get_brush(dev, colorspace, color, alpha);
	
	((userData *)dev->user)->started = true;
	((userData *)dev->user)->graphics->FillPath(brush, gpath);
	
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplus_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	GraphicsPath *gpath = gdiplus_get_path(path, ctm, stroke->start_cap || stroke->end_cap);
	Brush *brush = gdiplus_get_brush(dev, colorspace, color, alpha);
	Pen *pen = gdiplus_get_pen(brush, ctm, stroke);
	
	((userData *)dev->user)->started = true;
	((userData *)dev->user)->graphics->DrawPath(pen, gpath);
	
	delete pen;
	delete brush;
	delete gpath;
}

extern "C" static void
fz_gdiplus_clip_path(fz_device *dev, fz_path *path, fz_rect rect, int evenodd, fz_matrix ctm)
{
	GraphicsPath *gpath = gdiplus_get_path(path, ctm, false, evenodd);
	
	// TODO: clipping non-rectangular areas doesn't result in anti-aliased edges
	((userData *)dev->user)->pushClip(gpath);
	
	delete gpath;
}

extern "C" static void
fz_gdiplus_clip_stroke_path(fz_device *dev, fz_path *path, fz_rect rect, fz_stroke_state *stroke, fz_matrix ctm)
{
	GraphicsPath *gpath = gdiplus_get_path(path, ctm, stroke->start_cap || stroke->end_cap);
	
	Pen *pen = gdiplus_get_pen(&SolidBrush(Color()), ctm, stroke);
	gpath->Widen(pen);
	
	((userData *)dev->user)->pushClip(gpath);
	
	delete pen;
	delete gpath;
}

struct PathContext {
	PathContext(fz_context *ctx, fz_path *path) : ctx(ctx), path(path) { }
	fz_context *ctx;
	fz_path *path;
};

extern "C" static int move_to(const FT_Vector *to, void *user)
{
	PathContext *data = (PathContext *)user;
	fz_moveto(data->ctx, data->path, to->x, to->y);
	return 0;
}
extern "C" static int line_to(const FT_Vector *to, void *user)
{
	PathContext *data = (PathContext *)user;
	fz_lineto(data->ctx, data->path, to->x, to->y);
	return 0;
}
extern "C" static int cubic_to(const FT_Vector *ctrl1, const FT_Vector *ctrl2, const FT_Vector *to, void *user)
{
	PathContext *data = (PathContext *)user;
	fz_curveto(data->ctx, data->path, ctrl1->x, ctrl1->y, ctrl2->x, ctrl2->y, to->x, to->y);
	return 0;
}
extern "C" static int conic_to(const FT_Vector *ctrl, const FT_Vector *to, void *user)
{
	PathContext *data = (PathContext *)user;
	FT_Vector from, ctrl1, ctrl2;
	
	assert(data->path->len > 0);
	if (data->path->len == 0)
		fz_moveto(data->ctx, data->path, 0, 0);
	
	// cf. http://fontforge.sourceforge.net/bezier.html
	from.x = data->path->items[data->path->len - 2].v;
	from.y = data->path->items[data->path->len - 1].v;
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
ft_get_width_scale(fz_font *font, int gid)
{
	if (font->ft_substitute && gid < font->width_count)
	{
		FT_Fixed advance = 0;
		FT_Face face = (FT_Face)font->ft_face;
		FT_Get_Advance(face, gid, FT_LOAD_NO_BITMAP | (font->ft_hint ? 0 : FT_LOAD_NO_HINTING), &advance);
		
		if (advance)
		{
			float charSize = (float)fz_clampi(face->units_per_EM, 1000, 65536);
			return charSize * font->width_table[gid] / advance;
		}
	}
	
	return 1.0;
}

static WCHAR
ft_get_charcode(fz_font *font, fz_text_item *el)
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
ft_render_glyph(fz_context *ctx, fz_font *font, int gid, fz_hash_table *outlines)
{
	FT_Face face = (FT_Face)font->ft_face;
	ftglyphkey key = { face, gid };
	
	GraphicsPath *glyph = (GraphicsPath *)fz_hash_find(ctx, outlines, &key);
	if (glyph)
		return glyph;
	
	FT_Error fterr = FT_Load_Glyph(face, gid, FT_LOAD_NO_BITMAP | (font->ft_hint ? 0 : FT_LOAD_NO_HINTING));
	if (fterr)
		return NULL;
	
	if (font->ft_bold)
	{
		float unit = 26.6f;
		FT_Outline_Embolden(&face->glyph->outline, 2 * unit);
		FT_Outline_Translate(&face->glyph->outline, -unit, -unit);
	}
	
	fz_path *path = fz_new_path(ctx);
	FT_Outline_Decompose(&face->glyph->outline, &OutlineFuncs, &PathContext(ctx, path));
	int evenodd = face->glyph->outline.flags & FT_OUTLINE_EVEN_ODD_FILL;
	
	glyph = gdiplus_get_path(path, fz_scale(ft_get_width_scale(font, gid), 1), false, evenodd);
	
	fz_free_path(ctx, path);
	fz_hash_insert(ctx, outlines, &key, glyph);
	
	return glyph;
}

static void
gdiplus_render_text(fz_device *dev, fz_text *text, fz_matrix ctm, Brush *brush, GraphicsPath *gpath=NULL)
{
	userData *user = (userData *)dev->user;
	Graphics *graphics = user->graphics;
	
	if (!user->outlines)
		user->outlines = fz_new_hash_table(dev->ctx, 509, sizeof(ftglyphkey), -1);
	
	FT_Face face = (FT_Face)text->font->ft_face;
	FT_UShort charSize = fz_clampi(face->units_per_EM, 1000, 65536);
	FT_Set_Char_Size(face, charSize, charSize, 72, 72);
	FT_Set_Transform(face, NULL, NULL);
	
	for (int i = 0; i < text->len; i++)
	{
		GraphicsPath *glyph = ft_render_glyph(dev->ctx, text->font, text->items[i].gid, user->outlines);
		if (!glyph)
			continue;
		
		fz_matrix ctm2 = fz_translate(text->items[i].x, text->items[i].y);
		ctm2 = fz_concat(fz_scale(1.0 / charSize, 1.0 / charSize), ctm2);
		ctm2 = fz_concat(text->trm, ctm2);
		if (text->font->ft_italic)
			ctm2 = fz_concat(fz_shear(0.3f, 0), ctm2);
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
gdiplus_run_text(fz_device *dev, fz_text *text, fz_matrix ctm, Brush *brush)
{
	float fontSize = fz_matrix_expansion(text->trm), cellAscent = 0;
	Font *font = gdiplus_get_font(dev, text->font, fontSize, &cellAscent);
	if (!font)
	{
		gdiplus_render_text(dev, text, ctm, brush);
		return;
	}
	
	Graphics *graphics = ((userData *)dev->user)->graphics;
	
	FT_Face face = (FT_Face)text->font->ft_face;
	FT_UShort charSize = fz_clampi(face->units_per_EM, 1000, 65536);
	FT_Set_Char_Size(face, charSize, charSize, 72, 72);
	FT_Set_Transform(face, NULL, NULL);
	
	const StringFormat *format = StringFormat::GenericTypographic();
	fz_matrix rotate = fz_concat(text->trm, fz_scale(-1.0 / fontSize, -1.0 / fontSize));
	assert(text->trm.e == 0 && text->trm.f == 0);
	fz_matrix oldCtm = gdiplus_get_transform(graphics);
	
	for (int i = 0; i < text->len; i++)
	{
		WCHAR out = ft_get_charcode(text->font, &text->items[i]);
		if (!out)
		{
			fz_text t2 = *text;
			t2.len = 1;
			t2.items = &text->items[i];
			gdiplus_apply_transform(graphics, oldCtm);
			gdiplus_render_text(dev, &t2, ctm, brush);
			continue;
		}
		
		fz_matrix ctm2 = fz_concat(fz_translate(text->items[i].x, text->items[i].y), ctm);
		ctm2 = fz_concat(fz_scale(-1, 1), fz_concat(rotate, ctm2));
		ctm2 = fz_concat(fz_translate(0, -fontSize * cellAscent), ctm2);
		float widthScale = ft_get_width_scale(text->font, text->items[i].gid);
		if (widthScale != 1.0)
			ctm2 = fz_concat(fz_scale(widthScale, 1), ctm2);
		
		gdiplus_apply_transform(graphics, fz_concat(ctm2, oldCtm));
		graphics->DrawString(&out, 1, font, PointF(0, 0), format, brush);
	}
	
	gdiplus_apply_transform(graphics, oldCtm);
	
	delete font;
}

static void
gdiplus_run_t3_text(fz_device *dev, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha, GraphicsPath *gpath=NULL)
{
	// TODO: type 3 glyphs are rendered slightly cropped
	if (gpath)
		fz_warn(dev->ctx, "stroking Type 3 glyphs is not supported");
	
	// TODO: support Type 3 glyphs recursively drawing other Type 3 glyphs
	if (((userData *)dev->user)->t3color)
	{
		fz_warn(dev->ctx, "drawing Type 3 glyphs recursively is not supported");
		return;
	}
	
	float rgb[3];
	fz_convert_color(dev->ctx, fz_device_rgb, rgb, colorspace, color);
	((userData *)dev->user)->t3color = rgb;
	
	fz_font *font = text->font;
	for (int i = 0; i < text->len; i++)
	{
		int gid = text->items[i].gid;
		if (gid < 0 || gid > 255 || !font->t3procs[gid])
			continue;
		
		fz_matrix ctm2 = fz_concat(fz_translate(text->items[i].x, text->items[i].y), ctm);
		ctm2 = fz_concat(text->trm, ctm2);
		ctm2 = fz_concat(font->t3matrix, ctm2);
		
		font->t3run(font->t3doc, font->t3resources, font->t3procs[gid], dev, ctm2, NULL, 0);
	}
	
	((userData *)dev->user)->t3color = NULL;
}

extern "C" static void
fz_gdiplus_fill_text(fz_device *dev, fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	Brush *brush = gdiplus_get_brush(dev, colorspace, color, alpha);
	((userData *)dev->user)->started = true;
	if (text->font->ft_face)
		gdiplus_run_text(dev, text, ctm, brush);
	else
		gdiplus_run_t3_text(dev, text, ctm, colorspace, color, alpha);
	
	delete brush;
}

extern "C" static void
fz_gdiplus_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	GraphicsPath gpath(FillModeWinding);
	if (text->font->ft_face)
		gdiplus_render_text(dev, text, ctm, NULL, &gpath);
	else
		gdiplus_run_t3_text(dev, text, ctm, colorspace, color, alpha, &gpath);
	gdiplus_apply_transform(&gpath, ctm);
	
	Brush *brush = gdiplus_get_brush(dev, colorspace, color, alpha);
	Pen *pen = gdiplus_get_pen(brush, ctm, stroke);
	((userData *)dev->user)->started = true;
	((userData *)dev->user)->graphics->DrawPath(pen, &gpath);
	
	delete pen;
	delete brush;
}

extern "C" static void
fz_gdiplus_clip_text(fz_device *dev, fz_text *text, fz_matrix ctm, int accumulate)
{
	GraphicsPath gpath(FillModeWinding);
	float black[3] = { 0 };
	if (text->font->ft_face)
		gdiplus_render_text(dev, text, ctm, NULL, &gpath);
	else
		gdiplus_run_t3_text(dev, text, ctm, fz_device_rgb, black, 1.0, &gpath);
	gdiplus_apply_transform(&gpath, ctm);
	
	((userData *)dev->user)->pushClip(&gpath, 1.0, accumulate == 2);
}

extern "C" static void
fz_gdiplus_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm)
{
	GraphicsPath gpath(FillModeWinding);
	float black[3] = { 0 };
	if (text->font->ft_face)
		gdiplus_render_text(dev, text, ctm, NULL, &gpath);
	else
		gdiplus_run_t3_text(dev, text, ctm, fz_device_rgb, black, 1.0, &gpath);
	gdiplus_apply_transform(&gpath, ctm);
	Pen *pen = gdiplus_get_pen(&SolidBrush(Color()), ctm, stroke);
	
	gpath.Widen(pen);
	((userData *)dev->user)->pushClip(&gpath);
	
	delete pen;
}

extern "C" static void
fz_gdiplus_ignore_text(fz_device *dev, fz_text *text, fz_matrix ctm)
{
}

extern "C" static void
fz_gdiplus_fill_shade(fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha)
{
	RectF clipRect;
	((userData *)dev->user)->started = true;
	((userData *)dev->user)->graphics->GetClipBounds(&clipRect);
	fz_rect clip = { clipRect.X, clipRect.Y, clipRect.X + clipRect.Width, clipRect.Y + clipRect.Height };
	
	fz_rect bounds = fz_bound_shade(dev->ctx, shade, ctm);
	clip = fz_intersect_rect(bounds, clip);
	
	if (!fz_is_empty_rect(shade->bbox))
	{
		bounds = fz_transform_rect(fz_concat(shade->matrix, ctm), shade->bbox);
		clip = fz_intersect_rect(bounds, clip);
	}
	fz_bbox bbox = fz_bbox_from_rect(clip);
	
	if (fz_is_empty_rect(bbox))
		return;
	
	fz_pixmap *dest = fz_new_pixmap_with_bbox(dev->ctx, fz_device_rgb, bbox);
	fz_clear_pixmap(dev->ctx, dest);
	
	if (shade->use_background)
	{
		float colorfv[4];
		fz_convert_color(dev->ctx, fz_device_rgb, colorfv, shade->colorspace, shade->background);
		colorfv[3] = 1.0;
		
		for (int y = bbox.y0; y < bbox.y1; y++)
		{
			unsigned char *s = dest->samples + ((bbox.x0 - dest->x) + (y - dest->y) * dest->w) * 4;
			for (int x = bbox.x0; x < bbox.x1; x++)
				for (int i = 0; i < 4; i++)
					*s++ = colorfv[i] * 255;
		}
	}
	
	fz_paint_shade(dev->ctx, shade, ctm, dest, bbox);
	fz_unmultiply_pixmap(dev->ctx, dest);
	
	ctm = fz_concat(fz_scale(dest->w, dest->h), fz_translate(dest->x, dest->y));
	((userData *)dev->user)->drawPixmap(dest, ctm, alpha);
	
	fz_drop_pixmap(dev->ctx, dest);
}

static fz_pixmap *
fz_image_to_pixmap_def(fz_context *ctx, fz_image *image, fz_matrix ctm)
{
	float dx = hypotf(ctm.a, ctm.b);
	float dy = hypotf(ctm.c, ctm.d);
	// TODO: use dx = image->w and dy = image->h instead?
	return fz_image_to_pixmap(ctx, image, dx, dy);
}

extern "C" static void
fz_gdiplus_fill_image(fz_device *dev, fz_image *image, fz_matrix ctm, float alpha)
{
	fz_pixmap *pixmap = fz_image_to_pixmap_def(dev->ctx, image, ctm);
	((userData *)dev->user)->started = true;
	((userData *)dev->user)->drawPixmap(pixmap, ctm, alpha);
	fz_drop_pixmap(dev->ctx, pixmap);
}

extern "C" static void
fz_gdiplus_fill_image_mask(fz_device *dev, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	float rgb[3];
	if (!((userData *)dev->user)->t3color)
		fz_convert_color(dev->ctx, fz_device_rgb, rgb, colorspace, color);
	else
		memcpy(rgb, ((userData *)dev->user)->t3color, sizeof(rgb));
	
	fz_pixmap *pixmap = fz_image_to_pixmap_def(dev->ctx, image, ctm);
	fz_pixmap *img2 = fz_new_pixmap_with_bbox(dev->ctx, fz_device_rgb, fz_pixmap_bbox(dev->ctx, pixmap));
	for (int i = 0; i < img2->w * img2->h; i++)
	{
		img2->samples[i * 4] = rgb[0] * 255;
		img2->samples[i * 4 + 1] = rgb[1] * 255;
		img2->samples[i * 4 + 2] = rgb[2] * 255;
		img2->samples[i * 4 + 3] = pixmap->samples[i];
	}
	img2->interpolate = pixmap->interpolate;
	
	((userData *)dev->user)->started = true;
	((userData *)dev->user)->drawPixmap(img2, ctm, alpha);
	
	fz_drop_pixmap(dev->ctx, img2);
	fz_drop_pixmap(dev->ctx, pixmap);
}

extern "C" static void
fz_gdiplus_clip_image_mask(fz_device *dev, fz_image *image, fz_rect rect, fz_matrix ctm)
{
	fz_pixmap *pixmap = fz_image_to_pixmap_def(dev->ctx, image, ctm);
	((userData *)dev->user)->pushClipMask(pixmap, ctm);
	fz_drop_pixmap(dev->ctx, pixmap);
}

extern "C" static void
fz_gdiplus_pop_clip(fz_device *dev)
{
	((userData *)dev->user)->popClip();
}

extern "C" static void
fz_gdiplus_begin_mask(fz_device *dev, fz_rect rect, int luminosity,
	fz_colorspace *colorspace, float *colorfv)
{
	float rgb[3] = { 0 };
	if (luminosity && colorspace && colorfv)
		fz_convert_color(dev->ctx, fz_device_rgb, rgb, colorspace, colorfv);
	
	((userData *)dev->user)->recordClipMask(rect, !!luminosity, rgb);
}

extern "C" static void
fz_gdiplus_end_mask(fz_device *dev)
{
	((userData *)dev->user)->applyClipMask();
}

extern "C" static void
fz_gdiplus_begin_group(fz_device *dev, fz_rect rect, int isolated, int knockout,
	int blendmode, float alpha)
{
	((userData *)dev->user)->pushClipBlend(rect, blendmode, alpha, !!isolated, !!knockout);
}

extern "C" static void
fz_gdiplus_end_group(fz_device *dev)
{
	((userData *)dev->user)->popClip();
}

extern "C" static void
fz_gdiplus_begin_tile(fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm)
{
	((userData *)dev->user)->recordTile(view, area, ctm, xstep, ystep);
}

extern "C" static void
fz_gdiplus_end_tile(fz_device *dev)
{
	((userData *)dev->user)->applyTiling();
}

extern "C" static void
fz_gdiplus_apply_transfer_function(fz_device *dev, fz_transfer_function *tr, int for_mask)
{
	((userData *)dev->user)->applyTransferFunction(tr, for_mask);
}

extern "C" static void
fz_gdiplus_free_user(fz_device *dev)
{
	delete (userData *)dev->user;
	
	fz_synchronize_begin();
	if (--g_gdiplusUsage == 0)
		GdiplusShutdown(g_gdiplusToken);
	fz_synchronize_end();
}

fz_device *
fz_new_gdiplus_device(fz_context *ctx, void *dc, fz_rect base_clip)
{
	fz_synchronize_begin();
	if (++g_gdiplusUsage == 1)
		GdiplusStartup(&g_gdiplusToken, &GdiplusStartupInput(), NULL);
	fz_synchronize_end();
	
	fz_device *dev = fz_new_device(ctx, new userData(ctx, (HDC)dc, base_clip));
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
	
	dev->apply_transfer_function = fz_gdiplus_apply_transfer_function;
	
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

extern "C" void
fz_synchronize_begin()
{
	globalLock.Acquire();
}

extern "C" void
fz_synchronize_end()
{
	globalLock.Release();
}
