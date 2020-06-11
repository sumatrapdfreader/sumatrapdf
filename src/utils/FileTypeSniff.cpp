
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/GdiplusUtil.h"
#include "utils/FileTypeSniff.h"

// TODO: replace with an enum class FileKind { Unknown, PDF, ... };
Kind kindFilePDF = "filePDF";
Kind kindFileMulti = "fileMulti";
Kind kindFileZip = "fileZip";
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

#define RAR_SIG "Rar!\x1A\x07\x00"
#define RAR_SIG_LEN (dimof(RAR_SIG) - 1)
#define RAR5_SIG "Rar!\x1A\x07\x01\x00"
#define RAR5_SIG_LEN (dimof(RAR5_SIG) - 1)
#define SEVEN_ZIP_SIG "7z\xBC\xAF\x27\x1C"
#define SEVEN_ZIP_SIG_LEN (dimof(SEVEN_ZIP_SIG) - 1)
#define ZIP_SIG "PK\x03\x04"
#define ZIP_SIG_LEN (dimof(ZIP_SIG) - 1)

extern bool IsPdfFileName(const WCHAR* path);
extern bool IfPdfFileContent(std::span<u8> d);

extern bool IsEngineMultiFileName(const WCHAR* path);

// detect file type based on file content
Kind SniffFileType(std::span<u8> d) {
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
    if (memeq(data, ZIP_SIG, ZIP_SIG_LEN)) {
        return kindFileZip;
    }
    if (memeq(data, RAR_SIG, RAR_SIG_LEN)) {
        return kindFileRar;
    }
    if (memeq(data, RAR5_SIG, RAR5_SIG_LEN)) {
        return kindFileRar;
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
    auto res = SniffFileType({(u8*)buf, (size_t)n});
    return res;
}

Kind FileTypeFromFileName(const WCHAR* path) {
    if (!path) {
        return nullptr;
    }
    if (IsPdfFileName(path)) {
        return kindFilePDF;
    }
    if (IsEngineMultiFileName(path)) {
        return kindFileMulti;
    }

    return nullptr;
}
