/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "ImageReader.h"

Pixmap* PixmapFromData(Str bmpData) {
    return PixmapFromDataFz(bmpData);
}

Vec<Pixmap*> PixmapsFromData(Str bmpData) {
    Vec<Pixmap*> res;
    Pixmap* px = PixmapFromDataFz(bmpData);
    if (px) {
        res.Append(px);
    }
    return res;
}

RenderedBitmap* LoadRenderedBitmap(Str /*path*/) {
    return nullptr;
}
