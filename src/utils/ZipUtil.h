/* Copyright 2011-2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ZipUtil_h
#define ZipUtil_h

#include "BaseUtil.h"
#include "Vec.h"
#include <unzip.h>

class ZipFile {
    unzFile uf;
    Allocator *allocator;
    Vec<const char *> filenames;
    Vec<unz_file_info64> fileinfo;

public:
    ZipFile(const TCHAR *path, Allocator *allocator=NULL);
    ZipFile(IStream *stream, Allocator *allocator=NULL);
    ~ZipFile();

    size_t GetFileCount() const;
    // caller must free() the result (fails for non-default allocators)
    TCHAR *GetFileName(size_t fileindex);

    // the result must be Allocator::Free'd with the correct allocator (default: NULL)
    char *GetFileData(const char *filename, size_t *len);
    char *GetFileData(size_t fileindex, size_t *len);

    FILETIME GetFileTime(const char *filename);
    FILETIME GetFileTime(size_t fileindex);

    bool UnzipFile(const char *filename, const TCHAR *dir, const TCHAR *unzippedName=NULL);

protected:
    void ExtractFilenames();
    size_t GetFileIndex(const char *filename);
};

#endif
