
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/GdiplusUtil.h"
#include "utils/FileTypeSniff.h"

// TODO: move those functions here
extern bool IsPdfFileName(const WCHAR* path);
extern bool IsPdfFileContent(std::span<u8> d);
extern bool IsPSFileContent(std::span<u8> d);
extern bool IsEngineMultiFileName(const WCHAR* path);
extern bool IsXpsArchive(const WCHAR* path);
extern bool IsDjVuFileName(const WCHAR* path);
extern bool IsImageEngineSupportedFile(const WCHAR* fileName, bool sniff);
extern bool IsEpubFile(const WCHAR* path);

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

// .fb2.zip etc. must be first so that it isn't classified as .zip
static const char* gFileExts =
    ".fb2.zip\0"
    ".ps.gz\0"
    ".ps\0"
    ".eps\0"
    ".vbkm\0"
    ".fb2\0"
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
    ".jp2\0"
    "\0";

static Kind gExtsKind[] = {
    kindFileFb2, kindFilePS,  kindFilePS,  kindFilePS,   kindFileVbkm, kindFileFb2,  kindFileCbz,  kindFileCbr,
    kindFileCb7, kindFileCbt, kindFileZip, kindFileRar,  kindFile7Z,   kindFileTar,  kindFilePDF,  kindFileXps,
    kindFileXps, kindFileChm, kindFilePng, kindFileJpeg, kindFileJpeg, kindFileGif,  kindFileTiff, kindFileTiff,
    kindFileBmp, kindFileTga, kindFileJxr, kindFileHdp,  kindFileWdp,  kindFileWebp, kindFileEpub, kindFileJp2,
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

static bool gDidCheckExtsMatch = false;
static void CheckExtsMatch() {
    if (!gDidCheckExtsMatch) {
        CrashAlwaysIf(kindFileJp2 != GetKindByFileExt(L"foo.JP2"));
        gDidCheckExtsMatch = true;
    }
}

static Kind imageEngineKinds[] = {
    kindFilePng, kindFileJpeg, kindFileGif, kindFileTiff, kindFileBmp, kindFileTga,
    kindFileJxr, kindFileHdp,  kindFileWdp, kindFileWebp, kindFileJp2,
};

static bool KindInArray(Kind* kinds, int nKinds, Kind kind) {
    for (int i = 0; i < nKinds; i++) {
        Kind k = kinds[i];
        if (k == kind) {
            return true;
        }
    }
    return false;
}

bool IsImageEngineKind(Kind kind) {
    int n = dimof(imageEngineKinds);
    return KindInArray(imageEngineKinds, n, kind);
}

static Kind cbxKinds[] = {
    kindFileCbz, kindFileCbr, kindFileCb7, kindFileCbt, kindFileZip, kindFileRar, kindFile7Z, kindFileTar,
};

bool IsCbxEngineKind(Kind kind) {
    int n = dimof(cbxKinds);
    return KindInArray(cbxKinds, n, kind);
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

// detect file type based on file content
// we don't support sniffing kindFileVbkm
Kind SniffFileTypeFromData(std::span<u8> d) {
    if (IsPdfFileContent(d)) {
        return kindFilePDF;
    }
    if (IsPSFileContent(d)) {
        return kindFilePS;
    }
    u8* data = d.data();
    size_t len = d.size();
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
    int n = (int)dimof(gFileSigs);
    for (int i = 0; i < n; i++) {
        const char* sig = gFileSigs[i].sig;
        size_t sigLen = gFileSigs[i].sigLen;
        if (memeq(data, sig, sigLen)) {
            return gFileSigs[i].kind;
        }
    }
    return nullptr;
}

// detect file type based on file content
Kind SniffFileType(const WCHAR* path) {
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
    auto res = SniffFileTypeFromData({(u8*)buf, (size_t)n});
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

Kind FileTypeFromFileName(const WCHAR* path) {
    CheckExtsMatch();

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

    // those are cases that cannot be decided just by
    // looking at extension
    if (IsPdfFileName(path)) {
        return kindFilePDF;
    }

    return nullptr;
}
