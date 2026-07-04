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

// Note: good enough for our purposes, can be expanded when needed
#define MAX_LZMA_ARCHIVE_FILES 128

struct SimpleArchive {
    int filesCount;
    FileInfo files[MAX_LZMA_ARCHIVE_FILES];
};

bool ParseSimpleArchive(const u8* archiveHeader, int dataLen, SimpleArchive* archiveOut);
int GetIdxFromName(SimpleArchive* archive, Str name);
u8* GetFileDataByIdx(SimpleArchive* archive, int idx, Arena* allocator);
u8* GetFileDataByName(SimpleArchive* archive, Str fileName, Arena* allocator);
// files is an array of Str entries, last element must be empty
bool ExtractFiles(Str archivePath, Str dstDir, Str* files, Arena* allocator);

} // namespace lzma
