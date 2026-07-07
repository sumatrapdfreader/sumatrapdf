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

// Read EXIF orientation from IFD0 (tag 0x0112). Returns 1-8 or 0 if not found.
int JpegExifOrientationFromTiff(ByteReader r, int tiffBase) {
    int n = r.len;
    if (tiffBase + 8 > n) {
        return 0;
    }
    bool isBE = r.Byte(tiffBase) == 'M';
    int ifdOff = (int)r.DWord(tiffBase + 4, isBE);
    int ifdAbs = tiffBase + ifdOff;
    if (ifdAbs + 2 > n) {
        return 0;
    }
    WORD count = r.Word(ifdAbs, isBE);
    for (WORD i = 0; i < count; i++) {
        int entryOff = ifdAbs + 2 + i * 12;
        if (entryOff + 12 > n) {
            break;
        }
        WORD tag = r.Word(entryOff, isBE);
        if (tag == 0x0112) { // Orientation tag
            return r.Word(entryOff + 8, isBE);
        }
    }
    return 0;
}

Pixmap* PixmapFromExtFormatsData(Str bmpData, Kind kind) {
    if (kindFileWebp == kind) {
        Pixmap* px = webp::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
    }
    if (kindFileJxl == kind) {
        Pixmap* px = jxl::PixmapFromData(bmpData);
        if (px) {
            return px;
        }
    }
    if (kindFileHeic == kind || kindFileAvif == kind) {
        Pixmap* px = PixmapFromAvifData(bmpData);
        if (px) {
            return px;
        }
    }
    return nullptr;
}

int WebpExifOrientation(Str d) {
    if (!webp::HasSignature(d)) {
        return 0;
    }
    ByteReader r(d);
    int idx = 12;
    while (idx + 8 <= r.len) {
        if (r.Byte(idx) == 'E' && r.Byte(idx + 1) == 'X' && r.Byte(idx + 2) == 'I' && r.Byte(idx + 3) == 'F') {
            int size = (int)r.DWordLE(idx + 4);
            int payload = idx + 8;
            if (payload + size <= r.len && size >= 8) {
                int orient = JpegExifOrientationFromTiff(r, payload);
                if (orient != 0) {
                    return orient;
                }
            }
        }
        int size = (int)r.DWordLE(idx + 4);
        int chunkSize = size + (size & 1);
        if (chunkSize < size) {
            return 0;
        }
        idx += 8 + chunkSize;
        if (idx < 8) {
            return 0;
        }
    }
    return 0;
}

bool WebpImageSizeFromData(ByteReader r, Size& result) {
    if (r.len >= 30 && str::StartsWith(Str((char*)(r.d + 12), r.len - 12), "VP8 ")) {
        result.dx = r.WordLE(26) & 0x3fff;
        result.dy = r.WordLE(28) & 0x3fff;
        return true;
    }
    Str bs((char*)(r.d), r.len);
    result = webp::SizeFromData(bs);
    return !result.IsEmpty();
}

bool AvifImageSizeFromData(ByteReader r, Size& result) {
    Str bs((char*)(r.d), r.len);
    result = AvifSizeFromData(bs);
    return !result.IsEmpty();
}