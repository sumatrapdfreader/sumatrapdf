/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/GuessFileType.h"

// must be last due to assert() over-write
#include "base/UtAssert.h"

static FileTypeInfo infoFromBytes(const u8* d, int n) {
    return GuessFileInfoFromData(Str((char*)d, n));
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
    utassert(GuessFileTypeFromData(Str((char*)png, dimofi(png))) == FileType::Png);

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
    utassert(fti.orientation == 0);

    // same but with an EXIF APP1 segment carrying orientation 6,
    // which swaps the reported width/height
    static const u8 jpgExif[] = {
        0xFF, 0xD8,                                     // SOI
        0xFF, 0xE1, 0x00, 0x22,                         // APP1, segment length 34
        'E',  'x',  'i',  'f',  0, 0,                   // Exif header
        'I',  'I',  0x2A, 0,                            // TIFF header, little-endian
        8,    0,    0,    0,                            // IFD0 offset
        1,    0,                                        // 1 entry
        0x12, 0x01, 3,    0,    1, 0, 0, 0, 6, 0, 0, 0, // Orientation (short) 6
        0,    0,    0,    0,                            // no next IFD
        0xFF, 0xC0, 0x00, 0x11,                         // SOF0, segment length
        0x08,                                           // precision
        0x00, 0x02,                                     // height 2
        0x00, 0x03,                                     // width 3
        0x00, 0x00, 0x00, 0x00,                         // padding
    };
    fti = infoFromBytes(jpgExif, dimofi(jpgExif));
    utassert(fti.ft == FileType::Jpeg);
    utassert(fti.orientation == 6);
    utassert(fti.imageDx == 2);
    utassert(fti.imageDy == 3);
    utassert(fti.hasImageSize);
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

static void heifTest() {
    // minimal 5x7 AVIF: ftyp, then meta > iprp > ipco > ispe
    static const u8 avif[] = {
        0, 0, 0, 16, 'f', 't', 'y', 'p', 'a', 'v', 'i', 'f', 0, 0, 0, 0, // ftyp
        0, 0, 0, 48, 'm', 'e', 't', 'a', 0,   0,   0,   0,               // meta (FullBox)
        0, 0, 0, 36, 'i', 'p', 'r', 'p',                                 // iprp
        0, 0, 0, 28, 'i', 'p', 'c', 'o',                                 // ipco
        0, 0, 0, 20, 'i', 's', 'p', 'e', 0,   0,   0,   0,               // ispe (FullBox)
        0, 0, 0, 5,                                                      // width 5
        0, 0, 0, 7,                                                      // height 7
    };
    FileTypeInfo fti = infoFromBytes(avif, dimofi(avif));
    utassert(fti.ft == FileType::Avif);
    utassert(fti.imageDx == 5);
    utassert(fti.imageDy == 7);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);

    // HEIC with a thumbnail ispe (2x3), the full-image ispe (5x7) and a
    // 90-degree irot, which swaps the reported width/height
    static const u8 heic[] = {
        0, 0, 0, 16, 'f', 't', 'y', 'p', 'h', 'e', 'i', 'c', 0, 0, 0, 0, // ftyp
        0, 0, 0, 77, 'm', 'e', 't', 'a', 0,   0,   0,   0,               // meta (FullBox)
        0, 0, 0, 65, 'i', 'p', 'r', 'p',                                 // iprp
        0, 0, 0, 57, 'i', 'p', 'c', 'o',                                 // ipco
        0, 0, 0, 20, 'i', 's', 'p', 'e', 0,   0,   0,   0,               // thumbnail ispe
        0, 0, 0, 2,                                                      // width 2
        0, 0, 0, 3,                                                      // height 3
        0, 0, 0, 20, 'i', 's', 'p', 'e', 0,   0,   0,   0,               // full-image ispe
        0, 0, 0, 5,                                                      // width 5
        0, 0, 0, 7,                                                      // height 7
        0, 0, 0, 9,  'i', 'r', 'o', 't', 1,                              // rotation: 90 degrees ccw
    };
    fti = infoFromBytes(heic, dimofi(heic));
    utassert(fti.ft == FileType::Heic);
    utassert(fti.imageDx == 7);
    utassert(fti.imageDy == 5);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);
}

static void jxlTest() {
    // raw codestream, small size header: ysize = (7+1)*8 = 64, ratio 1:1
    static const u8 jxl1[] = {0xFF, 0x0A, 0x4F, 0x02};
    FileTypeInfo fti = infoFromBytes(jxl1, dimofi(jxl1));
    utassert(fti.ft == FileType::Jxl);
    utassert(fti.imageDx == 64);
    utassert(fti.imageDy == 64);
    utassert(fti.hasImageSize);
    utassert(fti.nImages == 1);
    utassert(fti.orientation == 1);

    // ratio 2:1 (xsize 128, ysize 64) with orientation 6, which swaps
    // the reported width/height
    static const u8 jxl2[] = {0xFF, 0x0A, 0xCF, 0x2D};
    fti = infoFromBytes(jxl2, dimofi(jxl2));
    utassert(fti.ft == FileType::Jxl);
    utassert(fti.orientation == 6);
    utassert(fti.imageDx == 64);
    utassert(fti.imageDy == 128);
    utassert(fti.hasImageSize);

    // same 64x64 codestream in the ISO BMFF container (jxlc box)
    static const u8 jxlBmff[] = {
        0,   0,   0,   12,  'J', 'X', 'L', ' ', 0x0D, 0x0A, 0x87, 0x0A,             // signature box
        0,   0,   0,   20,  'f', 't', 'y', 'p', 'j',  'x',  'l',  ' ',  0, 0, 0, 0, // ftyp
        'j', 'x', 'l', ' ',                                                         //
        0,   0,   0,   12,  'j', 'x', 'l', 'c', 0xFF, 0x0A, 0x4F, 0x02,             // jxlc + codestream
    };
    fti = infoFromBytes(jxlBmff, dimofi(jxlBmff));
    utassert(fti.ft == FileType::Jxl);
    utassert(fti.imageDx == 64);
    utassert(fti.imageDy == 64);
    utassert(fti.hasImageSize);
}

static void nonImageTest() {
    static const char pdf[] = "%PDF-1.4\nhello";
    FileTypeInfo fti = GuessFileInfoFromData(StrL(pdf));
    utassert(fti.ft == FileType::PDF);
    utassert(fti.imageDx == 0 && fti.imageDy == 0);
    utassert(!fti.hasImageSize);
    utassert(fti.nImages == 0);

    fti = GuessFileInfoFromData(StrL("no signature here at all"));
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
    heifTest();
    jxlTest();
    nonImageTest();
}
