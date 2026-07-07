/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"

#ifdef _MSC_VER
#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable
#endif

extern "C" {
#include "mupdf/fitz.h"
#include "../mupdf/source/fitz/color-imp.h"
}

#include "FzImgReader.h"

struct MupdfContext {
    fz_locks_context fz_locks_ctx{};
    Mutex mutexes[FZ_LOCK_MAX];
    fz_context* ctx = nullptr;
};

static void fz_lock_context_cs(void* user, int lock) {
    MupdfContext* ctx = (MupdfContext*)user;
    ctx->mutexes[lock].Lock();
}

static void fz_unlock_context_cs(void* user, int lock) {
    MupdfContext* ctx = (MupdfContext*)user;
    ctx->mutexes[lock].Unlock();
}

// route mupdf's warnings/errors through our log() instead of the default
// callback, which does fputs() to stderr; that first fputs makes the CRT
// allocate a stdio buffer it never frees, which shows up as a leak
static void fz_log_cb(void*, const char* msg) {
    log(Str(msg));
}

fz_context* fz_new_context_windows(size_t maxStore) {
    auto c = new MupdfContext();
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
    delete c;
}

static Pixmap* PixmapFromFzPixmap(fz_context* ctx, fz_pixmap* pix);

static Pixmap* PixmapFromImageData(fz_context* ctx, const u8* data, size_t n) {
    fz_buffer* buf = nullptr;
    fz_image* img = nullptr;
    fz_pixmap* pix = nullptr;

    fz_var(buf);
    fz_var(img);
    fz_var(pix);

    fz_try(ctx) {
        buf = fz_new_buffer_from_shared_data(ctx, data, n);
        img = fz_new_image_from_buffer(ctx, buf);
        pix = fz_get_pixmap_from_image(ctx, img, nullptr, nullptr, nullptr, nullptr);
    }
    fz_always(ctx) {
        fz_drop_image(ctx, img);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        fz_drop_pixmap(ctx, pix);
        return nullptr;
    }

    return pix ? PixmapFromFzPixmap(ctx, pix) : nullptr;
}

static Pixmap* PixmapFromFzPixmap(fz_context* ctx, fz_pixmap* pix) {
    int w = pix->w;
    int h = pix->h;
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8);
    if (!px) {
        fz_drop_pixmap(ctx, pix);
        return nullptr;
    }
    px->xres = (float)pix->xres;
    px->yres = (float)pix->yres;

    // Zero-copy: borrow the Pixmap's buffer in an fz_pixmap and convert the decoded image
    // straight into it. fz_device_bgr lays the samples out as B,G,R,A, matching BGRA8.
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

Pixmap* PixmapFromDataFz(Str d) {
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
        result = PixmapFromImageData(ctx, data, n);
    } else if (memeq(data, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", 12)) {
        result = PixmapFromImageData(ctx, data, n);
    }

    fz_drop_context_windows(ctx);

    return result;
}
