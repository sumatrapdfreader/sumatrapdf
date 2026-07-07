/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/File.h"
#include "base/GdiPlus.h"
#include "base/Win.h"

#include "FzImgReader.h"

Pixmap* PixmapFromData(Str bmpData) {
    Pixmap* px = PixmapFromDataWin(bmpData);
    if (px) {
        return px;
    }
    return PixmapFromDataFz(bmpData);
}

Vec<Pixmap*> PixmapsFromData(Str bmpData) {
    Vec<Pixmap*> res = PixmapsFromDataWin(bmpData);
    if (len(res) > 0) {
        return res;
    }

    Pixmap* px = PixmapFromDataFz(bmpData);
    if (px) {
        res.Append(px);
    }
    return res;
}

RenderedBitmap* LoadRenderedBitmap(Str path) {
    if (!path) {
        return nullptr;
    }
    Str data = file::ReadFile(path);
    if (!data) {
        return nullptr;
    }

    Gdiplus::Bitmap* bmp = NewGdiplusBitmapFromPixmap(PixmapFromData(data));
    str::Free(data);
    if (!bmp) {
        return nullptr;
    }

    HBITMAP hbmp = nullptr;
    RenderedBitmap* rendered = nullptr;
    if (bmp->GetHBITMAP((Gdiplus::ARGB)Gdiplus::Color::White, &hbmp) == Gdiplus::Ok) {
        rendered = new RenderedBitmap(hbmp, Size(bmp->GetWidth(), bmp->GetHeight()));
    }
    delete bmp;

    return rendered;
}
