/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ByteReaderWriter.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"

#include "ImageReader.h"

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

#if !OS_WIN
static void WriteTiffIfd(ByteWriterLE& w, u32 nextIfd, u32 pixelOffset) {
    constexpr u16 kLong = 4;
    constexpr u16 kShort = 3;
    auto entry = [&w](u16 tag, u16 type, u32 value) {
        w.Write16(tag);
        w.Write16(type);
        w.Write32(1);
        w.Write32(value);
    };
    w.Write16(9);
    entry(256, kLong, 1);           // ImageWidth
    entry(257, kLong, 1);           // ImageLength
    entry(258, kShort, 8);          // BitsPerSample
    entry(259, kShort, 1);          // Compression = none
    entry(262, kShort, 1);          // PhotometricInterpretation = black is zero
    entry(273, kLong, pixelOffset); // StripOffsets
    entry(277, kShort, 1);          // SamplesPerPixel
    entry(278, kLong, 1);           // RowsPerStrip
    entry(279, kLong, 1);           // StripByteCounts
    w.Write32(nextIfd);
}
#endif

void ImageReader_UnitTests() {
    Pixmap* gifSrc = AllocPixmap(64, 64, PixmapFormat::RGBA8);
    utassert(gifSrc);
    for (int y = 0; y < gifSrc->height; y++) {
        u8* p = gifSrc->data + (size_t)y * gifSrc->stride;
        for (int x = 0; x < gifSrc->width; x++, p += 4) {
            p[0] = (u8)(x * 4);
            p[1] = (u8)(y * 4);
            p[2] = (u8)((x + y) * 2);
            p[3] = (x == 63 && y == 63) ? 0 : 255;
        }
    }
    gifSrc->hasAlpha = true;
    Str encodedGif = EncodeGifFromPixmap(gifSrc);
    utassert(len(encodedGif) > 0);
    utassert(GuessFileTypeFromData(encodedGif) == FileType::Gif);
    Pixmap* gifDecoded = PixmapFromData(encodedGif);
    utassert(gifDecoded && gifDecoded->width == 64 && gifDecoded->height == 64);
    utassert(gifDecoded->hasAlpha && gifDecoded->premultiplied);
    FreePixmap(gifDecoded);
    str::Free(encodedGif);
    FreePixmap(gifSrc);

    Pixmap* tiffSrc = AllocPixmap(2, 2, PixmapFormat::RGBA8);
    utassert(tiffSrc);
    const u8 pixels[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128};
    memcpy(tiffSrc->data, pixels, sizeof(pixels));
    tiffSrc->hasAlpha = true;
    Str tiff = EncodeTiffFromPixmap(tiffSrc);
    utassert(len(tiff) > 0);
    utassert(GuessFileTypeFromData(tiff) == FileType::Tiff);
    FileTypeInfo tiffInfo = GuessFileInfoFromData(tiff);
    utassert(tiffInfo.imageDx == 2 && tiffInfo.imageDy == 2);
    FreeFileTypeInfo(&tiffInfo);
    Pixmap* tiffDecoded = PixmapFromData(tiff);
    utassert(tiffDecoded && tiffDecoded->width == 2 && tiffDecoded->height == 2);
    FreePixmap(tiffDecoded);
    str::Free(tiff);
    FreePixmap(tiffSrc);

#if !OS_WIN
    Str pngData = file::ReadFile(StrL("docs/test/test.png"));
    utassert(len(pngData) > 0);
    Pixmap* png = PixmapFromData(pngData);
    utassert(png);
    FreePixmap(png);
    str::Free(pngData);

    constexpr u32 kFirstIfd = 8;
    constexpr u32 kIfdSize = 2 + 9 * 12 + 4;
    constexpr u32 kSecondIfd = kFirstIfd + kIfdSize;
    constexpr u32 kFirstPixel = kSecondIfd + kIfdSize;
    ByteWriterLE w(kFirstPixel + 2);
    w.Write8x2('I', 'I');
    w.Write16(42);
    w.Write32(kFirstIfd);
    WriteTiffIfd(w, kSecondIfd, kFirstPixel);
    WriteTiffIfd(w, 0, kFirstPixel + 1);
    w.Write8(0);
    w.Write8(255);

    Vec<Pixmap*> frames = PixmapsFromData(w.AsByteSlice());
    utassert(len(frames) == 2);
    for (Pixmap* frame : frames) {
        utassert(frame->width == 1 && frame->height == 1);
        FreePixmap(frame);
    }

#if !OS_WIN
    // Pillow's duplicate_frame.gif: three visually identical animation frames.
    const u8 gif[] = {
        0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x0a, 0x00, 0x0a, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x21, 0xf9, 0x04, 0x00, 0x64, 0x00, 0x00, 0x00, 0x21, 0xff, 0x0b, 0x4e, 0x45, 0x54, 0x53, 0x43, 0x41,
        0x50, 0x45, 0x32, 0x2e, 0x30, 0x03, 0x01, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a,
        0x00, 0x00, 0x02, 0x08, 0x84, 0x8f, 0xa9, 0xcb, 0xed, 0x0f, 0x63, 0x2b, 0x00, 0x21, 0xf9, 0x04, 0x00, 0x64,
        0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x80, 0x55, 0x55, 0x55, 0x00, 0x00,
        0x00, 0x02, 0x08, 0x84, 0x8f, 0xa9, 0xcb, 0xed, 0x0f, 0x63, 0x2b, 0x00, 0x21, 0xf9, 0x04, 0x00, 0x64, 0x00,
        0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x0a, 0x00, 0x80, 0x55, 0x55, 0x55, 0x00, 0x00, 0x00,
        0x02, 0x08, 0x84, 0x8f, 0xa9, 0xcb, 0xed, 0x0f, 0x63, 0x2b, 0x00, 0x3b,
    };
    frames = PixmapsFromData(Str((char*)gif, dimofi(gif)));
    utassert(len(frames) == 3);
    for (Pixmap* frame : frames) {
        utassert(frame->width == 10 && frame->height == 10);
        FreePixmap(frame);
    }

    // 2x1 frames: red/red, red/green, then blue/background after frame 2's
    // restore-background disposal.
    const u8 disposalGif[] = {
        'G', 'I', 'F', '8',  '9', 'a', 2,    0,    1,    0, 0x81, 0,    0, 0, 0,    0,    255, 0, 0,
        0,   255, 0,   0,    0,   255, 0x21, 0xf9, 4,    0, 0,    0,    0, 0, 0x2c, 0,    0,   0, 0,
        2,   0,   1,   0,    0,   2,   2,    0x0c, 0x53, 0, 0x21, 0xf9, 4, 8, 0,    0,    0,   0, 0x2c,
        1,   0,   0,   0,    1,   0,   1,    0,    0,    2, 2,    0x54, 1, 0, 0x21, 0xf9, 4,   0, 0,
        0,   0,   0,   0x2c, 0,   0,   0,    0,    1,    0, 1,    0,    0, 2, 2,    0x5c, 1,   0, 0x3b,
    };
    frames = PixmapsFromData(Str((char*)disposalGif, dimofi(disposalGif)));
    utassert(len(frames) == 3);
    if (len(frames) == 3) {
        utassert(frames[0]->data[2] == 255 && frames[0]->data[6] == 255);
        utassert(frames[1]->data[2] == 255 && frames[1]->data[5] == 255);
        utassert(frames[2]->data[0] == 255 && frames[2]->data[4] == 0 && frames[2]->data[5] == 0 &&
                 frames[2]->data[6] == 0);
    }
    for (Pixmap* frame : frames) {
        FreePixmap(frame);
    }
#endif

    Str icoData = file::ReadFile(StrL("src/gfx/SumatraPDF-smaller.ico"));
    FileTypeInfo icoInfo = GuessFileInfoFromData(icoData);
    utassert(icoInfo.nImages > 1);
    frames = PixmapsFromData(icoData);
    utassert(len(frames) == icoInfo.nImages);
    for (Pixmap* frame : frames) {
        utassert(frame->width > 0 && frame->height > 0);
        bool hasTransparentPixel = false;
        for (int y = 0; y < frame->height && !hasTransparentPixel; y++) {
            const u8* p = frame->data + y * frame->stride;
            for (int x = 0; x < frame->width; x++, p += 4) {
                if (p[3] < 255) {
                    hasTransparentPixel = true;
                    break;
                }
            }
        }
        utassert(frame->hasAlpha && frame->premultiplied && hasTransparentPixel);
        FreePixmap(frame);
    }
    FreeFileTypeInfo(&icoInfo);
    str::Free(icoData);
#endif
}
