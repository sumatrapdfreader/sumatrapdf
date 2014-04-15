/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// as little of mui as necessary to make ../EngineDump.cpp compile

#include "BaseUtil.h"
#include "MiniMui.h"
#include "WinUtil.h"

using namespace Gdiplus;

namespace mui {

// needed when Mui is occasionally used instead of MiniMui
void Initialize() { }
void Destroy() { }

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
    class Entry : public CachedFont {
    public:
        Entry *_next;

        Entry(const WCHAR *name, float sizePt, FontStyle style, Font *font, HFONT hdcFont) : _next(NULL) {
            this->name = str::Dup(name); this->sizePt = sizePt; this->style = style; this->font = font; this->hdcFont = hdcFont;
        }
        ~Entry() {
            free(name);
            ::delete font;
            DeleteObject(hdcFont);
            delete _next;
        }

        bool HasType(FontRenderType type) const {
            return FRT_Gdiplus == type ? !!font : FRT_Gdi == type ? !!hdcFont : false;
        }
    };

    ScopedGdiPlus scope;
    Entry *firstEntry;

public:
    FontCache() : firstEntry(NULL) { }
    ~FontCache() { delete firstEntry; }

    CachedFont *GetFont(const WCHAR *name, float size, FontStyle style, FontRenderType type) {
        Entry **entry = &firstEntry;
        for (; *entry; entry = &(*entry)->_next) {
            if ((*entry)->SameAs(name, size, style) && (*entry)->HasType(type)) {
                return *entry;
            }
        }

        if (FRT_Gdiplus == type) {
            Font *font = ::new Font(name, size, style);
            CrashIf(!font);
            if (!font) {
                // fall back to the default font, if a desired font can't be created
                font = ::new Font(L"Times New Roman", size, style);
                if (!font) {
                    return firstEntry;
                }
            }
            return (*entry = new Entry(name, size, style, font, NULL));
        }
        else if (FRT_Gdi == type) {
            // TODO: DPI scaling shouldn't belong that deep in the model
            int sizePx = (int)(size * 72 / 96);
            // TODO: take FontStyle into account as well
            HDC hdc = GetDC(NULL);
            HFONT font = CreateSimpleFont(hdc, name, sizePx);
            ReleaseDC(NULL, hdc);
            return (*entry = new Entry(name, size, style, NULL, font));
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
