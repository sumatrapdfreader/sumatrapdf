/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "gui/PlatformFont.h"
#if OS_WIN
#include "base/AutoWin.h"
#include "base/GdiPlusUtil.h"
#include "base/Win.h"
#elif OS_LINUX
#include <pango/pangocairo.h>
#elif OS_DARWIN
#include "base/MacTypesHide.h"
#include <CoreText/CoreText.h>
#include "base/MacTypesShow.h"
#endif

#include "gui/PlatformText.h"

// TODO: not quite sure why measuring a space directly gives a (much) bigger
// size than the difference below, which looks like better spacing to me
float PlatformTextRender::GetSpaceDx() {
    float l1 = Measure(StrL("wa")).dx;
    float l2 = Measure(StrL("w a")).dx;
    return l2 - l1;
}

int PlatformTextRender::StringLenForWidth(Str s, float dx, float sWidth) {
    float fullWidth = sWidth >= 0 ? sWidth : Measure(s).dx;
    if (fullWidth <= dx) {
        return len(s);
    }
    // make the best guess of the length that fits
    int sLen = len(s);
    int n = (int)((dx / fullWidth) * (float)sLen);
    ReportIf(n > sLen);
    // the guess can land inside a utf-8 sequence; back up to its start
    n = Utf8CodepointStartByte(s, n);
    if (n == 0) {
        // nothing fits in the remaining space; caller flushes the line and
        // re-lays the run at full width. Don't Measure an empty string.
        return 0;
    }
    RectF r = Measure(Str(s.s, n));
    // find the length of s that fits within dx iff the next codepoint makes it
    // exceed dx
    int dir = 1; // increasing length
    if (r.dx > dx) {
        dir = -1; // decreasing length
    }
    while (n > 0) {
        int prevN = n;
        if (dir == 1) {
            Utf8CodepointNext(s, n);
        } else {
            Utf8CodepointPrev(s, n);
        }
        if (n == prevN) {
            break;
        }
        r = Measure(Str(s.s, n));
        if (1 == dir) {
            // if advancing length, we know that previous string did fit, so if
            // the new one doesn't fit, the previous length was the right one
            if (r.dx > dx) {
                return prevN;
            }
        } else {
            // if decreasing length, we know that previous string didn't fit, so if
            // the one one fits, it's of the correct length
            if (r.dx <= dx) {
                return n;
            }
        }
    }
    // even a single codepoint is longer than available space
    return 0;
}

// Guesses sizes and draws nothing when there is no platform text engine.
struct StubTextRender : PlatformTextRender {
    PlatformFont* currFont = nullptr;

    // font sizes are in points; measure in 96 dpi pixels like the native renderers
    float CurrFontSize() const {
        if (currFont && currFont->GetSize() > 0) {
            return currFont->GetSize() * 96.0f / 72.0f;
        }
        return 12.5f;
    }

    float AverageCharDx() const { return CurrFontSize() * 0.55f; }

    void SetFont(PlatformFont* font) override { currFont = font; }

    float GetCurrFontLineSpacing() override { return CurrFontSize() * 1.25f; }

    // a rough guess, so measure by codepoint rather than by utf-8 byte
    RectF Measure(Str s) override {
        return {0, 0, (float)Utf8CodepointCount(s) * AverageCharDx(), GetCurrFontLineSpacing()};
    }

    void SetTextColor(Color) override {}
    void SetTextBgColor(Color) override {}
    void Lock() override {}
    void Unlock() override {}
    void Draw(Str, RectF, bool) override {}
};

#if OS_WIN
// defined in the OS_WIN section below
PlatformTextRender* CreateNativeTextRender();
#elif OS_LINUX
PlatformTextRender* CreateNativeTextRender();
#elif OS_DARWIN
PlatformTextRender* CreateNativeTextRender();
#endif

PlatformTextRender* CreatePlatformTextRender() {
#if OS_WIN || OS_LINUX || OS_DARWIN
    return CreateNativeTextRender();
#else
    return new StubTextRender();
#endif
}

#if OS_LINUX
struct PangoTextRender : PlatformTextRender {
    cairo_t* cairo = nullptr;
    PangoLayout* layout = nullptr;
    PlatformFont* currFont = nullptr;
    Color textColor = kColBlack;

    explicit PangoTextRender(cairo_t* cr) : cairo(cr) {
        if (cairo) {
            layout = pango_cairo_create_layout(cairo);
            return;
        }
        PangoFontMap* map = pango_cairo_font_map_get_default();
        PangoContext* context = pango_font_map_create_context(map);
        layout = pango_layout_new(context);
        g_object_unref(context);
    }

    ~PangoTextRender() override {
        if (layout) {
            g_object_unref(layout);
        }
    }

    void SetFont(PlatformFont* font) override {
        currFont = font;
        pango_layout_set_font_description(layout, font ? (PangoFontDescription*)font->nativeFont : nullptr);
    }

    float GetCurrFontLineSpacing() override { return (float)PlatformFontLineHeight(currFont); }

    RectF Measure(Str s) override {
        pango_layout_set_width(layout, -1);
        pango_layout_set_text(layout, s.s, len(s));
        int dx = 0;
        int dy = 0;
        pango_layout_get_pixel_size(layout, &dx, &dy);
        return {0, 0, (float)dx, (float)dy};
    }

    void SetTextColor(Color col) override { textColor = col; }
    void SetTextBgColor(Color) override {}
    void Lock() override {}
    void Unlock() override {}

    void Draw(Str s, RectF bb, bool isRtl) override {
        if (!cairo || !currFont || len(s) == 0) {
            return;
        }
        PangoContext* context = pango_layout_get_context(layout);
        pango_context_set_base_dir(context, isRtl ? PANGO_DIRECTION_RTL : PANGO_DIRECTION_LTR);
        pango_layout_context_changed(layout);
        pango_layout_set_auto_dir(layout, false);
        pango_layout_set_width(layout, -1);
        pango_layout_set_text(layout, s.s, len(s));

        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        UnpackColor(textColor, r, g, b);
        cairo_set_source_rgb(cairo, r / 255.0, g / 255.0, b / 255.0);
        cairo_move_to(cairo, bb.x, bb.y);
        pango_cairo_show_layout(cairo, layout);
    }
};

PlatformTextRender* CreateNativeTextRender() {
    return new PangoTextRender(nullptr);
}

PlatformTextRender* CreateCairoTextRender(cairo_t* cairo) {
    return new PangoTextRender(cairo);
}
#endif

#if OS_DARWIN
static CFStringRef NewCoreTextString(Str s) {
    return CFStringCreateWithBytes(kCFAllocatorDefault, (const UInt8*)s.s, len(s), kCFStringEncodingUTF8, false);
}

struct CoreTextRender : PlatformTextRender {
    CGContextRef context = nullptr;
    PlatformFont* currFont = nullptr;
    Color textColor = kColBlack;

    explicit CoreTextRender(CGContextRef context) : context(context) {}

    void SetFont(PlatformFont* font) override { currFont = font; }
    float GetCurrFontLineSpacing() override { return (float)PlatformFontLineHeight(currFont); }

    RectF Measure(Str s) override {
        Size size = PlatformFontMeasureText(currFont, s);
        return {0, 0, (float)size.dx, (float)size.dy};
    }

    void SetTextColor(Color col) override { textColor = col; }
    void SetTextBgColor(Color) override {}
    void Lock() override {}
    void Unlock() override {}

    void Draw(Str s, RectF bb, bool isRtl) override {
        if (!context || !currFont || !currFont->nativeFont || len(s) == 0) {
            return;
        }
        CFStringRef text = NewCoreTextString(s);
        if (!text) {
            return;
        }
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        UnpackColor(textColor, r, g, b);
        CGColorRef color = CGColorCreateGenericRGB(r / 255.0, g / 255.0, b / 255.0, 1);
        CTWritingDirection direction = isRtl ? kCTWritingDirectionRightToLeft : kCTWritingDirectionLeftToRight;
        CTParagraphStyleSetting setting = {kCTParagraphStyleSpecifierBaseWritingDirection, sizeof(direction),
                                           &direction};
        CTParagraphStyleRef paragraph = CTParagraphStyleCreate(&setting, 1);
        const void* keys[] = {kCTFontAttributeName, kCTForegroundColorAttributeName, kCTParagraphStyleAttributeName};
        const void* values[] = {(CTFontRef)currFont->nativeFont, color, paragraph};
        CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault, keys, values, dimof(keys),
                                                   &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFAttributedStringRef attributed = CFAttributedStringCreate(kCFAllocatorDefault, text, attrs);
        CTLineRef line = CTLineCreateWithAttributedString(attributed);
        CGFloat ascent = 0;
        CTLineGetTypographicBounds(line, &ascent, nullptr, nullptr);
        CGContextSetTextPosition(context, bb.x, bb.y + ascent);
        CTLineDraw(line, context);
        CFRelease(line);
        CFRelease(attributed);
        CFRelease(attrs);
        CFRelease(paragraph);
        CGColorRelease(color);
        CFRelease(text);
    }
};

PlatformTextRender* CreateNativeTextRender() {
    return new CoreTextRender(nullptr);
}

PlatformTextRender* CreateCoreTextRender(CGContextRef context) {
    return new CoreTextRender(context);
}
#endif

#if !OS_WIN
void PlatformFontDestroy() {
    // portable renderers keep no global cache
}
#endif

#if OS_WIN

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

    ThreadId threadId = GetCurrentThreadId();
    int refCount = 0;
    bool retired = false;

    Graphics* gfx = nullptr;
    Bitmap* bmp = nullptr;
    u8 data[bmpDx * bmpDy * 4]{};

    bool Create();
    ~GraphicsCacheEntry();
};

struct GraphicsCache {
    // Borrowers can outlive the application's GDI+ scope.
    AutoGdiPlusShutdown gdiPlus;
    Vec<GraphicsCacheEntry*> entries;
};

static Mutex gGraphicsCacheMutex;
static GraphicsCache* gGraphicsCache = nullptr;

bool GraphicsCacheEntry::Create() {
    // Measuring only needs a small backing bitmap.
    bmp = new Bitmap(bmpDx, bmpDy, stride, PixelFormat32bppARGB, data);
    if (!bmp || bmp->GetLastStatus() != Ok) {
        return false;
    }
    gfx = new Graphics((Image*)bmp);
    if (!gfx || gfx->GetLastStatus() != Ok) {
        return false;
    }
    InitGraphicsMode(gfx);
    return true;
}

GraphicsCacheEntry::~GraphicsCacheEntry() {
    ReportIf(refCount != 0);
    delete gfx;
    delete bmp;
}

static void RemoveGraphicsCacheEntry(int i) {
    GraphicsCacheEntry* entry = gGraphicsCache->entries[i];
    VecRemoveAt(gGraphicsCache->entries, i);
    delete entry;
}

static void DeleteEmptyGraphicsCache() {
    if (len(gGraphicsCache->entries) != 0) {
        return;
    }
    delete gGraphicsCache;
    gGraphicsCache = nullptr;
}

static Graphics* AllocGraphicsForMeasureText() {
    ScopedMutex lock(&gGraphicsCacheMutex);
    if (!gGraphicsCache) {
        gGraphicsCache = new GraphicsCache();
    }

    ThreadId threadId = GetCurrentThreadId();
    for (GraphicsCacheEntry* e : gGraphicsCache->entries) {
        if (!e->retired && e->threadId == threadId) {
            e->refCount++;
            return e->gfx;
        }
    }

    auto* ce = new GraphicsCacheEntry();
    if (!ce->Create() || !VecAppend(gGraphicsCache->entries, ce)) {
        delete ce;
        DeleteEmptyGraphicsCache();
        return nullptr;
    }
    ce->refCount = 1;
    constexpr int kMaxCacheEntries = 64;
    if (len(gGraphicsCache->entries) <= kMaxCacheEntries) {
        return ce->gfx;
    }

    // Evict the oldest idle entry. Borrowed entries may exceed the limit.
    for (int i = 0; i < len(gGraphicsCache->entries) - 1; i++) {
        GraphicsCacheEntry* e = gGraphicsCache->entries[i];
        if (e->refCount == 0) {
            RemoveGraphicsCacheEntry(i);
            break;
        }
    }
    return ce->gfx;
}

static void FreeGraphicsForMeasureText(Graphics* gfx) {
    ScopedMutex lock(&gGraphicsCacheMutex);
    if (!gGraphicsCache) {
        ReportIf(true);
        return;
    }
    for (int i = 0; i < len(gGraphicsCache->entries); i++) {
        GraphicsCacheEntry* e = gGraphicsCache->entries[i];
        if (e->gfx != gfx) {
            continue;
        }
        ReportIf(e->threadId != GetCurrentThreadId());
        if (e->refCount <= 0) {
            ReportIf(true);
            return;
        }
        if (--e->refCount == 0 && e->retired) {
            RemoveGraphicsCacheEntry(i);
            DeleteEmptyGraphicsCache();
        }
        return;
    }
    ReportIf(true);
}

void PlatformFontDestroy() {
    ScopedMutex lock(&gGraphicsCacheMutex);
    if (!gGraphicsCache) {
        return;
    }
    for (int i = len(gGraphicsCache->entries) - 1; i >= 0; i--) {
        GraphicsCacheEntry* e = gGraphicsCache->entries[i];
        if (e->refCount == 0) {
            RemoveGraphicsCacheEntry(i);
        } else {
            e->retired = true;
        }
    }
    DeleteEmptyGraphicsCache();
}

#if IS_DEBUG
bool PlatformTextCache_UnitTest() {
    auto isCached = [](Graphics* gfx) {
        ScopedMutex lock(&gGraphicsCacheMutex);
        if (!gGraphicsCache) {
            return false;
        }
        for (GraphicsCacheEntry* entry : gGraphicsCache->entries) {
            if (entry->gfx == gfx) {
                return true;
            }
        }
        return false;
    };

    Graphics* first = AllocGraphicsForMeasureText();
    Graphics* shared = AllocGraphicsForMeasureText();
    if (!first || first != shared) {
        return false;
    }

    ThreadHandle thread = StartThread(MkFunc0Void(PlatformFontDestroy), StrL("TextCacheShutdownTest"));
    if (!JoinThread(&thread, -1) || !isCached(first)) {
        return false;
    }

    Graphics* next = AllocGraphicsForMeasureText();
    bool ok = next && next != first && first->GetDpiX() > 0;
    if (next) {
        FreeGraphicsForMeasureText(next);
    }
    PlatformFontDestroy();
    ok = ok && !isCached(next);
    FreeGraphicsForMeasureText(first);
    ok = ok && isCached(shared) && shared->GetDpiX() > 0;
    FreeGraphicsForMeasureText(shared);
    return ok && !gGraphicsCache;
}
#endif

// --- the renderers

// What both implementations have in common: the Graphics they draw to (or,
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

    float GetCurrFontLineSpacing() override {
        if (!currFont || !currFont->gdiFont) {
            return 0.f;
        }
        return currFont->gdiFont->GetHeight(gfx);
    }
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

    if (currFont) {
        SelectFont(hdcGfxLocked, currFont->GetHFont());
    }
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
    return MeasureTextAccurate(gfx, currFont->gdiFont, ToWStrTemp(s));
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

PlatformTextRender* CreateGdiplusTextRender(Graphics* gfx) {
    TextRenderGdiplus* res = new TextRenderGdiplus();
    res->gfx = gfx;
    // default to red to make mistakes stand out
    res->SetTextColor(kColRed);
    return res;
}

// called by CreatePlatformTextRender() in PlatformText.cpp. That caller only
// measures, so the Graphics comes from the per-thread cache
PlatformTextRender* CreateNativeTextRender() {
    Graphics* gfx = AllocGraphicsForMeasureText();
    if (!gfx) {
        return new StubTextRender();
    }
    WinTextRender* res = NewTextRenderGdi(gfx);
    res->ownsGfx = true;
    return res;
}

#endif
