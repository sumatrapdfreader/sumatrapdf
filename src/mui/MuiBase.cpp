/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Mui.h"

namespace mui {

// a critical section for everything that needs protecting in mui
// we use only one for simplicity as long as contention is not a problem
static CRITICAL_SECTION gMuiCs;

void EnterMuiCriticalSection()
{
    EnterCriticalSection(&gMuiCs);
}

void LeaveMuiCriticalSection()
{
    LeaveCriticalSection(&gMuiCs);
}

// Global, thread-safe font cache. Font objects live forever.
struct FontCacheEntry {
    WCHAR *     name;
    float       size;
    FontStyle   style;

    Font *      font;

    bool SameAs(const WCHAR *name, float size, FontStyle style);
};

static Vec<FontCacheEntry> *gFontsCache = NULL;

// Graphics objects cannot be used across threads. We have a per-thread
// cache so that it's easy to grab Graphics object to be used for
// measuring text
struct GraphicsCacheEntry
{
    enum {
        bmpDx = 32,
        bmpDy = 4,
        stride = bmpDx * 4,
    };

    DWORD       threadId;
    int         refCount;

    Graphics *  gfx;
    Bitmap *    bmp;
    BYTE        data[bmpDx * bmpDy * 4];

    bool Create();
    void Free();
};

static Vec<GraphicsCacheEntry> *gGraphicsCache = NULL;

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

bool GraphicsCacheEntry::Create()
{
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

void GraphicsCacheEntry::Free()
{
    CrashIf(0 != refCount);
    ::delete gfx;
    ::delete bmp;
}

void InitializeBase()
{
    InitializeCriticalSection(&gMuiCs);
    gFontsCache = new Vec<FontCacheEntry>();
    gGraphicsCache = new Vec<GraphicsCacheEntry>();
    // allocate the first entry in gGraphicsCache for UI thread, ref count
    // ensures it stays alive forever
    AllocGraphicsForMeasureText();
}

void DestroyBase()
{
    FreeGraphicsForMeasureText(gGraphicsCache->At(0).gfx);

    for (GraphicsCacheEntry *e = gGraphicsCache->IterStart(); e; e = gGraphicsCache->IterNext()) {
        e->Free();
    }
    delete gGraphicsCache;

    for (FontCacheEntry *e = gFontsCache->IterStart(); e; e = gFontsCache->IterNext()) {
        free(e->name);
        ::delete e->font;
    }
    delete gFontsCache;

    DeleteCriticalSection(&gMuiCs);
}

bool FontCacheEntry::SameAs(const WCHAR *otherName, float otherSize, FontStyle otherStyle)
{
    if (size != otherSize)
        return false;
    if (style != otherStyle)
        return false;
    return str::Eq(name, otherName);
}

// convenience function: given cached style, get a Font object matching the font
// properties.
// Caller should not delete the font - it's cached for performance and deleted at exit
Font *GetCachedFont(const WCHAR *name, float size, FontStyle style)
{
    ScopedMuiCritSec muiCs;

    for (FontCacheEntry *e = gFontsCache->IterStart(); e; e = gFontsCache->IterNext()) {
        if (e->SameAs(name, size, style)) {
            return e->font;
        }
    }

    FontCacheEntry f = { str::Dup(name), size, style, NULL };
    // TODO: handle a failure to create a font. Use fontCache[0] if exists
    // or try to fallback to a known font like Times New Roman
    f.font = ::new Font(name, size, style, UnitPixel);
    gFontsCache->Append(f);
    return f.font;
}

Graphics *AllocGraphicsForMeasureText()
{
    ScopedMuiCritSec muiCs;

    DWORD threadId = GetCurrentThreadId();
    for (GraphicsCacheEntry *e = gGraphicsCache->IterStart(); e; e = gGraphicsCache->IterNext()) {
        if (e->threadId == threadId) {
            e->refCount++;
            return e->gfx;
        }
    }
    GraphicsCacheEntry ce;
    ce.Create();
    gGraphicsCache->Append(ce);
    if (gGraphicsCache->Count() < 64)
        return ce.gfx;

    // try to limit the size of cache by evicting the oldest entries, but don't remove
    // first (for ui thread) or last (one we just added) entries
    for (size_t i = 1; i < gGraphicsCache->Count() - 1; i++) {
        GraphicsCacheEntry e = gGraphicsCache->At(i);
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

void FreeGraphicsForMeasureText(Graphics *gfx)
{
    ScopedMuiCritSec muiCs;

    DWORD threadId = GetCurrentThreadId();
    for (GraphicsCacheEntry *e = gGraphicsCache->IterStart(); e; e = gGraphicsCache->IterNext()) {
        if (e->gfx == gfx) {
            CrashIf(e->threadId != threadId);
            e->refCount--;
            CrashIf(e->refCount < 0);
            return;
        }
    }
    CrashIf(true);
}

}
