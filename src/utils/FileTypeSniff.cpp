
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

// TODO: replace with an enum class FileKind { Unknown, PDF, ... };
Kind kindFilePDF = "filePDF";
Kind kindFileMulti = "fileMulti";
Kind kindFileXps = "fileXPS";
Kind kindFileDjVu = "fileDjVu";
Kind kindFileZip = "fileZip";
Kind kindFile7Z = "file7Z";
Kind kindFileRar = "fileRar";
Kind kindFileBmp = "fileBmp";
Kind kindFilePng = "filePng";
Kind kindFileJpeg = "fileJpeg";
Kind kindFileGif = "fileGif";
Kind kindFileJxr = "fileJx";
Kind kindFileTga = "fileTga";
Kind kindFileTiff = "fileTiff";
Kind kindFileWebp = "fileTWebp";
Kind kindFileJp2 = "fileJp2";

#define FILE_SIGS(V)                       \
    V("Rar!\x1A\x07\x00", kindFileRar)     \
    V("Rar!\x1A\x07\x01\x00", kindFileRar) \
    V("7z\xBC\xAF\x27\x1C", kindFile7Z)    \
    V("PK\x03\x04", kindFileZip)           \
    V("AT&T", kindFileDjVu)

struct FileSigInfo {
    const char* sig;
    size_t sigLen;
    Kind kind;
};

#define MK_SIG(SIG, KIND) {SIG, sizeof(SIG) - 1, KIND},

FileSigInfo gFileSigs[] = {FILE_SIGS(MK_SIG)};

#undef MK_SIG

// detect file type based on file content
Kind SniffFileTypeFromData(std::span<u8> d) {
    if (IfPdfFileContent(d)) {
        return kindFilePDF;
    }
    // we don't support sniffing kindFileMulti

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
    if (IsPdfFileName(path)) {
        return kindFilePDF;
    }
    if (IsXpsFileName(path)) {
        return kindFileXps;
    }
    if (IsDjVuFileName(path)) {
        return kindFileDjVu;
    }

    // must be at the end, as it opens any folder as .vbkm
    if (IsEngineMultiFileName(path)) {
        return kindFileMulti;
    }

    return nullptr;
}
