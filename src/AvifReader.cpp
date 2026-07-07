/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "AvifReader.h"

#ifndef NO_AVIF

#include <libheif/heif.h>

Size AvifSizeFromData(Str d) {
    Size res;
    struct heif_image_handle* hdl = nullptr;

    heif_context* ctx = heif_context_alloc();

    // TODO: can I provide a subset of the image?
    auto err = heif_context_read_from_memory_without_copy(ctx, (const void*)(u8*)d.s, (size_t)d.len, nullptr);
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

Pixmap* PixmapFromAvifData(Str d) {
    Pixmap* px = nullptr;
    struct heif_image_handle* hdl = nullptr;
    struct heif_image* img = nullptr;
    int dx, dy, srcStride;
    // int hasAlpha;
    const u8* data = nullptr;
    heif_chroma chroma;
    heif_colorspace cs;

    heif_context* ctx = heif_context_alloc();
    auto err = heif_context_read_from_memory_without_copy(ctx, (const void*)(u8*)d.s, (size_t)d.len, nullptr);

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

    // expand interleaved RGB into a BGRA8 Pixmap (opaque alpha)
    px = AllocPixmap(dx, dy, PixmapFormat::BGRA8);
    if (px) {
        int dstStride = px->stride;
        u8* src = (u8*)data;
        u8* dst = px->data;
        for (int i = 0; i < dy; i++) {
            u8* srcTmp = src;
            u8* dstTmp = dst;
            for (int j = 0; j < dx; j++) {
                *dst++ = src[2]; // B
                *dst++ = src[1]; // G
                *dst++ = src[0]; // R
                *dst++ = 255;    // A (opaque)
                src += 3;
            }
            src = srcTmp + srcStride;
            dst = dstTmp + dstStride;
        }
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
    return px;
}

bool AvifExifBlobFromData(Str d, u8** outData, size_t* outSize) {
    *outData = nullptr;
    *outSize = 0;
    heif_context* ctx = heif_context_alloc();
    if (!ctx) {
        return false;
    }
    bool ok = false;
    heif_image_handle* hdl = nullptr;
    u8* buf = nullptr;
    auto err = heif_context_read_from_memory_without_copy(ctx, (u8*)d.s, (size_t)d.len, nullptr);
    if (err.code == heif_error_Ok) {
        err = heif_context_get_primary_image_handle(ctx, &hdl);
    }
    if (err.code == heif_error_Ok && hdl) {
        int n = heif_image_handle_get_number_of_metadata_blocks(hdl, "Exif");
        heif_item_id id = 0;
        if (n > 0 && heif_image_handle_get_list_of_metadata_block_IDs(hdl, "Exif", &id, 1) == 1) {
            size_t sz = heif_image_handle_get_metadata_size(hdl, id);
            if (sz > 4) {
                buf = (u8*)malloc(sz);
                if (buf) {
                    err = heif_image_handle_get_metadata(hdl, id, buf);
                    if (err.code == heif_error_Ok) {
                        size_t payload = sz - 4;
                        u8* copy = (u8*)malloc(payload);
                        if (copy) {
                            memcpy(copy, buf + 4, payload);
                            *outData = copy;
                            *outSize = payload;
                            ok = true;
                        }
                    }
                }
            }
        }
    }
    free(buf);
    if (hdl) {
        heif_image_handle_release(hdl);
    }
    heif_context_free(ctx);
    return ok;
}
#else
Size AvifSizeFromData(Str) {
    return {};
}
Pixmap* PixmapFromAvifData(Str) {
    return nullptr;
}
bool AvifExifBlobFromData(Str, u8**, size_t*) {
    return false;
}
#endif
