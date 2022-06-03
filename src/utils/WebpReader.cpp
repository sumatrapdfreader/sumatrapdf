/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
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

Gdiplus::Bitmap* ImageFromData(const ByteSlice& d) {
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
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
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
Gdiplus::Bitmap* ImageFromData(const ByteSlice&) {
    return nullptr;
}
} // namespace webp

#endif
