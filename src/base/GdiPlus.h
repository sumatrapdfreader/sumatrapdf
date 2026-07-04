/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct RenderedBitmap;
struct Pixmap;

Gdiplus::RectF RectToRectF(Gdiplus::Rect r);

Gdiplus::Bitmap* NewGdiplusBitmapFromPixmap(Pixmap* px);
Gdiplus::Bitmap* WrapPixmapGdiplus(const Pixmap* px);
Pixmap* PixmapFromGdiplus(Gdiplus::Bitmap* bmp);
Pixmap* PixmapApplyExifOrientation(Pixmap* px, int orientation);

typedef RectF (*TextMeasureAlgorithm)(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s);

RectF MeasureTextAccurate(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s);
RectF MeasureTextStandard(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s);
RectF MeasureTextQuick(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s);
RectF MeasureText(Gdiplus::Graphics* g, Gdiplus::Font* f, WStr s, TextMeasureAlgorithm algo = nullptr);
// float     GetSpaceDx(Graphics *g, Font *f, TextMeasureAlgorithm algo=nullptr);
// int   StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx, TextMeasureAlgorithm
// algo=nullptr);

void GetBaseTransform(Gdiplus::Matrix& m, Gdiplus::RectF pageRect, float zoom, int rotation);

Pixmap* PixmapFromDataWin(Str bmpData);
Vec<Pixmap*> PixmapsFromDataWin(Str bmpData);
Size ImageSizeFromData(Str);
Size ImageSizeFromHeader(Str);
bool ExifOrientationSwapsDimensions(int orientation);
void ApplyExifOrientation(Gdiplus::Bitmap* bmp, int exifOrientation);
int WebpExifOrientation(Str d);
CLSID GetGdiPlusEncoderClsid(WStr format);
RenderedBitmap* LoadRenderedBitmapWin(Str path);
