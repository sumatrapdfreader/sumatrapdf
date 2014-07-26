/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef LzzaUtil_h
#define LzzaUtil_h

// Lzma ZIP Archive is a restricted ZIP-subset containing only LZMA compressed data
// (this allows for less code required for reading and writing such files)

namespace lzza {

struct Archive {
    const char *data;
    size_t dataLen;

    Archive(const char *data=NULL, size_t dataLen=0) : data(data), dataLen(dataLen) { }
};

struct FileData {
    const char *name;
    FILETIME ft;
    size_t dataSize;
    char data[1];
};

FileData *GetFileData(Archive *lzza, int idx, Allocator *allocator=NULL);
FileData *GetFileData(Archive *lzza, const char *fileName, Allocator *allocator=NULL);

// files must be NULL-terminated, all paths must be UTF-8 encoded
bool ExtractFiles(const char *archivePath, const char *dstDir, const char **files, Allocator *allocator=NULL);

// for every file, names must contain a relative path to srcDir followed by either NULL or the desired in-archive name
bool CreateArchive(const WCHAR *archivePath, const WCHAR *srcDir, WStrVec& names);

}

#endif
