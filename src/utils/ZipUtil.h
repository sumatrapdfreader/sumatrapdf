/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ZipUtil_h
#define ZipUtil_h

#include <unzip.h>

class ZipFile {
    unzFile uf;
    Allocator *allocator;
    Vec<TCHAR *> filenames;
    Vec<uint32_t> filenameHashes;
    Vec<unz_file_info64> fileinfo;
    Vec<unz64_file_pos> filepos;
    uLong commentLen;

public:
    ZipFile(const TCHAR *path, Allocator *allocator=NULL);
    ZipFile(IStream *stream, Allocator *allocator=NULL);
    ~ZipFile();

    size_t GetFileCount() const;
    // the result is owned by ZipFile
    const TCHAR *GetFileName(size_t fileindex);
    // reverts GetFileName
    size_t GetFileIndex(const TCHAR *filename);

    // caller must free() the result (or rather Allocator::Free it)
    char *GetFileData(const TCHAR *filename, size_t *len=NULL);
    char *GetFileData(size_t fileindex, size_t *len=NULL);

    FILETIME GetFileTime(const TCHAR *filename);
    FILETIME GetFileTime(size_t fileindex);

    char *GetComment(size_t *len=NULL);

    bool UnzipFile(const TCHAR *filename, const TCHAR *dir, const TCHAR *unzippedName=NULL);

protected:
    void ExtractFilenames();
};

// TODO: this is just a sketch of an API, without most of the code
#if 0
class ZipFileInfo {
    friend class ZipExtractorImpl;
public:
    // of interest to callers of ZipExtractor APIs
    TCHAR *     fileName;
    FILETIME    fileTime;

private:
    // for internal use
    uint32_t        hash;
    unz_file_info64 fileInfo;
    unz64_file_pos  filePos;
};

class ZipExtractor {
public:
    friend class ZipExtractorImpl;

    static ZipExtractor *CreateFromFile(const TCHAR *path, Allocator *allocator=NULL);
    static ZipExtractor *CreateFromStream(IStream *stream, Allocator *allocator=NULL);

    virtual ~ZipExtractor() {}

    virtual Vec<ZipFileInfo> *GetFileInfos() = 0;
    virtual bool ExtractTo(size_t fileInfoIdx, const TCHAR *dir, const TCHAR *extractedName = NULL) = 0;
    virtual char *GetFileData(size_t fileInfoIdx, size_t *lenOut=NULL) = 0;

private:
    ZipExtractor() {}
};
#endif

class ZipCreator {
public:
    friend class ZipCreatorImpl;

    virtual ~ZipCreator() {}
    virtual bool AddFile(const TCHAR *filePath) = 0;
    virtual bool SaveAs(const TCHAR *zipFilePath) = 0;

    static ZipCreator *Create();

private:
    ZipCreator() {};
};

#endif
