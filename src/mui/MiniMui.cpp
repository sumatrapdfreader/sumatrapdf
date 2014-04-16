/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// as little of mui as necessary to make ../EngineDump.cpp compile

#include "BaseUtil.h"
#include "MiniMui.h"
#include "WinUtil.h"

using namespace Gdiplus;

namespace mui {

class CachedFontItem : public CachedFont {
public:
    enum FontRenderType { Gdiplus, Gdi };

    CachedFontItem *_next;

    CachedFontItem(const WCHAR *name, float sizePt, FontStyle style, Font *font, HFONT hdcFont) : _next(NULL) {
        this->name = str::Dup(name); this->sizePt = sizePt; this->style = style; this->font = font; this->hdcFont = hdcFont;
    }
    ~CachedFontItem() {
        free(name);
        ::delete font;
        DeleteObject(hdcFont);
        delete _next;
    }

    bool HasType(FontRenderType type) const {
        return Gdiplus == type ? !!font : Gdi == type ? !!hdcFont : false;
    }
};

static CachedFontItem *gFontCache = NULL;

static CachedFont *GetCachedFont(HDC hdc, const WCHAR *name, float size, FontStyle style, CachedFontItem::FontRenderType type)
{
    CachedFontItem **item = &gFontCache;
    for (; *item; item = &(*item)->_next) {
        if ((*item)->SameAs(name, size, style) && (*item)->HasType(type)) {
            return *item;
        }
    }

    if (CachedFontItem::Gdiplus == type) {
        Font *font = ::new Font(name, size, style);
        CrashIf(!font);
        if (!font) {
            // fall back to the default font, if a desired font can't be created
            font = ::new Font(L"Times New Roman", size, style);
            if (!font) {
                return gFontCache;
            }
        }
        return (*item = new CachedFontItem(name, size, style, font, NULL));
    }
    else if (CachedFontItem::Gdi == type) {
        // TODO: take FontStyle into account as well
        bool release = false;
        if (!hdc) {
            hdc = GetDC(NULL);
            release = true;
        }
        HFONT font = CreateSimpleFont(hdc, name, (int)(size * 96 / 72));
        if (release)
            ReleaseDC(NULL, hdc);
        return (*item = new CachedFontItem(name, size, style, NULL, font));
    }
    else {
        CrashIf(true);
        return NULL;
    }
}

CachedFont *GetCachedFontGdi(HDC hdc, const WCHAR *name, float size, FontStyle style)
{
    return GetCachedFont(hdc, name, size, style, CachedFontItem::Gdi);
}

CachedFont *GetCachedFontGdiplus(const WCHAR *name, float size, FontStyle style)
{
    return GetCachedFont(NULL, name, size, style, CachedFontItem::Gdiplus);
}

// set consistent mode for our graphics objects so that we get
// the same results when measuring text
void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);
}

class GlobalGraphicsHack {
    Bitmap bmp;
public:
    Graphics gfx;

    GlobalGraphicsHack() : bmp(1, 1, PixelFormat32bppARGB), gfx(&bmp) {
        InitGraphicsMode(&gfx);
    }
};

static GlobalGraphicsHack *gGraphicsHack = NULL;

Graphics *AllocGraphicsForMeasureText()
{
    if (!gGraphicsHack) {
        gGraphicsHack = new GlobalGraphicsHack();
    }
    return &gGraphicsHack->gfx;
}

void FreeGraphicsForMeasureText(Graphics *g) { /* deallocation happens in mui::Destroy */ }

void Initialize() { /* all initialization happens on demand */ }

void Destroy()
{
    delete gFontCache;
    gFontCache = NULL;
    delete gGraphicsHack;
    gGraphicsHack = NULL;
}

}
