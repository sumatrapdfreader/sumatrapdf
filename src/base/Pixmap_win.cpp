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

bool BlitPixmapRegion(Pixmap* p, HDC hdc, Rect target, Rect source) {
    if (!p || !p->data || target.IsEmpty() || source.IsEmpty()) {
        return false;
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
                pixel[0] = linkB;
                pixel[1] = linkG;
                pixel[2] = linkR;
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
        Size size = GetBitmapSize(hbmp);
        pixmap = AllocPixmap(size.dx, size.dy, PixmapFormat::BGR8);
        if (pixmap) {
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
            bmi.bmiHeader.biWidth = size.dx;
            bmi.bmiHeader.biHeight = -size.dy;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 24;
            bmi.bmiHeader.biCompression = BI_RGB;

            HDC hdc = GetDC(nullptr);
            if (!GetDIBits(hdc, hbmp, 0, size.dy, pixmap->data, &bmi, DIB_RGB_COLORS)) {
                FreePixmap(pixmap);
                pixmap = nullptr;
            }
            ReleaseDC(nullptr, hdc);
        }
    }
    CloseClipboard();
    return pixmap;
}
