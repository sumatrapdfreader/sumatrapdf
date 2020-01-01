/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/HtmlParserLookup.h"
#include "Mui.h"
#include "utils/WinUtil.h"

namespace mui {

// a critical section for everything that needs protecting in mui
// we use only one for simplicity as long as contention is not a problem
static CRITICAL_SECTION gMuiCs;

void EnterMuiCriticalSection() {
    EnterCriticalSection(&gMuiCs);
}

void LeaveMuiCriticalSection() {
    LeaveCriticalSection(&gMuiCs);
}

class FontListItem {
  public:
    FontListItem(const WCHAR* name, float sizePt, FontStyle style, Font* font, HFONT hFont) : next(nullptr) {
        cf.name = str::Dup(name);
        cf.sizePt = sizePt;
        cf.style = style;
        cf.font = font;
        cf.hFont = hFont;
    }
    ~FontListItem() {
        str::Free(cf.name);
        ::delete cf.font;
        DeleteObject(cf.hFont);
        delete next;
    }

    CachedFont cf;
    FontListItem* next;
};

// Global, thread-safe font cache. Font objects live forever.
static FontListItem* gFontsCache = nullptr;

// Graphics objects cannot be used across threads. We have a per-thread
// cache so that it's easy to grab Graphics object to be used for
// measuring text
struct GraphicsCacheEntry {
    enum {
        bmpDx = 32,
        bmpDy = 4,
        stride = bmpDx * 4,
    };

    DWORD threadId;
    int refCount;

    Graphics* gfx;
    Bitmap* bmp;
    BYTE data[bmpDx * bmpDy * 4];

    bool Create();
    void Free();
};

static Vec<GraphicsCacheEntry>* gGraphicsCache = nullptr;

// set consistent mode for our graphics objects so that we get
// the same results when measuring text
void InitGraphicsMode(Graphics* g) {
    g->SetCompositingQuality(CompositingQualityHighQuality);
    g->SetSmoothingMode(SmoothingModeAntiAlias);
    // g.SetSmoothingMode(SmoothingModeHighQuality);
    g->SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(UnitPixel);
}

bool GraphicsCacheEntry::Create() {
    memset(data, 0, sizeof(data));
    refCount = 1;
    threadId = GetCurrentThreadId();
    // using a small bitmap under assumption that Graphics used only
    // for measuring text doesn't need the actual bitmap
    bmp = ::new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, data);
    if (!bmp)
        return false;
    gfx = ::new Graphics((Image*)bmp);
    if (!gfx)
        return false;
    InitGraphicsMode(gfx);
    return true;
}

void GraphicsCacheEntry::Free() {
    CrashIf(0 != refCount);
    ::delete gfx;
    ::delete bmp;
}

void InitializeBase() {
    InitializeCriticalSection(&gMuiCs);
    gGraphicsCache = new Vec<GraphicsCacheEntry>();
    // allocate the first entry in gGraphicsCache for UI thread, ref count
    // ensures it stays alive forever
    AllocGraphicsForMeasureText();
}

void DestroyBase() {
    FreeGraphicsForMeasureText(gGraphicsCache->at(0).gfx);
    for (GraphicsCacheEntry& e : *gGraphicsCache) {
        e.Free();
    }
    delete gGraphicsCache;
    delete gFontsCache;
    DeleteCriticalSection(&gMuiCs);
}

bool CachedFont::SameAs(const WCHAR* otherName, float otherSizePt, FontStyle otherStyle) const {
    if (sizePt != otherSizePt)
        return false;
    if (style != otherStyle)
        return false;
    return str::Eq(name, otherName);
}

HFONT CachedFont::GetHFont() {
    LOGFONTW lf;
    EnterMuiCriticalSection();
    if (!hFont) {
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
    LeaveMuiCriticalSection();
    return hFont;
}

// convenience function: given cached style, get a Font object matching the font
// properties.
// Caller should not delete the font - it's cached for performance and deleted at exit
CachedFont* GetCachedFont(const WCHAR* name, float sizePt, FontStyle style) {
    ScopedMuiCritSec muiCs;

    for (FontListItem* item = gFontsCache; item; item = item->next) {
        if (item->cf.SameAs(name, sizePt, style) && item->cf.font != nullptr) {
            return &item->cf;
        }
    }

    Font* font = ::new Font(name, sizePt, style);
    if (!font) {
        font = ::new Font(L"Times New Roman", sizePt, style);
        if (!font) {
            // if no font is available, return the last successfully created one
            return gFontsCache ? &gFontsCache->cf : nullptr;
        }
    }

    FontListItem* item = new FontListItem(name, sizePt, style, font, nullptr);
    ListInsert(&gFontsCache, item);
    return &item->cf;
}

Graphics* AllocGraphicsForMeasureText() {
    ScopedMuiCritSec muiCs;

    DWORD threadId = GetCurrentThreadId();
    for (GraphicsCacheEntry& e : *gGraphicsCache) {
        if (e.threadId == threadId) {
            e.refCount++;
            return e.gfx;
        }
    }
    GraphicsCacheEntry ce;
    ce.Create();
    gGraphicsCache->Append(ce);
    if (gGraphicsCache->size() < 64)
        return ce.gfx;

    // try to limit the size of cache by evicting the oldest entries, but don't remove
    // first (for ui thread) or last (one we just added) entries
    for (size_t i = 1; i < gGraphicsCache->size() - 1; i++) {
        GraphicsCacheEntry e = gGraphicsCache->at(i);
        if (0 == e.refCount) {
            e.Free();
            gGraphicsCache->RemoveAt(i);
            return ce.gfx;
        }
    }
    // We shouldn't get here - indicates ref counting problem
    CrashIf(true);
    return ce.gfx;
}

void FreeGraphicsForMeasureText(Graphics* gfx) {
    ScopedMuiCritSec muiCs;

    DWORD threadId = GetCurrentThreadId();
    for (GraphicsCacheEntry& e : *gGraphicsCache) {
        if (e.gfx == gfx) {
            CrashIf(e.threadId != threadId);
            e.refCount--;
            CrashIf(e.refCount < 0);
            return;
        }
    }
    CrashIf(true);
}

int CeilI(float n) {
    n = ceil(n);
    return (int)n;
}
} // namespace mui
