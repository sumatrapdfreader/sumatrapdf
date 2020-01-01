/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// as little of mui as necessary to make ../EngineDump.cpp compile

#include "utils/BaseUtil.h"
#include "MiniMui.h"
#include "utils/WinUtil.h"

using namespace Gdiplus;

namespace mui {

HFONT CachedFont::GetHFont() {
    if (!hFont) {
        LOGFONTW lf{};
        // TODO: Graphics is probably only used for metrics,
        // so this might not be 100% correct (e.g. 2 monitors with different DPIs?)
        // but previous code wasn't much better
        Graphics* gfx = AllocGraphicsForMeasureText();
        Status status = font->GetLogFontW(gfx, &lf);
        FreeGraphicsForMeasureText(gfx);
        CrashIf(status != Ok);
        hFont = CreateFontIndirectW(&lf);
        CrashIf(!hFont);
    }
    return hFont;
}

class CachedFontItem : public CachedFont {
  public:
    CachedFontItem* _next;

    CachedFontItem(const WCHAR* name, float sizePt, FontStyle style, Font* font) : _next(nullptr) {
        this->name = str::Dup(name);
        this->sizePt = sizePt;
        this->style = style;
        this->font = font;
    }
    ~CachedFontItem() {
        free(name);
        ::delete font;
        DeleteObject(hFont);
        delete _next;
    }
};

static CachedFontItem* gFontCache = nullptr;

CachedFont* GetCachedFont(const WCHAR* name, float size, FontStyle style) {
    CachedFontItem** item = &gFontCache;
    for (; *item; item = &(*item)->_next) {
        if ((*item)->SameAs(name, size, style)) {
            return *item;
        }
    }

    Font* font = ::new Font(name, size, style);
    CrashIf(!font);
    if (!font) {
        // fall back to the default font, if a desired font can't be created
        font = ::new Font(L"Times New Roman", size, style);
        if (!font) {
            return gFontCache;
        }
    }

    return (*item = new CachedFontItem(name, size, style, font));
}

// set consistent mode for our graphics objects so that we get
// the same results when measuring text
void InitGraphicsMode(Graphics* g) {
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    // g.SetSmoothingMode(SmoothingModeHighQuality);
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

static GlobalGraphicsHack* gGraphicsHack = nullptr;

Graphics* AllocGraphicsForMeasureText() {
    if (!gGraphicsHack) {
        gGraphicsHack = new GlobalGraphicsHack();
    }
    return &gGraphicsHack->gfx;
}

void FreeGraphicsForMeasureText(Graphics* g) {
    UNUSED(g); /* deallocation happens in mui::Destroy */
}

// allow for calls to mui::Initialize and mui::Destroy to be nested
static LONG gMiniMuiRefCount = 0;

void Initialize() {
    InterlockedIncrement(&gMiniMuiRefCount);
}

void Destroy() {
    if (InterlockedDecrement(&gMiniMuiRefCount) != 0)
        return;

    delete gFontCache;
    gFontCache = nullptr;
    delete gGraphicsHack;
    gGraphicsHack = nullptr;
}
} // namespace mui
