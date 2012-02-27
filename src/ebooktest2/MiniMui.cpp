/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "MiniMui.h"
#include "Scoped.h"
#include "Vec.h"

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
