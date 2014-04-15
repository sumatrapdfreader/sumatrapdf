/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// as little of mui as necessary to make ../EngineDump.cpp compile

#include "BaseUtil.h"
#include "MiniMui.h"
#include "WinUtil.h"

namespace mui {

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

enum FontRenderType { FRT_Gdiplus, FRT_Gdi };

class FontCache {
    struct Entry : public CachedFont {
        Entry(WCHAR *name=NULL, float sizePt=0.0f, FontStyle style=FontStyleRegular, Font *font=NULL, HFONT hdcFont=NULL) {
            this->name = name; this->sizePt = sizePt; this->style = style; this->font = font; this->hdcFont = hdcFont;
        }
        bool operator==(const Entry& other) const {
            return SameAs(other.name, other.sizePt, other.style) &&
                   !font == !other.font && !hdcFont == !other.hdcFont;
        }
    };

    ScopedGdiPlus scope;
    Vec<Entry> cache;

public:
    FontCache() { }
    ~FontCache() {
        for (Entry *e = cache.IterStart(); e; e = cache.IterNext()) {
            free(e->name);
            ::delete e->font;
            DeleteObject(e->hdcFont);
        }
    }

    CachedFont *GetFont(const WCHAR *name, float size, FontStyle style, FontRenderType type) {
        int idx = cache.Find(Entry((WCHAR *)name, size, style, FRT_Gdiplus == type ? (Font *)-1 : NULL, FRT_Gdi == type ? (HFONT)-1 : NULL));
        if (idx != -1)
            return &cache.At(idx);

        if (FRT_Gdiplus == type) {
            Font *font = ::new Font(name, size, style);
            CrashIf(!font);
            if (!font) {
                // fall back to the default font, if a desired font can't be created
                font = ::new Font(L"Times New Roman", size, style);
                if (!font) {
                    return cache.Count() > 0 ? &cache.At(0) : NULL;
                }
            }
            cache.Append(Entry(str::Dup(name), size, style, font, NULL));
            return &cache.Last();
        }
        else if (FRT_Gdi == type) {
            // TODO: DPI scaling doesn't belong that deep in the model
            int sizePx = (int)(size * 72 / 96);
            // TODO: take FontStyle into account as well
            HDC hdc = GetDC(NULL);
            HFONT font = CreateSimpleFont(hdc, name, sizePx);
            ReleaseDC(NULL, hdc);
            cache.Append(Entry(str::Dup(name), size, style, NULL, font));
            return &cache.Last();
        }
        else {
            CrashIf(true);
            return NULL;
        }
    }
};

static FontCache gFontCache;

CachedFont *GetCachedFontGdi(const WCHAR *name, float size, FontStyle style)
{
    return gFontCache.GetFont(name, size, style, FRT_Gdi);
}

CachedFont *GetCachedFontGdiplus(const WCHAR *name, float size, FontStyle style)
{
    return gFontCache.GetFont(name, size, style, FRT_Gdiplus);
}

class GlobalGraphicsHack {
    ScopedGdiPlus scope;
    Bitmap bmp;
public:
    Graphics gfx;

    GlobalGraphicsHack() : bmp(1, 1, PixelFormat32bppARGB), gfx(&bmp) {
        InitGraphicsMode(&gfx);
    }
};

static GlobalGraphicsHack gGH;

Graphics *AllocGraphicsForMeasureText()
{
    return &gGH.gfx;
}

void FreeGraphicsForMeasureText(Graphics *g) { }

}
