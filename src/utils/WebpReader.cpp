/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WebpReader.h"

#ifndef NO_LIBWEBP

#include <webp/decode.h>

namespace webp {

// checks whether this could be data for a WebP image
bool HasSignature(const char* data, size_t len) {
    return len > 12 && str::StartsWith(data, "RIFF") && str::StartsWith(data + 8, "WEBP");
}

Gdiplus::Size SizeFromData(const char* data, size_t len) {
    Gdiplus::Size size;
    WebPGetInfo((const uint8_t*)data, len, &size.Width, &size.Height);
    return size;
}

Gdiplus::Bitmap* ImageFromData(const char* data, size_t len) {
    int w, h;
    if (!WebPGetInfo((const uint8_t*)data, len, &w, &h))
        return nullptr;

    Gdiplus::Bitmap bmp(w, h, PixelFormat32bppARGB);
    Gdiplus::Rect bmpRect(0, 0, w, h);
    Gdiplus::BitmapData bmpData;
    Gdiplus::Status ok = bmp.LockBits(&bmpRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Gdiplus::Ok)
        return nullptr;
    if (!WebPDecodeBGRAInto((const uint8_t*)data, len, (uint8_t*)bmpData.Scan0, bmpData.Stride * h, bmpData.Stride))
        return nullptr;
    bmp.UnlockBits(&bmpData);

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
}

} // namespace webp

#else

namespace webp {
bool HasSignature(const char* data, size_t len) {
    UNUSED(data);
    UNUSED(len);
    return false;
}
Gdiplus::Size SizeFromData(const char* data, size_t len) {
    UNUSED(data);
    UNUSED(len);
    return Gdiplus::Size();
}
Gdiplus::Bitmap* ImageFromData(const char* data, size_t len) {
    UNUSED(data);
    UNUSED(len);
    return nullptr;
}
} // namespace webp

#endif
