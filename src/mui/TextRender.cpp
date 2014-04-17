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
*/

/* Note: I would prefer this code be in utils but it depends on mui, so it must
be in mui to avoid circular dependency */

namespace mui {

TextRenderGdi *TextRenderGdi::Create(Graphics *gfx) {
    TextRenderGdi *res = new TextRenderGdi();
    res->gfx = gfx;
    // default to red to make mistakes stand out
    res->SetTextColor(Color(0xff, 0xff, 0x0, 0x0));
    res->CreateHdcForTextMeasure(); // could do lazily, but that's more things to track, so not worth it
    return res;
}

void TextRenderGdi::CreateHdcForTextMeasure() {
    HDC hdc = hdcGfxLocked;
    bool unlock = false;
    if (!hdc) {
        hdc = gfx->GetHDC();
        unlock = true;
    }
    hdcForTextMeasure = CreateCompatibleDC(hdc);
    if (unlock) {
        gfx->ReleaseHDC(hdc);
    }
}

TextRenderGdi::~TextRenderGdi() {
    ReleaseDC(NULL, hdcForTextMeasure);
    CrashIf(hdcGfxLocked); // hasn't been Unlock()ed
}

CachedFont *TextRenderGdi::CreateCachedFont(const WCHAR *name, float size, FontStyle style) {
    return GetCachedFontGdi(hdcForTextMeasure, name, size, style);
}

void TextRenderGdi::SetFont(mui::CachedFont *font) {
    CrashIf(!font->hdcFont);
    // I'm not sure how expensive SelectFont() is so avoid it just in case
    if (currFont == font->hdcFont) {
        return;
    }
    currFont = font->hdcFont;
    if (hdcGfxLocked) {
        SelectFont(hdcGfxLocked, font->hdcFont);
    }
    if (hdcForTextMeasure) {
        SelectFont(hdcForTextMeasure, font->hdcFont);
    }
}

// TODO: those are not the same as for TextRenderGdiplus (e.g. a given font is 18 here and 18.9
// in TextRenderGdiplus. One way to fix it would be to also construct Gdiplus::Font alongside
// HDC font and use that for line spacing or in TextRenderGdiplus pull out LOGFONTW and do GetTextMetric()
// on info from that
float TextRenderGdi::GetCurrFontLineSpacing() {
    CrashIf(!currFont);
    TEXTMETRIC tm;
    GetTextMetrics(hdcForTextMeasure, &tm);
    return (float)tm.tmHeight;
}

RectF TextRenderGdi::Measure(const WCHAR *s, size_t sLen) {
    SIZE txtSize;
    GetTextExtentPoint32W(hdcForTextMeasure, s, (int) sLen, &txtSize);
    RectF res(0.0f, 0.0f, (float) txtSize.cx, (float) txtSize.cy);
    return res;
}

RectF TextRenderGdi::Measure(const char *s, size_t sLen) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    return Measure(txtConvBuf, strLen);
}

void TextRenderGdi::SetTextColor(Gdiplus::Color col) {
    if (textColor.GetValue() == col.GetValue()) {
        return;
    }
    textColor = col;
    if (hdcGfxLocked) {
        ::SetTextColor(hdcGfxLocked, col.ToCOLORREF());
    }
}

void TextRenderGdi::SetTextBgColor(Gdiplus::Color col) {
    if (textBgColor.GetValue() == col.GetValue()) {
        return;
    }
    textBgColor = col;
    if (hdcGfxLocked) {
        ::SetBkColor(hdcGfxLocked, textBgColor.ToCOLORREF());
    }
}

void TextRenderGdi::Lock() {
    CrashIf(hdcGfxLocked);
    hdcGfxLocked = gfx->GetHDC();
    SelectFont(hdcGfxLocked, currFont);
    ::SetTextColor(hdcGfxLocked, textColor.ToCOLORREF());
    ::SetBkColor(hdcGfxLocked, textBgColor.ToCOLORREF());
}

void TextRenderGdi::Unlock() {
    CrashIf(!hdcGfxLocked);
    gfx->ReleaseHDC(hdcGfxLocked);
    hdcGfxLocked = NULL;
}

void TextRenderGdi::Draw(const WCHAR *s, size_t sLen, RectF& bb, bool isRtl) {
    CrashIf(!hdcGfxLocked); // hasn't been Lock()ed
    int x = (int) bb.X;
    int y = (int) bb.Y;
    UINT opts = ETO_OPAQUE;
    if (isRtl)
        opts = opts | ETO_RTLREADING;
    ExtTextOutW(hdcGfxLocked, x, y, opts, NULL, s, (int)sLen, NULL);
}

void TextRenderGdi::Draw(const char *s, size_t sLen, RectF& bb, bool isRtl) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    return Draw(txtConvBuf, strLen, bb, isRtl);
}

TextRenderGdiplus *TextRenderGdiplus::Create(Graphics *gfx, RectF (*measureAlgo)(Graphics *g, Font *f, const WCHAR *s, int len)) {
    TextRenderGdiplus *res = new TextRenderGdiplus();
    res->gfx = gfx;
    res->fnt = NULL;
    if (NULL == measureAlgo)
        res->measureAlgo = MeasureTextAccurate;
    else
        res->measureAlgo = measureAlgo;
    // default to red to make mistakes stand out
    res->SetTextColor(Color(0xff, 0xff, 0x0, 0x0));
    return res;
}

CachedFont *TextRenderGdiplus::CreateCachedFont(const WCHAR *name, float size, FontStyle style) {
    return GetCachedFontGdiplus(name, size, style);
}

void TextRenderGdiplus::SetFont(mui::CachedFont *font) {
    CrashIf(!font->font);
    this->fnt = font->font;
}

float TextRenderGdiplus::GetCurrFontLineSpacing() {
    return fnt->GetHeight(gfx);
}

RectF TextRenderGdiplus::Measure(const WCHAR *s, size_t sLen) {
    CrashIf(!fnt);
    return MeasureText(gfx, fnt, s, sLen, measureAlgo);
}

RectF TextRenderGdiplus::Measure(const char *s, size_t sLen) {
    CrashIf(!fnt);
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    return MeasureText(gfx, fnt, txtConvBuf, strLen, measureAlgo);
}

TextRenderGdiplus::~TextRenderGdiplus() {
    ::delete textColorBrush;
}

void TextRenderGdiplus::SetTextColor(Gdiplus::Color col) {
    if (textColor.GetValue() == col.GetValue()) {
        return;
    }
    textColor = col;
    ::delete textColorBrush;
    textColorBrush = ::new SolidBrush(col);
}

void TextRenderGdiplus::Draw(const WCHAR *s, size_t sLen, RectF& bb, bool isRtl) {
    PointF pos;
    bb.GetLocation(&pos);
    if (!isRtl) {
        gfx->DrawString(s, (INT)sLen, fnt, pos, NULL, textColorBrush);
    } else {
        StringFormat rtl;
        rtl.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
        pos.X += bb.Width;
        gfx->DrawString(s, (INT)sLen, fnt, pos, &rtl, textColorBrush);
    }
}

void TextRenderGdiplus::Draw(const char *s, size_t sLen, RectF& bb, bool isRtl) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    Draw(txtConvBuf, strLen, bb, isRtl);
}

ITextRender *CreateTextRender(TextRenderMethod method, Graphics *gfx) {

    if (TextRenderMethodGdiplus == method) {
        return TextRenderGdiplus::Create(gfx);
    }
    if (TextRenderMethodGdiplusQuick == method) {
        return TextRenderGdiplus::Create(gfx, MeasureTextQuick);
    }
    if (TextRenderMethodGdi == method) {
        return TextRenderGdi::Create(gfx);
    }
    CrashIf(true);
    return NULL;
}

// returns number of characters of string s that fits in a given width dx
// note: could be speed up a bit because in our use case we already know
// the width of the whole string so we could supply it to the function, but
// this shouldn't happen often, so that's fine. It's also possible that
// a smarter approach is possible, but this usually only does 3 MeasureText
// calls, so it's not that bad
size_t StringLenForWidth(ITextRender *textMeasure, const WCHAR *s, size_t len, float dx)
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
REAL GetSpaceDx(ITextRender *textMeasure)
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
