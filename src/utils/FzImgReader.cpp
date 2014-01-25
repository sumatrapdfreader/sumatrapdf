/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef NO_LIBMUPDF

extern "C" {
#include <mupdf/fitz.h>
}

#include "FzImgReader.h"

// interaction between '_setjmp' and C++ object destruction is non-portable
#pragma warning(disable: 4611)

using namespace Gdiplus;

namespace fitz {

Bitmap *ImageFromData(const char *data, size_t len)
{
    if (len > INT_MAX)
        return NULL;

    fz_context *ctx = fz_new_context(NULL, NULL, 0);
    if (!ctx)
        return NULL;

    int w, h, xres, yres;

    fz_image *img = NULL;
    fz_buffer *buf = NULL;
    fz_pixmap *pix, *pix_argb;

    fz_var(img);
    fz_var(buf);

    fz_try(ctx) {
        buf = fz_new_buffer(ctx, (int)len);
        memcpy(buf->data, data, (buf->len = (int)len));
        img = fz_new_image_from_buffer(ctx, buf);
        w = img->w; h = img->h; xres = img->xres; yres = img->yres;
        pix = fz_new_pixmap_from_image(ctx, img, 0, 0);
    }
    fz_always(ctx) {
        fz_drop_image(ctx, img);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx) {
        pix = NULL;
    }
    CrashIf(pix && (w != pix->w || h != pix->h));
    if (!pix || w != pix->w || h != pix->h) {
        fz_drop_pixmap(ctx, pix);
        fz_free_context(ctx);
        return NULL;
    }

    Bitmap bmp(w, h, PixelFormat32bppARGB);
    Rect bmpRect(0, 0, w, h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Ok || bmpData.Stride != w * 4) {
        CrashIf(Ok == ok && bmpData.Stride != w * 4);
        fz_drop_pixmap(ctx, pix);
        fz_free_context(ctx);
        return NULL;
    }
    bmp.SetResolution(xres, yres);

    fz_try(ctx) {
        pix_argb = fz_new_pixmap_with_data(ctx, fz_device_bgr(ctx), w, h, (unsigned char *)bmpData.Scan0);
        fz_convert_pixmap(ctx, pix_argb, pix);
    }
    fz_always(ctx) {
        fz_drop_pixmap(ctx, pix);
    }
    fz_catch(ctx) {
        pix_argb = NULL;
    }
    bmp.UnlockBits(&bmpData);

    Bitmap *result = NULL;
    if (pix_argb) {
        // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
        result = bmp.Clone(0, 0, pix_argb->w, pix_argb->h, PixelFormat32bppARGB);
    }

    fz_drop_pixmap(ctx, pix_argb);
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
