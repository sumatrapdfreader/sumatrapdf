/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/GuessFileType.h"
#include "base/GdiPlusUtil.h"

#ifndef NO_LIBWEBP
#include <webp/decode.h>
#endif

#include "WebpReader.h"

#ifndef NO_LIBWEBP

namespace webp {

Pixmap* PixmapFromData(const Str& d) {
    int w, h;
    if (!WebPGetInfo((const u8*)d.s, (size_t)d.len, &w, &h)) {
        return nullptr;
    }

    // decode BGRA straight into the Pixmap buffer (no intermediate bitmap, no copy)
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8);
    if (!px) {
        return nullptr;
    }
    if (!WebPDecodeBGRAInto((const u8*)d.s, (size_t)d.len, px->data, (size_t)px->stride * h, px->stride)) {
        FreePixmap(px);
        return nullptr;
    }
    return PixmapApplyExifOrientation(px, WebpExifOrientation(d));
}

// Decodes to RGB24, or RGBA32 (straight alpha) if the image has alpha, into
// the buffer allocDst returns. Fails for EXIF-rotated images: PixmapFromData
// handles those.
bool DecodeRgbInto(Str d, DecodeDstAllocFn allocDst, void* user) {
    int orientation = WebpExifOrientation(d);
    if (orientation > 1) {
        return false;
    }
    WebPBitstreamFeatures features{};
    if (WebPGetFeatures((const u8*)d.s, (size_t)d.len, &features) != VP8_STATUS_OK) {
        return false;
    }
    int w = features.width;
    int h = features.height;
    if (features.has_animation || w <= 0 || h <= 0) {
        return false;
    }
    bool hasAlpha = features.has_alpha != 0;
    int stride = 0;
    u8* dst = allocDst(user, w, h, hasAlpha, &stride);
    if (!dst) {
        return false;
    }
    size_t size = (size_t)stride * h;
    if (hasAlpha) {
        return WebPDecodeRGBAInto((const u8*)d.s, (size_t)d.len, dst, size, stride) != nullptr;
    }
    return WebPDecodeRGBInto((const u8*)d.s, (size_t)d.len, dst, size, stride) != nullptr;
}

} // namespace webp

#else
namespace webp {
Pixmap* PixmapFromData(const Str&) {
    return nullptr;
}
bool DecodeRgbInto(Str, DecodeDstAllocFn, void*) {
    return false;
}
} // namespace webp

#endif
