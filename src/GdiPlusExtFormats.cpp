/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Pixmap.h"
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
