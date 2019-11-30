/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable

#ifndef NO_LIBMUPDF
extern "C" {
#include <mupdf/fitz.h>
}
#endif

#include "utils/BaseUtil.h"
#include "FzImgReader.h"

#ifndef NO_LIBMUPDF

using namespace Gdiplus;

namespace fitz {

static Bitmap* ImageFromJpegData(fz_context* ctx, const char* data, int len) {
    int w = 0, h = 0, xres = 0, yres = 0;
    fz_colorspace* cs = nullptr;
    fz_stream* stm = nullptr;

    fz_var(cs);
    fz_var(stm);

    fz_try(ctx) {
        fz_load_jpeg_info(ctx, (unsigned char*)data, len, &w, &h, &xres, &yres, &cs);
        stm = fz_open_memory(ctx, (unsigned char*)data, len);
        stm = fz_open_dctd(ctx, stm, -1, 0, nullptr);
    }
    fz_catch(ctx) {
        fz_drop_colorspace(ctx, cs);
        cs = nullptr;
    }

    PixelFormat fmt = fz_device_rgb(ctx) == cs
                          ? PixelFormat24bppRGB
                          : fz_device_gray(ctx) == cs
                                ? PixelFormat24bppRGB
                                : fz_device_cmyk(ctx) == cs ? PixelFormat32bppCMYK : PixelFormatUndefined;
    if (PixelFormatUndefined == fmt || w <= 0 || h <= 0 || !cs) {
        fz_drop_stream(ctx, stm);
        fz_drop_colorspace(ctx, cs);
        return nullptr;
    }

    Bitmap bmp(w, h, fmt);
    bmp.SetResolution(xres, yres);

    Rect bmpRect(0, 0, w, h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, ImageLockModeWrite, fmt, &bmpData);
    if (ok != Ok) {
        fz_drop_stream(ctx, stm);
        fz_drop_colorspace(ctx, cs);
        return nullptr;
    }

    fz_var(bmp);
    fz_var(bmpRect);

    fz_try(ctx) {
        for (int y = 0; y < h; y++) {
            unsigned char* line = (unsigned char*)bmpData.Scan0 + y * bmpData.Stride;
            for (int x = 0; x < w; x++) {
                int read = fz_read(ctx, stm, line, cs->n);
                if (read != cs->n)
                    fz_throw(ctx, FZ_ERROR_GENERIC, "insufficient data for image");
                if (3 == cs->n) { // RGB -> BGR
                    std::swap(line[0], line[2]);
                    line += 3;
                } else if (1 == cs->n) { // gray -> BGR
                    line[1] = line[2] = line[0];
                    line += 3;
                } else if (4 == cs->n) { // CMYK color inversion
                    for (int k = 0; k < 4; k++)
                        line[k] = 255 - line[k];
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
        return nullptr;
    }

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, fmt);
}

static Bitmap* ImageFromJp2Data(fz_context* ctx, const char* data, int len) {
    fz_pixmap* pix = nullptr;
    fz_pixmap* pix_argb = nullptr;

    fz_var(pix);
    fz_var(pix_argb);

    fz_try(ctx) {
        pix = fz_load_jpx(ctx, (unsigned char*)data, len, nullptr);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    int w = pix->w, h = pix->h;
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    bmp.SetResolution(pix->xres, pix->yres);

    Rect bmpRect(0, 0, w, h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Ok) {
        fz_drop_pixmap(ctx, pix);
        return nullptr;
    }

    fz_var(bmp);
    fz_var(bmpRect);

    fz_try(ctx) {
        unsigned char* data = (unsigned char*)bmpData.Scan0;
        // TODO(port): figure out seps, alpha and stride
        CrashMe();
        fz_separations* seps = nullptr;
        int alpha = 0;
        int stride = w;
        fz_colorspace* cs = fz_device_bgr(ctx);
        fz_colorspace* cs_des = fz_device_bgr(ctx);
        fz_colorspace* prf = nullptr;
        fz_default_colorspaces* default_cs = nullptr;
        fz_color_params colparms = fz_default_color_params;
        pix_argb = fz_new_pixmap_with_data(ctx, cs, w, h, seps, alpha, stride, data);
        // TODO(port): should this be fz_convert_pixmap_samples
        fz_convert_pixmap(ctx, pix_argb, cs, nullptr, nullptr, colparms, 1);
    }
    fz_always(ctx) {
        bmp.UnlockBits(&bmpData);
        fz_drop_pixmap(ctx, pix);
        fz_drop_pixmap(ctx, pix_argb);
    }
    fz_catch(ctx) {
        return nullptr;
    }

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
}

Bitmap* ImageFromData(const char* data, size_t len) {
    if (len > INT_MAX || len < 12)
        return nullptr;

    fz_context* ctx = fz_new_context(nullptr, nullptr, 0);
    if (!ctx)
        return nullptr;

    Bitmap* result = nullptr;
    if (str::StartsWith(data, "\xFF\xD8"))
        result = ImageFromJpegData(ctx, data, (int)len);
    else if (memeq(data, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", 12))
        result = ImageFromJp2Data(ctx, data, (int)len);

    fz_drop_context(ctx);

    return result;
}

} // namespace fitz

#else

namespace fitz {
Gdiplus::Bitmap* ImageFromData(const char* data, size_t len) {
    UNUSED(data);
    UNUSED(len);
    return nullptr;
}
} // namespace fitz

#endif
