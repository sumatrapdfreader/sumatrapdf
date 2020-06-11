
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Kind kindFilePDF;
extern Kind kindFileMulti;
extern Kind kindFileXps;
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

Kind SniffFileTypeFromData(std::span<u8> d);
Kind SniffFileType(const WCHAR* filePath);
Kind FileTypeFromFileName(const WCHAR*);
