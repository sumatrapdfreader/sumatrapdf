/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Pixmap.h"

Pixmap* AllocPixmapDIB(int w, int h) {
    if (w <= 0 || h <= 0) {
        return nullptr;
    }
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hbmp || !bits) {
        DeleteObject(hbmp);
        return nullptr;
    }
    Pixmap* p = new Pixmap();
    p->width = w;
    p->height = h;
    p->stride = w * 4;
    p->format = PixmapFormat::BGRA8;
    p->data = (u8*)bits;
    p->hbmp = hbmp;
    return p;
}

// Adopt an existing HBITMAP (and optional file mapping) into a Pixmap that owns them.
// If it's a DIB section, expose its pixels through data/stride/format; otherwise only
// carry the blittable handle.
Pixmap* PixmapFromHBITMAP(HBITMAP hbmp, Size size, HANDLE hMap) {
    if (!hbmp) {
        return nullptr;
    }
    Pixmap* p = new Pixmap();
    p->width = size.dx;
    p->height = size.dy;
    p->hbmp = hbmp;
    p->hMap = hMap;
    DIBSECTION ds{};
    if (GetObject(hbmp, sizeof(ds), &ds) == sizeof(ds) && ds.dsBm.bmBits) {
        p->stride = ds.dsBm.bmWidthBytes;
        p->data = (u8*)ds.dsBm.bmBits;
        // stride is the DIB's real byte width either way, so the memory
        // accounting stays right even when the pixels aren't ours to read
        int bpp = ds.dsBm.bmBitsPixel;
        p->format = PixmapFormat::Native;
        if (bpp == 24) {
            p->format = PixmapFormat::BGR8;
        } else if (bpp == 32) {
            p->format = PixmapFormat::BGRA8;
        }
        // Orientation follows the DIB. Pixel readers that care about orientation should
        // prefer the HBITMAP-based helpers, which handle it.
    }
    return p;
}

Pixmap* PixmapCopyAs32bppDIB(const Pixmap* p) {
    if (!p || !p->hbmp || p->width <= 0 || p->height <= 0) {
        return nullptr;
    }
    Pixmap* dst = AllocPixmapDIB(p->width, p->height);
    if (!dst) {
        return nullptr;
    }
    bool ok = false;
    HDC dstDC = CreateCompatibleDC(nullptr);
    HDC srcDC = CreateCompatibleDC(nullptr);
    if (dstDC && srcDC) {
        HGDIOBJ prevDst = SelectObject(dstDC, dst->hbmp);
        HGDIOBJ prevSrc = SelectObject(srcDC, p->hbmp);
        if (prevDst && prevSrc) {
            ok = BitBlt(dstDC, 0, 0, p->width, p->height, srcDC, 0, 0, SRCCOPY) != 0;
            GdiFlush();
        }
        if (prevDst) {
            SelectObject(dstDC, prevDst);
        }
        if (prevSrc) {
            SelectObject(srcDC, prevSrc);
        }
    }
    DeleteDC(dstDC);
    DeleteDC(srcDC);
    if (!ok) {
        FreePixmap(dst);
        return nullptr;
    }
    // BitBlt leaves the alpha channel alone (i.e. at the zero CreateDIBSection
    // gave us), which would make the copy fully transparent
    for (int y = 0; y < dst->height; y++) {
        u8* d = dst->data + ((size_t)y * dst->stride);
        for (int x = 0; x < dst->width; x++, d += 4) {
            d[3] = 0xff;
        }
    }
    dst->xres = p->xres;
    dst->yres = p->yres;
    return dst;
}

Pixmap* PixmapFromRenderedBitmap(RenderedBitmap* rb) {
    if (!rb) {
        return nullptr;
    }
    Pixmap* p = PixmapFromHBITMAP(rb->hbmp, rb->size, rb->hMap);
    rb->hbmp = nullptr;
    rb->hMap = nullptr;
    delete rb;
    return p;
}

// The alpha in a DIB section is straight, not premultiplied: that is what PNG,
// CF_DIBV5 and GDI+ all expect of a 32bpp bitmap, and mupdf hands us
// premultiplied pixels. Undoing it here keeps the invariant in one place.
static void UnpremultiplyBgra(u8* d) {
    u32 a = d[3];
    if (a == 0 || a == 255) {
        return;
    }
    d[0] = (u8)std::min<u32>(255, ((u32)d[0] * 255 + (a / 2)) / a);
    d[1] = (u8)std::min<u32>(255, ((u32)d[1] * 255 + (a / 2)) / a);
    d[2] = (u8)std::min<u32>(255, ((u32)d[2] * 255 + (a / 2)) / a);
}

RenderedBitmap* RenderedBitmapFromPixmap(Pixmap* px) {
    if (!px) {
        return nullptr;
    }
    if (!px->hbmp) {
        if (!px->data) {
            FreePixmap(px);
            return nullptr;
        }
        Pixmap* dib = AllocPixmapDIB(px->width, px->height);
        if (!dib) {
            FreePixmap(px);
            return nullptr;
        }
        for (int y = 0; y < px->height; y++) {
            const u8* src = px->data + ((size_t)y * px->stride);
            u8* dst = dib->data + ((size_t)y * dib->stride);
            for (int x = 0; x < px->width; x++) {
                if (px->format == PixmapFormat::BGR8) {
                    dst[0] = src[0];
                    dst[1] = src[1];
                    dst[2] = src[2];
                    dst[3] = 0xff;
                    src += 3;
                } else if (px->format == PixmapFormat::RGBA8) {
                    dst[0] = src[2];
                    dst[1] = src[1];
                    dst[2] = src[0];
                    dst[3] = src[3];
                    src += 4;
                } else {
                    memcpy(dst, src, 4);
                    src += 4;
                }
                if (px->premultiplied) {
                    UnpremultiplyBgra(dst);
                }
                dst += 4;
            }
        }
        FreePixmap(px);
        px = dib;
    }
    auto* rb = new RenderedBitmap(px->hbmp, Size(px->width, px->height), px->hMap);
    px->hbmp = nullptr;
    px->hMap = nullptr;
    px->data = nullptr;
    FreePixmap(px);
    return rb;
}

// DIB-section-backed: GDI owns the pixels, free via the native handles
// frees a DIB-section-backed Pixmap's native handles (and its pixels).
void FreePixmapNativeBitmap(Pixmap* p) {
    if (!p) {
        return;
    }
    if (p->hbmp) {
        DeleteObject(p->hbmp);
        p->hbmp = nullptr;
    }
    if (p->hMap) {
        CloseHandle(p->hMap);
        p->hMap = nullptr;
    }
    p->data = nullptr;
}

// Shell / association icons carry transparency in a 1-bit AND mask or a 32bpp
// alpha channel. Gdiplus::Bitmap(HICON) drops that and leaves transparent
// pixels as opaque black, which then shows as a black square on the home page.
Pixmap* PixmapFromHICON(HICON hicon) {
    if (!hicon) {
        return nullptr;
    }
    ICONINFO ii{};
    if (!GetIconInfo(hicon, &ii)) {
        return nullptr;
    }
    BITMAP bm{};
    HBITMAP srcBmp = ii.hbmColor ? ii.hbmColor : ii.hbmMask;
    if (!srcBmp || GetObjectW(srcBmp, sizeof(bm), &bm) == 0 || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
        return nullptr;
    }
    int w = bm.bmWidth;
    int h = ii.hbmColor ? bm.bmHeight : bm.bmHeight / 2;
    if (h <= 0) {
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
        return nullptr;
    }

    Pixmap* px = AllocPixmapDIB(w, h);
    if (!px) {
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
        return nullptr;
    }
    memset(px->data, 0, (size_t)px->stride * (size_t)h);
    px->hasAlpha = true;

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc) {
        FreePixmap(px);
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
        return nullptr;
    }
    HGDIOBJ old = SelectObject(hdc, px->hbmp);
    DrawIconEx(hdc, 0, 0, hicon, w, h, 0, nullptr, DI_NORMAL);
    if (old) {
        SelectObject(hdc, old);
    }

    bool anyAlpha = false;
    for (int y = 0; y < h && !anyAlpha; y++) {
        const u8* d = px->data + ((size_t)y * px->stride);
        for (int x = 0; x < w; x++, d += 4) {
            if (d[3] != 0) {
                anyAlpha = true;
                break;
            }
        }
    }

    if (!anyAlpha && ii.hbmMask) {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        Pixmap* mask = AllocPixmap(w, h, PixmapFormat::BGRA8);
        if (mask && mask->data) {
            GetDIBits(hdc, ii.hbmMask, 0, (UINT)h, mask->data, &bmi, DIB_RGB_COLORS);
            for (int y = 0; y < h; y++) {
                u8* d = px->data + ((size_t)y * px->stride);
                const u8* m = mask->data + ((size_t)y * mask->stride);
                for (int x = 0; x < w; x++, d += 4, m += 4) {
                    if (m[0] || m[1] || m[2]) {
                        d[0] = d[1] = d[2] = d[3] = 0;
                    } else {
                        d[3] = 255;
                    }
                }
            }
        }
        FreePixmap(mask);
    }

    DeleteDC(hdc);
    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);
    return px;
}

static bool BlitPixmapRegionComposited(Pixmap* p, HDC hdc, Rect target, Rect source);

bool BlitPixmapRegion(Pixmap* p, HDC hdc, Rect target, Rect source) {
    if (!p || !p->data || target.IsEmpty() || source.IsEmpty()) {
        return false;
    }
    // a pixmap that carries real transparency (an image with an alpha channel,
    // or a PDF page rendered with a transparent backdrop) has to be blended
    // with what's underneath, or SRCCOPY paints its transparent parts black
    // (issues #5844, #1809). DIB-backed pages used to skip this and BitBlt.
    if (p->hasAlpha && p->format == PixmapFormat::BGRA8) {
        return BlitPixmapRegionComposited(p, hdc, target, source);
    }
    SetStretchBltMode(hdc, HALFTONE);
    if (p->hbmp) {
        HDC bmpDC = CreateCompatibleDC(hdc);
        if (!bmpDC) {
            return false;
        }
        HGDIOBJ oldBmp = SelectObject(bmpDC, p->hbmp);
        bool ok = false;
        if (oldBmp && target.dx == source.dx && target.dy == source.dy) {
            ok = BitBlt(hdc, target.x, target.y, target.dx, target.dy, bmpDC, source.x, source.y, SRCCOPY) != 0;
        } else if (oldBmp) {
            ok = StretchBlt(hdc, target.x, target.y, target.dx, target.dy, bmpDC, source.x, source.y, source.dx,
                            source.dy, SRCCOPY) != 0;
        }
        if (oldBmp) {
            SelectObject(bmpDC, oldBmp);
        }
        DeleteDC(bmpDC);
        return ok;
    }
    source = Rect(0, 0, p->width, p->height).Intersect(source);
    if (source.IsEmpty()) {
        return false;
    }
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = p->width;
    bmi.bmiHeader.biHeight = -source.dy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = p->format == PixmapFormat::BGR8 ? 24 : 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    const u8* rows = p->data + ((size_t)source.y * p->stride);
    int n = StretchDIBits(hdc, target.x, target.y, target.dx, target.dy, source.x, 0, source.dx, source.dy, rows, &bmi,
                          DIB_RGB_COLORS, SRCCOPY);
    return n != GDI_ERROR && n != 0;
}

bool BlitPixmap(Pixmap* p, HDC hdc, Rect target) {
    if (!p || !p->data) {
        return false;
    }
    if (p->hbmp) {
        return BlitHBITMAP(p->hbmp, hdc, target);
    }
    return BlitPixmapRegion(p, hdc, target, Rect(0, 0, p->width, p->height));
}

static inline u8 BlendOver(u8 src, u8 dst, u32 srcAlpha, bool premultiplied) {
    u32 inv = 255 - srcAlpha;
    u32 s = premultiplied ? src : (((u32)src * srcAlpha) + 127) / 255;
    return (u8)std::min<u32>(255, s + ((((u32)dst * inv) + 127) / 255));
}

// Like BlitPixmap(), but honours the source alpha. BlitPixmap() is a straight
// SRCCOPY, which paints the transparent parts of an icon black; anything drawn
// over an arbitrary background (toolbar icons on the home page) needs this.
//
// The compositing is done by hand rather than with AlphaBlend() or GDI+:
// msimg32 and GdiPlusUtil are linked into SumatraPDF.exe but not into the other
// consumers of base (test_util, PdfFilter, ...). Reading the destination back
// costs a BitBlt, which is nothing at icon sizes.
//
// Only 1:1 blits are composited; a scaling blit falls back to the opaque path.
bool BlitPixmapAlpha(Pixmap* p, HDC hdc, Rect target) {
    if (!p || !p->data || target.IsEmpty()) {
        return false;
    }
    bool sameSize = (target.dx == p->width) && (target.dy == p->height);
    if (p->format != PixmapFormat::BGRA8 || !sameSize) {
        return BlitPixmap(p, hdc, target);
    }
    int dx = p->width;
    int dy = p->height;
    Pixmap* dst = AllocPixmapDIB(dx, dy);
    if (!dst) {
        return false;
    }
    bool ok = false;
    HDC memDC = CreateCompatibleDC(hdc);
    if (memDC) {
        HGDIOBJ prev = SelectObject(memDC, dst->hbmp);
        if (prev) {
            // read the background, blend the source over it, put it back
            BitBlt(memDC, 0, 0, dx, dy, hdc, target.x, target.y, SRCCOPY);
            GdiFlush();
            for (int y = 0; y < dy; y++) {
                const u8* s = p->data + ((size_t)y * p->stride);
                u8* d = dst->data + ((size_t)y * dst->stride);
                for (int x = 0; x < dx; x++, s += 4, d += 4) {
                    u32 a = s[3];
                    if (a == 0) {
                        continue;
                    }
                    if (a == 255) {
                        d[0] = s[0];
                        d[1] = s[1];
                        d[2] = s[2];
                        continue;
                    }
                    d[0] = BlendOver(s[0], d[0], a, p->premultiplied);
                    d[1] = BlendOver(s[1], d[1], a, p->premultiplied);
                    d[2] = BlendOver(s[2], d[2], a, p->premultiplied);
                }
            }
            GdiFlush();
            ok = BitBlt(hdc, target.x, target.y, dx, dy, memDC, 0, 0, SRCCOPY) != 0;
            SelectObject(memDC, prev);
        }
        DeleteDC(memDC);
    }
    FreePixmap(dst);
    return ok;
}

// Draw a region of an alpha-carrying pixmap, blending over what's already on
// the target so the document background - a solid colour, or the checkered
// pattern - shows through the transparent parts (issue #5844).
//
// Hand-rolled for the same reason as BlitPixmapAlpha: msimg32's AlphaBlend()
// isn't linked into every consumer of base. Unlike that one this has to handle
// a scaling blit, because a stale tile is stretched while the new one renders.
// The sampling is nearest-neighbour, which is all a stretched stale tile is.
static bool BlitPixmapRegionComposited(Pixmap* p, HDC hdc, Rect target, Rect source) {
    source = Rect(0, 0, p->width, p->height).Intersect(source);
    if (source.IsEmpty()) {
        return false;
    }
    int dx = target.dx;
    int dy = target.dy;
    Pixmap* dst = AllocPixmapDIB(dx, dy);
    if (!dst) {
        return false;
    }
    bool ok = false;
    HDC memDC = CreateCompatibleDC(hdc);
    if (memDC) {
        HGDIOBJ prev = SelectObject(memDC, dst->hbmp);
        if (prev) {
            // read the background, blend the source over it, put it back
            BitBlt(memDC, 0, 0, dx, dy, hdc, target.x, target.y, SRCCOPY);
            GdiFlush();
            for (int y = 0; y < dy; y++) {
                int sy = (dy == source.dy) ? y : (int)(((i64)y * source.dy) / dy);
                const u8* srcRow = p->data + ((size_t)(source.y + sy) * p->stride);
                u8* d = dst->data + ((size_t)y * dst->stride);
                for (int x = 0; x < dx; x++, d += 4) {
                    int sx = (dx == source.dx) ? x : (int)(((i64)x * source.dx) / dx);
                    const u8* s = srcRow + ((size_t)(source.x + sx) * 4);
                    u32 a = s[3];
                    if (a == 0) {
                        continue;
                    }
                    if (a == 255) {
                        d[0] = s[0];
                        d[1] = s[1];
                        d[2] = s[2];
                        continue;
                    }
                    d[0] = BlendOver(s[0], d[0], a, p->premultiplied);
                    d[1] = BlendOver(s[1], d[1], a, p->premultiplied);
                    d[2] = BlendOver(s[2], d[2], a, p->premultiplied);
                }
            }
            GdiFlush();
            ok = BitBlt(hdc, target.x, target.y, dx, dy, memDC, 0, 0, SRCCOPY) != 0;
            SelectObject(memDC, prev);
        }
        DeleteDC(memDC);
    }
    FreePixmap(dst);
    return ok;
}

static bool SkipRecolorPixel(int x, int y, Vec<Rect>* skipRects) {
    if (skipRects) {
        for (Rect& r : *skipRects) {
            if (r.Contains(x, y)) {
                return true;
            }
        }
    }
    return false;
}

static int Mul255(int a, int b) {
    int n = (a * b) + 128;
    n += n >> 8;
    return n >> 8;
}

void RecolorPixmap(Pixmap* px, Color textColor, Color bgColor, Color linkColor, Vec<Rect>* skipRects) {
    if (!px) {
        return;
    }
    if (px->hbmp) {
        UpdateBitmapColors(px->hbmp, textColor, bgColor, linkColor, skipRects);
        return;
    }
    if (!px->data || px->width <= 0 || px->height <= 0 || px->format == PixmapFormat::RGBA8) {
        return;
    }
    if ((textColor & 0xffffff) == kColBlack && (bgColor & 0xffffff) == kColWhite && !linkColor && !skipRects) {
        return;
    }
    byte linkR = 0, linkG = 0, linkB = 0;
    UnpackColor(linkColor, linkR, linkG, linkB);
    byte textR, textG, textB, bgR, bgG, bgB;
    UnpackColor(textColor, textR, textG, textB);
    UnpackColor(bgColor, bgR, bgG, bgB);
    const int base[3] = {textB, textG, textR};
    const int diff[3] = {(int)bgB - textB, (int)bgG - textG, (int)bgR - textR};
    int bpp = PixmapBytesPerPixel(px->format);
    for (int y = 0; y < px->height; y++) {
        u8* pixel = px->data + ((size_t)y * px->stride);
        for (int x = 0; x < px->width; x++, pixel += bpp) {
            if (SkipRecolorPixel(x, y, skipRects)) {
                continue;
            }
            int maxRG = pixel[2] > pixel[1] ? pixel[2] : pixel[1];
            int lum = (pixel[0] + pixel[1] + pixel[2]) / 3;
            if (linkColor && pixel[0] >= maxRG + 25 && pixel[0] >= 72 && lum <= 230) {
                int rg = ((int)pixel[1] + pixel[2]) / 2;
                pixel[0] = (u8)(linkB + Mul255(rg, (int)bgB - linkB));
                pixel[1] = (u8)(linkG + Mul255(rg, (int)bgG - linkG));
                pixel[2] = (u8)(linkR + Mul255(rg, (int)bgR - linkR));
                continue;
            }
            for (int i = 0; i < 3; i++) {
                pixel[i] = (u8)(base[i] + Mul255(pixel[i], diff[i]));
            }
        }
    }
}

static Size GetBitmapSize(HBITMAP hbmp) {
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    return {bmpInfo.bmWidth, bmpInfo.bmHeight};
}

// Copy an HBITMAP into a top-down 32bpp BGRA pixmap. Reading the clipboard as
// 24bpp GetDIBits sheared paste-as-stamp (issue #6059): area-copy puts a
// 32-bpp CF_BITMAP on the clipboard, and converting that to DWORD-padded 24bpp
// rows does not land on the stride the stamp path uses. 32bpp BI_RGB rows are
// always width*4, the same as AllocPixmap(BGRA8).
static Pixmap* PixmapFromHBITMAPPixels(HBITMAP hbmp) {
    Size size = GetBitmapSize(hbmp);
    if (size.dx <= 0 || size.dy <= 0) {
        return nullptr;
    }
    Pixmap* pixmap = AllocPixmap(size.dx, size.dy, PixmapFormat::BGRA8);
    if (!pixmap) {
        return nullptr;
    }
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = size.dx;
    bmi.bmiHeader.biHeight = -size.dy; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    int n = GetDIBits(hdc, hbmp, 0, (UINT)size.dy, pixmap->data, &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);
    if (!n) {
        FreePixmap(pixmap);
        return nullptr;
    }
    // CF_BITMAP has no alpha; GetDIBits leaves it 0, which would make a stamp
    // fully transparent.
    for (int y = 0; y < pixmap->height; y++) {
        u8* d = pixmap->data + ((size_t)y * pixmap->stride);
        for (int x = 0; x < pixmap->width; x++, d += 4) {
            d[3] = 0xff;
        }
    }
    return pixmap;
}

// Returns a copy of the clipboard bitmap as a platform-independent Pixmap.
Pixmap* GetClipboardImageAsPixmap() {
    if (!IsClipboardFormatAvailable(CF_BITMAP) || !OpenClipboard(nullptr)) {
        return nullptr;
    }

    Pixmap* pixmap = nullptr;
    // CF_BITMAP is synthesized by Windows from CF_DIB and vice versa. The HBITMAP
    // returned by GetClipboardData() remains owned by the clipboard.
    HBITMAP hbmp = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (hbmp) {
        pixmap = PixmapFromHBITMAPPixels(hbmp);
    }
    CloseClipboard();
    return pixmap;
}
