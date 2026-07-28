/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "AvifReader.h"

#ifndef NO_AVIF

#include "heic.h"

Size AvifSizeFromData(Str d) {
    Size res;

    heic_ctx* ctx = heic_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!ctx) {
        return res;
    }
    heic_doc* doc = heic_doc_open(ctx, (const u8*)d.s, (size_t)d.len);
    if (doc) {
        heic_image_info info{};
        if (heic_doc_info(doc, &info) == 0) {
            res.dx = (int)info.width;
            res.dy = (int)info.height;
        }
        heic_doc_close(doc);
    }
    heic_ctx_free(ctx);
    return res;
}

Pixmap* PixmapFromAvifData(Str d) {
    Pixmap* px = nullptr;

    heic_ctx* ctx = heic_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!ctx) {
        return nullptr;
    }
    heic_doc* doc = heic_doc_open(ctx, (const u8*)d.s, (size_t)d.len);
    if (!doc) {
        heic_ctx_free(ctx);
        return nullptr;
    }

    // decode straight to BGRA for PixmapFormat::BGRA8
    heic_image* img = heic_doc_decode(doc, HEIC_FORMAT_BGRA);
    if (img && img->data) {
        int dx = (int)img->width;
        int dy = (int)img->height;
        px = AllocPixmap(dx, dy, PixmapFormat::BGRA8);
        if (px) {
            int srcStride = img->stride;
            int dstStride = px->stride;
            u8* src = img->data;
            u8* dst = px->data;
            int rowBytes = dx * 4;
            for (int y = 0; y < dy; y++) {
                memcpy(dst, src, (size_t)rowBytes);
                src += srcStride;
                dst += dstStride;
            }
        }
    }

    if (img) {
        heic_image_destroy(ctx, img);
    }
    heic_doc_close(doc);
    heic_ctx_free(ctx);
    return px;
}

bool AvifExifBlobFromData(Str d, u8** outData, size_t* outSize) {
    *outData = nullptr;
    *outSize = 0;

    heic_ctx* ctx = heic_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!ctx) {
        return false;
    }
    heic_doc* doc = heic_doc_open(ctx, (const u8*)d.s, (size_t)d.len);
    if (!doc) {
        heic_ctx_free(ctx);
        return false;
    }

    // TIFF payload; HEIF 4-byte prefix already stripped by heic_doc_exif.
    // heic allocates with a size header (must free via heic_free); copy out so
    // callers can free() with the ordinary allocator.
    u8* exif = nullptr;
    size_t n = 0;
    bool ok = heic_doc_exif(doc, &exif, &n) != 0 && exif && n > 0;
    if (ok) {
        u8* copy = (u8*)malloc(n);
        if (copy) {
            memcpy(copy, exif, n);
            *outData = copy;
            *outSize = n;
        } else {
            ok = false;
        }
        heic_free(ctx, exif);
    }

    heic_doc_close(doc);
    heic_ctx_free(ctx);
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
