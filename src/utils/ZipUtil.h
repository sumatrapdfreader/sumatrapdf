/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ZipUtil_h
#define ZipUtil_h

#include <unzip.h>

enum ZipMethod { Zip_Any=-1, Zip_None=0, Zip_Deflate=8, Zip_Deflate64=9, Zip_Bzip=12 };

class ZipFile {
    unzFile uf;
    Allocator *allocator;
    WStrList filenames;
    Vec<unz_file_info64> fileinfo;
    Vec<unz64_file_pos> filepos;
    uLong commentLen;

    void ExtractFilenames(ZipMethod method=Zip_Any);

public:
    ZipFile(const WCHAR *path, ZipMethod method=Zip_Any, Allocator *allocator=NULL);
    ZipFile(IStream *stream, ZipMethod method=Zip_Any, Allocator *allocator=NULL);
    ~ZipFile();

    size_t GetFileCount() const;
    // the result is owned by ZipFile
    const WCHAR *GetFileName(size_t fileindex);
    // reverts GetFileName
    size_t GetFileIndex(const WCHAR *filename);

    // caller must free() the result (or rather Allocator::Free it)
    char *GetFileDataByName(const WCHAR *filename, size_t *len=NULL);
    char *GetFileDataByIdx(size_t fileindex, size_t *len=NULL);

    FILETIME GetFileTime(const WCHAR *filename);
    FILETIME GetFileTime(size_t fileindex);

    // caller must free() the result (or rather Allocator::Free it)
    char *GetComment(size_t *len=NULL);

    bool UnzipFile(const WCHAR *filename, const WCHAR *dir, const WCHAR *unzippedName=NULL);
};

IStream *OpenDirAsZipStream(const WCHAR *dirPath, bool recursive=false);

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
