
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Kind kindFileZip;
extern Kind kindFileRar;

extern Kind kindFileBmp;
extern Kind kindFileGif;
extern Kind kindFileJpeg;
extern Kind kindFilePng;
extern Kind kindFileJxr;
extern Kind kindFileTga;
extern Kind kindFileTiff;
extern Kind kindFileWebp;
extern Kind kindFileJp2;

struct SniffedFile {
    AutoFreeWstr filePath;
    bool wasSniffed = false;
    bool wasError = false;
    Kind kind = nullptr;

    SniffedFile() = default;
    ~SniffedFile() = default;
};

// detect file type based on file content
Kind SniffFileType(std::string_view d);
Kind SniffFileType(SniffedFile*);
