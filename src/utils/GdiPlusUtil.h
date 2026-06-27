/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct RenderedBitmap;
struct Pixmap;

Gdiplus::RectF RectToRectF(Gdiplus::Rect r);

// Zero-copy: wrap a Pixmap's pixels in a Gdiplus::Bitmap that borrows the buffer and
// takes ownership of the Pixmap (frees it when the returned bitmap is deleted). The
// Pixmap must outlive the bitmap, which the returned object guarantees. Returns nullptr
// (and frees px) on failure. Only BGRA8/BGR8 Pixmaps are supported.
Gdiplus::Bitmap* NewGdiplusBitmapFromPixmap(Pixmap* px);
// Copy a Gdiplus::Bitmap's pixels out into a freshly allocated BGRA8 Pixmap (used to
// turn an awkwardly-formatted GDI+ decode - 16bpp TGA, CMYK JPEG - into a uniform
// Pixmap). Does not take ownership of bmp. Returns nullptr on failure.
Pixmap* PixmapFromGdiplus(Gdiplus::Bitmap* bmp);
// Apply an EXIF orientation (2..8) to a Pixmap, returning a possibly-rotated Pixmap and
// freeing the input. orientation 0/1 (or out of range) returns px unchanged. Rotation
// is done via GDI+, so this lives here rather than in the portable Pixmap.h.
Pixmap* PixmapApplyExifOrientation(Pixmap* px, int orientation);

typedef RectF (*TextMeasureAlgorithm)(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);

RectF MeasureTextAccurate(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
RectF MeasureTextStandard(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
RectF MeasureTextQuick(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
RectF MeasureText(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, size_t len = -1,
                  TextMeasureAlgorithm algo = nullptr);
// float     GetSpaceDx(Graphics *g, Font *f, TextMeasureAlgorithm algo=nullptr);
// size_t   StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx, TextMeasureAlgorithm
// algo=nullptr);

void GetBaseTransform(Gdiplus::Matrix& m, Gdiplus::RectF pageRect, float zoom, int rotation);

Gdiplus::Bitmap* BitmapFromDataWin(const ByteSlice& bmpData);
Size ImageSizeFromData(const ByteSlice&);
Size ImageSizeFromHeader(const ByteSlice&);
bool ExifOrientationSwapsDimensions(int orientation);
void ApplyExifOrientation(Gdiplus::Bitmap* bmp, int exifOrientation);
int WebpExifOrientation(const ByteSlice& d);
CLSID GetGdiPlusEncoderClsid(const WCHAR* format);
RenderedBitmap* LoadRenderedBitmapWin(const char* path);
