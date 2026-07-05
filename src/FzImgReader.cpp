/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable

extern "C" {
#include "../mupdf/source/fitz/color-imp.h"
#include "../mupdf/source/fitz/image-imp.h"
}

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/Win.h"
#include "base/GdiPlus.h"
#include "base/File.h"

#include "FzImgReader.h"

#include "base/Log.h"

struct MupdfContext {
    fz_locks_context fz_locks_ctx{};
    CRITICAL_SECTION mutexes[FZ_LOCK_MAX];
    fz_context* ctx = nullptr;
};

static void fz_lock_context_cs(void* user, int lock) {
    MupdfContext* ctx = (MupdfContext*)user;
    EnterCriticalSection(&ctx->mutexes[lock]);
}

static void fz_unlock_context_cs(void* user, int lock) {
    MupdfContext* ctx = (MupdfContext*)user;
    LeaveCriticalSection(&ctx->mutexes[lock]);
}

// route mupdf's warnings/errors through our log() instead of the default
// callback, which does fputs() to stderr; that first fputs makes the CRT
// allocate a stdio buffer it never frees, which shows up as a leak
static void fz_log_cb(void*, const char* msg) {
    log(Str(msg));
}

fz_context* fz_new_context_windows(size_t maxStore) {
    auto c = new MupdfContext();
    for (int i = 0; i < FZ_LOCK_MAX; i++) {
        InitializeCriticalSection(&c->mutexes[i]);
    }
    c->fz_locks_ctx.user = c;
    c->fz_locks_ctx.lock = fz_lock_context_cs;
    c->fz_locks_ctx.unlock = fz_unlock_context_cs;
    c->ctx = fz_new_context(nullptr, &c->fz_locks_ctx, maxStore);
    if (c->ctx) {
        fz_set_warning_callback(c->ctx, fz_log_cb, nullptr);
        fz_set_error_callback(c->ctx, fz_log_cb, nullptr);
    }
    return c->ctx;
}

void fz_drop_context_windows(fz_context* ctx) {
    auto c = (MupdfContext*)ctx->locks.user;
    ReportIf(ctx != c->ctx);
    fz_drop_context(ctx);
    for (int i = 0; i < FZ_LOCK_MAX; i++) {
        DeleteCriticalSection(&c->mutexes[i]);
    }
    delete c;
}

static Pixmap* PixmapFromJpegData(fz_context* ctx, const u8* data, int n) {
    int w = 0, h = 0, xres = 0, yres = 0;
    fz_colorspace* cs = nullptr;
    fz_stream* stm = nullptr;
    uint8_t orient = 0;

    fz_var(cs);
    fz_var(stm);
    fz_var(orient);

    fz_try(ctx) {
        fz_load_jpeg_info(ctx, data, n, &w, &h, &xres, &yres, &cs, &orient);
        stm = fz_open_memory(ctx, data, n);
        stm = fz_open_dctd(ctx, stm, -1, 1, 0, nullptr);
    }
    fz_catch(ctx) {
        fz_drop_colorspace(ctx, cs);
        cs = nullptr;
        fz_report_error(ctx);
    }

#ifndef PixelFormat32bppCMYK
#define PixelFormat32bppCMYK (15 | (32 << 8) | PixelFormatGDI)
#endif
    Gdiplus::PixelFormat fmt = fz_device_rgb(ctx) == cs    ? PixelFormat24bppRGB
                               : fz_device_gray(ctx) == cs ? PixelFormat24bppRGB
                               : fz_device_cmyk(ctx) == cs ? PixelFormat32bppCMYK
                                                           : PixelFormatUndefined;
    if (PixelFormatUndefined == fmt || w <= 0 || h <= 0 || !cs) {
        fz_drop_stream(ctx, stm);
        fz_drop_colorspace(ctx, cs);
        return nullptr;
    }

    // Decode into a temporary GDI+ bitmap (handles RGB/gray/CMYK), then copy out into a
    // uniform BGRA8 Pixmap. The fz JPEG path is a rare fallback (WIC handles most JPEGs),
    // so the extra copy is irrelevant and we reuse GDI+'s CMYK->RGB conversion on read.
    Gdiplus::Bitmap bmp(w, h, fmt);
    bmp.SetResolution(xres, yres);

    Gdiplus::Rect bmpRect(0, 0, w, h);
    Gdiplus::BitmapData bmpData;
    Gdiplus::Status ok = bmp.LockBits(&bmpRect, Gdiplus::ImageLockModeWrite, fmt, &bmpData);
    if (ok != Gdiplus::Ok) {
        fz_drop_stream(ctx, stm);
        fz_drop_colorspace(ctx, cs);
        return nullptr;
    }

    fz_var(bmp);
    fz_var(bmpRect);

    fz_try(ctx) {
        // decode straight into the locked GDI+ pixels, no intermediate buffer
        for (int y = 0; y < h; y++) {
            u8* line = (u8*)bmpData.Scan0 + y * bmpData.Stride;
            for (int x = 0; x < w; x++) {
                int read = (int)fz_read(ctx, stm, line, cs->n);
                if (read != cs->n) {
                    fz_throw(ctx, FZ_ERROR_GENERIC, "insufficient data for image");
                }
                if (3 == cs->n) { // RGB -> BGR
                    std::swap(line[0], line[2]);
                    line += 3;
                } else if (1 == cs->n) { // gray -> BGR
                    line[1] = line[2] = line[0];
                    line += 3;
                } else if (4 == cs->n) { // CMYK color inversion
                    for (int k = 0; k < 4; k++) {
                        line[k] = 255 - line[k];
                    }
                    line += 4;
                }
            }
        }
    }
    fz_always(ctx) {
        bmp.UnlockBits(&bmpData);
        fz_drop_stream(ctx, stm);
        fz_drop_colorspace(ctx, cs);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        return nullptr;
    }

    return PixmapFromGdiplus(&bmp);
}

static Pixmap* PixmapFromJp2Data(fz_context* ctx, const u8* data, int n) {
    fz_pixmap* pix = nullptr;

    fz_var(pix);

    fz_try(ctx) {
        pix = fz_load_jpx(ctx, data, n, nullptr);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        pix = nullptr;
    }
    if (!pix) {
        return nullptr;
    }

    int w = pix->w, h = pix->h;
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8);
    if (!px) {
        fz_drop_pixmap(ctx, pix);
        return nullptr;
    }
    px->xres = (float)pix->xres;
    px->yres = (float)pix->yres;

    // Zero-copy: borrow the Pixmap's buffer in an fz_pixmap (fz_new_pixmap_with_data does
    // not set FZ_PIXMAP_FLAG_FREE_SAMPLES) and convert the decoded image straight into it.
    // fz_device_bgr lays the samples out as B,G,R,A in memory, matching BGRA8. No
    // intermediate pixmap, no memcpy.
    fz_pixmap* dest = nullptr;
    fz_var(px);
    fz_var(dest);

    fz_try(ctx) {
        fz_colorspace* csdest = fz_device_bgr(ctx);
        dest = fz_new_pixmap_with_data(ctx, csdest, w, h, nullptr, 1, px->stride, px->data);
        fz_convert_pixmap_samples(ctx, pix, dest, nullptr, nullptr, fz_default_color_params, 0);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, dest);
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        FreePixmap(px);
        return nullptr;
    }

    return px;
}

static Pixmap* FzImageFromData(Str d) {
    const u8* data = (const u8*)d.s;
    size_t n = (size_t)d.len;
    if (n > INT_MAX || n < 12) {
        return nullptr;
    }

    fz_context* ctx = fz_new_context_windows();
    if (!ctx) {
        return nullptr;
    }

    Pixmap* result = nullptr;
    if (str::StartsWith(d, "\xFF\xD8")) {
        result = PixmapFromJpegData(ctx, data, (int)n);
    } else if (memeq(data, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", 12)) {
        result = PixmapFromJp2Data(ctx, data, (int)n);
    }

    fz_drop_context_windows(ctx);

    return result;
}

Pixmap* PixmapFromData(Str bmpData) {
    Pixmap* px = PixmapFromDataWin(bmpData);
    if (px) {
        return px;
    }
    return FzImageFromData(bmpData);
}

Vec<Pixmap*> PixmapsFromData(Str bmpData) {
    Vec<Pixmap*> res = PixmapsFromDataWin(bmpData);
    if (len(res) > 0) {
        return res;
    }
    // formats only the fz path decodes (JPEG/JP2) are single-frame
    Pixmap* px = FzImageFromData(bmpData);
    if (px) {
        res.Append(px);
    }
    return res;
}

RenderedBitmap* LoadRenderedBitmap(Str path) {
    if (!path) {
        return nullptr;
    }
    Gdiplus::Bitmap* bmp;
    {
        Str data = file::ReadFile(path);
        if (!data) {
            return nullptr;
        }
        bmp = NewGdiplusBitmapFromPixmap(PixmapFromData(data));
        str::Free(data);
        if (!bmp) {
            return nullptr;
        }
    }

    HBITMAP hbmp;
    RenderedBitmap* rendered = nullptr;
    if (bmp->GetHBITMAP((Gdiplus::ARGB)Gdiplus::Color::White, &hbmp) == Gdiplus::Ok) {
        rendered = new RenderedBitmap(hbmp, Size(bmp->GetWidth(), bmp->GetHeight()));
    }
    delete bmp;

    return rendered;
}
