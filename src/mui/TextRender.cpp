/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

using namespace Gdiplus;
#include "GdiPlusUtil.h"
#include "WinUtil.h"

/*
TODO:
 - add transparent rendering to GDI, see:
   http://stackoverflow.com/questions/1340166/transparency-to-text-in-gdi
   http://theartofdev.wordpress.com/2013/10/24/transparent-text-rendering-with-gdi/
 - get perf data about time to format using GDI and GDI+ measurement
 - combine ITextMeasure and ITextDraw into single ITextRender object.
   They have too much in common
*/

/* Note: I would prefer this code be in utils but it depends on mui, so it must
be in mui to avoid circular dependency */

namespace mui {

TextMeasureGdi *TextMeasureGdi::Create(HDC hdc) {
    TextMeasureGdi *res = new TextMeasureGdi();
    if (hdc == NULL) {
        res->hdc = GetDC(NULL);
        res->ownsHdc = true;
    } else {
        res->hdc = hdc;
        res->ownsHdc = false;
    }
    res->origFont = NULL;
    res->currFont = NULL;
    return res;
}

TextMeasureGdi::~TextMeasureGdi() {
    if (origFont != NULL) {
        SelectObject(hdc, origFont);
    }
}

CachedFont *TextMeasureGdi::CreateCachedFont(const WCHAR *name, float size, FontStyle style) {
    return GetCachedFontGdi(name, size, style);
}

void TextMeasureGdi::SetFont(mui::CachedFont *font) {
    CrashIf(!font->hdcFont);
    // I'm not sure how expensive SelectFont() is so avoid it just in case
    if (currFont == font->hdcFont) {
        return;
    }
    if (origFont == NULL) {
        origFont = SelectFont(hdc, font->hdcFont);
    } else {
        SelectFont(hdc, font->hdcFont);
    }
    currFont = font->hdcFont;
}

float TextMeasureGdi::GetCurrFontLineSpacing() {
    CrashIf(!currFont);
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    return (float)tm.tmHeight;
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

TextDrawGdi *TextDrawGdi::Create(Graphics *gfx) {
    TextDrawGdi *res = new TextDrawGdi();
    res->gfx = gfx;
    // default to red to make mistakes stand out
    res->SetTextColor(Color(0xff, 0xff, 0x0, 0x0));
    return res;
}

TextDrawGdi::~TextDrawGdi() {
    CrashIf(hdc); // hasn't been Unlock()ed
}

void TextDrawGdi::SetFont(mui::CachedFont *font) {
    CrashIf(!font->hdcFont);
    // not sure how expensive SelectFont() is so avoid it just in case
    if (currFont == font->hdcFont) {
        return;
    }
    currFont = font->hdcFont;
    if (hdc) {
        SelectFont(hdc, currFont);
    }
}

void TextDrawGdi::SetTextColor(Gdiplus::Color col) {
    if (textColor.GetValue() == col.GetValue()) {
        return;
    }
    textColor = col;
    if (hdc) {
        ::SetTextColor(hdc, col.ToCOLORREF());
    }
}

void TextDrawGdi::Lock() {
    CrashIf(hdc);
    hdc = gfx->GetHDC();
    SelectFont(hdc, currFont);
    ::SetTextColor(hdc, textColor.ToCOLORREF());
    //SetBkMode(hdc, TRANSPARENT);
}

void TextDrawGdi::Unlock() {
    CrashIf(!hdc);
    gfx->ReleaseHDC(hdc);
    hdc = NULL;
}

void TextDrawGdi::Draw(const WCHAR *s, size_t sLen, RectF& bb, bool isLtr) {
    CrashIf(!hdc); // hasn't been Lock()ed
    int x = (int) bb.X;
    int y = (int) bb.Y;
    UINT opts = ETO_OPAQUE;
    if (!isLtr)
        opts = opts | ETO_RTLREADING;
    ExtTextOutW(hdc, x, y, opts, NULL, s, (int)sLen, NULL);
}

void TextDrawGdi::Draw(const char *s, size_t sLen, RectF& bb, bool isLtr) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    return Draw(txtConvBuf, strLen, bb, isLtr);
}

TextMeasureGdiplus *TextMeasureGdiplus::Create(Graphics *gfx, RectF (*measureAlgo)(Graphics *g, Font *f, const WCHAR *s, int len)) {
    TextMeasureGdiplus *res = new TextMeasureGdiplus();
    res->gfx = gfx;
    res->fnt = NULL;
    if (NULL == measureAlgo)
        res->measureAlgo = MeasureTextAccurate;
    else
        res->measureAlgo = measureAlgo;
    return res;
}

TextMeasureGdiplus::~TextMeasureGdiplus() {
};

CachedFont *TextMeasureGdiplus::CreateCachedFont(const WCHAR *name, float size, FontStyle style) {
    return GetCachedFontGdiplus(name, size, style);
}

void TextMeasureGdiplus::SetFont(mui::CachedFont *font) {
    CrashIf(!font->font);
    this->fnt = font->font;
}

float TextMeasureGdiplus::GetCurrFontLineSpacing() {
    return fnt->GetHeight(gfx);
}

RectF TextMeasureGdiplus::Measure(const WCHAR *s, size_t sLen) {
    CrashIf(!fnt);
    return MeasureText(gfx, fnt, s, sLen, measureAlgo);
}

RectF TextMeasureGdiplus::Measure(const char *s, size_t sLen) {
    CrashIf(!fnt);
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    return MeasureText(gfx, fnt, txtConvBuf, strLen, measureAlgo);
}

TextDrawGdiplus *TextDrawGdiplus::Create(Graphics *gfx) {
    TextDrawGdiplus *res = new TextDrawGdiplus();
    res->gfx = gfx;
    // default to red to make mistakes stand out
    res->SetTextColor(Color(0xff, 0xff, 0x0, 0x0));
    return res;
}

TextDrawGdiplus::~TextDrawGdiplus() {
    ::delete textColorBrush;
}

void TextDrawGdiplus::SetFont(mui::CachedFont *font) {
    CrashIf(!font->font);
    this->fnt = font->font;
}

void TextDrawGdiplus::SetTextColor(Gdiplus::Color col) {
    if (textColor.GetValue() == col.GetValue()) {
        return;
    }
    textColor = col;
    ::delete textColorBrush;
    textColorBrush = ::new SolidBrush(col);
}

void TextDrawGdiplus::Draw(const WCHAR *s, size_t sLen, RectF& bb, bool isLtr) {
    PointF pos;
    bb.GetLocation(&pos);
    if (isLtr) {
        gfx->DrawString(s, (INT) sLen, fnt, pos, NULL, textColorBrush);
    } else {
        StringFormat rtl;
        rtl.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
        pos.X += bb.Width;
        gfx->DrawString(s, (INT)sLen, fnt, pos, &rtl, textColorBrush);
    }
}

void TextDrawGdiplus::Draw(const char *s, size_t sLen, RectF& bb, bool isLtr) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    PointF loc;
    bb.GetLocation(&loc);
    gfx->DrawString(txtConvBuf, (INT) strLen, fnt, loc, textColorBrush);
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

// TODO: not quite sure why spaceDx1 != spaceDx2, using spaceDx2 because
// is smaller and looks as better spacing to me
REAL GetSpaceDx(ITextMeasure *textMeasure)
{
    RectF bbox;
#if 0
    bbox = textMeasure->Measure(L" ", 1, algo);
    REAL spaceDx1 = bbox.Width;
    return spaceDx1;
#else
    // this method seems to return (much) smaller size that measuring
    // the space itself
    bbox = textMeasure->Measure(L"wa", 2);
    REAL l1 = bbox.Width;
    bbox = textMeasure->Measure(L"w a", 3);
    REAL l2 = bbox.Width;
    REAL spaceDx2 = l2 - l1;
    return spaceDx2;
#endif
}

}  // namespace mui
