/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/AvifReader.h"

#ifndef NO_AVIF

#include <libheif/heif.h>

Size AvifSizeFromData(const ByteSlice& d) {
    Size res;
    struct heif_image_handle* hdl = nullptr;

    heif_context* ctx = heif_context_alloc();

    // TODO: can I provide a subset of the image?
    auto err = heif_context_read_from_memory_without_copy(ctx, (const void*)d.Get(), d.size(), nullptr);
    if (err.code != heif_error_Ok) {
        goto Exit;
    }
    err = heif_context_get_primary_image_handle(ctx, &hdl);
    if (err.code != heif_error_Ok) {
        goto Exit;
    }
    res.dx = heif_image_handle_get_width(hdl);
    res.dy = heif_image_handle_get_height(hdl);

Exit:
    if (hdl) {
        heif_image_handle_release(hdl);
    }
    heif_context_free(ctx);
    return res;
}

Gdiplus::Bitmap* AvifImageFromData(const ByteSlice& d) {
    Gdiplus::Bitmap* bmp = nullptr;
    struct heif_image_handle* hdl = nullptr;
    struct heif_image* img = nullptr;
    int dx, dy, srcStride;
    // int hasAlpha;
    const u8* data = nullptr;
    heif_chroma chroma;
    heif_colorspace cs;

    heif_context* ctx = heif_context_alloc();
    auto err = heif_context_read_from_memory_without_copy(ctx, (const void*)d.Get(), d.size(), nullptr);

    if (err.code != heif_error_Ok) {
        goto Exit;
    }

    err = heif_context_get_primary_image_handle(ctx, &hdl);
    if (err.code != heif_error_Ok) {
        goto Exit;
    }

    dx = heif_image_handle_get_width(hdl);
    dy = heif_image_handle_get_height(hdl);

    // hasAlpha = heif_image_handle_has_alpha_channel(hdl);
    // chroma = hasAlpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB;
    chroma = heif_chroma_interleaved_RGB;
    cs = heif_colorspace_RGB; // TODO: can I do it or do I have to match alpha?
    err = heif_decode_image(hdl, &img, cs, chroma, nullptr);

    if (err.code != heif_error_Ok) {
        goto Exit;
    }

    data = heif_image_get_plane_readonly(img, heif_channel_interleaved, &srcStride);
    if (!data) {
        goto Exit;
    }

    bmp = new Gdiplus::Bitmap(dx, dy, PixelFormat32bppRGB);
    {
        Gdiplus::Rect bmpRect(0, 0, dx, dy);
        Gdiplus::BitmapData bmpData;
        Gdiplus::Status ok = bmp->LockBits(&bmpRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppRGB, &bmpData);
        if (ok != Gdiplus::Ok) {
            return nullptr;
        }
        int dstStride = bmpData.Stride;
        u8* src = (u8*)data;
        u8* dst = (u8*)bmpData.Scan0;
        for (int i = 0; i < dy; i++) {
            u8* srcTmp = src;
            u8* dstTmp = dst;
            // TODO: memcpy?
            for (int j = 0; j < dx; j++) {
                *dst++ = src[2];
                *dst++ = src[1];
                *dst++ = src[0];
                dst++;
                src += 3;
            }
            src = srcTmp + srcStride;
            dst = dstTmp + dstStride;
        }
        bmp->UnlockBits(&bmpData);
    }

Exit:
    if (img) {
        heif_image_release(img);
    }
    if (hdl) {
        heif_image_handle_release(hdl);
    }
    if (ctx) {
        heif_context_free(ctx);
    }
    return bmp;
}
#else
Size AvifSizeFromData(const ByteSlice&) {
    return {};
}
Gdiplus::Bitmap* AvifImageFromData(const ByteSlice&) {
    return nullptr;
}
#endif
