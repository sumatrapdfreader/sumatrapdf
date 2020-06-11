
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/GdiplusUtil.h"
#include "utils/FileTypeSniff.h"

// TODO: move those functions here
extern bool IsPdfFileName(const WCHAR* path);
extern bool IfPdfFileContent(std::span<u8> d);
extern bool IsEngineMultiFileName(const WCHAR* path);
extern bool IsXpsFileName(const WCHAR* path);
extern bool IsXpsArchive(const WCHAR* path);
extern bool IsDjVuFileName(const WCHAR* path);
extern bool IsImageEngineSupportedFile(const WCHAR* fileName, bool sniff);

// TODO: replace with an enum class FileKind { Unknown, PDF, ... };
Kind kindFilePDF = "filePDF";
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

static const char* gFileExts =
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
    ".jp2\0";

static Kind gExtsKind[] = {
    kindFileVbkm, kindFileFb2,  kindFileCbz,  kindFileCbr, kindFileCb7,  kindFileCbt,  kindFileZip,
    kindFileRar,  kindFile7Z,   kindFileTar,  kindFilePDF, kindFileXps,  kindFileXps,  kindFileChm,
    kindFilePng,  kindFileJpeg, kindFileJpeg, kindFileGif, kindFileTiff, kindFileTiff, kindFileBmp,
    kindFileTga,  kindFileJxr,  kindFileHdp,  kindFileWdp, kindFileWebp, kindFileJp2,
};

static Kind GetKindByFileExt(const WCHAR* path) {
    AutoFree pathA = strconv::WstrToUtf8(path);
    const char* ext = path::GetExtNoFree(pathA);
    AutoFree extLower = str::ToLower(ext);
    int idx = seqstrings::StrToIdx(gFileExts, extLower);
    if (idx < 0) {
        return nullptr;
    }
    CrashIf(idx >= (int)dimof(gExtsKind));
    return gExtsKind[idx];
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
Kind SniffFileTypeFromData(std::span<u8> d) {
    if (IfPdfFileContent(d)) {
        return kindFilePDF;
    }
    // we don't support sniffing kindFileVbkm

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
        // TODO: should this check the content of directory?
        return nullptr;
    }

    char buf[1024] = {0};
    int n = file::ReadN(path, buf, dimof(buf));
    if (n <= 0) {
        return nullptr;
    }
    auto res = SniffFileTypeFromData({(u8*)buf, (size_t)n});
    if (res == kindFileZip) {
        if (IsXpsArchive(path)) {
            res = kindFileXps;
        }
    }
    return res;
}

Kind FileTypeFromFileName(const WCHAR* path) {
    if (!path) {
        return nullptr;
    }
    if (path::IsDirectory(path)) {
        return kindFileDir;
    }
    // must be called before GetKindByFileExt() as that will detect. zip
    if (str::EndsWithI(path, L".fb2.zip")) {
        return kindFileFb2;
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
