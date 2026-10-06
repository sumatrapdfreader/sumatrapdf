/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Exif.h"
#include "base/Pixmap.h"

#if OS_WIN
#include "base/GdiPlusUtil.h"
#endif

#ifndef NO_AVIF
#include "heic.h"
#endif

#include "AvifReader.h"

#ifndef NO_AVIF

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

    return px;
}

#else
Pixmap* PixmapFromAvifData(Str) {
    return nullptr;
}
#endif
