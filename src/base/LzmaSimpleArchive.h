/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace lzma {

struct FileInfo {
    // public data
    u32 compressedSize;
    u32 uncompressedSize;
    u32 uncompressedCrc32;
    FILETIME ftModified;
    Str name;
    const u8* compressedData;
};

// translations + marked + mermaid + in-app manual assets share one LzSA
constexpr int kMaxLzmaArchiveFiles = 256;

struct SimpleArchive {
    int filesCount;
    FileInfo files[kMaxLzmaArchiveFiles];
};

bool ParseSimpleArchive(const u8* archiveHeader, int dataLen, SimpleArchive* archiveOut);
int GetIdxFromName(SimpleArchive* archive, Str name);
u8* GetFileDataByIdx(SimpleArchive* archive, int idx, Arena* a);
u8* GetFileDataByName(SimpleArchive* archive, Str fileName, Arena* a);
bool ExtractFiles(Str archivePath, Str dstDir, Str* files, Arena* a);

} // namespace lzma
