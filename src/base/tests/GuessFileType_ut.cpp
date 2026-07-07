/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GuessFileType.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

static FileTypeInfo infoFromBytes(const u8* d, int n) {
    return GuessFileInfoFromContent(Str((char*)d, n));
}

static void pngTest() {
    // 1x2 PNG: signature + IHDR chunk
    static const u8 png[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, // signature
        0,    0,   0,   13,  'I',  'H',  'D',  'R',  // IHDR chunk header
        0,    0,   0,   1,                           // width 1
        0,    0,   0,   2,                           // height 2
        8,    6,   0,   0,   0,                      // bit depth etc.
        0,    0,   0,   0,                           // crc (not validated)
    };
    FileTypeInfo fti = infoFromBytes(png, dimofi(png));
    utassert(fti.ft == FileType::Png);
    utassert(fti.imageDx == 1);
    utassert(fti.imageDy == 2);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);
    utassert(GuessFileTypeFromContent(Str((char*)png, dimofi(png))) == FileType::Png);

    // APNG: same but with an acTL chunk (3 frames) after IHDR
    static const u8 apng[] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, // signature
        0,    0,   0,   13,  'I',  'H',  'D',  'R',  // IHDR chunk header
        0,    0,   0,   1,                           // width 1
        0,    0,   0,   2,                           // height 2
        8,    6,   0,   0,   0,                      // bit depth etc.
        0,    0,   0,   0,                           // crc
        0,    0,   0,   8,   'a',  'c',  'T',  'L',  // acTL chunk header
        0,    0,   0,   3,                           // num_frames 3
        0,    0,   0,   0,                           // num_plays
        0,    0,   0,   0,                           // crc
    };
    fti = infoFromBytes(apng, dimofi(apng));
    utassert(fti.ft == FileType::Png);
    utassert(fti.nImages == 3);
    utassert(fti.imageDx == 0 && fti.imageDy == 0);
    utassert(!fti.hasImageSize);
}

static void gifTest() {
    // 3x2 GIF with a single frame
    static const u8 gif1[] = {
        'G',  'I', 'F',  '8', '9', 'a',             // signature
        3,    0,   2,    0,   0,   0,   0,          // logical screen descriptor
        0x2C, 0,   0,    0,   0,   3,   0, 2, 0, 0, // image descriptor 3x2
        0x02, 1,   0xAA, 0,                         // LZW min code size, 1 data sub-block, terminator
        0x3B,                                       // trailer
    };
    FileTypeInfo fti = infoFromBytes(gif1, dimofi(gif1));
    utassert(fti.ft == FileType::Gif);
    utassert(fti.imageDx == 3);
    utassert(fti.imageDy == 2);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);

    // same with two frames
    static const u8 gif2[] = {
        'G',  'I', 'F',  '8', '9', 'a',             // signature
        3,    0,   2,    0,   0,   0,   0,          // logical screen descriptor
        0x2C, 0,   0,    0,   0,   3,   0, 2, 0, 0, // image descriptor 3x2
        0x02, 1,   0xAA, 0,                         // frame data
        0x2C, 0,   0,    0,   0,   3,   0, 2, 0, 0, // second image descriptor
        0x02, 1,   0xBB, 0,                         // frame data
        0x3B,                                       // trailer
    };
    fti = infoFromBytes(gif2, dimofi(gif2));
    utassert(fti.ft == FileType::Gif);
    utassert(fti.nImages == 2);
    utassert(fti.imageDx == 0 && fti.imageDy == 0);
    utassert(!fti.hasImageSize);
}

static void bmpTest() {
    // 7x5 top-down BMP (negative height)
    u8 bmp[54] = {'B', 'M'};
    bmp[14] = 40; // BITMAPINFOHEADER size
    bmp[18] = 7;  // width
    // height -5, little-endian
    bmp[22] = 0xFB;
    bmp[23] = 0xFF;
    bmp[24] = 0xFF;
    bmp[25] = 0xFF;
    FileTypeInfo fti = infoFromBytes(bmp, dimofi(bmp));
    utassert(fti.ft == FileType::Bmp);
    utassert(fti.imageDx == 7);
    utassert(fti.imageDy == 5);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);
}

static void jpegTest() {
    // SOF0 with height 2, width 3
    static const u8 jpg[] = {
        0xFF, 0xD8,             // SOI
        0xFF, 0xC0, 0x00, 0x11, // SOF0, segment length
        0x08,                   // precision
        0x00, 0x02,             // height 2
        0x00, 0x03,             // width 3
        0x00, 0x00, 0x00, 0x00, // padding so the parser can read past the marker
    };
    FileTypeInfo fti = infoFromBytes(jpg, dimofi(jpg));
    utassert(fti.ft == FileType::Jpeg);
    utassert(fti.imageDx == 3);
    utassert(fti.imageDy == 2);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);
}

static void webpTest() {
    // lossless 4x3 WebP: VP8L stores width-1/height-1 as 14-bit values
    static const u8 webp[] = {
        'R',  'I',  'F',  'F',  18, 0, 0, 0, // RIFF header
        'W',  'E',  'B',  'P',               //
        'V',  'P',  '8',  'L',  5,  0, 0, 0, // VP8L chunk, size 5
        0x2F,                                // VP8L signature
        0x03, 0x80, 0x00, 0x00,              // (4-1) | (3-1) << 14, little-endian
        0x00,                                // padding to even chunk size
    };
    FileTypeInfo fti = infoFromBytes(webp, dimofi(webp));
    utassert(fti.ft == FileType::Webp);
    utassert(fti.imageDx == 4);
    utassert(fti.imageDy == 3);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);
}

static void tiffTest() {
    // little-endian TIFF, single 5x6 page
    static const u8 tif1[] = {
        'I',  'I',  0x2A, 0,                         // header
        8,    0,    0,    0,                         // first IFD offset
        2,    0,                                     // 2 entries
        0x00, 0x01, 3,    0, 1, 0, 0, 0, 5, 0, 0, 0, // ImageWidth (short) 5
        0x01, 0x01, 3,    0, 1, 0, 0, 0, 6, 0, 0, 0, // ImageLength (short) 6
        0,    0,    0,    0,                         // no next IFD
    };
    FileTypeInfo fti = infoFromBytes(tif1, dimofi(tif1));
    utassert(fti.ft == FileType::Tiff);
    utassert(fti.imageDx == 5);
    utassert(fti.imageDy == 6);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);

    // same with a second (empty) IFD chained at offset 38
    static const u8 tif2[] = {
        'I',  'I',  0x2A, 0,                         // header
        8,    0,    0,    0,                         // first IFD offset
        2,    0,                                     // 2 entries
        0x00, 0x01, 3,    0, 1, 0, 0, 0, 5, 0, 0, 0, // ImageWidth (short) 5
        0x01, 0x01, 3,    0, 1, 0, 0, 0, 6, 0, 0, 0, // ImageLength (short) 6
        38,   0,    0,    0,                         // next IFD at offset 38
        0,    0,                                     // second IFD: 0 entries
        0,    0,    0,    0,                         // no next IFD
    };
    fti = infoFromBytes(tif2, dimofi(tif2));
    utassert(fti.ft == FileType::Tiff);
    utassert(fti.nImages == 2);
    utassert(fti.imageDx == 0 && fti.imageDy == 0);
    utassert(!fti.hasImageSize);
}

static void nonImageTest() {
    static const char pdf[] = "%PDF-1.4\nhello";
    FileTypeInfo fti = GuessFileInfoFromContent(StrL(pdf));
    utassert(fti.ft == FileType::PDF);
    utassert(fti.imageDx == 0 && fti.imageDy == 0);
    utassert(!fti.hasImageSize);
    utassert(fti.nImages == 0);

    fti = GuessFileInfoFromContent(StrL("no signature here at all"));
    utassert(fti.ft == FileType::Unknown);
    utassert(fti.nImages == 0);
}

void GuessFileTypeTest() {
    pngTest();
    gifTest();
    bmpTest();
    jpegTest();
    webpTest();
    tiffTest();
    nonImageTest();
}
