
/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern Kind kindFilePDF;
extern Kind kindFilePS;
extern Kind kindFileXps;
extern Kind kindFileDjVu;
extern Kind kindFileChm;

extern Kind kindFileZip;
extern Kind kindFileCbz;
extern Kind kindFileCbr;
extern Kind kindFileRar;
extern Kind kindFile7Z;
extern Kind kindFileCb7;
extern Kind kindFileTar;
extern Kind kindFileCbt;

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

extern Kind kindFileFb2;
extern Kind kindFileEpub;
extern Kind kindFileMobi;
extern Kind kindFilePalmDoc;
extern Kind kindFileHTML;
extern Kind kindFileTxt;

extern Kind kindFileVbkm;
extern Kind kindFileDir;

const WCHAR* FindEmbeddedPdfFileStreamNo(const WCHAR* path);

Kind GuessFileTypeFromContent(const WCHAR* path);
Kind GuessFileTypeFromContent(std::span<u8> d);
Kind GuessFileTypeFromName(const WCHAR*);
Kind GuessFileType(const WCHAR* path, bool fromContent);

bool KindInArray(Kind* kinds, int nKinds, Kind kind);
