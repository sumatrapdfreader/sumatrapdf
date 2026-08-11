/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct RenderedBitmap;
struct Pixmap;

Gdiplus::RectF RectToRectF(Gdiplus::Rect r);

// set a consistent mode on a Graphics so that measuring and drawing text give
// the same results everywhere
void InitGraphicsMode(Gdiplus::Graphics* g);

Gdiplus::Bitmap* NewGdiplusBitmapFromPixmap(Pixmap* px);
Gdiplus::Bitmap* WrapPixmapGdiplus(const Pixmap* px);
Pixmap* PixmapFromGdiplus(Gdiplus::Bitmap* bmp);
Pixmap* PixmapApplyExifOrientation(Pixmap* px, int orientation);

typedef RectF (*TextMeasureAlgorithm)(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s);

RectF MeasureTextAccurate(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s);
RectF MeasureTextStandard(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s);
RectF MeasureTextQuick(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s);
RectF MeasureText(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s, TextMeasureAlgorithm algo = nullptr);

void GetBaseTransform(Gdiplus::Matrix& m, Gdiplus::RectF pageRect, float zoom, int rotation);

void ApplyExifOrientation(Gdiplus::Bitmap* bmp, int exifOrientation);
CLSID GetGdiPlusEncoderClsid(WStr format);
