/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "WinUtil.h"

/* Note: this code should be in utils but it depends on mui code, so it's
in mui, to avoid circular dependency */

#define FONT_NAME       L"Tahoma"
#define FONT_SIZE       10

/*
float DpiScaled(float n) {
    static float scale = 0.0f;
    if (scale == 0.0f) {
        win::GetHwndDpi(HWND_DESKTOP, &scale);
    }
    return n * scale;
}

int PixelToPoint(int n) {
    return n * 96 / 72;
}

HFONT gFont = NULL;

HFONT GetGdiFont() {
    if (!gFont) {
        HDC hdc = GetDC(NULL);
        gFont = CreateSimpleFont(hdc, FONT_NAME, (int) DpiScaled((float) PixelToPoint(FONT_SIZE)));
        ReleaseDC(NULL, hdc);
    }
    return gFont;
}

void DeleteGdiFont() {
    DeleteObject(gFont);
}
*/

namespace mui {

TextMeasureGdi *TextMeasureGdi::Create(HDC hdc) {
    TextMeasureGdi *res = new TextMeasureGdi();
    res->hdc = hdc;
    res->origFont = NULL;
    return res;
}

TextMeasureGdi::~TextMeasureGdi() {
    if (origFont != NULL) {
        SelectObject(hdc, origFont);
    }
}

void TextMeasureGdi::SetFont(mui::CachedFont *font) {
    CrashIf(!font->hdcFont);
    if (origFont == NULL) {
        origFont = SelectFont(hdc, font->hdcFont);
    } else {
        SelectFont(hdc, font->hdcFont);
    }
}

RectF TextMeasureGdi::Measure(const WCHAR *s, size_t sLen) {
    SIZE txtSize;
    GetTextExtentPoint32W(hdc, s, (int) sLen, &txtSize);
    RectF res(0.0f, 0.0f, (float) txtSize.cx, (float) txtSize.cy);
    return res;
}

RectF TextMeasureGdi::Measure(const char *s, size_t sLen) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    SIZE txtSize;
    GetTextExtentPoint32W(hdc, txtConvBuf, (int) strLen, &txtSize);
    RectF res(0.0f, 0.0f, (float) txtSize.cx, (float) txtSize.cy);
    return res;
}

TextDrawGdi *TextDrawGdi::Create(HDC hdc) {
    TextDrawGdi *res = new TextDrawGdi();
    res->hdc = hdc;
    return res;
}

void TextDrawGdi::Draw(const char *s, size_t sLen, RectF& bb) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    PointF loc;
    bb.GetLocation(&loc);
    int x = (int) bb.X;
    int y = (int) bb.Y;
    ExtTextOutW(hdc, x, y, 0, NULL, txtConvBuf, (int)strLen, NULL);
}

TextMeasureGdiplus *TextMeasureGdiplus::Create(Graphics *gfx, RectF (*measureAlgo)(Graphics *g, Font *f, const WCHAR *s, int len)) {
    TextMeasureGdiplus *res = new TextMeasureGdiplus();
    //res->bmp = ::new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, res->data);
    //if (!res->bmp)
    //    return NULL;
    //res->gfx = ::new Graphics((Image*) res->bmp);
    res->gfx = gfx;
    mui::InitGraphicsMode(res->gfx);
    //res->fnt = ::new Font(hdc, GetGdiFont());
    //res->fnt = ::new Font(FONT_NAME, FONT_SIZE);
    res->fnt = NULL;
    if (NULL == measureAlgo)
        res->measureAlgo = MeasureTextAccurate;
    else
        res->measureAlgo = measureAlgo;
    return res;
}

TextMeasureGdiplus::~TextMeasureGdiplus() {
    ::delete bmp;
    //::delete gfx;
    //::delete fnt;
};

void TextMeasureGdiplus::SetFont(mui::CachedFont *font) {
    CrashIf(!font->font);
    this->fnt = font->font;
}

RectF TextMeasureGdiplus::Measure(const WCHAR *s, size_t sLen) {
    CrashIf(!fnt);
    return MeasureText(gfx, fnt, s, sLen, measureAlgo);
}

RectF TextMeasureGdiplus::Measure(const char *s, size_t sLen) {
    CrashIf(!fnt);
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    return MeasureText(gfx, fnt, txtConvBuf, strLen, measureAlgo);
    //return MeasureTextAccurate(gfx, fnt, txtConvBuf, (int)strLen);
}

TextDrawGdiplus *TextDrawGdiplus::Create(Graphics *gfx) {
    TextDrawGdiplus *res = new TextDrawGdiplus();
    //res->gfx = ::new Graphics(dc);
    res->gfx = gfx;
    mui::InitGraphicsMode(res->gfx);
    //res->fnt = ::new Font(dc, GetGdiFont());

    // TODO: need to be able to set font
    res->fnt = ::new Font(FONT_NAME, FONT_SIZE);
    res->col = ::new SolidBrush(Color(0, 0, 0));
    return res;
}

TextDrawGdiplus::~TextDrawGdiplus() {
    //::delete gfx;
    ::delete fnt;
}

void TextDrawGdiplus::Draw(const char *s, size_t sLen, RectF& bb) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    PointF loc;
    bb.GetLocation(&loc);
    gfx->DrawString(txtConvBuf, (INT) strLen, fnt, loc, col);
}

// returns number of characters of string s that fits in a given width dx
// note: could be speed up a bit because in our use case we already know
// the width of the whole string so we could supply it to the function, but
// this shouldn't happen often, so that's fine. It's also possible that
// a smarter approach is possible, but this usually only does 3 MeasureText
// calls, so it's not that bad
size_t StringLenForWidth(ITextMeasure *textMeasure, const WCHAR *s, size_t len, float dx)
{
    RectF r = textMeasure->Measure(s, len);
    if (r.Width <= dx)
        return len;
    // make the best guess of the length that fits
    size_t n = (size_t)((dx / r.Width) * (float)len);
    CrashIf((0 == n) || (n > len));
    r = textMeasure->Measure(s, n);
    // find the length len of s that fits within dx iff width of len+1 exceeds dx
    int dir = 1; // increasing length
    if (r.Width > dx)
        dir = -1; // decreasing length
    for (;;) {
        n += dir;
        r = textMeasure->Measure(s, n);
        if (1 == dir) {
            // if advancing length, we know that previous string did fit, so if
            // the new one doesn't fit, the previous length was the right one
            if (r.Width > dx)
                return n - 1;
        } else {
            // if decreasing length, we know that previous string didn't fit, so if
            // the one one fits, it's of the correct length
            if (r.Width < dx)
                return n;
        }
    }
}

}
