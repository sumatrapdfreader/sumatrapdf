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
    ::delete gfx;
    ::delete bmp;
}

void InitializeBase()
{
    InitializeCriticalSection(&gMuiCs);
    gFontsCache = new Vec<FontCacheEntry>();
    gGraphicsCache = new Vec<GraphicsCacheEntry>();
}

void DestroyBase()
{
    // if this fires, time to implement cache eviction
    CrashIf(gGraphicsCache->Count() > 64);
    for (size_t i = 0; i < gGraphicsCache->Count(); i++) {
        GraphicsCacheEntry e = gGraphicsCache->At(i);
        e.Free();
    }
    delete gGraphicsCache;

    for (size_t i = 0; i < gFontsCache->Count(); i++) {
        FontCacheEntry e = gFontsCache->At(i);
        free(e.name);
        ::delete e.font;
    }
    delete gFontsCache;

    DeleteCriticalSection(&gMuiCs);
}

static Font *CreateFontByStyle(const WCHAR *name, REAL size, FontStyle style)
{
    return ::new Font(name, size, style);
}

bool FontCacheEntry::SameAs(const WCHAR *otherName, float otherSize, FontStyle otherStyle)
{
    if (size != otherSize)
        return false;
    if (style != otherStyle)
        return false;
    return str::Eq(name, otherName);
}

Font *GetCachedFont(const WCHAR *name, float size, FontStyle style)
{
    ScopedMuiCritSec muiCs;

    for (size_t i = 0; i < gFontsCache->Count(); i++) {
        FontCacheEntry f = gFontsCache->At(i);
        if (f.SameAs(name, size, style)) {
            return f.font;
        }
    }
    FontCacheEntry f = { str::Dup(name), size, style, NULL };
    // TODO: handle a failure to create a font. Use fontCache[0] if exists
    // or try to fallback to a known font like Times New Roman
    f.font = CreateFontByStyle(name, size, style);
    gFontsCache->Append(f);
    return f.font;
}

Graphics *AllocGraphicsForMeasureText()
{
    DWORD threadId = GetCurrentThreadId();
    for (size_t i = 0; i < gGraphicsCache->Count(); i++) {
        GraphicsCacheEntry e = gGraphicsCache->At(i);
        if (e.threadId == threadId)
            return e.gfx;
    }
    GraphicsCacheEntry e;
    e.Create();
    gGraphicsCache->Append(e);
    return e.gfx;
}

// TODO: we should evict entries if the cache grows too much
// so that if we often launch threads that allocate their own
// Graphics objects, we don't use too much memory for them.
// This should be based on usage (i.e. count 
void FreeGraphicsForMeasureText(Graphics *gfx)
{
    // this is just diagnostic
    DWORD threadId = GetCurrentThreadId();
    for (size_t i = 0; i < gGraphicsCache->Count(); i++) {
        GraphicsCacheEntry e = gGraphicsCache->At(i);
        CrashIf((e.gfx == gfx) && (e.threadId != threadId));
    }
}

}
