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

} // namespace webp

#else
namespace webp {
Pixmap* PixmapFromData(const Str&) {
    return nullptr;
}
} // namespace webp

#endif
