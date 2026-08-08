/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "JxlReader.h"

#ifndef NO_LIBJXL

#include "jxl.h"

namespace jxl {

// jxldec detects both the raw JPEG XL codestream and the ISOBMFF container form
bool HasSignature(Str d) {
    jxl_signature sig = jxl_signature_check((const u8*)d.s, (size_t)d.len);
    return sig == JXLDEC_SIG_CODESTREAM || sig == JXLDEC_SIG_CONTAINER;
}

Pixmap* PixmapFromData(Str d) {
    // JXL container format starts with a 0 byte
    if (len(d) == 0) {
        return nullptr;
    }
    jxl_ctx* ctx = jxl_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!ctx) {
        return nullptr;
    }
    // Decode straight to BGRA for PixmapFormat::BGRA8 (no channel swizzle).
    jxl_ctx_set_bgr(ctx, 1);
    // We blit the pixels to an sRGB display as-is, so ask for sRGB rather than
    // whatever the file declares. Images encoded in linear light otherwise come
    // out dark and over-saturated (issue #5919).
    jxl_ctx_set_srgb_output(ctx, 1);
    jxl_image* img = jxl_decode(ctx, (const u8*)d.s, (size_t)d.len, JXLDEC_FORMAT_RGBA32);
    Pixmap* px = nullptr;
    if (img && img->data && img->width > 0 && img->height > 0) {
        int w = img->width;
        int h = img->height;
        px = AllocPixmap(w, h, PixmapFormat::BGRA8);
        if (px) {
            int srcStride = img->stride;
            int dstStride = px->stride;
            int rowBytes = w * 4;
            u8* src = img->data;
            u8* dst = px->data;
            for (int y = 0; y < h; y++) {
                memcpy(dst, src, (size_t)rowBytes);
                src += srcStride;
                dst += dstStride;
            }
        }
    }
    if (img) {
        jxl_image_destroy(ctx, img);
    }
    jxl_ctx_free(ctx);
    return px;
}

Size SizeFromData(Str d) {
    Size size;
    if (len(d) == 0) {
        return size;
    }
    jxl_ctx* ctx = jxl_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!ctx) {
        return size;
    }
    int w = 0, h = 0;
    if (jxl_decode_size(ctx, (const u8*)d.s, (size_t)d.len, &w, &h) == 0) {
        size = Size(w, h);
    }
    jxl_ctx_free(ctx);
    return size;
}

} // namespace jxl

#else

namespace jxl {
bool HasSignature(Str) {
    return false;
}
Size SizeFromData(Str) {
    return Size();
}
Pixmap* PixmapFromData(Str) {
    return nullptr;
}
} // namespace jxl

#endif
