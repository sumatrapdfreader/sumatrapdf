/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Exif.h"
#include "base/Pixmap.h"
#include "AvifReader.h"

#if OS_WIN
#include "base/GdiPlusUtil.h"
#endif

#ifndef NO_AVIF

#include "heic.h"

// Set pixmap xres/yres from EXIF density. DisplayModel uses xres as fileDPI:
// zoomReal at 100% is screenDPI/fileDPI, so a missing density (default 96)
// makes photos with EXIF 72 or 300 DPI look the wrong physical size.
static void ApplyExifDensity(Pixmap* px, const ExifParser& parser) {
    if (!px) {
        return;
    }
    double dpiX = 0, dpiY = 0;
    if (!parser.GetFloatProp(ExifProp::XResolution, &dpiX) || !parser.GetFloatProp(ExifProp::YResolution, &dpiY) ||
        dpiX <= 0 || dpiY <= 0) {
        return;
    }
    // ResolutionUnit: 2 = inches (default), 3 = cm → convert to dpi.
    i64 unit = 2;
    parser.GetIntProp(ExifProp::ResolutionUnit, &unit);
    if (unit == 3) {
        dpiX *= 2.54;
        dpiY *= 2.54;
    }
    if (dpiX >= 1.0 && dpiX <= 10000.0) {
        px->xres = (float)dpiX;
    }
    if (dpiY >= 1.0 && dpiY <= 10000.0) {
        px->yres = (float)dpiY;
    }
}

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

    // EXIF density + orientation. heicdec returns decoded pixels without
    // applying density (defaults to 96 dpi); WIC/GDI+ honor EXIF resolution,
    // which is what DisplayModel uses for 100% zoom size.
    if (px) {
        u8* exif = nullptr;
        size_t n = 0;
        if (heic_doc_exif(doc, &exif, &n) != 0 && exif && n > 0) {
            ExifParser parser;
            if (parser.Parse(Str((const char*)exif, (int)n))) {
                ApplyExifDensity(px, parser);
#if OS_WIN
                i64 orient = 0;
                if (parser.GetIntProp(ExifProp::Orientation, &orient)) {
                    px = PixmapApplyExifOrientation(px, (int)orient);
                }
#endif
            }
            heic_free(ctx, exif);
        }
    }

    heic_doc_close(doc);
    heic_ctx_free(ctx);
    return px;
}

// Returns TIFF EXIF payload (caller frees *outData). Skips 4-byte HEIF Exif prefix.
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
// Returns TIFF EXIF payload (caller frees *outData). Skips 4-byte HEIF Exif prefix.
bool AvifExifBlobFromData(Str, u8**, size_t*) {
    return false;
}
#endif
