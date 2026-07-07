/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// Pixmap is a platform-independent, raw (uncompressed) in-memory bitmap: a tightly
// described pixel buffer with no dependency on any OS imaging API. It exists so image
// decoders can return decoded pixels without binding to Gdiplus::Bitmap, and so the
// pixels can be handed to the platform's imaging API with zero copies:
//  - Windows: Gdiplus::Bitmap(w, h, stride, format, scan0) borrows the buffer
//  - macOS:   CGBitmapContextCreate / CGImageCreate over the buffer
//  - Linux:   cairo_image_surface_create_for_data over the buffer
//
// BGRA8 (premultiplied) is the canonical layout because it is the natively zero-copy
// layout on all three platforms (Gdiplus 32bppPARGB, CoreGraphics
// PremultipliedFirst|ByteOrder32Little, cairo ARGB32). The Windows-specific zero-copy
// conversion helpers live in GdiPlus.h (NewGdiplusBitmapFromPixmap / PixmapFromGdiplus).
//
// A Pixmap may additionally be backed by a platform "present" object so it can be blitted
// to the screen with no copy (a GDI DIB section on Windows; later a CGImage/cairo surface
// on mac/linux). That backing is described per-platform with #if rather than an opaque
// handle. When present (hbmp != null on Windows), `data` points into the present object's
// pixels and is owned by it (freed via FreePixmapNativeBitmap), not by malloc/free.
//
// On Windows this header uses HBITMAP/HANDLE directly; <windows.h> is assumed to have
// been included before it (it comes in via Base.h).

enum class PixmapFormat : u8 {
    // byte order in memory. BGRA8 is the zero-copy layout on all 3 platforms.
    BGRA8, // 32bpp B,G,R,A -> Gdiplus 32bppARGB/PARGB
    BGR8,  // 24bpp B,G,R    -> Gdiplus 24bppRGB (rows still padded to a multiple of 4)
    RGBA8, // 32bpp R,G,B,A  -> needs a swizzle for any platform API; for source data
};

struct Pixmap {
    int width = 0;
    int height = 0;
    int stride = 0; // bytes per row; top-down (row y starts at data + y*stride); multiple of 4
    PixmapFormat format = PixmapFormat::BGRA8;
    bool premultiplied = false; // alpha premultiplied into RGB
    float xres = 96.0f;
    float yres = 96.0f;
    u8* data = nullptr; // pixel buffer; owned by malloc, or by hbmp when DIB-section-backed

#if defined(_WIN32)
    // When non-null, the Pixmap is backed by a GDI DIB section: `data` is its pixels and
    // the bitmap is directly blittable (BlitPixmap). Owns these handles.
    HBITMAP hbmp = nullptr;
    HANDLE hMap = nullptr; // optional file mapping backing hbmp
#endif
};

Str PixmapToBmpFormat(const Pixmap* pixmap);

#if defined(_WIN32)
// frees a DIB-section-backed Pixmap's native handles (and its pixels). implemented in
// Win.cpp where <windows.h> is available. Does nothing if not DIB-backed.
void FreePixmapNativeBitmap(Pixmap* p);
#endif

inline int PixmapBytesPerPixel(PixmapFormat fmt) {
    return fmt == PixmapFormat::BGR8 ? 3 : 4;
}

// approximate memory footprint (4 bytes/pixel, ignoring stride). 0 for null.
inline i64 PixmapByteSize(const Pixmap* p) {
    return p ? (i64)p->width * (i64)p->height * 4 : 0;
}

// allocate a top-down Pixmap; data is uninitialized. returns nullptr on bad args / OOM.
inline Pixmap* AllocPixmap(int w, int h, PixmapFormat fmt = PixmapFormat::BGRA8, bool premultiplied = false) {
    if (w <= 0 || h <= 0) {
        return nullptr;
    }
    size_t bpp = (size_t)PixmapBytesPerPixel(fmt);
    size_t stride = (((size_t)w * bpp) + 3) & ~(size_t)3;
    size_t nBytes = stride * (size_t)h;
    // guard against overflow on absurd dimensions
    if (stride > INT_MAX || nBytes / stride != (size_t)h) {
        return nullptr;
    }
    u8* data = (u8*)malloc(nBytes);
    if (!data) {
        return nullptr;
    }
    Pixmap* p = new Pixmap();
    p->width = w;
    p->height = h;
    p->stride = (int)stride;
    p->format = fmt;
    p->premultiplied = premultiplied;
    p->data = data;
    return p;
}

inline void FreePixmap(Pixmap* p) {
    if (!p) {
        return;
    }
#if defined(_WIN32)
    if (p->hbmp) {
        // DIB-section-backed: GDI owns the pixels, free via the native handles
        FreePixmapNativeBitmap(p);
        delete p;
        return;
    }
#endif
    free(p->data);
    delete p;
}

// deep copy (own pixels). returns nullptr on bad input / OOM.
inline Pixmap* ClonePixmap(const Pixmap* src) {
    if (!src || !src->data) {
        return nullptr;
    }
    Pixmap* p = AllocPixmap(src->width, src->height, src->format, src->premultiplied);
    if (!p) {
        return nullptr;
    }
    p->xres = src->xres;
    p->yres = src->yres;
    memcpy(p->data, src->data, (size_t)src->stride * (size_t)src->height);
    return p;
}
