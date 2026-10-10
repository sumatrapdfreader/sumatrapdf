/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "gui/Dpi.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "base/ByteReaderWriter.h"
#include "ImageReader.h"
#include "ImageSaveCropResize.h"
#include "ImageSaveCropResizeCommon.h"

Str ImageSaveExtFromData(Str data) {
    if (len(data) == 0) {
        return {};
    }
    switch (GuessFileTypeFromData(data)) {
        case FileType::Png:
            return StrL(".png");
        case FileType::Jpeg:
            return StrL(".jpg");
        case FileType::Gif:
            return StrL(".gif");
        case FileType::Tiff:
            return StrL(".tif");
        case FileType::Bmp:
            return StrL(".bmp");
        case FileType::Ico:
            return StrL(".ico");
        case FileType::Webp:
            return StrL(".webp");
        case FileType::Jxl:
            return StrL(".jxl");
        case FileType::Jp2:
            return StrL(".jp2");
        default:
            return {};
    }
}

bool ExtMatchesOriginal(Str ext, Str originalExt) {
    if (len(ext) == 0 || len(originalExt) == 0) {
        return false;
    }
    if (str::EqI(ext, originalExt)) {
        return true;
    }
    if (str::EqI(originalExt, StrL(".jpg")) && str::EqI(ext, StrL(".jpeg"))) {
        return true;
    }
    if (str::EqI(originalExt, StrL(".tif")) && str::EqI(ext, StrL(".tiff"))) {
        return true;
    }
    return false;
}

TempStr PathWithExtTemp(Str path, Str ext) {
    TempStr noExt = path::GetPathNoExtTemp(path);
    return fmt("%s%s", noExt, ext);
}

// Uncompressed chunky CMYK TIFF (PhotometricInterpretation = Separated).
// samples are packed C,M,Y,K, stride = w*4, PDF polarity (0 = no ink).
static bool WriteCmykTiff(Str destPath, int w, int h, int srcStride, const u8* samples) {
    if (len(destPath) == 0 || w <= 0 || h <= 0 || srcStride < w * 4 || !samples) {
        return false;
    }
    const int rowBytes = w * 4;
    const u32 dataLen = (u32)rowBytes * (u32)h;
    const int nTags = 11;
    const u32 ifdOff = 8;
    const u32 ifdSize = 2 + ((u32)nTags * 12) + 4;
    const u32 bitsOff = ifdOff + ifdSize;
    const u32 dataOff = bitsOff + 8;

    ByteWriterLE wr(8 + (int)ifdSize + 8 + (int)dataLen);
    wr.Write8x2('I', 'I');
    wr.Write16(42);
    wr.Write32(ifdOff);
    wr.Write16((u16)nTags);

    auto tagLong = [&](u16 tag, u32 val) {
        wr.Write16(tag);
        wr.Write16(4); // LONG
        wr.Write32(1);
        wr.Write32(val);
    };
    auto tagShort = [&](u16 tag, u16 val) {
        wr.Write16(tag);
        wr.Write16(3); // SHORT
        wr.Write32(1);
        wr.Write16(val);
        wr.Write16(0);
    };
    auto tagShortArray = [&](u16 tag, u32 count, u32 offset) {
        wr.Write16(tag);
        wr.Write16(3);
        wr.Write32(count);
        wr.Write32(offset);
    };

    tagLong(256, (u32)w);           // ImageWidth
    tagLong(257, (u32)h);           // ImageLength
    tagShortArray(258, 4, bitsOff); // BitsPerSample
    tagShort(259, 1);               // Compression = none
    tagShort(262, 5);               // PhotometricInterpretation = Separated
    tagLong(273, dataOff);          // StripOffsets
    tagShort(277, 4);               // SamplesPerPixel
    tagLong(278, (u32)h);           // RowsPerStrip
    tagLong(279, dataLen);          // StripByteCounts
    tagShort(284, 1);               // PlanarConfiguration = chunky
    tagShort(332, 1);               // InkSet = CMYK
    wr.Write32(0);                  // next IFD

    wr.Write16(8);
    wr.Write16(8);
    wr.Write16(8);
    wr.Write16(8);

    for (int y = 0; y < h; y++) {
        const u8* row = samples + ((size_t)y * (size_t)srcStride);
        for (int x = 0; x < rowBytes; x++) {
            wr.Write8(row[x]);
        }
    }
    return file::WriteFile(destPath, wr.AsByteSlice());
}

// CMYK JPEG (Adobe polarity) → CMYK TIFF (PDF polarity). False if not CMYK.
bool TrySaveOriginalAsCmykTiff(Str originalData, Str destPath) {
    int w = 0, h = 0, stride = 0;
    Vec<u8> samples;
    if (!DecodeJpegToCmyk(originalData, w, h, stride, samples)) {
        return false;
    }
    return WriteCmykTiff(destPath, w, h, stride, samples.els);
}
