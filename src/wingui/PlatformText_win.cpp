/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GdiPlusUtil.h"

#include "wingui/PlatformFont.h"
#include "wingui/PlatformText.h"

#include "mui/Mui.h"

using Gdiplus::Bitmap;
using Gdiplus::Graphics;
using Gdiplus::Image;

// Graphics objects cannot be used across threads, and creating one is not free,
// so keep a per-thread one around for measuring text. It's all private to
// measuring: nothing else needs a Graphics that isn't drawn to
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

// a single lock for everything here; contention is not a problem
static Mutex gGraphicsCacheMutex;
static Vec<GraphicsCacheEntry>* gGraphicsCache = nullptr;

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
    mui::InitGraphicsMode(gfx);
    return true;
}

void GraphicsCacheEntry::Free() const {
    ReportIf(0 != refCount);
    delete gfx;
    delete bmp;
}

// caller must hold gGraphicsCacheMutex (it is not re-entrant)
static Graphics* AllocGraphicsForMeasureTextNoLock() {
    if (!gGraphicsCache) {
        gGraphicsCache = new Vec<GraphicsCacheEntry>();
    }
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

    // try to limit the size of cache by evicting the oldest entries, but don't
    // remove the last (the one we just added) entry
    for (int i = 0; i < len(*gGraphicsCache) - 1; i++) {
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

static Graphics* AllocGraphicsForMeasureText() {
    gGraphicsCacheMutex.Lock();
    defer {
        gGraphicsCacheMutex.Unlock();
    };
    return AllocGraphicsForMeasureTextNoLock();
}

static void FreeGraphicsForMeasureText(Graphics* gfx) {
    gGraphicsCacheMutex.Lock();
    defer {
        gGraphicsCacheMutex.Unlock();
    };
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

void PlatformFontDestroy() {
    gGraphicsCacheMutex.Lock();
    defer {
        gGraphicsCacheMutex.Unlock();
    };
    if (!gGraphicsCache) {
        return;
    }
    for (GraphicsCacheEntry& e : *gGraphicsCache) {
        // whoever held this is gone by now
        e.refCount = 0;
        e.Free();
    }
    delete gGraphicsCache;
    gGraphicsCache = nullptr;
}

static mui::TextRenderMethod ToMuiTextRenderMethod(PlatformTextMeasureMethod method) {
    switch (method) {
        case PlatformTextMeasureMethod::Gdiplus:
            return mui::TextRenderMethod::Gdiplus;
        case PlatformTextMeasureMethod::GdiplusQuick:
            return mui::TextRenderMethod::GdiplusQuick;
        case PlatformTextMeasureMethod::Gdi:
            return mui::TextRenderMethod::Gdi;
        case PlatformTextMeasureMethod::Hdc:
            return mui::TextRenderMethod::Hdc;
        case PlatformTextMeasureMethod::Stub:
            break;
    }
    return mui::TextRenderMethod::Gdiplus;
}

struct WinTextMeasurer : PlatformTextMeasurer {
    Graphics* gfx = nullptr;
    mui::ITextRender* textMeasure = nullptr;

    explicit WinTextMeasurer(PlatformTextMeasureMethod method) {
        gfx = AllocGraphicsForMeasureText();
        textMeasure = mui::CreateTextRender(ToMuiTextRenderMethod(method), gfx, 10, 10);
    }

    ~WinTextMeasurer() override {
        delete textMeasure;
        FreeGraphicsForMeasureText(gfx);
    }

    void SetFont(PlatformFont* font) override { textMeasure->SetFont(font); }

    float GetCurrFontLineSpacing() override { return textMeasure->GetCurrFontLineSpacing(); }

    float GetSpaceDx() override { return mui::GetSpaceDx(textMeasure); }

    RectF Measure(Str s) override { return textMeasure->Measure(ToWStrTemp(s)); }

    // mui measures utf-16, so its answer is a WCHAR count: turn it back into the
    // byte count the caller asked for
    int StringLenForWidth(Str s, float dx, float sWidth) override {
        TempWStr ws = ToWStrTemp(s);
        int nChars = mui::StringLenForWidth(textMeasure, ws, dx, sWidth);
        if (nChars >= len(ws)) {
            return len(s);
        }
        if (nChars <= 0) {
            return 0;
        }
        return len(ToUtf8Temp(WStr(ws.s, nChars)));
    }
};

// called by CreatePlatformTextMeasurer() in PlatformText.cpp
PlatformTextMeasurer* CreateNativeTextMeasurer(PlatformTextMeasureMethod method) {
    return new WinTextMeasurer(method);
}
