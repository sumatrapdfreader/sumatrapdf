
/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

enum class FileType : u8 {
    Unknown = 0,
    PDF = 1,
    PS = 2,
    Xps = 3,
    DjVu = 4,
    Chm = 5,

    Zip = 6,
    Cbz = 7,
    Cbr = 8,
    Rar = 9,
    SevenZ = 10,
    Cb7 = 11,
    Tar = 12,
    Cbt = 13,

    Png = 14,
    Jpeg = 15,
    Gif = 16,
    Tiff = 17,
    Bmp = 18,
    Tga = 19,
    Jxr = 20,
    Hdp = 21,
    Wdp = 22,
    Webp = 23,
    Jxl = 24,
    Jp2 = 25,

    Fb2 = 26,
    Fb2z = 27,
    Epub = 28,
    Markdown = 29,
    Mobi = 30,
    PalmDoc = 31,
    HTML = 32,
    Svg = 33,
    Heic = 34,
    Avif = 35,
    Txt = 36,

    Directory = 37,
};
constexpr int kFileTypeCount = (int)FileType::Directory + 1;

// embedded PDF files have paths like "c:/foo.pdf:${pdfStreamNo}"
// or "c:/foo.pdf:${pdfStreamNo}:attachname=${hexUtf8Name}"
struct EmbeddedPdfName {
    Str streamNoStr;  // ":${pdfStreamNo}" substring within path, {} if not an embedded-PDF path
    TempStr fileName; // hex-decoded attachment name, {} if there's no ":attachname=" part
};
EmbeddedPdfName ParseEmbeddedPdfName(Str path);

FileType GuessFileTypeFromFile(Str path);
FileType GuessFileTypeFromContent(Str d);
FileType GuessFileTypeFromName(Str path);
FileType GuessFileType(Str path, bool sniff);
TempStr GfxFileExtFromDataTemp(Str);
TempStr GfxFileExtFromTypeTemp(FileType);
TempStr GetExtForFileTypeTemp(FileType);

int FileTypeIndexOf(const FileType* types, int nTypes, FileType ft);

// Headless test helper: compare GuessFileTypeFromName's canonical extension
// to an expected one (e.g. "sample.ai" -> ".pdf").
TempStr FileKindResultTemp(Str path, Str expectedExt, int* exitCodeOut = nullptr);
