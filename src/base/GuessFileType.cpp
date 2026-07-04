
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/ByteReader.h"
#include "base/Archive.h"
#include "base/TgaReader.h"
#include "base/WebpReader.h"
#include "base/JxlReader.h"
#include "base/GuessFileType.h"

Kind kindFilePDF = "filePDF";
Kind kindFilePS = "filePS";
Kind kindFileXps = "fileXPS";
Kind kindFileDjVu = "fileDjVu";
Kind kindFileChm = "fileChm";
Kind kindFilePng = "filePng";
Kind kindFileJpeg = "fileJpeg";
Kind kindFileGif = "fileGif";
Kind kindFileTiff = "fileTiff";
Kind kindFileBmp = "fileBmp";
Kind kindFileTga = "fileTga";
Kind kindFileJxr = "fileJxr";
Kind kindFileHdp = "fileHdp";
Kind kindFileWdp = "fileWdp";
Kind kindFileWebp = "fileWebp";
Kind kindFileJxl = "fileJxl";
Kind kindFileJp2 = "fileJp2";
Kind kindFileCbz = "fileCbz";
Kind kindFileCbr = "fileCbr";
Kind kindFileCb7 = "fileCb7";
Kind kindFileCbt = "fileCbt";
Kind kindFileZip = "fileZip";
Kind kindFileRar = "fileRar";
Kind kindFile7Z = "file7Z";
Kind kindFileTar = "fileTar";
Kind kindFileFb2 = "fileFb2";
Kind kindFileFb2z = "fileFb2z"; // fb2 but inside .zip file
Kind kindDirectory = "directory";
Kind kindFileEpub = "fileEpub";
Kind kindFileMarkdown = "fileMarkdown";
// TODO: introduce kindFileTealDoc?
Kind kindFileMobi = "fileMobi";
Kind kindFilePalmDoc = "filePalmDoc";
Kind kindFileHTML = "fileHTML";
Kind kindFileTxt = "fileTxt";
Kind kindFileSvg = "fileSvg";
Kind kindFileHeic = "fileHeic";
Kind kindFileAvif = "fileAvif";

// http://en.wikipedia.org/wiki/.nfo
// http://en.wikipedia.org/wiki/FILE_ID.DIZ
// http://en.wikipedia.org/wiki/Read.me
// http://www.cix.co.uk/~gidds/Software/TCR.html

// TODO: should .prc be kindFilePalmDoc instead of kindFileMobi?
// .zip etc. are at the end so that .fb2.zip etc. is recognized at fb2
#define DEF_EXT_KIND(V)              \
    V(".txt", kindFileTxt)           \
    V(".js", kindFileTxt)            \
    V(".json", kindFileTxt)          \
    V(".xml", kindFileTxt)           \
    V(".log", kindFileTxt)           \
    V("file_id.diz", kindFileTxt)    \
    V("read.me", kindFileTxt)        \
    V(".nfo", kindFileTxt)           \
    V(".tcr", kindFileTxt)           \
    V(".ps", kindFilePS)             \
    V(".ps.gz", kindFilePS)          \
    V(".eps", kindFilePS)            \
    V(".fb2", kindFileFb2)           \
    V(".fb2z", kindFileFb2z)         \
    V(".fbz", kindFileFb2z)          \
    V(".zfb2", kindFileFb2z)         \
    V(".fb2.zip", kindFileFb2z)      \
    V(".cbz", kindFileCbz)           \
    V(".ora", kindFileCbz)           \
    V(".cbr", kindFileCbr)           \
    V(".cb7", kindFileCb7)           \
    V(".cbt", kindFileCbt)           \
    V(".pdf", kindFilePDF)           \
    V(".ai", kindFilePDF)            \
    V(".xps", kindFileXps)           \
    V(".oxps", kindFileXps)          \
    V(".xod", kindFileXps)           \
    V(".dwfx", kindFileXps)          \
    V(".chm", kindFileChm)           \
    V(".png", kindFilePng)           \
    V(".jpg", kindFileJpeg)          \
    V(".jpeg", kindFileJpeg)         \
    V(".jfif", kindFileJpeg)         \
    V(".gif", kindFileGif)           \
    V(".tif", kindFileTiff)          \
    V(".tiff", kindFileTiff)         \
    V(".bmp", kindFileBmp)           \
    V(".tga", kindFileTga)           \
    V(".jxr", kindFileJxr)           \
    V(".hdp", kindFileHdp)           \
    V(".wdp", kindFileWdp)           \
    V(".webp", kindFileWebp)         \
    V(".jxl", kindFileJxl)           \
    V(".epub", kindFileEpub)         \
    V(".md", kindFileMarkdown)       \
    V(".markdown", kindFileMarkdown) \
    V(".mobi", kindFileMobi)         \
    V(".prc", kindFileMobi)          \
    V(".azw", kindFileMobi)          \
    V(".azw1", kindFileMobi)         \
    V(".azw3", kindFileMobi)         \
    V(".azw4", kindFileMobi)         \
    V(".pdb", kindFilePalmDoc)       \
    V(".html", kindFileHTML)         \
    V(".htm", kindFileHTML)          \
    V(".xhtml", kindFileHTML)        \
    V(".svg", kindFileSvg)           \
    V(".djvu", kindFileDjVu)         \
    V(".djv", kindFileDjVu)          \
    V(".jp2", kindFileJp2)           \
    V(".j2k", kindFileJp2)           \
    V(".jpx", kindFileJp2)           \
    V(".jpf", kindFileJp2)           \
    V(".jpm", kindFileJp2)           \
    V(".j2c", kindFileJp2)           \
    V(".zip", kindFileZip)           \
    V(".rar", kindFileRar)           \
    V(".7z", kindFile7Z)             \
    V(".heic", kindFileHeic)         \
    V(".heif", kindFileHeic)         \
    V(".avif", kindFileAvif)         \
    V(".tar", kindFileTar)

#define EXT(ext, kind) ext "\0"

// .fb2.zip etc. must be first so that it isn't classified as .zip
static const char* gFileExts = DEF_EXT_KIND(EXT);
#undef EXT

#define KIND(ext, kind) kind,
static Kind gExtsKind[] = {DEF_EXT_KIND(KIND)};
#undef KIND

static Kind GetKindByFileExt(Str path) {
    TempStr ext = path::GetExtTemp(path);
    int idx = SeqStrIndexIS(gFileExts, ext);
    if (idx < 0) {
        return nullptr;
    }
    int n = (int)dimof(gExtsKind);
    if (idx >= n) {
        return nullptr;
    }
    return gExtsKind[idx];
}

TempStr GetExtForKindTemp(Kind kind) {
    int idx = KindIndexOf(gExtsKind, dimofi(gExtsKind), kind);
    if (idx >= 0) {
        return SeqStrByIndex(gFileExts, idx);
    }
    return {};
}

// ensure gFileExts and gExtsKind match
static bool gDidVerifyExtsMatch = false;
static void VerifyExtsMatch() {
    if (gDidVerifyExtsMatch) {
        return;
    }
    ReportIf(kindFileEpub != GetKindByFileExt("foo.epub"));
    ReportIf(kindFileJp2 != GetKindByFileExt("foo.JP2"));
    gDidVerifyExtsMatch = true;
}

int KindIndexOf(Kind* kinds, int nKinds, Kind kind) {
    for (int i = 0; i < nKinds; i++) {
        Kind k = kinds[i];
        if (k == kind) {
            return i;
        }
    }
    return -1;
}

#define FILE_SIGS(V)                                    \
    V(0, "Rar!\x1A\x07\x00", kindFileRar)               \
    V(0, "Rar!\x1A\x07\x01\x00", kindFileRar)           \
    V(0, "7z\xBC\xAF\x27\x1C", kindFile7Z)              \
    V(0, "PK\x03\x04", kindFileZip)                     \
    V(0, "ITSF", kindFileChm)                           \
    V(0x3c, "BOOKMOBI", kindFileMobi)                   \
    V(0x3c, "TEXtREAd", kindFilePalmDoc)                \
    V(0x3c, "TEXtTlDc", kindFilePalmDoc)                \
    V(0x3c, "DataPlkr", kindFilePalmDoc)                \
    V(0, "\x89PNG\x0D\x0A\x1A\x0A", kindFilePng)        \
    V(0, "\xFF\xD8", kindFileJpeg)                      \
    V(0, "GIF87a", kindFileGif)                         \
    V(0, "GIF89a", kindFileGif)                         \
    V(0, "BM", kindFileBmp)                             \
    V(0, "MM\x00\x2A", kindFileTiff)                    \
    V(0, "II\x2A\x00", kindFileTiff)                    \
    V(0, "II\xBC\x01", kindFileJxr)                     \
    V(0, "II\xBC\x00", kindFileJxr)                     \
    V(0, "\0\0\0\x0CjP  \x0D\x0A\x87\x0A", kindFileJp2) \
    V(0, "AT&T", kindFileDjVu)

// a file signaure is a sequence of bytes at a specific
// offset in the file
struct FileSig {
    size_t offset;
    Str sig;
    size_t sigLen;
    Kind kind;
};

#define MK_SIG(OFF, SIG, KIND) {OFF, SIG, sizeof(SIG) - 1, KIND},
static FileSig gFileSigs[] = {FILE_SIGS(MK_SIG)};
#undef MK_SIG

// PDF files have %PDF-${ver} somewhere in the beginning of the file
static bool IsPdfFileContent(Str d) {
    if ((size_t)d.len < 8) {
        return false;
    }
    Str data = Str((char*)((u8*)d.s), (int)((size_t)d.len - 5));
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
    size_t n = (size_t)d.len;
    if (n < 64) {
        return false;
    }
    // Windows-format EPS file - cf. http://partners.adobe.com/public/developer/en/ps/5002.EPSF_Spec.pdf
    if (str::StartsWith(header, "\xC5\xD0\xD3\xC6")) {
        DWORD psStart = ByteReader(d).DWordLE(4);
        if (psStart >= n - 12) {
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
static Kind DetectHicAndAvif(Str d) {
    if ((size_t)d.len < 0x18) {
        return nullptr;
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
        return kindFileHeic;
    }
    if (str::StartsWith(hdr, "ftypheix")) {
        return kindFileHeic;
    }
    if (str::StartsWith(hdr, "ftypmif1")) {
        return kindFileHeic;
    }
    if (str::StartsWith(hdr, "ftypavif")) {
        return kindFileAvif;
    }
    hdr = Str(s.s + 16, s.len - 16);
    if (str::StartsWith(hdr, "mif1heic")) {
        return kindFileHeic;
    }
    return nullptr;
}

// detect file type based on file content
Kind GuessFileTypeFromContent(Str d) {
    // TODO: sniff .fb2 content
    u8* data = (u8*)d.s;
    size_t dataLen = (size_t)d.len;
    int n = (int)dimof(gFileSigs);

    for (int i = 0; i < n; i++) {
        Str sig = gFileSigs[i].sig;
        size_t off = gFileSigs[i].offset;
        size_t sigLen = gFileSigs[i].sigLen;
        size_t sigMaxLen = off + sigLen;
        u8* dat = data + off;
        if ((dataLen > sigMaxLen) && memeq(dat, sig.s, (int)sigLen)) {
            return gFileSigs[i].kind;
        }
    }
    Kind kind = DetectHicAndAvif(d);
    if (kind) {
        return kind;
    }

    if (IsPdfFileContent(d)) {
        return kindFilePDF;
    }
    if (IsPSFileContent(d)) {
        return kindFilePS;
    }
    if (tga::HasSignature(d)) {
        return kindFileTga;
    }
    if (webp::HasSignature(d)) {
        return kindFileWebp;
    }
    if (jxl::HasSignature(d)) {
        return kindFileJxl;
    }
    return nullptr;
}

static bool IsEpubArchive(MultiFormatArchive* archive) {
    // assume that if this file exists, this is a epub file
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1801
    auto* container = archive->GetFileDataByName("META-INF/container.xml");
    if (container && container->data) {
        return true;
    }

    auto* mimeType = archive->GetFileDataByName("mimetype");
    if (!mimeType || !mimeType->data) {
        return false;
    }

    char* mt = mimeType->data;
    // trailing whitespace is allowed for the mimetype file
    size_t n = mimeType->fileSizeUncompressed;
    for (size_t i = n; i > 0; i--) {
        if (!str::IsWs(mt[i - 1])) {
            n = i;
            break;
        }
        mt[i - 1] = '\0';
        if (i == 1) {
            n = 0;
        }
    }

#if 0
    // a proper EPUB document has a "mimetype" file with content
    // "application/epub+zip" as the first entry in its ZIP structure
    // https://web.archive.org/web/20140201013228/http://forums.fofou.org:80/sumatrapdf/topic?id=2599331&comments=6
    if (!str::Eq(archive.GetFileName(0), L"mimetype")) {
        return false; 
    }
#endif
    Str mtStr = Str(mt, (int)n);
    if (str::Eq(mtStr, "application/epub+zip")) {
        return true;
    }
    // also open renamed .ibooks files
    // http://en.wikipedia.org/wiki/IBooks#Formats
    return str::Eq(mtStr, "application/x-ibooks+zip");
}

// check if a given file is a likely a .zip archive containing XPS
// document
static bool IsXpsArchive(MultiFormatArchive* archive) {
    bool res = archive->GetFileId("_rels/.rels") != (size_t)-1 ||
               archive->GetFileId("_rels/.rels/[0].piece") != (size_t)-1 ||
               archive->GetFileId("_rels/.rels/[0].last.piece") != (size_t)-1;
    return res;
}

// we expect 1 file ending with .fb2
static bool IsFb2Archive(MultiFormatArchive* archive) {
    auto files = archive->GetFileInfos();
    if (len(files) != 1) {
        return false;
    }
    auto fi = files[0];
    auto name = fi->name;
    return str::EndsWithI(name, ".fb2");
}

// detect file type based on file content
Kind GuessFileTypeFromFile(Str path) {
    ReportIf(!path);
    if (path::IsDirectory(path)) {
        TempStr mimetypePath = path::JoinTemp(path, StrL("mimetype"));
        if (file::StartsWith(mimetypePath, "application/epub+zip")) {
            return kindFileEpub;
        }
        // TODO: check the content of directory for more types?
        return nullptr;
    }

    // +1 for zero-termination
    char buf[2048 + 1]{};
    int n = file::ReadN(path, (u8*)buf, dimof(buf) - 1);
    if (n <= 0) {
        return nullptr;
    }

    Str d = Str((char*)(buf), (int)(n));
    auto res = GuessFileTypeFromContent(d);
    if (res == kindFileZip) {
        ArchiveExtractProgressCb emptyCb;
        MultiFormatArchive* archive = OpenArchiveFromFile(path, /*eagerLoad=*/false, emptyCb);
        if (archive) {
            if (IsXpsArchive(archive)) {
                res = kindFileXps;
            }
            if (IsEpubArchive(archive)) {
                res = kindFileEpub;
            }
            if (IsFb2Archive(archive)) {
                res = kindFileFb2z;
            }
            delete archive;
        }
    }
    return res;
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
            char* name = AllocArrayTemp<char>((size_t)nameLen + 1);
            if (str::HexToMem(hex, Str(name, nameLen))) {
                name[nameLen] = 0;
                res.fileName = Str(name, nameLen);
            }
        }
    }
    return res;
}

Kind GuessFileTypeFromName(Str path) {
    VerifyExtsMatch();

    if (!path) {
        return nullptr;
    }
    if (path::IsDirectory(path)) {
        return kindDirectory;
    }
    Kind res = GetKindByFileExt(path);
    if (res != nullptr) {
        return res;
    }

    // cases that cannot be decided just by looking at file extension
    if (ParseEmbeddedPdfName(path).streamNoStr) {
        return kindFilePDF;
    }

    return nullptr;
}

Kind GuessFileType(Str path, bool sniff) {
    if (sniff) {
        Kind kind = GuessFileTypeFromFile(path);
        if (kind) {
            return kind;
        }
        // for some file types we don't have sniffing so fall back to
        // guess from file name
        return GuessFileTypeFromName(path);
    }
    return GuessFileTypeFromName(path);
}

// clang-format off
static const Kind gImageKinds[] = {
    kindFilePng,
    kindFileJpeg,
    kindFileJpeg,
    kindFileGif,
    kindFileBmp,
    kindFileTiff,
    kindFileTiff,
    kindFileTga,
    kindFileJxr,
    kindFileWebp,
    kindFileJp2,
    kindFileHeic,
    kindFileAvif
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

static int FindImageKindIdx(Kind kind) {
    int n = (int)dimof(gImageKinds);
    for (int i = 0; i < n; i++) {
        if (kind == gImageKinds[i]) {
            return i;
        }
    }
    return -1;
}

TempStr GfxFileExtFromKindTemp(Kind kind) {
    int idx = FindImageKindIdx(kind);
    if (idx >= 0) {
        return SeqStrByIndex(gImageFormatExts, idx);
    }
    return {};
}

TempStr GfxFileExtFromDataTemp(Str d) {
    Kind kind = GuessFileTypeFromContent(d);
    return GfxFileExtFromKindTemp(kind);
}

TempStr FileKindResultTemp(Str path, Str expectedKindName, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (len(path) == 0 || len(expectedKindName) == 0) {
        return fail(StrL("ERROR missing path or expectedKind"));
    }
    Kind kind = GuessFileTypeFromName(path);
    if (!kind) {
        return fail(StrL("ERROR unknown-kind"));
    }
    if (!str::Eq(kind, expectedKindName)) {
        out.Append(fmt("FAIL path=%s got=%s expected=%s\n", path, Str(kind), expectedKindName));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }
    out.Append(fmt("OK path=%s kind=%s\n", path, Str(kind)));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}
