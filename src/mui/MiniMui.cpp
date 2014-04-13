/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// as little of mui as necessary to make ../EngineDump.cpp compile

#include "BaseUtil.h"
#include "MiniMui.h"

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

class FontCache {
    struct Entry {
        WCHAR *     name;
        float       size;
        FontStyle   style;
        Font *      font;

        Entry(WCHAR *name=NULL, float size=0.0f, FontStyle style=FontStyleRegular, Font *font=NULL) :
            name(name), size(size), style(style), font(font) { }
        bool operator==(const Entry& other) const {
            return size == other.size && style == other.style && str::Eq(name, other.name);
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
        }
    }

    Font *GetFont(const WCHAR *name, float size, FontStyle style) {
        int idx = cache.Find(Entry((WCHAR *)name, size, style));
        if (idx != -1)
            return cache.At(idx).font;

        Font *font = ::new Font(name, size, style);
        if (!font) {
            // fall back to the default font, if a desired font can't be created
            font = ::new Font(L"Times New Roman", size, style);
            if (!font) {
                return cache.Count() > 0 ? cache.At(0).font : NULL;
            }
        }
        cache.Append(Entry(str::Dup(name), size, style, font));
        return font;
    }
};

static FontCache gFontCache;

Font *GetCachedFont(const WCHAR *name, float size, FontStyle style)
{
    return gFontCache.GetFont(name, size, style);
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
