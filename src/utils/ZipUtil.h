/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ZipUtil_h
#define ZipUtil_h

// define for using unarr instead of miniunzip
#define USE_UNARR_AS_UNZIP

#ifdef USE_UNARR_AS_UNZIP
extern "C" {
typedef struct ar_stream_s ar_stream;
typedef struct ar_archive_s ar_archive;
}
#else
#include <unzip.h>
#endif

class ZipFile {
    Allocator *allocator;
    WStrList filenames;
#ifdef USE_UNARR_AS_UNZIP
    ar_stream *data;
    ar_archive *ar;
    Vec<int64_t> filepos;
    // not needed
    Vec<char> fileinfo;
    size_t commentLen;
#else
    unzFile uf;
    Vec<unz_file_info64> fileinfo;
    Vec<unz64_file_pos> filepos;
    uLong commentLen;
#endif

    void ExtractFilenames(bool deflatedOnly);

public:
    explicit ZipFile(const WCHAR *path, bool deflatedOnly=false, Allocator *allocator=NULL);
    explicit ZipFile(IStream *stream, bool deflatedOnly=false, Allocator *allocator=NULL);
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
