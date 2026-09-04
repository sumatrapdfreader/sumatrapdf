
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// This file must only contain code that doesn't depend on
// external libraries (ext/). GuessFileTypeFromFile.cpp has
// the parts that need base/Archive.h (and thus ext/libarchive).

#include "base/Base.h"
#include "base/File.h"
#include "base/ByteReaderWriter.h"
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
    V(".lit", FileType::Lit)           \
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
    V(".ico", FileType::Ico)           \
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
    // Prefer the longest registered suffix so multi-dot names like
    // "book.fb2.zip" map to Fb2z rather than Zip (path::GetExtTemp only
    // returns the last ".zip" component).
    int n = dimofi(gExtsType);
    int bestIdx = -1;
    int bestLen = 0;
    for (int i = 0; i < n; i++) {
        TempStr ext = SeqStrByIndex(gFileExts, i);
        if (len(ext) == 0) {
            continue;
        }
        if (!str::EndsWithI(path, ext)) {
            continue;
        }
        if (len(ext) > bestLen) {
            bestLen = len(ext);
            bestIdx = i;
        }
    }
    if (bestIdx < 0) {
        return FileType::Unknown;
    }
    return gExtsType[bestIdx];
}

TempStr GetExtForFileTypeTemp(FileType ft) {
    int idx = FileTypeIndexOf(gExtsType, dimofi(gExtsType), ft);
    if (idx >= 0) {
        return SeqStrByIndex(gFileExts, idx);
    }
    return {};
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
    V(0, "ITOLITLS", FileType::Lit)                       \
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
    V(0, "BA", FileType::Bmp)                             \
    V(0, "\0\0\x01\x00", FileType::Ico)                   \
    V(0, "MM\x00\x2A", FileType::Tiff)                    \
    V(0, "II\x2A\x00", FileType::Tiff)                    \
    V(0, "II\xBC\x01", FileType::Jxr)                     \
    V(0, "II\xBC\x00", FileType::Jxr)                     \
    V(0, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", FileType::Jp2) \
    V(0, "\xFF\x4F\xFF\x51", FileType::Jp2)               \
    V(0, "AT&T", FileType::DjVu)

// a file signaure is a sequence of bytes at a specific
// offset in the file
struct FileSig {
    int offset;
    Str sig;
    int sigLen;
    FileType ft;
};

#define MK_SIG(OFF, SIG, FT) {OFF, StrL(SIG), (int)(sizeof(SIG) - 1), FT},
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
    if (str::StartsWith(header, StrL("\xC5\xD0\xD3\xC6"))) {
        DWORD psStart = ByteReader(d).UInt32LE(4);
        if ((int)psStart >= n - 12) {
            return true;
        }
        Str sub = Str(header.s + psStart, header.len - (int)psStart);
        return str::StartsWith(sub, StrL("%!PS-Adobe-"));
    }
    if (str::StartsWith(header, StrL("%!PS-Adobe-"))) {
        return true;
    }
    // PJL (Printer Job Language) files containing Postscript data
    // https://developers.hp.com/system/files/PJL_Technical_Reference_Manual.pdf
    bool isPJL = str::StartsWith(header, StrL("\x1B%-12345X@PJL"));
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
    if (str::StartsWith(hdr, StrL("ftypheic"))) {
        return FileType::Heic;
    }
    if (str::StartsWith(hdr, StrL("ftypheix"))) {
        return FileType::Heic;
    }
    if (str::StartsWith(hdr, StrL("ftypmif1"))) {
        return FileType::Heic;
    }
    if (str::StartsWith(hdr, StrL("ftypavif"))) {
        return FileType::Avif;
    }
    hdr = Str(s.s + 16, s.len - 16);
    if (str::StartsWith(hdr, StrL("mif1heic"))) {
        return FileType::Heic;
    }
    return FileType::Unknown;
}

static bool HasWebpSignature(Str d) {
    return d.len >= 12 && str::StartsWith(d, StrL("RIFF")) && str::StartsWith(Str(d.s + 8, d.len - 8), StrL("WEBP"));
}

// payload of the first RIFF chunk whose FourCC is fourcc. out is a view into d.
bool FindWebpChunk(Str d, const char fourcc[4], Str& out) {
    out = {};
    if (!fourcc || !HasWebpSignature(d)) {
        return false;
    }
    ByteReader r(d);
    int idx = 12;
    while (idx + 8 <= r.len) {
        int size = (int)r.UInt32LE(idx + 4);
        int payload = idx + 8;
        if (size < 0 || payload + size > r.len) {
            return false;
        }
        if (MemEq(r.d + idx, fourcc, 4)) {
            out = Str((char*)(r.d + payload), size);
            return true;
        }
        int chunkSize = size + (size & 1);
        if (chunkSize < size) {
            return false;
        }
        idx = payload + chunkSize;
        if (idx < 8) {
            return false;
        }
    }
    return false;
}

static bool HasJxlSignature(Str d) {
    static const u8 jxlCodestream[] = {0xff, 0x0a};
    static const u8 jxlContainer[] = {0x00, 0x00, 0x00, 0x0c, 0x4a, 0x58, 0x4c, 0x20, 0x0d, 0x0a, 0x87, 0x0a};

    const u8* data = (const u8*)d.s;
    return (d.len >= sizeofi(jxlCodestream) && MemEq(data, jxlCodestream, sizeofi(jxlCodestream))) ||
           (d.len >= sizeofi(jxlContainer) && MemEq(data, jxlContainer, sizeofi(jxlContainer)));
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
    return str::EqN(Str(footer->signature), StrL("TRUEVISION-XFILE."), sizeof(footer->signature));
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
    int n = dimofi(gFileSigs);

    for (int i = 0; i < n; i++) {
        Str sig = gFileSigs[i].sig;
        int off = gFileSigs[i].offset;
        int sigLen = gFileSigs[i].sigLen;
        int sigMaxLen = off + sigLen;
        u8* dat = data + off;
        if ((dataLen > sigMaxLen) && MemEq(dat, sig.s, sigLen)) {
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

// append an image size to res.imageSizes, growing it geometrically.
// n is the number of sizes appended so far, cap the current capacity
static void AppendImageSize(FileTypeInfo& res, int n, int& cap, int dx, int dy) {
    if (n >= cap) {
        int newCap = cap ? cap * 2 : 4;
        Size* p = (Size*)realloc(res.imageSizes, (size_t)newCap * sizeof(Size));
        if (!p) {
            return;
        }
        res.imageSizes = p;
        cap = newCap;
    }
    res.imageSizes[n] = Size(dx, dy);
}

// PNG: dimensions from the IHDR chunk; for APNG the acTL chunk gives the
// frame count and each fcTL chunk a frame's size
static void ParsePng(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    if (r.len < 24 || !MemEq(r.d + 12, "IHDR", 4)) {
        return;
    }
    res.imageDx = (int)r.UInt32BE(16);
    res.imageDy = (int)r.UInt32BE(20);
    int nDeclared = 0; // frame count from acTL
    int nFcTL = 0;
    int cap = 0;
    int idx = 8;
    for (;;) {
        if (idx + 8 > r.len) {
            break;
        }
        u32 chunkLen = r.UInt32BE(idx);
        const u8* type = r.d + idx + 4;
        if (MemEq(type, "acTL", 4)) {
            nDeclared = (int)r.UInt32BE(idx + 8);
        } else if (MemEq(type, "fcTL", 4) && chunkLen >= 12) {
            // sequence number, then u32 width and height
            AppendImageSize(res, nFcTL, cap, (int)r.UInt32BE(idx + 12), (int)r.UInt32BE(idx + 16));
            nFcTL++;
        } else if (MemEq(type, "IEND", 4)) {
            break;
        }
        if (chunkLen > (u32)r.len) {
            break;
        }
        idx += 12 + (int)chunkLen; // length + type + data + crc
    }
    if (nDeclared > 1) {
        res.nImages = nDeclared;
    }
    if (nFcTL != res.nImages) {
        // e.g. truncated data with fewer fcTL chunks than acTL declares
        free(res.imageSizes);
        res.imageSizes = nullptr;
    }
}

// try to get image dimensions from EXIF sub-IFD (tags 0xA002/0xA003)
// tiffBase is the offset into r where the TIFF header starts
static bool JpegSizeFromExif(ByteReader r, int tiffBase, FileTypeInfo& res) {
    int n = r.len;
    if (tiffBase + 8 > n) {
        return false;
    }
    bool isBE = r.UInt8(tiffBase) == 'M';
    // read IFD0 offset
    int ifdOff = (int)r.UInt32(tiffBase + 4, isBE);
    int ifdAbs = tiffBase + ifdOff;
    if (ifdAbs + 2 > n) {
        return false;
    }
    u16 count = r.UInt16(ifdAbs, isBE);
    int exifIfdOff = 0;
    // scan IFD0 for ExifIFD pointer (tag 0x8769)
    for (u16 i = 0; i < count; i++) {
        int entryOff = ifdAbs + 2 + (i * 12);
        if (entryOff + 12 > n) {
            break;
        }
        u16 tag = r.UInt16(entryOff, isBE);
        if (tag == 0x8769) {
            exifIfdOff = (int)r.UInt32(entryOff + 8, isBE);
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
    count = r.UInt16(exifAbs, isBE);
    for (u16 i = 0; i < count; i++) {
        int entryOff = exifAbs + 2 + (i * 12);
        if (entryOff + 12 > n) {
            break;
        }
        u16 tag = r.UInt16(entryOff, isBE);
        u16 type = r.UInt16(entryOff + 2, isBE);
        if (tag == 0xA002) {
            // PixelXDimension
            if (type == 4) {
                res.imageDx = (int)r.UInt32(entryOff + 8, isBE);
            } else if (type == 3) {
                res.imageDx = r.UInt16(entryOff + 8, isBE);
            }
        } else if (tag == 0xA003) {
            // PixelYDimension
            if (type == 4) {
                res.imageDy = (int)r.UInt32(entryOff + 8, isBE);
            } else if (type == 3) {
                res.imageDy = r.UInt16(entryOff + 8, isBE);
            }
        }
    }
    return res.imageDx > 0 && res.imageDy > 0;
}

static void ParseJpeg(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    int n = r.len;
    int idx = 2;
    for (;;) {
        // resync to the next marker, skipping garbage and 0xff fill bytes
        while (idx < n && r.UInt8(idx) != 0xff) {
            idx++;
        }
        while (idx < n && r.UInt8(idx) == 0xff) {
            idx++;
        }
        if (idx >= n) {
            return;
        }
        u8 marker = r.UInt8(idx);
        idx++;
        if (0xC0 <= marker && marker <= 0xC3 || 0xC9 <= marker && marker <= 0xCB) {
            // start of frame for non-differential Huffman/arithmetic coding:
            // segment length, precision, then height and width
            if (idx + 7 > n) {
                return;
            }
            res.imageDy = r.UInt16BE(idx + 3);
            res.imageDx = r.UInt16BE(idx + 5);
            return;
        }
        if (marker == 0xDA || marker == 0xD9) {
            // start of scan / end of image without a start of frame
            return;
        }
        if (marker == 0x01 || (0xD0 <= marker && marker <= 0xD8)) {
            // standalone marker without a segment
            continue;
        }
        int segLen = r.UInt16BE(idx);
        if (segLen < 2) {
            return;
        }
        if (marker == 0xE1 && idx + 8 <= n) {
            // APP1: if it's EXIF, opportunistically parse dimensions from it;
            // a start of frame later in the buffer overwrites them, but for a
            // truncated buffer this may be the only size available
            if (r.UInt8(idx + 2) == 'E' && r.UInt8(idx + 3) == 'x' && r.UInt8(idx + 4) == 'i' &&
                r.UInt8(idx + 5) == 'f' && r.UInt8(idx + 6) == 0 && r.UInt8(idx + 7) == 0) {
                JpegSizeFromExif(r, idx + 8, res);
            }
        }
        idx += segLen;
    }
}

// GIF: walk the blocks counting image descriptors; the first image's size is
// preferred over the "logical screen" size which is sometimes too large.
// If the data is truncated the frame count is a lower bound.
static void ParseGif(ByteReader r, FileTypeInfo& res) {
    int n = r.len;
    if (n < 13) {
        res.nImages = 1;
        return;
    }
    int idx = 13;
    // skip the global color table
    if (r.UInt8(10) & 0x80) {
        idx += 3 * (1 << ((r.UInt8(10) & 0x07) + 1));
    }
    int nFrames = 0;
    int cap = 0;
    while (idx < n) {
        u8 b = r.UInt8(idx);
        if (b == 0x2C) { // image descriptor
            if (idx + 10 > n) {
                break;
            }
            int w = r.UInt16LE(idx + 5);
            int h = r.UInt16LE(idx + 7);
            if (nFrames == 0) {
                res.imageDx = w;
                res.imageDy = h;
            }
            AppendImageSize(res, nFrames, cap, w, h);
            nFrames++;
            u8 flags = r.UInt8(idx + 9);
            idx += 10;
            // skip the local color table
            if (flags & 0x80) {
                idx += 3 * (1 << ((flags & 0x07) + 1));
            }
            idx += 1; // LZW minimum code size
            // skip image data sub-blocks
            while (idx < n && r.UInt8(idx) != 0) {
                idx += r.UInt8(idx) + 1;
            }
            idx++;              // block terminator
        } else if (b == 0x21) { // extension: introducer, label, then sub-blocks
            idx += 2;
            while (idx < n && r.UInt8(idx) != 0) {
                idx += r.UInt8(idx) + 1;
            }
            idx++; // block terminator
        } else {
            break; // 0x3B trailer or corrupt data
        }
    }
    res.nImages = nFrames > 0 ? nFrames : 1;
    if (res.imageDx == 0 && res.imageDy == 0) {
        // no image descriptor seen: fall back to the logical screen size
        res.imageDx = r.UInt16LE(6);
        res.imageDy = r.UInt16LE(8);
    }
}

// dimensions from the width/height tags of the IFD at off
static Size TiffIfdSize(ByteReader r, int off, bool isBE, bool isJxr) {
    const u16 wTag = isJxr ? 0xBC80 : 0x0100;
    const u16 hTag = isJxr ? 0xBC81 : 0x0101;
    Size res;
    u16 count = off <= r.len - 2 ? r.UInt16(off, isBE) : 0;
    for (int idx = off + 2; count > 0 && idx <= r.len - 12; count--, idx += 12) {
        u16 tag = r.UInt16(idx, isBE);
        if (tag != wTag && tag != hTag) {
            continue;
        }
        u16 type = r.UInt16(idx + 2, isBE);
        int typeSize = 0;
        if (type == 1) {
            typeSize = 1;
        } else if (type == 3) {
            typeSize = 2;
        } else if (type == 4) {
            typeSize = 4;
        }
        u32 nVals = r.UInt32(idx + 4, isBE);
        if (typeSize == 0 || nVals == 0) {
            continue;
        }
        // the value is inline if it fits in the 4-byte field, otherwise the
        // field holds the offset of the value; either way read the first one
        int valOff = idx + 8;
        if (nVals > 4u / typeSize) {
            valOff = (int)r.UInt32(idx + 8, isBE);
            if (valOff < 0 || valOff + typeSize > r.len) {
                continue;
            }
        }
        int val;
        if (typeSize == 4) {
            val = (int)r.UInt32(valOff, isBE);
        } else if (typeSize == 2) {
            val = r.UInt16(valOff, isBE);
        } else {
            val = r.UInt8(valOff);
        }
        if (tag == wTag) {
            res.dx = val;
        } else {
            res.dy = val;
        }
    }
    return res;
}

// TIFF and JXR: one image per IFD in the next-IFD chain, with dimensions
// from each IFD's width/height tags
static void ParseTiff(ByteReader r, FileTypeInfo& res, bool isJxr) {
    res.nImages = 1;
    if (r.len < 10) {
        return;
    }
    bool isBE = r.UInt8(0) == 'M';
    int nIfds = 0;
    int cap = 0;
    u32 off = r.UInt32(4, isBE);
    // 4096 iterations bound protects against cycles in corrupt data
    while (off > 0 && (int)off + 2 <= r.len && nIfds < 4096) {
        Size size = TiffIfdSize(r, (int)off, isBE, isJxr);
        if (nIfds == 0) {
            res.imageDx = size.dx;
            res.imageDy = size.dy;
        }
        AppendImageSize(res, nIfds, cap, size.dx, size.dy);
        nIfds++;
        u16 nEntries = r.UInt16((int)off, isBE);
        int nextOff = (int)off + 2 + (nEntries * 12);
        if (nextOff + 4 > r.len) {
            break;
        }
        off = r.UInt32(nextOff, isBE);
    }
    if (nIfds > 0) {
        res.nImages = nIfds;
    }
}

static void ParseIco(ByteReader r, FileTypeInfo& res) {
    // ICONDIR (6 bytes) + ICONDIRENTRY[n] (16 bytes each). Width/height 0 means 256.
    // PNG-compressed images (Vista+) store the real size in the PNG IHDR.
    if (r.len < 6) {
        return;
    }
    int n = r.UInt16LE(4);
    if (n <= 0) {
        return;
    }
    n = std::min(n, 256);
    int cap = 0;
    int got = 0;
    for (int i = 0; i < n; i++) {
        int ent = 6 + i * 16;
        if (ent + 16 > r.len) {
            break;
        }
        int dx = r.UInt8(ent);
        int dy = r.UInt8(ent + 1);
        if (dx == 0) {
            dx = 256;
        }
        if (dy == 0) {
            dy = 256;
        }
        int imgOff = (int)r.UInt32LE(ent + 12);
        if (imgOff >= 0 && imgOff + 24 <= r.len && MemEq(r.d + imgOff, "\x89PNG\r\n\x1a\n", 8) &&
            MemEq(r.d + imgOff + 12, "IHDR", 4)) {
            dx = (int)r.UInt32BE(imgOff + 16);
            dy = (int)r.UInt32BE(imgOff + 20);
        }
        if (dx > 0 && dy > 0) {
            AppendImageSize(res, got, cap, dx, dy);
            got++;
        }
    }
    res.nImages = got;
    if (got == 1 && res.imageSizes) {
        res.imageDx = res.imageSizes[0].dx;
        res.imageDy = res.imageSizes[0].dy;
    }
}

static void ParseBmp(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    int off = 0;
    // "BA" is an OS/2 bitmap array: a 14-byte array header followed by the
    // first bitmap's regular "BM" file header
    if (r.UInt8(0) == 'B' && r.UInt8(1) == 'A') {
        off = 14;
        if (r.UInt8(off) != 'B' || r.UInt8(off + 1) != 'M') {
            return;
        }
    }
    // 14-byte file header followed by the info header, whose first field is
    // its own size: 12 for an OS/2 BITMAPCOREHEADER with u16 dimensions,
    // otherwise BITMAPINFOHEADER or later with i32 dimensions
    if (r.UInt32LE(off + 14) == 12) {
        res.imageDx = r.UInt16LE(off + 18);
        res.imageDy = r.UInt16LE(off + 20);
        return;
    }
    if (r.len < off + 26) {
        return;
    }
    res.imageDx = (int)r.UInt32LE(off + 18);
    int dy = (int)r.UInt32LE(off + 22);
    res.imageDy = dy >= 0 ? dy : -dy; // negative height means a top-down bitmap
}

static void ParseTga(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    if (r.len >= 16) {
        res.imageDx = r.UInt16LE(12);
        res.imageDy = r.UInt16LE(14);
    }
}

// WebP: walk the RIFF chunks; canvas size from VP8X, frame size from
// VP8 (lossy) / VP8L (lossless), animation frame count from ANMF chunks
static void ParseWebp(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    int nFrames = 0;
    int cap = 0;
    int idx = 12;
    while (idx + 8 <= r.len) {
        const u8* fourcc = r.d + idx;
        u32 size = r.UInt32LE(idx + 4);
        int payload = idx + 8;
        if (MemEq(fourcc, "VP8X", 4) && size >= 10) {
            // 4 flag bytes, then 24-bit little-endian width-1 and height-1
            res.imageDx = 1 + (r.UInt8(payload + 4) | (r.UInt8(payload + 5) << 8) | (r.UInt8(payload + 6) << 16));
            res.imageDy = 1 + (r.UInt8(payload + 7) | (r.UInt8(payload + 8) << 8) | (r.UInt8(payload + 9) << 16));
        } else if (MemEq(fourcc, "VP8 ", 4) && res.imageDx == 0 && size >= 10) {
            res.imageDx = r.UInt16LE(payload + 6) & 0x3fff;
            res.imageDy = r.UInt16LE(payload + 8) & 0x3fff;
        } else if (MemEq(fourcc, "VP8L", 4) && res.imageDx == 0 && size >= 5 && r.UInt8(payload) == 0x2f) {
            u32 bits = r.UInt32LE(payload + 1);
            res.imageDx = (int)(bits & 0x3FFF) + 1;
            res.imageDy = (int)((bits >> 14) & 0x3FFF) + 1;
        } else if (MemEq(fourcc, "ANMF", 4) && size >= 12) {
            // 24-bit little-endian frame x, y, then width-1 and height-1
            int w = 1 + (r.UInt8(payload + 6) | (r.UInt8(payload + 7) << 8) | (r.UInt8(payload + 8) << 16));
            int h = 1 + (r.UInt8(payload + 9) | (r.UInt8(payload + 10) << 8) | (r.UInt8(payload + 11) << 16));
            AppendImageSize(res, nFrames, cap, w, h);
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

// EXIF orientations 5-8 swap width and height
bool ExifOrientationSwapsDimensions(int orientation) {
    return orientation >= 5 && orientation <= 8;
}

// Read EXIF orientation from IFD0 (tag 0x0112). Returns 1-8 or 0 if not found.
// tiffBase is the offset into r where the TIFF header starts
static int ExifOrientationFromTiff(ByteReader r, int tiffBase) {
    int n = r.len;
    if (tiffBase + 8 > n) {
        return 0;
    }
    bool isBE = r.UInt8(tiffBase) == 'M';
    int ifdOff = (int)r.UInt32(tiffBase + 4, isBE);
    int ifdAbs = tiffBase + ifdOff;
    if (ifdAbs + 2 > n) {
        return 0;
    }
    u16 count = r.UInt16(ifdAbs, isBE);
    for (u16 i = 0; i < count; i++) {
        int entryOff = ifdAbs + 2 + (i * 12);
        if (entryOff + 12 > n) {
            break;
        }
        u16 tag = r.UInt16(entryOff, isBE);
        if (tag == 0x0112) { // Orientation tag
            return r.UInt16(entryOff + 8, isBE);
        }
    }
    return 0;
}

// Read EXIF orientation from JPEG data. Returns 1-8 or 0 if not found.
static int JpegExifOrientation(ByteReader r) {
    int n = r.len;
    int idx = 2;
    for (;;) {
        // resync to the next marker, skipping garbage and 0xff fill bytes
        while (idx < n && r.UInt8(idx) != 0xff) {
            idx++;
        }
        while (idx < n && r.UInt8(idx) == 0xff) {
            idx++;
        }
        if (idx >= n) {
            return 0;
        }
        u8 marker = r.UInt8(idx);
        idx++;
        if (marker == 0xDA || marker == 0xD9) { // start of scan / end of image, stop
            return 0;
        }
        if (marker == 0x01 || (0xD0 <= marker && marker <= 0xD8)) {
            // standalone marker without a segment
            continue;
        }
        if (marker == 0xE1 && idx + 8 <= n) {
            // APP1 - check for EXIF
            if (r.UInt8(idx + 2) == 'E' && r.UInt8(idx + 3) == 'x' && r.UInt8(idx + 4) == 'i' &&
                r.UInt8(idx + 5) == 'f' && r.UInt8(idx + 6) == 0 && r.UInt8(idx + 7) == 0) {
                return ExifOrientationFromTiff(r, idx + 8);
            }
        }
        int segLen = r.UInt16BE(idx);
        if (segLen < 2) {
            return 0;
        }
        idx += segLen;
    }
}

// Read EXIF orientation from a WebP EXIF chunk. Returns 1-8 or 0 if not found.
int WebpExifOrientation(Str d) {
    Str exif;
    if (!FindWebpChunk(d, "EXIF", exif) || exif.len < 8) {
        return 0;
    }
    return ExifOrientationFromTiff(ByteReader(exif), 0);
}

// find a box of the given type among the ISO BMFF boxes in [idx, end).
// Returns the offset of the box's payload and sets boxEndOut to the box's
// end, or returns -1 if not found
static int FindIsoBmffBox(ByteReader r, int idx, int end, const char* type, int* boxEndOut) {
    while (idx + 8 <= end) {
        i64 size = (i64)r.UInt32BE(idx);
        int hdr = 8;
        if (size == 1) {
            // 64-bit size follows the type
            if (idx + 16 > end) {
                return -1;
            }
            u64 size64 = r.UInt64BE(idx + 8);
            if (size64 > (u64)(end - idx)) {
                return -1;
            }
            size = (i64)size64;
            hdr = 16;
        } else if (size == 0) {
            size = end - idx; // box extends to the end
        }
        if (size < hdr || size > end - idx) {
            return -1;
        }
        if (MemEq(r.d + idx + 4, type, 4)) {
            *boxEndOut = idx + (int)size;
            return idx + hdr;
        }
        idx += (int)size;
    }
    return -1;
}

// dimensions from the SIZ marker segment of a JPEG 2000 codestream
// starting at idx: the image grid size minus the image offset
static void Jp2SizeFromSIZ(ByteReader r, int idx, FileTypeInfo& res) {
    if (idx + 24 > r.len || r.UInt8(idx) != 0xff || r.UInt8(idx + 1) != 0x4f || r.UInt8(idx + 2) != 0xff ||
        r.UInt8(idx + 3) != 0x51) {
        return;
    }
    u32 xsiz = r.UInt32BE(idx + 8);
    u32 ysiz = r.UInt32BE(idx + 12);
    u32 xosiz = r.UInt32BE(idx + 16);
    u32 yosiz = r.UInt32BE(idx + 20);
    if (xsiz <= xosiz || ysiz <= yosiz || xsiz > (u32)INT_MAX || ysiz > (u32)INT_MAX) {
        return;
    }
    res.imageDx = (int)(xsiz - xosiz);
    res.imageDy = (int)(ysiz - yosiz);
}

// JPEG 2000: either a raw codestream (starts with the SOC marker) or a JP2
// container with dimensions in the ihdr box inside the jp2h box; if there's
// no ihdr, from the SIZ segment of the codestream in the jp2c box
static void ParseJp2(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    if (r.UInt8(0) == 0xff) {
        Jp2SizeFromSIZ(r, 0, res);
        return;
    }
    int boxEnd;
    int idx = FindIsoBmffBox(r, 0, r.len, "jp2h", &boxEnd);
    if (idx >= 0) {
        int ihdrEnd;
        int ihdr = FindIsoBmffBox(r, idx, boxEnd, "ihdr", &ihdrEnd);
        if (ihdr >= 0 && ihdr + 8 <= ihdrEnd) {
            res.imageDy = (int)r.UInt32BE(ihdr);
            res.imageDx = (int)r.UInt32BE(ihdr + 4);
            return;
        }
    }
    idx = FindIsoBmffBox(r, 0, r.len, "jp2c", &boxEnd);
    if (idx >= 0) {
        Jp2SizeFromSIZ(r, idx, res);
    }
}

// HEIF/AVIF (ISO BMFF): dimensions from the 'ispe' (spatial extents) boxes
// in meta/iprp/ipco. Several items can have an ispe (thumbnail, alpha plane,
// grid tiles); the largest one is the full image. An odd 'irot' rotation
// (90/270 degrees) swaps the displayed width/height.
static void ParseHeif(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    int boxEnd;
    int idx = FindIsoBmffBox(r, 0, r.len, "meta", &boxEnd);
    if (idx < 0) {
        return;
    }
    idx += 4; // meta is a FullBox: skip version/flags
    idx = FindIsoBmffBox(r, idx, boxEnd, "iprp", &boxEnd);
    if (idx < 0) {
        return;
    }
    idx = FindIsoBmffBox(r, idx, boxEnd, "ipco", &boxEnd);
    if (idx < 0) {
        return;
    }
    int dx = 0, dy = 0;
    bool swapDims = false;
    while (idx + 8 <= boxEnd) {
        i64 size = (i64)r.UInt32BE(idx);
        if (size < 8 || size > boxEnd - idx) {
            break;
        }
        const u8* type = r.d + idx + 4;
        if (MemEq(type, "ispe", 4) && size >= 20) {
            // version/flags, then u32 width and height
            int w = (int)r.UInt32BE(idx + 12);
            int h = (int)r.UInt32BE(idx + 16);
            if ((i64)w * h > (i64)dx * dy) {
                dx = w;
                dy = h;
            }
        } else if (MemEq(type, "irot", 4) && size >= 9) {
            // one byte: rotation in 90-degree counter-clockwise units
            swapDims = (r.UInt8(idx + 8) & 1) != 0;
        }
        idx += (int)size;
    }
    if (swapDims) {
        int tmp = dx;
        dx = dy;
        dy = tmp;
    }
    res.imageDx = dx;
    res.imageDy = dy;
}

// LSB-first bit reader for the JPEG XL codestream header
struct JxlBitReader {
    ByteReader r;
    int pos; // in bits

    JxlBitReader(ByteReader r, int startByte) : r(r), pos(startByte * 8) {}
    u32 Bits(int n) {
        u32 res = 0;
        for (int i = 0; i < n; i++) {
            u32 bit = (r.UInt8(pos >> 3) >> (pos & 7)) & 1;
            res |= bit << i;
            pos++;
        }
        return res;
    }
};

// variable-length u32: 2-bit selector, then 9/13/18/30 bits, value + 1
static u32 JxlU32(JxlBitReader& br) {
    static const int nBits[] = {9, 13, 18, 30};
    u32 sel = br.Bits(2);
    return br.Bits(nBits[sel]) + 1;
}

// dimensions from the SizeHeader and orientation from the ImageMetadata of
// a JPEG XL codestream, per ISO/IEC 18181-1 (see also jxl-rs
// jxl/src/headers/size.rs). Reads past the end of a truncated buffer as 0
// bits, which yields 0-sized dimensions.
static void JxlSizeFromCodestream(ByteReader r, FileTypeInfo& res) {
    if (r.len < 4 || r.UInt8(0) != 0xff || r.UInt8(1) != 0x0a) {
        return;
    }
    JxlBitReader br(r, 2);
    bool isSmall = br.Bits(1) != 0;
    u32 ysize;
    if (isSmall) {
        ysize = (br.Bits(5) + 1) * 8;
    } else {
        ysize = JxlU32(br);
    }
    u32 xsize;
    u32 ratio = br.Bits(3);
    if (ratio != 0) {
        // xsize is derived from ysize via a fixed aspect-ratio table
        static const u32 num[] = {0, 1, 12, 4, 3, 16, 5, 2};
        static const u32 den[] = {0, 1, 10, 3, 2, 9, 4, 1};
        xsize = (u32)(((u64)ysize * num[ratio]) / den[ratio]);
    } else if (isSmall) {
        xsize = (br.Bits(5) + 1) * 8;
    } else {
        xsize = JxlU32(br);
    }
    // ImageMetadata: orientation lives in the optional extra_fields
    int orientation = 1;
    bool allDefault = br.Bits(1) != 0;
    if (!allDefault) {
        bool extraFields = br.Bits(1) != 0;
        if (extraFields) {
            orientation = (int)br.Bits(3) + 1;
        }
    }
    if (xsize > (u32)INT_MAX || ysize > (u32)INT_MAX) {
        return;
    }
    res.imageDx = (int)xsize;
    res.imageDy = (int)ysize;
    res.orientation = orientation;
}

static void ParseJxl(ByteReader r, FileTypeInfo& res) {
    res.nImages = 1;
    if (r.UInt8(0) == 0xff && r.UInt8(1) == 0x0a) {
        JxlSizeFromCodestream(r, res);
        return;
    }
    // ISO BMFF container: the codestream is in a jxlc box or split across
    // jxlp boxes, whose payload starts with a 4-byte part index
    int boxEnd;
    int idx = FindIsoBmffBox(r, 0, r.len, "jxlc", &boxEnd);
    if (idx < 0) {
        idx = FindIsoBmffBox(r, 0, r.len, "jxlp", &boxEnd);
        if (idx < 0) {
            return;
        }
        idx += 4;
    }
    if (boxEnd <= idx) {
        return;
    }
    JxlSizeFromCodestream(ByteReader(r.d + idx, boxEnd - idx), res);
}

// Like GuessFileTypeFromData() but for image files also reports the number
// of images and, for single-image files, width/height parsed from the header
// (hasImageSize tells if they were found; if not, callers can fall back to a
// more expensive method like decoding the image). For multi-image files the
// per-image sizes are in imageSizes; the caller must FreeFileTypeInfo() the
// result. Dimensions are after applying the EXIF orientation, if any. Counts
// reflect the provided buffer: they can under-report if d is a truncated
// prefix of the file.
FileTypeInfo GuessFileInfoFromData(Str d) {
    FileTypeInfo res;
    res.ft = DetectFileTypeFromData(d);
    ByteReader r(d);
    switch (res.ft) {
        case FileType::Png:
            ParsePng(r, res);
            break;
        case FileType::Jpeg:
            ParseJpeg(r, res);
            break;
        case FileType::Gif:
            ParseGif(r, res);
            break;
        case FileType::Bmp:
            ParseBmp(r, res);
            break;
        case FileType::Ico:
            ParseIco(r, res);
            break;
        case FileType::Tiff:
            ParseTiff(r, res, false);
            break;
        case FileType::Jxr:
            ParseTiff(r, res, true);
            break;
        case FileType::Tga:
            ParseTga(r, res);
            break;
        case FileType::Jp2:
            ParseJp2(r, res);
            break;
        case FileType::Webp:
            ParseWebp(r, res);
            break;
        case FileType::Heic:
        case FileType::Avif:
            ParseHeif(r, res);
            break;
        case FileType::Jxl:
            ParseJxl(r, res);
            break;
        default:
            break;
    }
    if (res.ft == FileType::Jpeg) {
        res.orientation = JpegExifOrientation(r);
    } else if (res.ft == FileType::Webp) {
        res.orientation = WebpExifOrientation(d);
    }
    if (res.nImages != 1) {
        // imageDx/imageDy are only reported for single-image files
        res.imageDx = 0;
        res.imageDy = 0;
    }
    if (res.nImages <= 1) {
        // imageSizes is only reported for multi-image files
        free(res.imageSizes);
        res.imageSizes = nullptr;
    }
    if (ExifOrientationSwapsDimensions(res.orientation)) {
        int tmp = res.imageDx;
        res.imageDx = res.imageDy;
        res.imageDy = tmp;
        for (int i = 0; i < res.nImages && res.imageSizes; i++) {
            tmp = res.imageSizes[i].dx;
            res.imageSizes[i].dx = res.imageSizes[i].dy;
            res.imageSizes[i].dy = tmp;
        }
    }
    res.hasImageSize = res.imageDx > 0 && res.imageDy > 0;
    return res;
}

void FreeFileTypeInfo(FileTypeInfo* fti) {
    if (!fti) {
        return;
    }
    free(fti->imageSizes);
    fti->imageSizes = nullptr;
}

FileType GuessFileTypeFromData(Str d) {
    FileTypeInfo fti = GuessFileInfoFromData(d);
    FreeFileTypeInfo(&fti);
    return fti.ft;
}

// parse an embedded-PDF path of the form "c:/foo.pdf:${pdfStreamNo}"
// or "c:/foo.pdf:${pdfStreamNo}:attachname=${hexUtf8Name}" into its pieces
EmbeddedPdfName ParseEmbeddedPdfName(Str path) {
    EmbeddedPdfName res;
    if (len(path) == 0) {
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

// path::IsDirectory() is expensive on network drives so we can pass notDir=true if we know the path is not a directory
FileType GuessFileTypeFromName(Str path, bool notDir) {
    if (len(path) == 0) {
        return FileType::Unknown;
    }
    if (!notDir && path::IsDirectory(path)) {
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
    FileType::Avif,
    FileType::Ico
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
    ".ico\0"
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
// Headless test helper: compare GuessFileTypeFromName's canonical extension
// to an expected one (e.g. "sample.ai" -> ".pdf").
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
