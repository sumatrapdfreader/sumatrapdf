/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef LzmaDecUtil_h
#define LzmaDecUtil_h

namespace lzma {

struct FileInfo {
    // public data
    size_t          sizeUncompressed;
    size_t          sizeCompressed;
    const char *    name;
    const char *    data;

    // for internal use
    size_t          off;
};

// Note: good enough for our purposes, can be expanded when needed
#define MAX_LZMA_ARCHIVE_FILES 128

struct ArchiveInfo {
    int             filesCount;
    FileInfo        files[MAX_LZMA_ARCHIVE_FILES];
};

bool   GetArchiveInfo(const char *archiveData, ArchiveInfo *archiveInfoOut);
char*  Decompress(const char *in, size_t inSize, size_t *uncompressedSizeOut, Allocator *allocator);

bool   ExtractFiles(const char *archivePath, const char *dstDir, const char **files, Allocator *allocator=NULL);

}

#endif
