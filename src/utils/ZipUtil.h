/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ZipUtil_h
#define ZipUtil_h

#include <unzip.h>

class ZipFile {
    unzFile uf;
    Allocator *allocator;
    WStrList filenames;
    Vec<unz_file_info64> fileinfo;
    Vec<unz64_file_pos> filepos;
    uLong commentLen;

public:
    ZipFile(const WCHAR *path, Allocator *allocator=NULL);
    ZipFile(IStream *stream, Allocator *allocator=NULL);
    ~ZipFile();

    size_t GetFileCount() const;
    // the result is owned by ZipFile
    const WCHAR *GetFileName(size_t fileindex);
    // reverts GetFileName
    size_t GetFileIndex(const WCHAR *filename);

    // caller must free() the result (or rather Allocator::Free it)
    char *GetFileData(const WCHAR *filename, size_t *len=NULL);
    char *GetFileData(size_t fileindex, size_t *len=NULL);

    FILETIME GetFileTime(const WCHAR *filename);
    FILETIME GetFileTime(size_t fileindex);

    // caller must free() the result (or rather Allocator::Free it)
    char *GetComment(size_t *len=NULL);

    bool UnzipFile(const WCHAR *filename, const WCHAR *dir, const WCHAR *unzippedName=NULL);

protected:
    void ExtractFilenames();
};

class ZipCreatorData;

class ZipCreator {
    ZipCreatorData *d;
public:
    ZipCreator();
    ~ZipCreator();

    bool AddFile(const WCHAR *filePath, const WCHAR *nameInZip=NULL);
    bool AddFileFromDir(const WCHAR *filePath, const WCHAR *dir);
    bool SaveAs(const WCHAR *zipFilePath);
};

#endif
