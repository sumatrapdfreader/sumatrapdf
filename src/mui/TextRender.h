/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

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
    virtual void SetFont(CachedFont* font) = 0;
    virtual void SetTextColor(Gdiplus::Color col) = 0;

    // this is only for the benefit of TextRenderGdi. In GDI+, Draw() uses
    // transparent background color (i.e. whatever is under).
    // GDI doesn't support such transparency so the best we can do is simulate
    // that if the background is solid color. It won't work in other cases
    virtual void SetTextBgColor(Gdiplus::Color col) = 0;

    virtual float GetCurrFontLineSpacing() = 0;

    virtual RectF Measure(const char* s, size_t sLen) = 0;
    virtual RectF Measure(const WCHAR* s, size_t sLen) = 0;

    // GDI+ calls cannot be done if we called Graphics::GetHDC(). However, getting/releasing
    // hdc is very expensive and kills performance if we do it for every Draw(). So we add
    // explicit Lock()/Unlock() calls (only important for TextDrawGdi) so that a caller
    // can batch Draw() calls to minimize GetHDC()/ReleaseHDC() calls
    virtual void Lock() = 0;
    virtual void Unlock() = 0;

    virtual void Draw(const char* s, size_t sLen, RectF bb, bool isRtl) = 0;
    virtual void Draw(const WCHAR* s, size_t sLen, RectF bb, bool isRtl) = 0;

    virtual ~ITextRender() = default;
    ;

    TextRenderMethod method = TextRenderMethod::Hdc;
};

class TextRenderGdi : public ITextRender {
  private:
    HDC hdcGfxLocked = nullptr;
    HDC hdcForTextMeasure = nullptr;
    HGDIOBJ hdcForTextMeasurePrevFont = nullptr;
    CachedFont* currFont = nullptr;
    Gdiplus::Graphics* gfx = nullptr;
    Gdiplus::Color textColor;
    Gdiplus::Color textBgColor;

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
    // note: Draw() ignores any transformation set on gfx
    static TextRenderGdi* Create(Gdiplus::Graphics* gfx);

    void SetFont(CachedFont* font) override;
    void SetTextColor(Gdiplus::Color col) override;
    void SetTextBgColor(Gdiplus::Color col) override;

    float GetCurrFontLineSpacing() override;

    RectF Measure(const char* s, size_t sLen) override;
    RectF Measure(const WCHAR* s, size_t sLen) override;

    void Lock() override;
    void Unlock() override;

    void Draw(const char* s, size_t sLen, RectF bb, bool isRtl) override;
    void Draw(const WCHAR* s, size_t sLen, RectF bb, bool isRtl) override;

    void DrawTransparent(const char* s, size_t sLen, RectF bb, bool isRtl);
    void DrawTransparent(const WCHAR* s, size_t sLen, RectF bb, bool isRtl);

    ~TextRenderGdi() override;
};

class TextRenderGdiplus : public ITextRender {
  private:
    TextMeasureAlgorithm measureAlgo = nullptr;

    // We don't own gfx and currFont
    Gdiplus::Graphics* gfx = nullptr;
    CachedFont* currFont = nullptr;
    Gdiplus::Color textColor{};
    Gdiplus::Brush* textColorBrush = nullptr;

    TextRenderGdiplus() = default;

  public:
    static TextRenderGdiplus* Create(Gdiplus::Graphics* gfx, TextMeasureAlgorithm measureAlgo = nullptr);

    void SetFont(CachedFont* font) override;
    void SetTextColor(Gdiplus::Color col) override;
    void SetTextBgColor(Gdiplus::Color) override {
    }

    float GetCurrFontLineSpacing() override;

    RectF Measure(const char* s, size_t sLen) override;
    RectF Measure(const WCHAR* s, size_t sLen) override;

    void Lock() override {
    }
    void Unlock() override {
    }

    void Draw(const char* s, size_t sLen, RectF bb, bool isRtl) override;
    void Draw(const WCHAR* s, size_t sLen, RectF bb, bool isRtl) override;

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
    CachedFont* currFont = nullptr;
    Gdiplus::Color textColor{};
    Gdiplus::Color textBgColor{};

    TextRenderHdc() = default;

  public:
    static TextRenderHdc* Create(Gdiplus::Graphics* gfx, int dx, int dy);

    void SetFont(CachedFont* font) override;
    void SetTextColor(Gdiplus::Color col) override;
    void SetTextBgColor(Gdiplus::Color col) override;

    float GetCurrFontLineSpacing() override;

    RectF Measure(const char* s, size_t sLen) override;
    RectF Measure(const WCHAR* s, size_t sLen) override;

    void Lock() override;
    void Unlock() override;

    void Draw(const char* s, size_t sLen, RectF bb, bool isRtl) override;
    void Draw(const WCHAR* s, size_t sLen, RectF bb, bool isRtl) override;

    ~TextRenderHdc() override;
};

ITextRender* CreateTextRender(TextRenderMethod method, Graphics* gfx, int dx, int dy);

size_t StringLenForWidth(ITextRender* textMeasure, const WCHAR* s, size_t len, float dx);
float GetSpaceDx(ITextRender* textMeasure);
