/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WebpReader.h"

#ifndef NO_LIBWEBP

#include <webp/decode.h>
using namespace Gdiplus;

namespace webp {

// checks whether this could be data for a WebP image
bool HasSignature(const char *data, size_t len)
{
    return len > 12 && str::StartsWith(data, "RIFF") && str::StartsWith(data + 8, "WEBP");
}

Size SizeFromData(const char *data, size_t len)
{
    Size size;
    WebPGetInfo((const uint8_t *)data, len, &size.Width, &size.Height);
    return size;
}

Bitmap *ImageFromData(const char *data, size_t len)
{
    int w, h;
    if (!WebPGetInfo((const uint8_t *)data, len, &w, &h))
        return NULL;

    Bitmap bmp(w, h, PixelFormat32bppARGB);
    Rect bmpRect(0, 0, w, h);
    BitmapData bmpData;
    Status ok = bmp.LockBits(&bmpRect, ImageLockModeWrite, PixelFormat32bppARGB, &bmpData);
    if (ok != Ok)
        return NULL;
    if (!WebPDecodeBGRAInto((const uint8_t *)data, len, (uint8_t *)bmpData.Scan0, bmpData.Stride * h, bmpData.Stride))
        return NULL;
    bmp.UnlockBits(&bmpData);

    // hack to avoid the use of ::new (because there won't be a corresponding ::delete)
    return bmp.Clone(0, 0, w, h, PixelFormat32bppARGB);
}

}

#else

namespace webp {
bool HasSignature(const char *data, size_t len) { return false; }
Gdiplus::Size SizeFromData(const char *data, size_t len) { return Gdiplus::Size(); }
Gdiplus::Bitmap *ImageFromData(const char *data, size_t len) { return NULL; }
}

#endif
