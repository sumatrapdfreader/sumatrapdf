/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "Pixmap.h"
#include "TgaReader.h"

namespace tga {

Str SerializeBitmap(HBITMAP hbmp) {
    BITMAP bmpInfo;
    GetObject(hbmp, sizeof(BITMAP), &bmpInfo);
    if ((u32)bmpInfo.bmWidth > USHRT_MAX || (u32)bmpInfo.bmHeight > USHRT_MAX) {
        return {};
    }

    Pixmap* pixmap = AllocPixmap(bmpInfo.bmWidth, bmpInfo.bmHeight, PixmapFormat::BGR8);
    if (!pixmap) {
        return {};
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = pixmap->width;
    bmi.bmiHeader.biHeight = -pixmap->height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hDC = GetDC(nullptr);
    if (!GetDIBits(hDC, hbmp, 0, pixmap->height, pixmap->data, &bmi, DIB_RGB_COLORS)) {
        ReleaseDC(nullptr, hDC);
        FreePixmap(pixmap);
        return {};
    }
    ReleaseDC(nullptr, hDC);

    Str res = PixmapToTgaFormat(pixmap);
    FreePixmap(pixmap);
    return res;
}

} // namespace tga
