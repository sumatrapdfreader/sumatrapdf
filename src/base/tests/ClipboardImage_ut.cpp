/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/Pixmap.h"
#include "base/GdiPlusUtil.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

// left half opaque red, right half fully transparent (straight alpha)
static Pixmap* MakeHalfTransparent(int w, int h) {
    Pixmap* p = AllocPixmap(w, h, PixmapFormat::BGRA8, false);
    if (!p) {
        return nullptr;
    }
    for (int y = 0; y < h; y++) {
        u8* d = p->data + ((size_t)y * p->stride);
        for (int x = 0; x < w; x++, d += 4) {
            bool opaque = x < w / 2;
            d[0] = 0;                // b
            d[1] = 0;                // g
            d[2] = opaque ? 220 : 0; // r
            d[3] = opaque ? 255 : 0; // a
        }
    }
    return p;
}

static Str ReadClipboardFormat(UINT fmt) {
    HANDLE h = GetClipboardData(fmt);
    if (!h) {
        return {};
    }
    void* data = GlobalLock(h);
    if (!data) {
        return {};
    }
    size_t n = GlobalSize(h);
    Str res = str::Dup(Str((const char*)data, (int)n));
    GlobalUnlock(h);
    return res;
}

// the IHDR colour type of a PNG: 6 is truecolour + alpha
static int PngColorType(Str png) {
    if (len(png) < 26) {
        return -1;
    }
    const u8* d = (const u8*)png.s;
    static const u8 sig[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (memcmp(d, sig, 8) != 0) {
        return -1;
    }
    if (memcmp(d + 12, "IHDR", 4) != 0) {
        return -1;
    }
    return d[25];
}

// an image with transparency has to reach the clipboard in a format that can
// carry alpha, or pasting it into an image editor loses the transparency
// (issues #5598, #5844)
static void TransparentImageTest() {
    UINT cfPng = RegisterClipboardFormatW(L"PNG");
    utassert(cfPng != 0);

    Pixmap* p = MakeHalfTransparent(8, 4);
    utassert(p != nullptr);
    bool ok = CopyPixmapToClipboard(p, false);
    utassert(ok);

    utassert(IsClipboardFormatAvailable(cfPng));
    utassert(IsClipboardFormatAvailable(CF_DIBV5));
    // still there for Paint / Word, which don't understand the other two
    utassert(IsClipboardFormatAvailable(CF_BITMAP));

    utassert(OpenClipboard(nullptr));
    Str png = ReadClipboardFormat(cfPng);
    utassert(len(png) > 26);
    utassert(PngColorType(png) == 6);

    Str dib = ReadClipboardFormat(CF_DIBV5);
    utassert(len(dib) > (int)sizeof(BITMAPV5HEADER));
    auto* bi = (BITMAPV5HEADER*)dib.s;
    utassert(bi->bV5BitCount == 32);
    utassert(bi->bV5AlphaMask == 0xff000000);
    utassert(bi->bV5Width == p->width);
    utassert(bi->bV5Height == -p->height); // top-down
    const u8* bits = (const u8*)dib.s + sizeof(BITMAPV5HEADER);
    utassert(bits[3] == 255);                            // first pixel: opaque
    utassert(bits[((size_t)p->width - 1) * 4 + 3] == 0); // last pixel of row 0: transparent
    CloseClipboard();

    FreePixmap(p);
}

// an opaque image keeps the old behaviour: CF_BITMAP only, no need to publish
// formats that only exist to carry an alpha channel
static void OpaqueImageTest() {
    UINT cfPng = RegisterClipboardFormatW(L"PNG");
    Pixmap* p = AllocPixmap(4, 4, PixmapFormat::BGRA8, false);
    utassert(p != nullptr);
    memset(p->data, 0xff, (size_t)p->stride * (size_t)p->height);
    utassert(CopyPixmapToClipboard(p, false));
    utassert(IsClipboardFormatAvailable(CF_BITMAP));
    utassert(!IsClipboardFormatAvailable(cfPng));
    FreePixmap(p);
}

// the exact chain CmdCopyImage takes: the engine hands out a RenderedBitmap,
// which becomes a Pixmap again before it reaches the clipboard. The alpha has
// to survive the round trip, and premultiplied pixels have to come back
// straight (a DIB section's alpha is straight)
static void RoundTripThroughRenderedBitmapTest() {
    UINT cfPng = RegisterClipboardFormatW(L"PNG");
    Pixmap* p = MakeHalfTransparent(8, 4);
    utassert(p != nullptr);
    p->premultiplied = true; // what mupdf gives us

    RenderedBitmap* rb = RenderedBitmapFromPixmap(p); // takes ownership of p
    utassert(rb != nullptr);
    Pixmap* back = PixmapFromRenderedBitmap(rb); // takes ownership of rb
    utassert(back != nullptr);
    utassert(back->format == PixmapFormat::BGRA8);
    utassert(back->data != nullptr);
    utassert(back->data[3] == 255);                               // opaque half
    utassert(back->data[((size_t)back->width - 1) * 4 + 3] == 0); // transparent half

    utassert(CopyPixmapToClipboard(back, false));
    utassert(IsClipboardFormatAvailable(cfPng));
    FreePixmap(back);
}

// Area-copy then paste-as-stamp (GetClipboardImageAsPixmap). Widths that are
// not a multiple of 4 used to shear when the clipboard bitmap was read as
// 24bpp (issue #6059).
static void ClipboardStampRoundTripTest() {
    const int ws[] = {1, 2, 3, 5, 7, 8};
    for (int wi = 0; wi < dimof(ws); wi++) {
        int w = ws[wi];
        int h = 3;
        Pixmap* src = AllocPixmap(w, h, PixmapFormat::BGRA8, false);
        utassert(src != nullptr);
        for (int y = 0; y < h; y++) {
            u8* d = src->data + ((size_t)y * src->stride);
            for (int x = 0; x < w; x++, d += 4) {
                d[0] = (u8)(x * 17 + y); // b
                d[1] = (u8)(y * 40 + 10);
                d[2] = (u8)(x * 40 + 20);
                d[3] = 255;
            }
        }
        utassert(CopyPixmapToClipboard(src, false));
        Pixmap* got = GetClipboardImageAsPixmap();
        utassert(got != nullptr);
        utassert(got->width == w);
        utassert(got->height == h);
        utassert(got->format == PixmapFormat::BGRA8);
        for (int y = 0; y < h; y++) {
            const u8* s = src->data + ((size_t)y * src->stride);
            const u8* g = got->data + ((size_t)y * got->stride);
            for (int x = 0; x < w; x++, s += 4, g += 4) {
                utassert(s[0] == g[0]);
                utassert(s[1] == g[1]);
                utassert(s[2] == g[2]);
            }
        }
        FreePixmap(got);
        FreePixmap(src);
    }
}

void ClipboardImageTest() {
    ScopedGdiPlus gdiPlus;
    TransparentImageTest();
    OpaqueImageTest();
    RoundTripThroughRenderedBitmapTest();
    ClipboardStampRoundTripTest();
}
