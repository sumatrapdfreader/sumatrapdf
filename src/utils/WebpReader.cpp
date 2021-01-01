/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WebpReader.h"

#ifndef NO_LIBWEBP

#include <webp/decode.h>

namespace webp {

// checks whether this could be data for a WebP image
bool HasSignature(std::span<u8> d) {
    if (d.size() <= 12) {
        return false;
    }
    char* data = (char*)d.data();
    return str::StartsWith(data, "RIFF") && str::StartsWith(data + 8, "WEBP");
}

Size SizeFromData(std::span<u8> d) {
    Size size;
    WebPGetInfo((const u8*)d.data(), d.size(), &size.dx, &size.dy);
    return size;
}

Gdiplus::Bitmap* ImageFromData(std::span<u8> d) {
    int w, h;
    if (!WebPGetInfo((const u8*)d.data(), d.size(), &w, &h)) {
        return nullptr;
    }

    Gdiplus::Bitmap bmp(w, h, PixelFormat32bppARGB);
    Gdiplus::Rect bmpRect(0, 0, w, h);
    Gdiplus::BitmapData bmpData;
    Gdiplus::Status ok = bmp.LockBits(&bmpRect, Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Gdiplus::Ok) {
        return nullptr;
    }
    if (!WebPDecodeBGRAInto((const u8*)d.data(), d.size(), (u8*)bmpData.Scan0, bmpData.Stride * h, bmpData.Stride)) {
        return nullptr;
    }
    bmp.UnlockBits(&bmpData);

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
}

} // namespace webp

#else
namespace webp {
bool HasSignature(std::span<u8>) {
    return false;
}
Size SizeFromData(std::span<u8>) {
    return Size();
}
Gdiplus::Bitmap* ImageFromData(std::span<u8>) {
    return nullptr;
}
} // namespace webp

#endif
