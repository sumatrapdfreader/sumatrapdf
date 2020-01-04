
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/GdiplusUtil.h"
#include "utils/FileTypeSniff.h"

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

Kind SniffFileType(std::string_view d) {
    const char* data = d.data();
    size_t len = d.size();
    ImgFormat fmt = GfxFormatFromData(data, len);
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
    if (str::EqN(data, ZIP_SIG, ZIP_SIG_LEN)) {
        return kindFileZip;
    }
    if (str::EqN(data, RAR_SIG, RAR_SIG_LEN)) {
        return kindFileRar;
    }
    if (str::EqN(data, RAR5_SIG, RAR5_SIG_LEN)) {
        return kindFileRar;
    }
    return nullptr;
}

Kind SniffFileType(SniffedFile* f) {
    if (f->wasSniffed) {
        return f->kind;
    }
    f->wasSniffed = true;

    CrashIf(!f->filePath);
    char buf[64] = {0};
    int n = file::ReadN(f->filePath, buf, dimof(buf));
    if (n <= 0) {
        f->wasError = true;
        return nullptr;
    }
    f->kind = SniffFileType({(char*)buf, (size_t)n});
    return f->kind;
}
