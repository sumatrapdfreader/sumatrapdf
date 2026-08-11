/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// PlatformFont (wingui/PlatformFont.h) must be included before this header

enum class TextRenderMethod {
    Gdiplus,      // uses MeasureTextAccurate, which is slower than MeasureTextQuick
    GdiplusQuick, // uses MeasureTextQuick
    Gdi,
    Hdc,
    // TODO: implement TextRenderDirectDraw
    // TextRenderDirectDraw
};

class ITextRender {
  public:
    virtual void SetFont(PlatformFont* font) = 0;
    virtual void SetTextColor(COLORREF col) = 0;

    // this is only for the benefit of TextRenderGdi. In GDI+, Draw() uses
    // transparent background color (i.e. whatever is under).
    // GDI doesn't support such transparency so the best we can do is simulate
    // that if the background is solid color. It won't work in other cases
    virtual void SetTextBgColor(COLORREF col) = 0;

    virtual float GetCurrFontLineSpacing() = 0;

    // s is utf-8; converting to whatever the platform draws with is up to the
    // implementation
    virtual RectF Measure(Str s) = 0;

    // GDI+ calls cannot be done if we called Graphics::GetHDC(). However, getting/releasing
    // hdc is very expensive and kills performance if we do it for every Draw(). So we add
    // explicit Lock()/Unlock() calls (only important for TextDrawGdi) so that a caller
    // can batch Draw() calls to minimize GetHDC()/ReleaseHDC() calls
    virtual void Lock() = 0;
    virtual void Unlock() = 0;

    virtual void Draw(Str s, RectF bb, bool isRtl) = 0;

    virtual ~ITextRender() = default;
    ;

    TextRenderMethod method = TextRenderMethod::Hdc;
};

class TextRenderGdi : public ITextRender {
  private:
    HDC hdcGfxLocked = nullptr;
    HDC hdcForTextMeasure = nullptr;
    HGDIOBJ hdcForTextMeasurePrevFont = nullptr;
    PlatformFont* currFont = nullptr;
    Gdiplus::Graphics* gfx = nullptr;
    // black, like the Gdiplus::Color these used to default to
    COLORREF textColor = 0;
    COLORREF textBgColor = 0;

    HDC memHdc = nullptr;
    HGDIOBJ memHdcPrevFont = nullptr;
    HBITMAP memBmp = nullptr;
    HGDIOBJ memHdcPrevBitmap = nullptr;
    void* memBmpData = nullptr;
    int memBmpDx = 0;
    int memBmpDy = 0;

    TextRenderGdi() = default;

    void FreeMemBmp();
    void CreateClearBmpOfSize(int dx, int dy);
    void RestoreMemHdcPrevFont();
    void RestoreHdcForTextMeasurePrevFont();
    void RestoreMemHdcPrevBitmap();

  public:
    void CreateHdcForTextMeasure();
    static TextRenderGdi* Create(Gdiplus::Graphics* gfx);

    void SetFont(PlatformFont* font) override;
    void SetTextColor(COLORREF col) override;
    void SetTextBgColor(COLORREF col) override;

    float GetCurrFontLineSpacing() override;

    RectF Measure(Str s) override;

    void Lock() override;
    void Unlock() override;

    void Draw(Str s, RectF bb, bool isRtl) override;

    void DrawTransparent(Str s, RectF bb, bool isRtl);

    ~TextRenderGdi() override;
};

class TextRenderGdiplus : public ITextRender {
  private:
    TextMeasureAlgorithm measureAlgo = nullptr;

    // We don't own gfx and currFont
    Gdiplus::Graphics* gfx = nullptr;
    PlatformFont* currFont = nullptr;
    COLORREF textColor = 0;
    Gdiplus::Brush* textColorBrush = nullptr;

    TextRenderGdiplus() = default;

  public:
    static TextRenderGdiplus* Create(Gdiplus::Graphics* gfx, TextMeasureAlgorithm measureAlgo = nullptr);

    void SetFont(PlatformFont* font) override;
    void SetTextColor(COLORREF col) override;
    void SetTextBgColor(COLORREF) override {}

    float GetCurrFontLineSpacing() override;

    RectF Measure(Str s) override;

    void Lock() override {}
    void Unlock() override {}

    void Draw(Str s, RectF bb, bool isRtl) override;

    ~TextRenderGdiplus() override;
};

// Note: this is not meant to be used, just exists so that I can see
// perf compared to other TextRender* implementations
class TextRenderHdc : public ITextRender {
    BITMAPINFO bmi{};

    HDC hdc = nullptr;
    HBITMAP bmp = nullptr;
    void* bmpData = nullptr;

    // We don't own gfx and currFont
    Gdiplus::Graphics* gfx = nullptr;
    PlatformFont* currFont = nullptr;
    // black, like the Gdiplus::Color these used to default to
    COLORREF textColor = 0;
    COLORREF textBgColor = 0;

    TextRenderHdc() = default;

  public:
    static TextRenderHdc* Create(Gdiplus::Graphics* gfx, int dx, int dy);

    void SetFont(PlatformFont* font) override;
    void SetTextColor(COLORREF col) override;
    void SetTextBgColor(COLORREF col) override;

    float GetCurrFontLineSpacing() override;

    RectF Measure(Str s) override;

    void Lock() override;
    void Unlock() override;

    void Draw(Str s, RectF bb, bool isRtl) override;

    ~TextRenderHdc() override;
};

ITextRender* CreateTextRender(TextRenderMethod method, Gdiplus::Graphics* gfx, int dx, int dy);

// the length is in utf-8 bytes and never cuts a sequence in half
int StringLenForWidth(ITextRender* textMeasure, Str s, float dx, float sWidth = -1);
float GetSpaceDx(ITextRender* textMeasure);
