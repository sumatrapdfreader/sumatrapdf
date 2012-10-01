/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* This file is doesn't contain code used by Sumatra.
It's for experiments and code I want to preserve but
have not better place for. It's compiled in debug build 
but only to make sure it's compileable. */

#include "BaseUtil.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <iowin32s.h>
#include <zip.h>
#include <unzip.h>

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

class ZipExtractorImpl : public ZipExtractor {
    friend class ZipExtractor;

    Vec<ZipFileInfo> fileInfos;
    Allocator *allocator;
public:
    virtual ~ZipExtractorImpl() {}
    ZipExtractorImpl() {}

    virtual Vec<ZipFileInfo> *GetFileInfos();
    virtual bool ExtractTo(size_t fileInfoIdx, const TCHAR *dir, const TCHAR *extractedName = NULL);
    virtual char *GetFileData(size_t fileInfoIdx, size_t *lenOut=NULL);

private:
    ZipExtractorImpl(Allocator *a) : allocator(a) {}
    bool OpenFile(const TCHAR *name);
    bool OpenStream(IStream *stream);
};

ZipExtractor *ZipExtractor::CreateFromFile(const TCHAR *path, Allocator *allocator)
{
    ZipExtractorImpl *ze = new ZipExtractorImpl(allocator);
    if (!ze->OpenFile(path)) {
        delete ze;
        return NULL;
    }
    return ze;
}

ZipExtractor *ZipExtractor::CreateFromStream(IStream *stream, Allocator *allocator)
{
    ZipExtractorImpl *ze = new ZipExtractorImpl(allocator);
    if (!ze->OpenStream(stream)) {
        delete ze;
        return NULL;
    }
    return ze;
}

bool ZipExtractorImpl::OpenFile(const TCHAR *name)
{
    return false;
}

bool ZipExtractorImpl::OpenStream(IStream *stream)
{
    return false;
}

Vec<ZipFileInfo> *ZipExtractorImpl::GetFileInfos()
{
    return NULL;
}

bool ZipExtractorImpl::ExtractTo(size_t fileInfoIdx, const TCHAR *dir, const TCHAR *extractedName)
{
    return false;
}

char *ZipExtractorImpl::GetFileData(size_t fileInfoIdx, size_t *lenOut)
{
    return NULL;
}
