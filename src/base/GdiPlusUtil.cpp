/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Pixmap.h"

#include "base/GdiPlusUtil.h"

using Gdiplus::Bitmap;
using Gdiplus::BitmapData;
using Gdiplus::CharacterRange;
using Gdiplus::Font;
using Gdiplus::Graphics;
using Gdiplus::Matrix;
using Gdiplus::MatrixOrderAppend;
using Gdiplus::Ok;
using Gdiplus::Region;
using Gdiplus::Status;
using Gdiplus::StringFormat;
using Gdiplus::StringFormatFlagsMeasureTrailingSpaces;

void InitGraphicsMode(Graphics* g) {
    g->SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
    g->SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g->SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    g->SetPageUnit(Gdiplus::UnitPixel);
}

Gdiplus::RectF RectToRectF(const Gdiplus::Rect r) {
    return {(float)r.X, (float)r.Y, (float)r.Width, (float)r.Height};
}

// note: gdi+ seems to under-report the width, the longer the text, the
// bigger the difference. I'm trying to correct for that with those magic values
constexpr float kPerCharDxAdjust = .2f;
constexpr float kPerStrDxAdjust = 1.f;

// http://www.codeproject.com/KB/GDI-plus/measurestring.aspx
RectF MeasureTextAccurate(Graphics* g, Font* f, WStr s) {
    int n = s.len;
    if (0 == n) {
        return {0, 0, 0, 0}; // TODO: should set height to font's height
    }
    // note: frankly, I don't see a difference between those StringFormat variations
    StringFormat sf(StringFormat::GenericTypographic());
    sf.SetFormatFlags(sf.GetFormatFlags() | StringFormatFlagsMeasureTrailingSpaces);
    // StringFormat sf(StringFormat::GenericDefault());
    // StringFormat sf;
    Gdiplus::RectF layoutRect;
    CharacterRange cr(0, n);
    sf.SetMeasurableCharacterRanges(1, &cr);
    Region r;
    Status status = g->MeasureCharacterRanges(s.s, n, f, layoutRect, &sf, 1, &r);
    if (status != Ok) {
        // TODO: remove whem we figure out why we crash
        WStr logW = s ? s : WStr(L"<null>");
        TempStr s2 = ToUtf8Temp(logW);
        Str logStr = s2.len > 256 ? Str(s2.s, 256) : s2;
        logf("MeasureTextAccurate: status: %d, font: %p, len: %d, s: '%s'\n", (int)status, f, n, logStr);
        // ReportIf(status != Ok);
    }
    Gdiplus::RectF bbox;
    r.GetBounds(&bbox, g);
    if (bbox.Width != 0) {
        bbox.Width += kPerStrDxAdjust + (kPerCharDxAdjust * (float)n);
    }
    return RectF{bbox};
}

// this usually reports size that is too large
RectF MeasureTextStandard(Graphics* g, Font* f, WStr s) {
    Gdiplus::RectF bbox;
    Gdiplus::PointF pz(0, 0);
    g->MeasureString(s.s, s.len, f, pz, &bbox);
    return RectF{bbox};
}

RectF MeasureTextQuick(Graphics* g, Font* f, WStr s) {
    int n = s.len;
    ReportIf(0 >= n);

    static Vec<Font*> fontCache;
    static Vec<bool> fixCache;

    Gdiplus::RectF bbox;
    g->MeasureString(s.s, n, f, Gdiplus::PointF(0, 0), &bbox);
    int idx = VecFind(fontCache, f);
    if (-1 == idx) {
        LOGFONTW lfw;
        Status ok = f->GetLogFontW(g, &lfw);
        bool isItalicOrMonospace = Ok != ok || lfw.lfItalic || wstr::Eq(lfw.lfFaceName, WStrL(L"Courier New")) ||
                                   wstr::FindFrom(lfw.lfFaceName, L"Consol") ||
                                   wstr::EndsWith(lfw.lfFaceName, WStrL(L"Mono")) ||
                                   wstr::EndsWith(lfw.lfFaceName, WStrL(L"Typewriter"));
        VecAppend(fontCache, f);
        VecAppend(fixCache, isItalicOrMonospace);
        idx = fontCache.len - 1;
    }
    // most documents look good enough with these adjustments
    if (!fixCache[idx]) {
        float correct = 0;
        for (int i = 0; i < n; i++) {
            switch (s.s[i]) {
                case 'i':
                case 'l':
                    correct += 0.2f;
                    break;
                case 't':
                case 'f':
                case 'I':
                case '.':
                case ',':
                case '!':
                    correct += 0.1f;
                    break;
            }
        }
        bbox.Width *= (1.0f - (correct / (float)n)) * 0.99f;
    }
    bbox.Height *= 0.95f;
    return RectF{bbox};
}

RectF MeasureText(Graphics* g, Font* f, WStr s, TextMeasureAlgorithm algo) {
    // TODO: ideally we should not be here with len == 0. This
    // might indicate a problem with fromatter code. See internals-en.epub
    // for a repro
    ReportIf((len(s) == 0) || (s.len > INT_MAX));
    if (algo) {
        return algo(g, f, s);
    }
    return MeasureTextAccurate(g, f, s);
}

// returns number of characters of string s that fits in a given width dx
// note: could be speed up a bit because in our use case we already know
// the width of the whole string so we could supply it to the function, but
// this shouldn't happen often, so that's fine. It's also possible that
// a smarter approach is possible, but this usually only does 3 MeasureText
// calls, so it's not that bad
int StringLenForWidth(Graphics* g, Font* f, WStr s, float dx, TextMeasureAlgorithm algo) {
    int sLen = s.len;
    auto r = MeasureText(g, f, s, algo);
    if (r.dx <= dx) {
        return sLen;
    }
    // make the best guess of the length that fits
    int n = (int)((dx / r.dx) * (float)sLen);
    ReportIf(n > sLen);
    if (n == 0) {
        // nothing fits in the remaining space; caller flushes the line and
        // re-lays the run at full width. Don't Measure an empty string.
        return 0;
    }
    r = MeasureText(g, f, WStr(s.s, n), algo);
    // find the length len of s that fits within dx iff width of len+1 exceeds dx
    int dir = 1; // increasing length
    if (r.dx > dx) {
        dir = -1; // decreasing length
    }
    for (;;) {
        n += dir;
        r = MeasureText(g, f, WStr(s.s, n), algo);
        if (1 == dir) {
            // if advancing length, we know that previous string did fit, so if
            // the new one doesn't fit, the previous length was the right one
            if (r.dx > dx) {
                return n - 1;
            }
        } else {
            // if decreasing length, we know that previous string didn't fit, so if
            // the one one fits, it's of the correct length
            if (r.dx < dx) {
                return n;
            }
        }
    }
}

// TODO: not quite sure why spaceDx1 != spaceDx2, using spaceDx2 because
// is smaller and looks as better spacing to me
float GetSpaceDx(Graphics* g, Font* f, TextMeasureAlgorithm algo) {
    RectF bbox;
#if 0
    bbox = MeasureText(g, f, L" ", 1, algo);
    float spaceDx1 = bbox.dx;
    return spaceDx1;
#else
    // this method seems to return (much) smaller size that measuring
    // the space itself
    bbox = MeasureText(g, f, WStr(L"wa", 2), algo);
    float l1 = bbox.dx;
    bbox = MeasureText(g, f, WStr(L"w a", 3), algo);
    float l2 = bbox.dx;
    float spaceDx2 = l2 - l1;
    return spaceDx2;
#endif
}

// float     GetSpaceDx(Graphics *g, Font *f, TextMeasureAlgorithm algo=nullptr);
// int   StringLenForWidth(Graphics *g, Font *f, const WCHAR *s, size_t len, float dx, TextMeasureAlgorithm
// algo=nullptr);
void GetBaseTransform(Matrix& m, Gdiplus::RectF pageRect, float zoom, int rotation) {
    rotation = rotation % 360;
    if (rotation < 0) {
        rotation = rotation + 360;
    }
    if (90 == rotation) {
        m.Translate(0, -pageRect.Height, MatrixOrderAppend);
    } else if (180 == rotation) {
        m.Translate(-pageRect.Width, -pageRect.Height, MatrixOrderAppend);
    } else if (270 == rotation) {
        m.Translate(-pageRect.Width, 0, MatrixOrderAppend);
    } else if (0 == rotation) {
        m.Translate(0, 0, MatrixOrderAppend);
    } else {
        ReportIf(true);
    }

    m.Scale(zoom, zoom, MatrixOrderAppend);
    m.Rotate((float)rotation, MatrixOrderAppend);
}

static Gdiplus::RotateFlipType rfts[] = {
    Gdiplus::RotateNoneFlipX,  Gdiplus::Rotate180FlipNone, Gdiplus::Rotate180FlipX,    Gdiplus::Rotate90FlipX,
    Gdiplus::Rotate90FlipNone, Gdiplus::Rotate270FlipX,    Gdiplus::Rotate270FlipNone,
};

void ApplyExifOrientation(Bitmap* bmp, int exifOrientation) {
    if (!bmp || exifOrientation < 2 || exifOrientation > 8) {
        return;
    }
    int iRot = exifOrientation - 2;
    if (iRot < dimofi(rfts)) {
        bmp->RotateFlip(rfts[iRot]);
    }
}

// 0 (an invalid gdi+ format) when the pixels aren't ours to read
static Gdiplus::PixelFormat PixmapToGdiplusPixelFormat(const Pixmap* px) {
    if (px->format == PixmapFormat::Native) {
        return 0;
    }
    if (px->format == PixmapFormat::BGR8) {
        return PixelFormat24bppRGB;
    }
    // BGRA8 (RGBA8 isn't produced by our decoders and has no zero-copy GDI+ format)
    return px->premultiplied ? PixelFormat32bppPARGB : PixelFormat32bppARGB;
}

// a Gdiplus::Bitmap that borrows a Pixmap's pixels and frees the Pixmap when destroyed.
// Gdiplus::Image has a virtual destructor, so deleting via Gdiplus::Bitmap* (as all
// callers do) runs this destructor and releases the borrowed buffer.
namespace {
struct PixmapBackedBitmap : Gdiplus::Bitmap {
    Pixmap* px;
    PixmapBackedBitmap(Pixmap* p, Gdiplus::PixelFormat fmt)
        : Gdiplus::Bitmap(p->width, p->height, p->stride, fmt, p->data), px(p) {}
    ~PixmapBackedBitmap() override { FreePixmap(px); }
};
} // namespace

// Zero-copy: wrap a Pixmap's pixels in a Gdiplus::Bitmap that borrows the buffer and
// takes ownership of the Pixmap (frees it when the returned bitmap is deleted). The
// Pixmap must outlive the bitmap, which the returned object guarantees. Returns nullptr
// (and frees px) on failure. Only BGRA8/BGR8 Pixmaps are supported.
Gdiplus::Bitmap* NewGdiplusBitmapFromPixmap(Pixmap* px) {
    if (!px) {
        return nullptr;
    }
    Gdiplus::PixelFormat fmt = PixmapToGdiplusPixelFormat(px);
    if (!fmt) {
        FreePixmap(px);
        return nullptr;
    }
    auto* bmp = new PixmapBackedBitmap(px, fmt);
    if (bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp; // also frees px
        return nullptr;
    }
    bmp->SetResolution(px->xres, px->yres);
    return bmp;
}

// Copy a Gdiplus::Bitmap's pixels out into a freshly allocated BGRA8 Pixmap (used to
// turn an awkwardly-formatted GDI+ decode - 16bpp TGA, CMYK JPEG - into a uniform
// Pixmap). Does not take ownership of bmp. Returns nullptr on failure.
Pixmap* PixmapFromGdiplus(Gdiplus::Bitmap* bmp) {
    if (!bmp) {
        return nullptr;
    }
    int w = (int)bmp->GetWidth();
    int h = (int)bmp->GetHeight();
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8);
    if (!px) {
        return nullptr;
    }
    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData bd;
    if (bmp->LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok) {
        FreePixmap(px);
        return nullptr;
    }
    for (int y = 0; y < h; y++) {
        memcpy(px->data + ((size_t)y * px->stride), (u8*)bd.Scan0 + ((size_t)y * bd.Stride), (size_t)w * 4);
    }
    bmp->UnlockBits(&bd);
    px->xres = bmp->GetHorizontalResolution();
    px->yres = bmp->GetVerticalResolution();
    return px;
}

// Apply an EXIF orientation (2..8) to a Pixmap, returning a possibly-rotated Pixmap and
// freeing the input. orientation 0/1 (or out of range) returns px unchanged. Rotation
// is done via GDI+, so this lives here rather than in the portable Pixmap.h.
Pixmap* PixmapApplyExifOrientation(Pixmap* px, int orientation) {
    if (!px || orientation < 2 || orientation > 8) {
        return px;
    }
    Gdiplus::PixelFormat fmt = PixmapToGdiplusPixelFormat(px);
    // borrow px's pixels (no copy), clone to an owning bitmap we can rotate in place
    Gdiplus::Bitmap borrow(px->width, px->height, px->stride, fmt, px->data);
    Gdiplus::Bitmap* rot = borrow.Clone(0, 0, px->width, px->height, fmt);
    if (!rot) {
        return px;
    }
    ApplyExifOrientation(rot, orientation);
    Pixmap* out = PixmapFromGdiplus(rot);
    delete rot;
    if (!out) {
        return px; // rotation failed; keep the unrotated pixels
    }
    out->xres = px->xres;
    out->yres = px->yres;
    FreePixmap(px);
    return out;
}

// Zero-copy borrow: wrap a Pixmap's pixels in a Gdiplus::Bitmap that does NOT own the
// Pixmap. The Pixmap must outlive the returned bitmap. Use when the Pixmap is owned
// elsewhere (e.g. EngineImage's frame list). Returns nullptr on failure.
Gdiplus::Bitmap* WrapPixmapGdiplus(const Pixmap* px) {
    if (!px) {
        return nullptr;
    }
    Gdiplus::PixelFormat fmt = PixmapToGdiplusPixelFormat(px);
    if (!fmt) {
        return nullptr;
    }
    auto* bmp = new Gdiplus::Bitmap(px->width, px->height, px->stride, fmt, px->data);
    if (bmp->GetLastStatus() != Gdiplus::Ok) {
        delete bmp;
        return nullptr;
    }
    bmp->SetResolution(px->xres, px->yres);
    return bmp;
}

CLSID GetGdiPlusEncoderClsid(WStr format) {
    CLSID null{};
    uint numEncoders, size;
    Status ok = Gdiplus::GetImageEncodersSize(&numEncoders, &size);
    if (ok != Ok || 0 == size) {
        return null;
    }
    auto* codecInfo = (Gdiplus::ImageCodecInfo*)AllocTemp((int)size);
    if (!codecInfo) {
        return null;
    }
    GetImageEncoders(numEncoders, size, codecInfo);
    for (uint j = 0; j < numEncoders; j++) {
        if (wstr::Eq(WStr(codecInfo[j].MimeType), format)) {
            return codecInfo[j].Clsid;
        }
    }
    return null;
}

static bool PixmapHasTransparency(const Pixmap* p) {
    if (!p || !p->data || p->format != PixmapFormat::BGRA8) {
        return false;
    }
    for (int y = 0; y < p->height; y++) {
        const u8* d = p->data + ((size_t)y * p->stride);
        for (int x = 0; x < p->width; x++, d += 4) {
            if (d[3] != 255) {
                return true;
            }
        }
    }
    return false;
}

// 32bpp top-down DIB with any alpha composited onto white, so apps that only
// understand CF_BITMAP / CF_DIB paste an image on white paper instead of the
// black that dropping the alpha channel gives them
static HBITMAP PixmapToHbitmapOnWhite(const Pixmap* p) {
    Pixmap* dib = AllocPixmapDIB(p->width, p->height);
    if (!dib) {
        return nullptr;
    }
    int bpp = PixmapBytesPerPixel(p->format);
    bool rgba = p->format == PixmapFormat::RGBA8;
    for (int y = 0; y < p->height; y++) {
        const u8* s = p->data + ((size_t)y * p->stride);
        u8* d = dib->data + ((size_t)y * dib->stride);
        for (int x = 0; x < p->width; x++, s += bpp, d += 4) {
            u32 a = bpp == 3 ? 255 : s[3];
            u32 b = rgba ? s[2] : s[0];
            u32 g = s[1];
            u32 r = rgba ? s[0] : s[2];
            u32 inv = 255 - a;
            d[0] = (u8)std::min<u32>(255, ((b * a + 127) / 255) + inv);
            d[1] = (u8)std::min<u32>(255, ((g * a + 127) / 255) + inv);
            d[2] = (u8)std::min<u32>(255, ((r * a + 127) / 255) + inv);
            d[3] = 255;
        }
    }
    HBITMAP hbmp = dib->hbmp;
    dib->hbmp = nullptr;
    dib->data = nullptr;
    FreePixmap(dib);
    return hbmp;
}

// CF_DIBV5: 32bpp BI_BITFIELDS with an alpha mask, top-down, straight alpha
static HGLOBAL PixmapToDibV5Global(const Pixmap* p) {
    size_t rowBytes = (size_t)p->width * 4;
    size_t nBytes = sizeof(BITMAPV5HEADER) + rowBytes * (size_t)p->height;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, nBytes);
    if (!hMem) {
        return nullptr;
    }
    auto* bi = (BITMAPV5HEADER*)GlobalLock(hMem);
    if (!bi) {
        GlobalFree(hMem);
        return nullptr;
    }
    memset(bi, 0, sizeof(*bi));
    bi->bV5Size = sizeof(BITMAPV5HEADER);
    bi->bV5Width = p->width;
    bi->bV5Height = -p->height; // top-down
    bi->bV5Planes = 1;
    bi->bV5BitCount = 32;
    bi->bV5Compression = BI_BITFIELDS;
    bi->bV5SizeImage = (DWORD)(rowBytes * (size_t)p->height);
    bi->bV5RedMask = 0x00ff0000;
    bi->bV5GreenMask = 0x0000ff00;
    bi->bV5BlueMask = 0x000000ff;
    bi->bV5AlphaMask = 0xff000000;
    bi->bV5CSType = LCS_WINDOWS_COLOR_SPACE;
    bi->bV5Intent = LCS_GM_IMAGES;
    u8* dst = (u8*)bi + sizeof(BITMAPV5HEADER);
    for (int y = 0; y < p->height; y++) {
        memcpy(dst + ((size_t)y * rowBytes), p->data + ((size_t)y * p->stride), rowBytes);
    }
    GlobalUnlock(hMem);
    return hMem;
}

static HGLOBAL PixmapToPngGlobal(const Pixmap* p) {
    Gdiplus::Bitmap* bmp = WrapPixmapGdiplus(p);
    if (!bmp) {
        return nullptr;
    }
    CLSID pngClsid = GetGdiPlusEncoderClsid(L"image/png");
    IStream* stream = nullptr;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, FALSE, &stream);
    if (FAILED(hr) || !stream) {
        delete bmp;
        return nullptr;
    }
    Gdiplus::Status status = bmp->Save(stream, &pngClsid, nullptr);
    HGLOBAL hMem = nullptr;
    if (status == Gdiplus::Ok) {
        GetHGlobalFromStream(stream, &hMem);
    }
    stream->Release();
    delete bmp;
    return status == Gdiplus::Ok ? hMem : nullptr;
}

// Put an image on the clipboard so that a paste into an image editor keeps its
// transparency. CF_BITMAP cannot carry alpha, so an image with transparent
// pixels is published in three formats, richest first:
//   PNG       - the registered format image editors prefer; lossless, alpha
//   CF_DIBV5  - alpha via BI_BITFIELDS, for the apps that read it
//   CF_BITMAP - alpha flattened onto white, so Paint / Word still paste
//               something sensible rather than the black that dropping the
//               alpha channel gives them
// A fully opaque image gets CF_BITMAP alone, exactly as before.
// Takes ownership of nothing; the caller still owns p.
bool CopyPixmapToClipboard(Pixmap* p, bool appendOnly) {
    if (!p || p->width <= 0 || p->height <= 0) {
        return false;
    }
    // a Native pixmap (e.g. the palette DIB the mupdf engine renders some pages
    // to) has no pixels we can read; copy it into one we can. It has no alpha
    // either, so this just keeps the plain CF_BITMAP path working
    Pixmap* readable = nullptr;
    if (p->format == PixmapFormat::Native || !p->data) {
        readable = PixmapCopyAs32bppDIB(p);
        if (!readable) {
            return false;
        }
        p = readable;
    }
    defer {
        FreePixmap(readable);
    };

    bool hasAlpha = PixmapHasTransparency(p);
    HBITMAP hbmp = PixmapToHbitmapOnWhite(p); // a no-op copy when there's no alpha
    if (!hbmp) {
        return false;
    }
    HGLOBAL hPng = hasAlpha ? PixmapToPngGlobal(p) : nullptr;
    HGLOBAL hDibV5 = hasAlpha ? PixmapToDibV5Global(p) : nullptr;

    if (!appendOnly && !OpenClipboardForUpdate()) {
        DeleteObject(hbmp);
        if (hPng) {
            GlobalFree(hPng);
        }
        if (hDibV5) {
            GlobalFree(hDibV5);
        }
        return false;
    }

    bool ok = false;
    if (hPng) {
        static UINT cfPng = RegisterClipboardFormatW(L"PNG");
        if (!cfPng || !SetClipboardData(cfPng, hPng)) {
            GlobalFree(hPng);
        }
    }
    if (hDibV5 && !SetClipboardData(CF_DIBV5, hDibV5)) {
        GlobalFree(hDibV5);
    }
    if (SetClipboardData(CF_BITMAP, hbmp)) {
        ok = true;
    } else {
        DeleteObject(hbmp);
    }

    if (!appendOnly) {
        CloseClipboardAfterUpdate();
    }
    return ok;
}
