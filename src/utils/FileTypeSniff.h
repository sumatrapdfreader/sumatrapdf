
/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Kind kindFilePDF;
extern Kind kindFilePS;
extern Kind kindFileMulti;
extern Kind kindFileXps;
extern Kind kindFileDjVu;
extern Kind kindFileChm;
extern Kind kindFileZip;
extern Kind kindFile7Z;
extern Kind kindFileRar;
extern Kind kindFileVbkm;

extern Kind kindFilePng;
extern Kind kindFileJpeg;
extern Kind kindFileGif;
extern Kind kindFileTiff;
extern Kind kindFileBmp;
extern Kind kindFileTga;
extern Kind kindFileJxr;
extern Kind kindFileHdp;
extern Kind kindFileWdp;
extern Kind kindFileWebp;
extern Kind kindFileJp2;
extern Kind kindFileDir;
extern Kind kindFileCbt;
extern Kind kindFileTar;
extern Kind kindFileEpub;

Kind SniffFileTypeFromData(std::span<u8> d);
Kind SniffFileType(const WCHAR* filePath);
Kind FileTypeFromFileName(const WCHAR*);
bool IsImageEngineKind(Kind);
bool IsCbxEngineKind(Kind);
