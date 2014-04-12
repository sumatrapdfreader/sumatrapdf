/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef TextRender_h
#define TextRender_h

HFONT GetGdiFont();

class ITextMeasure {
public:
    virtual RectF Measure(const char *s, size_t sLen) = 0;
};

class ITextDraw {
public:
    virtual void Draw(const char *s, size_t sLen, RectF& bb) = 0;
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

class TextMeasureGdi : public ITextMeasure {
private:
    HDC         hdc;
    WCHAR       txtConvBuf[512];

    TextMeasureGdi() { }

public:

    static TextMeasureGdi* Create(HDC hdc);

    virtual RectF Measure(const char *s, size_t sLen);
    virtual ~TextMeasureGdi() {}
};

class TextDrawGdiplus : public ITextDraw {
private:
    Font *      fnt;
    Brush *     col;
    WCHAR       txtConvBuf[512];

    TextDrawGdiplus() : gfx(NULL) { }

public:
    Graphics *  gfx;

    static TextDrawGdiplus *Create(HDC dc);
    virtual void Draw(const char *s, size_t sLen, RectF& bb);
    virtual ~TextDrawGdiplus();
};

class TextMeasureGdiplus : public ITextMeasure {
private:
    enum {
        bmpDx = 32,
        bmpDy = 4,
        stride = bmpDx * 4,
    };

    Graphics *  gfx;
    Font *      fnt;
    Bitmap *    bmp;
    BYTE        data[bmpDx * bmpDy * 4];
    WCHAR       txtConvBuf[512];
    TextMeasureGdiplus() : gfx(NULL), bmp(NULL), fnt(NULL) {}

public:
    float GetFontHeight() const {
        return fnt->GetHeight(gfx);
    }

    static TextMeasureGdiplus* Create(HDC hdc);

    virtual RectF Measure(const char *s, size_t sLen);
    virtual ~TextMeasureGdiplus();
};


#endif
