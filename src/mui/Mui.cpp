/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GdiPlusUtil.h"

#include "wingui/PlatformFont.h"

#include "Mui.h"

/*
MUI is a simple UI library for win32.
MUI stands for nothing, it's just ui and gui are overused.

MUI is intended to allow building UIs that have modern
capabilities not supported by the standard win32 HWND
architecture:
- overlapping, alpha-blended windows
- animations
- a saner layout

It's inspired by WPF, WDL (http://www.cockos.com/wdl/),
DirectUI (https://github.com/kjk/directui).

MUI is minimal - it only supports stuff needed for Sumatra.
I got burned trying to build the whole toolkit at once with DirectUI.
Less code there is, the easier it is to change or extend.

The basic architectures is that of a tree of "virtual" (not backed
by HWND) windows. Each window can have children (making it a container).
Children windows are positioned relative to its parent window and can
be positioned outside of parent's bounds.

There must be a parent window backed by HWND which handles windows
messages and paints child windows on WM_PAINT.

Event handling is loosly coupled.
*/

using Gdiplus::Bitmap;
using Gdiplus::CompositingQualityHighQuality;
using Gdiplus::Font;
using Gdiplus::Image;
using Gdiplus::Ok;
using Gdiplus::SmoothingModeAntiAlias;
using Gdiplus::Status;
using Gdiplus::TextRenderingHintClearTypeGridFit;
using Gdiplus::UnitPixel;

namespace mui {

// a single lock for everything that needs protecting in mui
// we use only one for simplicity as long as contention is not a problem
static Mutex gMuiCs;

static void EnterMuiLock() {
    gMuiCs.Lock();
}

static void LeaveMuiLock() {
    gMuiCs.Unlock();
}

class ScopedMuiCritSec {
  public:
    ScopedMuiCritSec() { EnterMuiLock(); }

    ~ScopedMuiCritSec() { LeaveMuiLock(); }
};

static Graphics* AllocGraphicsForMeasureTextNoLock();
static void FreeGraphicsForMeasureTextNoLock(Graphics* gfx);

// Graphics objects cannot be used across threads. We have a per-thread
// cache so that it's easy to grab Graphics object to be used for
// measuring text
struct GraphicsCacheEntry {
    enum {
        bmpDx = 32,
        bmpDy = 4,
        stride = bmpDx * 4,
    };

    ThreadId threadId;
    int refCount;

    Graphics* gfx;
    Bitmap* bmp;
    u8 data[bmpDx * bmpDy * 4];

    bool Create();
    void Free() const;
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
    bmp = new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, data);
    if (!bmp) {
        return false;
    }
    gfx = new Graphics((Image*)bmp);
    if (!gfx) {
        return false;
    }
    InitGraphicsMode(gfx);
    return true;
}

void GraphicsCacheEntry::Free() const {
    ReportIf(0 != refCount);
    delete gfx;
    delete bmp;
}

void Initialize() {
    gGraphicsCache = new Vec<GraphicsCacheEntry>();
    // allocate the first entry in gGraphicsCache for UI thread, ref count
    // ensures it stays alive forever
    AllocGraphicsForMeasureText();
}

void Destroy() {
    FreeGraphicsForMeasureText((*gGraphicsCache)[0].gfx);
    for (GraphicsCacheEntry& e : *gGraphicsCache) {
        e.Free();
    }
    delete gGraphicsCache;
    gGraphicsCache = nullptr;
}

// caller must hold the mui lock (gMuiCs is not re-entrant)
static Graphics* AllocGraphicsForMeasureTextNoLock() {
    ThreadId threadId = GetCurrentThreadId();
    for (GraphicsCacheEntry& e : *gGraphicsCache) {
        if (e.threadId == threadId) {
            e.refCount++;
            return e.gfx;
        }
    }
    GraphicsCacheEntry ce;
    ce.Create();
    gGraphicsCache->Append(ce);
    if (len(*gGraphicsCache) < 64) {
        return ce.gfx;
    }

    // try to limit the size of cache by evicting the oldest entries, but don't remove
    // first (for ui thread) or last (one we just added) entries
    for (int i = 1; i < len(*gGraphicsCache) - 1; i++) {
        GraphicsCacheEntry e = (*gGraphicsCache)[i];
        if (0 == e.refCount) {
            e.Free();
            gGraphicsCache->RemoveAt(i);
            return ce.gfx;
        }
    }
    // We shouldn't get here - indicates ref counting problem
    ReportIf(true);
    return ce.gfx;
}

Graphics* AllocGraphicsForMeasureText() {
    ScopedMuiCritSec muiCs;
    return AllocGraphicsForMeasureTextNoLock();
}

// caller must hold the mui lock (gMuiCs is not re-entrant)
static void FreeGraphicsForMeasureTextNoLock(Graphics* gfx) {
    ThreadId threadId = GetCurrentThreadId();
    for (GraphicsCacheEntry& e : *gGraphicsCache) {
        if (e.gfx == gfx) {
            ReportIf(e.threadId != threadId);
            e.refCount--;
            ReportIf(e.refCount < 0);
            return;
        }
    }
    ReportIf(true);
}

void FreeGraphicsForMeasureText(Graphics* gfx) {
    ScopedMuiCritSec muiCs;
    FreeGraphicsForMeasureTextNoLock(gfx);
}

} // namespace mui
