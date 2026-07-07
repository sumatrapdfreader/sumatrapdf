
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// This file must only contain code that doesn't depend on
// external libraries (mupdf/, ext/). GuessFileTypeFromFile.cpp has
// the parts that need base/Archive.h (and thus ext/libarchive).

#include "base/Base.h"
#include "base/File.h"
#include "base/ByteReader.h"
#include "base/GuessFileType.h"

// http://en.wikipedia.org/wiki/.nfo
// http://en.wikipedia.org/wiki/FILE_ID.DIZ
// http://en.wikipedia.org/wiki/Read.me
// http://www.cix.co.uk/~gidds/Software/TCR.html

// TODO: should .prc be FileType::PalmDoc instead of FileType::Mobi?
// .zip etc. are at the end so that .fb2.zip etc. is recognized at fb2
#define DEF_EXT_KIND(V)                \
    V(".txt", FileType::Txt)           \
    V(".js", FileType::Txt)            \
    V(".json", FileType::Txt)          \
    V(".xml", FileType::Txt)           \
    V(".log", FileType::Txt)           \
    V("file_id.diz", FileType::Txt)    \
    V("read.me", FileType::Txt)        \
    V(".nfo", FileType::Txt)           \
    V(".tcr", FileType::Txt)           \
    V(".ps", FileType::PS)             \
    V(".ps.gz", FileType::PS)          \
    V(".eps", FileType::PS)            \
    V(".fb2", FileType::Fb2)           \
    V(".fb2z", FileType::Fb2z)         \
    V(".fbz", FileType::Fb2z)          \
    V(".zfb2", FileType::Fb2z)         \
    V(".fb2.zip", FileType::Fb2z)      \
    V(".cbz", FileType::Cbz)           \
    V(".ora", FileType::Cbz)           \
    V(".cbr", FileType::Cbr)           \
    V(".cb7", FileType::Cb7)           \
    V(".cbt", FileType::Cbt)           \
    V(".pdf", FileType::PDF)           \
    V(".ai", FileType::PDF)            \
    V(".xps", FileType::Xps)           \
    V(".oxps", FileType::Xps)          \
    V(".xod", FileType::Xps)           \
    V(".dwfx", FileType::Xps)          \
    V(".chm", FileType::Chm)           \
    V(".png", FileType::Png)           \
    V(".jpg", FileType::Jpeg)          \
    V(".jpeg", FileType::Jpeg)         \
    V(".jfif", FileType::Jpeg)         \
    V(".gif", FileType::Gif)           \
    V(".tif", FileType::Tiff)          \
    V(".tiff", FileType::Tiff)         \
    V(".bmp", FileType::Bmp)           \
    V(".tga", FileType::Tga)           \
    V(".jxr", FileType::Jxr)           \
    V(".hdp", FileType::Hdp)           \
    V(".wdp", FileType::Wdp)           \
    V(".webp", FileType::Webp)         \
    V(".jxl", FileType::Jxl)           \
    V(".epub", FileType::Epub)         \
    V(".md", FileType::Markdown)       \
    V(".markdown", FileType::Markdown) \
    V(".mobi", FileType::Mobi)         \
    V(".prc", FileType::Mobi)          \
    V(".azw", FileType::Mobi)          \
    V(".azw1", FileType::Mobi)         \
    V(".azw3", FileType::Mobi)         \
    V(".azw4", FileType::Mobi)         \
    V(".pdb", FileType::PalmDoc)       \
    V(".html", FileType::HTML)         \
    V(".htm", FileType::HTML)          \
    V(".xhtml", FileType::HTML)        \
    V(".svg", FileType::Svg)           \
    V(".djvu", FileType::DjVu)         \
    V(".djv", FileType::DjVu)          \
    V(".jp2", FileType::Jp2)           \
    V(".j2k", FileType::Jp2)           \
    V(".jpx", FileType::Jp2)           \
    V(".jpf", FileType::Jp2)           \
    V(".jpm", FileType::Jp2)           \
    V(".j2c", FileType::Jp2)           \
    V(".zip", FileType::Zip)           \
    V(".rar", FileType::Rar)           \
    V(".7z", FileType::SevenZ)         \
    V(".heic", FileType::Heic)         \
    V(".heif", FileType::Heic)         \
    V(".avif", FileType::Avif)         \
    V(".tar", FileType::Tar)

#define EXT(ext, ft) ext "\0"

// .fb2.zip etc. must be first so that it isn't classified as .zip
static const char* gFileExts = DEF_EXT_KIND(EXT);
#undef EXT

#define KIND(ext, ft) ft,
static FileType gExtsType[] = {DEF_EXT_KIND(KIND)};
#undef KIND

static FileType GetTypeByFileExt(Str path) {
    TempStr ext = path::GetExtTemp(path);
    int idx = SeqStrIndexIS(gFileExts, ext);
    if (idx < 0) {
        return FileType::Unknown;
    }
    int n = (int)dimof(gExtsType);
    if (idx >= n) {
        return FileType::Unknown;
    }
    return gExtsType[idx];
}

TempStr GetExtForFileTypeTemp(FileType ft) {
    int idx = FileTypeIndexOf(gExtsType, dimofi(gExtsType), ft);
    if (idx >= 0) {
        return SeqStrByIndex(gFileExts, idx);
    }
    return {};
}

// ensure gFileExts and gExtsType match
static bool gDidVerifyExtsMatch = false;
static void VerifyExtsMatch() {
    if (gDidVerifyExtsMatch) {
        return;
    }
    ReportIf(FileType::Epub != GetTypeByFileExt("foo.epub"));
    ReportIf(FileType::Jp2 != GetTypeByFileExt("foo.JP2"));
    gDidVerifyExtsMatch = true;
}

int FileTypeIndexOf(const FileType* types, int nTypes, FileType ft) {
    for (int i = 0; i < nTypes; i++) {
        if (types[i] == ft) {
            return i;
        }
    }
    return -1;
}

#define FILE_SIGS(V)                                      \
    V(0, "Rar!\x1A\x07\x00", FileType::Rar)               \
    V(0, "Rar!\x1A\x07\x01\x00", FileType::Rar)           \
    V(0, "7z\xBC\xAF\x27\x1C", FileType::SevenZ)          \
    V(0, "PK\x03\x04", FileType::Zip)                     \
    V(0, "ITSF", FileType::Chm)                           \
    V(0x3c, "BOOKMOBI", FileType::Mobi)                   \
    V(0x3c, "TEXtREAd", FileType::PalmDoc)                \
    V(0x3c, "TEXtTlDc", FileType::PalmDoc)                \
    V(0x3c, "DataPlkr", FileType::PalmDoc)                \
    V(0, "\x89PNG\x0D\x0A\x1A\x0A", FileType::Png)        \
    V(0, "\xFF\xD8", FileType::Jpeg)                      \
    V(0, "GIF87a", FileType::Gif)                         \
    V(0, "GIF89a", FileType::Gif)                         \
    V(0, "BM", FileType::Bmp)                             \
    V(0, "MM\x00\x2A", FileType::Tiff)                    \
    V(0, "II\x2A\x00", FileType::Tiff)                    \
    V(0, "II\xBC\x01", FileType::Jxr)                     \
    V(0, "II\xBC\x00", FileType::Jxr)                     \
    V(0, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", FileType::Jp2) \
    V(0, "AT&T", FileType::DjVu)

// a file signaure is a sequence of bytes at a specific
// offset in the file
struct FileSig {
    int offset;
    Str sig;
    int sigLen;
    FileType ft;
};

#define MK_SIG(OFF, SIG, FT) {OFF, SIG, (int)(sizeof(SIG) - 1), FT},
static FileSig gFileSigs[] = {FILE_SIGS(MK_SIG)};
#undef MK_SIG

// PDF files have %PDF-${ver} somewhere in the beginning of the file
static bool IsPdfFileContent(Str d) {
    if (d.len < 8) {
        return false;
    }
    Str data = Str((char*)((u8*)d.s), d.len - 5);
    while (data.len >= 5) {
        int idx = str::IndexOfChar(data, '%');
        if (idx < 0) {
            return false;
        }
        if (str::EqN(Str(data.s + idx, data.len - idx), StrL("%PDF-"), 5)) {
            return true;
        }
        data = Str(data.s + idx + 1, data.len - idx - 1);
    }
    return false;
}

static bool IsPSFileContent(Str d) {
    Str header = d;
    int n = d.len;
    if (n < 64) {
        return false;
    }
    // Windows-format EPS file - cf. http://partners.adobe.com/public/developer/en/ps/5002.EPSF_Spec.pdf
    if (str::StartsWith(header, "\xC5\xD0\xD3\xC6")) {
        DWORD psStart = ByteReader(d).DWordLE(4);
        if ((int)psStart >= n - 12) {
            return true;
        }
        Str sub = Str(header.s + psStart, header.len - (int)psStart);
        return str::StartsWith(sub, "%!PS-Adobe-");
    }
    if (str::StartsWith(header, "%!PS-Adobe-")) {
        return true;
    }
    // PJL (Printer Job Language) files containing Postscript data
    // https://developers.hp.com/system/files/PJL_Technical_Reference_Manual.pdf
    bool isPJL = str::StartsWith(header, "\x1B%-12345X@PJL");
    if (isPJL && !str::Contains(header, StrL("%!PS-Adobe-"))) {
        isPJL = false;
    }
    return isPJL;
}

// https://github.com/file/file/blob/7449263e1d6167233b3b6abfc3e4c13407d6432c/magic/Magdir/animation#L265
// https://nokiatech.github.io/heif/technical.html
// TODO: need to figure out heif vs. heic
static FileType DetectHicAndAvif(Str d) {
    if (d.len < 0x18) {
        return FileType::Unknown;
    }
    Str s = d;
    Str hdr = Str(s.s + 4, s.len - 4);
    // ftyp values per https://github.com/strukturag/libheif/issues/83
    /*
        'heic': the usual HEIF images
        'heix': 10bit images, or anything that uses h265 with range extension
        'hevc', 'hevx': brands for image sequences
        'heim': multiview
        'heis': scalable
        'hevm': multiview sequence
        'hevs': scalable sequence

        'mif1' also happens?
    */
    // TODO: support more ftyp types?
    if (str::StartsWith(hdr, "ftypheic")) {
        return FileType::Heic;
    }
    if (str::StartsWith(hdr, "ftypheix")) {
        return FileType::Heic;
    }
    if (str::StartsWith(hdr, "ftypmif1")) {
        return FileType::Heic;
    }
    if (str::StartsWith(hdr, "ftypavif")) {
        return FileType::Avif;
    }
    hdr = Str(s.s + 16, s.len - 16);
    if (str::StartsWith(hdr, "mif1heic")) {
        return FileType::Heic;
    }
    return FileType::Unknown;
}

static bool HasWebpSignature(Str d) {
    return d.len > 12 && str::StartsWith(d, "RIFF") && str::StartsWith(Str(d.s + 8, d.len - 8), "WEBP");
}

static bool HasJxlSignature(Str d) {
    static const u8 jxlCodestream[] = {0xff, 0x0a};
    static const u8 jxlContainer[] = {0x00, 0x00, 0x00, 0x0c, 0x4a, 0x58, 0x4c, 0x20, 0x0d, 0x0a, 0x87, 0x0a};

    const u8* data = (const u8*)d.s;
    return (d.len >= (int)sizeof(jxlCodestream) && memeq(data, jxlCodestream, (int)sizeof(jxlCodestream))) ||
           (d.len >= (int)sizeof(jxlContainer) && memeq(data, jxlContainer, (int)sizeof(jxlContainer)));
}

#pragma pack(push, 1)
struct TgaHeader {
    u8 idLength;
    u8 cmapType;
    u8 imageType;
    u16 cmapFirstEntry;
    u16 cmapLength;
    u8 cmapBitDepth;
    u16 offsetX;
    u16 offsetY;
    u16 width;
    u16 height;
    u8 bitDepth;
    u8 flags;
};

struct TgaFooter {
    u32 extAreaOffset;
    u32 devAreaOffset;
    char signature[18];
};
#pragma pack(pop)

static_assert(sizeof(TgaHeader) == 18);
static_assert(sizeof(TgaFooter) == 26);

static bool HasTgaVersion2Footer(const u8* data, size_t n) {
    if (n < sizeof(TgaHeader) + sizeof(TgaFooter)) {
        return false;
    }
    const TgaFooter* footer = (const TgaFooter*)(data + n - sizeof(TgaFooter));
    return str::EqN(footer->signature, "TRUEVISION-XFILE.", sizeof(footer->signature));
}

static bool IsSupportedTgaPixelFormat(const TgaHeader* header) {
    const u8 typePalette = 1;
    const u8 typeTruecolor = 2;
    const u8 typeGrayscale = 3;
    const u8 typePaletteRle = 9;
    const u8 typeTruecolorRle = 10;
    const u8 typeGrayscaleRle = 11;
    const u8 alphaMask = 0x0f;

    int bits;
    if (header->imageType == typePalette || header->imageType == typePaletteRle) {
        if (header->cmapType != 1 || (header->bitDepth != 8 && header->bitDepth != 16)) {
            return false;
        }
        bits = header->cmapBitDepth;
    } else if (header->imageType == typeTruecolor || header->imageType == typeTruecolorRle) {
        bits = header->bitDepth;
    } else if (header->imageType == typeGrayscale || header->imageType == typeGrayscaleRle) {
        return header->bitDepth == 8 && (header->flags & alphaMask) == 0;
    } else {
        return false;
    }

    int alphaBits = header->flags & alphaMask;
    return (bits == 15 && alphaBits == 0) || (bits == 16 && (alphaBits == 0 || alphaBits == 1)) ||
           (bits == 24 && alphaBits == 0) || (bits == 32 && (alphaBits == 0 || alphaBits == 8));
}

static bool HasTgaSignature(Str d) {
    size_t n = (size_t)d.len;
    const u8* data = (const u8*)d.s;
    if (HasTgaVersion2Footer(data, n)) {
        return true;
    }
    if (n < sizeof(TgaHeader)) {
        return false;
    }
    const TgaHeader* header = (const TgaHeader*)data;
    if (header->cmapType != 0 && header->cmapType != 1) {
        return false;
    }
    if (header->flags & 0xc0) {
        return false;
    }
    return IsSupportedTgaPixelFormat(header);
}

// detect file type based on file content
static FileType DetectFileTypeFromData(Str d) {
    // TODO: sniff .fb2 content
    u8* data = (u8*)d.s;
    int dataLen = d.len;
    int n = (int)dimof(gFileSigs);

    for (int i = 0; i < n; i++) {
        Str sig = gFileSigs[i].sig;
        int off = gFileSigs[i].offset;
        int sigLen = gFileSigs[i].sigLen;
        int sigMaxLen = off + sigLen;
        u8* dat = data + off;
        if ((dataLen > sigMaxLen) && memeq(dat, sig.s, sigLen)) {
            return gFileSigs[i].ft;
        }
    }
    FileType ft = DetectHicAndAvif(d);
    if (ft != FileType::Unknown) {
        return ft;
    }

    if (IsPdfFileContent(d)) {
        return FileType::PDF;
    }
    if (IsPSFileContent(d)) {
        return FileType::PS;
    }
    if (HasTgaSignature(d)) {
        return FileType::Tga;
    }
    if (HasWebpSignature(d)) {
        return FileType::Webp;
    }
    if (HasJxlSignature(d)) {
        return FileType::Jxl;
    }
    return FileType::Unknown;
}

// PNG: dimensions from the IHDR chunk; for APNG the acTL chunk gives the frame count
static void PngInfoFromData(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    if (r.len < 24 || !memeq(r.d + 12, "IHDR", 4)) {
        return;
    }
    res.imageDx = (int)r.DWordBE(16);
    res.imageDy = (int)r.DWordBE(20);
    // look for an APNG acTL chunk, which must appear before IDAT
    int idx = 8;
    for (;;) {
        if (idx + 8 > r.len) {
            return;
        }
        u32 chunkLen = r.DWordBE(idx);
        const u8* type = r.d + idx + 4;
        if (memeq(type, "IDAT", 4)) {
            return;
        }
        if (memeq(type, "acTL", 4)) {
            int nFrames = (int)r.DWordBE(idx + 8);
            if (nFrames > 1) {
                res.nImages = nFrames;
            }
            return;
        }
        if (chunkLen > (u32)r.len) {
            return;
        }
        idx += 12 + (int)chunkLen; // length + type + data + crc
    }
}

// try to get image dimensions from EXIF sub-IFD (tags 0xA002/0xA003)
// tiffBase is the offset into r where the TIFF header starts
static bool JpegSizeFromExif(ByteReader r, int tiffBase, FileTypeInfo& res) {
    int n = r.len;
    if (tiffBase + 8 > n) {
        return false;
    }
    bool isBE = r.Byte(tiffBase) == 'M';
    // read IFD0 offset
    int ifdOff = (int)r.DWord(tiffBase + 4, isBE);
    int ifdAbs = tiffBase + ifdOff;
    if (ifdAbs + 2 > n) {
        return false;
    }
    u16 count = r.Word(ifdAbs, isBE);
    int exifIfdOff = 0;
    // scan IFD0 for ExifIFD pointer (tag 0x8769)
    for (u16 i = 0; i < count; i++) {
        int entryOff = ifdAbs + 2 + i * 12;
        if (entryOff + 12 > n) {
            break;
        }
        u16 tag = r.Word(entryOff, isBE);
        if (tag == 0x8769) {
            exifIfdOff = (int)r.DWord(entryOff + 8, isBE);
            break;
        }
    }
    if (exifIfdOff == 0) {
        return false;
    }
    // read EXIF sub-IFD
    int exifAbs = tiffBase + exifIfdOff;
    if (exifAbs + 2 > n) {
        return false;
    }
    count = r.Word(exifAbs, isBE);
    for (u16 i = 0; i < count; i++) {
        int entryOff = exifAbs + 2 + i * 12;
        if (entryOff + 12 > n) {
            break;
        }
        u16 tag = r.Word(entryOff, isBE);
        u16 type = r.Word(entryOff + 2, isBE);
        if (tag == 0xA002) {
            // PixelXDimension
            if (type == 4) {
                res.imageDx = (int)r.DWord(entryOff + 8, isBE);
            } else if (type == 3) {
                res.imageDx = r.Word(entryOff + 8, isBE);
            }
        } else if (tag == 0xA003) {
            // PixelYDimension
            if (type == 4) {
                res.imageDy = (int)r.DWord(entryOff + 8, isBE);
            } else if (type == 3) {
                res.imageDy = r.Word(entryOff + 8, isBE);
            }
        }
    }
    return res.imageDx > 0 && res.imageDy > 0;
}

static void JpegInfoFromData(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    // find the last start of frame marker for non-differential Huffman/arithmetic coding
    int n = r.len;
    int idx = 2;
    for (;;) {
        if (idx + 9 >= n) {
            return;
        }
        u8 b = r.Byte(idx);
        if (b != 0xff) {
            return;
        }
        b = r.Byte(idx + 1);
        if (0xC0 <= b && b <= 0xC3 || 0xC9 <= b && b <= 0xCB) {
            res.imageDx = r.WordBE(idx + 7);
            res.imageDy = r.WordBE(idx + 5);
            return;
        }
        int segLen = r.WordBE(idx + 2);
        int nextIdx = idx + segLen + 2;
        if (nextIdx + 9 >= n) {
            // can't read past this segment; if it's APP1/EXIF, try parsing dimensions from EXIF
            if (b == 0xE1 && idx + 10 < n) {
                // check for "Exif\0\0" signature
                if (r.Byte(idx + 4) == 'E' && r.Byte(idx + 5) == 'x' && r.Byte(idx + 6) == 'i' &&
                    r.Byte(idx + 7) == 'f' && r.Byte(idx + 8) == 0 && r.Byte(idx + 9) == 0) {
                    int tiffBase = idx + 10; // TIFF header starts after "Exif\0\0"
                    JpegSizeFromExif(r, tiffBase, res);
                }
            }
            return;
        }
        idx = nextIdx;
    }
}

// GIF: walk the blocks counting image descriptors; the first image's size is
// preferred over the "logical screen" size which is sometimes too large.
// If the data is truncated the frame count is a lower bound.
static void GifInfoFromData(ByteReader r, FileTypeInfo& res) {
    int n = r.len;
    if (n < 13) {
        res.nImages = 1;
        return;
    }
    int idx = 13;
    // skip the global color table
    if (r.Byte(10) & 0x80) {
        idx += 3 * (1 << ((r.Byte(10) & 0x07) + 1));
    }
    int nFrames = 0;
    while (idx < n) {
        u8 b = r.Byte(idx);
        if (b == 0x2C) { // image descriptor
            if (idx + 10 > n) {
                break;
            }
            nFrames++;
            if (nFrames == 1) {
                res.imageDx = r.WordLE(idx + 5);
                res.imageDy = r.WordLE(idx + 7);
            }
            u8 flags = r.Byte(idx + 9);
            idx += 10;
            // skip the local color table
            if (flags & 0x80) {
                idx += 3 * (1 << ((flags & 0x07) + 1));
            }
            idx += 1; // LZW minimum code size
            // skip image data sub-blocks
            while (idx < n && r.Byte(idx) != 0) {
                idx += r.Byte(idx) + 1;
            }
            idx++;              // block terminator
        } else if (b == 0x21) { // extension: introducer, label, then sub-blocks
            idx += 2;
            while (idx < n && r.Byte(idx) != 0) {
                idx += r.Byte(idx) + 1;
            }
            idx++; // block terminator
        } else {
            break; // 0x3B trailer or corrupt data
        }
    }
    res.nImages = nFrames > 0 ? nFrames : 1;
    if (res.imageDx == 0 && res.imageDy == 0) {
        // no image descriptor seen: fall back to the logical screen size
        res.imageDx = r.WordLE(6);
        res.imageDy = r.WordLE(8);
    }
}

// TIFF and JXR: dimensions from the first IFD; number of images is the
// length of the next-IFD chain
static void TiffInfoFromData(ByteReader r, FileTypeInfo& res, bool isJxr) {
    if (r.len < 10) {
        res.nImages = 1;
        return;
    }
    bool isBE = r.Byte(0) == 'M';
    const u16 wTag = isJxr ? 0xBC80 : 0x0100;
    const u16 hTag = isJxr ? 0xBC81 : 0x0101;
    int idx = (int)r.DWord(4, isBE);
    u16 count = idx <= r.len - 2 ? r.Word(idx, isBE) : 0;
    for (idx += 2; count > 0 && idx <= r.len - 12; count--, idx += 12) {
        u16 tag = r.Word(idx, isBE);
        u16 type = r.Word(idx + 2, isBE);
        if (r.DWord(idx + 4, isBE) != 1) {
            continue;
        } else if (wTag == tag && 4 == type) {
            res.imageDx = (int)r.DWord(idx + 8, isBE);
        } else if (wTag == tag && 3 == type) {
            res.imageDx = r.Word(idx + 8, isBE);
        } else if (wTag == tag && 1 == type) {
            res.imageDx = r.Byte(idx + 8);
        } else if (hTag == tag && 4 == type) {
            res.imageDy = (int)r.DWord(idx + 8, isBE);
        } else if (hTag == tag && 3 == type) {
            res.imageDy = r.Word(idx + 8, isBE);
        } else if (hTag == tag && 1 == type) {
            res.imageDy = r.Byte(idx + 8);
        }
    }
    int nIfds = 0;
    u32 off = r.DWord(4, isBE);
    // 4096 iterations bound protects against cycles in corrupt data
    while (off > 0 && (int)off + 2 <= r.len && nIfds < 4096) {
        nIfds++;
        u16 nEntries = r.Word((int)off, isBE);
        int nextOff = (int)off + 2 + nEntries * 12;
        if (nextOff + 4 > r.len) {
            break;
        }
        off = r.DWord(nextOff, isBE);
    }
    res.nImages = nIfds > 0 ? nIfds : 1;
}

static void BmpInfoFromData(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    // 14-byte file header followed by BITMAPINFOHEADER
    if (r.len < 26) {
        return;
    }
    res.imageDx = (int)r.DWordLE(18);
    int dy = (int)r.DWordLE(22);
    res.imageDy = dy >= 0 ? dy : -dy; // negative height means a top-down bitmap
}

static void TgaInfoFromData(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    if (r.len >= 16) {
        res.imageDx = r.WordLE(12);
        res.imageDy = r.WordLE(14);
    }
}

#define JP2_JP2H 0x6a703268 /**< JP2 header box (super-box) */
#define JP2_IHDR 0x69686472 /**< Image header box */

static void Jp2InfoFromData(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    int n = r.len;
    if (n < 32) {
        return;
    }
    int idx = 0;
    while (idx < n - 32) {
        u32 boxLen = r.DWordBE(idx);
        u32 boxType = r.DWordBE(idx + 4);
        if (JP2_JP2H == boxType) {
            idx += 8;
            u32 boxLen2 = r.DWordBE(idx);
            u32 boxType2 = r.DWordBE(idx + 4);
            bool isIhdr = boxType2 == JP2_IHDR;
            idx += 8;
            if (isIhdr && boxLen2 <= (boxLen - 8)) {
                int dy = (int)r.DWordBE(idx);
                int dx = (int)r.DWordBE(idx + 4);
                if (dx > 64 * 1024 || dy > 64 * 1024) {
                    // sanity check, assuming that images that big can't
                    // possibly be valid
                    return;
                }
                res.imageDx = dx;
                res.imageDy = dy;
                return;
            }
            break;
        } else if (boxLen != 0 && (u32)idx < UINT32_MAX - boxLen) {
            idx += boxLen;
        } else {
            break;
        }
    }
}

// WebP: walk the RIFF chunks; canvas size from VP8X, frame size from
// VP8 (lossy) / VP8L (lossless), animation frame count from ANMF chunks
static void WebpInfoFromData(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    int nFrames = 0;
    int idx = 12;
    while (idx + 8 <= r.len) {
        const u8* fourcc = r.d + idx;
        u32 size = r.DWordLE(idx + 4);
        int payload = idx + 8;
        if (memeq(fourcc, "VP8X", 4) && size >= 10) {
            // 4 flag bytes, then 24-bit little-endian width-1 and height-1
            res.imageDx = 1 + (r.Byte(payload + 4) | (r.Byte(payload + 5) << 8) | (r.Byte(payload + 6) << 16));
            res.imageDy = 1 + (r.Byte(payload + 7) | (r.Byte(payload + 8) << 8) | (r.Byte(payload + 9) << 16));
        } else if (memeq(fourcc, "VP8 ", 4) && res.imageDx == 0 && size >= 10) {
            res.imageDx = r.WordLE(payload + 6) & 0x3fff;
            res.imageDy = r.WordLE(payload + 8) & 0x3fff;
        } else if (memeq(fourcc, "VP8L", 4) && res.imageDx == 0 && size >= 5 && r.Byte(payload) == 0x2f) {
            u32 bits = r.DWordLE(payload + 1);
            res.imageDx = (int)(bits & 0x3FFF) + 1;
            res.imageDy = (int)((bits >> 14) & 0x3FFF) + 1;
        } else if (memeq(fourcc, "ANMF", 4)) {
            nFrames++;
        }
        u32 advance = size + (size & 1); // chunks are padded to even size
        if (advance >= (u32)r.len) {
            break;
        }
        idx = payload + (int)advance;
    }
    if (nFrames > 0) {
        res.nImages = nFrames;
    }
}

// Like GuessFileTypeFromData() but for image files also reports the number
// of images and, for single-image files, width/height parsed from the header
// (hasImageSize tells if they were found; if not, callers can fall back to a
// more expensive method like decoding the image). Dimensions are as encoded
// i.e. EXIF orientation is not applied; use ImageSizeFromData() for display
// dimensions. Counts reflect the provided buffer: they can under-report if d
// is a truncated prefix of the file.
FileTypeInfo GuessFileInfoFromData(Str d) {
    FileTypeInfo res;
    res.ft = DetectFileTypeFromData(d);
    ByteReader r(d);
    switch (res.ft) {
        case FileType::Png:
            PngInfoFromData(r, res);
            break;
        case FileType::Jpeg:
            JpegInfoFromData(r, res);
            break;
        case FileType::Gif:
            GifInfoFromData(r, res);
            break;
        case FileType::Bmp:
            BmpInfoFromData(r, res);
            break;
        case FileType::Tiff:
            TiffInfoFromData(r, res, false);
            break;
        case FileType::Jxr:
            TiffInfoFromData(r, res, true);
            break;
        case FileType::Tga:
            TgaInfoFromData(r, res);
            break;
        case FileType::Jp2:
            Jp2InfoFromData(r, res);
            break;
        case FileType::Webp:
            WebpInfoFromData(r, res);
            break;
        case FileType::Jxl:
        case FileType::Heic:
        case FileType::Avif:
            // TODO: dimensions require full container/bitstream parsing
            res.nImages = 1;
            break;
        default:
            break;
    }
    if (res.nImages != 1) {
        // imageDx/imageDy are only reported for single-image files
        res.imageDx = 0;
        res.imageDy = 0;
    }
    res.hasImageSize = res.imageDx > 0 && res.imageDy > 0;
    return res;
}

FileType GuessFileTypeFromData(Str d) {
    return GuessFileInfoFromData(d).ft;
}

// parse an embedded-PDF path of the form "c:/foo.pdf:${pdfStreamNo}"
// or "c:/foo.pdf:${pdfStreamNo}:attachname=${hexUtf8Name}" into its pieces
EmbeddedPdfName ParseEmbeddedPdfName(Str path) {
    EmbeddedPdfName res;
    if (!path) {
        return res;
    }

    // find the last ":attachname=" occurrence, if any
    Str meta;
    int searchOff = 0;
    while (searchOff < path.len) {
        Str rest = Str(path.s + searchOff, path.len - searchOff);
        int idx = str::IndexOf(rest, StrL(":attachname="));
        if (idx < 0) {
            break;
        }
        int matchOff = searchOff + idx;
        meta = Str(path.s + matchOff, path.len - matchOff);
        searchOff = matchOff + 1;
    }

    // stream number: ":${digits}" right before the attachname meta (or at the end of path)
    Str parseEnd = meta ? Str(path.s, (int)(meta.s - path.s)) : path;
    int endIdx = parseEnd.len - 1;
    int nDigits = 0;
    while (endIdx >= 0) {
        char c = parseEnd.s[endIdx];
        if (c == ':') {
            if (nDigits > 0) {
                res.streamNoStr = Str(parseEnd.s + endIdx, parseEnd.len - endIdx);
            }
            break;
        }
        if (!str::IsDigit(c)) {
            break;
        }
        nDigits++;
        endIdx--;
    }

    // attachment name: hex-decoded utf8 after ":attachname="
    if (meta) {
        int prefixLen = LenL(":attachname=");
        Str hex = Str(meta.s + prefixLen, meta.len - prefixLen);
        int hexLen = hex.len;
        if (hexLen > 0 && (hexLen % 2) == 0) {
            int nameLen = hexLen / 2;
            char* name = AllocArrayTemp<char>(nameLen + 1);
            if (str::HexToMem(hex, Str(name, nameLen))) {
                name[nameLen] = 0;
                res.fileName = Str(name, nameLen);
            }
        }
    }
    return res;
}

FileType GuessFileTypeFromName(Str path) {
    VerifyExtsMatch();

    if (!path) {
        return FileType::Unknown;
    }
    if (path::IsDirectory(path)) {
        return FileType::Directory;
    }
    FileType res = GetTypeByFileExt(path);
    if (res != FileType::Unknown) {
        return res;
    }

    // cases that cannot be decided just by looking at file extension
    if (ParseEmbeddedPdfName(path).streamNoStr) {
        return FileType::PDF;
    }

    return FileType::Unknown;
}

// clang-format off
static const FileType gImageTypes[] = {
    FileType::Png,
    FileType::Jpeg,
    FileType::Jpeg,
    FileType::Gif,
    FileType::Bmp,
    FileType::Tiff,
    FileType::Tiff,
    FileType::Tga,
    FileType::Jxr,
    FileType::Webp,
    FileType::Jp2,
    FileType::Heic,
    FileType::Avif
};

static const char* gImageFormatExts =
    ".png\0"
    ".jpg\0"
    ".jpeg\0"
    ".gif\0"
    ".bmp\0"
    ".tif\0"
    ".tiff\0"
    ".tga\0"
    ".jxr\0"
    ".webp\0"
    ".jp2\0"
    ".heic\0"
    ".avif\0"
    "\0";
// clang-format on

TempStr GfxFileExtFromTypeTemp(FileType ft) {
    int idx = FileTypeIndexOf(gImageTypes, dimofi(gImageTypes), ft);
    if (idx >= 0) {
        return SeqStrByIndex(gImageFormatExts, idx);
    }
    return {};
}

TempStr GfxFileExtFromDataTemp(Str d) {
    FileType ft = GuessFileTypeFromData(d);
    return GfxFileExtFromTypeTemp(ft);
}

// compares the guessed type's canonical extension (the first extension
// registered for it, e.g. ".pdf" for sample.ai) to expectedExt
TempStr FileKindResultTemp(Str path, Str expectedExt, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (len(path) == 0 || len(expectedExt) == 0) {
        return fail(StrL("ERROR missing path or expectedExt"));
    }
    FileType ft = GuessFileTypeFromName(path);
    if (ft == FileType::Unknown) {
        return fail(StrL("ERROR unknown-kind"));
    }
    TempStr ext = GetExtForFileTypeTemp(ft);
    if (!str::EqI(ext, expectedExt)) {
        out.Append(fmt("FAIL path=%s got=%s expected=%s\n", path, ext, expectedExt));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }
    out.Append(fmt("OK path=%s ext=%s\n", path, ext));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}
