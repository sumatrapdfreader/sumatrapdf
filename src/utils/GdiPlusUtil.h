/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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

ImgFormat GfxFormatFromData(std::span<u8>);

Gdiplus::RectF RectToRectF(const Gdiplus::Rect r);

typedef RectF (*TextMeasureAlgorithm)(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);

RectF MeasureTextAccurate(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
RectF MeasureTextAccurate2(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
RectF MeasureTextStandard(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
RectF MeasureTextQuick(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, int len);
RectF MeasureText(Gdiplus::Graphics* g, Gdiplus::Font* f, const WCHAR* s, size_t len = -1,
                  TextMeasureAlgorithm algo = nullptr);
// float     GetSpaceDx(Graphics *g, Font *f, TextMeasureAlgorithm algo=nullptr);
// size_t   StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx, TextMeasureAlgorithm
// algo=nullptr);

void GetBaseTransform(Gdiplus::Matrix& m, Gdiplus::RectF pageRect, float zoom, int rotation);

const WCHAR* GfxFileExtFromData(std::span<u8>);
bool IsGdiPlusNativeFormat(std::span<u8>);
Gdiplus::Bitmap* BitmapFromData(std::span<u8>);
Size BitmapSizeFromData(std::span<u8>);
CLSID GetEncoderClsid(const WCHAR* format);

// TODO: for the lack of a better place
struct ImageData {
    char* data{nullptr};
    size_t len{0};

    size_t size() const;
    std::span<u8> AsSpan() const;
};

struct ImageData2 {
    ImageData base;
    // path by which content refers to this image
    char* fileName = nullptr;
    // document specific id by whcih to find this image
    size_t fileId = 0;
};
