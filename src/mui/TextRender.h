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
    virtual void SetFont(CachedFont *font) = 0;
    virtual Gdiplus::RectF Measure(const char *s, size_t sLen) = 0;
    virtual Gdiplus::RectF Measure(const WCHAR *s, size_t sLen) = 0;
};

class ITextDraw {
public:
    virtual void Draw(const char *s, size_t sLen, RectF& bb) = 0;
};

class TextMeasureGdi : public ITextMeasure {
private:
    HDC         hdc;
    WCHAR       txtConvBuf[512];
    HFONT       origFont;

    TextMeasureGdi() { }

public:

    static TextMeasureGdi* Create(HDC hdc);

    virtual void SetFont(CachedFont *font);
    virtual Gdiplus::RectF Measure(const char *s, size_t sLen);
    virtual Gdiplus::RectF Measure(const WCHAR *s, size_t sLen);
    virtual ~TextMeasureGdi();
};

class TextDrawGdi : public ITextDraw {
private:
    HDC hdc;
    WCHAR       txtConvBuf[512];

    TextDrawGdi() { }

public:
    static TextDrawGdi *Create(HDC hdc);

    virtual void Draw(const char *s, size_t sLen, RectF& bb);
    virtual ~TextDrawGdi() {}
};

class TextMeasureGdiplus : public ITextMeasure {
private:
    enum {
        bmpDx = 32,
        bmpDy = 4,
        stride = bmpDx * 4,
    };

    Gdiplus::RectF        (*measureAlgo)(Gdiplus::Graphics *g, Gdiplus::Font *f, const WCHAR *s, int len);
    Gdiplus::Graphics *  gfx;
    Gdiplus::Font *      fnt;
    Gdiplus::Bitmap *    bmp;
    BYTE        data[bmpDx * bmpDy * 4];
    WCHAR       txtConvBuf[512];
    TextMeasureGdiplus() : gfx(NULL), bmp(NULL), fnt(NULL) {}

public:
    float GetFontHeight() const {
        return fnt->GetHeight(gfx);
    }

    static TextMeasureGdiplus* Create(Gdiplus::Graphics *gfx, Gdiplus::RectF (*measureAlgo)(Gdiplus::Graphics *g, Gdiplus::Font *f, const WCHAR *s, int len)=NULL);

    virtual void SetFont(CachedFont *font);
    virtual Gdiplus::RectF Measure(const char *s, size_t sLen);
    virtual Gdiplus::RectF Measure(const WCHAR *s, size_t sLen);
    virtual ~TextMeasureGdiplus();
};

class TextDrawGdiplus : public ITextDraw {
private:
    Gdiplus::Font *      fnt;
    Gdiplus::Brush *     col;
    WCHAR       txtConvBuf[512];

    TextDrawGdiplus() : gfx(NULL) { }

public:
    Gdiplus::Graphics *  gfx;

    static TextDrawGdiplus *Create(Gdiplus::Graphics *gfx);
    virtual void Draw(const char *s, size_t sLen, RectF& bb);
    virtual ~TextDrawGdiplus();
};

size_t StringLenForWidth(ITextMeasure *textMeasure, const WCHAR *s, size_t len, float dx);

