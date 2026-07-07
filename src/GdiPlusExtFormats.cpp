/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/ByteReader.h"
#include "base/GuessFileType.h"
#include "AvifReader.h"
#include "JxlReader.h"
#include "WebpReader.h"
#include "base/GdiPlus.h"
#include "GdiPlusExtFormats.h"

Pixmap* PixmapFromExtFormatsData(Str bmpData, FileType kind) {
    if (FileType::Webp == kind) {
        Pixmap* px = webp::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
    }
    if (FileType::Jxl == kind) {
        Pixmap* px = jxl::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
    }
    if (FileType::Heic == kind || FileType::Avif == kind) {
        Pixmap* px = PixmapFromAvifData(bmpData);
        if (px) {
            return px;
        }
    }
    return nullptr;
}

// only called when GuessFileInfoFromData() couldn't parse the size from
// the VP8X/VP8/VP8L headers; asks libwebp instead
bool WebpImageSizeFromData(ByteReader r, Size& result) {
    Str bs((char*)(r.d), r.len);
    result = webp::SizeFromData(bs);
    return !result.IsEmpty();
}

bool AvifImageSizeFromData(ByteReader r, Size& result) {
    Str bs((char*)(r.d), r.len);
    result = AvifSizeFromData(bs);
    return !result.IsEmpty();
}