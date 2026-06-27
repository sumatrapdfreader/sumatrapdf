/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Pixmap.h"
#include "utils/GdiPlusUtil.h"
#include "utils/WebpReader.h"

#ifndef NO_LIBWEBP

#include <webp/decode.h>

namespace webp {

// checks whether this could be data for a WebP image
bool HasSignature(const ByteSlice& d) {
    if (d.size() <= 12) {
        return false;
    }
    char* data = (char*)d.data();
    return str::StartsWith(data, "RIFF") && str::StartsWith(data + 8, "WEBP");
}

Size SizeFromData(const ByteSlice& d) {
    Size size;
    WebPGetInfo((const u8*)d.data(), d.size(), &size.dx, &size.dy);
    return size;
}

Pixmap* PixmapFromData(const ByteSlice& d) {
    int w, h;
    if (!WebPGetInfo((const u8*)d.data(), d.size(), &w, &h)) {
        return nullptr;
    }

    // decode BGRA straight into the Pixmap buffer (no intermediate bitmap, no copy)
    Pixmap* px = AllocPixmap(w, h, PixmapFormat::BGRA8);
    if (!px) {
        return nullptr;
    }
    if (!WebPDecodeBGRAInto((const u8*)d.data(), d.size(), px->data, (size_t)px->stride * h, px->stride)) {
        FreePixmap(px);
        return nullptr;
    }
    return PixmapApplyExifOrientation(px, WebpExifOrientation(d));
}

} // namespace webp

#else
namespace webp {
bool HasSignature(const ByteSlice&) {
    return false;
}
Size SizeFromData(const ByteSlice&) {
    return Size();
}
Pixmap* PixmapFromData(const ByteSlice&) {
    return nullptr;
}
} // namespace webp

#endif
