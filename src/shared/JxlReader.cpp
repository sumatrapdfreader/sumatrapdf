/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "jxl.h"

#include "JxlReader.h"

namespace jxl {

Pixmap* PixmapFromData(Str d) {
    // JXL container format starts with a 0 byte
    if (len(d) == 0) {
        return nullptr;
    }
    jxl_ctx* ctx = jxl_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!ctx) {
        return nullptr;
    }
    AutoCall freeCtx(jxl_ctx_free, ctx);
    // Decode BGRA in sRGB for display; linear light renders too dark (#5919).
    jxl_ctx_set_bgr(ctx, 1);
    jxl_ctx_set_srgb_output(ctx, 1);
    jxl_image* img = jxl_decode(ctx, (const u8*)d.s, (size_t)d.len, JXLDEC_FORMAT_RGBA32);
    AutoCall freeImage(jxl_image_destroy, ctx, img);
    if (!img || !img->data || img->width <= 0 || img->height <= 0) {
        return nullptr;
    }
    Pixmap* px = AllocPixmap(img->width, img->height, PixmapFormat::BGRA8);
    if (px) {
        CopyPixmapRows(px, img->data, img->stride);
    }
    return px;
}

// Decodes to RGB24, or RGBA32 (straight alpha) if the image has alpha, into
// the buffer allocDst returns. Skips the copy PixmapFromData makes.
bool DecodeRgbInto(Str d, DecodeDstAllocFn allocDst, void* user) {
    if (len(d) == 0) {
        return false;
    }
    jxl_ctx* ctx = jxl_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!ctx) {
        return false;
    }
    AutoCall freeCtx(jxl_ctx_free, ctx);
    jxl_ctx_set_srgb_output(ctx, 1);
    jxl_doc* doc = jxl_doc_open(ctx, (const u8*)d.s, (size_t)d.len);
    AutoCall closeDoc(jxl_doc_close, doc);
    jxl_image_info info{};
    if (!doc || jxl_doc_info(doc, &info) != 0) {
        return false;
    }
    bool hasAlpha = info.alpha_bits > 0;
    jxl_format fmt = hasAlpha ? JXLDEC_FORMAT_RGBA32 : JXLDEC_FORMAT_RGB24;
    jxl_render_info ri{};
    if (jxl_frame_render_info(doc, 0, fmt, &ri) != 0 || ri.width <= 0 || ri.height <= 0) {
        return false;
    }
    int stride = 0;
    u8* dst = allocDst(user, ri.width, ri.height, hasAlpha, &stride);
    return dst && jxl_frame_render_into(doc, 0, fmt, dst, stride) == 0;
}

} // namespace jxl
