/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/CmdLineArgs.h"
#include "base/Pixmap.h"
#include "base/ScopedWin.h"
#include "base/Win.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

// Round-trip QuoteCmdLineArgTemp through CommandLineToArgvW (ParseCmdLine).
// The GHSA-xvxg-cwmx-hr7j breakout payload must stay a single argv element.
static void QuoteCmdLineArgTest() {
    auto roundTripOne = [](Str input) {
        TempStr quoted = QuoteCmdLineArgTemp(input);
        utassert(quoted.s != nullptr);
        // ParseCmdLine uses CommandLineToArgvW; prefix a dummy argv[0].
        TempStr cmdLine = fmt("exe %s", quoted);
        StrNode* args = ParseCmdLine(cmdLine);
        defer {
            FreeStrNode(nullptr, args);
        };
        utassert(args != nullptr);
        utassert(str::Eq(args->s, StrL("exe")));
        if (len(input) == 0) {
            // `exe ""` → only "exe" after empty-token skip in ParseCmdLine
            utassert(args->next == nullptr);
            return;
        }
        utassert(args->next != nullptr);
        utassert(args->next->next == nullptr);
        utassert(str::Eq(args->next->s, input));
    };

    roundTripOne(StrL("hello"));
    roundTripOne(StrL("hello world"));
    roundTripOne(StrL("say \"hi\""));
    roundTripOne(StrL("path\\with\\backslashes"));
    roundTripOne(StrL("trailing\\"));
    roundTripOne(StrL("trailing\\\\"));
    // PoC from the advisory: naive " -> \" turns this into a breakout.
    roundTripOne(StrL("X\\\" --always-approve "));
    roundTripOne(StrL("some text \\\" --dangerously-skip-permissions "));
    roundTripOne(StrL("Text: X\\\" --model evil"));
    roundTripOne(StrL(""));
    roundTripOne(StrL("a\\\"b\\\"c"));
    roundTripOne(StrL("ends with quote\""));
    roundTripOne(StrL("\\"));
    roundTripOne(StrL("\\\""));

    // Explicit expected encodings for the breakout cases
    utassert(str::Eq(QuoteCmdLineArgTemp(StrL("X\\\" --always-approve ")), StrL("\"X\\\\\\\" --always-approve \"")));
    utassert(str::Eq(QuoteCmdLineArgTemp(StrL("trailing\\")), StrL("\"trailing\\\\\"")));
    utassert(str::Eq(QuoteCmdLineArgTemp(StrL("a b")), StrL("\"a b\"")));
    utassert(str::Eq(QuoteCmdLineArgTemp(StrL("")), StrL("\"\"")));
    utassert(QuoteCmdLineArgTemp({}).s == nullptr);
}

static u8 RecolorLerp(u8 from, u8 to, int t) {
    int n = (t * ((int)to - from)) + 128;
    n += n >> 8;
    return (u8)(from + (n >> 8));
}

static void FillLinkAaPixels(u8* p, int bpp) {
    // paper
    p[0] = 255;
    p[1] = 255;
    p[2] = 255;
    // solid blue URL ink
    p[bpp] = 220;
    p[bpp + 1] = 0;
    p[bpp + 2] = 0;
    // ~50% coverage (R/G halfway to paper)
    p[2 * bpp] = 255;
    p[2 * bpp + 1] = 128;
    p[2 * bpp + 2] = 128;
}

static void CheckLinkAaPixels(u8* p, int bpp) {
    Color bg = MkRgb(0x1e, 0x1e, 0x1e);
    Color link = MkRgb(0x50, 0xa0, 0xff);

    u8 lr, lg, lb, br, bgc, bb;
    UnpackColor(link, lr, lg, lb);
    UnpackColor(bg, br, bgc, bb);

    // paper -> background
    utassert(p[0] == bb && p[1] == bgc && p[2] == br);
    // solid ink -> link color
    utassert(p[bpp] == lb && p[bpp + 1] == lg && p[bpp + 2] == lr);
    // fringe interpolates toward background, not snapped to link
    utassert(p[2 * bpp] == RecolorLerp(lb, bb, 128));
    utassert(p[2 * bpp + 1] == RecolorLerp(lg, bgc, 128));
    utassert(p[2 * bpp + 2] == RecolorLerp(lr, br, 128));
}

static void RecolorLinkAaTest() {
    // blue-on-white AA must not snap to a solid link color (issue #5911)
    Color text = MkRgb(0xf0, 0xf0, 0xf0);
    Color bg = MkRgb(0x1e, 0x1e, 0x1e);
    Color link = MkRgb(0x50, 0xa0, 0xff);

    Pixmap* heap = AllocPixmap(3, 1, PixmapFormat::BGR8);
    utassert(heap && heap->data);
    FillLinkAaPixels(heap->data, 3);
    RecolorPixmap(heap, text, bg, link);
    CheckLinkAaPixels(heap->data, 3);
    FreePixmap(heap);

    // live page tiles are DIB-backed (UpdateBitmapColors)
    Pixmap* dib = AllocPixmapDIB(3, 1);
    utassert(dib && dib->data && dib->hbmp);
    FillLinkAaPixels(dib->data, 4);
    RecolorPixmap(dib, text, bg, link);
    CheckLinkAaPixels(dib->data, 4);
    FreePixmap(dib);
}

// 16x16 red square in the middle, transparent corners (AND mask).
static HICON MakeMaskedRedIcon() {
    const int n = 16;
    Pixmap* color = AllocPixmapDIB(n, n);
    if (!color || !color->data) {
        FreePixmap(color);
        return nullptr;
    }
    memset(color->data, 0, (size_t)color->stride * (size_t)n);
    for (int y = 4; y < 12; y++) {
        u8* d = color->data + ((size_t)y * color->stride) + (4 * 4);
        for (int x = 4; x < 12; x++, d += 4) {
            d[0] = 0;
            d[1] = 0;
            d[2] = 255;
            d[3] = 255;
        }
    }
    // 1-bit mask, MSB is leftmost. White (1) = transparent, black (0) = opaque.
    u8 maskBits[16 * 2];
    memset(maskBits, 0xFF, sizeof(maskBits));
    for (int y = 4; y < 12; y++) {
        maskBits[y * 2] = 0xF0;
        maskBits[y * 2 + 1] = 0x0F;
    }
    HBITMAP hbmMask = CreateBitmap(n, n, 1, 1, maskBits);
    if (!hbmMask) {
        FreePixmap(color);
        return nullptr;
    }
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = hbmMask;
    ii.hbmColor = color->hbmp;
    HICON hicon = CreateIconIndirect(&ii);
    DeleteObject(hbmMask);
    FreePixmap(color);
    return hicon;
}

static void PixmapFromHICONAlphaTest() {
    HICON hicon = MakeMaskedRedIcon();
    utassert(hicon);
    Pixmap* px = PixmapFromHICON(hicon);
    DestroyIcon(hicon);
    utassert(px && px->data && px->hasAlpha);
    utassert(px->width == 16 && px->height == 16);
    utassert(px->format == PixmapFormat::BGRA8);
    const u8* corner = px->data;
    utassert(corner[3] == 0);
    const u8* center = px->data + ((size_t)8 * px->stride) + (8 * 4);
    utassert(center[3] > 128);
    utassert(center[2] > 128);
    FreePixmap(px);
}

void WinUtilTest() {
    ScopedCom comScope;

    QuoteCmdLineArgTest();
    RecolorLinkAaTest();
    PixmapFromHICONAlphaTest();

    {
        Str string = StrL("abcde");
        auto strm = CreateStreamFromData(string);
        ScopedComPtr<IStream> stream(strm);
        utassert(stream);
        Str data = ReadIStream(stream);
        utassert((u8*)data.s);
        utassert(string.len == data.len);
        utassert(data.s[data.len] == 0);
        utassert(data.s[data.len + 1] == 0);
        Str s = data;
        utassert(str::Eq(s, string));
        str::Free(data);
    }

    {
        WStr string = L"abcde";
        size_t stringSize = string.len * sizeof(WCHAR);
        auto strm = CreateStreamFromData(Str((char*)string.s, (int)stringSize));
        ScopedComPtr<IStream> stream(strm);
        utassert(stream);
        Str dataTmp = ReadIStream(stream);
        WStr data = WStr((WCHAR*)(u8*)dataTmp.s, (int)((size_t)dataTmp.len / sizeof(WCHAR)));
        utassert(data && stringSize == (size_t)dataTmp.len && wstr::Eq(data, string));
        utassert(dataTmp.s[dataTmp.len] == 0);
        utassert(dataTmp.s[dataTmp.len + 1] == 0);
        str::Free(dataTmp);
    }

    {
        Rect oneScreen = HwndGetFullscreenRect(nullptr);
        Rect allScreens = GetVirtualScreenRect();
        utassert(allScreens.Intersect(oneScreen) == oneScreen);
    }
}
