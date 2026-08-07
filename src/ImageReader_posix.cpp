/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"

#include "ImageReader.h"

// Decode image bytes to a single (first-frame) Pixmap. Caller owns it (FreePixmap).
// Windows: JPEG→turbo, WebP→libwebp, JXL→jxldec; HEIC/AVIF→heicdec then WIC in
// Debug, WIC then heicdec in Release; else TGA/GDI+/WIC. POSIX: MuPDF for now.
Pixmap* PixmapFromData(Str bmpData) {
    return PixmapFromDataFz(bmpData);
}

// One Pixmap per frame (multi-page TIFF / animated GIF yield >1); caller owns each.
Vec<Pixmap*> PixmapsFromData(Str bmpData) {
    Vec<Pixmap*> res;
    Pixmap* px = PixmapFromDataFz(bmpData);
    if (px) {
        res.Append(px);
    }
    return res;
}

// Load path into a RenderedBitmap (Windows); nullptr on POSIX for now.
RenderedBitmap* LoadRenderedBitmap(Str /*path*/) {
    return nullptr;
}
