/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace lzma {

struct FileInfo {
    // public data
    u32 compressedSize;
    u32 uncompressedSize;
    u32 uncompressedCrc32;
    FILETIME ftModified;
    const char* name;
    const u8* compressedData;
};

// Note: good enough for our purposes, can be expanded when needed
#define MAX_LZMA_ARCHIVE_FILES 128

struct SimpleArchive {
    int filesCount;
    FileInfo files[MAX_LZMA_ARCHIVE_FILES];
};

bool ParseSimpleArchive(const u8* archiveHeader, size_t dataLen, SimpleArchive* archiveOut);
int GetIdxFromName(SimpleArchive* archive, const char* name);
u8* GetFileDataByIdx(SimpleArchive* archive, int idx, Allocator* allocator);
u8* GetFileDataByName(SimpleArchive* archive, const char* fileName, Allocator* allocator);
// files is an array of char * entries, last element must be nullptr
bool ExtractFiles(const char* archivePath, const char* dstDir, const char** files, Allocator* allocator);

} // namespace lzma
