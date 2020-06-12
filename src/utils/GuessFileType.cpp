
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/GdiplusUtil.h"
#include "utils/ByteReader.h"
#include "utils/PalmDbReader.h"
#include "utils/Archive.h"
#include "utils/GuessFileType.h"

// TODO: replace with an enum class FileKind { Unknown, PDF, ... };
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
Kind kindFileMobi = "fileMobi";
Kind kindFilePalmDoc = "filePalmDoc";
Kind kindFileHTML = "fileHTML";
Kind kindFileTxt = "fileTxt";

// http://en.wikipedia.org/wiki/.nfo
// http://en.wikipedia.org/wiki/FILE_ID.DIZ
// http://en.wikipedia.org/wiki/Read.me
// http://www.cix.co.uk/~gidds/Software/TCR.html

// .fb2.zip etc. must be first so that it isn't classified as .zip
static const char* gFileExts =
    ".fb2.zip\0"
    ".ps.gz\0"
    "file_id.diz\0"
    "Read.me\0"
    ".txt\0"
    ".log\0"
    ".nfo\0"
    ".tcr\0"
    ".ps\0"
    ".eps\0"
    ".vbkm\0"
    ".fb2\0"
    ".fb2z\0"
    ".zfb2\0"
    ".cbz\0"
    ".cbr\0"
    ".cb7\0"
    ".cbt\0"
    ".zip\0"
    ".rar\0"
    ".7z\0"
    ".tar\0"
    ".pdf\0"
    ".xps\0"
    ".oxps\0"
    ".chm\0"
    ".png\0"
    ".jpg\0"
    ".jpeg\0"
    ".gif\0"
    ".tif\0"
    ".tiff\0"
    ".bmp\0"
    ".tga\0"
    ".jxr\0"
    ".hdp\0"
    ".wdp\0"
    ".webp\0"
    ".epub\0"
    ".mobi\0"
    ".prc\0"
    ".azw\0"
    ".azw1\0"
    ".azw3\0"
    ".pdb\0"
    ".prc\0"
    ".html\0"
    ".htm\0"
    ".xhtml\0"
    ".djvu\0"
    ".jp2\0"
    "\0";

static Kind gExtsKind[] = {
    kindFileFb2,  kindFilePS,   kindFileTxt,     kindFileTxt,     kindFileTxt,  kindFileTxt,  kindFileTxt,
    kindFileTxt,  kindFilePS,   kindFilePS,      kindFileVbkm,    kindFileFb2,  kindFileFb2,  kindFileFb2,
    kindFileCbz,  kindFileCbr,  kindFileCb7,     kindFileCbt,     kindFileZip,  kindFileRar,  kindFile7Z,
    kindFileTar,  kindFilePDF,  kindFileXps,     kindFileXps,     kindFileChm,  kindFilePng,  kindFileJpeg,
    kindFileJpeg, kindFileGif,  kindFileTiff,    kindFileTiff,    kindFileBmp,  kindFileTga,  kindFileJxr,
    kindFileHdp,  kindFileWdp,  kindFileWebp,    kindFileEpub,    kindFileMobi, kindFileMobi, kindFileMobi,
    kindFileMobi, kindFileMobi, kindFilePalmDoc, kindFilePalmDoc, kindFileHTML, kindFileHTML, kindFileHTML,
    kindFileDjVu, kindFileJp2,
};

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

#define FILE_SIGS(V)                       \
    V("Rar!\x1A\x07\x00", kindFileRar)     \
    V("Rar!\x1A\x07\x01\x00", kindFileRar) \
    V("7z\xBC\xAF\x27\x1C", kindFile7Z)    \
    V("PK\x03\x04", kindFileZip)           \
    V("ITSF", kindFileChm)                 \
    V("AT&T", kindFileDjVu)

struct FileSig {
    const char* sig;
    size_t sigLen;
    Kind kind;
};

#define MK_SIG(SIG, KIND) {SIG, sizeof(SIG) - 1, KIND},

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
        const char* hdr = str::Find(header, "\n%!PS-Adobe-");
        if (!hdr) {
            isPJL = false;
        }
    }
    return isPJL;
}

static Kind PalmOrMobiType(std::span<u8> d) {
    if (d.size() < sizeof(PdbHeader)) {
        return nullptr;
    }
    size_t off = 0x3c;
    char* s = (char*)d.data() + off;
    if (memeq(s, MOBI_TYPE_CREATOR, 8)) {
        return kindFileMobi;
    }
    // TODO: introduce kindFileTealDoc?
    if (memeq(s, PALMDOC_TYPE_CREATOR, 8)) {
        return kindFilePalmDoc;
    }
    if (memeq(s, TEALDOC_TYPE_CREATOR, 8)) {
        return kindFilePalmDoc;
    }
    return nullptr;
}

// detect file type based on file content
// we don't support sniffing kindFileVbkm
static Kind GuessFileTypeFromContent(std::span<u8> d) {
    // TODO: sniff .fb2 content
    u8* data = d.data();
    size_t len = d.size();
    int n = (int)dimof(gFileSigs);

    for (int i = 0; i < n; i++) {
        const char* sig = gFileSigs[i].sig;
        size_t sigLen = gFileSigs[i].sigLen;
        if (memeq(data, sig, sigLen)) {
            return gFileSigs[i].kind;
        }
    }

    if (IsPdfFileContent(d)) {
        return kindFilePDF;
    }
    if (IsPSFileContent(d)) {
        return kindFilePS;
    }
    Kind kind = PalmOrMobiType(d);
    if (kind != nullptr) {
        return kind;
    }

    ImgFormat fmt = GfxFormatFromData(d);
    switch (fmt) {
        case ImgFormat::BMP:
            return kindFileBmp;
        case ImgFormat::GIF:
            return kindFileGif;
        case ImgFormat::JPEG:
            return kindFileJpeg;
        case ImgFormat::JXR:
            return kindFileJxr;
        case ImgFormat::PNG:
            return kindFilePng;
        case ImgFormat::TGA:
            return kindFileTga;
        case ImgFormat::TIFF:
            return kindFileTiff;
        case ImgFormat::WebP:
            return kindFileWebp;
        case ImgFormat::JP2:
            return kindFileJp2;
    }
    return nullptr;
}

static bool IsEpubFile(const WCHAR* path) {
    AutoDelete<MultiFormatArchive> archive = OpenZipArchive(path, true);
    if (!archive.get()) {
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
