
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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
extern Kind kindFileJxl;
extern Kind kindFileJp2;

extern Kind kindFileFb2;
extern Kind kindFileFb2z;
extern Kind kindFileEpub;
extern Kind kindFileMarkdown;
extern Kind kindFileMobi;
extern Kind kindFilePalmDoc;
extern Kind kindFileHTML;
extern Kind kindFileSvg;
extern Kind kindFileHeic;
extern Kind kindFileAvif;
extern Kind kindFileTxt;

extern Kind kindDirectory;

Str FindEmbeddedPdfFileStreamNo(Str path);

Kind GuessFileTypeFromContent(Str path);
Kind GuessFileTypeFromContent(const ByteSlice& d);
Kind GuessFileTypeFromName(Str path);
Kind GuessFileType(Str path, bool sniff);
Str GfxFileExtFromData(const ByteSlice&);
Str GfxFileExtFromKind(Kind);
Str GetExtForKind(Kind kind);

int KindIndexOf(Kind* kinds, int nKinds, Kind kind);

// Headless test helper: compare GuessFileTypeFromName to an expected kind name.
Str TestFileKindResult(Str path, Str expectedKindName, int* exitCodeOut = nullptr);