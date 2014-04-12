/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

using namespace Gdiplus;
#include "TextRender.h"
#include "GdiPlusUtil.h"
#include "WinUtil.h"

/* Note: this code should be in utils but it depends on mui code, so it's
in mui, to avoid circular dependency */

#define FONT_NAME       L"Tahoma"
#define FONT_SIZE       10

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
    if (gFont == nullptr) {
        HDC hdc = GetDC(NULL);
        gFont = CreateSimpleFont(hdc, FONT_NAME, (int) DpiScaled((float) PixelToPoint(FONT_SIZE)));
        ReleaseDC(NULL, hdc);
    }
    return gFont;
}

void DeleteGdiFont() {
    DeleteObject(gFont);
}

RectF TextMeasureGdi::Measure(const char *s, size_t sLen) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    SIZE txtSize;
    GetTextExtentPoint32W(hdc, txtConvBuf, (int) strLen, &txtSize);
    RectF res(0.0f, 0.0f, (float) txtSize.cx, (float) txtSize.cy);
    return res;
}

TextMeasureGdi *TextMeasureGdi::Create(HDC hdc) {
    auto res = new TextMeasureGdi();
    res->hdc = hdc;
    return res;
}


TextMeasureGdiplus::~TextMeasureGdiplus() {
    ::delete bmp;
    ::delete gfx;
    ::delete fnt;
};

RectF TextMeasureGdiplus::Measure(const char *s, size_t sLen) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    return MeasureTextAccurate(gfx, fnt, txtConvBuf, strLen);
}

TextMeasureGdiplus *TextMeasureGdiplus::Create(HDC hdc) {
    auto res = new TextMeasureGdiplus();
    res->bmp = ::new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, res->data);
    if (!res->bmp)
        return nullptr;
    res->gfx = ::new Graphics((Image*) res->bmp);
    mui::InitGraphicsMode(res->gfx);
    res->fnt = ::new Font(hdc, GetGdiFont());
    //res->fnt = ::new Font(FONT_NAME, FONT_SIZE);
    return res;
}

TextDrawGdi* TextDrawGdi::Create(HDC hdc) {
    auto res = new TextDrawGdi();
    res->hdc = hdc;
    return res;
}

void TextDrawGdi::Draw(const char *s, size_t sLen, RectF& bb) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    PointF loc;
    bb.GetLocation(&loc);
    int x = (int) bb.X;
    int y = (int) bb.Y;
    ExtTextOutW(hdc, x, y, 0, NULL, txtConvBuf, strLen, NULL);
}

TextDrawGdiplus* TextDrawGdiplus::Create(HDC dc) {
    auto res = new TextDrawGdiplus();
    res->gfx = ::new Graphics(dc);
    mui::InitGraphicsMode(res->gfx);
    res->fnt = ::new Font(dc, GetGdiFont());
    //res->fnt = ::new Font(FONT_NAME, FONT_SIZE);
    res->col = ::new SolidBrush(Color(0, 0, 0));
    return res;
}

TextDrawGdiplus::~TextDrawGdiplus() {
    ::delete gfx;
    ::delete fnt;
}

void TextDrawGdiplus::Draw(const char *s, size_t sLen, RectF& bb) {
    size_t strLen = str::Utf8ToWcharBuf(s, sLen, txtConvBuf, dimof(txtConvBuf));
    PointF loc;
    bb.GetLocation(&loc);
    gfx->DrawString(txtConvBuf, (INT) strLen, fnt, loc, col);
}

