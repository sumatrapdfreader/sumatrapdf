/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef NO_LIBMUPDF

// interaction between '_setjmp' and C++ object destruction is non-portable
#pragma warning(disable: 4611)

extern "C" {
#include <mupdf/fitz.h>
}

#include "FzImgReader.h"

using namespace Gdiplus;

namespace fitz {

static Bitmap *ImageFromJpegData(fz_context *ctx, const char *data, int len)
{
    int w = 0, h = 0, xres = 0, yres = 0;
    fz_colorspace *cs = NULL;
    fz_stream *stm = NULL;

    fz_var(cs);
    fz_var(stm);

    fz_try(ctx) {
        fz_load_jpeg_info(ctx, (unsigned char *)data, len, &w, &h, &xres, &yres, &cs);
        stm = fz_open_memory(ctx, (unsigned char *)data, len);
        stm = fz_open_dctd(stm, -1, 0, NULL);
    }
    fz_catch(ctx) {
        fz_drop_colorspace(ctx, cs);
        cs = NULL;
    }

    PixelFormat fmt = fz_device_rgb(ctx) == cs ? PixelFormat32bppARGB :
                      fz_device_gray(ctx) == cs ? PixelFormat32bppARGB :
                      fz_device_cmyk(ctx) == cs ? PixelFormat32bppCMYK :
                      PixelFormatUndefined;
    if (PixelFormatUndefined == fmt || w <= 0 || h <= 0 || !cs) {
        fz_close(stm);
        fz_drop_colorspace(ctx, cs);
        return NULL;
    }

    Bitmap bmp(w, h, fmt);
    bmp.SetResolution(xres, yres);

    Rect bmpRect(0, 0, w, h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, ImageLockModeWrite, fmt, &bmpData);
    if (ok != Ok) {
        fz_close(stm);
        fz_drop_colorspace(ctx, cs);
        return NULL;
    }

    fz_var(bmp);
    fz_var(bmpRect);

    fz_try(ctx) {
        for (int y = 0; y < h; y++) {
            unsigned char *line = (unsigned char *)bmpData.Scan0 + y * bmpData.Stride;
            for (int x = 0; x < w * 4; x += 4) {
                int read = fz_read(stm, line + x, cs->n);
                if (read != cs->n)
                    fz_throw(ctx, FZ_ERROR_GENERIC, "insufficient data for image");
                if (3 == cs->n) { // RGB -> BGRA
                    Swap(line[x], line[x + 2]);
                    line[x + 3] = 0xFF;
                }
                else if (1 == cs->n) { // gray -> BGRA
                    line[x + 1] = line[x + 2] = line[x];
                    line[x + 3] = 0xFF;
                }
                else if (4 == cs->n) { // CMYK color inversion
                    for (int k = 0; k < 4; k++)
                        line[x + k] = 255 - line[x + k];
                }
            }
        }
    }
    fz_always(ctx) {
        bmp.UnlockBits(&bmpData);
        fz_close(stm);
        fz_drop_colorspace(ctx, cs);
    }
    fz_catch(ctx) {
        return NULL;
    }

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, fmt);
}

static Bitmap *ImageFromJp2Data(fz_context *ctx, const char *data, int len)
{
    fz_pixmap *pix = NULL;
    fz_pixmap *pix_argb = NULL;

    fz_var(pix);
    fz_var(pix_argb);

    fz_try(ctx) {
        pix = fz_load_jpx(ctx, (unsigned char *)data, len, NULL, 0);
    }
    fz_catch(ctx) {
        return NULL;
    }

    int w = pix->w, h = pix->h;
    Bitmap bmp(w, h, PixelFormat32bppARGB);
    bmp.SetResolution(pix->xres, pix->yres);

    Rect bmpRect(0, 0, w, h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Ok) {
        fz_drop_pixmap(ctx, pix);
        return NULL;
    }

    fz_var(bmp);
    fz_var(bmpRect);

    fz_try(ctx) {
        pix_argb = fz_new_pixmap_with_data(ctx, fz_device_bgr(ctx), w, h, (unsigned char *)bmpData.Scan0);
        fz_convert_pixmap(ctx, pix_argb, pix);
    }
    fz_always(ctx) {
        bmp.UnlockBits(&bmpData);
        fz_drop_pixmap(ctx, pix);
        fz_drop_pixmap(ctx, pix_argb);
    }
    fz_catch(ctx) {
        return NULL;
    }

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
}

Bitmap *ImageFromData(const char *data, size_t len)
{
    if (len > INT_MAX || len < 12)
        return NULL;

    fz_context *ctx = fz_new_context(NULL, NULL, 0);
    if (!ctx)
        return NULL;

    Bitmap *result = NULL;
    if (str::StartsWith(data, "\xFF\xD8"))
        result = ImageFromJpegData(ctx, data, (int)len);
    else if (memeq(data, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", 12))
        result = ImageFromJp2Data(ctx, data, (int)len);

    fz_free_context(ctx);

    return result;
}

}

#else

#include "FzImgReader.h"

namespace fitz {
Gdiplus::Bitmap *ImageFromData(const char *data, size_t len) { return NULL; }
}

#endif
