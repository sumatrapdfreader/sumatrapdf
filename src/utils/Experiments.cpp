/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* This file is doesn't contain code used by Sumatra.
It's for experiments and code I want to preserve but
have not better place for. It's compiled in debug build 
but only to make sure it's compileable. */

#include "BaseUtil.h"
#include "FileUtil.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <iowin32s.h>
#include <zip.h>
#include <unzip.h>

class ZipFileInfo {
    friend class ZipExtractor;
public:
    // of interest to callers of ZipExtractor APIs
    WCHAR *     fileName;
    FILETIME    fileTime;

private:
    // for internal use
    uint32_t        hash;
    unz_file_info64 fileInfo;
    unz64_file_pos  filePos;
};

struct ZipExtractorData;

class ZipExtractor {

private:
    ZipExtractorData *d;
    ZipExtractor(Allocator *allocator);

public:
    static ZipExtractor *CreateFromFile(const WCHAR *path, Allocator *allocator=NULL);
    static ZipExtractor *CreateFromStream(IStream *stream, Allocator *allocator=NULL);

    ~ZipExtractor();

    Vec<ZipFileInfo> *GetFileInfos();
    bool ExtractTo(size_t fileInfoIdx, const WCHAR *dir, const WCHAR *extractedName = NULL);
    void *GetFileData(size_t fileInfoIdx, size_t *lenOut=NULL);
};

struct ZipExtractorData {
    Allocator *allocator;
    Vec<ZipFileInfo> fileInfos;
    const WCHAR *path;
    IStream *stream;
};

ZipExtractor::ZipExtractor(Allocator *allocator)
{
    d = new ZipExtractorData;
    d->allocator = allocator;
    d->path = NULL;
    d->stream = NULL;
}

ZipExtractor::~ZipExtractor()
{
    delete d;
}

ZipExtractor *ZipExtractor::CreateFromFile(const WCHAR *path, Allocator *allocator)
{
    ZipExtractor *ze = new ZipExtractor(allocator);
    ze->d->path = path;
    // TODO: do sth.
    return ze;
}

ZipExtractor *ZipExtractor::CreateFromStream(IStream *stream, Allocator *allocator)
{
    ZipExtractor *ze = new ZipExtractor(allocator);
    ze->d->stream = stream;
    // TODO: do sth.
    return ze;
}

Vec<ZipFileInfo> *ZipExtractor::GetFileInfos()
{
    return NULL;
}

bool ZipExtractor::ExtractTo(size_t fileInfoIdx, const WCHAR *dir, const WCHAR *extractedName)
{
    size_t fileDataSize;
    void *fileData = GetFileData(fileInfoIdx, &fileDataSize);
    if (!fileData)
        return false;
    WCHAR *path = path::Join(dir, extractedName);
    bool ok = file::WriteAll(path, fileData, fileDataSize);
    free(fileData);
    return ok;
}

void *ZipExtractor::GetFileData(size_t, size_t *)
{
    return NULL;
}
