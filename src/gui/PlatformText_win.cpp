/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GdiPlusUtil.h"
#include "base/Win.h"

#include "gui/PlatformFont.h"
#include "gui/PlatformText.h"

/*
TODO:
 - text drawing is still too slow. each html page takes ~20ms to draw, which is
   terrible and much slower than what I think the test render was doing (~1ms)
   Is it beacuase it draws to gfx->GetHDC() instead of e.g. natural or bitmap
   HDC? In which case maybe I should render text to bitmap hdc and then
   blit that once to Graphics?
 - figure out a way to get rid of Lock()/Unlock(). One way is to turn
   PlatformTextRender into a full-blown IGraphics abstraction (add drawing calls
   to it) and then the GDI+-based implementation could track locking state
   internally, so that the caller doesn't have to.
   Another options would be to figure out a way to draw to a bitmap and blit
   that bitmap to Graphics object.
*/

using Gdiplus::Bitmap;
using Gdiplus::Graphics;
using Gdiplus::Image;
using Gdiplus::Ok;
using Gdiplus::Region;
using Gdiplus::SolidBrush;
using Gdiplus::Status;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsDirectionRightToLeft;

// --- a Graphics to measure with

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
    InitGraphicsMode(gfx);
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

// --- the renderers

// what all three implementations have in common: the Graphics they draw to (or,
// when only measuring, the cached one they borrowed), the current font and the
// colors. Line spacing comes from the font, so it's the same everywhere too
struct WinTextRender : PlatformTextRender {
    Graphics* gfx = nullptr;
    // set when gfx came from the measuring cache rather than from a caller
    bool ownsGfx = false;
    PlatformFont* currFont = nullptr;
    // black, like the Gdiplus::Color these used to default to
    Color textColor = 0;
    Color textBgColor = 0;

    ~WinTextRender() override {
        if (ownsGfx) {
            FreeGraphicsForMeasureText(gfx);
        }
    }

    float GetCurrFontLineSpacing() override { return currFont->gdiFont->GetHeight(gfx); }
};

// draws with gdi, into the hdc of the Graphics. Draw() must be bracketed by
// Lock()/Unlock(), which is where that hdc is taken and given back
struct TextRenderGdi : WinTextRender {
    HDC hdcGfxLocked = nullptr;
    HDC hdcForTextMeasure = nullptr;
    HGDIOBJ hdcForTextMeasurePrevFont = nullptr;

    ~TextRenderGdi() override;

    void CreateHdcForTextMeasure();
    void RestoreHdcForTextMeasurePrevFont();

    void SetFont(PlatformFont* font) override;
    RectF Measure(Str s) override;
    void SetTextColor(Color col) override;
    void SetTextBgColor(Color col) override;
    void Lock() override;
    void Unlock() override;
    // note: ignores any transformation set on gfx
    void Draw(Str s, RectF bb, bool isRtl) override;
};

void TextRenderGdi::CreateHdcForTextMeasure() {
    HDC hdc = hdcGfxLocked;
    bool unlock = false;
    if (!hdc) {
        hdc = gfx->GetHDC();
        unlock = true;
    }
    hdcForTextMeasure = CreateCompatibleDC(hdc);
    if (unlock) {
        gfx->ReleaseHDC(hdc);
    }
}

TextRenderGdi::~TextRenderGdi() {
    RestoreHdcForTextMeasurePrevFont();
    DeleteDC(hdcForTextMeasure);
    ReportIf(hdcGfxLocked); // hasn't been Unlock()ed
}

void TextRenderGdi::RestoreHdcForTextMeasurePrevFont() {
    if (hdcForTextMeasurePrevFont != nullptr) {
        SelectObject(hdcForTextMeasure, hdcForTextMeasurePrevFont);
        hdcForTextMeasurePrevFont = nullptr;
    }
}

void TextRenderGdi::SetFont(PlatformFont* font) {
    // I'm not sure how expensive SelectFont() is so avoid it just in case
    if (currFont == font) {
        return;
    }
    currFont = font;
    HFONT hfont = font->GetHFont();
    if (hdcGfxLocked) {
        SelectFont(hdcGfxLocked, hfont);
    }
    if (hdcForTextMeasure) {
        RestoreHdcForTextMeasurePrevFont();
        hdcForTextMeasurePrevFont = SelectFont(hdcForTextMeasure, hfont);
    }
}

RectF TextRenderGdi::Measure(Str s) {
    Size size = HdcGetTextExtentPoint32(hdcForTextMeasure, s);
    RectF res(0.0f, 0.0f, (float)size.dx, (float)size.dy);
    return res;
}

void TextRenderGdi::SetTextColor(Color col) {
    if (textColor == col) {
        return;
    }
    textColor = col;
    if (hdcGfxLocked) {
        ::SetTextColor(hdcGfxLocked, col);
    }
}

void TextRenderGdi::SetTextBgColor(Color col) {
    if (textBgColor == col) {
        return;
    }
    textBgColor = col;
    if (hdcGfxLocked) {
        ::SetBkColor(hdcGfxLocked, textBgColor);
    }
}

void TextRenderGdi::Lock() {
    ReportIf(hdcGfxLocked);
    Region r;
    Status st = gfx->GetClip(&r); // must call before GetHDC(), which locks gfx
    ReportIf(st != Ok);
    HRGN hrgn = r.GetHRGN(gfx);

    hdcGfxLocked = gfx->GetHDC();
    SelectClipRgn(hdcGfxLocked, hrgn);
    DeleteObject(hrgn);

    SelectFont(hdcGfxLocked, currFont);
    ::SetTextColor(hdcGfxLocked, textColor);
    ::SetBkColor(hdcGfxLocked, textBgColor);
}

void TextRenderGdi::Unlock() {
    ReportIf(!hdcGfxLocked);
    gfx->ReleaseHDC(hdcGfxLocked);
    hdcGfxLocked = nullptr;
}

void TextRenderGdi::Draw(Str s, const RectF bb, bool isRtl) {
    ReportIf(!hdcGfxLocked); // hasn't been Lock()ed
    TempWStr buf = ToWStrTemp(s);
    int x = (int)bb.x;
    int y = (int)bb.y;
    uint opts = ETO_OPAQUE;
    if (isRtl) {
        opts = opts | ETO_RTLREADING;
    }
    ExtTextOut(hdcGfxLocked, x, y, opts, nullptr, buf.s, (uint)buf.len, nullptr);
}

// draws with gdi+, straight to the Graphics
struct TextRenderGdiplus : WinTextRender {
    TextMeasureAlgorithm measureAlgo = nullptr;
    Gdiplus::Brush* textColorBrush = nullptr;

    ~TextRenderGdiplus() override { delete textColorBrush; }

    void SetFont(PlatformFont* font) override;
    RectF Measure(Str s) override;
    void SetTextColor(Color col) override;
    void SetTextBgColor(Color) override {}
    void Lock() override {}
    void Unlock() override {}
    void Draw(Str s, RectF bb, bool isRtl) override;
};

void TextRenderGdiplus::SetFont(PlatformFont* font) {
    ReportIf(!font->gdiFont);
    currFont = font;
}

RectF TextRenderGdiplus::Measure(Str s) {
    if (!currFont) {
        ReportIf(true);
        return {};
    }
    return MeasureText(gfx, currFont->gdiFont, ToWStrTemp(s), measureAlgo);
}

void TextRenderGdiplus::SetTextColor(Color col) {
    if (textColor == col) {
        return;
    }
    textColor = col;
    delete textColorBrush;
    textColorBrush = new SolidBrush(GdiRgbFromColor(col));
}

static Gdiplus::PointF ToGdipPointF(const PointF p) {
    return {p.x, p.y};
}

void TextRenderGdiplus::Draw(Str s, const RectF bb, bool isRtl) {
    TempWStr buf = ToWStrTemp(s);
    Gdiplus::PointF pos = ToGdipPointF(bb.TL());
    if (!isRtl) {
        gfx->DrawString(buf.s, (INT)buf.len, currFont->gdiFont, pos, nullptr, textColorBrush);
    } else {
        StringFormat rtl;
        rtl.SetFormatFlags(StringFormatFlagsDirectionRightToLeft);
        pos.X += bb.dx;
        gfx->DrawString(buf.s, (INT)buf.len, currFont->gdiFont, pos, &rtl, textColorBrush);
    }
}

// Note: this is not meant to be used, just exists so that I can see perf
// compared to the other implementations. Draws with gdi into a bitmap of its
// own, which Unlock() blits onto the Graphics
struct TextRenderHdc : WinTextRender {
    BITMAPINFO bmi{};

    HDC hdc = nullptr;
    HBITMAP bmp = nullptr;
    void* bmpData = nullptr;

    ~TextRenderHdc() override;

    void SetFont(PlatformFont* font) override;
    RectF Measure(Str s) override;
    void SetTextColor(Color col) override;
    void SetTextBgColor(Color col) override;
    void Lock() override;
    void Unlock() override;
    void Draw(Str s, RectF bb, bool isRtl) override;
};

TextRenderHdc::~TextRenderHdc() {
    DeleteObject(bmp);
    DeleteDC(hdc);
}

void TextRenderHdc::SetFont(PlatformFont* font) {
    ReportIf(!hdc);
    // I'm not sure how expensive SelectFont() is so avoid it just in case
    if (currFont == font) {
        return;
    }
    currFont = font;
    SelectFont(hdc, font->GetHFont());
}

RectF TextRenderHdc::Measure(Str s) {
    ReportIf(!currFont);
    ReportIf(!hdc);
    Size size = HdcGetTextExtentPoint32(hdc, s);
    RectF res(0.0f, 0.0f, (float)size.dx, (float)size.dy);
    return res;
}

void TextRenderHdc::SetTextColor(Color col) {
    ReportIf(!hdc);
    if (textColor == col) {
        return;
    }
    textColor = col;
    ::SetTextColor(hdc, col);
}

void TextRenderHdc::SetTextBgColor(Color col) {
    ReportIf(!hdc);
    if (textBgColor == col) {
        return;
    }
    textBgColor = col;
    ::SetBkColor(hdc, textBgColor);
}

void TextRenderHdc::Lock() {
    int dx = bmi.bmiHeader.biWidth;
    int dy = bmi.bmiHeader.biHeight;
    ZeroMemory(bmpData, (size_t)dx * dy * 4);
}

void TextRenderHdc::Unlock() {
    Bitmap* b = Bitmap::FromBITMAPINFO(&bmi, bmpData);
    gfx->DrawImage(b, 0, 0);
    delete b;
}

void TextRenderHdc::Draw(Str s, const RectF bb, bool /* isRtl */) {
    ReportIf(!hdc);
    int x = (int)bb.x;
    int y = (int)bb.y;
    uint opts = ETO_OPAQUE;
    HdcExTextOut(hdc, Point(x, y), opts, Rect(), s);
}

// --- creating them

static TextRenderGdi* NewTextRenderGdi(Graphics* gfx) {
    TextRenderGdi* res = new TextRenderGdi();
    res->gfx = gfx;
    // default to red to make mistakes stand out
    res->SetTextColor(kColRed);
    res->CreateHdcForTextMeasure(); // could do lazily, but that's more things to track, so not
                                    // worth it
    return res;
}

static TextRenderGdiplus* NewTextRenderGdiplus(Graphics* gfx, TextMeasureAlgorithm measureAlgo) {
    TextRenderGdiplus* res = new TextRenderGdiplus();
    res->gfx = gfx;
    res->measureAlgo = measureAlgo ? measureAlgo : MeasureTextAccurate;
    // default to red to make mistakes stand out
    res->SetTextColor(kColRed);
    return res;
}

static TextRenderHdc* NewTextRenderHdc(Graphics* gfx, int dx, int dy) {
    TextRenderHdc* res = new TextRenderHdc();
    res->gfx = gfx;

    HDC hdc = gfx->GetHDC();
    res->hdc = CreateCompatibleDC(hdc);
    gfx->ReleaseHDC(hdc);

    res->bmi.bmiHeader.biSize = sizeof(res->bmi.bmiHeader);
    res->bmi.bmiHeader.biWidth = dx;
    res->bmi.bmiHeader.biHeight = dy;
    res->bmi.bmiHeader.biPlanes = 1;
    res->bmi.bmiHeader.biBitCount = 32;
    res->bmi.bmiHeader.biCompression = BI_RGB;
    res->bmi.bmiHeader.biSizeImage = dx * dy * 4; // doesn't seem necessary?

    res->bmp = CreateDIBSection(res->hdc, &res->bmi, DIB_RGB_COLORS, &res->bmpData, nullptr, 0);
    if (!res->bmp) {
        delete res;
        return nullptr;
    }

    if (res->bmpData) {
        size_t n = (size_t)dx * (size_t)dy * 4;
        ZeroMemory(res->bmpData, n);
    }
    SelectObject(res->hdc, res->bmp);

    // default to red to make mistakes stand out
    res->SetTextColor(kColRed);
    return res;
}

PlatformTextRender* CreateGdiplusTextRender(Graphics* gfx) {
    return NewTextRenderGdiplus(gfx, nullptr);
}

// called by CreatePlatformTextRender() in PlatformText.cpp. That caller only
// measures, so the Graphics comes from the per-thread cache
PlatformTextRender* CreateNativeTextRender(PlatformTextMeasureMethod method) {
    Graphics* gfx = AllocGraphicsForMeasureText();
    // only matters for the hdc renderer, which is not meant to be used
    constexpr int kDx = 10;
    constexpr int kDy = 10;
    WinTextRender* res = nullptr;
    switch (method) {
        case PlatformTextMeasureMethod::Gdiplus:
            res = NewTextRenderGdiplus(gfx, nullptr);
            break;
        case PlatformTextMeasureMethod::GdiplusQuick:
            res = NewTextRenderGdiplus(gfx, MeasureTextQuick);
            break;
        case PlatformTextMeasureMethod::Gdi:
            res = NewTextRenderGdi(gfx);
            break;
        case PlatformTextMeasureMethod::Hdc:
            res = NewTextRenderHdc(gfx, kDx, kDy);
            break;
        case PlatformTextMeasureMethod::Stub:
            break;
    }
    if (!res) {
        ReportIf(true);
        res = NewTextRenderGdiplus(gfx, nullptr);
    }
    res->ownsGfx = true;
    return res;
}
