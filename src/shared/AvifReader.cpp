/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Exif.h"
#include "base/Pixmap.h"

#if OS_WIN
#include "base/GdiPlusUtil.h"
#endif

#include "heic.h"

#include "AvifReader.h"

// Apply EXIF density so 100% zoom uses the photo's physical size.
static void ApplyExifDensity(Pixmap* px, const ExifParser& parser) {
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

Pixmap* PixmapFromAvifData(Str d) {
    Pixmap* px = nullptr;

    heic_ctx* ctx = heic_ctx_new(nullptr, nullptr, nullptr, nullptr);
    if (!ctx) {
        return nullptr;
    }
    AutoCall freeCtx(heic_ctx_free, ctx);
    heic_doc* doc = heic_doc_open(ctx, (const u8*)d.s, (size_t)d.len);
    if (!doc) {
        return nullptr;
    }
    AutoCall closeDoc(heic_doc_close, doc);

    // decode straight to BGRA for PixmapFormat::BGRA8
    heic_image* img = heic_doc_decode(doc, HEIC_FORMAT_BGRA);
    if (img && img->data) {
        int dx = (int)img->width;
        int dy = (int)img->height;
        px = AllocPixmap(dx, dy, PixmapFormat::BGRA8);
        if (px) {
            CopyPixmapRows(px, img->data, img->stride);
        }
    }

    heic_image_destroy(ctx, img);

    if (!px) {
        return nullptr;
    }

    // Match the EXIF density and orientation used by Windows image decoders.
    u8* exif = nullptr;
    size_t n = 0;
    if (heic_doc_exif(doc, &exif, &n) == 0 || !exif || n == 0) {
        return px;
    }

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
    return px;
}
