
/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/ByteReader.h"
#include "utils/Archive.h"
#include "utils/TgaReader.h"
#include "utils/WebpReader.h"
#include "utils/GuessFileType.h"

Kind kindFilePDF = "filePDF";
Kind kindFilePS = "filePS";
Kind kindFileVbkm = "fileVbkm";
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
Kind kindFileDir = "fileDir";
Kind kindFileEpub = "fileEpub";
// TODO: introduce kindFileTealDoc?
Kind kindFileMobi = "fileMobi";
Kind kindFilePalmDoc = "filePalmDoc";
Kind kindFileHTML = "fileHTML";
Kind kindFileTxt = "fileTxt";

// http://en.wikipedia.org/wiki/.nfo
// http://en.wikipedia.org/wiki/FILE_ID.DIZ
// http://en.wikipedia.org/wiki/Read.me
// http://www.cix.co.uk/~gidds/Software/TCR.html

// TODO: should .prc be kindFilePalmDoc instead of kindFileMobi?
// .zip etc. are at the end so that .fb2.zip etc. is recognized at fb2
#define DEF_EXT_KIND(V)             \
    V(".txt\0", kindFileTxt)        \
    V(".js\0", kindFileTxt)         \
    V(".json\0", kindFileTxt)       \
    V(".xml\0", kindFileTxt)        \
    V(".log\0", kindFileTxt)        \
    V("file_id.diz\0", kindFileTxt) \
    V("read.me\0", kindFileTxt)     \
    V(".nfo\0", kindFileTxt)        \
    V(".tcr\0", kindFileTxt)        \
    V(".ps\0", kindFilePS)          \
    V(".ps.gz\0", kindFilePS)       \
    V(".eps\0", kindFilePS)         \
    V(".vbkm\0", kindFileVbkm)      \
    V(".fb2\0", kindFileFb2)        \
    V(".fb2z\0", kindFileFb2)       \
    V(".zfb2\0", kindFileFb2)       \
    V(".fb2.zip\0", kindFileFb2)    \
    V(".cbz\0", kindFileCbz)        \
    V(".cbr\0", kindFileCbr)        \
    V(".cb7\0", kindFileCb7)        \
    V(".cbt\0", kindFileCbt)        \
    V(".pdf\0", kindFilePDF)        \
    V(".xps\0", kindFileXps)        \
    V(".oxps\0", kindFileXps)       \
    V(".chm\0", kindFileChm)        \
    V(".png\0", kindFilePng)        \
    V(".jpg\0", kindFileJpeg)       \
    V(".jpeg\0", kindFileJpeg)      \
    V(".gif\0", kindFileGif)        \
    V(".tif\0", kindFileTiff)       \
    V(".tiff\0", kindFileTiff)      \
    V(".bmp\0", kindFileBmp)        \
    V(".tga\0", kindFileTga)        \
    V(".jxr\0", kindFileJxr)        \
    V(".hdp\0", kindFileHdp)        \
    V(".wdp\0", kindFileWdp)        \
    V(".webp\0", kindFileWebp)      \
    V(".epub\0", kindFileEpub)      \
    V(".mobi\0", kindFileMobi)      \
    V(".prc\0", kindFileMobi)       \
    V(".azw\0", kindFileMobi)       \
    V(".azw1\0", kindFileMobi)      \
    V(".azw3\0", kindFileMobi)      \
    V(".pdb\0", kindFilePalmDoc)    \
    V(".html\0", kindFileHTML)      \
    V(".htm\0", kindFileHTML)       \
    V(".xhtml\0", kindFileHTML)     \
    V(".djvu\0", kindFileDjVu)      \
    V(".jp2\0", kindFileJp2)        \
    V(".zip\0", kindFileZip)        \
    V(".rar\0", kindFileRar)        \
    V(".7z\0", kindFile7Z)          \
    V(".tar\0", kindFileTar)

#define EXT(ext, kind) ext

// .fb2.zip etc. must be first so that it isn't classified as .zip
static const char* gFileExts = DEF_EXT_KIND(EXT) "\0";
#undef EXT

#define KIND(ext, kind) kind,
static Kind gExtsKind[] = {DEF_EXT_KIND(KIND)};
#undef KIND

static Kind GetKindByFileExt(const WCHAR* path) {
    AutoFree pathA = strconv::WstrToUtf8(path);
    int idx = 0;
    const char* curr = gFileExts;
    while (curr && *curr) {
        if (str::EndsWithI(pathA.Get(), curr)) {
            int n = (int)dimof(gExtsKind);
            CrashIf(idx >= n);
            if (idx >= n) {
                return nullptr;
            }
            return gExtsKind[idx];
        }
        curr = seqstrings::SkipStr(curr);
        idx++;
    }
    return nullptr;
}

// ensure gFileExts and gExtsKind match
static bool gDidVerifyExtsMatch = false;
static void VerifyExtsMatch() {
    if (gDidVerifyExtsMatch) {
        return;
    }
    CrashAlwaysIf(kindFileEpub != GetKindByFileExt(L"foo.epub"));
    CrashAlwaysIf(kindFileJp2 != GetKindByFileExt(L"foo.JP2"));
    gDidVerifyExtsMatch = true;
}

bool KindInArray(Kind* kinds, int nKinds, Kind kind) {
    for (int i = 0; i < nKinds; i++) {
        Kind k = kinds[i];
        if (k == kind) {
            return true;
        }
    }
    return false;
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
    const char* sig;
    size_t sigLen;
    Kind kind;
};

#define MK_SIG(OFF, SIG, KIND) {OFF, SIG, sizeof(SIG) - 1, KIND},
static FileSig gFileSigs[] = {FILE_SIGS(MK_SIG)};
#undef MK_SIG

// PDF files have %PDF-${ver} somewhere in the beginning of the file
static bool IsPdfFileContent(std::span<u8> d) {
    if (d.size() < 8) {
        return false;
    }
    int n = (int)d.size() - 5;
    char* data = (char*)d.data();
    char* end = data + n;
    while (data < end) {
        size_t nLeft = end - data;
        data = (char*)std::memchr(data, '%', nLeft);
        if (!data) {
            return false;
        }
        if (str::EqN(data, "%PDF-", 5)) {
            return true;
        }
        ++data;
    }
    return false;
}

static bool IsPSFileContent(std::span<u8> d) {
    char* header = (char*)d.data();
    size_t n = d.size();
    if (n < 64) {
        return false;
    }
    // Windows-format EPS file - cf. http://partners.adobe.com/public/developer/en/ps/5002.EPSF_Spec.pdf
    if (str::StartsWith(header, "\xC5\xD0\xD3\xC6")) {
        DWORD psStart = ByteReader(d).DWordLE(4);
        return psStart >= n - 12 || str::StartsWith(header + psStart, "%!PS-Adobe-");
    }
    if (str::StartsWith(header, "%!PS-Adobe-")) {
        return true;
    }
    // PJL (Printer Job Language) files containing Postscript data
    // https://developers.hp.com/system/files/PJL_Technical_Reference_Manual.pdf
    bool isPJL = str::StartsWith(header, "\x1B%-12345X@PJL");
    if (isPJL) {
        // TODO: use something else other than str::Find() so that it works even if header is not null-terminated
        const char* hdr = str::Find(header, "%!PS-Adobe-");
        if (!hdr) {
            isPJL = false;
        }
    }
    return isPJL;
}

// detect file type based on file content
// we don't support sniffing kindFileVbkm
Kind GuessFileTypeFromContent(std::span<u8> d) {
    // TODO: sniff .fb2 content
    u8* data = d.data();
    size_t len = d.size();
    int n = (int)dimof(gFileSigs);

    for (int i = 0; i < n; i++) {
        const char* sig = gFileSigs[i].sig;
        size_t off = gFileSigs[i].offset;
        size_t sigLen = gFileSigs[i].sigLen;
        size_t sigMaxLen = off + sigLen;
        u8* dat = data + off;
        if ((len > sigMaxLen) && memeq(dat, sig, sigLen)) {
            return gFileSigs[i].kind;
        }
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
    return nullptr;
}

static bool IsEpubFile(const WCHAR* path) {
    AutoDelete<MultiFormatArchive> archive = OpenZipArchive(path, true);
    if (!archive.Get()) {
        return false;
    }
    AutoFree mimetype(archive->GetFileDataByName("mimetype"));
    if (!mimetype.data) {
        return false;
    }
    char* d = mimetype.data;
    // trailing whitespace is allowed for the mimetype file
    for (size_t i = mimetype.size(); i > 0; i--) {
        if (!str::IsWs(d[i - 1])) {
            break;
        }
        d[i - 1] = '\0';
    }
    // a proper EPUB document has a "mimetype" file with content
    // "application/epub+zip" as the first entry in its ZIP structure
    /* cf. http://forums.fofou.org/sumatrapdf/topic?id=2599331
    if (!str::Eq(zip.GetFileName(0), L"mimetype"))
        return false; */
    if (str::Eq(mimetype.data, "application/epub+zip")) {
        return true;
    }
    // also open renamed .ibooks files
    // cf. http://en.wikipedia.org/wiki/IBooks#Formats
    return str::Eq(mimetype.data, "application/x-ibooks+zip");
}

// check if a given file is a likely a .zip archive containing XPS
// document
static bool IsXpsArchive(const WCHAR* path) {
    MultiFormatArchive* archive = OpenZipArchive(path, true);
    if (!archive) {
        return false;
    }

    bool res = archive->GetFileId("_rels/.rels") != (size_t)-1 ||
               archive->GetFileId("_rels/.rels/[0].piece") != (size_t)-1 ||
               archive->GetFileId("_rels/.rels/[0].last.piece") != (size_t)-1;
    delete archive;
    return res;
}

// detect file type based on file content
Kind GuessFileTypeFromContent(const WCHAR* path) {
    CrashIf(!path);

    if (path::IsDirectory(path)) {
        AutoFreeWstr mimetypePath(path::Join(path, L"mimetype"));
        if (file::StartsWith(mimetypePath, "application/epub+zip")) {
            return kindFileEpub;
        }
        // TODO: check the content of directory for more types?
        return nullptr;
    }

    // +1 for zero-termination
    char buf[2048 + 1] = {0};
    int n = file::ReadN(path, buf, dimof(buf) - 1);
    if (n <= 0) {
        return nullptr;
    }
    auto res = GuessFileTypeFromContent({(u8*)buf, (size_t)n});
    if (res == kindFileZip) {
        if (IsXpsArchive(path)) {
            res = kindFileXps;
        }
        if (IsEpubFile(path)) {
            res = kindFileEpub;
        }
    }
    return res;
}

// embedded PDF files have names like "c:/foo.pdf:${pdfStreamNo}"
// return pointer starting at ":${pdfStream}"
const WCHAR* FindEmbeddedPdfFileStreamNo(const WCHAR* path) {
    const WCHAR* start = path;
    const WCHAR* end = start + str::Len(start) - 1;

    int nDigits = 0;
    while (end > start) {
        WCHAR c = *end;
        if (c == ':') {
            if (nDigits > 0) {
                return end;
            }
            // it was just ':' at the end
            return nullptr;
        }
        if (!str::IsDigit(c)) {
            return nullptr;
        }
        nDigits++;
        end--;
    }
    return nullptr;
}

Kind GuessFileTypeFromName(const WCHAR* path) {
    VerifyExtsMatch();

    if (!path) {
        return nullptr;
    }
    if (path::IsDirectory(path)) {
        return kindFileDir;
    }
    Kind res = GetKindByFileExt(path);
    if (res != nullptr) {
        return res;
    }

    // cases that cannot be decided just by looking at file extension
    if (FindEmbeddedPdfFileStreamNo(path) != nullptr) {
        return kindFilePDF;
    }

    return nullptr;
}

Kind GuessFileType(const WCHAR* path, bool sniff) {
    if (sniff) {
        Kind kind = GuessFileTypeFromContent(path);
        if (kind) {
            return kind;
        }
        // for some file types we don't have sniffing so fall back to
        // guess from file name
        return GuessFileTypeFromName(path);
    }
    return GuessFileTypeFromName(path);
}
