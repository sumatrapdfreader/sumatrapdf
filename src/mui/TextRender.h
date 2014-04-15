/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef Mui_h
#error "this is only meant to be included by Mui.h inside mui namespace"
#endif
#ifdef TextRender_h
#error "dont include twice!"
#endif
#define TextRender_h

enum TextRenderMethod {
    TextRenderGdiplus, // uses MeasureTextAccurate, which is slower than MeasureTextQuick
    TextRenderGdiplusQuick, // uses MeasureTextQuick
    TextRenderGdi,
    //TODO: implement TextRenderDirectDraw
    //TextRenderDirectDraw
};

class ITextMeasure {
public:
    virtual void            SetFont(CachedFont *font) = 0;
    virtual CachedFont *    CreateCachedFont(const WCHAR *name, float size, FontStyle style) = 0;
    virtual float           GetCurrFontLineSpacing() = 0;

    virtual Gdiplus::RectF  Measure(const char *s, size_t sLen) = 0;
    virtual Gdiplus::RectF  Measure(const WCHAR *s, size_t sLen) = 0;

    virtual ~ITextMeasure() { }
};

class ITextDraw {
public:
    virtual void SetFont(CachedFont *font) = 0;
    virtual void SetTextColor(Gdiplus::Color col) = 0;
    virtual void Draw(const char *s, size_t sLen, RectF& bb, bool isRtl=false) = 0;
    virtual void Draw(const WCHAR *s, size_t sLen, RectF& bb, bool isRtl=false) = 0;

    // GDI+ calls cannot be done if we called Graphics::GetHDC(). However, getting/releasing
    // hdc is very expensive and kills performance if we do it for every Draw(). So we add
    // explicit Lock()/Unlock() calls (only important for TextDrawGdi) so that a caller
    // can batch Draw() calls to minimize GetHDC()/ReleaseHDC() calls
    virtual void Lock() {}
    virtual void Unlock() {}

    virtual ~ITextDraw() { }
};

class TextMeasureGdi : public ITextMeasure {
private:
    HDC         hdc;
    bool        ownsHdc;
    HFONT       origFont;
    HFONT       currFont;
    WCHAR       txtConvBuf[512];

    TextMeasureGdi() : hdc(NULL), ownsHdc(false), origFont(NULL), currFont(NULL) { }

public:

    static TextMeasureGdi*  Create(HDC hdc);

    virtual void            SetFont(CachedFont *font);
    virtual CachedFont *    CreateCachedFont(const WCHAR *name, float size, FontStyle style);
    virtual float           GetCurrFontLineSpacing();
    virtual Gdiplus::RectF  Measure(const char *s, size_t sLen);
    virtual Gdiplus::RectF  Measure(const WCHAR *s, size_t sLen);
    virtual ~TextMeasureGdi();
};

class TextDrawGdi : public ITextDraw {
private:
    Gdiplus::Graphics *     gfx;
    Gdiplus::Color          textColor;
    HDC                     hdc;
    HFONT                   currFont;
    WCHAR                   txtConvBuf[512];

    TextDrawGdi() : gfx(NULL), hdc(NULL), currFont(NULL) { }

public:
    //static TextDrawGdi *Create(HDC hdc);
    static TextDrawGdi *Create(Gdiplus::Graphics *gfx);

    virtual ~TextDrawGdi();
    virtual void SetFont(CachedFont *font);
    virtual void SetTextColor(Gdiplus::Color col);
    virtual void Draw(const char *s, size_t sLen, RectF& bb, bool isRtl=false);
    virtual void Draw(const WCHAR *s, size_t sLen, RectF& bb, bool isRtl=false);
    virtual void Lock();
    virtual void Unlock();
};

class TextMeasureGdiplus : public ITextMeasure {
private:
    Gdiplus::RectF        (*measureAlgo)(Gdiplus::Graphics *g, Gdiplus::Font *f, const WCHAR *s, int len);

    // We don't own gfx and fnt
    Gdiplus::Graphics *  gfx;
    Gdiplus::Font *      fnt;
    WCHAR       txtConvBuf[512];
    TextMeasureGdiplus() : gfx(NULL), fnt(NULL) {}

public:
    static TextMeasureGdiplus*  Create(Gdiplus::Graphics *gfx, Gdiplus::RectF (*measureAlgo)(Gdiplus::Graphics *g, Gdiplus::Font *f, const WCHAR *s, int len)=NULL);

    virtual void                SetFont(CachedFont *font);
    virtual CachedFont *        CreateCachedFont(const WCHAR *name, float size, FontStyle style);
    virtual float               GetCurrFontLineSpacing();
    virtual Gdiplus::RectF      Measure(const char *s, size_t sLen);
    virtual Gdiplus::RectF      Measure(const WCHAR *s, size_t sLen);
    virtual ~TextMeasureGdiplus();
};

class TextDrawGdiplus : public ITextDraw {
private:
    // we don't own gfx or fnt
    Gdiplus::Graphics *  gfx;
    Gdiplus::Font *      fnt;
    Gdiplus::Color       textColor;
    Gdiplus::Brush *     textColorBrush;
    WCHAR                txtConvBuf[512];

    TextDrawGdiplus() : gfx(NULL), fnt(NULL), textColorBrush(NULL), textColor(0,0,0,0) { }

public:

    static TextDrawGdiplus *Create(Gdiplus::Graphics *gfx);

    virtual void SetFont(CachedFont *font);
    virtual void SetTextColor(Gdiplus::Color col);
    virtual void Draw(const char *s, size_t sLen, RectF& bb, bool isRtl=false);
    virtual void Draw(const WCHAR *s, size_t sLen, RectF& bb, bool isRtl=false);
    virtual ~TextDrawGdiplus();
};

size_t StringLenForWidth(ITextMeasure *textMeasure, const WCHAR *s, size_t len, float dx);
REAL GetSpaceDx(ITextMeasure *textMeasure);
