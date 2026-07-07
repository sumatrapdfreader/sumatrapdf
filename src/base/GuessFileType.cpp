
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

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
FileType GuessFileTypeFromContent(Str d) {
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
    FileType ft = GuessFileTypeFromContent(d);
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
