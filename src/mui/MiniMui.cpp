/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// as little of mui as necessary to make ../EngineDump.cpp compile

#include "MiniMui.h"
#include "Scoped.h"
#include "Vec.h"

namespace mui {

class FontCache {
    struct Entry{
        WCHAR *     name;
        float       size;
        FontStyle   style;
        Font *      font;

        bool operator==(Entry& other){
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
        Entry f = { (WCHAR *)name, size, style, NULL };
        for (Entry *e = cache.IterStart(); e; e = cache.IterNext()) {
            if (f == *e)
                return e->font;
        }

        f.font = ::new Font(name, size, style);
        if (!f.font) {
            // fall back to the default font, if a desired font can't be created
            f.font = ::new Font(L"Times New Roman", size, style);
            if (!f.font) {
                if (cache.Count() > 0)
                    return cache.At(0).font;
                return NULL;
            }
        }
        f.name = str::Dup(f.name);
        cache.Append(f);
        return f.font;
    }
};

static FontCache gFontCache;

Font *GetCachedFont(const WCHAR *name, float size, FontStyle style)
{
    return gFontCache.GetFont(name, size, style);
}

static void InitGraphicsMode(Graphics *g)
{
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    //g.SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);
}

class GlobalGraphicsHack {
    ScopedGdiPlus scope;
    Bitmap bmp;
public:
    Graphics gfx;

    GlobalGraphicsHack() : bmp(1, 1, PixelFormat32bppARGB), gfx(&bmp) {
        // cf. EpubEngineImpl::RenderPage
        gfx.SetCompositingQuality(CompositingQualityHighQuality);
        gfx.SetSmoothingMode(SmoothingModeAntiAlias);
        gfx.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        gfx.SetPageUnit(UnitPixel);
    }
};

static GlobalGraphicsHack gGH;

Graphics *AllocGraphicsForMeasureText()
{
    return &gGH.gfx;
}

void FreeGraphicsForMeasureText(Graphics *g) { }

}
