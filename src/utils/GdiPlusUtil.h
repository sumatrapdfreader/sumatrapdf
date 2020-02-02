/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class ImgFormat {
    Unknown,
    BMP,
    GIF,
    JPEG,
    JXR,
    PNG,
    TGA,
    TIFF,
    WebP,
    JP2,
};

ImgFormat GfxFormatFromData(const char* data, size_t len);

typedef Gdiplus::RectF (*TextMeasureAlgorithm)(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);

Gdiplus::RectF MeasureTextAccurate(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
Gdiplus::RectF MeasureTextAccurate2(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
Gdiplus::RectF MeasureTextStandard(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
Gdiplus::RectF MeasureTextQuick(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
Gdiplus::RectF MeasureText(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, size_t len = -1,
                           TextMeasureAlgorithm algo = nullptr);
// REAL     GetSpaceDx(Graphics *g, Font *f, TextMeasureAlgorithm algo=nullptr);
// size_t   StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx, TextMeasureAlgorithm
// algo=nullptr);

void GetBaseTransform(Gdiplus::Matrix& m, Gdiplus::RectF pageRect, float zoom, int rotation);

const WCHAR* GfxFileExtFromData(const char* data, size_t len);
bool IsGdiPlusNativeFormat(const char* data, size_t len);
Gdiplus::Bitmap* BitmapFromData(const char* data, size_t len);
Gdiplus::Size BitmapSizeFromData(const char* data, size_t len);
CLSID GetEncoderClsid(const WCHAR* format);

// TODO: for the lack of a better place
struct ImageData {
    char* data = nullptr;
    size_t len = 0;

    size_t size() const;
};

struct ImageData2 {
    ImageData base;
    // path by which content refers to this image
    char* fileName = nullptr;
    // document specific id by whcih to find this image
    size_t fileId = 0;
};
