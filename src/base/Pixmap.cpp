/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "Pixmap.h"

static void AppendLE16(str::Builder& data, u16 v) {
    data.AppendChar((char)(v & 0xff));
    data.AppendChar((char)((v >> 8) & 0xff));
}

static void AppendLE32(str::Builder& data, u32 v) {
    data.AppendChar((char)(v & 0xff));
    data.AppendChar((char)((v >> 8) & 0xff));
    data.AppendChar((char)((v >> 16) & 0xff));
    data.AppendChar((char)((v >> 24) & 0xff));
}

static void AppendPixmapPixelBGR(str::Builder& data, const Pixmap* pixmap, int x, int y) {
    const u8* src = pixmap->data + (size_t)y * pixmap->stride + (size_t)x * PixmapBytesPerPixel(pixmap->format);
    if (pixmap->format == PixmapFormat::RGBA8) {
        data.AppendChar((char)src[2]);
        data.AppendChar((char)src[1]);
        data.AppendChar((char)src[0]);
        return;
    }
    data.AppendChar((char)src[0]);
    data.AppendChar((char)src[1]);
    data.AppendChar((char)src[2]);
}

// create data for a .bmp file from this pixmap. The output matches the classic BMP
// format accepted by Windows LoadImage(..., LD_LOADFROMFILE): BITMAPFILEHEADER +
// BITMAPINFOHEADER + bottom-up 24bpp BGR rows.
Str PixmapToBmpFormat(const Pixmap* pixmap) {
    if (!pixmap || !pixmap->data || pixmap->width <= 0 || pixmap->height <= 0) {
        return {};
    }
    if (pixmap->format != PixmapFormat::BGRA8 && pixmap->format != PixmapFormat::BGR8 &&
        pixmap->format != PixmapFormat::RGBA8) {
        return {};
    }

    size_t w = (size_t)pixmap->width;
    size_t h = (size_t)pixmap->height;
    size_t rowStride = (w * 3 + 3) & ~(size_t)3;
    size_t headerLen = 14 + 40;
    size_t bmpBytes = rowStride * h + headerLen;
    if (rowStride > UINT32_MAX || bmpBytes > INT_MAX || bmpBytes > UINT32_MAX) {
        return {};
    }

    str::Builder bmpData((int)bmpBytes);
    AppendLE16(bmpData, 0x4d42); // "BM"
    AppendLE32(bmpData, (u32)bmpBytes);
    AppendLE16(bmpData, 0);
    AppendLE16(bmpData, 0);
    AppendLE32(bmpData, (u32)headerLen);

    AppendLE32(bmpData, 40); // BITMAPINFOHEADER size
    AppendLE32(bmpData, (u32)w);
    AppendLE32(bmpData, (u32)h);
    AppendLE16(bmpData, 1);  // planes
    AppendLE16(bmpData, 24); // bit count
    AppendLE32(bmpData, 0);  // BI_RGB
    AppendLE32(bmpData, (u32)(rowStride * h));
    AppendLE32(bmpData, 0);
    AppendLE32(bmpData, 0);
    AppendLE32(bmpData, 0);
    AppendLE32(bmpData, 0);

    int padding = (int)rowStride - pixmap->width * 3;
    for (int y = pixmap->height - 1; y >= 0; y--) {
        for (int x = 0; x < pixmap->width; x++) {
            AppendPixmapPixelBGR(bmpData, pixmap, x, y);
        }
        for (int i = 0; i < padding; i++) {
            bmpData.AppendChar(0);
        }
    }

    return bmpData.TakeStr();
}
